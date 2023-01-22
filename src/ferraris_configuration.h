#pragma once

#include <Arduino.h>


extern bool saveConfig;


// get configuration from web page into program logic
void parseConfigurationPage();
void updateDashboardFromConfiguration();

// MQTT subscribe callback
void parseMQTTmessage(char* topic, byte* payload, unsigned int length);
