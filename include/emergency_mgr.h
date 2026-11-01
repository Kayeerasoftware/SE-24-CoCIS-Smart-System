#ifndef EMERGENCY_MGR_H
#define EMERGENCY_MGR_H

#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize emergency evacuation hardware interrupt on GPIO 32.
 * Handled by Kayeera Nathan (Group Leader & Architect).
 */
esp_err_t emergency_mgr_init(void);

/**
 * @brief FreeRTOS Task to handle high-priority evacuation sequences and health heartbeat.
 */
void task_emergency_handler(void *pvParameters);
void task_system_heartbeat(void *pvParameters);

/**
 * @brief Trigger or clear system-wide emergency state.
 */
void trigger_emergency_evacuation(bool active, const char *source);

#ifdef __cplusplus
}
#endif

#endif /* EMERGENCY_MGR_H */
