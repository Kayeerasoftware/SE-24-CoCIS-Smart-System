/*
 * ====================================================================
 * CoCIS Smart System - Anti-Passback & Intrusion Lockout Subsystem
 * Component: anti_passback.c
 * Owner: Kasozi David Paul & Sseku Jovan - Group SE-24
 * ====================================================================
 */

#include "anti_passback.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sync_objects.h"
#include "config_defaults.h"
#include "door_actuator.h"

static const char *TAG = "ANTI_PASSBACK";

static occupant_record_t s_occupants[MAX_BUILDING_OCCUPANTS];
static int s_occupant_count = 0;

static int      s_failed_attempts = 0;
static bool     s_is_locked_out   = false;
static TickType_t s_lockout_start_tick = 0;

void anti_passback_init(void) {
    if (xSemaphoreTake(xOccupancyMutex, portMAX_DELAY) == pdTRUE) {
        memset(s_occupants, 0, sizeof(s_occupants));
        s_occupant_count = 0;
        s_failed_attempts = 0;
        s_is_locked_out = false;
        xSemaphoreGive(xOccupancyMutex);
    }
    ESP_LOGI(TAG, "Anti-Passback and Intrusion Lockout engine initialized.");
}

bool anti_passback_is_inside(const char *tag_id) {
    if (!tag_id) return false;
    bool inside = false;
    if (xSemaphoreTake(xOccupancyMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_BUILDING_OCCUPANTS; i++) {
            if (s_occupants[i].is_present && strcmp(s_occupants[i].tag_id, tag_id) == 0) {
                inside = true;
                break;
            }
        }
        xSemaphoreGive(xOccupancyMutex);
    }
    return inside;
}

bool anti_passback_check(uint8_t direction, const char *tag_id, char *failure_reason, size_t max_reason_len) {
    if (!tag_id) return false;

    bool inside = anti_passback_is_inside(tag_id);

    if (direction == 0) { // ENTRY
        if (inside) {
            ESP_LOGW(TAG, "ANTI-PASSBACK VIOLATION: Tag %s is already registered INSIDE.", tag_id);
            if (failure_reason && max_reason_len > 0) {
                snprintf(failure_reason, max_reason_len, "Anti-Passback: Already Inside");
            }
            return false;
        }
    } else {              // EXIT
        if (!inside) {
            ESP_LOGW(TAG, "ANTI-PASSBACK VIOLATION: Tag %s has NO record of entering.", tag_id);
            if (failure_reason && max_reason_len > 0) {
                snprintf(failure_reason, max_reason_len, "Anti-Passback: Not Inside");
            }
            return false;
        }
    }

    return true;
}

bool anti_passback_record_entry(const char *tag_id, const char *user_name, const char *timestamp) {
    if (!tag_id) return false;
    bool success = false;

    if (xSemaphoreTake(xOccupancyMutex, portMAX_DELAY) == pdTRUE) {
        // Find empty slot
        for (int i = 0; i < MAX_BUILDING_OCCUPANTS; i++) {
            if (!s_occupants[i].is_present) {
                strncpy(s_occupants[i].tag_id, tag_id, sizeof(s_occupants[i].tag_id) - 1);
                strncpy(s_occupants[i].user_name, user_name ? user_name : "Unknown", sizeof(s_occupants[i].user_name) - 1);
                strncpy(s_occupants[i].entry_timestamp, timestamp ? timestamp : "Unknown", sizeof(s_occupants[i].entry_timestamp) - 1);
                s_occupants[i].is_present = true;
                s_occupant_count++;
                success = true;
                ESP_LOGI(TAG, "Occupancy ADD: %s (%s). Total occupants inside: %d",
                         s_occupants[i].user_name, s_occupants[i].tag_id, s_occupant_count);
                break;
            }
        }
        xSemaphoreGive(xOccupancyMutex);
    }
    return success;
}

