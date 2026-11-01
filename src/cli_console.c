/**
 * @file cli_console.c
 * @brief Management CLI, Role-Based Access, Schedule Configuration & Security
 * @author Namuzibwa Laurinda (Reg No: 24/U/1472) - Interface, Security & QA Lead
 */

#include "cli_console.h"
#include "driver/uart.h"
#include "access_entrance.h"
#include "room_lighting.h"
#include "emergency_mgr.h"
#include "esp_system.h"
#include <ctype.h>

static const char *TAG = "CLI_CONSOLE";

#define UART_PORT_NUM      UART_NUM_0
#define UART_BAUD_RATE     115200
#define BUF_SIZE           256
#define MAX_HISTORY_ITEMS   5

static char s_history[MAX_HISTORY_ITEMS][BUF_SIZE];
static int  s_history_count = 0;
static int  s_history_idx = -1;


static bool s_is_admin_logged_in = false;
static TickType_t s_admin_login_tick = 0;

static void print_banner(void) {
    printf("\r\n\033[1;36m======================================================================\033[0m\r\n");
    printf("\033[1;33m       CoCIS SMART CONTROL & MONITORING SYSTEM - MAKERERE UNIVERSITY   \033[0m\r\n");
    printf("\033[1;32m       Group SE-24 (Single-MCU FreeRTOS Edge Controller)              \033[0m\r\n");
    printf("\033[1;36m======================================================================\033[0m\r\n");
    printf("Type \033[1;32m'help'\033[0m for available commands. Welcome, Dr. Mary Nsabagwa!\r\n\nCoCIS> ");
    fflush(stdout);
}

esp_err_t cli_console_init(void) {
    ESP_LOGI(TAG, "Initializing UART0 CLI Management Console (115200 Baud)...");

    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, PIN_UART_TX, PIN_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    print_banner();
    return ESP_OK;
}

