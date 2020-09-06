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

  Ver. 0.5 (20200906)
  (Eisbaeeer)
  - Bugfix vars neu einlesen nach EEProm WriteReadEEPROM

  Ver. 0.4 (20200905)
  (Eisbaeeer)
  - Bugfix Zähler 3 und 4 (Zählerstand)
  - Neu: MQTT Server Port konfigurierbar
  - Neu: MQTT publish Zeit einstellbar (1-9999 Sekunden)
  - Blinken der internen LED aus kompatibilitätsgründen von anderen Boards entfernt (manche Boards nutzen D4 für die interne LED)
  (ACHTUNG: mit dieser Version gehen die Zählerdaten verloren! bitte über Browser neu eintragen!)
  (Im Code bitte die Zeilen: "SPIFFS.format();" auskommentieren, hochladen, wieder kommentieren und hochladen)
  (Danach über den Browser die Werte neu eingeben!)  
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

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>

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

// no more delay with interval 
unsigned long previousMillis = 0;
const long interval = 1000;   // one second
int seconds;                  // var for counting seconds
int mqttReconnect;            // timeout for reconnecting MQTT Server
int mqttPublishTime;          // last publish time in seconds

// Set web server port number to 80
//WiFiServer server(80);
ESP8266WebServer server ( 80 );
ESP8266HTTPUpdateServer httpUpdater(true);

// Variable to store the HTTP request
String webString="";
String header;

// Assign variables for ir-counter, electricity meter, mqtt parameter
char messure_place[20];
char mqtt_server[20];
char mqtt_port[5];
char mqtt_publish[5];
char mqtt_user[20];
char mqtt_password[20];
char meter_counter_reading_1[10];    // energy counter
char meter_counter_reading_2[10];    
char meter_counter_reading_3[10];    
char meter_counter_reading_4[10];    
int counter_reading_1 = 0;
int counter_reading_2 = 0;
int counter_reading_3 = 0;
int counter_reading_4 = 0;
float flo_meter_counter_reading_1;  // actual power
float flo_meter_counter_reading_2;  
float flo_meter_counter_reading_3;  
float flo_meter_counter_reading_4;  
char meter_loops_count_1[6];        // rounds per KWh
char meter_loops_count_2[6];        
char meter_loops_count_3[6];        
char meter_loops_count_4[6];        
int loops_count_1 = 75;
int loops_count_2 = 75;
int loops_count_3 = 75;
int loops_count_4 = 75;
int loops_actual_1 = 1;
int loops_actual_2 = 1;
int loops_actual_3 = 1;
int loops_actual_4 = 1;
char char_meter_kw_1[6];
char char_meter_kw_2[6];
char char_meter_kw_3[6];
char char_meter_kw_4[6];
float Meter_KW_1 = 0;
float Meter_KW_2 = 0;
float Meter_KW_3 = 0;
float Meter_KW_4 = 0;

#include "Eprom.h"

char htmlResponse1[3000];
char htmlResponse2[3000];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

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

void PublishMQTT(void) {                     //MQTTclient.publish
    String topic = "Ferraris/";
           topic = topic + messure_place;
           topic = topic +"/Zähler1/Stand";
    MQTTclient.publish(topic.c_str(), meter_counter_reading_1);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler1/KW";
    MQTTclient.publish(topic.c_str(), char_meter_kw_1);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler1/UKWh";    
    MQTTclient.publish(topic.c_str(), meter_loops_count_1);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler2/Stand";
    MQTTclient.publish(topic.c_str(), meter_counter_reading_2);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler2/KW";
    MQTTclient.publish(topic.c_str(), char_meter_kw_2);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler2/UKWh";
    MQTTclient.publish(topic.c_str(), meter_loops_count_2);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler3/Stand";
    MQTTclient.publish(topic.c_str(), meter_counter_reading_3);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler3/KW";
    MQTTclient.publish(topic.c_str(), char_meter_kw_3);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler3/UKWh";    
    MQTTclient.publish(topic.c_str(), meter_loops_count_3);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler4/Stand";
    MQTTclient.publish(topic.c_str(), meter_counter_reading_4);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler4/KW";
    MQTTclient.publish(topic.c_str(), char_meter_kw_4);
          topic = "Ferraris/";
          topic = topic + messure_place;
          topic = topic +"/Zähler4/UKWh";
    MQTTclient.publish(topic.c_str(), meter_loops_count_4);
}

