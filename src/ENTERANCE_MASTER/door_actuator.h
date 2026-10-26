#ifndef DOOR_ACTUATOR_H
#define DOOR_ACTUATOR_H

#include <stdbool.h>
#include <stdint.h>

// Door direction enum
typedef enum {
    DOOR_ENTRY = 0,
    DOOR_EXIT  = 1
} door_direction_t;

// Initialization
void door_actuator_init(void);

// Grant access on entry or exit: unlocks servo to 90 deg, blinks green LED, beeps, relocks in 5s
void door_grant_access(door_direction_t dir);

// Deny access: flashes red LED, sounds denial tone
void door_deny_access(void);

// Sound alarm during intrusion lockout
void door_trigger_lockout_alarm(bool active);

// Emergency Evacuation: swings both servos to 90 deg open and sounds alert
void door_emergency_override(bool active);

// Manually set servo angle (0 to 90 degrees)
void door_set_servo_angle(door_direction_t dir, uint32_t angle);

#endif // DOOR_ACTUATOR_H

