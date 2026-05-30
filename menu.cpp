#include "menu.h"
#include "state.h"
#include "actuators.h"
#include "config.h"
#include <LiquidCrystal_I2C.h>

// LCD 
static LiquidCrystal_I2C lcd(0x27, 20, 4);

// Encoder
static volatile int  encDelta  = 0;
static volatile bool encBtn    = false;
static int  lastClk = HIGH;
static portMUX_TYPE encMux = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR isrEncoder() {
  int clk = digitalRead(PIN_ENC_CLK);
  if (clk != lastClk) {
    portENTER_CRITICAL_ISR(&encMux);
    encDelta += (digitalRead(PIN_ENC_DT) != clk) ? 1 : -1;
    portEXIT_CRITICAL_ISR(&encMux);
    lastClk = clk;
  }
}

static int  readDelta() {
  portENTER_CRITICAL(&encMux);
  int d = encDelta; encDelta = 0;
  portEXIT_CRITICAL(&encMux);
  return d;
}

// Button debounce
enum class BtnEvent : uint8_t { NONE, SHORT, LONG };
static BtnEvent readButton() {
  static bool     pressed   = false;
  static uint32_t pressedAt = 0;
  const  uint32_t LONG_MS   = 800;

  bool low = (digitalRead(PIN_ENC_SW) == LOW);
  if (low && !pressed) {
    pressed = true; pressedAt = millis();
  } else if (!low && pressed) {
    pressed = false;
    return (millis() - pressedAt > LONG_MS) ? BtnEvent::LONG : BtnEvent::SHORT;
  }
  return BtnEvent::NONE;
}

//FSM states
enum class MenuState : uint8_t {
  SENSORS,       
  MAIN_MENU,     
  STAGE_LIST,   
  STAGE_EDIT,    
  STATUS         
};

static MenuState menuState = MenuState::SENSORS;

//MAIN MENU items 
static const char* mainItems[]  = { "Sensors", "Stage / Edit", "Status", "Mode: AUTO" };
static const int   MAIN_COUNT   = 4;

// STAGE items 
static const char* stageItems[] = { "Seed", "Grow", "Bloom", "Custom" };
static const int   STAGE_COUNT  = 4;

//Stage edit field labels
static const char* editLabels[] = {
  "Temp Max", "Hum Min", "Lux Min", "Soil Min", "Air Max",
  "Pump Force", "Light Force", "APPLY"
};
static const int EDIT_COUNT = 8;


static int  curMain  = 0, scrollMain  = 0;
static int  curStage = 0, scrollStage = 0;
static int  curEdit  = 0, scrollEdit  = 0;
static int  selectedStage = 1; // etap default

static Thresholds editThresh;
static bool editPump  = false;
static bool editLight = false;

// lcd functns
static void lcdClear() { lcd.clear(); }

static void lcdPrintRow(uint8_t row, const char* fmt, ...) {
  char buf[21];
  va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
  lcd.setCursor(0, row); lcd.print(buf);
}

static void drawList(const char** items, int count, int cursor, int scroll) {
  lcd.clear();
  for (int row = 0; row < 4 && (scroll + row) < count; row++) {
    lcd.setCursor(0, row);
    lcd.print((scroll + row) == cursor ? '>' : ' ');
    lcd.print(items[scroll + row]);
  }
}

// draw lcd
static void drawSensors() {
  SystemState s = stateGet();
  lcd.clear();
  char buf[21];

  snprintf(buf, sizeof(buf), "T:%.1fC  H:%.0f%%", s.sensors.temperature, s.sensors.humidity);
  lcd.setCursor(0,0); lcd.print(buf);

  snprintf(buf, sizeof(buf), "Lux:%-6.0f Gas:%-4.0f", s.sensors.lux, s.sensors.airRaw);
  lcd.setCursor(0,1); lcd.print(buf);

  snprintf(buf, sizeof(buf), "Soil:%.0f%%  P:%.0fhPa", s.sensors.soilPct, s.sensors.pressure);
  lcd.setCursor(0,2); lcd.print(buf);

  snprintf(buf, sizeof(buf), "%s  Pump:%s Lt:%s",
           s.mode == Mode::AUTO ? "AUTO" : "MAN",
           s.actuators.pumpOn  ? "ON" : "OF",
           s.actuators.lightOn ? "ON" : "OF");
  lcd.setCursor(0,3); lcd.print(buf);
}