void reconnect();
void IRSensorHandle();

void setup() {
  Serial.begin(115200);

  // IR-Sensor
  pinMode(IRPIN1, INPUT_PULLUP);
  pinMode(IRPIN2, INPUT_PULLUP);
  pinMode(IRPIN3, INPUT_PULLUP);
  pinMode(IRPIN4, INPUT_PULLUP);
  
  // !!! clean FS, for testing
  // SPIFFS.format();
  
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  //wifiManager.addParameter(&custom_output);
   
  // !!! Uncomment and run it once, if you want to erase all the stored information
  // wifiManager.resetSettings();
  
  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  // if you get here you have connected to the WiFi
  IPAddress ip;
  ip = WiFi.localIP();
  Serial.println("Connected.");
  Serial.print("IP-address : ");
  Serial.println(ip);
  
  //strcpy(output, custom_output.getValue());
 
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    //json["output"] = output;
  
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  // HTTP Server
  server.on ( "/", handleRoot );
  server.on ("/save", handleSave);
  httpUpdater.setup(&server);          // /update for upload .bin files
  server.begin();
  Serial.println ( "HTTP server started" );

  ReadEEPROM();
  // convert the global vars from char
  counter_reading_1 = atoi(meter_counter_reading_1);
  loops_count_1 = atoi(meter_loops_count_1);
  counter_reading_2 = atoi(meter_counter_reading_2);
  loops_count_2 = atoi(meter_loops_count_2);
  counter_reading_3 = atoi(meter_counter_reading_3);
  loops_count_3 = atoi(meter_loops_count_3);
  counter_reading_4 = atoi(meter_counter_reading_4);
  loops_count_4 = atoi(meter_loops_count_4);


  int int_mqtt_port = atoi(mqtt_port);
  int int_mqtt_publish = atoi(mqtt_publish);
  MQTTclient.setServer(mqtt_server, int_mqtt_port);
  MQTTclient.setCallback(callback);
  
  // OTA Part
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
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

   Meter_KW_1 = 3600000.00 / took1 / loops_count_1;
   dtostrf(Meter_KW_1, 3, 2, char_meter_kw_1); 

  if(!startup1) {
    Serial.print(Meter_KW_1);
    Serial.print(" kW @ ");
    Serial.print(took1);
    Serial.println("ms");
    Serial.print("char_meter_kw_1 :");
    Serial.print(char_meter_kw_1);
    Serial.println(" KW");
    
    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_1 :");
    Serial.print(loops_actual_1);
    Serial.print(" / ");
    Serial.println(loops_count_1);
    
    if(loops_actual_1 < loops_count_1) {
      loops_actual_1++;
    } else {
    counter_reading_1++;
    loops_actual_1 = 1;
    itoa(counter_reading_1, meter_counter_reading_1, 10);   //convert int to char
    SaveEEPROM();
    }
    
    Serial.print("meter_counter_reading_1 :");
    Serial.print(counter_reading_1);
    Serial.println(" KWh");
  
    } else {
    startup1=false;
  }
}

