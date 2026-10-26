/*
 * ====================================================================
 * CoCIS Smart System - PIR Motion Sensor & 30-Second Vacancy Timer
 * Component: motion_sensor.c
 * Owner: Zziwa Mark Alex (24/U/11978/EVE) - Group SE-24
 * ====================================================================
 */

#include "motion_sensor.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "config_defaults.h"

static const char *TAG = "MOTION_SENSOR";

static volatile bool     s_is_occupied    = false;
static volatile uint32_t s_countdown_sec  = 0;

void motion_sensor_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_PIR_MOTION),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    s_is_occupied = false;
    s_countdown_sec = 0;

    ESP_LOGI(TAG, "PIR Motion sensor initialized on GPIO %d (Vacancy delay: %d seconds).",
             PIN_PIR_MOTION, NO_MOTION_TIMEOUT_SEC);
}

void motion_sensor_tick_1s(void) {
    int pin_level = gpio_get_level(PIN_PIR_MOTION);

    if (pin_level == 1) {
        // Motion currently detected
        if (!s_is_occupied) {
            ESP_LOGI(TAG, "PIR MOTION DETECTED: Room transitioned from VACANT -> OCCUPIED.");
        }
        s_is_occupied = true;
        s_countdown_sec = NO_MOTION_TIMEOUT_SEC; // Reset 30s timer
    } else {
        // No motion detected on this tick
        if (s_countdown_sec > 0) {
            s_countdown_sec--;
            if (s_countdown_sec == 0 && s_is_occupied) {
                s_is_occupied = false;
                ESP_LOGI(TAG, "30-second vacancy timer elapsed: Room transitioned -> VACANT.");
            }
        }
    }
}

bool motion_sensor_is_occupied(void) {
    return s_is_occupied;
}

uint32_t motion_sensor_get_countdown(void) {
    return s_countdown_sec;
}

