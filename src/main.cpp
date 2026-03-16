#include <vector>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

#include "esp_bt.h"
#include "esp_wifi.h"

#include <definitions.h>
#include <config.h>
#include <blejam.h>
#include <jammer.h>
#include <deauth.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BTN_UP 25
#define BTN_DOWN 32
#define BTN_OK 33

#define OLED_SDA 4
#define OLED_SCL 5

int active = -1;
bool needsRedraw = true;

SPIClass hspi(HSPI);
SPIClass vspi(VSPI);

void scan_action();
void deauth_target_action();
void blejam_action();
void jammer_2_4_action();

using Func = void (*)();
struct Action
{
  const char *name;
  Func action;
};

enum DIRECTION
{
  UP,
  DOWN
};

enum MODE
{
  NONE,
  BLEJAM_MODE,
  WIFIJAM_MODE,
  DEAUTH_MODE,
  SCAN_MODE
};

struct Menu
{
  int selectIdx = 0;
  MODE mode = NONE;
  std::vector<Action> actions;
  std::vector<AP> aps;
  AP selectedTarget;
  bool hasTarget = false;
  int subMenuIdx = -1;

  void init()
  {
    actions.push_back({"SCANNER", scan_action});
    actions.push_back({"DEAUTH", deauth_target_action});
    actions.push_back({"2.4 JAMMER", jammer_2_4_action});
    actions.push_back({"BLE JAMMER", blejam_action});
  }

  void draw(Adafruit_SSD1306 *display)
  {
    if (!needsRedraw)
      return;
    needsRedraw = false;

    display->clearDisplay();

    // Header
    display->setTextColor(SSD1306_WHITE);
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->print(F("NEON v1.0"));
    display->setCursor(80, 0);
    if (mode == DEAUTH_MODE)
      display->print(F("[DEAUTH]"));
    else if (mode == BLEJAM_MODE)
      display->print(F("[BLEJAM]"));
    else if (mode == WIFIJAM_MODE)
      display->print(F("[WIFIJAM]"));
    else
      display->print(F("[IDLE]"));
    display->drawLine(0, 9, 128, 9, SSD1306_WHITE);

    if (mode == SCAN_MODE)
    {
      display->setCursor(20, 30);
      display->print(F("SCANNING WIFI..."));
      // Simple scanning animation
      static int bar = 0;
      display->drawRect(20, 45, 88, 8, SSD1306_WHITE);
      display->fillRect(22, 47, (bar % 84), 4, SSD1306_WHITE);
      bar += 10;
      display->display();
      return;
    }

    if (mode == DEAUTH_MODE)
    {
      display->setCursor(0, 15);
      display->print(F("TARGET: "));
      display->println(selectedTarget.ssid.substring(0, 12));

      display->drawRoundRect(5, 28, 118, 32, 4, SSD1306_WHITE);
      display->setTextSize(2);
      display->setCursor(15, 35);
      display->print(F("SENT: "));
      display->print(eliminated_stations);

      display->setTextSize(1);
      if ((millis() / 500) % 2 == 0)
      {
        display->setCursor(85, 15);
        display->print(F("*TX*"));
      }
      display->display();
      return;
    }

    if (mode == BLEJAM_MODE || mode == WIFIJAM_MODE)
    {
      display->setCursor(0, 15);
      uint8_t current_ch[2];
      if (mode == BLEJAM_MODE)
      {
        display->println(F("BLE NOISE BURSTS"));
        current_ch[0] = ble_ch[0];
        current_ch[1] = ble_ch[1];
      }
      else
      {
        display->println(F("WIFI LAYER1 NOISE"));
        current_ch[0] = ch[0];
        current_ch[1] = ch[1];
      }

      // Visual bars for channels
      int b1 = map(current_ch[0], 0, 81, 0, 100);
      int b2 = map(current_ch[1], 0, 81, 0, 100);

      display->drawRect(10, 30, 108, 6, SSD1306_WHITE);
      display->fillRect(10, 30, b1, 6, SSD1306_WHITE);
      display->drawRect(10, 42, 108, 6, SSD1306_WHITE);
      display->fillRect(10, 42, b2, 6, SSD1306_WHITE);

      display->setCursor(10, 52);
      display->printf("CH1:%02d  CH2:%02d", current_ch[0], current_ch[1]);
      display->display();
      return;
    }

    if (subMenuIdx == -1)
    {
      // Main Menu
      for (int i = 0; i < actions.size(); i++)
      {
        int y = 14 + (i * 12);
        if (i == selectIdx)
        {
          display->fillRect(0, y - 1, 128, 11, SSD1306_WHITE);
          display->setTextColor(SSD1306_BLACK);
        }
        else
        {
          display->setTextColor(SSD1306_WHITE);
        }

        display->setCursor(5, y);
        display->print(actions[i].name);

        if (active == i)
        {
          display->setCursor(110, y);
          display->print(F("[A]"));
        }
      }
    }
    else
    {
      // AP Selection List
      display->setCursor(0, 11);
      display->print(F("SELECT AP:"));
      display->drawLine(0, 20, 60, 20, SSD1306_WHITE);

      int start = (selectIdx / 4) * 4;
      for (int i = start; i < aps.size() && i < start + 4; i++)
      {
        int y = 24 + ((i - start) * 10);
        if (i == selectIdx)
        {
          display->drawRect(0, y - 1, 128, 10, SSD1306_WHITE);
        }
        display->setCursor(2, y);
        display->print(aps[i].ssid.substring(0, 14));

        // RSSI Bar
        int bars = map(aps[i].rssi, -100, -30, 1, 5);
        for (int b = 0; b < bars; b++)
        {
          display->fillRect(110 + (b * 3), y + 6 - (b * 1), 2, b + 1, SSD1306_WHITE);
        }
      }
    }
    display->display();
  }