void calcPower2(void)  {
  unsigned long took2 = pendingmillis2 - lastmillis2;
  lastmillis2 = pendingmillis2;

   Meter_KW_2 = 3600000.00 / took2 / loops_count_2;
   dtostrf(Meter_KW_2, 3, 2, char_meter_kw_2); 

  if(!startup2) {
    Serial.print(Meter_KW_2);
    Serial.print(" kW @ ");
    Serial.print(took2);
    Serial.println("ms");
    Serial.print("char_meter_kw_2 :");
    Serial.print(char_meter_kw_2);
    Serial.println(" KW");

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_2 :");
    Serial.print(loops_actual_2);
    Serial.print(" / ");
    Serial.println(loops_count_2);
    
    if(loops_actual_2 < loops_count_2) {
      loops_actual_2++;
    } else {
    counter_reading_2++;
    loops_actual_2 = 1;
    itoa(counter_reading_2, meter_counter_reading_2, 10);   //convert int to char
    SaveEEPROM();
    }
    
    Serial.print("meter_counter_reading_2 :");
    Serial.print(counter_reading_2);
    Serial.println(" KWh");
  
    } else {
    startup2=false;
  }
}

void calcPower3(void)  {
  unsigned long took3 = pendingmillis3 - lastmillis3;
  lastmillis3 = pendingmillis3;

   Meter_KW_3 = 3600000.00 / took3 / loops_count_3;
   dtostrf(Meter_KW_3, 3, 2, char_meter_kw_3); 

  if(!startup3) {
    Serial.print(Meter_KW_3);
    Serial.print(" kW @ ");
    Serial.print(took3);
    Serial.println("ms");
    Serial.print("char_meter_kw_3 :");
    Serial.print(char_meter_kw_3);
    Serial.println(" KW");

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_3 :");
    Serial.print(loops_actual_3);
    Serial.print(" / ");
    Serial.println(loops_count_3);
    
    if(loops_actual_3 < loops_count_3) {
      loops_actual_3++;
    } else {
    counter_reading_3++;
    loops_actual_3 = 1;
    itoa(counter_reading_3, meter_counter_reading_3, 10);   //convert int to char
    SaveEEPROM();
    }
    
    Serial.print("meter_counter_reading_3 :");
    Serial.print(counter_reading_3);
    Serial.println(" KWh");
  
    } else {
    startup3=false;
  }
}

void calcPower4(void)  {
  unsigned long took4 = pendingmillis4 - lastmillis4;
  lastmillis4 = pendingmillis4;

   Meter_KW_4 = 3600000.00 / took4 / loops_count_4;
   dtostrf(Meter_KW_4, 3, 2, char_meter_kw_4); 

  if(!startup4) {
    Serial.print(Meter_KW_4);
    Serial.print(" kW @ ");
    Serial.print(took4);
    Serial.println("ms");
    Serial.print("char_meter_kw_4 :");
    Serial.print(char_meter_kw_4);
    Serial.println(" KW");

    // check if one KWh is gone (75 rpm then ++ kwh) and store values in file-system
    Serial.print("loops_actual_4 :");
    Serial.print(loops_actual_4);
    Serial.print(" / ");
    Serial.println(loops_count_4);
    
    if(loops_actual_4 < loops_count_4) {
      loops_actual_4++;
    } else {
    counter_reading_4++;
    loops_actual_4 = 1;
    itoa(counter_reading_4, meter_counter_reading_4, 10);   //convert int to char
    SaveEEPROM();
    }
    
    Serial.print("meter_counter_reading_4 :");
    Serial.print(counter_reading_4);
    Serial.println(" KWh");
  
    } else {
    startup4=false;
  }
}

