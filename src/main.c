/**
 * @file main.c
 * @brief Master Integration & FreeRTOS Dual-Core Concurrency Scheduler
 * @author Kayeera Nathan (Reg No: 24/U/25913/EVE) - Group Leader & Architect
 * @project CoCIS Smart Control and Monitoring System - Makerere University
 */

#include "project_config.h"
#include "emergency_mgr.h"
#include "access_entrance.h"
#include "room_lighting.h"
#include "storage_manager.h"
#include "cli_console.h"

static const char *TAG = "MAIN_APP";

/* Global FreeRTOS Handles */
QueueHandle_t xQueueAccessEvents = NULL;
QueueHandle_t xQueueTelemetry = NULL;
QueueHandle_t xQueueLogStorage = NULL;
EventGroupHandle_t xEventGroupSystem = NULL;
SemaphoreHandle_t xSemaphoreSpiMutex = NULL;
SemaphoreHandle_t xSemaphoreDbMutex = NULL;

/* Global State Trackers */
access_schedule_t g_system_schedule = {
    .start_hour = 7,
    .start_min  = 0,
    .end_hour   = 20,
    .end_min    = 0,
    .curfew_enforced = false
};
volatile uint32_t g_building_occupancy_count = 0;
volatile bool g_emergency_evacuation_active = false;

void app_main(void) {
    ESP_LOGI(TAG, "=================================================================");
    ESP_LOGI(TAG, "  STARTING CoCIS SMART CONTROL & MONITORING SYSTEM (GROUP SE-24) ");
    ESP_LOGI(TAG, "  Target: Single ESP32 DevKit V1 (Free Wokwi Simulation Tier)    ");
    ESP_LOGI(TAG, "  Academic Year 2026/2027 - Makerere University                  ");
    ESP_LOGI(TAG, "=================================================================");

    // 1. Initialize FreeRTOS Queues and Synchronization Primitives
    xQueueAccessEvents = xQueueCreate(16, sizeof(access_event_t));
    xQueueTelemetry    = xQueueCreate(16, sizeof(room_telemetry_t));
    xQueueLogStorage   = xQueueCreate(32, sizeof(log_entry_t));
    xEventGroupSystem  = xEventGroupCreate();
    xSemaphoreSpiMutex = xSemaphoreCreateMutex();
    xSemaphoreDbMutex  = xSemaphoreCreateMutex();

    if (!xQueueAccessEvents || !xQueueTelemetry || !xQueueLogStorage ||
        !xEventGroupSystem || !xSemaphoreSpiMutex || !xSemaphoreDbMutex) {
        ESP_LOGE(TAG, "FATAL: Failed to allocate FreeRTOS synchronization objects!");
        return;
    }

    // 2. Initialize Subsystem Peripherals in dependency order
    ESP_ERROR_CHECK(access_entrance_init());  // Configures Servos, LEDs, Buzzers, RFID SPI
    ESP_ERROR_CHECK(emergency_mgr_init());   // Attaches Emergency Pushbutton ISR
    ESP_ERROR_CHECK(room_lighting_init());   // Initializes PIRs, LDRs, and Relay Actuators
    ESP_ERROR_CHECK(storage_manager_init()); // Mounts SD FATFS (fallback in-memory)
    ESP_ERROR_CHECK(cli_console_init());     // Starts UART CLI console

    // 3. Pin Real-Time Tasks to Core 0 (I/O, Interrupts, Actuation)
    xTaskCreatePinnedToCore(task_emergency_handler,    "TaskEmergency", 3072, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_entrance_controller,  "TaskEntrance",  4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_room1_controller,     "TaskRoom1",     3072, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(task_room2_controller,     "TaskRoom2",     3072, NULL, 4, NULL, 0);

    // 4. Pin Management & Storage Tasks to Core 1 (UART CLI, SD FATFS, Health)
    xTaskCreatePinnedToCore(task_storage_writer,       "TaskStorage",   4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(task_cli_console,          "TaskCLI",       4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(task_system_heartbeat,     "TaskHeartbeat", 2048, NULL, 1, NULL, 1);

    ESP_LOGI(TAG, "All 7 FreeRTOS Tasks running concurrently across dual cores.");
}
