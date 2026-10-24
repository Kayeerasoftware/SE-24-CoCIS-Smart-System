#ifndef SYNC_OBJECTS_H
#define SYNC_OBJECTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "protocol.h"

// Access Event Structure
typedef struct {
    char tag_id[16];
    char user_name[32];
    uint8_t direction;        // 0 = ENTRY, 1 = EXIT
    bool granted;
    char reason[32];
    uint32_t timestamp;
} access_event_t;

// Mutex Handles
extern SemaphoreHandle_t xOccupancyMutex;
extern SemaphoreHandle_t xScheduleMutex;
extern SemaphoreHandle_t xSDCardMutex;
extern SemaphoreHandle_t xRoomStateMutex;

// Queue Handles
extern QueueHandle_t xAccessEventQueue;
extern QueueHandle_t xSDLogQueue;
extern QueueHandle_t xWirelessPacketQueue;

void init_concurrency_objects(void);

#endif // SYNC_OBJECTS_H
