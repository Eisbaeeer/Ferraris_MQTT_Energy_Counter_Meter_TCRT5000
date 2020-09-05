/**** Save settings to eeprom
Size of Char: 1
Size of Bool: 1
Size of Int: 2
Size of Unsigned Int: 2
Size of Long: 4
 */


void SaveEEPROM(void)
{
      EEPROM.begin(1024);
      Serial.println("");
      Serial.println("--- Write EEPROM! ---");
     
      EEPROM.put( 0,messure_place );                  // char[20]
      EEPROM.put( 20, mqtt_server );                  // char[20]
      EEPROM.put( 40, mqtt_port );                    // char[5]
      EEPROM.put( 45, mqtt_publish );                 // char[5]
      EEPROM.put( 50, mqtt_user );                    // char[20]
      EEPROM.put( 70, mqtt_password);                 // char[20]
      EEPROM.put( 90, meter_counter_reading_1);       // char[10]
      EEPROM.put( 100, meter_counter_reading_2 );     // char[10]
      EEPROM.put( 110, meter_counter_reading_3);      // char[10]
      EEPROM.put( 120, meter_counter_reading_4);      // char[10]
      EEPROM.put( 130, meter_loops_count_1);          // char[6]
      EEPROM.put( 136, meter_loops_count_2);          // char[6]
      EEPROM.put( 142, meter_loops_count_3);          // char[6]
      EEPROM.put( 148, meter_loops_count_4);          // char[6]
      EEPROM.end();
    }

// Read settings from eeprom
void ReadEEPROM(void)
{
      EEPROM.begin(1024);
      EEPROM.get( 0, messure_place );                 // char[20]
      EEPROM.get( 20, mqtt_server );                  // char[20]
      EEPROM.get( 40, mqtt_port );                    // char[5]
      EEPROM.get( 45, mqtt_publish );                 // char[5]
      EEPROM.get( 50, mqtt_user );                    // char[20]
      EEPROM.get( 70, mqtt_password );                // char[20]
      EEPROM.get( 90, meter_counter_reading_1);       // char[10]
      EEPROM.get( 100, meter_counter_reading_2);      // char[10]
      EEPROM.get( 110, meter_counter_reading_3);      // char[10]
      EEPROM.get( 120, meter_counter_reading_4);      // char[10]
      EEPROM.get( 130, meter_loops_count_1);          // char[6]
      EEPROM.get( 136, meter_loops_count_2);          // char[6]
      EEPROM.get( 142, meter_loops_count_3);          // char[6]
      EEPROM.get( 148, meter_loops_count_4);          // char[6]
      EEPROM.commit();
      EEPROM.end();

      Serial.println(" --- READ EEPROM --- ");
          Serial.print("messure_place ");
          Serial.println(messure_place);
          Serial.print("mqtt_server ");
          Serial.println(mqtt_server);
          Serial.print("mqtt_port ");
          Serial.println(mqtt_port);
          Serial.print("mqtt_publish ");
          Serial.println(mqtt_publish);
          Serial.print("mqtt_user ");
          Serial.println(mqtt_user);
          Serial.print("mqtt_password ");
          Serial.println(mqtt_password);
          Serial.print("meter_counter_reading_1 ");
          Serial.println(meter_counter_reading_1);
          Serial.print("meter_loops_count_1 ");
          Serial.println(meter_loops_count_1);
          Serial.print("meter_counter_reading_2 ");
          Serial.println(meter_counter_reading_2);
          Serial.println(counter_reading_2);
          Serial.print("meter_loops_count_2 ");
          Serial.println(meter_loops_count_2);
          Serial.print("meter_counter_reading_3 ");
          Serial.println(meter_counter_reading_3);
          Serial.print("meter_loops_count_3 ");
          Serial.println(meter_loops_count_3);
          Serial.print("meter_counter_reading_4 ");
          Serial.println(meter_counter_reading_4);
          Serial.print("meter_loops_count_4 ");
          Serial.println(meter_loops_count_4);
    }
