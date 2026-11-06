#ifndef ROOM_LIGHTING_H
#define ROOM_LIGHTING_H

#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//include for Smart Energy Management Lead
//Initialize PIR
//LDR (ADC1)
//Lighting Relays for Rooms 1 and 2
//Smart Energy Management Lead).

esp_err_t room_lighting_init(void);

//FreeRTOS Task to monitor sensors
//manage 30s timers
//control relays

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
