#pragma once
#include <Arduino.h>

void actuatorsInit();
void setPump(bool on);
void setLight(bool on);
void setServo(int angle);
void taskControl(void* pv);
