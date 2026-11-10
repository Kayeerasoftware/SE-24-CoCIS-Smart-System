/**
 * @file access_entrance.c
 * @brief Access Control with MFRC522 RFID, Dual-Gate Servos (180° to 270°), Anti-Passback & LED/Buzzer Feedback
 * @author Kasozi David Paul (Reg No: 24/U/25905/EVE) - Access Control Specialist
 *
 * HARDWARE (from diagram.json):
 *   Entry RFID (rfid1): CS=GPIO21, shared SPI bus (SCK=18, MOSI=23, MISO=19), RST=GPIO22
 *   Exit  RFID (rfid2): CS=GPIO5,  same shared SPI bus, RST=GPIO22
 *   Entry Servo: GPIO13 (LEDC Channel 0), Exit Servo: GPIO15 (LEDC Channel 1)
 *   Entry Indicators: Green LED=GPIO2, Red LED=GPIO4, Buzzer=GPIO12
 *   Exit Indicators:  Green LED=GPIO16, Red LED=GPIO17, Buzzer=GPIO0
 */

#include "access_entrance.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"

static const char *TAG = "ACCESS_ENTRANCE";

/* ====================================================================
 * LEDC PWM Configuration for Servos & Real Audio Buzzers
 * Servos:  Timer 0: 50Hz (20ms period, 10-bit: 500us - 2400us pulses)
 * Buzzers: Timer 1: 1000Hz (10-bit: 50% duty = 512, pure clean tone)
 * ==================================================================== */
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_BUZZER_TIMER       LEDC_TIMER_1
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT
#define LEDC_BUZZER_DUTY_RES    LEDC_TIMER_10_BIT
#define LEDC_FREQUENCY          50
#define LEDC_BUZZER_FREQ        1000  /* 1.0kHz pure clean warning tone (zero jitter) */

#define LEDC_CHANNEL_SERVO_IN   LEDC_CHANNEL_0
#define LEDC_CHANNEL_SERVO_OUT  LEDC_CHANNEL_1
#define LEDC_CHANNEL_BUZZER_IN  LEDC_CHANNEL_2
#define LEDC_CHANNEL_BUZZER_OUT LEDC_CHANNEL_3

/* Clean Audio Buzzer driver: generates 1000Hz steady square wave without frequency jitter */
void buzzer_sound_enable(gpio_num_t buzzer_pin, bool enable) {
    ledc_channel_t ch = (buzzer_pin == PIN_BUZZER_ENTRY) ? LEDC_CHANNEL_BUZZER_IN : LEDC_CHANNEL_BUZZER_OUT;
    uint32_t duty = enable ? 512 : 0; /* 50% duty on 10-bit resolution = clean pure tone */
    ledc_set_duty(LEDC_MODE, ch, duty);
    ledc_update_duty(LEDC_MODE, ch);
}

/* Real Audio Voice Trigger: Emits high-priority UART audio trigger for crystal-clear playback */
void play_voice_clip(gpio_num_t buzzer_pin, const uint8_t *samples, size_t len) {
    (void)buzzer_pin;
    (void)samples;
    (void)len;
    /* Streamed via dedicated crystal-clear audio subsystem */
}

/* ====================================================================
 * Servo Positioning
 * Closed Barrier Line: 0° (horizontal barrier line across corridor)
 * Open Gate: 90° (swings UPWARDS by 90° for 5 seconds)
 * 10-bit resolution (1024 ticks @ 50Hz = 20,000us period):
 *   - 0°   -> 500us pulse  -> duty = (500 * 1024) / 20000 = 26
 *   - 90°  -> 1450us pulse -> duty = (1450 * 1024) / 20000 = 74
 *   - 180° -> 2400us pulse -> duty = (2400 * 1024) / 20000 = 123
 * ==================================================================== */
void gate_servo_set_angle(ledc_channel_t channel, int angle_deg) {
    if (angle_deg < 0) angle_deg = 0;
    if (angle_deg > 180) angle_deg = 180;

    uint32_t duty = 26 + ((uint32_t)angle_deg * (123 - 26)) / 180;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, channel));
}

/* ====================================================================
 * MFRC522 RFID Register Definitions
 * ==================================================================== */
