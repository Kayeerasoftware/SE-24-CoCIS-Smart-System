/*
 * ====================================================================
 * CoCIS Smart Control and Monitoring System - ROOM_NODE_1
 * Node: ROOM_NODE_1 (Lecture Room 1 Controller - ESP32 #2)
 * Owner: Zziwa Mark Alex (24/U/11978/EVE) - Group SE-24
 * Integrator: Kayeera Nathan (24/U/25913/EVE)
 * ====================================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "protocol.h"
#include "config_defaults.h"
#include "mac_address_map.h"
#include "motion_sensor.h"
#include "light_sensor.h"
#include "light_relay.h"
#include "esp_now_sender.h"

static const char *TAG = "ROOM_NODE_1";

void app_main(void) {
    ESP_LOGI(TAG, "=================================================================");
    ESP_LOGI(TAG, "Starting ROOM_NODE_1 (Lecture Room 1 Controller)...");
    ESP_LOGI(TAG, "=================================================================");

    // 1. Initialize NVS (Required for Wi-Fi / ESP-NOW)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize PIR Motion Sensor (GPIO 14)
    motion_sensor_init();

    // 3. Initialize Ambient LDR Light Sensor (ADC1_CH6 / GPIO 34)
    light_sensor_init();

    // 4. Initialize Lighting Relay (GPIO 12) with Daylight-Aware Energy Saving
    light_relay_init();

    // 5. Initialize Wireless ESP-NOW Sender with Room 1 MAC (24:0A:C4:00:02:00)
    esp_now_sender_init(1, MAC_ROOM_NODE_1);

    // 6. Spawn Periodic Sensor Loop and Wireless Telemetry Task
    xTaskCreate(esp_now_telemetry_task, "TelemetryTask", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "ROOM_NODE_1 fully operational. Streaming wireless telemetry to Master.");
}
