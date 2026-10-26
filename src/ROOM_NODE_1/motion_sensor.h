#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

// Initialization of PIR sensor (GPIO 14)
void motion_sensor_init(void);

// Polling and 30-second countdown update routine (called every 1s)
void motion_sensor_tick_1s(void);

// Occupancy state queries
bool     motion_sensor_is_occupied(void);
uint32_t motion_sensor_get_countdown(void);

#endif // MOTION_SENSOR_H

