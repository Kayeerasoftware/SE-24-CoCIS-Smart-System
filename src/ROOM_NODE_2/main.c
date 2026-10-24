/*
 * ====================================================================
 * CoCIS Smart Control and Monitoring System - ROOM_NODE_2
 * Node: ROOM_NODE_2 (Lecture Room 2 Controller - ESP32 #3)
 * Owner: Zziwa Mark Alex (24/U/11978/EVE) - Group SE-24
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
#include "config_defaults.h"
#include "mac_address_map.h"

static const char *TAG = "ROOM_NODE_2";

static uint8_t g_occupied = 0;
static uint8_t g_lights_on = 0;
static uint16_t g_lux = 180;
static uint32_t g_energy_saved_sec = 0;

void init_room_hardware(void);
void init_esp_now_sender(void);
void telemetry_task(void *pvParameters);

void app_main(void) {
    ESP_LOGI(TAG, "Starting ROOM_NODE_2 (Lecture Room 2)...");

    nvs_flash_init();
    init_room_hardware();
    init_esp_now_sender();

    xTaskCreate(telemetry_task, "TelemetryTask", 3072, NULL, 3, NULL);
    ESP_LOGI(TAG, "ROOM_NODE_2 operational.");
}

void init_room_hardware(void) {
    gpio_set_direction(PIN_PIR_MOTION, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_LIGHT_RELAY, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_STATUS_LED, GPIO_MODE_OUTPUT);

    gpio_set_level(PIN_LIGHT_RELAY, 0);
    gpio_set_level(PIN_STATUS_LED, 0);
}

static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(esp_now_packet_t)) {
        esp_now_packet_t pkt;
        memcpy(&pkt, data, sizeof(esp_now_packet_t));
        if (pkt.msg_type == MSG_TYPE_EMERGENCY_ALERT) {
            ESP_LOGW(TAG, "EMERGENCY BROADCAST RECEIVED! Forcing lights ON.");
            gpio_set_level(PIN_LIGHT_RELAY, 1);
            g_lights_on = 1;
        }
    }
}

void init_esp_now_sender(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_recv_cb(on_data_recv);

    esp_now_peer_info_t master_peer = {0};
    memcpy(master_peer.peer_addr, MAC_ENTERANCE_MASTER, 6);
    master_peer.channel = 0;
    master_peer.encrypt = false;
    esp_now_add_peer(&master_peer);
}

void telemetry_task(void *pvParameters) {
    esp_now_packet_t pkt;
    pkt.magic = ESP_NOW_PAYLOAD_MAGIC;
    pkt.sender_id = NODE_ROOM_2;
    pkt.target_id = NODE_ENTERANCE_MASTER;
    pkt.msg_type = MSG_TYPE_ROOM_STATUS;
    pkt.payload_len = sizeof(room_status_payload_t);

    uint8_t seq = 0;
    while (1) {
        room_status_payload_t status = {
            .occupied = g_occupied,
            .lights_on = g_lights_on,
            .light_level_lux = g_lux,
            .seconds_active = g_energy_saved_sec
        };
        memcpy(pkt.payload, &status, sizeof(room_status_payload_t));
        pkt.msg_id = seq++;
        pkt.checksum = calculate_checksum(&pkt);

        esp_now_send(MAC_ENTERANCE_MASTER, (uint8_t *)&pkt, sizeof(esp_now_packet_t));
        gpio_set_level(PIN_STATUS_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(PIN_STATUS_LED, 0);

        vTaskDelay(pdMS_TO_TICKS(1900));
    }
}
