/*
 * ====================================================================
 * CoCIS Smart System - Ambient LDR Light Sensor (Room 2)
 * Component: light_sensor.c
 * Owner: Zziwa Mark Alex (24/U/11978/EVE) - Group SE-24
 * ====================================================================
 */

#include "light_sensor.h"
#include <stdio.h>
#include "driver/adc.h"
#include "esp_log.h"
#include "config_defaults.h"

static const char *TAG = "LIGHT_SENSOR_2";

#define LDR_ADC_CHANNEL ADC1_CHANNEL_6 // GPIO 34

void light_sensor_init(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LDR_ADC_CHANNEL, ADC_ATTEN_DB_11);
    ESP_LOGI(TAG, "Room 2 LDR sensor initialized on GPIO %d.", PIN_LDR_ADC);
}

uint16_t light_sensor_read_lux(void) {
    int raw = adc1_get_raw(LDR_ADC_CHANNEL);
    if (raw < 0) raw = 0;
    if (raw > 4095) raw = 4095;
    uint16_t lux = (uint16_t)(((uint32_t)raw * 1000) / 4095);
    return lux;
}

bool light_sensor_is_daylight_sufficient(void) {
    return (light_sensor_read_lux() >= DAYLIGHT_LUX_THRESHOLD);
}

