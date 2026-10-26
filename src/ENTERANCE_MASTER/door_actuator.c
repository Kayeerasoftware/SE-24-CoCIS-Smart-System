/*
 * ====================================================================
 * CoCIS Smart System - Door Actuator & Barrier Subsystem
 * Component: door_actuator.c
 * Owner: Kasozi David Paul (24/U/25905/EVE) - Group SE-24
 * ====================================================================
 */

#include "door_actuator.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "config_defaults.h"

static const char *TAG = "DOOR_ACTUATOR";

#define SERVO_LEDC_TIMER          LEDC_TIMER_0
#define SERVO_LEDC_MODE           LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_DUTY_RES       LEDC_TIMER_14_BIT  // 0 - 16383
#define SERVO_LEDC_FREQ           50                 // 50 Hz (20ms period)

#define SERVO_ENTRY_CHANNEL       LEDC_CHANNEL_0     // GPIO 13
#define SERVO_EXIT_CHANNEL        LEDC_CHANNEL_1     // GPIO 15

// Duty calculation for 14-bit @ 50 Hz (20ms)
// 0.5ms = 410, 1.5ms (90 deg) = 1229, 2.5ms (180 deg) = 2048
#define SERVO_DUTY_CLOSED         410
#define SERVO_DUTY_OPEN_90        1229

static TimerHandle_t s_entry_relock_timer = NULL;
static TimerHandle_t s_exit_relock_timer  = NULL;
static bool s_emergency_active = false;

static void set_servo_duty(ledc_channel_t channel, uint32_t duty) {
    ledc_set_duty(SERVO_LEDC_MODE, channel, duty);
    ledc_update_duty(SERVO_LEDC_MODE, channel);
}

static void on_entry_relock_timer(TimerHandle_t xTimer) {
    if (s_emergency_active) return;
    ESP_LOGI(TAG, "5-second relock timer elapsed: Closing Entry Door.");
    set_servo_duty(SERVO_ENTRY_CHANNEL, SERVO_DUTY_CLOSED);
    gpio_set_level(PIN_LED_GREEN, 0);
}

static void on_exit_relock_timer(TimerHandle_t xTimer) {
    if (s_emergency_active) return;
    ESP_LOGI(TAG, "5-second relock timer elapsed: Closing Exit Door.");
    set_servo_duty(SERVO_EXIT_CHANNEL, SERVO_DUTY_CLOSED);
    gpio_set_level(PIN_LED_GREEN, 0);
}

void door_actuator_init(void) {
    // 1. Configure LEDs and Buzzer GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_LED_GREEN) | (1ULL << PIN_LED_RED) | (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_LED_GREEN, 0);
    gpio_set_level(PIN_LED_RED, 0);
    gpio_set_level(PIN_BUZZER, 0);

    // 2. Configure LEDC Timer for Servo PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_LEDC_MODE,
        .timer_num        = SERVO_LEDC_TIMER,
        .duty_resolution  = SERVO_LEDC_DUTY_RES,
        .freq_hz          = SERVO_LEDC_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 3. Configure Entry Servo Channel (GPIO 13)
    ledc_channel_config_t entry_channel = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = SERVO_ENTRY_CHANNEL,
        .timer_sel      = SERVO_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_SERVO_ENTRY,
        .duty           = SERVO_DUTY_CLOSED,
        .hpoint         = 0
    };
    ledc_channel_config(&entry_channel);

    // 4. Configure Exit Servo Channel (GPIO 15)
    ledc_channel_config_t exit_channel = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = SERVO_EXIT_CHANNEL,
        .timer_sel      = SERVO_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_SERVO_EXIT,
        .duty           = SERVO_DUTY_CLOSED,
        .hpoint         = 0
    };
    ledc_channel_config(&exit_channel);

    // 5. Initialize Relock Timers (5-second auto relock)
    s_entry_relock_timer = xTimerCreate("EntryRelock", pdMS_TO_TICKS(DOOR_UNLOCK_DURATION_SEC * 1000),
                                        pdFALSE, (void *)0, on_entry_relock_timer);
    s_exit_relock_timer  = xTimerCreate("ExitRelock", pdMS_TO_TICKS(DOOR_UNLOCK_DURATION_SEC * 1000),
                                        pdFALSE, (void *)1, on_exit_relock_timer);

    ESP_LOGI(TAG, "Door Actuator subsystem initialized (Servos closed, LEDs ready).");
}