  void move(DIRECTION dir)
  {
    int limit = (subMenuIdx == -1) ? actions.size() : aps.size();
    if (limit == 0)
      return;

    if (dir == UP)
      selectIdx--;
    else
      selectIdx++;

    if (selectIdx < 0)
      selectIdx = limit - 1;
    if (selectIdx >= limit)
      selectIdx = 0;
    needsRedraw = true;
  }

  void select()
  {
    if (subMenuIdx == -1)
    {
      if (actions[selectIdx].action)
        actions[selectIdx].action();
    }
    else
    {
      if (aps.size() > 0)
      {
        selectedTarget = aps[selectIdx];
        hasTarget = true;
        subMenuIdx = -1;
        selectIdx = 0;
      }
    }
    needsRedraw = true;
  }

  void handle_loop()
  {
    switch (mode)
    {
    case BLEJAM_MODE:
      handle_ble_jamming();
      break;
    case WIFIJAM_MODE:
      handle_2_4_jamming();
      break;
    case DEAUTH_MODE:
      handle_deauth(selectedTarget);
      break;
    }
  }

  void reset()
  {
    if (mode == BLEJAM_MODE)
      stop_ble_jam();
    if (mode == WIFIJAM_MODE)
      stop_2_4_jammer();
    if (mode == DEAUTH_MODE)
      stop_deauth();
    mode = NONE;
    active = -1;
    needsRedraw = true;
  }
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Menu menu;

void scan_action()
{
  if (menu.mode != NONE)
  {
    menu.reset();
    return;
  }
  menu.mode = SCAN_MODE;
  menu.draw(&display);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  menu.aps.clear();
  for (int i = 0; i < n; i++)
  {
    AP ap;
    ap.ssid = WiFi.SSID(i);
    ap.ch = WiFi.channel(i);
    ap.rssi = WiFi.RSSI(i);
    ap.index = i;
    memcpy(ap.bssid, WiFi.BSSID(i), 6);
    menu.aps.push_back(ap);
  }
  menu.mode = NONE;
  menu.subMenuIdx = 0;
  menu.selectIdx = 0;
  needsRedraw = true;
}

void deauth_target_action()
{
  if (menu.mode != NONE)
  {
    menu.reset();
    return;
  }
  if (!menu.hasTarget)
  {
    Serial.println("No target selected!");
    return;
  }
  menu.mode = DEAUTH_MODE;
  active = 1;
  start_deauth(menu.selectedTarget.index, DEAUTH_TYPE_SINGLE, 7);
  needsRedraw = true;
}

void blejam_action()
{
  if (menu.mode != NONE)
  {
    menu.reset();
    return;
  }
  menu.mode = BLEJAM_MODE;
  active = 2;
  start_ble_jam();
}

void jammer_2_4_action()
{
  if (menu.mode != NONE)
  {
    menu.reset();
    return;
  }
  menu.mode = WIFIJAM_MODE;
  active = 3;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  start_2_4_jammer();
}

void HandleInput()
{
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  if (now - lastPress < 200)
    return;

  if (digitalRead(BTN_UP) == LOW)
  {
    menu.move(UP);
    lastPress = now;
  }
  if (digitalRead(BTN_DOWN) == LOW)
  {
    menu.move(DOWN);
    lastPress = now;
  }
  if (digitalRead(BTN_OK) == LOW)
  {
    menu.select();
    lastPress = now;
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();

  Wire.begin(OLED_SDA, OLED_SCL); // SDA, SCL
  Wire.setClock(400000);          // Fast I2C mode
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED failed");
    while (1)
      ;
  }
  display.clearDisplay();
  display.display();

  // Pass -1 for SS pin to let RF24 library handle CSN manually
  hspi.begin(RF1_SCK, RF1_MISO, RF1_MOSI, -1);
  // hspi.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  
  vspi.begin(RF2_SCK, RF2_MISO, RF2_MOSI, -1);

  init_jammer_hardware(&hspi, &vspi);
  init_ble_hardware(&hspi, &vspi);

  menu.init();
}

void loop()
{
  HandleInput();
  menu.handle_loop();
  menu.draw(&display);
}
