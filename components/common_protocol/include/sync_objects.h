#ifndef SYNC_OBJECTS_H
#define SYNC_OBJECTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "protocol.h"

// Access Event Structure (Pushed from RFID/Simulation to Access Control & SD Logger)
typedef struct {
    char     tag_id[16];
    char     user_name[32];
    uint8_t  direction;        // 0 = ENTRY, 1 = EXIT
    bool     granted;          // true = GRANTED, false = DENIED
    char     reason[32];       // "Authorized", "Outside Schedule", "Anti-Passback", etc.
    uint32_t timestamp;        // Epoch / simulated clock timestamp
} access_event_t;

// Cached Room Telemetry Structure (Maintained on Master for Room 1 & 2)
typedef struct {
    uint8_t  room_id;              // 1 = Room 1, 2 = Room 2
    bool     is_online;            // true if heartbeat received within 10 seconds
    uint8_t  occupied;             // 1 = Occupied, 0 = Vacant
    uint8_t  lights_on;            // 1 = ON, 0 = OFF
    uint16_t light_level_lux;      // Ambient brightness in lux
    uint32_t energy_saved_sec;     // Seconds lights kept OFF while occupied / energy saved
    uint32_t last_seen_tick;       // Tick count when last packet arrived
    uint8_t  override_mode;        // 0 = Auto, 1 = Force OFF, 2 = Force ON
} room_telemetry_t;

// Thread-Safe Mutex Handles
extern SemaphoreHandle_t xOccupancyMutex;
extern SemaphoreHandle_t xScheduleMutex;
extern SemaphoreHandle_t xSDCardMutex;
extern SemaphoreHandle_t xRoomStateMutex;

// Inter-Task Communication Queue Handles
extern QueueHandle_t xAccessEventQueue;
extern QueueHandle_t xSDLogQueue;
extern QueueHandle_t xWirelessPacketQueue;

// Initialization routine
void init_concurrency_objects(void);

#endif // SYNC_OBJECTS_H
