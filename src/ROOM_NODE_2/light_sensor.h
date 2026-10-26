#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

void     light_sensor_init(void);
uint16_t light_sensor_read_lux(void);
bool     light_sensor_is_daylight_sufficient(void);

#endif // LIGHT_SENSOR_H

