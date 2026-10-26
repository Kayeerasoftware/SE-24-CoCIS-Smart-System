#ifndef ESP_NOW_SENDER_H
#define ESP_NOW_SENDER_H

#include <stdint.h>
#include <stdbool.h>

// Initialization of ESP-NOW transmitter with node MAC and Master peer
void esp_now_sender_init(uint8_t room_id, const uint8_t *my_mac);

// FreeRTOS task transmitting telemetry every 2 seconds
void esp_now_telemetry_task(void *pvParameters);

#endif // ESP_NOW_SENDER_H

