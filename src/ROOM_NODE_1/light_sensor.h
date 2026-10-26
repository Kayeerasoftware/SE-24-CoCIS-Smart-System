#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

// Initialization of LDR sensor on ADC1 Channel 6 (GPIO 34)
void light_sensor_init(void);

// Read current ambient light level in lux (0 - 1000 lux)
uint16_t light_sensor_read_lux(void);

// Check if daylight is sufficient to keep lights off
bool light_sensor_is_daylight_sufficient(void);

#endif // LIGHT_SENSOR_H

