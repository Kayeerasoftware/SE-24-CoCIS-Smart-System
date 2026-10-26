/*
 * ====================================================================
 * CoCIS Smart System - MicroSD Card Storage & Data Management
 * Component: sd_storage.c
 * Owner: Sseku Jovan (24/U/11172/EVE) - Group SE-24
 * ====================================================================
 */

#include "sd_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "config_defaults.h"
#include "anti_passback.h"
#include "schedule_enforcer.h"

static const char *TAG = "SD_STORAGE";

static bool s_is_mounted = false;
static sdmmc_card_t *s_card = NULL;

// In-Memory Fallback Registry (Keeps system operational even if SD hardware is not attached)
static user_record_t s_mem_users[] = {
    {"TAG_1001", "Kayeera Nathan",      "admin",   true},
    {"TAG_1002", "Kasozi David",        "staff",   true},
    {"TAG_1003", "Zziwa Mark",          "student", true},
    {"TAG_1004", "Sseku Jovan",         "student", true},
    {"TAG_1005", "Namuzibwa Laurinda",  "staff",   true}
};
static const int s_num_default_users = sizeof(s_mem_users) / sizeof(s_mem_users[0]);

static void ensure_default_files(void) {
    struct stat st;

    // 1. Ensure users.csv
    if (stat("/sdcard/users.csv", &st) != 0) {
        ESP_LOGI(TAG, "Creating default /sdcard/users.csv database...");
        FILE *f = fopen("/sdcard/users.csv", "w");
        if (f) {
            fprintf(f, "tag_id,name,role,is_active\n");
            for (int i = 0; i < s_num_default_users; i++) {
                fprintf(f, "%s,%s,%s,%d\n", s_mem_users[i].tag_id, s_mem_users[i].name,
                        s_mem_users[i].role, s_mem_users[i].is_active ? 1 : 0);
            }
            fclose(f);
        }
    }

    // 2. Ensure access_log.csv
    if (stat("/sdcard/access_log.csv", &st) != 0) {
        ESP_LOGI(TAG, "Creating /sdcard/access_log.csv header...");
        FILE *f = fopen("/sdcard/access_log.csv", "w");
        if (f) {
            fprintf(f, "timestamp,tag_id,user_name,direction,decision,reason\n");
            fclose(f);
        }
    }

    // 3. Ensure config.txt
    if (stat("/sdcard/config.txt", &st) != 0) {
        ESP_LOGI(TAG, "Creating default /sdcard/config.txt...");
        FILE *f = fopen("/sdcard/config.txt", "w");
        if (f) {
            fprintf(f, "schedule_start=07:00\n");
            fprintf(f, "schedule_end=20:00\n");
            fprintf(f, "lockout_attempts=3\n");
            fprintf(f, "lockout_duration_sec=60\n");
            fprintf(f, "no_motion_timeout_sec=30\n");
            fprintf(f, "heartbeat_interval_sec=2\n");
            fclose(f);
        }
    }
}

bool sd_storage_init(void) {
    ESP_LOGI(TAG, "Initializing SPI MicroSD Card driver (CS=GPIO %d)...", PIN_SD_CS);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // Initialize SPI bus if not already initialized
    esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus initialize returned: %s. Using internal memory storage.", esp_err_to_name(ret));
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_SD_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret == ESP_OK) {
        s_is_mounted = true;
        ESP_LOGI(TAG, "MicroSD Card successfully mounted at %s (Capacity: %llu MB)",
                 SD_MOUNT_POINT, ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));
        ensure_default_files();
        sd_load_configuration();
    } else {
        s_is_mounted = false;
        ESP_LOGW(TAG, "MicroSD mount failed (%s). Virtual fallback registry active.", esp_err_to_name(ret));
    }

    return s_is_mounted;
}

bool sd_storage_is_mounted(void) {
    return s_is_mounted;
}

