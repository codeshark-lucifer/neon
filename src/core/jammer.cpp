#include "jammer.h"
#include "config.h" // Ensure RF1_CE, RF1_CSN, etc. are defined here

// Instantiate the radio objects with pins from config.h
RF24 radio1(RF1_CE, RF1_CSN, 16000000);
RF24 radio2(RF2_CE, RF2_CSN, 16000000);

SPIClass *spi_h = nullptr;
SPIClass *spi_v = nullptr;

uint8_t ch[2] = {45, 45};
uint8_t flag[2] = {0, 0};
bool jamming_active = false;
extern bool needsRedraw;
uint8_t target_low = 1;
uint8_t target_high = 22;

void init_jammer_hardware(SPIClass *h, SPIClass *v)
{
    spi_h = h;
    spi_v = v;
}

void start_2_4_jammer()
{
    if (!spi_h || !spi_v)
        return;

    // Initialize radios if they haven't been already
    if (!radio1.begin(spi_h))
        Serial.println("R1 Fail");
    if (!radio2.begin(spi_v))
        Serial.println("R2 Fail");

    RF24 *radios[] = {&radio1, &radio2};
    for (int i = 0; i < 2; i++)
    {
        radios[i]->setAutoAck(false);
        radios[i]->stopListening();
        radios[i]->setRetries(0, 0);
        radios[i]->setDataRate(RF24_2MBPS);
        radios[i]->setPALevel(RF24_PA_MAX, true); // true = Use LNA if present
        radios[i]->stopListening();
        radios[i]->setCRCLength(RF24_CRC_DISABLED);
        // Start carrier on initial channels
        radios[i]->startConstCarrier(RF24_PA_MAX, 45);
    }

    jamming_active = true;
    randomSeed(micros());
    Serial.println("Jammer: CW Mode Active");
}

void stop_2_4_jammer()
{
    jamming_active = false;

    RF24 *radios[] = {&radio1, &radio2};
    for (int i = 0; i < 2; i++)
    {
        radios[i]->stopConstCarrier();
        radios[i]->stopListening();
        radios[i]->powerDown();
    }
    Serial.println("Jammer: Stopped");
}

void handle_2_4_jamming()
{
    if (!jamming_active)
        return;

    // Dummy "noise" payload
    uint8_t noise[32];
    for (int n = 0; n < 32; n++)
        noise[n] = random(0, 255);

    // Reduced inner loop to 10 so the UI sees more variety
    for (int i = 0; i < 10; i++)
    {
        ch[0] = random(80);
        radio1.setChannel(ch[0]);
        radio1.startWrite(noise, 32, true);
        
        ch[1] = random(80);
        radio2.setChannel(ch[1]);
        radio2.startWrite(noise, 32, true); // Send noise packet
    }

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 200)
    {
        needsRedraw = true;
        lastUpdate = millis();
    }
}