void handleSave(void) {
    String str1;
    Serial.println("");
  if (server.arg("aa")!= ""){
    Serial.println("Counter_Meter 1: " + server.arg("aa"));
    // convert int to char
    str1=String(server.arg("aa"));
    str1.toCharArray(meter_counter_reading_1,10);
    counter_reading_1 = atoi(meter_counter_reading_1);
  }

  if (server.arg("bb")!= ""){
    Serial.println("Counter_RpKWh 1: " + server.arg("bb"));
    // convert int to char
    str1=String(server.arg("bb"));
    str1.toCharArray(meter_loops_count_1,6);
    loops_count_1 = atoi(meter_loops_count_1);
  }

  if (server.arg("cc")!= ""){
    Serial.println("Counter_Meter 2: " + server.arg("cc"));
    // convert int to char
    str1=String(server.arg("cc"));
    str1.toCharArray(meter_counter_reading_2,10);
    counter_reading_2 = atoi(meter_counter_reading_2);
  }

  if (server.arg("dd")!= ""){
    Serial.println("Counter_RpKWh 2: " + server.arg("dd"));
    // convert int to char
    str1=String(server.arg("dd"));
    str1.toCharArray(meter_loops_count_2,6);
    loops_count_2 = atoi(meter_loops_count_2);
  }

  if (server.arg("ee")!= ""){
    Serial.println("Counter_Meter 3: " + server.arg("ee"));
    // convert int to char
    str1=String(server.arg("ee"));
    str1.toCharArray(meter_counter_reading_3,10);
    counter_reading_3 = atoi(meter_counter_reading_3);
  }

  if (server.arg("ff")!= ""){
    Serial.println("Counter_RpKWh 3: " + server.arg("ff"));
    // convert int to char
    str1=String(server.arg("ff"));
    str1.toCharArray(meter_loops_count_3,6);
    loops_count_3 = atoi(meter_loops_count_3);
  }

  if (server.arg("gg")!= ""){
    Serial.println("Counter_Meter 4: " + server.arg("gg"));
    // convert int to char
    str1=String(server.arg("gg"));
    str1.toCharArray(meter_counter_reading_4,10);
    counter_reading_4 = atoi(meter_counter_reading_4);
  }

  if (server.arg("hh")!= ""){
    Serial.println("Counter_RpKWh 4: " + server.arg("hh"));
    // convert int to char
    str1=String(server.arg("hh"));
    str1.toCharArray(meter_loops_count_4,6);
    loops_count_4 = atoi(meter_loops_count_4);
  }

   if (server.arg("ab")!= ""){
    str1=String(server.arg("ab"));
    str1.toCharArray(messure_place,20);
  }

   if (server.arg("ac")!= ""){
    str1=String(server.arg("ac"));
    str1.toCharArray(mqtt_server,20);
  }

   if (server.arg("ad")!= ""){
    str1=String(server.arg("ad"));
    str1.toCharArray(mqtt_user,20);
  }

   if (server.arg("ae")!= ""){
    str1=String(server.arg("ae"));
    str1.toCharArray(mqtt_password,20);
  }

  if (server.arg("af")!= ""){
    str1=String(server.arg("af"));
    str1.toCharArray(mqtt_port,5);
  }

  if (server.arg("ag")!= ""){
    str1=String(server.arg("ag"));
    str1.toCharArray(mqtt_publish,5);
  }

  SaveEEPROM();
  // convert the global vars from char
  counter_reading_1 = atoi(meter_counter_reading_1);
  loops_count_1 = atoi(meter_loops_count_1);
  counter_reading_2 = atoi(meter_counter_reading_2);
  loops_count_2 = atoi(meter_loops_count_2);
  counter_reading_3 = atoi(meter_counter_reading_3);
  loops_count_3 = atoi(meter_loops_count_3);
  counter_reading_4 = atoi(meter_counter_reading_4);
  loops_count_4 = atoi(meter_loops_count_4);

}

