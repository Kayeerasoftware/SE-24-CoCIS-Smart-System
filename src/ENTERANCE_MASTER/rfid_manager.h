#ifndef RFID_MANAGER_H
#define RFID_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "door_actuator.h"

// Reader Identification
typedef enum {
    RFID_READER_ENTRY = 0,
    RFID_READER_EXIT  = 1
} rfid_reader_t;

// Initialization of Dual RC522 SPI Readers
void rfid_manager_init(void);

// Polling routine for real MFRC522 hardware
bool rfid_poll_reader(rfid_reader_t reader, char *tag_id_out, size_t max_len);

// Swipe Simulation API (For CLI, Wokwi virtual testing & automated test matrices)
void rfid_simulate_swipe(door_direction_t dir, const char *tag_id);

#endif // RFID_MANAGER_H
