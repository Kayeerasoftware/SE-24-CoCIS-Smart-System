/*
 * ====================================================================
 * CoCIS Smart System - Daylight-Aware Lighting Relay Subsystem
 * Component: light_relay.c
 * Owner: Zziwa Mark Alex (24/U/11978/EVE) - Group SE-24
 * ====================================================================
 */

#include "light_relay.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "config_defaults.h"
#include "motion_sensor.h"
#include "light_sensor.h"

static const char *TAG = "LIGHT_RELAY";

static bool     s_is_relay_on          = false;
static uint8_t  s_override_mode        = LIGHT_OVERRIDE_AUTO;
static bool     s_emergency_evac       = false;
static uint32_t s_energy_saved_seconds = 0;

void light_relay_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_LIGHT_RELAY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_LIGHT_RELAY, 0);

    s_is_relay_on = false;
    s_override_mode = LIGHT_OVERRIDE_AUTO;
    s_emergency_evac = false;
    s_energy_saved_seconds = 0;

    ESP_LOGI(TAG, "Light Relay subsystem initialized on GPIO %d (Active High).", PIN_LIGHT_RELAY);
}

void light_relay_tick_1s(void) {
    bool desired_relay_state = false;

    if (s_emergency_evac) {
        // Priority 1: Emergency Evacuation forces lights ON for safety
        desired_relay_state = true;
    } else if (s_override_mode == LIGHT_OVERRIDE_FORCE_ON) {
        // Priority 2: Master manual override FORCE ON
        desired_relay_state = true;
    } else if (s_override_mode == LIGHT_OVERRIDE_FORCE_OFF) {
        // Priority 3: Master manual override FORCE OFF
        desired_relay_state = false;
    } else {
        // Priority 4: Intelligent Daylight-Aware Autonomous Logic
        bool occupied = motion_sensor_is_occupied();
        uint16_t current_lux = light_sensor_read_lux();

        if (occupied) {
            if (current_lux < DAYLIGHT_LUX_THRESHOLD) {
                // Occupied & Dark -> Turn lights ON
                desired_relay_state = true;
            } else {
                // Occupied & Bright Daylight -> Turn lights OFF (Energy saving!)
                desired_relay_state = false;
                ESP_LOGD(TAG, "Daylight-Aware Saving: Room is occupied but ambient lux (%d >= %d). Lights kept OFF.",
                         current_lux, DAYLIGHT_LUX_THRESHOLD);
            }
        } else {
            // Vacant room -> Lights OFF
            desired_relay_state = false;
        }
    }

    // Apply relay state to hardware
    if (desired_relay_state != s_is_relay_on) {
        s_is_relay_on = desired_relay_state;
        gpio_set_level(PIN_LIGHT_RELAY, s_is_relay_on ? 1 : 0);
        ESP_LOGI(TAG, "Relay state changed -> %s (Lux: %d, Occ: %d, Mode: %d)",
                 s_is_relay_on ? "ON" : "OFF",
                 light_sensor_read_lux(),
                 motion_sensor_is_occupied() ? 1 : 0,
                 s_override_mode);
    }

    // Track energy saved when lights are kept OFF
    if (!s_is_relay_on) {
        s_energy_saved_seconds++;
    }
}

bool light_relay_is_on(void) {
    return s_is_relay_on;
}

uint32_t light_relay_get_energy_saved_sec(void) {
    return s_energy_saved_seconds;
}

uint8_t light_relay_get_override_mode(void) {
    return s_override_mode;
}

void light_relay_set_override(uint8_t mode) {
    s_override_mode = mode;
    ESP_LOGI(TAG, "Master light override mode set to %d (0=Auto, 1=Force OFF, 2=Force ON)", mode);
}

void light_relay_set_emergency(bool active) {
    s_emergency_evac = active;
    if (active) {
        ESP_LOGW(TAG, "EMERGENCY EVACUATION ACTIVE: Forcing room lights ON!");
        s_is_relay_on = true;
        gpio_set_level(PIN_LIGHT_RELAY, 1);
    } else {
        ESP_LOGI(TAG, "Emergency Evacuation cleared. Resuming autonomous daylight schedule.");
    }
}

