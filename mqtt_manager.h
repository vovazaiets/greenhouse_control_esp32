#pragma once
#include <Arduino.h>

void wifiBegin();
void mqttInit();
void taskMQTT(void* pv);
