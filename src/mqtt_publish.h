#pragma once

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

void checkMQTTconnection(void);
void publishMQTT(void);