static void drawMainMenu() {
  // update mode 
  SystemState s = stateGet();
  static char modeLabel[16];
  snprintf(modeLabel, sizeof(modeLabel), "Mode: %s", s.mode == Mode::AUTO ? "AUTO" : "MAN");
  mainItems[3] = modeLabel;
  drawList(mainItems, MAIN_COUNT, curMain, scrollMain);
}

static void drawStageList() {
  drawList(stageItems, STAGE_COUNT, curStage, scrollStage);
}

static float* editField(int idx) {
  switch (idx) {
    case 0: return &editThresh.tempMax;
    case 1: return &editThresh.humMin;
    case 2: return &editThresh.luxMin;
    case 3: return &editThresh.soilMin;
    case 4: return &editThresh.airMax;
    default: return nullptr;
  }
}

static void drawStageEdit() {
  lcd.clear();
  for (int row = 0; row < 3 && (scrollEdit + row) < EDIT_COUNT; row++) {
    int idx = scrollEdit + row;
    lcd.setCursor(0, row);
    lcd.print((idx == curEdit) ? '>' : ' ');
    lcd.print(editLabels[idx]);
    lcd.print(':');
    if (idx < 5) {
      char v[8]; dtostrf(*editField(idx), 4, 0, v);
      lcd.print(v);
    } else if (idx == 5) {
      lcd.print(editPump  ? "ON " : "OFF");
    } else if (idx == 6) {
      lcd.print(editLight ? "ON " : "OFF");
    } else {
      lcd.print("[OK]");
    }
  }
  // 4th row
  lcd.setCursor(0, 3);
  lcd.print("Enc=change Btn=next");
}

static void drawStatus() {
  SystemState s = stateGet();
  lcd.clear();
  lcdPrintRow(0, "WiFi: %s", s.wifiConnected ? "OK" : "---");
  lcdPrintRow(1, "MQTT: %s", s.mqttConnected ? "OK" : "---");
  lcdPrintRow(2, "IP: %s", s.ipStr);
  lcdPrintRow(3, "Long-press=back");
}

static void adjustScroll(int& cursor, int& scroll, int delta, int count, int visible = 4) {
  cursor = constrain(cursor + delta, 0, count - 1);
  if (cursor < scroll) scroll = cursor;
  if (cursor >= scroll + visible) scroll = cursor - visible + 1;
}

static void setThresh(float tMax, float hMin, float lMin, float sMin, float aMax) {
  editThresh.tempMax = tMax;
  editThresh.humMin  = hMin;
  editThresh.luxMin  = lMin;
  editThresh.soilMin = sMin;
  editThresh.airMax  = aMax;
}

static void applyPreset(int stageIdx) {
  switch (stageIdx) {
    case 0: setThresh(25.0f, 70.0f, 1500.0f, 50.0f, 500.0f); break; // etap1
    case 1: setThresh(28.0f, 60.0f, 3000.0f, 40.0f, 600.0f); break; // etap2
    case 2: setThresh(26.0f, 55.0f, 4000.0f, 35.0f, 550.0f); break; // etap3
    default: editThresh = stateGet().thresh;                   break; // etap4
  }
  editPump  = false;
  editLight = false;
  curEdit   = 0;
  scrollEdit = 0;
}

static uint32_t lastDraw = 0;
static bool     needDraw = true;

