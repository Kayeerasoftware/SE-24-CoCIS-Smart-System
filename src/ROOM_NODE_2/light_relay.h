#ifndef LIGHT_RELAY_H
#define LIGHT_RELAY_H

#include <stdbool.h>
#include <stdint.h>
#include "protocol.h"

void     light_relay_init(void);
void     light_relay_tick_1s(void);
bool     light_relay_is_on(void);
uint32_t light_relay_get_energy_saved_sec(void);
uint8_t  light_relay_get_override_mode(void);
void     light_relay_set_override(uint8_t mode);
void     light_relay_set_emergency(bool active);

#endif // LIGHT_RELAY_H

