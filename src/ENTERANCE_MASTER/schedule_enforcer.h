#ifndef SCHEDULE_ENFORCER_H
#define SCHEDULE_ENFORCER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Schedule check result
typedef enum {
    SCHEDULE_PERMITTED = 0,
    SCHEDULE_DENIED_ENTRY_OUTSIDE_HOURS,
    SCHEDULE_DENIED_EXIT_CURFEW_LOCKDOWN
} schedule_check_result_t;

// Initialization
void schedule_enforcer_init(void);

// Evaluate whether access is permitted at the current simulated time
// direction: 0 = ENTRY, 1 = EXIT
// is_admin: if true, schedule check is bypassed
schedule_check_result_t schedule_check_access(uint8_t direction, bool is_admin);

// Schedule getters & setters (Protected by xScheduleMutex)
void schedule_set_hours(uint8_t start_h, uint8_t start_m, uint8_t end_h, uint8_t end_m);
void schedule_get_hours(uint8_t *start_h, uint8_t *start_m, uint8_t *end_h, uint8_t *end_m);

// Simulated clock progression & management
void schedule_set_clock(uint8_t hour, uint8_t minute);
void schedule_get_clock(uint8_t *hour, uint8_t *minute, uint8_t *second);
void schedule_get_timestamp_str(char *buffer, size_t max_len);

#endif // SCHEDULE_ENFORCER_H
