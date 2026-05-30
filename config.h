#pragma once
#include <Arduino.h>

#define PIN_DHT        4
#define PIN_SOIL      35
#define PIN_MQ135     34
#define PIN_ENC_CLK   32
#define PIN_ENC_DT    25
#define PIN_ENC_SW    33
#define PIN_RELAY_1   26   // полив
#define PIN_RELAY_2   27   // освітлення
#define PIN_SERVO     14

#define WIFI_SSID      "tplink"
#define WIFI_PASSWORD  "fgjkjy11"
#define WIFI_TIMEOUT_MS 8000

//  MQTT 
#define MQTT_BROKER    "192.168.0.103"
#define MQTT_PORT      1883
#define MQTT_CLIENT_ID "greenhouse_esp32"

#define MQTT_T_TEMP    "greenhouse/temp"
#define MQTT_T_HUM     "greenhouse/hum"
#define MQTT_T_LUX     "greenhouse/lux"
#define MQTT_T_SOIL    "greenhouse/soil"
#define MQTT_T_AIR     "greenhouse/air"
#define MQTT_T_PRESS   "greenhouse/pressure"
#define MQTT_T_LIGHT   "greenhouse/light_relay"
#define MQTT_T_PUMP    "greenhouse/pomp_relay"
#define MQTT_T_SERVO   "greenhouse/servo"
#define MQTT_T_TIME   "greenhouse/time"
#define MQTT_T_MODE "greenhouse/mode"


// subscribe 
#define MQTT_S_LIGHT   "greenhouse/light_relay/set"
#define MQTT_S_PUMP    "greenhouse/pomp_relay/set"
#define MQTT_S_SERVO   "greenhouse/servo/set"
#define MQTT_S_THRESH  "greenhouse/thresholds/set"
#define MQTT_S_MODE "greenhouse/mode/set"

#define SENSOR_PERIOD_MS   5000
#define MQTT_PUB_PERIOD_MS 10000
#define MQTT_RECONN_MS     5000
#define CONTROL_PERIOD_MS  3000
#define LCD_REFRESH_MS     500

#define DEF_TEMP_MAX   28.0f
#define DEF_HUM_MIN    50.0f
#define DEF_LUX_MIN    1500.0f
#define DEF_SOIL_MIN   30.0f
#define DEF_AIR_MAX    600.0f  

// RELAY LOGIC (active LOW)
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

//  SERVO angles
#define SERVO_OPEN   90
#define SERVO_CLOSED 0
