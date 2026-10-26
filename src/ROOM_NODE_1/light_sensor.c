/*
 * ====================================================================
 * CoCIS Smart System - Ambient LDR Light Sensor Subsystem
 * Component: light_sensor.c
 * Owner: Zziwa Mark Alex (24/U/11978/EVE) - Group SE-24
 * ====================================================================
 */

#include "light_sensor.h"
#include <stdio.h>
#include "driver/adc.h"
#include "esp_log.h"
#include "config_defaults.h"

static const char *TAG = "LIGHT_SENSOR";

#define LDR_ADC_CHANNEL ADC1_CHANNEL_6 // GPIO 34

void light_sensor_init(void) {
    // 12-bit ADC resolution (0 - 4095), 11dB attenuation (up to ~3.3V)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_11);

    ESP_LOGI(TAG, "LDR Daylight sensor initialized on ADC1_CH6 (GPIO %d, Threshold: %d lux).",
             PIN_LDR_ADC, DAYLIGHT_LUX_THRESHOLD);
}

uint16_t light_sensor_read_lux(void) {
    int raw = adc1_get_raw(LDR_ADC_CHANNEL);
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;

    // Convert 12-bit ADC (0 - 4095) to calibrated lux (0 - 1000 lux)
    uint16_t lux = (uint16_t)(((uint32_t)raw * 1000) / 4095);
    return lux;
}

bool light_sensor_is_daylight_sufficient(void) {
    uint16_t lux = light_sensor_read_lux();
    return (lux >= DAYLIGHT_LUX_THRESHOLD);
}

