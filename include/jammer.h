#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// Function declarations
void init_jammer_hardware(SPIClass* h, SPIClass* v);
void start_2_4_jammer();
void stop_2_4_jammer();
void handle_2_4_jamming();

// External variables for UI/Display
extern uint8_t ch[2];
extern bool jamming_active;