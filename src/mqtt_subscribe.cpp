#include "mqtt_subscribe.h"
#include "configManager.h"


bool saveConfig = false;


// declare some external stuff -> todo: better refactoring
String getSetTopicName(int meter, String measurement);

// MQTT subscribe callback
void parseMQTTmessage(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // copy payload into string compatible format
  char pl[length+1];
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    pl[i] = (char)payload[i];
  }
  pl[length] = '\0';
  Serial.println();

  String p = String(pl);
  String t = String(topic);

  String ukwhCmdTopic;
  String kwhCmdTopic;
  String debounceTimeCmdTopic;
  bool processed=false;
  for (int i=0; i<4; i++) {
    ukwhCmdTopic         = getSetTopicName(i+1, "UKWh");
    kwhCmdTopic          = getSetTopicName(i+1, "Stand");
    debounceTimeCmdTopic = getSetTopicName(i+1, "Entprellzeit");

    if (t == ukwhCmdTopic){
      int16_t meters_per_loop = p.toInt();
      switch (i+1) {
        case 1:
          Serial.print("Setting configManager.data.meter_loops_count_1 to ");
          Serial.print(meters_per_loop);
          Serial.println();
          configManager.data.meter_loops_count_1=meters_per_loop;
          saveConfig = true;
          processed  = true;
          break;
        case 2:
          Serial.print("Setting configManager.data.meter_loops_count_2 to ");
          Serial.print(meters_per_loop);
          Serial.println();
          configManager.data.meter_loops_count_2=meters_per_loop;
          saveConfig = true;
          processed  = true;
          break;
        case 3:
          Serial.print("Setting configManager.data.meter_loops_count_3 to ");
          Serial.print(meters_per_loop);
          Serial.println();
          configManager.data.meter_loops_count_3=meters_per_loop;
          saveConfig = true;
          processed  = true;
          break;
        case 4:
          Serial.print("Setting configManager.data.meter_loops_count_4 to ");
          Serial.print(meters_per_loop);
          Serial.println();
          configManager.data.meter_loops_count_4=meters_per_loop;
          saveConfig = true;
          processed  = true;
          break;

        default:
          break;
      }

      if (processed) {
        break;
      }
    }

    if (t == kwhCmdTopic) {
      int16_t meter_value = p.toInt();
      switch (i+1) {
        case 1:
          Serial.print("Setting configManager.data.meter_counter_reading_1 to ");
          Serial.print(meter_value);
          Serial.println();
          configManager.data.meter_counter_reading_1=meter_value;
          saveConfig = true;
          processed  = true;
          break;
        case 2:
          Serial.print("Setting configManager.data.meter_counter_reading_2 to ");
          Serial.print(meter_value);
          Serial.println();
          configManager.data.meter_counter_reading_2=meter_value;
          saveConfig = true;
          processed  = true;
          break;
        case 3:
          Serial.print("Setting configManager.data.meter_counter_reading_3 to ");
          Serial.print(meter_value);
          Serial.println();
          configManager.data.meter_counter_reading_3=meter_value;
          saveConfig = true;
          processed  = true;
          break;
        case 4:
          Serial.print("Setting configManager.data.meter_counter_reading_4 to ");
          Serial.print(meter_value);
          Serial.println();
          configManager.data.meter_counter_reading_4=meter_value;
          saveConfig = true;
          processed  = true;
          break;

        default:
          break;
      }

      if (processed) {
        break;
      }
    }

    if (t == debounceTimeCmdTopic) {
      int16_t debounce_value = p.toInt();
      switch (i+1) {
        case 1:
          Serial.print("Setting configManager.data.debounce_1 to ");
          Serial.print(debounce_value);
          Serial.println();
          configManager.data.meter_debounce_1 = debounce_value;
          saveConfig = true;
          processed  = true;
          break;
        case 2:
          Serial.print("Setting configManager.data.debounce_2 to ");
          Serial.print(debounce_value);
          Serial.println();
          configManager.data.meter_debounce_2 = debounce_value;
          saveConfig = true;
          processed  = true;
          break;
        case 3:
          Serial.print("Setting configManager.data.debounce_3 to ");
          Serial.print(debounce_value);
          Serial.println();
          configManager.data.meter_debounce_3 = debounce_value;
          saveConfig = true;
          processed  = true;
          break;
        case 4:
          Serial.print("Setting configManager.data.debounce_4 to ");
          Serial.print(debounce_value);
          Serial.println();
          configManager.data.meter_debounce_4 = debounce_value;
          saveConfig = true;
          processed  = true;
          break;

        default:
          break;
      }

      if (processed) {
        break;
      }
    }
  }

  if (!processed) {
    Serial.print("Could not process request!");
    Serial.println();
  }
}
