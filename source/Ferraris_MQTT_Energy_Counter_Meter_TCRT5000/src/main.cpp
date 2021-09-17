

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
  
 * Used pins
 * Internal LED       (D0) GPIO 16
 * IR Pin Messure 1   (D1) GPIO 05
 * IR Pin Messure 2   (D2) GPIO 04
 * IR Pin Messure 3   (D3) GPIO 00
 * IR Pin Messure 4   (D4) GPIO 02
 * free               (D5) GPIO 14
 * free               (D6) GPIO 12
 * free               (D7) GPIO 13
 * free               (D8) GPIO 15
 * free               (SDD3) GPIO 10
 * 
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

// Infrared vars
#define IRPIN1 D1
#define IRPIN2 D2
#define IRPIN3 D3
#define IRPIN4 D5
#define RED1 LOW
#define SILVER1 HIGH
#define RED2 LOW
#define SILVER2 HIGH
#define RED3 LOW
#define SILVER3 HIGH
#define RED4 LOW
#define SILVER4 HIGH
#define MINTIME 2    //in 10ms = 20ms
#define MSG_BUFFER_SIZE	(20)
char result[MSG_BUFFER_SIZE];

bool lastState1 = 0;  // 0 = Silver->Red; 1 = Red->Silver
bool lastState2 = 0;  
bool lastState3 = 0; 
bool lastState4 = 0;  
unsigned long lastmillis1 = 0;
unsigned long pendingmillis1 = 0;
unsigned long lastmillis2 = 0;
unsigned long pendingmillis2 = 0;
unsigned long lastmillis3 = 0;
unsigned long pendingmillis3 = 0;
unsigned long lastmillis4 = 0;
unsigned long pendingmillis4 = 0;
bool inbuf1[MINTIME];
bool inbuf2[MINTIME];
bool inbuf3[MINTIME];
bool inbuf4[MINTIME];
bool startup1=true;
bool startup2=true;
bool startup3=true;
bool startup4=true;
int loops_actual_1 = 0;
int loops_actual_2 = 0;
int loops_actual_3 = 0;
int loops_actual_4 = 0;
float counter_reading_1 = 0;
float counter_reading_2 = 0;
float counter_reading_3 = 0;
float counter_reading_4 = 0;
char char_meter_kw_1[6];
char char_meter_kw_2[6];
char char_meter_kw_3[6];
char char_meter_kw_4[6];
const int analogInPin = A0;   // ESP8266 Analog Pin ADC0 = A0

int mqttPublishTime;          // last publish time in seconds
int mqttReconnect;            // timeout for reconnecting MQTT Server

// MQTT
WiFiClient espClient;
PubSubClient MQTTclient(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  }

// Tasks
struct task
{    
    unsigned long rate;
    unsigned long previous;
};

task taskA = { .rate = 1000, .previous = 0 };
task taskB = { .rate = 200, .previous = 0 };

// ### Begin Subroutines

void PublishMQTT(void) {                     //MQTTclient.publish
    String topic = "Ferraris/";
           topic = topic + configManager.data.messure_place;
           topic = topic +"/Zähler1/Stand";
      dtostrf(configManager.data.meter_counter_reading_1, 7, 3, result);
    MQTTclient.publish(topic.c_str(), result);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler1/KW";
          char char_Leistung_Zaehler1[6];
          dtostrf(dash.data.Leistung_Zaehler1, 4, 3, char_Leistung_Zaehler1);
    MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler1);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler1/UKWh";    
          char char_meter_loop_counts1[5];
          dtostrf(configManager.data.meter_loops_count_1,4,0, char_meter_loop_counts1);
    MQTTclient.publish(topic.c_str(), char_meter_loop_counts1);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler2/Stand";
      dtostrf(configManager.data.meter_counter_reading_2, 8, 3, result);
    MQTTclient.publish(topic.c_str(), result);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler2/KW";
          char char_Leistung_Zaehler2[6];
          dtostrf(dash.data.Leistung_Zaehler2, 4, 3, char_Leistung_Zaehler2);
    MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler2);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler2/UKWh";
          char char_meter_loop_counts2[5];
          dtostrf(configManager.data.meter_loops_count_2,4,0, char_meter_loop_counts2);
    MQTTclient.publish(topic.c_str(), char_meter_loop_counts2);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler3/Stand";
      dtostrf(configManager.data.meter_counter_reading_3, 8, 3, result);
    MQTTclient.publish(topic.c_str(), result);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler3/KW";
          char char_Leistung_Zaehler3[6];
          dtostrf(dash.data.Leistung_Zaehler3, 4, 3, char_Leistung_Zaehler3);
    MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler3);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler3/UKWh";
          char char_meter_loop_counts3[5];
          dtostrf(configManager.data.meter_loops_count_3,4,0, char_meter_loop_counts3);
    MQTTclient.publish(topic.c_str(), char_meter_loop_counts3);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler4/Stand";
      dtostrf(configManager.data.meter_counter_reading_4, 8, 3, result);
    MQTTclient.publish(topic.c_str(), result);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler4/KW";
          char char_Leistung_Zaehler4[6];
          dtostrf(dash.data.Leistung_Zaehler4, 4, 3, char_Leistung_Zaehler4);
    MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler4);
          topic = "Ferraris/";
          topic = topic + configManager.data.messure_place;
          topic = topic +"/Zähler4/UKWh";
          char char_meter_loop_counts4[5];
          dtostrf(configManager.data.meter_loops_count_4,4,0, char_meter_loop_counts4);
    MQTTclient.publish(topic.c_str(), char_meter_loop_counts4);
}