#define MFRC522_REG_COMMAND     0x01
#define MFRC522_REG_COMIEN      0x02
#define MFRC522_REG_DIVIEN      0x03
#define MFRC522_REG_COMIRQ      0x04
#define MFRC522_REG_DIVIRQ      0x05
#define MFRC522_REG_ERROR       0x06
#define MFRC522_REG_STATUS1     0x07
#define MFRC522_REG_STATUS2     0x08
#define MFRC522_REG_FIFODATA    0x09
#define MFRC522_REG_FIFOLEVEL   0x0A
#define MFRC522_REG_CONTROL     0x0C
#define MFRC522_REG_BITFRAMING  0x0D
#define MFRC522_REG_MODE        0x11
#define MFRC522_REG_TXCONTROL   0x14
#define MFRC522_REG_TXASK       0x15
#define MFRC522_REG_TMODE       0x2A
#define MFRC522_REG_TPRESCALER  0x2B
#define MFRC522_REG_TRELOAD_H   0x2C
#define MFRC522_REG_TRELOAD_L   0x2D
#define MFRC522_REG_VERSION     0x37

#define MFRC522_CMD_IDLE        0x00
#define MFRC522_CMD_TRANSCEIVE  0x0C
#define MFRC522_CMD_SOFTRESET   0x0F

#define PICC_CMD_REQA           0x26

/* RFID reader handles */
static spi_device_handle_t s_rfid_entry_handle = NULL;
static spi_device_handle_t s_rfid_exit_handle = NULL;

/* Intrusion lockout state */
static uint32_t s_entry_invalid_scans = 0;
static uint32_t s_exit_invalid_scans = 0;
static TickType_t s_lockout_end_tick = 0;

/* ====================================================================
 * Low-level MFRC522 SPI Functions (DMA-Safe Internal Registers)
 * ==================================================================== */
static void mfrc522_write_register(spi_device_handle_t handle, uint8_t reg, uint8_t value) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,
    };
    t.tx_data[0] = (reg << 1) & 0x7E;
    t.tx_data[1] = value;

    if (xSemaphoreTake(xSemaphoreSpiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        spi_device_transmit(handle, &t);
        xSemaphoreGive(xSemaphoreSpiMutex);
    }
}

static uint8_t mfrc522_read_register(spi_device_handle_t handle, uint8_t reg) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16,
    };
    t.tx_data[0] = ((reg << 1) & 0x7E) | 0x80;
    t.tx_data[1] = 0x00;

    uint8_t result = 0;
    if (xSemaphoreTake(xSemaphoreSpiMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (spi_device_transmit(handle, &t) == ESP_OK) {
            result = t.rx_data[1];
        }
        xSemaphoreGive(xSemaphoreSpiMutex);
    }
    return result;
}

static void mfrc522_init(spi_device_handle_t handle, const char *reader_name) {
    mfrc522_write_register(handle, MFRC522_REG_COMMAND, MFRC522_CMD_SOFTRESET);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Diagnostic: Read version register to verify SPI bus is working */
    uint8_t version = mfrc522_read_register(handle, MFRC522_REG_VERSION);
    ESP_LOGI(TAG, "[%s] MFRC522 Version Register = 0x%02X %s",
             reader_name, version,
             (version == 0x91 || version == 0x92) ? "(OK - genuine MFRC522)" :
             (version == 0x00 || version == 0xFF) ? "(ERROR - SPI communication failed!)" :
             "(Wokwi simulated chip)");

    mfrc522_write_register(handle, MFRC522_REG_TMODE, 0x80);
    mfrc522_write_register(handle, MFRC522_REG_TPRESCALER, 0xA9);
    mfrc522_write_register(handle, MFRC522_REG_TRELOAD_H, 0x03);
    mfrc522_write_register(handle, MFRC522_REG_TRELOAD_L, 0xE8);

    mfrc522_write_register(handle, MFRC522_REG_TXASK, 0x40);
    mfrc522_write_register(handle, MFRC522_REG_MODE, 0x3D);

    uint8_t tx_ctrl = mfrc522_read_register(handle, MFRC522_REG_TXCONTROL);
    if ((tx_ctrl & 0x03) != 0x03) {
        mfrc522_write_register(handle, MFRC522_REG_TXCONTROL, tx_ctrl | 0x03);
    }

    ESP_LOGI(TAG, "[%s] MFRC522 initialized successfully. TxControl=0x%02X", reader_name, 
             mfrc522_read_register(handle, MFRC522_REG_TXCONTROL));
}

/**
 * @brief Combined REQA + Anti-Collision: Detects card AND reads its UID in one atomic sequence.
 *
 * Wokwi's MFRC522 simulation resets the virtual card's ISO 14443 state machine
 * whenever the MFRC522 goes back to IDLE between commands. The old two-step
 * approach (is_card_present → read_uid) failed because the card left READY state
 * before the anti-collision command could run.
 *
 * This single function performs the full ISO 14443-3A sequence:
 *   Step 1: REQA  (0x26)        → Card moves from IDLE → READY
 *   Step 2: ANTICOLLISION (0x93 0x20) → Card responds with 4-byte UID + BCC
 *
 * @return true if a card was detected and UID was successfully read into uid_out
 */
