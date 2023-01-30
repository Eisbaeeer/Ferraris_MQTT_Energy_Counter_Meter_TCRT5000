#include "mqtt_publish.h"
// libraries
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
// access to data on web server
#include "configManager.h"
#include "dashboard.h"
#include "ferraris.h"


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
/*
void IRAM_ATTR IRSensorHandle1();
void IRAM_ATTR IRSensorHandle2();
void IRAM_ATTR IRSensorHandle3();
void IRAM_ATTR IRSensorHandle4();
*/

static int mqttReconnect;   // timeout for reconnecting MQTT Server

void checkMQTTconnection(void)
{
  if (mqttReconnect++ > 60) {
    mqttReconnect = 0;    // reset reconnect timeout
    // reconnect to MQTT Server
    if (!MQTTclient.connected()) {
      /*
      detachInterrupt(digitalPinToInterrupt(IRPIN1));
      detachInterrupt(digitalPinToInterrupt(IRPIN2));
      detachInterrupt(digitalPinToInterrupt(IRPIN3));
      detachInterrupt(digitalPinToInterrupt(IRPIN4));
      */
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
      /*
      attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
      attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
      */
    }
  }
}


#define MSG_BUFFER_SIZE	(20)
char message_buffer[MSG_BUFFER_SIZE];

void publishMQTT(void)
{
  /*
  detachInterrupt(digitalPinToInterrupt(IRPIN1));
  detachInterrupt(digitalPinToInterrupt(IRPIN2));
  detachInterrupt(digitalPinToInterrupt(IRPIN3));
  detachInterrupt(digitalPinToInterrupt(IRPIN4));
  */

  String topic;
  String cmdTopic;
  String haTopic;
  if (configManager.data.home_assistant_auto_discovery) {
    StaticJsonDocument<240> discoverDocument;
    char discoverJson[240];
    char uniqueId[30];
    String meterName;
    ESP.wdtFeed();  // keep WatchDog alive
    for (int i = 0; i < 4; i++) {
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
      ESP.wdtFeed();  // keep WatchDog alive
    }
  }

  // Meter #1
  topic = getTopicName(1, "Stand");
  dtostrf(Ferraris::getInstance(0).get_kWh(), 1, 1, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(1, "W");
  dtostrf(Ferraris::getInstance(0).get_W(), 1, 1, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, false);

  topic = getTopicName(1, "UKWh");
  itoa(configManager.data.meter_loops_count_1, message_buffer, 10);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(1, "Entprellzeit");
  itoa(configManager.data.meter_debounce_1, message_buffer, 10);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  // Meter #2
  topic = getTopicName(2, "Stand");
  dtostrf(Ferraris::getInstance(1).get_kWh(), 1, 1, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(2, "W");
  dtostrf(Ferraris::getInstance(1).get_W(), 1, 1, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, false);

  topic = getTopicName(2, "UKWh");
  itoa(configManager.data.meter_loops_count_2, message_buffer, 10);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(2,"Entprellzeit");
  itoa(configManager.data.meter_debounce_2, message_buffer, 10);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  // Meter #3
  topic = getTopicName(3, "Stand");
  dtostrf(Ferraris::getInstance(2).get_kWh(), 1, 1, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(3, "W");
  dtostrf(Ferraris::getInstance(2).get_W(), 1, 1, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, false);

  topic = getTopicName(3, "UKWh");
  itoa(configManager.data.meter_loops_count_3, message_buffer, 10);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(3, "Entprellzeit");
  itoa(configManager.data.meter_debounce_3, message_buffer, 10);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  // Meter #4
  topic = getTopicName(4, "Stand");
  dtostrf(Ferraris::getInstance(3).get_kWh(), 1, 1, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(4, "W");
  dtostrf(Ferraris::getInstance(3).get_W(), 1, 1, message_buffer);
  MQTTclient.publish(topic.c_str(), message_buffer, false);

  topic = getTopicName(4, "UKWh");
  itoa(configManager.data.meter_loops_count_4, message_buffer, 10);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  topic = getTopicName(4, "Entprellzeit");
  itoa(configManager.data.meter_debounce_4, message_buffer, 10);
  MQTTclient.publish(topic.c_str(), message_buffer, true);

  /*
  attachInterrupt(digitalPinToInterrupt(IRPIN1), IRSensorHandle1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN2), IRSensorHandle2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN3), IRSensorHandle3, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IRPIN4), IRSensorHandle4, CHANGE);
  */
}
