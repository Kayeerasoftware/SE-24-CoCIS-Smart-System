// ZZIWA MARK ALEX
// 24/U/11978/EVE
// 2400711978
// SMART LIGHTING, PIR MOTION, LDR LUX INTERFACING

#include "room_lighting.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "ROOM_LIGHTING";

static adc_oneshot_unit_handle_t s_adc1_handle = NULL;

static room_telemetry_t s_room1_data = {
    .room_id = 1, .occupied = false, .ambient_lux = 500, .lights_on = false, .countdown_sec = 0, .cumulative_kwh_saved = 0.0f};
static room_telemetry_t s_room2_data = {
    .room_id = 2, .occupied = false, .ambient_lux = 500, .lights_on = false, .countdown_sec = 0, .cumulative_kwh_saved = 0.0f};

// Initialization

esp_err_t room_lighting_init(void)
{
    ESP_LOGI(TAG, "Initializing Room 1 & Room 2 Sensors and Relay Actuators...");

    // Configure PIR Motion Sensor Inputs
    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << PIN_PIR_ROOM1) | (1ULL << PIN_PIR_ROOM2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&pir_conf);

    // Configure Relay Output Pins
    // PNP relay module: IN=HIGH → relay OFF (default safe state)
    gpio_config_t relay_conf = {
        .pin_bit_mask = (1ULL << PIN_RELAY_ROOM1) | (1ULL << PIN_RELAY_ROOM2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&relay_conf);
    // Start with lights OFF (relay de-energized = IN HIGH for PNP)
    gpio_set_level(PIN_RELAY_ROOM1, 1);
    gpio_set_level(PIN_RELAY_ROOM2, 1);

    // Configure ADC1 for LDR Sensors
    adc_oneshot_unit_init_cfg_t adc_init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_cfg, &s_adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, ADC_CHANNEL_6, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, ADC_CHANNEL_7, &chan_cfg));

    ESP_LOGI(TAG, "Room 1 (PIR=%d, LDR=%d, Relay=%d) initialized.",
             PIN_PIR_ROOM1, PIN_LDR_ROOM1, PIN_RELAY_ROOM1);
    ESP_LOGI(TAG, "Room 2 (PIR=%d, LDR=%d, Relay=%d) initialized.",
             PIN_PIR_ROOM2, PIN_LDR_ROOM2, PIN_RELAY_ROOM2);
    return ESP_OK;
}

//Get Telemetry Data (for CLI/display)
room_telemetry_t get_room_telemetry(uint8_t room_id)
{
    return (room_id == 1) ? s_room1_data : s_room2_data;
}

//Read ambient light level from LDR via ADC
//Maps 12-bit ADC raw (0–4095) to lux (0–1000)
//Higher ADC value = more light = higher lux
static uint32_t read_lux_from_adc(adc_channel_t channel)
{
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(s_adc1_handle, channel, &raw);
    if (ret != ESP_OK || raw < 0)
        raw = 0;
    uint32_t lux = (uint32_t)((raw * 1000) / 4095);
    return lux;
}