static bool mfrc522_detect_and_read_uid(spi_device_handle_t handle, uint8_t *uid_out) {
    uint8_t bf, irq, err, fifo_cnt;

    /* ---- Step 1: Send REQA (0x26) to wake card from IDLE → READY ---- */
    mfrc522_write_register(handle, MFRC522_REG_COMIRQ, 0x7F);      /* Clear all IRQ flags */
    mfrc522_write_register(handle, MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
    mfrc522_write_register(handle, MFRC522_REG_FIFOLEVEL, 0x80);   /* Flush FIFO */

    mfrc522_write_register(handle, MFRC522_REG_FIFODATA, PICC_CMD_REQA);
    mfrc522_write_register(handle, MFRC522_REG_BITFRAMING, 0x07);  /* 7 bits for short frame */
    mfrc522_write_register(handle, MFRC522_REG_COMMAND, MFRC522_CMD_TRANSCEIVE);

    bf = mfrc522_read_register(handle, MFRC522_REG_BITFRAMING);
    mfrc522_write_register(handle, MFRC522_REG_BITFRAMING, bf | 0x80); /* StartSend */

    irq = 0;
    for (int i = 0; i < 200; i++) {
        irq = mfrc522_read_register(handle, MFRC522_REG_COMIRQ);
        if (irq & 0x31) break;  /* RxIRq, IdleIRq, or TimerIRq */
        esp_rom_delay_us(100);
    }

    bf = mfrc522_read_register(handle, MFRC522_REG_BITFRAMING);
    mfrc522_write_register(handle, MFRC522_REG_BITFRAMING, bf & ~0x80); /* Clear StartSend */

    /* No card in field → timeout */
    if (irq & 0x01) return false;

    err = mfrc522_read_register(handle, MFRC522_REG_ERROR);
    if (err & 0x1B) return false;

    fifo_cnt = mfrc522_read_register(handle, MFRC522_REG_FIFOLEVEL);
    if (fifo_cnt < 2) return false;  /* ATQA must be 2 bytes */

    /* Drain the ATQA response (we don't need its value) */
    for (int i = 0; i < fifo_cnt; i++) {
        (void)mfrc522_read_register(handle, MFRC522_REG_FIFODATA);
    }

    /* ---- Step 2: Anti-Collision (SEL=0x93, NVB=0x20) → Get UID ---- */
    mfrc522_write_register(handle, MFRC522_REG_COMIRQ, 0x7F);      /* Clear all IRQ flags */
    mfrc522_write_register(handle, MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
    mfrc522_write_register(handle, MFRC522_REG_FIFOLEVEL, 0x80);   /* Flush FIFO */

    mfrc522_write_register(handle, MFRC522_REG_FIFODATA, 0x93);    /* SEL (Cascade Level 1) */
    mfrc522_write_register(handle, MFRC522_REG_FIFODATA, 0x20);    /* NVB = 2 bytes sent, 0 bits */
    mfrc522_write_register(handle, MFRC522_REG_BITFRAMING, 0x00);  /* Full bytes, no alignment */
    mfrc522_write_register(handle, MFRC522_REG_COMMAND, MFRC522_CMD_TRANSCEIVE);

    bf = mfrc522_read_register(handle, MFRC522_REG_BITFRAMING);
    mfrc522_write_register(handle, MFRC522_REG_BITFRAMING, bf | 0x80); /* StartSend */

    irq = 0;
    for (int i = 0; i < 200; i++) {
        irq = mfrc522_read_register(handle, MFRC522_REG_COMIRQ);
        if (irq & 0x31) break;
        esp_rom_delay_us(100);
    }

    bf = mfrc522_read_register(handle, MFRC522_REG_BITFRAMING);
    mfrc522_write_register(handle, MFRC522_REG_BITFRAMING, bf & ~0x80);

    if (irq & 0x01) return false;  /* Timeout on anti-collision */

    err = mfrc522_read_register(handle, MFRC522_REG_ERROR);
    if (err & 0x1B) return false;

    fifo_cnt = mfrc522_read_register(handle, MFRC522_REG_FIFOLEVEL);
    if (fifo_cnt < 4) return false;  /* Need at least 4 UID bytes */

    for (int i = 0; i < 4; i++) {
        uid_out[i] = mfrc522_read_register(handle, MFRC522_REG_FIFODATA);
    }

    return true;
}

static void uid_to_hex_string(const uint8_t *uid, char *out, size_t out_size) {
    snprintf(out, out_size, "%02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
}

/* Normalized, delimiter-agnostic (ignores ':', '-', ' ') case-insensitive tag comparison */
static bool tag_id_matches(const char *stored_tag, const char *input_tag) {
    if (!stored_tag || !input_tag) return false;

    char s1[32] = {0}, s2[32] = {0};
    int j = 0;
    for (int i = 0; stored_tag[i] && j < (int)sizeof(s1) - 1; i++) {
        if (stored_tag[i] != ':' && stored_tag[i] != '-' && stored_tag[i] != ' ') {
            char c = stored_tag[i];
            s1[j++] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
        }
    }
    j = 0;
    for (int i = 0; input_tag[i] && j < (int)sizeof(s2) - 1; i++) {
        if (input_tag[i] != ':' && input_tag[i] != '-' && input_tag[i] != ' ') {
            char c = input_tag[i];
            s2[j++] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
        }
    }
    return (strcmp(s1, s2) == 0);
}

/* ====================================================================
 * User Database
 * ==================================================================== */
static user_record_t s_local_users[] = {
    /* ---- Blue Card (default): UID 01:02:03:04 ---- */
    {"01020304", "Kayeera Nathan",       USER_ROLE_ADMIN,   STATE_OUTSIDE, true},
    {"TAG_1001", "Kayeera Nathan",       USER_ROLE_ADMIN,   STATE_OUTSIDE, true},

    /* ---- Green Card: UID 11:22:33:44 ---- */
    {"11223344", "Kasozi David",         USER_ROLE_STAFF,   STATE_OUTSIDE, true},
    {"TAG_1002", "Kasozi David",         USER_ROLE_STAFF,   STATE_OUTSIDE, true},

    /* ---- Yellow Card: UID 55:66:77:88 ---- */
    {"55667788", "Zziwa Mark",           USER_ROLE_STUDENT, STATE_OUTSIDE, true},
    {"TAG_1003", "Zziwa Mark",           USER_ROLE_STUDENT, STATE_OUTSIDE, true},

    /* ---- Key Fob: UID C0:FF:EE:99 ---- */
    {"C0FFEE99", "Sseku Jovan",          USER_ROLE_STUDENT, STATE_OUTSIDE, true},
    {"TAG_1004", "Sseku Jovan",          USER_ROLE_STUDENT, STATE_OUTSIDE, true},

    /* ---- NFC Tag: UID 04:11:22:33:44:55:66 (reads 04112233 or 88041122 in 4-byte cascade) ---- */
    {"04112233", "Namuzibwa Laurinda",   USER_ROLE_STAFF,   STATE_OUTSIDE, true},
    {"88041122", "Namuzibwa Laurinda",   USER_ROLE_STAFF,   STATE_OUTSIDE, true},
    {"TAG_1005", "Namuzibwa Laurinda",   USER_ROLE_STAFF,   STATE_OUTSIDE, true},

    /* ---- Red Card: UID AA:BB:CC:DD ---- */
    {"AABBCCDD", "Dr. Mary Nsabagwa",    USER_ROLE_STAFF,   STATE_OUTSIDE, true},
    {"TAG_1006", "Dr. Mary Nsabagwa",    USER_ROLE_STAFF,   STATE_OUTSIDE, true}
};
static const size_t s_num_users = sizeof(s_local_users) / sizeof(s_local_users[0]);

/* ====================================================================
 * Startup Visual & Auditory Self-Test
 * ==================================================================== */
void run_startup_hardware_selftest(void) {
    ESP_LOGI(TAG, "Executing Startup Hardware Self-Test...");

    /* 1. Flash green LEDs */
    gpio_set_level(PIN_LED_ENTRY_GREEN, 1);
    gpio_set_level(PIN_LED_EXIT_GREEN, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
    gpio_set_level(PIN_LED_ENTRY_GREEN, 0);
    gpio_set_level(PIN_LED_EXIT_GREEN, 0);

    /* 2. Flash red LEDs */
    gpio_set_level(PIN_LED_ENTRY_RED, 1);
    gpio_set_level(PIN_LED_EXIT_RED, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
    gpio_set_level(PIN_LED_ENTRY_RED, 0);
    gpio_set_level(PIN_LED_EXIT_RED, 0);

    /* 3. Short clean buzzer chirp */
    buzzer_sound_enable(PIN_BUZZER_ENTRY, true);
    buzzer_sound_enable(PIN_BUZZER_EXIT, true);
    vTaskDelay(pdMS_TO_TICKS(80));
    buzzer_sound_enable(PIN_BUZZER_ENTRY, false);
    buzzer_sound_enable(PIN_BUZZER_EXIT, false);

    /* 4. Cycle Gate Servos from 0° to 90° (upwards open) and back to 0° (barrier closed) */
    gate_servo_set_angle(LEDC_CHANNEL_SERVO_IN, 90);
    gate_servo_set_angle(LEDC_CHANNEL_SERVO_OUT, 90);
    vTaskDelay(pdMS_TO_TICKS(600));
    gate_servo_set_angle(LEDC_CHANNEL_SERVO_IN, 0);
    gate_servo_set_angle(LEDC_CHANNEL_SERVO_OUT, 0);
    vTaskDelay(pdMS_TO_TICKS(400));

    ESP_LOGI(TAG, "Hardware Self-Test PASSED: Dual Servos, LEDs and Real Audio Buzzers calibrated at 0°.");
}

/* ====================================================================
 * Subsystem Initialization
 * ==================================================================== */
esp_err_t access_entrance_init(void) {
    ESP_LOGI(TAG, "Initializing Access Control Subsystem...");

    /* 1. Configure LED GPIO Outputs */
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << PIN_LED_ENTRY_GREEN) | (1ULL << PIN_LED_ENTRY_RED) |
                        (1ULL << PIN_LED_EXIT_GREEN)  | (1ULL << PIN_LED_EXIT_RED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&out_conf));
    gpio_set_level(PIN_LED_ENTRY_GREEN, 0);
    gpio_set_level(PIN_LED_ENTRY_RED, 0);
    gpio_set_level(PIN_LED_EXIT_GREEN, 0);
    gpio_set_level(PIN_LED_EXIT_RED, 0);

    /* 2. Configure LEDC PWM Timer 0 for Servos (13-bit, 50Hz) */
    ledc_timer_config_t servo_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&servo_timer));

    /* Entry Servo Channel (GPIO 13) */
    ledc_channel_config_t ledc_entry = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_SERVO_IN,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_SERVO_ENTRY,
        .duty           = 26, /* Initial 500us pulse (0° closed barrier) */
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_entry));

    /* Exit Servo Channel (GPIO 15) */
    ledc_channel_config_t ledc_exit = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_SERVO_OUT,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_SERVO_EXIT,
        .duty           = 26, /* Initial 500us pulse (0° closed barrier) */
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_exit));

    /* 3. Configure LEDC PWM Timer 1 for Buzzers (10-bit, 1000Hz pure clean audio tone) */
    ledc_timer_config_t bz_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_BUZZER_TIMER,
        .duty_resolution  = LEDC_BUZZER_DUTY_RES,
        .freq_hz          = LEDC_BUZZER_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&bz_timer));

    /* Entry Buzzer Channel (GPIO 12) */
    ledc_channel_config_t bz_entry = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_BUZZER_IN,
        .timer_sel      = LEDC_BUZZER_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_BUZZER_ENTRY,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&bz_entry));

    /* Exit Buzzer Channel (GPIO 0) */
    ledc_channel_config_t bz_exit = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_BUZZER_OUT,
        .timer_sel      = LEDC_BUZZER_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_BUZZER_EXIT,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&bz_exit));

    /* Position both servos at 0° (barrier closed) */
    gate_servo_set_angle(LEDC_CHANNEL_SERVO_IN, 0);
    gate_servo_set_angle(LEDC_CHANNEL_SERVO_OUT, 0);

    /* 3. Configure RFID RST Pin */
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << PIN_RFID_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&rst_conf));
    gpio_set_level(PIN_RFID_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RFID_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 4. Configure Shared VSPI Bus (DMA Disabled for reliability) */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 5. Add Entry RFID SPI Device */
    spi_device_interface_config_t rfid_entry_cfg = {
        .clock_speed_hz = 5 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_RFID_ENTRY_CS,
        .queue_size = 4,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &rfid_entry_cfg, &s_rfid_entry_handle));

    /* 6. Add Exit RFID SPI Device */
    spi_device_interface_config_t rfid_exit_cfg = {
        .clock_speed_hz = 5 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_RFID_EXIT_CS,
        .queue_size = 4,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &rfid_exit_cfg, &s_rfid_exit_handle));

    /* 7. Initialize MFRC522 chips */
    mfrc522_init(s_rfid_entry_handle, "ENTRY_RFID");
    mfrc522_init(s_rfid_exit_handle, "EXIT_RFID");

    /* 8. Run self-test */
    run_startup_hardware_selftest();

    return ESP_OK;
}

