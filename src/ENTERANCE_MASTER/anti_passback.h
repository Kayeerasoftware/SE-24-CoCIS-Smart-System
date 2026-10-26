#ifndef ANTI_PASSBACK_H
#define ANTI_PASSBACK_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_BUILDING_OCCUPANTS 32

typedef struct {
    char tag_id[16];
    char user_name[32];
    char entry_timestamp[32];
    bool is_present;
} occupant_record_t;

// Initialization
void anti_passback_init(void);

// Validation
bool anti_passback_is_inside(const char *tag_id);
bool anti_passback_check(uint8_t direction, const char *tag_id, char *failure_reason, size_t max_reason_len);

// Occupancy Updates (Protected by xOccupancyMutex)
bool anti_passback_record_entry(const char *tag_id, const char *user_name, const char *timestamp);
bool anti_passback_record_exit(const char *tag_id);
int  anti_passback_get_count(void);
bool anti_passback_get_occupant(int index, char *tag_id, char *user_name, char *timestamp);
void anti_passback_clear_all(void);

// Intrusion Lockout Engine
void anti_passback_record_failure(void);
void anti_passback_reset_failures(void);
bool anti_passback_is_locked_out(void);
uint32_t anti_passback_get_lockout_remaining(void);

#endif // ANTI_PASSBACK_H

