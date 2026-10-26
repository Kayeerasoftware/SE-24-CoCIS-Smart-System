#ifndef SD_STORAGE_H
#define SD_STORAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "sync_objects.h"

#define SD_MOUNT_POINT "/sdcard"

// User record structure
typedef struct {
    char tag_id[16];
    char name[32];
    char role[16];     // "admin", "staff", "student"
    bool is_active;
} user_record_t;

// Initialization
bool sd_storage_init(void);
bool sd_storage_is_mounted(void);

// User Database Operations (/sdcard/users.csv)
bool sd_lookup_user(const char *tag_id, char *name_out, char *role_out, bool *is_active_out);
bool sd_add_user(const char *tag_id, const char *name, const char *role);
bool sd_deactivate_user(const char *tag_id);

// Audit Trail Logging (/sdcard/access_log.csv)
bool sd_log_access_event(const access_event_t *event);
void sd_dump_logs(int count);

// Crash Recovery & Persistence (/sdcard/occupancy.csv)
bool sd_save_occupancy_snapshot(void);
int  sd_recover_occupancy_snapshot(void);

// Configuration Storage (/sdcard/config.txt)
bool sd_load_configuration(void);
bool sd_save_configuration(uint8_t start_h, uint8_t start_m, uint8_t end_h, uint8_t end_m);

#endif // SD_STORAGE_H

