#include "sync_objects.h"

SemaphoreHandle_t xOccupancyMutex      = NULL;
SemaphoreHandle_t xScheduleMutex       = NULL;
SemaphoreHandle_t xSDCardMutex         = NULL;
SemaphoreHandle_t xRoomStateMutex      = NULL;

QueueHandle_t xAccessEventQueue        = NULL;
QueueHandle_t xSDLogQueue              = NULL;
QueueHandle_t xWirelessPacketQueue     = NULL;

void init_concurrency_objects(void) {
    if (xOccupancyMutex == NULL) xOccupancyMutex = xSemaphoreCreateMutex();
    if (xScheduleMutex == NULL)  xScheduleMutex  = xSemaphoreCreateMutex();
    if (xSDCardMutex == NULL)    xSDCardMutex    = xSemaphoreCreateMutex();
    if (xRoomStateMutex == NULL) xRoomStateMutex = xSemaphoreCreateMutex();

    if (xAccessEventQueue == NULL)    xAccessEventQueue    = xQueueCreate(10, sizeof(access_event_t));
    if (xSDLogQueue == NULL)          xSDLogQueue          = xQueueCreate(20, sizeof(access_event_t));
    if (xWirelessPacketQueue == NULL) xWirelessPacketQueue = xQueueCreate(15, sizeof(esp_now_packet_t));
}
