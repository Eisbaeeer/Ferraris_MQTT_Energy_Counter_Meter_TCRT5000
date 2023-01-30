/***************************************************************************
  Part one:
  IR Ferraris Reader
  2019 Adlerweb//Bitbastelei
  Florian Knodt - www.adlerweb.info
  Part two:
  Rui Santos
  Complete project details at https://randomnerdtutorials.com
  https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password/
  Part three:
  MQTT and combine other parts
  2020 Eisbaeeer
  This sketch allows you to connect an infrared sensor who detect the red mark on a ferraris energy counter.
  The data will be sent via MQTT to a server. The counter will be stored on the file-system as json.
  The ESP firmware update can be done via "Over-The-Air".

  History
  Ver. 0.97 (202302xx)
  - use "Ferraris" objects to capsule code and state for each meter
  - allow 1..7 meters just by one #define
  - switch from [kW] -> [W] for live consumption, so we do not need float there
  - use the count of revolutions as source of total consumption [kWh] to allow fractional part there and no float in IRQ handler

  Ver. 0.96 (20230130)
  - first part of code refactoring: split into multiple source files
  - use header lines in Dashboard and Configuration for more structured view
  - minor changes to improve stability
  - use blue LED to show lost WiFi
  - fix: value of analog sensor in Dashboard

  Ver. 0.95 (20230121)
  - Fixed: WiFi automatic reconnect after WiFi loss
  - Fixed: prevent watchdog reboot during long running MQTT update
  - Fixed: compiler warnings

  Ver. 0.94 (20221225)
  - Changed structure for auto-compiling different boards
  - Fixed: missing react-website compile by auto-compile
  - removed obsolete binary folder

  Ver. 0.92 (20211014)
  - Bugfix: Interrupt Routinen bei MQTT Übertragung unterbrochen
  - Bugfix: Interrupt Routinen beim Speichern mit littleFS unterbrochen
  - Dashboard mit zusätzlichen Infos erweitert

  Ver. 0.91 (20211011)
  - Graphen zum Dashboard hinzugefügt
  - ISR mit no-delay Entprellung angepasst
  - Nachkommastellen durch fehlerhafte addition von floats entfernt

  Ver. 0.9 (20210917)
  - Graphen zum Dashboard hinzugefügt
  - Analogwert vom Sensor wird jetzt auf dem Dashboard angezeigt

  Ver. 0.8 (20210914)
  - Bugfix Zählerroutine - jetzt per Interrupt auf alle Eingänge

  Ver. 0.7 (20210822)
  - Bugfix Zählerstand
  - Zählerstand auf Nachkommastellen erweitert

  Ver. 0.6 (20210818)
  - Change project to iot-framework

  Ver. 0.5 (20210813)
  (Eisbaeeer)
  - Bugfix boolean
  - Added 3 digits after dot

  Ver. 0.4 (20200905)
  (Eisbaeeer)
  - Bugfix Zähler 3 und 4 (Zählerstand)
  - Neu: MQTT Server Port konfigurierbar
  - Neu: MQTT publish Zeit einstellbar (1-9999 Sekunden)
  - Blinken der internen LED aus kompatibilitätsgründen von anderen Boards entfernt (manche Boards nutzen D4 für die interne LED)
  (ACHTUNG: mit dieser Version gehen die Zählerdaten verloren! bitte über Browser neu eintragen!)
  - Neu: Port D4 auf D5 umgezogen! (D4 ist bei manchen Boards die interne LED
  - Neu: Alle Zählerdaten werden im EEPROM abgespeichert.
  Ver. 0.3 (20200824)
  (Eisbaeeer)
  - OTA per Webbrowser (http://.../update)
  Ver. 0.2 (20200911)
  (Eisbaeeer)
  - Anpassung Entprellzeit (20ms)
  Ver. 0.1 (20200803)
  (Eisbaeeer)
  * initial version
  - Filesystem to store and read values from
  - Wifi-Manager to connect to Wifi easy
  - Stored values are in JSON format
  - MQTT client to publish values
  - HTTP page for configuration
  - Over the air update of firmware
  - 4 meter counter (IR-Input pins)

*********/
#include <Arduino.h>
#include <PubSubClient.h>
#include "LittleFS.h"
#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "dashboard.h"
#include "timeSync.h"
#include <time.h>    // time() ctime()
#include <ArduinoJson.h>

#include "ferraris.h"
#include "mqtt_subscribe.h"
#include "mqtt_publish.h"

const int analogInPin = A0;   // ESP8266 Analog Pin ADC0 = A0

int mqttPublishTime;          // last publish time in seconds

// MQTT
WiFiClient espClient;
PubSubClient MQTTclient(espClient);

// Tasks
struct task
{
  unsigned long rate;
  unsigned long previous;
};