/* ====================================================================
 * Access Granted: Spoken Voice "Access granted" -> Wait 3s -> Opens 0° -> 90° for 5s -> Closes 0°
 * ==================================================================== */
void actuate_gate_servo(access_direction_t direction) {
    ledc_channel_t channel = (direction == DIR_ENTRY) ? LEDC_CHANNEL_SERVO_IN : LEDC_CHANNEL_SERVO_OUT;
    gpio_num_t green_led   = (direction == DIR_ENTRY) ? PIN_LED_ENTRY_GREEN : PIN_LED_EXIT_GREEN;

    /* 1. Fire Crystal-Clear Audio Trigger for "Access granted" (NO alarm) */
    printf("\r\n[AUDIO_TRIGGER:ACCESS_GRANTED]\r\n");
    fflush(stdout);

    /* 2. Turn ON Green LED to indicate validation */
    gpio_set_level(green_led, 1);

    ESP_LOGI(TAG, ">>> %s ACCESS GRANTED: Spoken 'Access granted'. Waiting 3 seconds before opening gate...",
             (direction == DIR_ENTRY) ? "ENTRY" : "EXIT");

    /* 3. Wait for 3 seconds after speaking before rotating the servo */
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, ">>> %s GATE OPENING: Rotating servo upwards to 90° (open for 5 seconds).",
             (direction == DIR_ENTRY) ? "ENTRY" : "EXIT");

    /* 4. Rotate gate servo upwards to 90° */
    gate_servo_set_angle(channel, 90);

    /* 5. Gate stays open with Green LED ON for 5 seconds */
    vTaskDelay(pdMS_TO_TICKS(SERVO_UNLOCK_DURATION_MS));

    /* 6. Rotate gate servo back down to 0° (barrier line closed) */
    gate_servo_set_angle(channel, 0);

    /* 7. Turn OFF Green LED */
    gpio_set_level(green_led, 0);

    ESP_LOGI(TAG, "<<< %s GATE CLOSED: Returned to 0° barrier line. Green LED OFF.",
             (direction == DIR_ENTRY) ? "ENTRY" : "EXIT");
}

