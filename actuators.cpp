#include "actuators.h"
#include "state.h"
#include "config.h"
#include <ESP32Servo.h>

static Servo vent;

void actuatorsInit() {
  pinMode(PIN_RELAY_1, OUTPUT);
  pinMode(PIN_RELAY_2, OUTPUT);
  digitalWrite(PIN_RELAY_1, RELAY_OFF);
  digitalWrite(PIN_RELAY_2, RELAY_OFF);

  ESP32PWM::allocateTimer(0);
  vent.setPeriodHertz(50);
  vent.attach(PIN_SERVO, 500, 2400);
  vent.write(SERVO_CLOSED);

  Serial.println(F("[ACT] Actuators ready"));
}

void setPump(bool on) {
  digitalWrite(PIN_RELAY_1, on ? RELAY_ON : RELAY_OFF);
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.actuators.pumpOn = on;
  xSemaphoreGive(gStateMutex);
  Serial.printf("[ACT] Pump %s\n", on ? "ON" : "OFF");
}

void setLight(bool on) {
  digitalWrite(PIN_RELAY_2, on ? RELAY_ON : RELAY_OFF);
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.actuators.lightOn = on;
  xSemaphoreGive(gStateMutex);
  Serial.printf("[ACT] Light %s\n", on ? "ON" : "OFF");
}

void setServo(int angle) {
  angle = constrain(angle, 0, 180);
  vent.write(angle);
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  gState.actuators.servoAngle = angle;
  xSemaphoreGive(gStateMutex);
  Serial.printf("[ACT] Servo %d°\n", angle);
}

void taskControl(void* pv) {
  TickType_t xLastWake = xTaskGetTickCount();

  for (;;) {
    SystemState s = stateGet();

    if (s.mode == Mode::AUTO) {
      // Вентиляція
      bool ventOpen = (!isnan(s.sensors.temperature) && s.sensors.temperature > s.thresh.tempMax)
                   || (!isnan(s.sensors.airRaw)       && s.sensors.airRaw       > s.thresh.airMax);
      setServo(ventOpen ? SERVO_OPEN : SERVO_CLOSED);

      // Lighting: low lux
      if (!isnan(s.sensors.lux)) {
        setLight(s.sensors.lux < s.thresh.luxMin);
      }

      // Полив
      if (!isnan(s.sensors.soilPct)) {
        setPump(s.sensors.soilPct < s.thresh.soilMin);
      }
    }

    vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
  }
}
