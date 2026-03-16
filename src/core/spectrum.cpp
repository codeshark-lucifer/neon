#include <spectrum.h>
#include <config.h>

RF24 radio1(RF1_CE, RF1_CSN, 16000000);
RF24 radio2(RF2_CE, RF2_CSN, 16000000);

SPIClass *spi_h = nullptr;
SPIClass *spi_v = nullptr;
byte spec_ch[128];

void init_spec_hardware(SPIClass *h, SPIClass *v)
{
    spi_h = h;
    spi_v = v;
}

void start_spectrum()
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
        radios[i]->startListening();
        radios[i]->stopListening();
    }
}

void stop_spectrum()
{
}

void handle_spectrum()
{
    if (!spi_h || !spi_v) return;

    // We sweep 0-63 on Radio 1 and 64-127 on Radio 2
    for (int i = 0; i < 64; i++) {
        // Radio 1 Sweep
        radio1.setChannel(i);
        radio1.startListening();
        delayMicroseconds(120); // Wait for radio to settle
        spec_ch[i] = radio1.testRPD() ? (spec_ch[i] + 1) : (spec_ch[i] > 0 ? spec_ch[i] - 1 : 0);
        radio1.stopListening();

        // Radio 2 Sweep
        radio2.setChannel(i + 64);
        radio2.startListening();
        delayMicroseconds(120);
        spec_ch[i + 64] = radio2.testRPD() ? (spec_ch[i + 64] + 1) : (spec_ch[i + 64] > 0 ? spec_ch[i + 64] - 1 : 0);
        radio2.stopListening();
    }
}