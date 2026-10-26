#ifndef SERIAL_CLI_H
#define SERIAL_CLI_H

// Initialization of Serial CLI console
void serial_cli_init(void);

// FreeRTOS Task for Serial CLI
void serial_cli_task(void *pvParameters);

#endif // SERIAL_CLI_H

