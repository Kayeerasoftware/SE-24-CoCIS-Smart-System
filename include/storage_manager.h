#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize MicroSD Card SPI interface and FATFS Virtual File System.
 * Handled by Sseku Jovan (Persistent Storage & Crash Recovery Lead).
 */
esp_err_t storage_manager_init(void);

/**
 * @brief FreeRTOS Task to consume log queues and persist to SD card.
 */
void task_storage_writer(void *pvParameters);

/**
 * @brief Perform dynamic crash recovery on startup.
 */
esp_err_t perform_crash_recovery(void);

/**
 * @brief Append an access audit entry to /sdcard/access_log.csv.
 */
esp_err_t log_access_to_sd(const char *formatted_entry);

/**
 * @brief Update the persistent occupancy snapshot in /sdcard/occupancy.csv.
 */
esp_err_t update_sd_occupancy_snapshot(uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_MANAGER_H */