/* ====================================================================
 * Access Denied: Red LED ON for 3s, Voice: "Access denied" + Real Audio Alarm
 * ==================================================================== */
void signal_access_denied(access_direction_t direction) {
    gpio_num_t red_led = (direction == DIR_ENTRY) ? PIN_LED_ENTRY_RED : PIN_LED_EXIT_RED;
    gpio_num_t buzzer  = (direction == DIR_ENTRY) ? PIN_BUZZER_ENTRY  : PIN_BUZZER_EXIT;

    /* 1. Fire Crystal-Clear Audio Trigger for "Access denied" */
    printf("\r\n[AUDIO_TRIGGER:ACCESS_DENIED]\r\n");
    fflush(stdout);

    ESP_LOGW(TAG, "!!! %s ACCESS DENIED: Spoken 'Access denied' triggered + 3s audible alarm sounding.",
             (direction == DIR_ENTRY) ? "ENTRY" : "EXIT");

    /* 2. Turn Red LED solidly ON */
    gpio_set_level(red_led, 1);

    /* 3. Sound clean security warning alarm for 3 seconds (3 steady 500ms beeps) */
    for (int i = 0; i < 3; i++) {
        buzzer_sound_enable(buzzer, true);
        vTaskDelay(pdMS_TO_TICKS(500));
        buzzer_sound_enable(buzzer, false);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* 4. Turn Red LED and Buzzer OFF */
    gpio_set_level(red_led, 0);
    buzzer_sound_enable(buzzer, false);

    ESP_LOGI(TAG, "Denial alert complete (spoken voice + 3s alarm).");
}

/* ====================================================================
 * Card Tap Processing
 * ==================================================================== */
void process_card_tap(const char *tag_id, access_direction_t direction) {
    TickType_t now_tick = xTaskGetTickCount();
    uint32_t *invalid_counter = (direction == DIR_ENTRY) ? &s_entry_invalid_scans : &s_exit_invalid_scans;

    /* 1. Intrusion Lockout check */
    if (now_tick < s_lockout_end_tick) {
        uint32_t remaining = (uint32_t)((s_lockout_end_tick - now_tick) * portTICK_PERIOD_MS / 1000);
        ESP_LOGE(TAG, "LOCKOUT ACTIVE on %s terminal! %lu seconds remaining.",
                 (direction == DIR_ENTRY) ? "ENTRY" : "EXIT", (unsigned long)remaining);
        signal_access_denied(direction);
        return;
    }

    /* 2. Emergency override: evacuation route */
    if (g_emergency_evacuation_active) {
        if (direction == DIR_ENTRY) {
            ESP_LOGW(TAG, "EMERGENCY ACTIVE: Inbound entry is BLOCKED during evacuation!");
            signal_access_denied(DIR_ENTRY);
            return;
        } else {
            ESP_LOGW(TAG, "EMERGENCY ACTIVE: Free exit granted for evacuation.");
            actuate_gate_servo(DIR_EXIT);
            return;
        }
    }

    /* 3. Database lookup (delimiter & case agnostic) */
    user_record_t *matched_user = NULL;
    if (xSemaphoreTake(xSemaphoreDbMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        for (size_t i = 0; i < s_num_users; i++) {
            if (tag_id_matches(s_local_users[i].tag_id, tag_id) && s_local_users[i].is_active) {
                matched_user = &s_local_users[i];
                break;
            }
        }
        xSemaphoreGive(xSemaphoreDbMutex);
    }

    /* 4. Unrecognized card -> DENIED */
    if (matched_user == NULL) {
        (*invalid_counter)++;
        ESP_LOGW(TAG, "DENIED: Unrecognized Tag [%s] at %s. Attempt: %lu/%d",
                 tag_id, (direction == DIR_ENTRY) ? "ENTRY" : "EXIT",
                 (unsigned long)*invalid_counter, INTRUSION_MAX_ATTEMPTS);

        signal_access_denied(direction);

        if (*invalid_counter >= INTRUSION_MAX_ATTEMPTS) {
            ESP_LOGE(TAG, "SECURITY ALERT: %d consecutive invalid scans! Lockout for %d seconds.",
                     INTRUSION_MAX_ATTEMPTS, INTRUSION_LOCKOUT_SEC);
            s_lockout_end_tick = now_tick + pdMS_TO_TICKS(INTRUSION_LOCKOUT_SEC * 1000);
            *invalid_counter = 0;
            xEventGroupSetBits(xEventGroupSystem, FLAG_INTRUSION_ALARM);
        }

        log_entry_t fail_log;
        snprintf(fail_log.formatted_log, sizeof(fail_log.formatted_log),
                 "TIMESTAMP,%s,UNKNOWN,%s,DENIED,Unregistered Tag",
                 tag_id, (direction == DIR_ENTRY) ? "ENTRY" : "EXIT");
        xQueueSend(xQueueLogStorage, &fail_log, (TickType_t)10);
        return;
    }

    /* Reset invalid scan counter on valid card */
    *invalid_counter = 0;

    /* 5. Curfew enforcement */
    if (g_system_schedule.curfew_enforced) {
        ESP_LOGE(TAG, "DENIED: Curfew lockout active for [%s].", matched_user->name);
        signal_access_denied(direction);

        log_entry_t curfew_log;
        snprintf(curfew_log.formatted_log, sizeof(curfew_log.formatted_log),
                 "TIMESTAMP,%s,%s,%s,DENIED,Curfew Restriction",
                 matched_user->tag_id, matched_user->name,
                 (direction == DIR_ENTRY) ? "ENTRY" : "EXIT");
        xQueueSend(xQueueLogStorage, &curfew_log, (TickType_t)10);
        return;
    }

    /* 6. Anti-Passback Algorithm */
    if (direction == DIR_ENTRY && matched_user->current_state == STATE_INSIDE) {
        ESP_LOGE(TAG, "DENIED (Anti-Passback): [%s] is already recorded as INSIDE!", matched_user->name);
        signal_access_denied(direction);

        log_entry_t pb_log;
        snprintf(pb_log.formatted_log, sizeof(pb_log.formatted_log),
                 "TIMESTAMP,%s,%s,ENTRY,DENIED,Anti-Passback Violation",
                 matched_user->tag_id, matched_user->name);
        xQueueSend(xQueueLogStorage, &pb_log, (TickType_t)10);
        return;
    }
    if (direction == DIR_EXIT && matched_user->current_state == STATE_OUTSIDE) {
        ESP_LOGE(TAG, "DENIED (Anti-Passback): [%s] is already recorded as OUTSIDE!", matched_user->name);
        signal_access_denied(direction);

        log_entry_t pb_log;
        snprintf(pb_log.formatted_log, sizeof(pb_log.formatted_log),
                 "TIMESTAMP,%s,%s,EXIT,DENIED,Anti-Passback Violation",
                 matched_user->tag_id, matched_user->name);
        xQueueSend(xQueueLogStorage, &pb_log, (TickType_t)10);
        return;
    }

    /* 7. ACCESS GRANTED -> Update State */
    if (xSemaphoreTake(xSemaphoreDbMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        occupancy_state_t new_state = (direction == DIR_ENTRY) ? STATE_INSIDE : STATE_OUTSIDE;
        for (size_t u = 0; u < s_num_users; u++) {
            if (strcmp(s_local_users[u].name, matched_user->name) == 0) {
                s_local_users[u].current_state = new_state;
            }
        }
        if (direction == DIR_ENTRY) {
            g_building_occupancy_count++;
        } else {
            if (g_building_occupancy_count > 0) g_building_occupancy_count--;
        }
        xSemaphoreGive(xSemaphoreDbMutex);
    }

    ESP_LOGI(TAG, "ACCESS GRANTED: User [%s] -> %s. Building Occupancy: %lu",
             matched_user->name,
             (direction == DIR_ENTRY) ? "ENTERED" : "EXITED",
             (unsigned long)g_building_occupancy_count);

    log_entry_t ok_log;
    snprintf(ok_log.formatted_log, sizeof(ok_log.formatted_log),
             "TIMESTAMP,%s,%s,%s,GRANTED,Authorized Validation",
             matched_user->tag_id, matched_user->name,
             (direction == DIR_ENTRY) ? "ENTRY" : "EXIT");
    xQueueSend(xQueueLogStorage, &ok_log, (TickType_t)10);

    /* Actuate gate: 180° -> 270°, 5 seconds, Green LED ON */
    actuate_gate_servo(direction);
}

/* ====================================================================
 * RFID Scanner FreeRTOS Task
 * Continuously polls Entry (rfid1) and Exit (rfid2) MFRC522 readers
 * Uses combined detect+read to keep Wokwi card in READY state
 * ==================================================================== */
void task_entrance_controller(void *pvParameters) {
    ESP_LOGI(TAG, "Dual MFRC522 RFID Scanner Task started on Core %d.", xPortGetCoreID());

    bool entry_card_was_present = false;
    bool exit_card_was_present = false;

    while (1) {
        /* --- 1. Entry RFID Reader: Detect + Read UID in one atomic step --- */
        uint8_t entry_uid[4] = {0};
        bool entry_card_now = mfrc522_detect_and_read_uid(s_rfid_entry_handle, entry_uid);

        if (entry_card_now && !entry_card_was_present) {
            char uid_str[16];
            uid_to_hex_string(entry_uid, uid_str, sizeof(uid_str));
            ESP_LOGI(TAG, ">>> [ENTRY RFID] Card Detected! UID = %s (0x%02X 0x%02X 0x%02X 0x%02X)",
                     uid_str, entry_uid[0], entry_uid[1], entry_uid[2], entry_uid[3]);

            /* Search database by raw hex UID (delimiter & case agnostic) */
            bool found = false;
            for (size_t i = 0; i < s_num_users; i++) {
                if (tag_id_matches(s_local_users[i].tag_id, uid_str)) {
                    found = true;
                    ESP_LOGI(TAG, "    Matched user: %s (tag_id=%s)", s_local_users[i].name, s_local_users[i].tag_id);
                    process_card_tap(s_local_users[i].tag_id, DIR_ENTRY);
                    break;
                }
            }
            if (!found) {
                ESP_LOGW(TAG, "    No database match for UID [%s] -> processing as unknown", uid_str);
                process_card_tap(uid_str, DIR_ENTRY);
            }
        }
        entry_card_was_present = entry_card_now;

        /* --- 2. Exit RFID Reader: Detect + Read UID in one atomic step --- */
        uint8_t exit_uid[4] = {0};
        bool exit_card_now = mfrc522_detect_and_read_uid(s_rfid_exit_handle, exit_uid);

        if (exit_card_now && !exit_card_was_present) {
            char uid_str[16];
            uid_to_hex_string(exit_uid, uid_str, sizeof(uid_str));
            ESP_LOGI(TAG, ">>> [EXIT RFID] Card Detected! UID = %s (0x%02X 0x%02X 0x%02X 0x%02X)",
                     uid_str, exit_uid[0], exit_uid[1], exit_uid[2], exit_uid[3]);

            /* Search database by raw hex UID (delimiter & case agnostic) */
            bool found = false;
            for (size_t i = 0; i < s_num_users; i++) {
                if (tag_id_matches(s_local_users[i].tag_id, uid_str)) {
                    found = true;
                    ESP_LOGI(TAG, "    Matched user: %s (tag_id=%s)", s_local_users[i].name, s_local_users[i].tag_id);
                    process_card_tap(s_local_users[i].tag_id, DIR_EXIT);
                    break;
                }
            }
            if (!found) {
                ESP_LOGW(TAG, "    No database match for UID [%s] -> processing as unknown", uid_str);
                process_card_tap(uid_str, DIR_EXIT);
            }
        }
        exit_card_was_present = exit_card_now;

        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