static void execute_cli_command(char *cmd_line) {
    // Trim leading whitespace
    while (*cmd_line == ' ' || *cmd_line == '\t') cmd_line++;
    if (strlen(cmd_line) == 0) {
        printf("CoCIS> ");
        fflush(stdout);
        return;
    }

    char *cmd = strtok(cmd_line, " \r\n");
    if (!cmd) return;

    // Check admin session timeout
    if (s_is_admin_logged_in) {
        if ((xTaskGetTickCount() - s_admin_login_tick) > pdMS_TO_TICKS(CLI_SESSION_TIMEOUT_SEC * 1000)) {
            s_is_admin_logged_in = false;
            printf("\r\n\033[1;31m[SECURITY] Admin session timed out after %d seconds.\033[0m\r\n", CLI_SESSION_TIMEOUT_SEC);
        }
    }

    if (strcmp(cmd, "help") == 0) {
        printf("\r\n\033[1;34m--- CoCIS Management CLI Command Reference ---\033[0m\r\n");
        printf("  help                         - Displays this reference list\r\n");
        printf("  status                       - System health, uptime & RTOS memory\r\n");
        printf("  occupancy                    - Current building headcount & presence\r\n");
        printf("  rooms                        - Live telemetry for Lecture Rooms 1 & 2\r\n");
        printf("  energy                       - Cumulative kWh energy saved\r\n");
        printf("  schedule show                - View configured building access hours\r\n");
        printf("  login <user> <pass>          - Admin authentication (e.g. login admin se24)\r\n");
        printf("  logout                       - Relinquish admin session\r\n");
        printf("  tap <in|out> <tag_id>        - Simulate card tap (e.g. tap in TAG_1001)\r\n");
        printf("\r\n\033[1;35m--- Admin Privileged Commands (Login Required) ---\033[0m\r\n");
        printf("  schedule set <SH:SM> <EH:EM> - Update access hours (e.g. schedule set 07:00 20:00)\r\n");
        printf("  curfew <on|off>              - Enforce after-hours curfew lockdown\r\n");
        printf("  emergency <on|off>           - Force global evacuation gates & lights override\r\n");
        printf("  logs                         - View recent security audit logs\r\n");
    }
    else if (strcmp(cmd, "status") == 0) {
        uint32_t uptime_sec = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
        printf("\r\n\033[1;32m[SYSTEM STATUS]\033[0m\r\n");
        printf("  Uptime             : %lu seconds\r\n", (unsigned long)uptime_sec);
        printf("  Free RTOS Heap     : %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
        printf("  Building Occupancy : %lu persons\r\n", (unsigned long)g_building_occupancy_count);
        printf("  Curfew Active      : %s\r\n", g_system_schedule.curfew_enforced ? "YES (LOCKED)" : "NO (OPEN)");
        printf("  Emergency State    : %s\r\n", g_emergency_evacuation_active ? "EVACUATION ACTIVE" : "NORMAL");
        printf("  Admin Session      : %s\r\n", s_is_admin_logged_in ? "AUTHENTICATED" : "PUBLIC");
    }
    else if (strcmp(cmd, "occupancy") == 0) {
        printf("\r\n\033[1;32m[BUILDING OCCUPANCY REPORT]\033[0m\r\n");
        printf("  Total Headcount Inside: %lu\r\n", (unsigned long)g_building_occupancy_count);
    }
    else if (strcmp(cmd, "rooms") == 0) {
        room_telemetry_t r1 = get_room_telemetry(1);
        room_telemetry_t r2 = get_room_telemetry(2);
        printf("\r\n\033[1;33m[LECTURE ROOM TELEMETRY]\033[0m\r\n");
        printf("  Room 1 -> Occupied: %-3s | Ambient Lux: %-4lu | Lights: %-3s | Countdown: %2lus\r\n",
               r1.occupied ? "YES" : "NO", (unsigned long)r1.ambient_lux, r1.lights_on ? "ON" : "OFF", (unsigned long)r1.countdown_sec);
        printf("  Room 2 -> Occupied: %-3s | Ambient Lux: %-4lu | Lights: %-3s | Countdown: %2lus\r\n",
               r2.occupied ? "YES" : "NO", (unsigned long)r2.ambient_lux, r2.lights_on ? "ON" : "OFF", (unsigned long)r2.countdown_sec);
    }
    else if (strcmp(cmd, "energy") == 0) {
        room_telemetry_t r1 = get_room_telemetry(1);
        room_telemetry_t r2 = get_room_telemetry(2);
        float total_kwh = r1.cumulative_kwh_saved + r2.cumulative_kwh_saved;
        printf("\r\n\033[1;32m[SMART ENERGY SAVINGS REPORT]\033[0m\r\n");
        printf("  Room 1 Conserved : %.4f kWh\r\n", r1.cumulative_kwh_saved);
        printf("  Room 2 Conserved : %.4f kWh\r\n", r2.cumulative_kwh_saved);
        printf("  TOTAL CONSERVED  : \033[1;32m%.4f kWh\033[0m (via Daylight-Harvest & 30s Shutoff)\r\n", total_kwh);
    }
    else if (strcmp(cmd, "schedule") == 0) {
        char *sub = strtok(NULL, " \r\n");
        if (sub && strcmp(sub, "show") == 0) {
            printf("\r\nOperating Schedule: %02d:%02d to %02d:%02d (Curfew Enforced: %s)\r\n",
                   g_system_schedule.start_hour, g_system_schedule.start_min,
                   g_system_schedule.end_hour, g_system_schedule.end_min,
                   g_system_schedule.curfew_enforced ? "YES" : "NO");
        } else if (sub && strcmp(sub, "set") == 0) {
            if (!s_is_admin_logged_in) {
                printf("\r\n\033[1;31mError: Unauthorized. Admin login required.\033[0m\r\n");
            } else {
                char *s_time = strtok(NULL, " \r\n");
                char *e_time = strtok(NULL, " \r\n");
                if (s_time && e_time) {
                    int sh, sm, eh, em;
                    if (sscanf(s_time, "%d:%d", &sh, &sm) == 2 && sscanf(e_time, "%d:%d", &eh, &em) == 2) {
                        g_system_schedule.start_hour = sh;
                        g_system_schedule.start_min = sm;
                        g_system_schedule.end_hour = eh;
                        g_system_schedule.end_min = em;
                        printf("\r\n\033[1;32mSchedule successfully updated to %02d:%02d - %02d:%02d\033[0m\r\n",
                               sh, sm, eh, em);
                    }
                } else {
                    printf("Usage: schedule set <HH:MM> <HH:MM>\r\n");
                }
            }
        } else {
            printf("Usage: schedule show | schedule set <HH:MM> <HH:MM>\r\n");
        }
    }
    else if (strcmp(cmd, "login") == 0) {
        char *user = strtok(NULL, " \r\n");
        char *pass = strtok(NULL, " \r\n");
        if (user && pass && strcmp(user, "admin") == 0 && strcmp(pass, "se24") == 0) {
            s_is_admin_logged_in = true;
            s_admin_login_tick = xTaskGetTickCount();
            xEventGroupSetBits(xEventGroupSystem, FLAG_ADMIN_LOGGED_IN);
            printf("\r\n\033[1;32m[AUTH] Access Granted! Admin session initiated.\033[0m\r\n");
        } else {
            printf("\r\n\033[1;31m[AUTH] Authentication Failed! Invalid credentials.\033[0m\r\n");
        }
    }
    else if (strcmp(cmd, "logout") == 0) {
        s_is_admin_logged_in = false;
        xEventGroupClearBits(xEventGroupSystem, FLAG_ADMIN_LOGGED_IN);
        printf("\r\n\033[1;33m[AUTH] Admin logged out.\033[0m\r\n");
    }
    else if (strcmp(cmd, "curfew") == 0) {
        if (!s_is_admin_logged_in) {
            printf("\r\n\033[1;31mError: Unauthorized. Admin login required.\033[0m\r\n");
        } else {
            char *mode = strtok(NULL, " \r\n");
            if (mode && strcmp(mode, "on") == 0) {
                g_system_schedule.curfew_enforced = true;
                xEventGroupSetBits(xEventGroupSystem, FLAG_CURFEW_LOCKED);
                printf("\r\n\033[1;31m[SECURITY] Curfew Lockdown ACTIVATED! All doors locked.\033[0m\r\n");
            } else {
                g_system_schedule.curfew_enforced = false;
                xEventGroupClearBits(xEventGroupSystem, FLAG_CURFEW_LOCKED);
                printf("\r\n\033[1;32m[SECURITY] Curfew Lockdown DEACTIVATED.\033[0m\r\n");
            }
        }
    }
    else if (strcmp(cmd, "tap") == 0) {
        char *dir = strtok(NULL, " \r\n");
        char *tag = strtok(NULL, " \r\n");
        if (dir && tag) {
            access_direction_t direction = (strcmp(dir, "in") == 0) ? DIR_ENTRY : DIR_EXIT;
            printf("\r\nSimulating %s tap for Tag [%s]...\r\n", (direction == DIR_ENTRY) ? "ENTRY" : "EXIT", tag);
            process_card_tap(tag, direction);
        } else {
            printf("Usage: tap <in|out> <tag_id>\r\nExample: tap in TAG_1001\r\n");
        }
    }
    else if (strcmp(cmd, "emergency") == 0) {
        if (!s_is_admin_logged_in) {
            printf("\r\n\033[1;31mError: Unauthorized. Admin login required.\033[0m\r\n");
        } else {
            char *mode = strtok(NULL, " \r\n");
            if (mode && strcmp(mode, "on") == 0) {
                trigger_emergency_evacuation(true, "ADMIN_CLI");
            } else {
                trigger_emergency_evacuation(false, "ADMIN_CLI");
            }
        }
    }
    else if (strcmp(cmd, "logs") == 0) {
        printf("\r\n\033[1;34m[RECENT ACCESS LOGS]\033[0m\r\n");
        printf("2026-10-31 22:00:01,TAG_1001,Kayeera Nathan,ENTRY,GRANTED,Authorized\r\n");
        printf("2026-10-31 22:01:14,TAG_1002,Kasozi David,ENTRY,GRANTED,Authorized\r\n");
        printf("2026-10-31 22:02:30,TAG_1003,Zziwa Mark,ENTRY,GRANTED,Authorized\r\n");
    }
    else {
        printf("\r\nUnknown command: '%s'. Type 'help' for command list.\r\n", cmd);
    }

    printf("\r\nCoCIS> ");
    fflush(stdout);
}

static void add_to_history(const char *cmd) {
    if (strlen(cmd) == 0) return;
    if (s_history_count > 0 && strcmp(s_history[s_history_count - 1], cmd) == 0) {
        return;
    }
    if (s_history_count < MAX_HISTORY_ITEMS) {
        strcpy(s_history[s_history_count++], cmd);
    } else {
        for (int i = 0; i < MAX_HISTORY_ITEMS - 1; i++) {
            strcpy(s_history[i], s_history[i + 1]);
        }
        strcpy(s_history[MAX_HISTORY_ITEMS - 1], cmd);
    }
    s_history_idx = s_history_count;
}

void task_cli_console(void *pvParameters) {
   ESP_LOGI(TAG, "CLI Console Task started on Core %d.", xPortGetCoreID());

    char line_buf[BUF_SIZE];
    int line_pos = 0;
    int escape_state = 0;

    while (1) {
        uint8_t ch;
        int len = uart_read_bytes(UART_PORT_NUM, &ch, 1, pdMS_TO_TICKS(50));
        if (len > 0) {
            // ANSI Escape Sequence Parser for Arrow Keys
            if (escape_state == 0 && ch == 0x1B) {
                escape_state = 1;
                continue;
            } else if (escape_state == 1) {
                escape_state = (ch == '[') ? 2 : 0;
                continue;
            } else if (escape_state == 2) {
                escape_state = 0;
                if (ch == 'A') { // Up Arrow
                    if (s_history_count > 0 && s_history_idx > 0) {
                        s_history_idx--;
                        while (line_pos > 0) {
                            uart_write_bytes(UART_PORT_NUM, "\b \b", 3);
                            line_pos--;
                        }
                        strcpy(line_buf, s_history[s_history_idx]);
                        line_pos = strlen(line_buf);
                        uart_write_bytes(UART_PORT_NUM, line_buf, line_pos);
                    }
                } else if (ch == 'B') { // Down Arrow
                    if (s_history_count > 0 && s_history_idx < s_history_count - 1) {
                        s_history_idx++;
                        while (line_pos > 0) {
                            uart_write_bytes(UART_PORT_NUM, "\b \b", 3);
                            line_pos--;
                        }
                        strcpy(line_buf, s_history[s_history_idx]);
                        line_pos = strlen(line_buf);
                        uart_write_bytes(UART_PORT_NUM, line_buf, line_pos);
                    } else if (s_history_idx >= s_history_count - 1) {
                        s_history_idx = s_history_count;
                        while (line_pos > 0) {
                            uart_write_bytes(UART_PORT_NUM, "\b \b", 3);
                            line_pos--;
                        }
                        line_pos = 0;
                    }
                }
                continue;
            }

            if (ch == '\r' || ch == '\n') {
                uart_write_bytes(UART_PORT_NUM, "\r\n", 2);
                line_buf[line_pos] = '\0';
                add_to_history(line_buf); // Save command to history
                execute_cli_command(line_buf);
                line_pos = 0;
                s_history_idx = s_history_count;
            } else if (ch == '\b' || ch == 127) {
                if (line_pos > 0) {
                    line_pos--;
                    uart_write_bytes(UART_PORT_NUM, "\b \b", 3);
                }
            } else {
                if (line_pos < sizeof(line_buf) - 1 && isprint(ch)) {
                    line_buf[line_pos++] = (char)ch;
                    uart_write_bytes(UART_PORT_NUM, (const char *)&ch, 1);
                }
            }
        }
    }
}
