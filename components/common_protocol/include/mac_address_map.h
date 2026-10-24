#ifndef MAC_ADDRESS_MAP_H
#define MAC_ADDRESS_MAP_H

#include <stdint.h>

// Standard virtual MAC addresses for Wokwi ESP-NOW peer pairing
static const uint8_t MAC_ENTERANCE_MASTER[6] = {0x24, 0x0A, 0xC4, 0x00, 0x01, 0x00};
static const uint8_t MAC_ROOM_NODE_1[6]      = {0x24, 0x0A, 0xC4, 0x00, 0x02, 0x00};
static const uint8_t MAC_ROOM_NODE_2[6]      = {0x24, 0x0A, 0xC4, 0x00, 0x03, 0x00};
static const uint8_t MAC_BROADCAST[6]        = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#endif // MAC_ADDRESS_MAP_H
