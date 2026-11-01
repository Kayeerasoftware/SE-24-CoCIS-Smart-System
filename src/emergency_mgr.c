/**
 * @file emergency_mgr.c
 * @brief Emergency Evacuation Controller — Exit gate opens to 270°, dual buzzers siren, room lights ON
 * @author Kayeera Nathan (Reg No: 24/U/25913/EVE) - Group Leader & Architect
 */

#include "emergency_mgr.h"
#include "access_entrance.h"

static const char *TAG = "EMERGENCY_MGR";

/* ====================================================================
 * Initialization
 * ==================================================================== */
esp_err_t emergency_mgr_init(void) {
    ESP_LOGI(TAG, "Configuring Emergency Pushbutton on GPIO %d (Active Low, Pull-Up)...", PIN_BTN_EMERGENCY);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BTN_EMERGENCY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  /* Polled with debouncing in task to prevent startup glitch */
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) return err;

    g_emergency_evacuation_active = false;
    ESP_LOGI(TAG, "Emergency Manager initialized. Evacuation State = NORMAL.");
    return ESP_OK;
}

/* ====================================================================
 * Trigger / Clear Emergency Evacuation
 * ==================================================================== */
void trigger_emergency_evacuation(bool active, const char *source) {
    if (active) {
        if (g_emergency_evacuation_active) return; /* Already active */
        g_emergency_evacuation_active = true;
        xEventGroupSetBits(xEventGroupSystem, FLAG_EMERGENCY_ACTIVE);
        ESP_LOGE(TAG, "*** [EMERGENCY EVACUATION ACTIVATED] Source: %s ***", source);

        /* 1. Force room lighting relays ON for visibility (PNP relay: IN=LOW turns ON) */
        gpio_set_level(PIN_RELAY_ROOM1, 0);
        gpio_set_level(PIN_RELAY_ROOM2, 0);

        /* 2. Open ONLY the EXIT gate upwards to 90° */
        ESP_LOGW(TAG, "EMERGENCY: EXIT Gate OPENED upwards to 90°. Entry Gate LOCKED at 0°.");
        gate_servo_set_angle(LEDC_CHANNEL_1, 90);
        gpio_set_level(PIN_LED_EXIT_GREEN, 1);

        /* 3. Keep Entry gate strictly locked at 0° (barrier closed) */
        gate_servo_set_angle(LEDC_CHANNEL_0, 0);
        gpio_set_level(PIN_LED_ENTRY_GREEN, 0);

        /* 4. Start sirens with real audio */
        buzzer_sound_enable(PIN_BUZZER_ENTRY, true);
        buzzer_sound_enable(PIN_BUZZER_EXIT, true);

        log_entry_t alert_log;
        snprintf(alert_log.formatted_log, sizeof(alert_log.formatted_log),
                 "TIMESTAMP,SYSTEM,%s,EMERGENCY,ACTIVATED,Evacuation Route Open", source);
        xQueueSend(xQueueLogStorage, &alert_log, (TickType_t)10);
    } else {
        if (!g_emergency_evacuation_active) return; /* Already normal */
        g_emergency_evacuation_active = false;
        xEventGroupClearBits(xEventGroupSystem, FLAG_EMERGENCY_ACTIVE);
        ESP_LOGI(TAG, "=== [EMERGENCY CLEARED] Source: %s. Returning to Normal Operation. ===", source);

        /* 1. Close Exit gate back to 0° */
        gate_servo_set_angle(LEDC_CHANNEL_1, 0);
        gpio_set_level(PIN_LED_EXIT_GREEN, 0);

        /* 2. Silence both buzzers */
        buzzer_sound_enable(PIN_BUZZER_ENTRY, false);
        buzzer_sound_enable(PIN_BUZZER_EXIT, false);

        /* 3. Turn off room lights (revert to automated sensor control) */
        gpio_set_level(PIN_RELAY_ROOM1, 1);
        gpio_set_level(PIN_RELAY_ROOM2, 1);

        log_entry_t clear_log;
        snprintf(clear_log.formatted_log, sizeof(clear_log.formatted_log),
                 "TIMESTAMP,SYSTEM,%s,EMERGENCY,DEACTIVATED,Normal Operation Restored", source);
        xQueueSend(xQueueLogStorage, &clear_log, (TickType_t)10);
    }
}

/* ====================================================================
 * Emergency Task (Monitors Pushbutton & Pulses Alarm Siren)
 * ==================================================================== */
void task_emergency_handler(void *pvParameters) {
    ESP_LOGI(TAG, "Emergency Handler Task running on Core %d.", xPortGetCoreID());

    /* Wait 500ms after boot for all voltages and pull-ups to settle */
    vTaskDelay(pdMS_TO_TICKS(500));

    int prev_btn_state = gpio_get_level(PIN_BTN_EMERGENCY);

    while (1) {
        int current_btn = gpio_get_level(PIN_BTN_EMERGENCY);

        /* Detect button click (falling edge: transition from 1 to 0) */
        if (current_btn == 0 && prev_btn_state == 1) {
            vTaskDelay(pdMS_TO_TICKS(50)); /* Debounce */
            if (gpio_get_level(PIN_BTN_EMERGENCY) == 0) {
                /* Toggle Emergency Mode on every button click */
                bool new_state = !g_emergency_evacuation_active;
                trigger_emergency_evacuation(new_state, "RED_EMERGENCY_BUTTON");

                /* Wait for button release */
                while (gpio_get_level(PIN_BTN_EMERGENCY) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        }
        prev_btn_state = current_btn;

        /* If emergency is currently active, pulse the dual sirens with real audio */
        if (g_emergency_evacuation_active) {
            buzzer_sound_enable(PIN_BUZZER_ENTRY, true);
            buzzer_sound_enable(PIN_BUZZER_EXIT, true);
            vTaskDelay(pdMS_TO_TICKS(200));
            buzzer_sound_enable(PIN_BUZZER_ENTRY, false);
            buzzer_sound_enable(PIN_BUZZER_EXIT, false);
            vTaskDelay(pdMS_TO_TICKS(150));
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

/* ====================================================================
 * Heartbeat Watchdog Task
 * ==================================================================== */
void task_system_heartbeat(void *pvParameters) {
    ESP_LOGI(TAG, "Heartbeat Watchdog Task active on Core %d.", xPortGetCoreID());

    while (1) {
        ESP_LOGD(TAG, "Heartbeat tick. Free Heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
