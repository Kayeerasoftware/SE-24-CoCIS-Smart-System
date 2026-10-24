/*
 * ====================================================================
 * CoCIS Smart Control and Monitoring System - ENTERANCE_MASTER
 * Node: ENTERANCE_MASTER (ESP32 #1)
 * Lead Architect: Kayeera Nathan (24/U/25913/EVE) - Group SE-24
 * ====================================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/gpio.h"

#include "protocol.h"
#include "sync_objects.h"
#include "config_defaults.h"
#include "mac_address_map.h"

static const char *TAG = "ENTERANCE_MASTER";

// Function prototypes for modular subsystems
void init_entrance_hardware(void);
void init_wireless_gateway(void);
void access_control_task(void *pvParameters);
void sd_card_logger_task(void *pvParameters);
void wireless_rx_task(void *pvParameters);
void serial_cli_task(void *pvParameters);
void heartbeat_monitor_task(void *pvParameters);

void app_main(void) {
    ESP_LOGI(TAG, "Starting ENTERANCE_MASTER (Group SE-24)...");

    // 1. Initialize NVS (Required for Wi-Fi/ESP-NOW)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize FreeRTOS Mutexes and Queues
    init_concurrency_objects();
    ESP_LOGI(TAG, "Concurrency & Synchronization objects initialized.");

    // 3. Initialize Physical Hardware & GPIOs
    init_entrance_hardware();

    // 4. Initialize ESP-NOW Wireless Gateway
    init_wireless_gateway();

    // 5. Spawn FreeRTOS Multithreaded Tasks
    xTaskCreatePinnedToCore(access_control_task,    "AccessTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(wireless_rx_task,       "WifiRxTask", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(serial_cli_task,        "CLITask",    4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(sd_card_logger_task,    "SDLogTask",  4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(heartbeat_monitor_task, "HBMonTask",  3072, NULL, 2, NULL, 1);

    ESP_LOGI(TAG, "System initialization complete. All tasks operational.");
}

void init_entrance_hardware(void) {
    gpio_set_direction(PIN_LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED_RED,   GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BUZZER,    GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_EMERGENCY_BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_EMERGENCY_BTN, GPIO_PULLUP_ONLY);

    gpio_set_level(PIN_LED_GREEN, 0);
    gpio_set_level(PIN_LED_RED, 0);
    gpio_set_level(PIN_BUZZER, 0);
    ESP_LOGI(TAG, "Entrance GPIOs (Servos, LEDs, Buzzer) configured.");
}

static void on_esp_now_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(esp_now_packet_t)) {
        esp_now_packet_t pkt;
        memcpy(&pkt, data, sizeof(esp_now_packet_t));
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(xWirelessPacketQueue, &pkt, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    }
}

void init_wireless_gateway(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_esp_now_recv));

    // Register Broadcast Peer
    esp_now_peer_info_t bcast = {0};
    memcpy(bcast.peer_addr, MAC_BROADCAST, 6);
    bcast.channel = 0;
    bcast.encrypt = false;
    esp_now_add_peer(&bcast);

    ESP_LOGI(TAG, "Wireless ESP-NOW Gateway active. Listening for room nodes...");
}

void access_control_task(void *pvParameters) {
    access_event_t event;
    while (1) {
        if (xQueueReceive(xAccessEventQueue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Evaluating scan: Tag=%s Dir=%s", event.tag_id, event.direction == 0 ? "IN" : "OUT");
            // Mutex-protected schedule check and anti-passback evaluation will execute here
            xQueueSend(xSDLogQueue, &event, pdMS_TO_TICKS(100));
        }
    }
}

void wireless_rx_task(void *pvParameters) {
    esp_now_packet_t pkt;
    while (1) {
        if (xQueueReceive(xWirelessPacketQueue, &pkt, portMAX_DELAY) == pdTRUE) {
            if (validate_packet(&pkt)) {
                if (pkt.msg_type == MSG_TYPE_ROOM_STATUS) {
                    room_status_payload_t status;
                    memcpy(&status, pkt.payload, sizeof(room_status_payload_t));
                    xSemaphoreTake(xRoomStateMutex, portMAX_DELAY);
                    // Update cached room telemetry
                    xSemaphoreGive(xRoomStateMutex);
                }
            }
        }
    }
}

void serial_cli_task(void *pvParameters) {
    while (1) {
        // CLI command loop
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void sd_card_logger_task(void *pvParameters) {
    access_event_t event;
    while (1) {
        if (xQueueReceive(xSDLogQueue, &event, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(xSDCardMutex, portMAX_DELAY);
            // Append event to /sdcard/access_log.csv
            xSemaphoreGive(xSDCardMutex);
        }
    }
}

void heartbeat_monitor_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        // Check timestamps of ROOM_NODE_1 and ROOM_NODE_2
    }
}
