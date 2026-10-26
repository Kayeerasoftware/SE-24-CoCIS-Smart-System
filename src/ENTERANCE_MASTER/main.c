/*
 * ====================================================================
 * CoCIS Smart Control and Monitoring System - ENTERANCE_MASTER
 * Node: ENTERANCE_MASTER (ESP32 #1)
 * Lead Architect & Integrator: Kayeera Nathan (24/U/25913/EVE)
 * Team Contributors:
 *   - Kasozi David Paul   (24/U/25905/EVE) - RFID, Servos, Anti-Passback
 *   - Sseku Jovan         (24/U/11172/EVE) - MicroSD Storage & Crash Recovery
 *   - Namuzibwa Laurinda  (24/U/1472)      - Serial CLI & Auth Security
 * ====================================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "protocol.h"
#include "sync_objects.h"
#include "config_defaults.h"
#include "mac_address_map.h"
#include "door_actuator.h"
#include "schedule_enforcer.h"
#include "anti_passback.h"
#include "sd_storage.h"
#include "auth_security.h"
#include "rfid_manager.h"
#include "esp_now_gateway.h"
#include "serial_cli.h"

static const char *TAG = "ENTERANCE_MASTER";

// Forward declarations of tasks
void access_control_task(void *pvParameters);
void sd_card_logger_task(void *pvParameters);

void app_main(void) {
    ESP_LOGI(TAG, "=================================================================");
    ESP_LOGI(TAG, "Starting ENTERANCE_MASTER - CoCIS Smart System (Group SE-24)...");
    ESP_LOGI(TAG, "=================================================================");

    // 1. Initialize NVS (Required for Wi-Fi, ESP-NOW and RF calibration)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize FreeRTOS Concurrency Primitives (Mutexes and Queues)
    init_concurrency_objects();
    ESP_LOGI(TAG, "Concurrency & Synchronization objects initialized.");

    // 3. Initialize Physical Actuators, Servos, Buzzer and LEDs
    door_actuator_init();

    // 4. Initialize Schedule Enforcer & Simulated RTC Clock
    schedule_enforcer_init();

    // 5. Initialize Anti-Passback & Intrusion Lockout Engine
    anti_passback_init();

    // 6. Initialize MicroSD Card Storage & Crash Recovery
    sd_storage_init();
    int recovered_occupants = sd_recover_occupancy_snapshot();
    if (recovered_occupants > 0) {
        ESP_LOGI(TAG, "Restored %d active occupants from crash snapshot.", recovered_occupants);
    }

    // 7. Initialize Authentication & Security Subsystem
    auth_security_init();

    // 8. Initialize Dual RC522 RFID Hardware & Simulation Driver
    rfid_manager_init();

    // 9. Initialize Wireless ESP-NOW Gateway
    esp_now_gateway_init();

    // 10. Initialize Serial CLI Console
    serial_cli_init();

    // 11. Spawn FreeRTOS Multithreaded Tasks pinned to respective cores
    xTaskCreatePinnedToCore(access_control_task,    "AccessTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(wireless_rx_task,       "WifiRxTask", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(serial_cli_task,        "CLITask",    4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(sd_card_logger_task,    "SDLogTask",  4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(heartbeat_monitor_task, "HBMonTask",  3072, NULL, 2, NULL, 1);

    ESP_LOGI(TAG, "System operational: Dual RFID, MicroSD, Wireless ESP-NOW and CLI active.");
}

void access_control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Access Control decision engine running on Core 1.");
    access_event_t event;

    while (1) {
        if (xQueueReceive(xAccessEventQueue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Processing scan: Tag='%s', Direction=%s",
                     event.tag_id, (event.direction == 0) ? "ENTRY" : "EXIT");

            // 1. Intrusion Lockout Check
            if (anti_passback_is_locked_out()) {
                uint32_t rem = anti_passback_get_lockout_remaining();
                ESP_LOGW(TAG, "SCAN REJECTED: Terminal is in Intrusion Lockout (%lu seconds remaining).",
                         (unsigned long)rem);
                event.granted = false;
                strncpy(event.user_name, "Unknown", sizeof(event.user_name) - 1);
                strncpy(event.reason, "Terminal Locked Out", sizeof(event.reason) - 1);
                door_deny_access();
                xQueueSend(xSDLogQueue, &event, pdMS_TO_TICKS(100));
                continue;
            }

            // 2. Validate Tag in Database (users.csv / in-memory cache)
            char user_name[32] = {0};
            char user_role[16] = {0};
            bool is_active = false;
            bool tag_found = sd_lookup_user(event.tag_id, user_name, user_role, &is_active);

            if (!tag_found || !is_active) {
                anti_passback_record_failure();
                event.granted = false;
                strncpy(event.user_name, tag_found ? user_name : "Unregistered", sizeof(event.user_name) - 1);
                strncpy(event.reason, tag_found ? "Inactive Tag" : "Invalid Tag ID", sizeof(event.reason) - 1);
                door_deny_access();
                xQueueSend(xSDLogQueue, &event, pdMS_TO_TICKS(100));
                continue;
            }

            // Valid recognized tag -> reset failed attempts counter
            anti_passback_reset_failures();
            strncpy(event.user_name, user_name, sizeof(event.user_name) - 1);

            // 3. Operational Schedule Curfew Check (Coursework Section 2.6)
            bool is_admin = (strcmp(user_role, "admin") == 0);
            schedule_check_result_t sched_res = schedule_check_access(event.direction, is_admin);

            if (sched_res == SCHEDULE_DENIED_ENTRY_OUTSIDE_HOURS) {
                event.granted = false;
                strncpy(event.reason, "Outside Schedule Hours", sizeof(event.reason) - 1);
                door_deny_access();
                xQueueSend(xSDLogQueue, &event, pdMS_TO_TICKS(100));
                continue;
            } else if (sched_res == SCHEDULE_DENIED_EXIT_CURFEW_LOCKDOWN) {
                event.granted = false;
                strncpy(event.reason, "Curfew Lockdown (Exit Denied)", sizeof(event.reason) - 1);
                door_deny_access();
                xQueueSend(xSDLogQueue, &event, pdMS_TO_TICKS(100));
                continue;
            }

            // 4. Anti-Passback Validation (Coursework Section 2.7)
            char apb_reason[32] = {0};
            if (!anti_passback_check(event.direction, event.tag_id, apb_reason, sizeof(apb_reason))) {
                event.granted = false;
                strncpy(event.reason, apb_reason, sizeof(event.reason) - 1);
                door_deny_access();
                xQueueSend(xSDLogQueue, &event, pdMS_TO_TICKS(100));
                continue;
            }

            // 5. GRANT ACCESS
            event.granted = true;
            strncpy(event.reason, "Authorized", sizeof(event.reason) - 1);

            char current_time_str[32];
            schedule_get_timestamp_str(current_time_str, sizeof(current_time_str));

            if (event.direction == 0) { // ENTRY
                anti_passback_record_entry(event.tag_id, user_name, current_time_str);
                door_grant_access(DOOR_ENTRY);
            } else {                    // EXIT
                anti_passback_record_exit(event.tag_id);
                door_grant_access(DOOR_EXIT);
            }

            // Queue for SD logging and snapshot updating
            xQueueSend(xSDLogQueue, &event, pdMS_TO_TICKS(100));
        }
    }
}

void sd_card_logger_task(void *pvParameters) {
    ESP_LOGI(TAG, "SD Card audit logger running on Core 0.");
    access_event_t event;

    while (1) {
        if (xQueueReceive(xSDLogQueue, &event, portMAX_DELAY) == pdTRUE) {
            sd_log_access_event(&event);
        }
    }
}
