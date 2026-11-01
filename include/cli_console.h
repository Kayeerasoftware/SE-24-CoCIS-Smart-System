#ifndef CLI_CONSOLE_H
#define CLI_CONSOLE_H

#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Serial UART0 Interface and Command Parser.
 * Handled by Namuzibwa Laurinda (Interface, Security & QA Lead).
 */
esp_err_t cli_console_init(void);

/**
 * @brief FreeRTOS Task to handle serial terminal inputs and command execution.
 */
void task_cli_console(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* CLI_CONSOLE_H */
