#ifndef ACCESS_ENTRANCE_H
#define ACCESS_ENTRANCE_H

#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Entrance & Exit Hardware (Servos, LEDs, Buzzers, RFID SPI).
 */
esp_err_t access_entrance_init(void);

/**
 * @brief FreeRTOS Task to handle Inbound and Outbound Access Control.
 */
void task_entrance_controller(void *pvParameters);

/**
 * @brief Process an RFID Card Tap (Hardware or CLI simulated).
 */
void process_card_tap(const char *tag_id, access_direction_t direction);

/**
 * @brief Actuate gate servo for access granted:
 *        Opens from 180° to 90° (upwards) with Green LED ON for 5 seconds, then closes to 180°.
 */
void actuate_gate_servo(access_direction_t direction);

/**
 * @brief Signal access denial:
 *        Red LED ON for 3 seconds with real audio 2.4kHz alarm buzzer sounding.
 */
void signal_access_denied(access_direction_t direction);

/**
 * @brief Set gate servo directly to an angle (180° = closed barrier, 90° = upwards open).
 */
void gate_servo_set_angle(ledc_channel_t channel, int angle_deg);

/**
 * @brief Enable or disable real audio tone (2.4kHz) on a buzzer pin.
 */
void buzzer_sound_enable(gpio_num_t buzzer_pin, bool enable);

/**
 * @brief Run visual and auditory startup hardware self-test.
 */
void run_startup_hardware_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* ACCESS_ENTRANCE_H */