void reconnect(void) {
  if (mqttReconnect > 30) {
    mqttReconnect = 0;    // reset reconnect timeout
  // reconnect to MQTT Server
  if (!MQTTclient.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    //clientId += String(random(0xffff), HEX);
    clientId += String(configManager.data.messure_place);
    // Attempt to connect
    if (MQTTclient.connect(clientId.c_str(),configManager.data.mqtt_user,configManager.data.mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      PublishMQTT();
      // ... and resubscribe
      // MQTTclient.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" try again in one minute");
      //Wait 5 seconds before retrying
      //delay(5000);
     }
    }
   }
}

// IR-Sensor Subs
bool getInput(uint8_t pin) {
  byte inchk=0;
  for(byte i=0; i < 5; i++) {
    inchk += digitalRead(pin);
    delay(2);
  }
  if(inchk >= 3) return 1;
  return 0;
}

bool procInput1(bool state) {
  byte inchk=0;
  //Array shift
  for (byte k = MINTIME-2; (k >= 0 && k < MINTIME); k--) {
    inbuf1[k+1] = inbuf1[k];
    inchk += inbuf1[k];
  }

  //New value
  inbuf1[0] = state;
  inchk += state;
  
  //Return average
  if(inchk > MINTIME/2) return 1;
  return 0;
}

bool procInput2(bool state) {
  byte inchk=0;
  //Array shift
  for (byte k = MINTIME-2; (k >= 0 && k < MINTIME); k--) {
    inbuf2[k+1] = inbuf2[k];
    inchk += inbuf2[k];
  }

  //New value
  inbuf2[0] = state;
  inchk += state;
  
  //Return average
  if(inchk > MINTIME/2) return 1;
  return 0;
}

bool procInput3(bool state) {
  byte inchk=0;
  //Array shift
  for (byte k = MINTIME-2; (k >= 0 && k < MINTIME); k--) {
    inbuf3[k+1] = inbuf3[k];
    inchk += inbuf3[k];
  }

  //New value
  inbuf3[0] = state;
  inchk += state;
  
  //Return average
  if(inchk > MINTIME/2) return 1;
  return 0;
}

bool procInput4(bool state) {
  byte inchk=0;
  //Array shift
  for (byte k = MINTIME-2; (k >= 0 && k < MINTIME); k--) {
    inbuf4[k+1] = inbuf4[k];
    inchk += inbuf4[k];
  }

  //New value
  inbuf4[0] = state;
  inchk += state;
  
  //Return average
  if(inchk > MINTIME/2) return 1;
  return 0;
}

void calcPower1(void)  {
  unsigned long took1 = pendingmillis1 - lastmillis1;
  lastmillis1 = pendingmillis1;

  if(!startup1) {
    dash.data.Leistung_Zaehler1 = 3600000.00 / took1 / configManager.data.meter_loops_count_1;
    Serial.print(dash.data.Leistung_Zaehler1);
    Serial.print(" kW @ ");
    Serial.print(took1);
    Serial.println("ms");

    // adding float to meter count
    float delta_meter1 = 1.0 / configManager.data.meter_loops_count_1;
    configManager.data.meter_counter_reading_1 = configManager.data.meter_counter_reading_1 + delta_meter1;

    
    
    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_1 :");
    Serial.print(loops_actual_1);
    Serial.print(" / ");
    Serial.println(configManager.data.meter_loops_count_1);
    
    if(loops_actual_1 < configManager.data.meter_loops_count_1) {
      loops_actual_1++;
    } else {
    loops_actual_1 = 1;
    configManager.save();
    }
    
    Serial.print("meter_counter_reading_1 :");
    Serial.print(configManager.data.meter_counter_reading_1);
    Serial.println(" KWh");
  
    } else {
    startup1=false;
  }
}

void calcPower2(void)  {
  unsigned long took2 = pendingmillis2 - lastmillis2;
  lastmillis2 = pendingmillis2;

  if(!startup2) {
    dash.data.Leistung_Zaehler2 = 3600000.00 / took2 / configManager.data.meter_loops_count_2;
    Serial.print(dash.data.Leistung_Zaehler2);
    Serial.print(" kW @ ");
    Serial.print(took2);
    Serial.println("ms");

    // adding float to meter count
    float delta_meter2 = 1.0 / configManager.data.meter_loops_count_2;
    configManager.data.meter_counter_reading_2 = configManager.data.meter_counter_reading_2 + delta_meter2;

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_2 :");
    Serial.print(loops_actual_2);
    Serial.print(" / ");
    Serial.println(configManager.data.meter_loops_count_2);
    
    if(loops_actual_2 < configManager.data.meter_loops_count_2) {
      loops_actual_2++;
    } else {
    loops_actual_2 = 1;
    configManager.save();
    }
    
    Serial.print("meter_counter_reading_2 :");
    Serial.print(configManager.data.meter_counter_reading_2);
    Serial.println(" KWh");
  
    } else {
    startup2=false;
  }
}

void calcPower3(void)  {
  unsigned long took3 = pendingmillis3 - lastmillis3;
  lastmillis3 = pendingmillis3;

  if(!startup3) {
    dash.data.Leistung_Zaehler3 = 3600000.00 / took3 / configManager.data.meter_loops_count_3;
    Serial.print(dash.data.Leistung_Zaehler3);
    Serial.print(" kW @ ");
    Serial.print(took3);
    Serial.println("ms");

    // adding float to meter count
    float delta_meter3 = 1.0 / configManager.data.meter_loops_count_3;
    configManager.data.meter_counter_reading_3 = configManager.data.meter_counter_reading_3 + delta_meter3;

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_3 :");
    Serial.print(loops_actual_3);
    Serial.print(" / ");
    Serial.println(configManager.data.meter_loops_count_3);
    
    if(loops_actual_3 < configManager.data.meter_loops_count_3) {
      loops_actual_3++;
    } else {
    loops_actual_3 = 1;
    configManager.save();
    }
    
    Serial.print("meter_counter_reading_3 :");
    Serial.print(configManager.data.meter_counter_reading_3);
    Serial.println(" KWh");
  
    } else {
    startup3=false;
  }
}

void calcPower4(void)  {
  unsigned long took4 = pendingmillis4 - lastmillis4;
  lastmillis4 = pendingmillis4;

  if(!startup4) {
    dash.data.Leistung_Zaehler4 = 3600000.00 / took4 / configManager.data.meter_loops_count_4;
    Serial.print(dash.data.Leistung_Zaehler4);
    Serial.print(" kW @ ");
    Serial.print(took4);
    Serial.println("ms");

    // adding float to meter count
    float delta_meter4 = 1.0 / configManager.data.meter_loops_count_4;
    configManager.data.meter_counter_reading_4 = configManager.data.meter_counter_reading_4 + delta_meter4;

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_4 :");
    Serial.print(loops_actual_4);
    Serial.print(" / ");
    Serial.println(configManager.data.meter_loops_count_4);
    
    if(loops_actual_4 < configManager.data.meter_loops_count_4) {
      loops_actual_4++;
    } else {
    loops_actual_4 = 1;
    configManager.save();
    }
    
    Serial.print("meter_counter_reading_4 :");
    Serial.print(configManager.data.meter_counter_reading_4);
    Serial.println(" KWh");
  
    } else {
    startup4=false;
  }
}

IRAM_ATTR void IRSensorHandle1(void) {
 
   // IR Sensors
  bool cur1 = getInput(IRPIN1);
  cur1 = procInput1(cur1);
  
  switch(lastState1) {
    case 0: //Silver; Waiting for transition to red
      if(cur1 != SILVER1) {
        lastState1 = true;
        pendingmillis1 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower1();
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur1 != RED1) {
        lastState1=false;
        Serial.println("Red detected; Waiting for silver");
      }
      break;
  }
}

IRAM_ATTR void IRSensorHandle2(void) {
 
   // IR Sensors
  bool cur2 = getInput(IRPIN2);
  cur2 = procInput2(cur2);
  
    switch(lastState2) {
    case 0: //Silver; Waiting for transition to red
      if(cur2 != SILVER2) {
        lastState2=true;
        pendingmillis2 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower2();
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur2 != RED2) {
        lastState2=false;
        Serial.println("Red detected; Waiting for silver");
      }
      break;
  }
}

IRAM_ATTR void IRSensorHandle3(void) {
 
   // IR Sensors
  bool cur3 = getInput(IRPIN3);
  cur3 = procInput3(cur3);
  
  switch(lastState3) {
    case 0: //Silver; Waiting for transition to red
      if(cur3 != SILVER3) {
        lastState3=true;
        pendingmillis3 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower3();
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur3 != RED3) {
        lastState3=false;
        Serial.println("Red detected; Waiting for silver");
      }
      break;
  }
}

IRAM_ATTR void IRSensorHandle4(void) {
 
   // IR Sensors
  bool cur4 = getInput(IRPIN4);
  cur4 = procInput4(cur4);
  
  switch(lastState4) {
    case 0: //Silver; Waiting for transition to red
      if(cur4 != SILVER4) {
        lastState4=true;
        pendingmillis4 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower4();
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur4 != RED4) {
        lastState4=false;
        Serial.println("Red detected; Waiting for silver");
      }
      break;
  }
}
// ### End Subroutines


void setup() {
  Serial.begin(115200);

    LittleFS.begin();
    GUI.begin();
    WiFiManager.begin(configManager.data.projectName);
    configManager.begin();
    timeSync.begin();
    dash.begin(500);

  // WiFi
  WiFi.hostname(configManager.data.wifi_hostname);
  WiFi.begin();

  // IR-Sensor
  pinMode(IRPIN1, INPUT_PULLUP);
  pinMode(IRPIN2, INPUT_PULLUP);
  pinMode(IRPIN3, INPUT_PULLUP);
  pinMode(IRPIN4, INPUT_PULLUP);

  // Printout the IP address
  IPAddress ip;
  ip = WiFi.localIP();
  Serial.println("Connected.");
  Serial.print("IP-address : ");
  Serial.println(ip);

    String VERSION = F("v.0.9");
    int str_len = VERSION.length() + 1;
    VERSION.toCharArray(dash.data.Version,str_len);

    MQTTclient.setServer(configManager.data.mqtt_server, configManager.data.mqtt_port);
    MQTTclient.setCallback(callback);

    dash.data.KWh_Zaehler1 = configManager.data.meter_counter_reading_1;
    dash.data.KWh_Zaehler2 = configManager.data.meter_counter_reading_2;
    dash.data.KWh_Zaehler3 = configManager.data.meter_counter_reading_3;
    dash.data.KWh_Zaehler4 = configManager.data.meter_counter_reading_4;

    attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
    attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
  
}

void loop() {
  // framework things
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();
  MQTTclient.loop();

//task A
    if (taskA.previous == 0 || (millis() - taskA.previous > taskA.rate))
    {
        taskA.previous = millis();
        int rssi = 0;
        rssi = WiFi.RSSI();
        sprintf(dash.data.Wifi_RSSI, "%ld", rssi) ;
        dash.data.WLAN_RSSI = WiFi.RSSI();

        dash.data.KWh_Zaehler1 = configManager.data.meter_counter_reading_1;
        dash.data.KWh_Zaehler2 = configManager.data.meter_counter_reading_2;
        dash.data.KWh_Zaehler3 = configManager.data.meter_counter_reading_3;
        dash.data.KWh_Zaehler4 = configManager.data.meter_counter_reading_4;
         
          if (!MQTTclient.connected()) {
              reconnect();
            }
        mqttReconnect++;      

        if (mqttPublishTime <= configManager.data.mqtt_interval) {
          mqttPublishTime++;
        } else {
          PublishMQTT();
          mqttPublishTime = 0;
          Serial.println(F("Publish to MQTT Server"));

        // DEBUG
        Serial.print(F("meter_kw_1: "));
        Serial.print(dash.data.Leistung_Zaehler1);
        Serial.println(" KW");
        Serial.print("loops_actual_1: ");
        Serial.print(loops_actual_1);
        Serial.print(" / ");
        Serial.println(configManager.data.meter_loops_count_1);
        Serial.print("meter_counter_reading_1: ");
        Serial.print(configManager.data.meter_counter_reading_1);
        Serial.println(" KWh");
        }

    }

    if (taskB.previous == 0 || (millis() - taskB.previous > taskB.rate))
    {
        taskB.previous = millis();
        dash.data.Sensor = analogRead(analogInPin);
    }

}