void handleRoot(void) {
  
  String messplatz(messure_place);
  String mqttserver(mqtt_server);
  String mqttport(mqtt_port);
  String mqttpublish(mqtt_publish);
  String mqttuser(mqtt_user);
  String mqttpassword(mqtt_password);
  
  snprintf ( htmlResponse1, 3000,
"<!DOCTYPE html>\
<html lang=\"en\">\
  <head>\
    <meta charset=\"utf-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
  </head>\
  <body>\
  ");
  
  webString="<h1><center>MQTT ESP8266 Ferraris Energy Counter</h1>\
            <ceneter>&copy; Copyright by Eisbaeeer 2020<br>\
            (Eisbaeeer@gmail.com)<br>\
            https://github.com/Eisbaeeer<p>\
              <b>MQTT Server </b><input type='text' name='val_ac' id='val_ac' size=20 value=\""+mqttserver+"\" placeholder=\"MQTT Server IP\"><br> \
              <b>MQTT Server Port </b><input type='text' name='val_af' id='val_af' size=5 value=\""+mqttport+"\" placeholder=\"MQTT Server Port\"><br> \
              <b>MQTT Publish time in seconds (1-9999) </b><input type='text' name='val_ag' id='val_ag' size=5 value=\""+mqttpublish+"\" placeholder=\"MQTT Publish Time in seconds (1-9999)\"><br> \
              <b>MQTT Benutzer </b><input type='text' name='val_ad' id='val_ad' size=20 value=\""+mqttuser+"\" placeholder=\"MQTT Benutzer\"><br> \
              <b>MQTT Passwort </b><input type='text' name='val_ae' id='val_ae' size=20 value=\""+mqttpassword+"\" placeholder=\"MQTT Passwort\"><p> \                        
              <b>Me&szlig;platz </b><input type='text' name='val_ab' id='val_ab' size=20 value=\""+messplatz+"\" placeholder=\"Me&szlig;platz\" autofocus><p> \                        
              <b>Z&auml;hlerstand Z&auml;hler 1 </b><input type='text' name='val_aa' id='val_aa' size=9 value=\""+String((int)counter_reading_1)+"\" placeholder=\"Z&auml;lerstand\"> KWh<br> \
              <b>Umdrehungen Z&auml;hler 1 </b><input type='text' name='val_bb' id='val_bb' size=5 value=\""+String((int)loops_count_1)+"\" placeholder=\"Umdrehungen\"> U/KWh<p> \
              <b>Z&auml;hlerstand Z&auml;hler 2 </b><input type='text' name='val_cc' id='val_cc' size=9 value=\""+String((int)counter_reading_2)+"\" placeholder=\"Z&auml;lerstand\"> KWh<br> \
              <b>Umdrehungen Z&auml;hler 2 </b><input type='text' name='val_dd' id='val_dd' size=5 value=\""+String((int)loops_count_1)+"\" placeholder=\"Umdrehungen\"> U/KWh<p> \
              <b>Z&auml;hlerstand Z&auml;hler 3 </b><input type='text' name='val_ee' id='val_ee' size=9 value=\""+String((int)counter_reading_3)+"\" placeholder=\"Z&auml;lerstand\"> KWh<br> \
              <b>Umdrehungen Z&auml;hler 3 </b><input type='text' name='val_ff' id='val_ff' size=5 value=\""+String((int)loops_count_3)+"\" placeholder=\"Umdrehungen\"> U/KWh<p> \
              <b>Z&auml;hlerstand Z&auml;hler 4 </b><input type='text' name='val_gg' id='val_gg' size=9 value=\""+String((int)counter_reading_4)+"\" placeholder=\"Z&auml;lerstand\"> KWh<br> \
              <b>Umdrehungen Z&auml;hler 4 </b><input type='text' name='val_hh' id='val_hh' size=5 value=\""+String((int)loops_count_4)+"\" placeholder=\"Umdrehungen\"> U/KWh<p> \
              <div>\
              <button id=\"save_button\">Save</button>\
              </div>";
              

snprintf ( htmlResponse2, 3000,     
    "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.min.js\"></script>\    
    <script>\
      var aa;\
      var bb;\
      var cc;\
      var dd;\
      var ee;\
      var ff;\
      var gg;\
      var hh;\
      var ab;\
      var ac;\
      var ad;\
      var ae;\
      var af;\
      var ag;\
      $('#save_button').click(function(e){\
        e.preventDefault();\
        aa = $('#val_aa').val();\
        bb = $('#val_bb').val();\
        cc = $('#val_cc').val();\
        dd = $('#val_dd').val();\
        ee = $('#val_ee').val();\
        ff = $('#val_ff').val();\
        gg = $('#val_gg').val();\
        hh = $('#val_hh').val();\
        ab = $('#val_ab').val();\
        ac = $('#val_ac').val();\
        ad = $('#val_ad').val();\
        ae = $('#val_ae').val();\
        af = $('#val_af').val();\
        ag = $('#val_ag').val();\
        $.get('/save?aa=' + aa + '&bb=' + bb + '&cc=' + cc + '&dd=' + dd + '&ee=' + ee + '&ff=' + ff + '&gg=' + gg + '&hh=' + hh + '&ab=' + ab + '&ac=' + ac + '&ad=' + ad + '&ae=' + ae + '&af=' + af + '&ag=' + ag, function(data){\
          console.log(data);\
        });\
      });\      
    </script>\
  </body>\
</html>");   
   server.send ( 200, "text/html", htmlResponse1 + webString + htmlResponse2 ); 
}

void loop(){

  // OTA
  ArduinoOTA.handle();

  // One second interval
  unsigned long currentMillis = millis();
  int int_mqtt_publish = atoi(mqtt_publish);
  
  if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      mqttReconnect++;
      seconds++;

  if (mqttPublishTime <= int_mqtt_publish) {
     mqttPublishTime++;
     } else {
      PublishMQTT();
      mqttPublishTime = 0;
     }
      
      if (seconds >= 60) {
        Serial.println("--- One minute gone ---");
        seconds=0;

        // TEST output
        Serial.print("meter_kw_1 :");
        Serial.println(Meter_KW_1);
        Serial.print("char_meter_kw_1 :");
        Serial.print(char_meter_kw_1);
        Serial.println(" KW");
        Serial.print("loops_actual_1 :");
        Serial.print(loops_actual_1);
        Serial.print(" / ");
        Serial.println(loops_count_1);
        Serial.print("meter_counter_reading_1 :");
        Serial.print(counter_reading_1);
        Serial.println(" KWh");
  
    
      }
}

  //WiFiClient client = server.available();   // Listen for incoming clients
  server.handleClient();
  IRSensorHandle();
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
    clientId += String(messure_place);
    // Attempt to connect
    if (MQTTclient.connect(clientId.c_str(),mqtt_user,mqtt_password)) {
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

void IRSensorHandle(void) {
    //MQTT
  if (!MQTTclient.connected()) {
    // hier noch das Handling reconnect MQTT eintragen
    reconnect();
  }
  MQTTclient.loop();

  // IR Sensors
  bool cur1 = getInput(IRPIN1);
  cur1 = procInput1(cur1);
  bool cur2 = getInput(IRPIN2);
  cur2 = procInput2(cur2);
  bool cur3 = getInput(IRPIN3);
  cur3 = procInput3(cur3);
  bool cur4 = getInput(IRPIN4);
  cur4 = procInput4(cur4);

  switch(lastState1) {
    case 0: //Silver; Waiting for transition to red
      if(cur1 != SILVER1) {
        lastState1++;
        pendingmillis1 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower1();
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur1 != RED1) {
        lastState1=0;
        Serial.println("Red detected; Waiting for silver");
      }
      break;
  }

    switch(lastState2) {
    case 0: //Silver; Waiting for transition to red
      if(cur2 != SILVER2) {
        lastState2++;
        pendingmillis2 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower2();
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur2 != RED2) {
        lastState2=0;
        Serial.println("Red detected; Waiting for silver");
      }
      break;
  }

  switch(lastState3) {
    case 0: //Silver; Waiting for transition to red
      if(cur3 != SILVER3) {
        lastState3++;
        pendingmillis3 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower3();
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur3 != RED3) {
        lastState3=0;
        Serial.println("Red detected; Waiting for silver");
      }
      break;
  }

  switch(lastState4) {
    case 0: //Silver; Waiting for transition to red
      if(cur4 != SILVER4) {
        lastState4++;
        pendingmillis4 = millis();
        Serial.println("Silver detected; waiting for red");
        calcPower4();
      }
      break;
    case 1: //Red; Waiting for transition to silver
      if(cur4 != RED4) {
        lastState4=0;
        Serial.println("Red detected; Waiting for silver");
      }
      break;
  }
}
