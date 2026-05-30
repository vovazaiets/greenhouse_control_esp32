#include "mqtt_manager.h"
#include "state.h"
#include "actuators.h"
#include "config.h"
#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 3600;


#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

//  MQTT callback 
static void onMessage(char* topic, byte* payload, unsigned int len) {
  char buf[64];
  len = min(len, (unsigned int)63);
  memcpy(buf, payload, len);
  buf[len] = '\0';
  String t(topic);

  if (t == MQTT_S_PUMP) {
    setPump(String(buf) == "1" || String(buf) == "ON");
  } else if (t == MQTT_S_MODE) {
  if (strcmp(buf, "AUTO") == 0) {
    stateSetMode(Mode::AUTO);
  } else if (strcmp(buf, "MAN") == 0) {
    stateSetMode(Mode::MANUAL);
  }
  }else if (t == MQTT_S_LIGHT) {
    setLight(String(buf) == "1" || String(buf) == "ON");
  } else if (t == MQTT_S_SERVO) {
    setServo(atoi(buf));
  }else if (t == MQTT_S_THRESH) {
    // JSON: {"tMax":30,"hMin":40,"lMin":1000,"sMin":25,"aMax":700}
    Thresholds th = stateGet().thresh;
    auto field = [&](const char* key) -> float {
      char* p = strstr(buf, key);
      if (!p) return NAN;
      p = strchr(p, ':');
      if (!p) return NAN;
      return atof(p + 1);
    };
    float v;
    if (!isnan(v = field("tMax"))) th.tempMax = v;
    if (!isnan(v = field("hMin"))) th.humMin  = v;
    if (!isnan(v = field("lMin"))) th.luxMin  = v;
    if (!isnan(v = field("sMin"))) th.soilMin = v;
    if (!isnan(v = field("aMax"))) th.airMax  = v;
    stateSetThresh(th);
    Serial.println(F("[MQTT] Thresholds updated"));
  }
}

// wifi
void wifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println(F("[WIFI] Connecting..."));
}

void mqttInit() {
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(3);
}

static void mqttReconnect() {
  if (!stateGet().wifiConnected) return;
  if (mqtt.connect(MQTT_CLIENT_ID)) {
    mqtt.subscribe(MQTT_S_PUMP);
    mqtt.subscribe(MQTT_S_LIGHT);
    mqtt.subscribe(MQTT_S_SERVO);
    mqtt.subscribe(MQTT_S_THRESH);
    mqtt.subscribe(MQTT_S_MODE);
    stateSetMqtt(true);
    Serial.println(F("[MQTT] Connected"));
  } else {
    stateSetMqtt(false);
    Serial.printf("[MQTT] Failed rc=%d\n", mqtt.state());
  }
}

static void publishAll(const SystemState& s) {
  char buf[16];
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  auto pub = [&](const char* topic, float val, int dec = 1) {
    if (isnan(val)) return;
    dtostrf(val, 4, dec, buf);
    mqtt.publish(topic, buf, true);
  };

  pub(MQTT_T_TEMP,  s.sensors.temperature);
  pub(MQTT_T_HUM,   s.sensors.humidity);
  pub(MQTT_T_LUX,   s.sensors.lux, 0);
  pub(MQTT_T_SOIL,  s.sensors.soilPct, 0);
  pub(MQTT_T_AIR,   s.sensors.airRaw, 0);
  pub(MQTT_T_PRESS, s.sensors.pressure);
  mqtt.publish(MQTT_T_MODE,
             (s.mode == Mode::AUTO) ? "AUTO" : "MAN",
             true);

  mqtt.publish(MQTT_T_PUMP,  s.actuators.pumpOn  ? "1" : "0", true);
  mqtt.publish(MQTT_T_LIGHT, s.actuators.lightOn  ? "1" : "0", true);
  snprintf(buf, sizeof(buf), "%d", s.actuators.servoAngle);
  mqtt.publish(MQTT_T_SERVO, buf, true);
  // get time and send in mqtt topic 
  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {

    char timeString[64];

    strftime(timeString, sizeof(timeString),
            "%A, %B %d %Y %H:%M:%S",
            &timeinfo);

    Serial.println(timeString);

    mqtt.publish(MQTT_T_TIME, timeString, true);
  }
}
// MQTT Task
void taskMQTT(void* pv) {
  static uint32_t lastPub    = 0;
  static uint32_t lastReconn = 0;

  for (;;) {
    uint32_t now = millis();

    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    if (wifiOk) {
      char ip[16];
      strlcpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip));
      stateSetWifi(true, ip);
    } else {
      stateSetWifi(false);
      stateSetMqtt(false);
    }

    if (wifiOk) {
      if (!mqtt.connected()) {
        if (now - lastReconn > MQTT_RECONN_MS) {
          lastReconn = now;
          mqttReconnect();
        }
      } else {
        mqtt.loop();  
        if (now - lastPub > MQTT_PUB_PERIOD_MS) {
          lastPub = now;
          publishAll(stateGet());
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}
