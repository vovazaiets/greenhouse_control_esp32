#include "config.h"
#include "sensors.h"
#include "actuators.h"
#include "menu.h"
#include "mqtt_manager.h"
#include "state.h"
#include "time.h"

//rtos handles
TaskHandle_t hTaskSensors   = nullptr;
TaskHandle_t hTaskControl   = nullptr;
TaskHandle_t hTaskMQTT      = nullptr;
TaskHandle_t hTaskMenu      = nullptr;

void setup() {
  Serial.begin(115200);
  Serial.println(F("[BOOT] Greenhouse starting..."));

  stateInit();
  sensorsInit();
  actuatorsInit();
  menuInit();
  wifiBegin();       
  mqttInit();

  xTaskCreatePinnedToCore(taskSensors,  "Sensors",  3072, nullptr, 2, &hTaskSensors,  1);
  xTaskCreatePinnedToCore(taskControl,  "Control",  2048, nullptr, 2, &hTaskControl,  1);
  xTaskCreatePinnedToCore(taskMQTT,     "MQTT",     4096, nullptr, 1, &hTaskMQTT,     0);
  xTaskCreatePinnedToCore(taskMenu,     "Menu",     3072, nullptr, 3, &hTaskMenu,     0);

  Serial.println(F("[BOOT] Tasks created."));
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