bool anti_passback_record_exit(const char *tag_id) {
    if (!tag_id) return false;
    bool success = false;

    if (xSemaphoreTake(xOccupancyMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < MAX_BUILDING_OCCUPANTS; i++) {
            if (s_occupants[i].is_present && strcmp(s_occupants[i].tag_id, tag_id) == 0) {
                s_occupants[i].is_present = false;
                s_occupant_count--;
                if (s_occupant_count < 0) s_occupant_count = 0;
                success = true;
                ESP_LOGI(TAG, "Occupancy REMOVE: %s. Total occupants inside: %d", tag_id, s_occupant_count);
                break;
            }
        }
        xSemaphoreGive(xOccupancyMutex);
    }
    return success;
}

int anti_passback_get_count(void) {
    int count = 0;
    if (xSemaphoreTake(xOccupancyMutex, portMAX_DELAY) == pdTRUE) {
        count = s_occupant_count;
        xSemaphoreGive(xOccupancyMutex);
    }
    return count;
}

bool anti_passback_get_occupant(int index, char *tag_id, char *user_name, char *timestamp) {
    bool found = false;
    if (xSemaphoreTake(xOccupancyMutex, portMAX_DELAY) == pdTRUE) {
        int active_idx = 0;
        for (int i = 0; i < MAX_BUILDING_OCCUPANTS; i++) {
            if (s_occupants[i].is_present) {
                if (active_idx == index) {
                    if (tag_id) strncpy(tag_id, s_occupants[i].tag_id, 16);
                    if (user_name) strncpy(user_name, s_occupants[i].user_name, 32);
                    if (timestamp) strncpy(timestamp, s_occupants[i].entry_timestamp, 32);
                    found = true;
                    break;
                }
                active_idx++;
            }
        }
        xSemaphoreGive(xOccupancyMutex);
    }
    return found;
}

void anti_passback_clear_all(void) {
    if (xSemaphoreTake(xOccupancyMutex, portMAX_DELAY) == pdTRUE) {
        memset(s_occupants, 0, sizeof(s_occupants));
        s_occupant_count = 0;
        xSemaphoreGive(xOccupancyMutex);
    }
}

void anti_passback_record_failure(void) {
    s_failed_attempts++;
    ESP_LOGW(TAG, "Failed attempt logged (%d of %d).", s_failed_attempts, MAX_FAILED_ATTEMPTS);
    if (s_failed_attempts >= MAX_FAILED_ATTEMPTS) {
        s_is_locked_out = true;
        s_lockout_start_tick = xTaskGetTickCount();
        ESP_LOGE(TAG, "SECURITY ALARM: 3 failed attempts exceeded! Entering %d-second LOCKOUT.",
                 LOCKOUT_DURATION_SEC);
        door_trigger_lockout_alarm(true);
    }
}

void anti_passback_reset_failures(void) {
    s_failed_attempts = 0;
}

bool anti_passback_is_locked_out(void) {
    if (!s_is_locked_out) return false;

    TickType_t elapsed = xTaskGetTickCount() - s_lockout_start_tick;
    if (elapsed >= pdMS_TO_TICKS(LOCKOUT_DURATION_SEC * 1000)) {
        s_is_locked_out = false;
        s_failed_attempts = 0;
        door_trigger_lockout_alarm(false);
        ESP_LOGI(TAG, "Intrusion Lockout expired. Terminal restored to normal operation.");
        return false;
    }
    return true;
}

uint32_t anti_passback_get_lockout_remaining(void) {
    if (!s_is_locked_out) return 0;
    TickType_t elapsed = xTaskGetTickCount() - s_lockout_start_tick;
    uint32_t total_ticks = pdMS_TO_TICKS(LOCKOUT_DURATION_SEC * 1000);
    if (elapsed >= total_ticks) return 0;
    return (uint32_t)((total_ticks - elapsed) * portTICK_PERIOD_MS / 1000);
}

