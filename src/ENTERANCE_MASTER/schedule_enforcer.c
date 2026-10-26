/*
 * ====================================================================
 * CoCIS Smart System - Schedule Enforcer & Curfew Subsystem
 * Component: schedule_enforcer.c
 * Lead: Kayeera Nathan & Kasozi David Paul - Group SE-24
 * ====================================================================
 */

#include "schedule_enforcer.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "sync_objects.h"
#include "config_defaults.h"

static const char *TAG = "SCHEDULE_ENFORCER";

// Configured Operating Hours
static uint8_t s_start_hour = DEFAULT_SCHEDULE_START_HOUR; // 07
static uint8_t s_start_min  = DEFAULT_SCHEDULE_START_MIN;  // 00
static uint8_t s_end_hour   = DEFAULT_SCHEDULE_END_HOUR;   // 20
static uint8_t s_end_min    = DEFAULT_SCHEDULE_END_MIN;    // 00

// Simulated RTC Clock (Starts at 08:30:00 during normal school hours)
static uint8_t s_clock_hour = 8;
static uint8_t s_clock_min  = 30;
static uint8_t s_clock_sec  = 0;

static TimerHandle_t s_clock_timer = NULL;

static void on_clock_tick(TimerHandle_t xTimer) {
    if (xSemaphoreTake(xScheduleMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_clock_sec++;
        if (s_clock_sec >= 60) {
            s_clock_sec = 0;
            s_clock_min++;
            if (s_clock_min >= 60) {
                s_clock_min = 0;
                s_clock_hour = (s_clock_hour + 1) % 24;
            }
        }
        xSemaphoreGive(xScheduleMutex);
    }
}

void schedule_enforcer_init(void) {
    s_clock_timer = xTimerCreate("SimClockTimer", pdMS_TO_TICKS(1000), pdTRUE, (void *)0, on_clock_tick);
    if (s_clock_timer) {
        xTimerStart(s_clock_timer, 0);
    }
    ESP_LOGI(TAG, "Schedule Enforcer active. Hours: %02d:%02d - %02d:%02d | Current Clock: %02d:%02d:%02d",
             s_start_hour, s_start_min, s_end_hour, s_end_min,
             s_clock_hour, s_clock_min, s_clock_sec);
}

schedule_check_result_t schedule_check_access(uint8_t direction, bool is_admin) {
    // Admin bypasses schedule checks
    if (is_admin) {
        ESP_LOGI(TAG, "Admin privilege detected: Bypassing building schedule.");
        return SCHEDULE_PERMITTED;
    }

    schedule_check_result_t result = SCHEDULE_PERMITTED;

    if (xSemaphoreTake(xScheduleMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint32_t now_mins   = (s_clock_hour * 60) + s_clock_min;
        uint32_t start_mins = (s_start_hour * 60) + s_start_min;
        uint32_t end_mins   = (s_end_hour * 60) + s_end_min;

        bool inside_schedule = false;
        if (start_mins <= end_mins) {
            inside_schedule = (now_mins >= start_mins && now_mins < end_mins);
        } else {
            // Overnight schedule support
            inside_schedule = (now_mins >= start_mins || now_mins < end_mins);
        }

        if (!inside_schedule) {
            if (direction == 0) { // ENTRY
                ESP_LOGW(TAG, "CURFEW REJECTION: Entry attempted outside authorized hours (%02d:%02d).",
                         s_clock_hour, s_clock_min);
                result = SCHEDULE_DENIED_ENTRY_OUTSIDE_HOURS;
            } else {              // EXIT
                ESP_LOGW(TAG, "CURFEW LOCKDOWN: Exit attempted outside authorized hours (%02d:%02d). Asset protection enforced.",
                         s_clock_hour, s_clock_min);
                result = SCHEDULE_DENIED_EXIT_CURFEW_LOCKDOWN;
            }
        }
        xSemaphoreGive(xScheduleMutex);
    }

    return result;
}

void schedule_set_hours(uint8_t start_h, uint8_t start_m, uint8_t end_h, uint8_t end_m) {
    if (xSemaphoreTake(xScheduleMutex, portMAX_DELAY) == pdTRUE) {
        s_start_hour = start_h;
        s_start_min  = start_m;
        s_end_hour   = end_h;
        s_end_min    = end_m;
        ESP_LOGI(TAG, "Schedule updated to %02d:%02d - %02d:%02d",
                 s_start_hour, s_start_min, s_end_hour, s_end_min);
        xSemaphoreGive(xScheduleMutex);
    }
}

void schedule_get_hours(uint8_t *start_h, uint8_t *start_m, uint8_t *end_h, uint8_t *end_m) {
    if (xSemaphoreTake(xScheduleMutex, portMAX_DELAY) == pdTRUE) {
        if (start_h) *start_h = s_start_hour;
        if (start_m) *start_m = s_start_min;
        if (end_h)   *end_h   = s_end_hour;
        if (end_m)   *end_m   = s_end_min;
        xSemaphoreGive(xScheduleMutex);
    }
}

void schedule_set_clock(uint8_t hour, uint8_t minute) {
    if (xSemaphoreTake(xScheduleMutex, portMAX_DELAY) == pdTRUE) {
        s_clock_hour = hour % 24;
        s_clock_min  = minute % 60;
        s_clock_sec  = 0;
        ESP_LOGI(TAG, "Simulated Clock set to %02d:%02d:00", s_clock_hour, s_clock_min);
        xSemaphoreGive(xScheduleMutex);
    }
}

void schedule_get_clock(uint8_t *hour, uint8_t *minute, uint8_t *second) {
    if (xSemaphoreTake(xScheduleMutex, portMAX_DELAY) == pdTRUE) {
        if (hour)   *hour   = s_clock_hour;
        if (minute) *minute = s_clock_min;
        if (second) *second = s_clock_sec;
        xSemaphoreGive(xScheduleMutex);
    }
}

void schedule_get_timestamp_str(char *buffer, size_t max_len) {
    if (!buffer || max_len == 0) return;
    if (xSemaphoreTake(xScheduleMutex, portMAX_DELAY) == pdTRUE) {
        snprintf(buffer, max_len, "2026-10-26 %02d:%02d:%02d",
                 s_clock_hour, s_clock_min, s_clock_sec);
        xSemaphoreGive(xScheduleMutex);
    }
}

