#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 * HARDWARE GPIO PIN ASSIGNMENTS (Matching Wokwi diagram.json)
 * ==================================================================== */

/* --- Access Control: Entry Gate Side --- */
#define PIN_RFID_ENTRY_CS      GPIO_NUM_21  /* MFRC522 Entry Reader SDA/CS */
#define PIN_RFID_EXIT_CS       GPIO_NUM_5   /* MFRC522 Exit Reader SDA/CS */
#define PIN_RFID_RST           GPIO_NUM_22  /* Shared MFRC522 RST */
#define PIN_SERVO_ENTRY        GPIO_NUM_13  /* Entry Barrier Servo (PWM) */
#define PIN_SERVO_EXIT         GPIO_NUM_15  /* Exit Barrier Servo (PWM) */

/* --- Access Control: LED Indicators --- */
#define PIN_LED_ENTRY_GREEN    GPIO_NUM_2   /* Entry Gate Green LED (Granted) */
#define PIN_LED_ENTRY_RED      GPIO_NUM_4   /* Entry Gate Red LED (Denied) */
#define PIN_LED_EXIT_GREEN     GPIO_NUM_16  /* Exit Gate Green LED (Granted) */
#define PIN_LED_EXIT_RED       GPIO_NUM_17  /* Exit Gate Red LED (Denied) */

/* --- Emergency & Alarm Buzzers --- */
#define PIN_BTN_EMERGENCY      GPIO_NUM_32  /* Red Pushbutton: Emergency Evacuation ISR */
#define PIN_BUZZER_ENTRY       GPIO_NUM_12  /* Entry Piezo Alarm Buzzer */
#define PIN_BUZZER_EXIT        GPIO_NUM_0   /* Exit Piezo Alarm Buzzer */
#define PIN_BUZZER             PIN_BUZZER_ENTRY /* Global / Default Buzzer alias */

/* --- Shared SPI Bus (RFID + SD Card) --- */
#define PIN_SPI_SCK            GPIO_NUM_18  /* VSPI Clock */
#define PIN_SPI_MISO           GPIO_NUM_19  /* VSPI MISO */
#define PIN_SPI_MOSI           GPIO_NUM_23  /* VSPI MOSI */
#define PIN_SD_CS              GPIO_NUM_27  /* MicroSD Card SPI CS */

/* --- Room 1: Smart Lighting --- */
#define PIN_PIR_ROOM1          GPIO_NUM_14  /* Room 1 PIR Motion Sensor */
#define PIN_LDR_ROOM1          GPIO_NUM_34  /* Room 1 LDR (ADC1_CH6) */
#define PIN_RELAY_ROOM1        GPIO_NUM_26  /* Room 1 Relay Module IN */

/* --- Room 2: Smart Lighting --- */
#define PIN_PIR_ROOM2          GPIO_NUM_33  /* Room 2 PIR Motion Sensor */
#define PIN_LDR_ROOM2          GPIO_NUM_35  /* Room 2 LDR (ADC1_CH7) */
#define PIN_RELAY_ROOM2        GPIO_NUM_25  /* Room 2 Relay Module IN */

/* --- UART Console --- */
#define PIN_UART_TX            GPIO_NUM_1   /* Console TX */
#define PIN_UART_RX            GPIO_NUM_3   /* Console RX */

/* ====================================================================
 * SYSTEM TIMINGS & OPERATING THRESHOLDS
 * ==================================================================== */
#define SERVO_UNLOCK_DURATION_MS    5000    /* Gate stays open for 5 seconds */
#define ACCESS_DENIED_DURATION_MS   3000    /* Red light & buzzer for 3 seconds */
#define NO_MOTION_TIMEOUT_SEC       30000      /* Room lights off after 30s no motion */
#define DAYLIGHT_THRESHOLD_LUX      500     /* Inhibit lighting if lux >= 500 */
#define INTRUSION_MAX_ATTEMPTS      3       /* Lockout trigger limit */
#define INTRUSION_LOCKOUT_SEC       60      /* Buzzer lockout penalty duration */
#define CLI_SESSION_TIMEOUT_SEC     300000     /* Admin auto-logout after 5 mins */

/* ====================================================================
 * FREERTOS EVENT GROUP FLAG BITS
 * ==================================================================== */
#define FLAG_EMERGENCY_ACTIVE       (1 << 0)
#define FLAG_CURFEW_LOCKED          (1 << 1)
#define FLAG_INTRUSION_ALARM        (1 << 2)
#define FLAG_ADMIN_LOGGED_IN        (1 << 3)

/* ====================================================================
 * DATA STRUCTURES & PROTOCOL FRAMES
 * ==================================================================== */
typedef enum {
    DIR_ENTRY = 0,
    DIR_EXIT  = 1
} access_direction_t;

typedef enum {
    DECISION_GRANTED = 0,
    DECISION_DENIED  = 1
} access_decision_t;

typedef enum {
    USER_ROLE_STUDENT = 0,
    USER_ROLE_STAFF   = 1,
    USER_ROLE_ADMIN   = 2
} user_role_t;

typedef enum {
    STATE_OUTSIDE = 0,
    STATE_INSIDE  = 1
} occupancy_state_t;

typedef struct {
    char tag_id[16];
    char name[32];
    user_role_t role;
    occupancy_state_t current_state;
    bool is_active;
} user_record_t;

typedef struct {
    char timestamp[24];
    char tag_id[16];
    char user_name[32];
    access_direction_t direction;
    access_decision_t decision;
    char reason[48];
} access_event_t;

typedef struct {
    uint8_t room_id;            /* 1 or 2 */
    bool occupied;
    uint32_t ambient_lux;
    bool lights_on;
    uint32_t countdown_sec;
    float cumulative_kwh_saved;
} room_telemetry_t;

typedef struct {
    char formatted_log[128];
} log_entry_t;

typedef struct {
    uint8_t start_hour;
    uint8_t start_min;
    uint8_t end_hour;
    uint8_t end_min;
    bool curfew_enforced;
} access_schedule_t;

/* ====================================================================
 * GLOBAL FREERTOS SYNCHRONIZATION HANDLES
 * ==================================================================== */
extern QueueHandle_t xQueueAccessEvents;
extern QueueHandle_t xQueueTelemetry;
extern QueueHandle_t xQueueLogStorage;
extern EventGroupHandle_t xEventGroupSystem;
extern SemaphoreHandle_t xSemaphoreSpiMutex;
extern SemaphoreHandle_t xSemaphoreDbMutex;

/* Global state trackers */
extern access_schedule_t g_system_schedule;
extern volatile uint32_t g_building_occupancy_count;
extern volatile bool g_emergency_evacuation_active;

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */
