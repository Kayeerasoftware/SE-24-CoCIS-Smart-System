#ifndef ROOM_LIGHTING_H
#define ROOM_LIGHTING_H

#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize PIR, LDR (ADC1), and Lighting Relays for Rooms 1 & 2.
 * Handled by Zziwa Mark Alex (Smart Energy Management Lead).
 */
esp_err_t room_lighting_init(void);

/**
 * @brief FreeRTOS Task to monitor sensors, manage 30s timers, and control relays.
 */
void task_room1_controller(void *pvParameters);
void task_room2_controller(void *pvParameters);

/**
 * @brief Get current telemetry data for a specific room.
 */
room_telemetry_t get_room_telemetry(uint8_t room_id);

#ifdef __cplusplus
}
#endif

#endif /* ROOM_LIGHTING_H */