//Set relay state (accounting for PNP active-low logic)
//lights_on=true
//then GPIO_LEVEL=LOW  and relay energized turning LED on
//lights_on=false
//then GPIO_LEVEL=HIGH and relay off turning LED off
static void set_room_light(gpio_num_t relay_pin, bool lights_on)
{
    gpio_set_level(relay_pin, lights_on ? 0 : 1);
}
// CONTROLLER TASK FOR ROOM 1
void task_room1_controller(void *pvParameters)
{
    ESP_LOGI(TAG, "Room 1 Controller Task started on Core %d.", xPortGetCoreID());
    TickType_t last_telemetry_tick = xTaskGetTickCount();
    uint32_t seconds_light_off_saved = 0;

    while (1)
    {
        //read sensors
        int pir_motion = gpio_get_level(PIN_PIR_ROOM1);
        uint32_t ambient_lux = read_lux_from_adc(ADC_CHANNEL_6);

        //Emergency override: force all lights ON for evacuation visibility
        if (g_emergency_evacuation_active)
        {
            set_room_light(PIN_RELAY_ROOM1, true);
            s_room1_data.lights_on = true;
            s_room1_data.occupied = true;
            s_room1_data.ambient_lux = ambient_lux;
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        //if motion detected
        //then room occupied
        //reset 30s countdown */
        if (pir_motion == 1)
        {
            if (!s_room1_data.occupied)
            {
                ESP_LOGI(TAG, "Room 1: Motion detected! Occupancy = YES");
            }
            s_room1_data.occupied = true;
            s_room1_data.countdown_sec = NO_MOTION_TIMEOUT_SEC;
        }

        //Lighting control logic
        if (s_room1_data.countdown_sec > 0)
        {
            //Room is occupied
            if (ambient_lux < DAYLIGHT_THRESHOLD_LUX)
            {
                //when its dark
                //then turn lights ON
                set_room_light(PIN_RELAY_ROOM1, true);
                s_room1_data.lights_on = true;
            }
            else
            {
                //Sufficient daylight → keep lights OFF (energy saving!)
                set_room_light(PIN_RELAY_ROOM1, false);
                s_room1_data.lights_on = false;
                seconds_light_off_saved++;
            }
            s_room1_data.countdown_sec--;

            if (s_room1_data.countdown_sec == 0)
            {
                ESP_LOGI(TAG, "Room 1: 30s no-motion timeout. Occupancy = NO, lights OFF.");
            }
        }
        else
        {
            //Room is vacant
            //lights OFF
            s_room1_data.occupied = false;
            set_room_light(PIN_RELAY_ROOM1, false);
            s_room1_data.lights_on = false;
            seconds_light_off_saved++;
        }

        //Update telemetry
        s_room1_data.ambient_lux = ambient_lux;
        // formula for Energy saved in kWh: (100W fixture × time_off_seconds)/ 3600
        s_room1_data.cumulative_kwh_saved = (0.100f * seconds_light_off_saved) / 3600.0f;

        //Send telemetry to queue every 2 seconds
        if ((xTaskGetTickCount() - last_telemetry_tick) >= pdMS_TO_TICKS(2000))
        {
            xQueueSend(xQueueTelemetry, &s_room1_data, (TickType_t)0);
            last_telemetry_tick = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); /* 1-second control loop */
    }
}

//CONTROLLER TASK FOR ROOM 2
void task_room2_controller(void *pvParameters)
{
    ESP_LOGI(TAG, "Room 2 Controller Task started on Core %d.", xPortGetCoreID());
    TickType_t last_telemetry_tick = xTaskGetTickCount();
    uint32_t seconds_light_off_saved = 0;

    while (1)
    {
        //Read sensors
        int pir_motion = gpio_get_level(PIN_PIR_ROOM2);
        uint32_t ambient_lux = read_lux_from_adc(ADC_CHANNEL_7);

        //Emergency override
        if (g_emergency_evacuation_active)
        {
            set_room_light(PIN_RELAY_ROOM2, true);
            s_room2_data.lights_on = true;
            s_room2_data.occupied = true;
            s_room2_data.ambient_lux = ambient_lux;
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        //Motion detected condition
        if (pir_motion == 1)
        {
            if (!s_room2_data.occupied)
            {
                ESP_LOGI(TAG, "Room 2: Motion detected! Occupancy = YES");
            }
            s_room2_data.occupied = true;
            s_room2_data.countdown_sec = NO_MOTION_TIMEOUT_SEC;
        }

        //Lighting control
        if (s_room2_data.countdown_sec > 0)
        {
            if (ambient_lux < DAYLIGHT_THRESHOLD_LUX)
            {
                set_room_light(PIN_RELAY_ROOM2, true);
                s_room2_data.lights_on = true;
            }
            else
            {
                set_room_light(PIN_RELAY_ROOM2, false);
                s_room2_data.lights_on = false;
                seconds_light_off_saved++;
            }
            s_room2_data.countdown_sec--;

            if (s_room2_data.countdown_sec == 0)
            {
                ESP_LOGI(TAG, "Room 2: 30s no-motion timeout. Occupancy = NO, lights OFF.");
            }
        }
        else
        {
            s_room2_data.occupied = false;
            set_room_light(PIN_RELAY_ROOM2, false);
            s_room2_data.lights_on = false;
            seconds_light_off_saved++;
        }

        s_room2_data.ambient_lux = ambient_lux;
        s_room2_data.cumulative_kwh_saved = (0.100f * seconds_light_off_saved) / 3600.0f;

        if ((xTaskGetTickCount() - last_telemetry_tick) >= pdMS_TO_TICKS(2000))
        {
            xQueueSend(xQueueTelemetry, &s_room2_data, (TickType_t)0);
            last_telemetry_tick = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
