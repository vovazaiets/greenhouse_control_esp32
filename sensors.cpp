#include "sensors.h"
#include "state.h"
#include "config.h"

#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>

static DHT          dht(PIN_DHT, DHT11);
static Adafruit_BMP280 bmp;
static BH1750       lightMeter;

static bool bmpOk  = false;
static bool bhOk   = false;

void sensorsInit() {
  Wire.begin(21,22);
  dht.begin();

  bmpOk = bmp.begin(0x76);
  if (!bmpOk) bmpOk = bmp.begin(0x76);
  Serial.print("SensorID: 0x");
  Serial.println(bmp.sensorID(), HEX);
  if (!bmpOk) Serial.println(F("[SENSOR] BMP280 not found"));

  bhOk = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  if (!bhOk) Serial.println(F("[SENSOR] BH1750 not found"));

  Serial.println(F("[SENSOR] Init done"));
}


static float soilPercent(int raw) {
  const int DRY = 3200, WET = 800;
  float pct = 100.0f - ((float)(constrain(raw, WET, DRY) - WET) / (DRY - WET) * 100.0f);
  return pct;
}

void taskSensors(void* pv) {
  TickType_t xLastWake = xTaskGetTickCount();

  for (;;) {
    SensorData d;

    // 
    float t = bmp.readTemperature();
    float h = dht.readHumidity();
    d.temperature = isnan(t) ? NAN : t;
    d.humidity    = isnan(h) ? NAN : h;

    // BMP280
    d.pressure = bmpOk ? bmp.readPressure() / 100.0f : NAN;

    // BH1750
    d.lux = bhOk ? lightMeter.readLightLevel() : NAN;

    // Soil
    int soilRaw = analogRead(PIN_SOIL);
    d.soilPct = soilPercent(soilRaw);

    // MQ135
    d.airRaw = (float)analogRead(PIN_MQ135);
    
    stateSetSensors(d);

    Serial.printf("[SENSOR] T=%.1f H=%.1f Lux=%.0f Soil=%.0f Air=%.0f P=%.1f\n",
                  d.temperature, d.humidity, d.lux, d.soilPct, d.airRaw, d.pressure);

    vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}
