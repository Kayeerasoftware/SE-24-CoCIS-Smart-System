#ifndef ESP_NOW_GATEWAY_H
#define ESP_NOW_GATEWAY_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"
#include "sync_objects.h"

// Initialization of Wireless Gateway & Peer Registration
void esp_now_gateway_init(void);

// Telemetry Retrieval (Protected by xRoomStateMutex)
bool esp_now_gateway_get_room_telemetry(uint8_t room_id, room_telemetry_t *out_status);

// Wireless Command Dispatch
bool esp_now_gateway_send_light_override(uint8_t room_id, uint8_t override_mode);
bool esp_now_gateway_broadcast_emergency(bool active);

// Emergency Button Polling / Routine
void esp_now_gateway_check_emergency_button(void);

// Background Tasks for FreeRTOS
void wireless_rx_task(void *pvParameters);
void heartbeat_monitor_task(void *pvParameters);

#endif // ESP_NOW_GATEWAY_H

