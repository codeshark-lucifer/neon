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
#include <spectrum.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define BTN_UP 25
#define BTN_DOWN 32
#define BTN_OK 33

#define OLED_SDA 4
#define OLED_SCL 5

int active = -1;
bool needsRedraw = true;

float animSelectIdx = 0; // For smooth selection animation
unsigned long lastRedraw = 0;

SPIClass hspi(HSPI);
SPIClass vspi(VSPI);

void scan_action();
void deauth_target_action();
void blejam_action();
void jammer_2_4_action();
void spectrum_action();

using Func = void (*)();
struct Action
{
  const char *name;
  const char *icon; // Added icon field
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
  SCAN_MODE,
  SPECTRUM_MODE
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
    actions.push_back({"SCANNER", "S", scan_action});
    actions.push_back({"DEAUTH", "X", deauth_target_action});
    actions.push_back({"2.4 JAMMER", "J", jammer_2_4_action});
    actions.push_back({"BLE JAMMER", "B", blejam_action});
    actions.push_back({"SPECTRUM", "G", spectrum_action});
  }

  void draw(Adafruit_SSD1306 *display)
  {
    unsigned long now = millis();
    // Update animation: ease animSelectIdx toward selectIdx
    if (abs(animSelectIdx - selectIdx) > 0.01) {
      animSelectIdx += (selectIdx - animSelectIdx) * 0.3;
      needsRedraw = true;
    } else {
      animSelectIdx = selectIdx;
    }

    if (!needsRedraw && (now - lastRedraw < 30))
      return;
    needsRedraw = false;
    lastRedraw = now;

    display->clearDisplay();

    // Stylized Header
    display->fillRect(0, 0, 128, 10, SSD1306_WHITE);
    display->setTextColor(SSD1306_BLACK);
    display->setTextSize(1);
    display->setCursor(2, 1);
    display->print(F("NEON PROJECT"));
    
    display->setCursor(85, 1);
    if (mode == DEAUTH_MODE) display->print(F("DEAUTH"));
    else if (mode == BLEJAM_MODE) display->print(F("BLEJAM"));
    else if (mode == WIFIJAM_MODE) display->print(F("W-JAM"));
    else if (mode == SPECTRUM_MODE) display->print(F("SPEC"));
    else if (mode == SCAN_MODE) display->print(F("SCAN"));
    else display->print(F("IDLE"));

    display->setTextColor(SSD1306_WHITE);

    if (mode == SCAN_MODE)
    {
      display->setCursor(20, 30);
      display->print(F("SCANNING WIFI..."));
      static int bar = 0;
      display->drawRect(20, 45, 88, 8, SSD1306_WHITE);
      display->fillRect(22, 47, (bar % 84), 4, SSD1306_WHITE);
      bar += 2;
      display->display();
      needsRedraw = true;
      return;
    }

    if (mode == DEAUTH_MODE)
    {
      display->setCursor(0, 15);
      display->print(F("TARGET: "));
      display->println(selectedTarget.ssid.substring(0, 15));

      display->drawRoundRect(5, 28, 118, 32, 4, SSD1306_WHITE);
      display->setTextSize(2);
      display->setCursor(15, 38);
      display->print(F("SENT: "));
      display->print(eliminated_stations);

      display->setTextSize(1);
      if ((millis() / 500) % 2 == 0)
      {
        display->fillRect(100, 15, 25, 10, SSD1306_WHITE);
        display->setTextColor(SSD1306_BLACK);
        display->setCursor(102, 16);
        display->print(F("TX"));
        display->setTextColor(SSD1306_WHITE);
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

      int b1 = map(current_ch[0], 0, 81, 0, 108);
      int b2 = map(current_ch[1], 0, 81, 0, 108);

      display->drawRect(10, 30, 108, 6, SSD1306_WHITE);
      display->fillRect(10, 30, b1, 6, SSD1306_WHITE);
      display->drawRect(10, 42, 108, 6, SSD1306_WHITE);
      display->fillRect(10, 42, b2, 6, SSD1306_WHITE);

      display->setCursor(10, 52);
      display->printf("CH A:%02d  CH B:%02d", current_ch[0], current_ch[1]);
      display->display();
      needsRedraw = true;
      return;
    }

    if (mode == SPECTRUM_MODE)
    {
      // Draw frequency markers
      display->setCursor(0, 11);
      display->print("2.4G");
      display->setCursor(100, 11);
      display->print("2.5G");

      // Grid lines for common WiFi channels
      for(int ch : {1, 6, 11}) {
         int x = map(ch, 1, 14, 0, 127);
         for(int y=20; y<64; y+=4) display->drawPixel(x, y, SSD1306_WHITE);
      }

      static byte peak[128] = {0};
      for (int i = 0; i < 128; i++)
      {
        int val = spec_ch[i];
        if (val > peak[i]) peak[i] = val;
        else if (peak[i] > 0 && (millis() % 2 == 0)) peak[i]--;

        int barHeight = map(val, 0, 15, 0, 40);
        int peakHeight = map(peak[i], 0, 15, 0, 40);
        if (barHeight > 42) barHeight = 42;
        if (peakHeight > 42) peakHeight = 42;

        display->drawLine(i, 63, i, 63 - barHeight, SSD1306_WHITE);
        display->drawPixel(i, 63 - peakHeight, SSD1306_WHITE);
      }
      display->display();
      needsRedraw = true;
      return;
    }

    if (subMenuIdx == -1)
    {
      // Main Menu with smooth animation
      int yStart = 14;
      int itemHeight = 10;
      
      // Draw selection bar background
      int barY = yStart + (animSelectIdx * itemHeight) - 1;
      display->fillRect(0, barY, 128, itemHeight, SSD1306_WHITE);

      for (int i = 0; i < actions.size(); i++)
      {
        int y = yStart + (i * itemHeight);
        bool isSelected = (i == selectIdx);
        
        // Dynamic text color based on proximity to selection bar
        if (abs(animSelectIdx - i) < 0.5) {
           display->setTextColor(SSD1306_BLACK);
        } else {
           display->setTextColor(SSD1306_WHITE);
        }

        display->setCursor(15, y);
        display->print(actions[i].name);
        
        // Icon
        display->setCursor(2, y);
        display->print(actions[i].icon);

        if (active == i)
        {
          display->setCursor(110, y);
          display->print(F("<"));
        }
      }
    }
    else
    {
      // AP Selection List
      display->setCursor(0, 11);
      display->setTextColor(SSD1306_WHITE);
      display->print(F("SELECT AP:"));
      display->drawLine(0, 20, 128, 20, SSD1306_WHITE);

      int start = (selectIdx / 4) * 4;
      for (int i = start; i < aps.size() && i < start + 4; i++)
      {
        int y = 24 + ((i - start) * 10);
        if (i == selectIdx) {
          display->fillRect(0, y-1, 128, 10, SSD1306_WHITE);
          display->setTextColor(SSD1306_BLACK);
        } else {
          display->setTextColor(SSD1306_WHITE);
        }
        display->setCursor(2, y);
        display->print(aps[i].ssid.substring(0, 16));

        // RSSI Bar
        int bars = map(aps[i].rssi, -100, -30, 1, 5);
        if (bars < 1) bars = 1;
        if (bars > 5) bars = 5;
        for (int b = 0; b < bars; b++) {
          display->fillRect(110 + (b * 3), y + 6 - b, 2, b + 1, (i == selectIdx) ? SSD1306_BLACK : SSD1306_WHITE);
        }
      }
    }
    display->display();
  }

  void move(DIRECTION dir)
  {
    int limit = (subMenuIdx == -1) ? actions.size() : aps.size();
    if (limit == 0) return;

    if (dir == UP) selectIdx--;
    else selectIdx++;

    if (selectIdx < 0) selectIdx = limit - 1;
    if (selectIdx >= limit) selectIdx = 0;
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
        selectIdx = 1; // Select Deauth by default after choosing target
      }
    }
    needsRedraw = true;
  }

  void handle_loop()
  {
    switch (mode)
    {
    case BLEJAM_MODE: handle_ble_jamming(); break;
    case WIFIJAM_MODE: handle_2_4_jamming(); break;
    case DEAUTH_MODE: handle_deauth(selectedTarget); break;
    case SPECTRUM_MODE: handle_spectrum(); break;
    }
  }

  void reset()
  {
    if (mode == BLEJAM_MODE) stop_ble_jam();
    if (mode == WIFIJAM_MODE) stop_2_4_jammer();
    if (mode == DEAUTH_MODE) stop_deauth();
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
    // Maybe trigger scan if no target?
    scan_action();
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
  active = 3;
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
  active = 2;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  start_2_4_jammer();
}

