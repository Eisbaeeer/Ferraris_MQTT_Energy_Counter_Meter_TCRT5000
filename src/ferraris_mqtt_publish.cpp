#include "ferraris_mqtt_publish.h"
// libraries
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
// access to data on web server
#include "configManager.h"
#include "dashboard.h"


String getTopicName(int meter, String measurement) {
  String  topic = "Ferraris/";
  topic = topic + configManager.data.messure_place;
  topic = topic +"/Zähler";
  topic = topic + String(meter);
  topic = topic +"/";
  topic = topic + measurement;
  
  return topic;
}

String getHATopicName(String mqtt_type, char uniqueId[30]) {
  String  topic = "homeassistant/";
  topic = topic + mqtt_type;
  topic = topic +"/";
  topic = topic + String(uniqueId);
  topic = topic +"/config";

  return topic;
}

String getSetTopicName(int meter, String measurement) {
  String  topic = getTopicName(meter,measurement);
  topic = topic + "/set"; 
  
  return topic;
}


// declare some external stuff -> todo: better refactoring
extern PubSubClient MQTTclient;
void IRAM_ATTR IRSensorHandle1();
void IRAM_ATTR IRSensorHandle2();
void IRAM_ATTR IRSensorHandle3();
void IRAM_ATTR IRSensorHandle4();


static int mqttReconnect;   // timeout for reconnecting MQTT Server

void checkMQTTconnection(void)
{
  if (mqttReconnect++ > 60) {
    mqttReconnect = 0;    // reset reconnect timeout
    // reconnect to MQTT Server
    if (!MQTTclient.connected()) {
      detachInterrupt(digitalPinToInterrupt(IRPIN1));
      detachInterrupt(digitalPinToInterrupt(IRPIN2));
      detachInterrupt(digitalPinToInterrupt(IRPIN3));
      detachInterrupt(digitalPinToInterrupt(IRPIN4));
      dash.data.MQTT_Connected = false;
      Serial.println("Attempting MQTT connection...");
      // Create a random client ID
      String clientId = "FerrarisClient-";
      //clientId += String(random(0xffff), HEX);
      clientId += String(configManager.data.messure_place);
      // Attempt to connect
      if (MQTTclient.connect(clientId.c_str(),configManager.data.mqtt_user,configManager.data.mqtt_password)) {
        Serial.println("connected");
        dash.data.MQTT_Connected = true;
        // Once connected, publish an announcement...
        publishMQTT();
        // ... and resubscribe
        // MQTTclient.subscribe("inTopic");
        String topic[3]={"UKWh","Stand","Entprellzeit"};
        for (int tId = 0; tId < 3; tId++) {
          for (int i = 0; i < 4; i++) {
            ESP.wdtFeed();  // keep WatchDog alive
            String t = getSetTopicName(i+1, topic[tId]);
            if (MQTTclient.subscribe(t.c_str())) {
              Serial.print("subscribed to ");
            } else {
              Serial.print("failed to subscribe to ");
            }
            Serial.print(t);
            Serial.println();
          }
        }
      } else {
        Serial.print("failed, rc=");
        Serial.print(MQTTclient.state());
        Serial.println(" try again in one minute");
      }
      attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
    }
  }    
}


#define MSG_BUFFER_SIZE	(20)
char message_buffer[MSG_BUFFER_SIZE];