void door_set_servo_angle(door_direction_t dir, uint32_t angle) {
    if (angle > 90) angle = 90;
    uint32_t duty = SERVO_DUTY_CLOSED + (angle * (SERVO_DUTY_OPEN_90 - SERVO_DUTY_CLOSED) / 90);
    if (dir == DOOR_ENTRY) {
        set_servo_duty(SERVO_ENTRY_CHANNEL, duty);
    } else {
        set_servo_duty(SERVO_EXIT_CHANNEL, duty);
    }
}

void door_grant_access(door_direction_t dir) {
    if (s_emergency_active) return;

    ESP_LOGI(TAG, "ACCESS GRANTED: Opening %s Door (90 deg) for %d seconds.",
             dir == DOOR_ENTRY ? "ENTRY" : "EXIT", DOOR_UNLOCK_DURATION_SEC);

    // Visual & Audible feedback
    gpio_set_level(PIN_LED_GREEN, 1);
    gpio_set_level(PIN_LED_RED, 0);
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(100)); // Short chirp
    gpio_set_level(PIN_BUZZER, 0);

    // Open physical barrier
    if (dir == DOOR_ENTRY) {
        set_servo_duty(SERVO_ENTRY_CHANNEL, SERVO_DUTY_OPEN_90);
        xTimerStop(s_entry_relock_timer, 0);
        xTimerStart(s_entry_relock_timer, 0);
    } else {
        set_servo_duty(SERVO_EXIT_CHANNEL, SERVO_DUTY_OPEN_90);
        xTimerStop(s_exit_relock_timer, 0);
        xTimerStart(s_exit_relock_timer, 0);
    }
}

void door_deny_access(void) {
    if (s_emergency_active) return;

    ESP_LOGW(TAG, "ACCESS DENIED: Keeping barrier locked.");

    // Flash Red LED and sound denial beep
    gpio_set_level(PIN_LED_RED, 1);
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(300));
    gpio_set_level(PIN_BUZZER, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(300));
    gpio_set_level(PIN_BUZZER, 0);
    gpio_set_level(PIN_LED_RED, 0);
}

void door_trigger_lockout_alarm(bool active) {
    if (active) {
        gpio_set_level(PIN_LED_RED, 1);
        gpio_set_level(PIN_BUZZER, 1);
    } else {
        gpio_set_level(PIN_LED_RED, 0);
        gpio_set_level(PIN_BUZZER, 0);
    }
}

void door_emergency_override(bool active) {
    s_emergency_active = active;
    if (active) {
        ESP_LOGE(TAG, "EMERGENCY EVACUATION ACTIVE: Both doors locked wide open (90 deg).");
        xTimerStop(s_entry_relock_timer, 0);
        xTimerStop(s_exit_relock_timer, 0);
        set_servo_duty(SERVO_ENTRY_CHANNEL, SERVO_DUTY_OPEN_90);
        set_servo_duty(SERVO_EXIT_CHANNEL,  SERVO_DUTY_OPEN_90);
        gpio_set_level(PIN_LED_GREEN, 1);
        gpio_set_level(PIN_LED_RED, 1);
        gpio_set_level(PIN_BUZZER, 1);
    } else {
        ESP_LOGI(TAG, "Emergency Evacuation cleared. Returning doors to locked position.");
        set_servo_duty(SERVO_ENTRY_CHANNEL, SERVO_DUTY_CLOSED);
        set_servo_duty(SERVO_EXIT_CHANNEL,  SERVO_DUTY_CLOSED);
        gpio_set_level(PIN_LED_GREEN, 0);
        gpio_set_level(PIN_LED_RED, 0);
        gpio_set_level(PIN_BUZZER, 0);
    }
}

