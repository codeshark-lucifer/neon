#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

extern byte spec_ch[128];

void init_spec_hardware(SPIClass* h, SPIClass* v);
void start_spectrum();
void stop_spectrum();
void handle_spectrum();