void publishMQTT(void)
{
  detachInterrupt(digitalPinToInterrupt(IRPIN1));
  detachInterrupt(digitalPinToInterrupt(IRPIN2));
  detachInterrupt(digitalPinToInterrupt(IRPIN3));
  detachInterrupt(digitalPinToInterrupt(IRPIN4));

  String topic;
  String cmdTopic;
  String haTopic;
  if (configManager.data.home_assistant_auto_discovery) {
    StaticJsonDocument<240> discoverDocument;
    char discoverJson[240];
    char uniqueId[30];
    String meterName;
    for (int i = 0; i < 4; i++) {
      ESP.wdtFeed();  // keep WatchDog alive
      // kW
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));

      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "kw", i+1);
      topic = getTopicName(i+1, "KW");
      meterName = "Zähler "+String(i+1)+" kW";

      discoverDocument["dev_cla"] = "power";
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "kW";
      discoverDocument["val_tpl"] = "{{value}}";

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("sensor", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)) {
        Serial.print("failed to publish kw "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }

      // kWh / Stand
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));

      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "kwh", i+1);
      topic = getTopicName(i+1, "Stand");
      meterName = "Zähler "+String(i+1)+" kW/h";
      cmdTopic = getSetTopicName(i+1, "Stand");

      discoverDocument["dev_cla"] = "energy";
      discoverDocument["cmd_t"] = cmdTopic;
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "kWh";
      discoverDocument["val_tpl"] = "{{value}}";

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("sensor", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)) {
        Serial.print("failed to publish kwh "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }

      // Umdrehungen/kWh
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));
      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "ukwh", i+1);
      topic = getTopicName(i+1, "UKWh");
      meterName = "Zähler "+String(i+1)+" Umdrehungen/kWh";
      cmdTopic = getSetTopicName(i+1, "UKWh");

      discoverDocument["cmd_t"] = cmdTopic;
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "Umdrehungen/kWh";
      discoverDocument["val_tpl"] = "{{value}}";
      discoverDocument["max"] = 512;

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("number", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)) {
        Serial.print("failed to publish ukwh "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }

      // Entprellzeit
      discoverDocument.clear();
      memset(discoverJson, 0, sizeof(discoverJson));
      memset(uniqueId, 0, sizeof(uniqueId));
      snprintf_P(uniqueId, sizeof(uniqueId), PSTR("%06X_%s_%d"), ESP.getChipId(), "entprellzeit", i+1);
      topic = getTopicName(i+1, "Entprellzeit");
      meterName = "Zähler "+String(i+1)+" Entprellzeit";
      cmdTopic = getSetTopicName(i+1, "Entprellzeit");

      discoverDocument["cmd_t"] = cmdTopic;
      discoverDocument["uniq_id"] = uniqueId;
      discoverDocument["name"] = meterName;
      discoverDocument["stat_t"] = topic;
      discoverDocument["unit_of_meas"] = "ms";
      discoverDocument["val_tpl"] = "{{value}}";
      discoverDocument["max"] = 200; // TODO: Is this a reasonable maximum value?

      serializeJson(discoverDocument, discoverJson);

      haTopic = getHATopicName("number", uniqueId);
      if (!MQTTclient.publish(haTopic.c_str(), discoverJson, true)) {
        Serial.print("failed to publish debounce time "+String(i+1)+" discover json:");
        Serial.println();
        Serial.print(discoverJson);
        Serial.println();
      }
    }
  }

  // Meter #1
  topic = getTopicName(1,"Stand");
  dtostrf(configManager.data.meter_counter_reading_1, 7, 3, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(1,"KW");
  char char_Leistung_Zaehler1[6];
  dtostrf(dash.data.kW_1, 4, 3, char_Leistung_Zaehler1);
  MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler1, true);

  topic = getTopicName(1,"UKWh");
  char char_meter_loop_counts1[5];
  dtostrf(configManager.data.meter_loops_count_1,4,0, char_meter_loop_counts1);
  MQTTclient.publish(topic.c_str(), char_meter_loop_counts1, true);

  topic = getTopicName(1,"Entprellzeit");
  char char_debounce_1[4];
  dtostrf(configManager.data.meter_debounce_1,3,0, char_debounce_1);
  MQTTclient.publish(topic.c_str(), char_debounce_1, true);

  // Meter #2
  topic = getTopicName(2,"Stand");
  dtostrf(configManager.data.meter_counter_reading_2, 7, 3, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(2,"KW");
  char char_Leistung_Zaehler2[6];
  dtostrf(dash.data.kW_2, 4, 3, char_Leistung_Zaehler2);
  MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler2, true);

  topic = getTopicName(2,"UKWh");
  char char_meter_loop_counts2[5];
  dtostrf(configManager.data.meter_loops_count_2,4,0, char_meter_loop_counts2);
  MQTTclient.publish(topic.c_str(), char_meter_loop_counts2, true);

  topic = getTopicName(2,"Entprellzeit");
  char char_debounce_2[4];
  dtostrf(configManager.data.meter_debounce_2,3,0, char_debounce_2);
  MQTTclient.publish(topic.c_str(), char_debounce_2, true);

  // Meter #3
  topic = getTopicName(3,"Stand");
  dtostrf(configManager.data.meter_counter_reading_3, 7, 3, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(3,"KW");
  char char_Leistung_Zaehler3[6];
  dtostrf(dash.data.kW_3, 4, 3, char_Leistung_Zaehler3);
  MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler3, true);

  topic = getTopicName(3,"UKWh");
  char char_meter_loop_counts3[5];
  dtostrf(configManager.data.meter_loops_count_3,4,0, char_meter_loop_counts3);
  MQTTclient.publish(topic.c_str(), char_meter_loop_counts3, true);

  topic = getTopicName(3,"Entprellzeit");
  char char_debounce_3[4];
  dtostrf(configManager.data.meter_debounce_3,3,0, char_debounce_3);
  MQTTclient.publish(topic.c_str(), char_debounce_3, true);

  // Meter #4
  topic = getTopicName(4,"Stand");
  dtostrf(configManager.data.meter_counter_reading_4, 7, 3, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(4,"KW");
  char char_Leistung_Zaehler4[6];
  dtostrf(dash.data.kW_4, 4, 3, char_Leistung_Zaehler4);
  MQTTclient.publish(topic.c_str(), char_Leistung_Zaehler4, true);

  topic = getTopicName(4,"UKWh");
  char char_meter_loop_counts4[5];
  dtostrf(configManager.data.meter_loops_count_4,4,0, char_meter_loop_counts4);
  MQTTclient.publish(topic.c_str(), char_meter_loop_counts4, true);

  topic = getTopicName(4,"Entprellzeit");
  char char_debounce_4[4];
  dtostrf(configManager.data.meter_debounce_4,3,0, char_debounce_4);
  MQTTclient.publish(topic.c_str(), char_debounce_4, true);

  attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
}
