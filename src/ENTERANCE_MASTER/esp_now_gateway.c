/*
 * ====================================================================
 * CoCIS Smart System - Wireless ESP-NOW Gateway & Heartbeat Engine
 * Component: esp_now_gateway.c
 * Owner: Kayeera Nathan (24/U/25913/EVE) - System Architect, Group SE-24
 * ====================================================================
 */

#include "esp_now_gateway.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "driver/gpio.h"
#include "config_defaults.h"
#include "mac_address_map.h"
#include "door_actuator.h"

static const char *TAG = "ESP_NOW_GATEWAY";

static room_telemetry_t s_rooms[2]; // Index 0 = Room 1, Index 1 = Room 2
static uint8_t s_packet_seq = 0;
static bool s_emergency_active = false;

static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "Wireless TX failed to peer %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    }
}

static void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(esp_now_packet_t) && xWirelessPacketQueue) {
        esp_now_packet_t pkt;
        memcpy(&pkt, data, sizeof(esp_now_packet_t));
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(xWirelessPacketQueue, &pkt, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

void esp_now_gateway_init(void) {
    ESP_LOGI(TAG, "Initializing Wireless 2.4 GHz ESP-NOW Gateway Stack...");

    // Initialize Room state structures
    memset(s_rooms, 0, sizeof(s_rooms));
    s_rooms[0].room_id = 1;
    s_rooms[0].is_online = false;
    s_rooms[1].room_id = 2;
    s_rooms[1].is_online = false;

    // Wi-Fi Stack in Station Mode
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);

    // Set virtual MAC address for Wokwi peer identification
    esp_wifi_set_mac(WIFI_IF_STA, MAC_ENTERANCE_MASTER);
    esp_wifi_start();

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    // Register Room 1 Peer
    esp_now_peer_info_t peer1 = {0};
    memcpy(peer1.peer_addr, MAC_ROOM_NODE_1, 6);
    peer1.channel = 0;
    peer1.encrypt = false;
    esp_now_add_peer(&peer1);

    // Register Room 2 Peer
    esp_now_peer_info_t peer2 = {0};
    memcpy(peer2.peer_addr, MAC_ROOM_NODE_2, 6);
    peer2.channel = 0;
    peer2.encrypt = false;
    esp_now_add_peer(&peer2);

    // Register Broadcast Peer
    esp_now_peer_info_t bcast = {0};
    memcpy(bcast.peer_addr, MAC_BROADCAST, 6);
    bcast.channel = 0;
    bcast.encrypt = false;
    esp_now_add_peer(&bcast);

    // Emergency physical pushbutton on GPIO 32
    gpio_set_direction(PIN_EMERGENCY_BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_EMERGENCY_BTN, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "Wireless ESP-NOW Gateway active on MAC 24:0A:C4:00:01:00.");
}

bool esp_now_gateway_get_room_telemetry(uint8_t room_id, room_telemetry_t *out_status) {
    if (room_id < 1 || room_id > 2 || !out_status) return false;
    bool ok = false;
    if (xSemaphoreTake(xRoomStateMutex, portMAX_DELAY) == pdTRUE) {
        memcpy(out_status, &s_rooms[room_id - 1], sizeof(room_telemetry_t));
        ok = true;
        xSemaphoreGive(xRoomStateMutex);
    }
    return ok;
}

bool esp_now_gateway_send_light_override(uint8_t room_id, uint8_t override_mode) {
    esp_now_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.magic = ESP_NOW_PAYLOAD_MAGIC;
    pkt.msg_id = s_packet_seq++;
    pkt.sender_id = NODE_ENTERANCE_MASTER;
    pkt.target_id = (room_id == 0) ? NODE_BROADCAST : (room_id == 1 ? NODE_ROOM_1 : NODE_ROOM_2);
    pkt.msg_type = MSG_TYPE_LIGHT_OVERRIDE;

    light_override_payload_t payload = {
        .target_room = room_id,
        .override_mode = override_mode
    };
    pkt.payload_len = sizeof(payload);
    memcpy(pkt.payload, &payload, sizeof(payload));
    pkt.checksum = calculate_checksum(&pkt);

    const uint8_t *dest_mac = MAC_BROADCAST;
    if (room_id == 1) dest_mac = MAC_ROOM_NODE_1;
    else if (room_id == 2) dest_mac = MAC_ROOM_NODE_2;

    esp_err_t err = esp_now_send(dest_mac, (uint8_t *)&pkt, sizeof(pkt));
    ESP_LOGI(TAG, "Sent LIGHT_OVERRIDE (Room %d -> Mode %d, Result: %s)",
             room_id, override_mode, esp_err_to_name(err));

    // Update local cached override state
    if (xSemaphoreTake(xRoomStateMutex, portMAX_DELAY) == pdTRUE) {
        if (room_id == 0 || room_id == 1) s_rooms[0].override_mode = override_mode;
        if (room_id == 0 || room_id == 2) s_rooms[1].override_mode = override_mode;
        xSemaphoreGive(xRoomStateMutex);
    }

    return (err == ESP_OK);
}

bool esp_now_gateway_broadcast_emergency(bool active) {
    s_emergency_active = active;

    esp_now_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.magic = ESP_NOW_PAYLOAD_MAGIC;
    pkt.msg_id = s_packet_seq++;
    pkt.sender_id = NODE_ENTERANCE_MASTER;
    pkt.target_id = NODE_BROADCAST;
    pkt.msg_type = MSG_TYPE_EMERGENCY_ALERT;

    emergency_alert_payload_t payload = {
        .emergency_active = active ? 1 : 0,
        .timestamp = 0
    };
    pkt.payload_len = sizeof(payload);
    memcpy(pkt.payload, &payload, sizeof(payload));
    pkt.checksum = calculate_checksum(&pkt);

    // Retry broadcast up to 3 times for industrial reliability
    esp_err_t err = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        err = esp_now_send(MAC_BROADCAST, (uint8_t *)&pkt, sizeof(pkt));
        if (err == ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGW(TAG, "BROADCAST EMERGENCY EVACUATION: Active=%s (Send Result: %s)",
             active ? "YES" : "NO", esp_err_to_name(err));

    // Trigger local entrance doors wide open
    door_emergency_override(active);

    return (err == ESP_OK);
}

void esp_now_gateway_check_emergency_button(void) {
    // Physical button on GPIO 32 is active low with pull-up
    if (gpio_get_level(PIN_EMERGENCY_BTN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
        if (gpio_get_level(PIN_EMERGENCY_BTN) == 0) {
            ESP_LOGW(TAG, "PHYSICAL EMERGENCY BUTTON PRESSED ON GPIO 32!");
            // Toggle or activate emergency
            esp_now_gateway_broadcast_emergency(!s_emergency_active);
            while (gpio_get_level(PIN_EMERGENCY_BTN) == 0) {
                vTaskDelay(pdMS_TO_TICKS(100)); // Wait for release
            }
        }
    }
}

void wireless_rx_task(void *pvParameters) {
    ESP_LOGI(TAG, "Wireless RX task running on Core 1.");
    esp_now_packet_t pkt;

    while (1) {
        if (xQueueReceive(xWirelessPacketQueue, &pkt, portMAX_DELAY) == pdTRUE) {
            if (!validate_packet(&pkt)) {
                ESP_LOGW(TAG, "Received corrupted wireless packet (Checksum mismatch). Dropped.");
                continue;
            }

            int room_idx = -1;
            if (pkt.sender_id == NODE_ROOM_1) room_idx = 0;
            else if (pkt.sender_id == NODE_ROOM_2) room_idx = 1;

            if (room_idx >= 0) {
                if (xSemaphoreTake(xRoomStateMutex, portMAX_DELAY) == pdTRUE) {
                    s_rooms[room_idx].last_seen_tick = xTaskGetTickCount();
                    if (!s_rooms[room_idx].is_online) {
                        s_rooms[room_idx].is_online = true;
                        ESP_LOGI(TAG, "WIRELESS NODE ONLINE: Room %d is now transmitting.", room_idx + 1);
                    }

                    if (pkt.msg_type == MSG_TYPE_ROOM_STATUS && pkt.payload_len >= sizeof(room_status_payload_t)) {
                        room_status_payload_t status;
                        memcpy(&status, pkt.payload, sizeof(room_status_payload_t));
                        s_rooms[room_idx].occupied         = status.occupied;
                        s_rooms[room_idx].lights_on        = status.lights_on;
                        s_rooms[room_idx].light_level_lux  = status.light_level_lux;
                        s_rooms[room_idx].energy_saved_sec = status.seconds_active;

                        ESP_LOGD(TAG, "Telemetry Room %d: Occ=%d, Lights=%d, Lux=%d, Energy=%lu sec",
                                 room_idx + 1, status.occupied, status.lights_on,
                                 status.light_level_lux, status.seconds_active);
                    }
                    xSemaphoreGive(xRoomStateMutex);
                }
            }
        }
    }
}

void heartbeat_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Heartbeat & Offline Detection task running (Interval: 2s).");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_SEC * 1000));

        // Also poll physical emergency button
        esp_now_gateway_check_emergency_button();

        TickType_t now = xTaskGetTickCount();
        const TickType_t timeout_ticks = pdMS_TO_TICKS(NODE_OFFLINE_TIMEOUT_SEC * 1000);

        if (xSemaphoreTake(xRoomStateMutex, portMAX_DELAY) == pdTRUE) {
            for (int i = 0; i < 2; i++) {
                if (s_rooms[i].is_online) {
                    if ((now - s_rooms[i].last_seen_tick) > timeout_ticks) {
                        s_rooms[i].is_online = false;
                        ESP_LOGE(TAG, "WIRELESS NODE OFFLINE: Room %d missed heartbeats for > %d seconds!",
                                 i + 1, NODE_OFFLINE_TIMEOUT_SEC);
                    }
                }
            }
            xSemaphoreGive(xRoomStateMutex);
        }
    }
}

