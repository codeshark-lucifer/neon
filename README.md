# NEON - ESP32 Dual nRF24L01 Jammer & Deauther

NEON is a powerful multi-functional tool built on the ESP32 platform, utilizing dual nRF24L01+ modules for high-speed 2.4GHz and BLE jamming, alongside native WiFi deauthentication capabilities. It features a clean OLED-based UI for easy navigation.

## 🛠 Hardware Connection Setup

To ensure stability, especially during high-power transmission, it is **CRITICAL** to use a **100µF capacitor** soldered directly across the **VCC and GND** pins of each nRF24L01+ module. This prevents voltage drops that cause the modules to crash or perform poorly.

### Pin Mapping

#### ESP32 to nRF24L01+ (Radio 1 - HSPI)
| nRF24L01 Pin | ESP32 Pin | Function |
| :--- | :--- | :--- |
| VCC | 3.3V | Power (Add 100µF Cap) |
| GND | GND | Ground |
| CE | 22 | Chip Enable |
| CSN | 21 | Chip Select |
| SCK | 18 | SPI Clock |
| MISO | 19 | SPI MISO |
| MOSI | 23 | SPI MOSI |

#### ESP32 to nRF24L01+ (Radio 2 - VSPI)
| nRF24L01 Pin | ESP32 Pin | Function |
| :--- | :--- | :--- |
| VCC | 3.3V | Power (Add 100µF Cap) |
| GND | GND | Ground |
| CE | 16 | Chip Enable |
| CSN | 15 | Chip Select |
| SCK | 14 | SPI Clock |
| MISO | 12 | SPI MISO |
| MOSI | 13 | SPI MOSI |

#### ESP32 to SSD1306 OLED (I2C)
| OLED Pin | ESP32 Pin | Function |
| :--- | :--- | :--- |
| VCC | 3.3V | Power |
| GND | GND | Ground |
| SDA | 4 | I2C Data |
| SCL | 5 | I2C Clock |

#### Navigation Buttons
| Button | ESP32 Pin |
| :--- | :--- |
| UP | 25 |
| DOWN | 32 |
| OK / SELECT | 33 |

---

## 💻 How the Software Works

The software is designed to maximize the hardware's potential by running parallel tasks and utilizing the dual SPI buses of the ESP32.

### 1. Dual-Channel Jamming (Layer 1)
- **HSPI & VSPI:** The code initializes two separate SPI buses to communicate with both nRF24L01 modules simultaneously. This allows the system to jam two different frequencies or sweep through the spectrum twice as fast.
- **2.4GHz Mode:** Both radios generate raw "noise" packets and hop across 80 different channels in the 2.4GHz ISM band. This creates significant interference for WiFi (802.11b/g/n) and other proprietary 2.4GHz protocols.
- **BLE Mode:** Radio 1 focuses on the three primary BLE advertising channels (37, 38, 39) to prevent devices from discovering each other. Radio 2 performs a high-speed sweep across the data channels to disrupt existing connections.

### 2. WiFi Deauthentication (Layer 2)
- Unlike the nRF24L01 jamming (which is physical noise), the Deauther uses the ESP32's internal WiFi chip to send **802.11 deauthentication frames**.
- It scans for nearby Access Points (APs), allows you to select a target, and then sends packets that "spoof" the AP, telling connected clients to disconnect.

### 3. User Interface & Menu System
- The UI is built using the **Adafruit SSD1306** library.
- A custom `Menu` structure handles the logic for scrolling, selecting targets, and switching between operation modes.
- Buttons are handled with internal pull-ups and software debouncing to ensure responsive navigation.

### 4. Safety & Performance
- The code disables Bluetooth and WiFi when starting the nRF24L01 jammer to reduce internal heat and power consumption.
- It uses `yield()` and `delayMicroseconds()` strategically to prevent the ESP32's Watchdog Timer (WDT) from triggering during high-speed loops.

---

## 🚀 Getting Started

1.  Install [PlatformIO](https://platformio.org/).
2.  Connect your hardware according to the pin mapping above.
3.  **Don't forget the 100µF capacitors!**
4.  Flash the firmware to your ESP32.
5.  Use the UP/DOWN buttons to navigate and OK to start an action.

*Disclaimer: This tool is for educational and testing purposes only. Use it responsibly and only on networks you own or have permission to test.*