task taskA = { .rate = 1000, .previous = 0 };
task taskB = { .rate =  200, .previous = 0 };


// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

void copyConfig2Ferraris()
{
  Ferraris::getInstance(0).set_U_kWh      (configManager.data.meter_loops_count_1);
  Ferraris::getInstance(0).set_revolutions(configManager.data.meter_counter_reading_1 * configManager.data.meter_loops_count_1);
  Ferraris::getInstance(0).set_debounce   (configManager.data.meter_debounce_1);
#if FERRARIS_NUM > 1
  Ferraris::getInstance(1).set_U_kWh      (configManager.data.meter_loops_count_2);
  Ferraris::getInstance(1).set_revolutions(configManager.data.meter_counter_reading_2 * configManager.data.meter_loops_count_2);
  Ferraris::getInstance(1).set_debounce   (configManager.data.meter_debounce_2);
#endif
#if FERRARIS_NUM > 2
  Ferraris::getInstance(2).set_U_kWh      (configManager.data.meter_loops_count_3);
  Ferraris::getInstance(2).set_revolutions(configManager.data.meter_counter_reading_3 * configManager.data.meter_loops_count_3);
  Ferraris::getInstance(2).set_debounce   (configManager.data.meter_debounce_3);
#endif
#if FERRARIS_NUM > 3
  Ferraris::getInstance(3).set_U_kWh      (configManager.data.meter_loops_count_4);
  Ferraris::getInstance(3).set_revolutions(configManager.data.meter_counter_reading_4 * configManager.data.meter_loops_count_4);
  Ferraris::getInstance(3).set_debounce   (configManager.data.meter_debounce_4);
#endif
#if FERRARIS_NUM > 4
  Ferraris::getInstance(4).set_U_kWh      (configManager.data.meter_loops_count_5);
  Ferraris::getInstance(4).set_revolutions(configManager.data.meter_counter_reading_5 * configManager.data.meter_loops_count_5);
  Ferraris::getInstance(4).set_debounce   (configManager.data.meter_debounce_5;
#endif
#if FERRARIS_NUM > 5
  Ferraris::getInstance(5).set_U_kWh      (configManager.data.meter_loops_count_6);
  Ferraris::getInstance(5).set_revolutions(configManager.data.meter_counter_reading_6 * configManager.data.meter_loops_count_6);
  Ferraris::getInstance(5).set_debounce   (configManager.data.meter_debounce_6);
#endif
#if FERRARIS_NUM > 6
  Ferraris::getInstance(6).set_U_kWh      (configManager.data.meter_loops_count_7);
  Ferraris::getInstance(6).set_revolutions(configManager.data.meter_counter_reading_7 * configManager.data.meter_loops_count_7);
  Ferraris::getInstance(6).set_debounce   (configManager.data.meter_debounce_7);
#endif
}

// update Dashboard with current measurement values
void updateDashboard(uint8_t F)
{
  assert(F < FERRARIS_NUM);

  switch (F) {
    case 0:
      dash.data.revolutions_1 = Ferraris::getInstance(0).get_revolutions();
      dash.data.kWh_1         = Ferraris::getInstance(0).get_kWh();
      dash.data.W_1           = Ferraris::getInstance(0).get_W();
      configManager.data.meter_counter_reading_1 = Ferraris::getInstance(0).get_kWh();
      break;
#if FERRARIS_NUM > 1
    case 1:
      dash.data.revolutions_2 = Ferraris::getInstance(1).get_revolutions();
      dash.data.kWh_2         = Ferraris::getInstance(1).get_kWh();
      dash.data.W_2           = Ferraris::getInstance(1).get_W();
      configManager.data.meter_counter_reading_2 = Ferraris::getInstance(1).get_kWh();
      break;
#endif
#if FERRARIS_NUM > 2
    case 2:
      dash.data.revolutions_3 = Ferraris::getInstance(2).get_revolutions();
      dash.data.kWh_3         = Ferraris::getInstance(2).get_kWh();
      dash.data.W_3           = Ferraris::getInstance(2).get_W();
      configManager.data.meter_counter_reading_3 = Ferraris::getInstance(2).get_kWh();
      break;
#endif
#if FERRARIS_NUM > 3
    case 3:
      dash.data.revolutions_4 = Ferraris::getInstance(3).get_revolutions();
      dash.data.kWh_4         = Ferraris::getInstance(3).get_kWh();
      dash.data.W_4           = Ferraris::getInstance(3).get_W();
      configManager.data.meter_counter_reading_4 = Ferraris::getInstance(3).get_kWh();
      break;
#endif
#if FERRARIS_NUM > 4
    case 4:
      dash.data.revolutions_5 = Ferraris::getInstance(1).get_revolutions();
      dash.data.kWh_5         = Ferraris::getInstance(1).get_kWh();
      dash.data.W_5           = Ferraris::getInstance(1).get_W();
      configManager.data.meter_counter_reading_5 = Ferraris::getInstance(4).get_kWh();
      break;
#endif
#if FERRARIS_NUM > 5
    case 5:
      dash.data.revolutions_6 = Ferraris::getInstance(2).get_revolutions();
      dash.data.kWh_6         = Ferraris::getInstance(2).get_kWh();
      dash.data.W_6           = Ferraris::getInstance(2).get_W();
      configManager.data.meter_counter_reading_6 = Ferraris::getInstance(5).get_kWh();
      break;
#endif
#if FERRARIS_NUM > 6
    case 6:
      dash.data.revolutions_7 = Ferraris::getInstance(3).get_revolutions();
      dash.data.kWh_7         = Ferraris::getInstance(3).get_kWh();
      dash.data.W_7           = Ferraris::getInstance(3).get_W();
      configManager.data.meter_counter_reading_7 = Ferraris::getInstance(6).get_kWh();
      break;
#endif
  }
}

// update Dashboard with graph plot data, only necessary when client is connected
void updateDashboardGraph()
{
  int rssi = WiFi.RSSI();
  sprintf(dash.data.Wifi_RSSI, "%d", rssi) ;
  dash.data.WLAN_RSSI = WiFi.RSSI();

  dash.data.Sensor = analogRead(analogInPin);

  dash.data.Impuls_Z1 = !Ferraris::getInstance(0).get_state();
#if FERRARIS_NUM > 1
  dash.data.Impuls_Z2 = !Ferraris::getInstance(1).get_state();
#endif
#if FERRARIS_NUM > 2
  dash.data.Impuls_Z3 = !Ferraris::getInstance(2).get_state();
#endif
#if FERRARIS_NUM > 3
  dash.data.Impuls_Z4 = !Ferraris::getInstance(3).get_state();
#endif
#if FERRARIS_NUM > 4
  dash.data.Impuls_Z4 = !Ferraris::getInstance(4).get_state();
#endif
#if FERRARIS_NUM > 5
  dash.data.Impuls_Z4 = !Ferraris::getInstance(5).get_state();
#endif
#if FERRARIS_NUM > 6
  dash.data.Impuls_Z4 = !Ferraris::getInstance(6).get_state();
#endif

  for (uint8_t F=0; F<FERRARIS_NUM; F++)
    updateDashboard(F);
}


void setup() {
  Serial.begin(115200);

  LittleFS.begin();
  GUI.begin();
  configManager.begin();
  configManager.setConfigSaveCallback( copyConfig2Ferraris );
  // WiFi
  WiFi.hostname(configManager.data.wifi_hostname);
  WiFi.setAutoReconnect(true);
  WiFiManager.begin(configManager.data.projectName);
  timeSync.begin();
  dash.begin(taskB.rate);

  // IR-Sensor
  copyConfig2Ferraris();
  for (uint8_t F=0; F<FERRARIS_NUM; F++) {
    Ferraris::getInstance(F).begin();
    updateDashboard(F);
  }

  MQTTclient.setServer(configManager.data.mqtt_server, configManager.data.mqtt_port);
  MQTTclient.setCallback(parseMQTTmessage);
  MQTTclient.setBufferSize(320); // TODO: maybe we can calculate this based on the largest assumed request + its parameters?

  memcpy(dash.data.Version, "v.0.97", 6);

  // activate port for status LED
  pinMode(LED_BUILTIN, OUTPUT);
}


void loop() {
  // framework things
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();
  MQTTclient.loop();

  for (uint8_t F=0; F<FERRARIS_NUM; F++)
    if (Ferraris::getInstance(F).loop()) {
      updateDashboard(F);
    }

  // tasks
  if (taskA.previous == 0 || (millis() - taskA.previous > taskA.rate)) {
    taskA.previous = millis();
    if (WiFi.status() == WL_CONNECTED) {
      checkMQTTconnection();

      if (mqttPublishTime <= configManager.data.mqtt_interval) {
        mqttPublishTime++;
      } else {
        publishMQTT();
        mqttPublishTime = 0;
      }
    }
  }

  if (taskB.previous == 0 || (millis() - taskB.previous > taskB.rate)) {
    taskB.previous = millis();
    if (GUI.ws.count() > 0) // only when clients are connected to web-server
      updateDashboardGraph();

    digitalWrite(LED_BUILTIN, (WiFi.status() == WL_CONNECTED));
  }

  if (saveConfig) {
    Serial.println("[save config]...");
    saveConfig = false;
    configManager.save();   // may take ~50ms
    Serial.println("...[save config]");
  }
}