bool sd_lookup_user(const char *tag_id, char *name_out, char *role_out, bool *is_active_out) {
    if (!tag_id) return false;
    bool found = false;

    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        if (s_is_mounted) {
            FILE *f = fopen("/sdcard/users.csv", "r");
            if (f) {
                char line[128];
                // Skip header
                if (fgets(line, sizeof(line), f)) {
                    while (fgets(line, sizeof(line), f)) {
                        char file_tag[16], file_name[32], file_role[16];
                        int file_active = 0;
                        if (sscanf(line, "%15[^,],%31[^,],%15[^,],%d",
                                   file_tag, file_name, file_role, &file_active) == 4) {
                            if (strcmp(file_tag, tag_id) == 0) {
                                if (name_out) strcpy(name_out, file_name);
                                if (role_out) strcpy(role_out, file_role);
                                if (is_active_out) *is_active_out = (file_active != 0);
                                found = true;
                                break;
                            }
                        }
                    }
                }
                fclose(f);
            }
        }

        // Memory fallback if not found on SD
        if (!found) {
            for (int i = 0; i < s_num_default_users; i++) {
                if (strcmp(s_mem_users[i].tag_id, tag_id) == 0) {
                    if (name_out) strcpy(name_out, s_mem_users[i].name);
                    if (role_out) strcpy(role_out, s_mem_users[i].role);
                    if (is_active_out) *is_active_out = s_mem_users[i].is_active;
                    found = true;
                    break;
                }
            }
        }
        xSemaphoreGive(xSDCardMutex);
    }

    return found;
}

bool sd_add_user(const char *tag_id, const char *name, const char *role) {
    if (!tag_id || !name || !role) return false;
    bool success = false;

    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        if (s_is_mounted) {
            FILE *f = fopen("/sdcard/users.csv", "a");
            if (f) {
                fprintf(f, "%s,%s,%s,1\n", tag_id, name, role);
                fclose(f);
                success = true;
                ESP_LOGI(TAG, "Appended new user %s (%s, %s) to /sdcard/users.csv", name, tag_id, role);
            }
        }
        xSemaphoreGive(xSDCardMutex);
    }
    return success;
}

bool sd_deactivate_user(const char *tag_id) {
    if (!tag_id) return false;
    bool modified = false;

    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        if (s_is_mounted) {
            FILE *f = fopen("/sdcard/users.csv", "r");
            if (f) {
                FILE *temp = fopen("/sdcard/users.tmp", "w");
                if (temp) {
                    char line[128];
                    if (fgets(line, sizeof(line), f)) {
                        fputs(line, temp);
                        while (fgets(line, sizeof(line), f)) {
                            char file_tag[16], file_name[32], file_role[16];
                            int file_active = 0;
                            if (sscanf(line, "%15[^,],%31[^,],%15[^,],%d",
                                       file_tag, file_name, file_role, &file_active) == 4) {
                                if (strcmp(file_tag, tag_id) == 0) {
                                    fprintf(temp, "%s,%s,%s,0\n", file_tag, file_name, file_role);
                                    modified = true;
                                } else {
                                    fprintf(temp, "%s,%s,%s,%d\n", file_tag, file_name, file_role, file_active);
                                }
                            }
                        }
                    }
                    fclose(temp);
                }
                fclose(f);
                if (modified) {
                    unlink("/sdcard/users.csv");
                    rename("/sdcard/users.tmp", "/sdcard/users.csv");
                    ESP_LOGI(TAG, "Deactivated tag %s in /sdcard/users.csv", tag_id);
                } else {
                    unlink("/sdcard/users.tmp");
                }
            }
        }
        xSemaphoreGive(xSDCardMutex);
    }
    return modified;
}

bool sd_log_access_event(const access_event_t *event) {
    if (!event) return false;
    bool success = false;

    char time_str[32];
    schedule_get_timestamp_str(time_str, sizeof(time_str));

    const char *dir_str  = (event->direction == 0) ? "ENTRY" : "EXIT";
    const char *dec_str  = event->granted ? "GRANTED" : "DENIED";

    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        if (s_is_mounted) {
            FILE *f = fopen("/sdcard/access_log.csv", "a");
            if (f) {
                fprintf(f, "%s,%s,%s,%s,%s,%s\n",
                        time_str, event->tag_id, event->user_name,
                        dir_str, dec_str, event->reason);
                fclose(f);
                success = true;
            }
        }
        ESP_LOGI(TAG, "AUDIT LOG: [%s] %s | Tag:%s | Name:%s | Decision:%s | Reason:%s",
                 time_str, dir_str, event->tag_id, event->user_name, dec_str, event->reason);
        xSemaphoreGive(xSDCardMutex);
    }

    // Automatically update occupancy snapshot on SD for crash recovery
    if (event->granted) {
        sd_save_occupancy_snapshot();
    }

    return success;
}

