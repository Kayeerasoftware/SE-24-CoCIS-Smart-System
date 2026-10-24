#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define ESP_NOW_PAYLOAD_MAGIC 0xAA55
#define MAX_PAYLOAD_LEN 32

typedef enum {
    NODE_ENTERANCE_MASTER = 0x01,
    NODE_ROOM_1          = 0x02,
    NODE_ROOM_2          = 0x03
} node_id_t;

typedef enum {
    MSG_TYPE_HEARTBEAT       = 0x01,
    MSG_TYPE_ROOM_STATUS     = 0x02,
    MSG_TYPE_LIGHT_OVERRIDE  = 0x03,
    MSG_TYPE_EMERGENCY_ALERT = 0x04
} msg_type_t;

// ESP-NOW Wireless Packet Structure
typedef struct __attribute__((packed)) {
    uint16_t magic;           // 0xAA55
    uint8_t  msg_id;          // Sequence counter
    uint8_t  sender_id;       // Node ID of sender
    uint8_t  target_id;       // Target Node ID (or 0xFF for broadcast)
    uint8_t  msg_type;        // Type of message
    uint8_t  payload_len;     // Length of payload
    uint8_t  payload[MAX_PAYLOAD_LEN];
    uint8_t  checksum;        // XOR checksum
} esp_now_packet_t;

// Telemetry payload sent by ROOM_NODE_1 and ROOM_NODE_2
typedef struct __attribute__((packed)) {
    uint8_t  occupied;        // 1 = Occupied, 0 = Empty
    uint8_t  lights_on;       // 1 = ON, 0 = OFF
    uint16_t light_level_lux; // Ambient LDR reading
    uint32_t seconds_active;  // Energy tracking counter
} room_status_payload_t;

uint8_t calculate_checksum(const esp_now_packet_t *pkt);
bool validate_packet(const esp_now_packet_t *pkt);

#endif // PROTOCOL_H
