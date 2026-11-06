/**
 * @file storage_manager.c
 * @brief MicroSD SPI FATFS Driver, CSV Managers & Power-Loss Crash Recovery
 * @author Sseku Jovan (Reg No: 24/U/11172/EVE) - Persistent Storage & Crash Recovery Lead
 */

#include "storage_manager.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"

static const char *TAG = "STORAGE_MGR";
#define MOUNT_POINT "/sdcard"

// Keep the mounted card state available to the logging and recovery routines.
static sdmmc_card_t *s_card = NULL;
static bool s_sd_mounted = false;

esp_err_t storage_manager_init(void) {
    ESP_LOGI(TAG, "Initializing MicroSD Card on VSPI (CS: GPIO %d)...", PIN_SD_CS);

    // Do not format a card automatically if mounting fails; preserve its contents.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Configure the SPI bus pins used to communicate with the SD card.
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // Serialize bus setup with other SPI users to avoid conflicting access.
    if (xSemaphoreTake(xSemaphoreSpiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret == ESP_OK) {
            // Bind the SD card's chip-select pin to the shared SPI host.
            sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
            slot_config.gpio_cs = PIN_SD_CS;
            slot_config.host_id = SPI2_HOST;

            sdmmc_host_t host = SDSPI_HOST_DEFAULT();
            host.slot = SPI2_HOST;

            ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
            if (ret == ESP_OK) {
                s_sd_mounted = true;
                ESP_LOGI(TAG, "FATFS Successfully mounted at %s", MOUNT_POINT);
            } else {
                ESP_LOGW(TAG, "SD Card mount failed (%s). Simulation will use in-memory logging fallback.", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "SPI Bus init notice: %s. Using in-memory fallback.", esp_err_to_name(ret));
        }
        xSemaphoreGive(xSemaphoreSpiMutex);
    }

    // Restore the last saved occupancy count after the card mount attempt.
    perform_crash_recovery();
    return ESP_OK;
}

esp_err_t perform_crash_recovery(void) {
    ESP_LOGI(TAG, "Executing Startup Crash Recovery Evaluation...");

    if (!s_sd_mounted) {
        ESP_LOGI(TAG, "Clean initial boot: Building occupancy initialized to 0.");
        g_building_occupancy_count = 0;
        return ESP_OK;
    }

    // Read the first data row after the CSV header and restore its count.
    if (xSemaphoreTake(xSemaphoreSpiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        FILE *f = fopen(MOUNT_POINT "/occupancy.csv", "r");
        if (f != NULL) {
            char line[64];
            if (fgets(line, sizeof(line), f)) {
                // Skip the CSV column names before reading the saved count.
            }
            if (fgets(line, sizeof(line), f)) {
                uint32_t recovered_count = 0;
                if (sscanf(line, "%lu", (unsigned long *)&recovered_count) == 1) {
                    g_building_occupancy_count = recovered_count;
                    ESP_LOGI(TAG, "CRASH RECOVERY SUCCESS: Restored building occupancy count = %lu", (unsigned long)recovered_count);
                }
            }
            fclose(f);
        }
        xSemaphoreGive(xSemaphoreSpiMutex);
    }
    return ESP_OK;
}

esp_err_t log_access_to_sd(const char *formatted_entry) {
    ESP_LOGI(TAG, "[AUDIT LOG] %s", formatted_entry);

    // Keep console logging available when an SD card is not mounted.
    if (!s_sd_mounted) {
        return ESP_OK; // Gracefully logged to console
    }

    // Append each audit record so existing history is retained.
    if (xSemaphoreTake(xSemaphoreSpiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        FILE *f = fopen(MOUNT_POINT "/access_log.csv", "a");
        if (f != NULL) {
            fprintf(f, "%s\n", formatted_entry);
            fclose(f);
        }
        xSemaphoreGive(xSemaphoreSpiMutex);
    }
    return ESP_OK;
}

esp_err_t update_sd_occupancy_snapshot(uint32_t count) {
    // Occupancy persistence is optional; the live count remains in memory.
    if (!s_sd_mounted) return ESP_OK;

    // Replace the previous snapshot with the latest count and CSV header.
    if (xSemaphoreTake(xSemaphoreSpiMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        FILE *f = fopen(MOUNT_POINT "/occupancy.csv", "w");
        if (f != NULL) {
            fprintf(f, "building_count,timestamp\n");
            fprintf(f, "%lu,2026-10-31 22:00:00\n", (unsigned long)count);
            fclose(f);
        }
        xSemaphoreGive(xSemaphoreSpiMutex);
    }
    return ESP_OK;
}

void task_storage_writer(void *pvParameters) {
    ESP_LOGI(TAG, "Storage Writer Task started on Core %d.", xPortGetCoreID());
    log_entry_t entry;

    // Wait for queued audit entries, then persist both the log and current count.
    while (1) {
        if (xQueueReceive(xQueueLogStorage, &entry, portMAX_DELAY) == pdTRUE) {
            log_access_to_sd(entry.formatted_log);
            update_sd_occupancy_snapshot(g_building_occupancy_count);
        }
    }
}