void sd_dump_logs(int count) {
    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        if (s_is_mounted) {
            FILE *f = fopen("/sdcard/access_log.csv", "r");
            if (f) {
                char line[160];
                int lines_read = 0;
                printf("\n--- RECENT ACCESS AUDIT LOGS (/sdcard/access_log.csv) ---\n");
                while (fgets(line, sizeof(line), f)) {
                    printf("%s", line);
                    lines_read++;
                    if (count > 0 && lines_read > count) break;
                }
                printf("--- END OF AUDIT LOGS (%d records) ---\n\n", lines_read);
                fclose(f);
            } else {
                printf("[SD] /sdcard/access_log.csv not accessible.\n");
            }
        } else {
            printf("[SD] SD card not mounted. Running in virtual memory mode.\n");
        }
        xSemaphoreGive(xSDCardMutex);
    }
}

bool sd_save_occupancy_snapshot(void) {
    if (!s_is_mounted) return false;
    bool success = false;

    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        FILE *f = fopen("/sdcard/occupancy.csv", "w");
        if (f) {
            fprintf(f, "tag_id,user_name,entry_timestamp\n");
            int count = anti_passback_get_count();
            for (int i = 0; i < count; i++) {
                char tag[16], name[32], ts[32];
                if (anti_passback_get_occupant(i, tag, name, ts)) {
                    fprintf(f, "%s,%s,%s\n", tag, name, ts);
                }
            }
            fclose(f);
            success = true;
        }
        xSemaphoreGive(xSDCardMutex);
    }
    return success;
}

int sd_recover_occupancy_snapshot(void) {
    if (!s_is_mounted) return 0;
    int restored = 0;

    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        FILE *f = fopen("/sdcard/occupancy.csv", "r");
        if (f) {
            char line[128];
            // Skip header
            if (fgets(line, sizeof(line), f)) {
                while (fgets(line, sizeof(line), f)) {
                    char tag[16], name[32], ts[32];
                    if (sscanf(line, "%15[^,],%31[^,],%31[^\r\n]", tag, name, ts) == 3) {
                        anti_passback_record_entry(tag, name, ts);
                        restored++;
                    }
                }
            }
            fclose(f);
            ESP_LOGI(TAG, "CRASH RECOVERY: Restored %d active occupants from /sdcard/occupancy.csv", restored);
        }
        xSemaphoreGive(xSDCardMutex);
    }
    return restored;
}

bool sd_load_configuration(void) {
    if (!s_is_mounted) return false;
    bool success = false;

    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        FILE *f = fopen("/sdcard/config.txt", "r");
        if (f) {
            char line[64];
            int s_h = 7, s_m = 0, e_h = 20, e_m = 0;
            while (fgets(line, sizeof(line), f)) {
                if (sscanf(line, "schedule_start=%d:%d", &s_h, &s_m) == 2) {
                    schedule_set_hours(s_h, s_m, e_h, e_m);
                } else if (sscanf(line, "schedule_end=%d:%d", &e_h, &e_m) == 2) {
                    schedule_set_hours(s_h, s_m, e_h, e_m);
                }
            }
            fclose(f);
            success = true;
            ESP_LOGI(TAG, "Configuration loaded from /sdcard/config.txt.");
        }
        xSemaphoreGive(xSDCardMutex);
    }
    return success;
}

bool sd_save_configuration(uint8_t start_h, uint8_t start_m, uint8_t end_h, uint8_t end_m) {
    if (!s_is_mounted) return false;
    bool success = false;

    if (xSemaphoreTake(xSDCardMutex, portMAX_DELAY) == pdTRUE) {
        FILE *f = fopen("/sdcard/config.txt", "w");
        if (f) {
            fprintf(f, "schedule_start=%02d:%02d\n", start_h, start_m);
            fprintf(f, "schedule_end=%02d:%02d\n", end_h, end_m);
            fprintf(f, "lockout_attempts=%d\n", MAX_FAILED_ATTEMPTS);
            fprintf(f, "lockout_duration_sec=%d\n", LOCKOUT_DURATION_SEC);
            fprintf(f, "no_motion_timeout_sec=%d\n", NO_MOTION_TIMEOUT_SEC);
            fprintf(f, "heartbeat_interval_sec=%d\n", HEARTBEAT_INTERVAL_SEC);
            fclose(f);
            success = true;
            ESP_LOGI(TAG, "Saved updated configuration to /sdcard/config.txt");
        }
        xSemaphoreGive(xSDCardMutex);
    }
    return success;
}

