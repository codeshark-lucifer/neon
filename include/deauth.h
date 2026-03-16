#pragma once
#include <Arduino.h>
#include <esp_wifi.h> // Required for wifi_promiscuous_filter_t
#include <types.h>

void start_deauth(int wifi_number, int attack_type, uint16_t reason);
void stop_deauth();
void handle_deauth(AP& ap);

extern int eliminated_stations;
extern int deauth_type;