void spectrum_action()
{
  if (menu.mode != NONE)
  {
    menu.reset();
    return;
  }
  menu.mode = SPECTRUM_MODE;
  active = 4;
  start_spectrum();
}

void HandleInput()
{
  static unsigned long lastPress = 0;
  static unsigned long okPressedAt = 0;
  static bool okHeld = false;
  
  unsigned long now = millis();

  bool upPressed = (digitalRead(BTN_UP) == LOW);
  bool downPressed = (digitalRead(BTN_DOWN) == LOW);
  bool okPressed = (digitalRead(BTN_OK) == LOW);

  if (now - lastPress < 150) return;

  if (upPressed) {
    menu.move(UP);
    lastPress = now;
  } else if (downPressed) {
    menu.move(DOWN);
    lastPress = now;
  }

  if (okPressed) {
    if (okPressedAt == 0) okPressedAt = now;
    if (now - okPressedAt > 800 && !okHeld) {
      // Long press OK - Reset/Back
      menu.reset();
      if (menu.subMenuIdx != -1) {
        menu.subMenuIdx = -1;
        menu.selectIdx = 0;
      }
      okHeld = true;
      lastPress = now;
    }
  } else {
    if (okPressedAt != 0 && !okHeld) {
      // Short press OK
      menu.select();
      lastPress = now;
    }
    okPressedAt = 0;
    okHeld = false;
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
