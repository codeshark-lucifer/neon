#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// Configuration and Control
void init_ble_hardware(SPIClass* h, SPIClass* v);
void start_ble_jam();
void stop_ble_jam();
void handle_ble_jamming();

// Shared status for the UI
extern bool ble_jamming_active;
extern uint8_t ble_ch[2];