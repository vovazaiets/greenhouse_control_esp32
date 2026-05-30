#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

// mode
enum class Mode : uint8_t { MANUAL, AUTO };

// presets
enum class Stage : uint8_t { SEED, GROW, BLOOM, CUSTOM };

// Thresholds 
struct Thresholds {
  float tempMax  = DEF_TEMP_MAX;
  float humMin   = DEF_HUM_MIN;
  float luxMin   = DEF_LUX_MIN;
  float soilMin  = DEF_SOIL_MIN;
  float airMax   = DEF_AIR_MAX;
};

struct SensorData {
  float temperature = NAN;
  float humidity    = NAN;
  float lux         = NAN;
  float soilPct     = NAN; 
  float airRaw      = NAN; 
  float pressure    = NAN; 
};

struct ActuatorState {
  bool  pumpOn   = false;
  bool  lightOn  = false;
  int   servoAngle = SERVO_CLOSED;
};


struct SystemState {
  SensorData    sensors;
  ActuatorState actuators;
  Thresholds    thresh;
  Mode          mode  = Mode::AUTO;
  Stage         stage = Stage::GROW;
  bool          wifiConnected  = false;
  bool          mqttConnected  = false;
  char          ipStr[16]      = "0.0.0.0";
};

extern SystemState gState;
extern SemaphoreHandle_t gStateMutex;

void stateInit();

inline SystemState stateGet() {
  SystemState s;
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  s = gState;
  xSemaphoreGive(gStateMutex);
  return s;
}

inline void stateSetSensors(const SensorData& d) {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.sensors = d;
  xSemaphoreGive(gStateMutex);
}

inline void stateSetActuators(const ActuatorState& a) {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.actuators = a;
  xSemaphoreGive(gStateMutex);
}

inline void stateSetThresh(const Thresholds& t) {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.thresh = t;
  xSemaphoreGive(gStateMutex);
}

inline void stateSetMode(Mode m) {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.mode = m;
  xSemaphoreGive(gStateMutex);
}

inline void stateSetWifi(bool v, const char* ip = nullptr) {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.wifiConnected = v;
  if (ip) strlcpy(gState.ipStr, ip, sizeof(gState.ipStr));
  xSemaphoreGive(gStateMutex);
}

inline void stateSetMqtt(bool v) {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.mqttConnected = v;
  xSemaphoreGive(gStateMutex);
}
