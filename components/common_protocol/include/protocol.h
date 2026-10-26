#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define ESP_NOW_PAYLOAD_MAGIC   0xAA55
#define MAX_PAYLOAD_LEN         32

// Logical Node Identifiers
typedef enum {
    NODE_ENTERANCE_MASTER = 0x01,
    NODE_ROOM_1          = 0x02,
    NODE_ROOM_2          = 0x03,
    NODE_BROADCAST       = 0xFF
} node_id_t;

// Message Type Identifiers
typedef enum {
    MSG_TYPE_HEARTBEAT       = 0x01,  // Periodic keep-alive (every 2s)
    MSG_TYPE_ROOM_STATUS     = 0x02,  // Room telemetry (motion, light, lux, energy)
    MSG_TYPE_LIGHT_OVERRIDE  = 0x03,  // Master command: 0=Auto, 1=Force OFF, 2=Force ON
    MSG_TYPE_EMERGENCY_ALERT = 0x04   // Master broadcast: 1=Evacuate, 0=Normal
} msg_type_t;

// Light Override Command Values
typedef enum {
    LIGHT_OVERRIDE_AUTO      = 0,
    LIGHT_OVERRIDE_FORCE_OFF = 1,
    LIGHT_OVERRIDE_FORCE_ON  = 2
} light_override_mode_t;

// Standardized ESP-NOW Wireless Packet Frame (Fits easily inside ESP-NOW 250-byte max)
typedef struct __attribute__((packed)) {
    uint16_t magic;                    // 0xAA55 Protocol Identifier
    uint8_t  msg_id;                   // Sequence counter for retry & tracking
    uint8_t  sender_id;                // Node ID of sender
    uint8_t  target_id;                // Target Node ID (or 0xFF for broadcast)
    uint8_t  msg_type;                 // Message type
    uint8_t  payload_len;              // Length of valid application payload
    uint8_t  payload[MAX_PAYLOAD_LEN]; // Application payload bytes
    uint8_t  checksum;                 // XOR checksum over header and payload
} esp_now_packet_t;

// Telemetry Payload Structure (Sent by ROOM_NODE_1 and ROOM_NODE_2)
typedef struct __attribute__((packed)) {
    uint8_t  occupied;        // 1 = Occupied, 0 = Vacant
    uint8_t  lights_on;       // 1 = Relay ON, 0 = Relay OFF
    uint16_t light_level_lux; // Calibrated ambient light in lux
    uint32_t seconds_active;  // Cumulative energy saved seconds
} room_status_payload_t;

// Light Override Payload Structure (Sent by Master to Room nodes)
typedef struct __attribute__((packed)) {
    uint8_t target_room;      // 0 = All rooms, 1 = Room 1, 2 = Room 2
    uint8_t override_mode;    // 0 = Auto, 1 = Force OFF, 2 = Force ON
} light_override_payload_t;

// Emergency Alert Payload Structure (Broadcast by Master)
typedef struct __attribute__((packed)) {
    uint8_t emergency_active; // 1 = Active Evacuation, 0 = Normal / Reset
    uint32_t timestamp;       // System timestamp of alert trigger
} emergency_alert_payload_t;

// Checksum & Packet Validation Helpers
uint8_t calculate_checksum(const esp_now_packet_t *pkt);
bool validate_packet(const esp_now_packet_t *pkt);

#endif // PROTOCOL_H
