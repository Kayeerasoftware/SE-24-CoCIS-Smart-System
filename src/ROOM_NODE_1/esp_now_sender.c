/*
 * ====================================================================
 * CoCIS Smart System - Room Node Wireless ESP-NOW Telemetry Sender
 * Component: esp_now_sender.c
 * Owner: Zziwa Mark Alex & Kayeera Nathan - Group SE-24
 * ====================================================================
 */

#include "esp_now_sender.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "protocol.h"
#include "config_defaults.h"
#include "mac_address_map.h"
#include "motion_sensor.h"
#include "light_sensor.h"
#include "light_relay.h"

static const char *TAG = "ESP_NOW_SENDER";

static uint8_t s_room_id = 1;
static uint8_t s_packet_seq = 0;

static void on_packet_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(esp_now_packet_t)) {
        esp_now_packet_t pkt;
        memcpy(&pkt, data, sizeof(esp_now_packet_t));

        if (!validate_packet(&pkt)) {
            ESP_LOGW(TAG, "Corrupted packet received from Master (Checksum failed).");
            return;
        }

        if (pkt.msg_type == MSG_TYPE_EMERGENCY_ALERT && pkt.payload_len >= sizeof(emergency_alert_payload_t)) {
            emergency_alert_payload_t alert;
            memcpy(&alert, pkt.payload, sizeof(emergency_alert_payload_t));
            ESP_LOGW(TAG, "EMERGENCY BROADCAST RECEIVED! Active=%d", alert.emergency_active);
            light_relay_set_emergency(alert.emergency_active != 0);
        } else if (pkt.msg_type == MSG_TYPE_LIGHT_OVERRIDE && pkt.payload_len >= sizeof(light_override_payload_t)) {
            light_override_payload_t override_cmd;
            memcpy(&override_cmd, pkt.payload, sizeof(light_override_payload_t));
            if (override_cmd.target_room == 0 || override_cmd.target_room == s_room_id) {
                ESP_LOGI(TAG, "LIGHT OVERRIDE RECEIVED: Mode %d", override_cmd.override_mode);
                light_relay_set_override(override_cmd.override_mode);
            }
        }
    }
}

void esp_now_sender_init(uint8_t room_id, const uint8_t *my_mac) {
    s_room_id = room_id;
    ESP_LOGI(TAG, "Initializing Wireless ESP-NOW Sender for Room %d...", room_id);

    // Status LED on GPIO 2 (Blinks on packet send)
    gpio_set_direction(PIN_STATUS_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_STATUS_LED, 0);

    // Wi-Fi Stack Initialization in Station Mode
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);

    if (my_mac) {
        esp_wifi_set_mac(WIFI_IF_STA, my_mac);
    }
    esp_wifi_start();

    // ESP-NOW Stack & Callbacks
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_packet_recv));

    // Register Master Gateway Peer
    esp_now_peer_info_t master_peer = {0};
    memcpy(master_peer.peer_addr, MAC_ENTERANCE_MASTER, 6);
    master_peer.channel = 0;
    master_peer.encrypt = false;
    esp_now_add_peer(&master_peer);

    ESP_LOGI(TAG, "ESP-NOW Sender active. Registered Master Gateway 24:0A:C4:00:01:00.");
}

void esp_now_telemetry_task(void *pvParameters) {
    ESP_LOGI(TAG, "Telemetry transmission task active (Interval: %d seconds).", HEARTBEAT_INTERVAL_SEC);

    esp_now_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.magic = ESP_NOW_PAYLOAD_MAGIC;
    pkt.sender_id = (s_room_id == 1) ? NODE_ROOM_1 : NODE_ROOM_2;
    pkt.target_id = NODE_ENTERANCE_MASTER;
    pkt.msg_type = MSG_TYPE_ROOM_STATUS;
    pkt.payload_len = sizeof(room_status_payload_t);

    while (1) {
        // Run sensor and relay periodic ticks (1-second tick loop)
        motion_sensor_tick_1s();
        light_relay_tick_1s();

        // Every 2 seconds (or on second tick), transmit telemetry
        static int s_tick_count = 0;
        s_tick_count++;

        if (s_tick_count >= HEARTBEAT_INTERVAL_SEC) {
            s_tick_count = 0;

            room_status_payload_t status = {
                .occupied        = motion_sensor_is_occupied() ? 1 : 0,
                .lights_on       = light_relay_is_on() ? 1 : 0,
                .light_level_lux = light_sensor_read_lux(),
                .seconds_active  = light_relay_get_energy_saved_sec()
            };

            pkt.msg_id = s_packet_seq++;
            memcpy(pkt.payload, &status, sizeof(status));
            pkt.checksum = calculate_checksum(&pkt);

            // Transmit wirelessly over 2.4 GHz ESP-NOW
            esp_err_t ret = esp_now_send(MAC_ENTERANCE_MASTER, (uint8_t *)&pkt, sizeof(pkt));
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Telemetry send returned: %s", esp_err_to_name(ret));
            }

            // Blink Status LED on transmission
            gpio_set_level(PIN_STATUS_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(PIN_STATUS_LED, 0);

            vTaskDelay(pdMS_TO_TICKS(900));
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

