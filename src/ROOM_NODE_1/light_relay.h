#ifndef LIGHT_RELAY_H
#define LIGHT_RELAY_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

// Initialization of Relay on GPIO 12
void light_relay_init(void);

// Periodic 1-second update evaluating occupancy, daylight, and overrides
void light_relay_tick_1s(void);

// Queries
bool     light_relay_is_on(void);
uint32_t light_relay_get_energy_saved_sec(void);
uint8_t  light_relay_get_override_mode(void);

// Overrides from Master
void light_relay_set_override(uint8_t mode);
void light_relay_set_emergency(bool active);

#endif // LIGHT_RELAY_H

