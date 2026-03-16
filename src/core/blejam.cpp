#include "blejam.h"
#include "config.h"

// Initialize radios with pins defined in config.h
RF24 bleRadio1(RF1_CE, RF1_CSN);
RF24 bleRadio2(RF2_CE, RF2_CSN);

SPIClass *ble_hspi = nullptr;
SPIClass *ble_vspi = nullptr;

bool ble_jamming_active = false;
uint8_t ble_ch[2] = {2, 2}; // Channels for Radio 1 and 2

// BLE Advertising channels translated to nRF24 frequencies
const uint8_t adv_channels[] = {2, 26, 80};
uint8_t adv_idx = 0;
uint8_t sweep_idx = 2;

void init_ble_hardware(SPIClass *h, SPIClass *v)
{
  ble_hspi = h;
  ble_vspi = v;
}

extern bool needsRedraw;

void start_ble_jam() {
    if (!ble_hspi || !ble_vspi) return;

    bleRadio1.begin(ble_hspi);
    bleRadio2.begin(ble_vspi);

    RF24 *radios[] = {&bleRadio1, &bleRadio2};
    for (int i = 0; i < 2; i++) {
        radios[i]->setAddressWidth(3); // Shortest address = faster
        radios[i]->setAutoAck(false);
        radios[i]->setRetries(0, 0);
        radios[i]->setDataRate(RF24_2MBPS);
        radios[i]->setPALevel(RF24_PA_MAX, true);
        // radios[i]->setPALevel(RF24_PA_LOW, true);
        radios[i]->setCRCLength(RF24_CRC_DISABLED);
        radios[i]->stopListening();
    }

    ble_jamming_active = true;
}

void stop_ble_jam()
{
  ble_jamming_active = false;
  bleRadio1.stopConstCarrier();
  bleRadio2.stopConstCarrier();
  bleRadio1.powerDown();
  bleRadio2.powerDown();
  Serial.println("BLE JAMMER: Stopped");
}

void handle_ble_jamming() {
    if (!ble_jamming_active) return;

    static const uint8_t ble_adv_channels[] = {2, 26, 80}; // BLE 37, 38, 39
    static uint8_t adv_ptr = 0;
    static uint8_t sweep_ch = 2;
    
    // 32 bytes of random noise
    uint8_t noise[32];
    for(int n=0; n<32; n++) noise[n] = (uint8_t)random(255);

    // High-speed burst loop
    for(int i = 0; i < 20; i++) {
        // RADIO 1: Block Handshaking
        ble_ch[0] = ble_adv_channels[adv_ptr];
        bleRadio1.setChannel(ble_ch[0]);
        // Use 'true' for no-acknowledgment (multicast)
        bleRadio1.startWrite(noise, 32, true); 
        adv_ptr = (adv_ptr + 1) % 3;

        // RADIO 2: Create "Chaos" on data channels
        ble_ch[1] = sweep_ch;
        bleRadio2.setChannel(ble_ch[1]);
        bleRadio2.startWrite(noise, 32, true);
        
        sweep_ch += 4; // Jump in 4MHz steps to cover 80MHz faster
        if (sweep_ch > 80) sweep_ch = 2;

        // Micro-delay to allow the NRF24 PLL to settle and burst
        delayMicroseconds(80); 
    }
    yield(); // Prevent ESP32 Watchdog Trigger
}