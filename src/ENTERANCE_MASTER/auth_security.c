/*
 * ====================================================================
 * CoCIS Smart System - Authentication & Security Management
 * Component: auth_security.c
 * Owner: Namuzibwa Laurinda C (24/U/1472) - Group SE-24
 * ====================================================================
 */

#include "auth_security.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config_defaults.h"

static const char *TAG = "AUTH_SECURITY";

static bool        s_is_logged_in        = false;
static char        s_active_user[32]     = "";
static TickType_t  s_last_activity_tick  = 0;

void auth_security_init(void) {
    s_is_logged_in = false;
    memset(s_active_user, 0, sizeof(s_active_user));
    s_last_activity_tick = 0;
    ESP_LOGI(TAG, "Authentication subsystem initialized (Timeout: %d seconds).", SESSION_TIMEOUT_SEC);
}

bool auth_login(const char *username, const char *password) {
    if (!username || !password) return false;

    // Hardened administrator credentials
    if (strcmp(username, "admin") == 0 && strcmp(password, "admin123") == 0) {
        s_is_logged_in = true;
        strncpy(s_active_user, username, sizeof(s_active_user) - 1);
        s_last_activity_tick = xTaskGetTickCount();
        ESP_LOGI(TAG, "Admin login successful for user '%s'. Session granted.", username);
        return true;
    }

    // Default staff login
    if (strcmp(username, "staff") == 0 && strcmp(password, "staff123") == 0) {
        s_is_logged_in = true;
        strncpy(s_active_user, username, sizeof(s_active_user) - 1);
        s_last_activity_tick = xTaskGetTickCount();
        ESP_LOGI(TAG, "Staff login successful for user '%s'.", username);
        return true;
    }

    ESP_LOGW(TAG, "Login failed: Invalid credentials for user '%s'.", username);
    return false;
}

void auth_logout(void) {
    if (s_is_logged_in) {
        ESP_LOGI(TAG, "User '%s' logged out.", s_active_user);
    }
    s_is_logged_in = false;
    memset(s_active_user, 0, sizeof(s_active_user));
    s_last_activity_tick = 0;
}

bool auth_is_authenticated(void) {
    if (!s_is_logged_in) return false;

    // Check session timeout
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - s_last_activity_tick;
    if (elapsed >= pdMS_TO_TICKS(SESSION_TIMEOUT_SEC * 1000)) {
        ESP_LOGW(TAG, "Session expired after %d seconds of inactivity. Logging out.", SESSION_TIMEOUT_SEC);
        auth_logout();
        return false;
    }

    return true;
}

void auth_refresh_activity(void) {
    if (s_is_logged_in) {
        s_last_activity_tick = xTaskGetTickCount();
    }
}

const char *auth_get_logged_in_user(void) {
    if (auth_is_authenticated()) {
        return s_active_user;
    }
    return "GUEST";
}

uint32_t auth_get_session_remaining_sec(void) {
    if (!s_is_logged_in) return 0;

    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed_ticks = now - s_last_activity_tick;
    uint32_t elapsed_sec = (uint32_t)(elapsed_ticks / configTICK_RATE_HZ);

    if (elapsed_sec >= SESSION_TIMEOUT_SEC) {
        return 0;
    }
    return (uint32_t)(SESSION_TIMEOUT_SEC - elapsed_sec);
}

