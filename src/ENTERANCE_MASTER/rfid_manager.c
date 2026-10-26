/*
 * ====================================================================
 * CoCIS Smart System - Dual RFID Manager Subsystem (Entry & Exit)
 * Component: rfid_manager.c
 * Owner: Kasozi David Paul (24/U/25905/EVE) - Group SE-24
 * ====================================================================
 */

#include "rfid_manager.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "config_defaults.h"
#include "sync_objects.h"
#include "schedule_enforcer.h"

static const char *TAG = "RFID_MANAGER";

// RC522 Commands & Registers
#define RC522_CMD_IDLE           0x00
#define RC522_CMD_MEM            0x01
#define RC522_CMD_CALCCRC        0x03
#define RC522_CMD_TRANSMIT       0x04
#define RC522_CMD_NORESERVED     0x07
#define RC522_CMD_RECEIVE        0x08
#define RC522_CMD_TRANSCEIVE     0x0C
#define RC522_CMD_MFAUTH         0x0E
#define RC522_CMD_SOFTRESET      0x0F

#define RC522_REG_COMMAND        0x01
#define RC522_REG_COMM_IEN       0x02
#define RC522_REG_COMM_IRQ       0x04
#define RC522_REG_DIV_IRQ        0x05
#define RC522_REG_ERROR          0x06
#define RC522_REG_FIFO_DATA      0x09
#define RC522_REG_FIFO_LEVEL     0x0A
#define RC522_REG_CONTROL        0x0C
#define RC522_REG_BIT_FRAMING    0x0D
#define RC522_REG_MODE           0x11
#define RC522_REG_TX_CONTROL     0x14
#define RC522_REG_TX_AUTO        0x15
#define RC522_REG_VERSION        0x37

static spi_device_handle_t s_spi_entry = NULL;
static spi_device_handle_t s_spi_exit  = NULL;

static void rc522_write_reg(spi_device_handle_t spi, uint8_t reg, uint8_t val) {
    if (!spi) return;
    uint8_t tx_data[2] = { (uint8_t)((reg << 1) & 0x7E), val };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data
    };
    spi_device_polling_transmit(spi, &t);
}

static uint8_t rc522_read_reg(spi_device_handle_t spi, uint8_t reg) {
    if (!spi) return 0;
    uint8_t tx_data[2] = { (uint8_t)(((reg << 1) & 0x7E) | 0x80), 0x00 };
    uint8_t rx_data[2] = { 0 };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data
    };
    spi_device_polling_transmit(spi, &t);
    return rx_data[1];
}

static void rc522_init_device(spi_device_handle_t spi) {
    if (!spi) return;
    // Perform soft reset
    rc522_write_reg(spi, RC522_REG_COMMAND, RC522_CMD_SOFTRESET);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Configure Timer & Auto antenna
    rc522_write_reg(spi, RC522_REG_MODE, 0x3D);
    rc522_write_reg(spi, RC522_REG_TX_AUTO, 0x40);

    // Turn antenna on
    uint8_t tx_ctrl = rc522_read_reg(spi, RC522_REG_TX_CONTROL);
    if ((tx_ctrl & 0x03) != 0x03) {
        rc522_write_reg(spi, RC522_REG_TX_CONTROL, tx_ctrl | 0x03);
    }
}

void rfid_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Dual MFRC522 Readers (Entry CS=GPIO %d, Exit CS=GPIO %d)...",
             PIN_RFID_ENTRY_CS, PIN_RFID_EXIT_CS);

    // Reset pin configuration
    gpio_set_direction(PIN_RFID_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RFID_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RFID_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Entry Reader SPI device
    spi_device_interface_config_t dev_entry = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_RFID_ENTRY_CS,
        .queue_size = 1
    };
    esp_err_t ret = spi_bus_add_device(SPI2_HOST, &dev_entry, &s_spi_entry);
    if (ret == ESP_OK) {
        rc522_init_device(s_spi_entry);
        uint8_t v = rc522_read_reg(s_spi_entry, RC522_REG_VERSION);
        ESP_LOGI(TAG, "MFRC522 Entry Reader online (Firmware Version: 0x%02X)", v);
    } else {
        ESP_LOGW(TAG, "SPI Entry device add returned: %s", esp_err_to_name(ret));
    }

    // Exit Reader SPI device
    spi_device_interface_config_t dev_exit = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_RFID_EXIT_CS,
        .queue_size = 1
    };
    ret = spi_bus_add_device(SPI2_HOST, &dev_exit, &s_spi_exit);
    if (ret == ESP_OK) {
        rc522_init_device(s_spi_exit);
        uint8_t v = rc522_read_reg(s_spi_exit, RC522_REG_VERSION);
        ESP_LOGI(TAG, "MFRC522 Exit Reader online (Firmware Version: 0x%02X)", v);
    } else {
        ESP_LOGW(TAG, "SPI Exit device add returned: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Dual RFID Manager subsystem operational.");
}

bool rfid_poll_reader(rfid_reader_t reader, char *tag_id_out, size_t max_len) {
    spi_device_handle_t spi = (reader == RFID_READER_ENTRY) ? s_spi_entry : s_spi_exit;
    if (!spi || !tag_id_out || max_len < 16) return false;

    // Check FIFO / IRQ for card presence
    uint8_t fifo_level = rc522_read_reg(spi, RC522_REG_FIFO_LEVEL);
    if (fifo_level >= 4) {
        uint8_t b0 = rc522_read_reg(spi, RC522_REG_FIFO_DATA);
        uint8_t b1 = rc522_read_reg(spi, RC522_REG_FIFO_DATA);
        uint8_t b2 = rc522_read_reg(spi, RC522_REG_FIFO_DATA);
        uint8_t b3 = rc522_read_reg(spi, RC522_REG_FIFO_DATA);
        snprintf(tag_id_out, max_len, "%02X%02X%02X%02X", b0, b1, b2, b3);
        return true;
    }
    return false;
}

void rfid_simulate_swipe(door_direction_t dir, const char *tag_id) {
    if (!tag_id) return;

    access_event_t event;
    memset(&event, 0, sizeof(access_event_t));
    strncpy(event.tag_id, tag_id, sizeof(event.tag_id) - 1);
    event.direction = (dir == DOOR_ENTRY) ? 0 : 1;
    event.timestamp = 0; // Filled by access control task

    ESP_LOGI(TAG, "SIMULATED SWIPE: Tag=%s on %s terminal.",
             tag_id, dir == DOOR_ENTRY ? "ENTRY" : "EXIT");

    if (xAccessEventQueue) {
        if (xQueueSend(xAccessEventQueue, &event, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Access event queue full. Swipe dropped.");
        }
    }
}