static void menuStep() {
  int       delta = readDelta();
  BtnEvent  btn   = readButton();

  switch (menuState) {

    // sensors
    case MenuState::SENSORS:
      if (delta != 0 || btn == BtnEvent::SHORT) {
        menuState = MenuState::MAIN_MENU;
        needDraw = true;
      }
      break;

    case MenuState::MAIN_MENU:
      if (delta) { adjustScroll(curMain, scrollMain, delta, MAIN_COUNT); needDraw = true; }
      if (btn == BtnEvent::SHORT) {
        if (curMain == 0) { menuState = MenuState::SENSORS; }
        else if (curMain == 1) {
          curStage = 0; scrollStage = 0;
          menuState = MenuState::STAGE_LIST;
        }
        else if (curMain == 2) { menuState = MenuState::STATUS; }
        else if (curMain == 3) {
          xSemaphoreTake(gStateMutex, portMAX_DELAY);
          gState.mode = (gState.mode == Mode::AUTO) ? Mode::MANUAL : Mode::AUTO;
          xSemaphoreGive(gStateMutex);
        }
        needDraw = true;
      }
      if (btn == BtnEvent::LONG) { menuState = MenuState::SENSORS; needDraw = true; }
      break;

    case MenuState::STAGE_LIST:
      if (delta) { adjustScroll(curStage, scrollStage, delta, STAGE_COUNT); needDraw = true; }
      if (btn == BtnEvent::SHORT) {
        selectedStage = curStage;
        applyPreset(selectedStage);
        menuState = MenuState::STAGE_EDIT;
        needDraw = true;
      }
      if (btn == BtnEvent::LONG) { menuState = MenuState::MAIN_MENU; needDraw = true; }
      break;

    case MenuState::STAGE_EDIT: {
      if (delta) {
        if (curEdit < 5) {
          float* f = editField(curEdit);
          if (f) *f += delta;
        } else if (curEdit == 5) { editPump  = !editPump; }
        else if (curEdit == 6) { editLight = !editLight; }
        needDraw = true;
      }
      if (btn == BtnEvent::SHORT) {
        if (curEdit == EDIT_COUNT - 1) {
          // APPLY
          stateSetThresh(editThresh);
          setPump(editPump);
          setLight(editLight);
          xSemaphoreTake(gStateMutex, portMAX_DELAY);
          gState.stage = (Stage)selectedStage;
          xSemaphoreGive(gStateMutex);
          Serial.printf("[MENU] Applied stage %d\n", selectedStage);
          menuState = MenuState::SENSORS;
        } else {
          adjustScroll(curEdit, scrollEdit, 1, EDIT_COUNT, 3);
        }
        needDraw = true;
      }
      if (btn == BtnEvent::LONG) { menuState = MenuState::STAGE_LIST; needDraw = true; }
      break;
    }

    case MenuState::STATUS:
      if (btn == BtnEvent::LONG || btn == BtnEvent::SHORT) {
        menuState = MenuState::MAIN_MENU; needDraw = true;
      }
      break;
  }
}

static void menuDraw() {
  switch (menuState) {
    case MenuState::SENSORS:    drawSensors();   break;
    case MenuState::MAIN_MENU:  drawMainMenu();  break;
    case MenuState::STAGE_LIST: drawStageList(); break;
    case MenuState::STAGE_EDIT: drawStageEdit(); break;
    case MenuState::STATUS:     drawStatus();    break;
  }
}

// init 
void menuInit() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT,  INPUT_PULLUP);
  pinMode(PIN_ENC_SW,  INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), isrEncoder, CHANGE);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print(F("Greenhouse v1.0"));
  vTaskDelay(pdMS_TO_TICKS(1200));

  editThresh = stateGet().thresh;
  Serial.println(F("[MENU] Ready"));
}

// Menu Task 
void taskMenu(void* pv) {
  static uint32_t lastSensorRefresh = 0;

  for (;;) {
    menuStep();

    uint32_t now = millis();

    if (menuState == MenuState::SENSORS && now - lastSensorRefresh > LCD_REFRESH_MS) {
      lastSensorRefresh = now;
      needDraw = true;
    }

    if (needDraw) {
      menuDraw();
      needDraw = false;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
