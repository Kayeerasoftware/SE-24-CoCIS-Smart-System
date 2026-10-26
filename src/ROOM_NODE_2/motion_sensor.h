#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

void     motion_sensor_init(void);
void     motion_sensor_tick_1s(void);
bool     motion_sensor_is_occupied(void);
uint32_t motion_sensor_get_countdown(void);

#endif // MOTION_SENSOR_H

