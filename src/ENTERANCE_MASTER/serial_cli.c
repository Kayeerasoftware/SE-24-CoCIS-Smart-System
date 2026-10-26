/*
 * ====================================================================
 * CoCIS Smart System - Administrative Serial CLI Console
 * Component: serial_cli.c
 * Owner: Namuzibwa Laurinda C (24/U/1472) - Group SE-24
 * ====================================================================
 */

#include "serial_cli.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "auth_security.h"
#include "schedule_enforcer.h"
#include "anti_passback.h"
#include "sd_storage.h"
#include "esp_now_gateway.h"
#include "rfid_manager.h"
#include "door_actuator.h"
#include "config_defaults.h"

static const char *TAG = "SERIAL_CLI";

#define CLI_UART_NUM        UART_NUM_0
#define CLI_BUFFER_SIZE     256

static void print_banner(void) {
    printf("\n");
    printf("====================================================================\n");
    printf("      MAKERERE UNIVERSITY | CoCIS SMART SYSTEM GATEWAY (SE-24)      \n");
    printf("        Embedded Systems Coursework - Academic Year 2026/2027       \n");
    printf("====================================================================\n");
    printf("  Type 'help' to view available commands.\n");
    printf("  Default Admin Login: 'login admin admin123'\n");
    printf("--------------------------------------------------------------------\n\n");
}

static void print_prompt(void) {
    if (auth_is_authenticated()) {
        printf("CoCIS-Master (%s)# ", auth_get_logged_in_user());
    } else {
        printf("CoCIS-Master> ");
    }
    fflush(stdout);
}

static void cmd_help(void) {
    printf("\n=== CoCIS SYSTEM MANAGEMENT CLI COMMANDS ===\n");
    printf("  PUBLIC COMMANDS:\n");
    printf("    help                     - Displays this command reference list\n");
    printf("    status                   - System overview, clock, schedule & room status\n");
    printf("    occupancy                - Lists all occupants currently inside building\n");
    printf("    rooms                    - Live wireless telemetry from Room 1 & 2\n");
    printf("    lights                   - Detailed lighting, lux readings & relay states\n");
    printf("    schedule show            - Displays building operating hours & current clock\n");
    printf("    login <user> <pass>      - Authenticates admin or staff management session\n");
    printf("    logout                   - Terminates authenticated session\n");
    printf("    swipe <entry|exit> <tag> - Simulates an RFID card swipe (e.g. swipe entry TAG_1001)\n");
    printf("    clock <HH:MM>            - Sets simulated clock (e.g. clock 21:05 to test curfew)\n");
    printf("\n  ADMINISTRATOR COMMANDS (Requires 'login admin admin123'):\n");
    printf("    schedule set <S_H:S_M> <E_H:E_M> - Updates building access hours\n");
    printf("    logs [count]             - Dumps recent access audit logs from MicroSD\n");
    printf("    adduser <tag> <name> <role>      - Registers new card in users.csv\n");
    printf("    deluser <tag>            - Deactivates user card in users.csv\n");
    printf("    energy                   - Displays energy savings analytics\n");
    printf("    emergency <on|off>       - Broadcasts campus emergency evacuation alert\n");
    printf("    override <room> <auto|on|off>    - Controls room light overrides (room: 0=all, 1, 2)\n");
    printf("============================================\n\n");
}

static void cmd_status(void) {
    char time_str[32];
    schedule_get_timestamp_str(time_str, sizeof(time_str));

    uint8_t sh, sm, eh, em;
    schedule_get_hours(&sh, &sm, &eh, &em);

    room_telemetry_t r1 = {0}, r2 = {0};
    esp_now_gateway_get_room_telemetry(1, &r1);
    esp_now_gateway_get_room_telemetry(2, &r2);

    printf("\n+--------------------------------------------------------------+\n");
    printf("|                  CoCIS MASTER SYSTEM STATUS                  |\n");
    printf("+--------------------------------------------------------------+\n");
    printf("| System Clock       : %-39s |\n", time_str);
    printf("| Operating Hours    : %02d:%02d to %02d:%02d                          |\n", sh, sm, eh, em);
    printf("| Building Occupants : %-39d |\n", anti_passback_get_count());
    printf("| MicroSD Storage    : %-39s |\n", sd_storage_is_mounted() ? "MOUNTED (FATFS /sdcard)" : "VIRTUAL RAM FALLBACK");
    printf("| Intrusion Lockout  : %-39s |\n", anti_passback_is_locked_out() ? "ACTIVE (ALARM ON)" : "NORMAL");
    printf("| Wireless Room 1    : %-39s |\n", r1.is_online ? "ONLINE (2.4 GHz ESP-NOW)" : "OFFLINE (No Heartbeat)");
    printf("| Wireless Room 2    : %-39s |\n", r2.is_online ? "ONLINE (2.4 GHz ESP-NOW)" : "OFFLINE (No Heartbeat)");
    printf("| Active Session     : %-39s |\n", auth_is_authenticated() ? auth_get_logged_in_user() : "None (Public Mode)");
    printf("+--------------------------------------------------------------+\n\n");
}

static void cmd_occupancy(void) {
    int count = anti_passback_get_count();
    printf("\n--- CURRENT BUILDING OCCUPANTS (%d Total) ---\n", count);
    if (count == 0) {
        printf("  Building is currently vacant. No registered occupants inside.\n");
    } else {
        printf("  %-12s | %-24s | %-20s\n", "TAG ID", "NAME", "ENTRY TIMESTAMP");
        printf("  -------------+--------------------------+---------------------\n");
        for (int i = 0; i < count; i++) {
            char tag[16], name[32], ts[32];
            if (anti_passback_get_occupant(i, tag, name, ts)) {
                printf("  %-12s | %-24s | %-20s\n", tag, name, ts);
            }
        }
    }
    printf("---------------------------------------------\n\n");
}

static void cmd_rooms(void) {
    room_telemetry_t r1 = {0}, r2 = {0};
    esp_now_gateway_get_room_telemetry(1, &r1);
    esp_now_gateway_get_room_telemetry(2, &r2);

    printf("\n--- LIVE WIRELESS ROOM TELEMETRY (ESP-NOW) ---\n");
    printf("  Room 1 (Lecture Room 1 - MAC: 24:0A:C4:00:02:00):\n");
    printf("    Status         : %s\n", r1.is_online ? "ONLINE" : "OFFLINE");
    printf("    Occupancy      : %s\n", r1.occupied ? "OCCUPIED" : "VACANT");
    printf("    Relay Lighting : %s\n", r1.lights_on ? "ON" : "OFF");
    printf("    Ambient Light  : %d lux\n", r1.light_level_lux);
    printf("    Energy Saved   : %lu seconds\n", (unsigned long)r1.energy_saved_sec);

    printf("\n  Room 2 (Lecture Room 2 - MAC: 24:0A:C4:00:03:00):\n");
    printf("    Status         : %s\n", r2.is_online ? "ONLINE" : "OFFLINE");
    printf("    Occupancy      : %s\n", r2.occupied ? "OCCUPIED" : "VACANT");
    printf("    Relay Lighting : %s\n", r2.lights_on ? "ON" : "OFF");
    printf("    Ambient Light  : %d lux\n", r2.light_level_lux);
    printf("    Energy Saved   : %lu seconds\n", (unsigned long)r2.energy_saved_sec);
    printf("----------------------------------------------\n\n");
}

static void cmd_lights(void) {
    room_telemetry_t r1 = {0}, r2 = {0};
    esp_now_gateway_get_room_telemetry(1, &r1);
    esp_now_gateway_get_room_telemetry(2, &r2);

    const char *mode_str[] = {"AUTO (Daylight-Aware)", "FORCE OFF", "FORCE ON"};

    printf("\n--- LIGHTING & DAYLIGHT ILLUMINATION STATUS ---\n");
    printf("  Threshold : %d lux (Daylight saves energy above this level)\n", DAYLIGHT_LUX_THRESHOLD);
    printf("  Room 1    : Relay=%s | Lux=%d | Control Mode: %s\n",
           r1.lights_on ? "ON" : "OFF", r1.light_level_lux, mode_str[r1.override_mode % 3]);
    printf("  Room 2    : Relay=%s | Lux=%d | Control Mode: %s\n",
           r2.lights_on ? "ON" : "OFF", r2.light_level_lux, mode_str[r2.override_mode % 3]);
    printf("-----------------------------------------------\n\n");
}

static void cmd_energy(void) {
    if (!auth_is_authenticated()) {
        printf("Error: Permission denied. Please login as admin first.\n");
        return;
    }
    room_telemetry_t r1 = {0}, r2 = {0};
    esp_now_gateway_get_room_telemetry(1, &r1);
    esp_now_gateway_get_room_telemetry(2, &r2);

    uint32_t total_sec = r1.energy_saved_sec + r2.energy_saved_sec;
    // Assuming 500W lighting fixture per lecture room: 0.5 kW * (seconds / 3600)
    double kwh_saved = (double)total_sec * 0.5 / 3600.0;

    printf("\n=== CAMPUS ENERGY CONSERVATION ANALYTICS ===\n");
    printf("  Room 1 Energy Off Time : %lu seconds (%.2f hours)\n",
           (unsigned long)r1.energy_saved_sec, (double)r1.energy_saved_sec / 3600.0);
    printf("  Room 2 Energy Off Time : %lu seconds (%.2f hours)\n",
           (unsigned long)r2.energy_saved_sec, (double)r2.energy_saved_sec / 3600.0);
    printf("  Combined Energy Saved  : %lu seconds\n", (unsigned long)total_sec);
    printf("  Calculated Conservation: %.4f kWh (at 500W fixture load)\n", kwh_saved);
    printf("============================================\n\n");
}

static void parse_command(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return;

    auth_refresh_activity();

    char cmd[32] = {0};
    char arg1[64] = {0};
    char arg2[64] = {0};
    char arg3[64] = {0};

    int n = sscanf(line, "%31s %63s %63s %63s", cmd, arg1, arg2, arg3);

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "status") == 0) {
        cmd_status();
    } else if (strcmp(cmd, "occupancy") == 0) {
        cmd_occupancy();
    } else if (strcmp(cmd, "rooms") == 0) {
        cmd_rooms();
    } else if (strcmp(cmd, "lights") == 0) {
        cmd_lights();
    } else if (strcmp(cmd, "login") == 0) {
        if (n < 3) {
            printf("Usage: login <username> <password>\n");
        } else {
            if (auth_login(arg1, arg2)) {
                printf("Access Granted: Welcome, %s.\n", arg1);
            } else {
                printf("Access Denied: Invalid credentials.\n");
            }
        }
    } else if (strcmp(cmd, "logout") == 0) {
        auth_logout();
        printf("Session terminated.\n");
    } else if (strcmp(cmd, "schedule") == 0) {
        if (strcmp(arg1, "show") == 0 || n == 1) {
            uint8_t sh, sm, eh, em;
            schedule_get_hours(&sh, &sm, &eh, &em);
            char time_str[32];
            schedule_get_timestamp_str(time_str, sizeof(time_str));
            printf("Building Hours : %02d:%02d to %02d:%02d\n", sh, sm, eh, em);
            printf("Current Clock  : %s\n", time_str);
        } else if (strcmp(arg1, "set") == 0) {
            if (!auth_is_authenticated()) {
                printf("Error: Admin login required to change schedule.\n");
                return;
            }
            int sh = 0, sm = 0, eh = 0, em = 0;
            if (sscanf(arg2, "%d:%d", &sh, &sm) == 2 && sscanf(arg3, "%d:%d", &eh, &em) == 2) {
                schedule_set_hours(sh, sm, eh, em);
                sd_save_configuration(sh, sm, eh, em);
                printf("Schedule updated to %02d:%02d - %02d:%02d and saved to /sdcard/config.txt.\n",
                       sh, sm, eh, em);
            } else {
                printf("Usage: schedule set <HH:MM> <HH:MM> (e.g. schedule set 07:00 20:00)\n");
            }
        } else {
            printf("Usage: schedule show | schedule set <HH:MM> <HH:MM>\n");
        }
    } else if (strcmp(cmd, "clock") == 0) {
        int h = 0, m = 0;
        if (n >= 2 && sscanf(arg1, "%d:%d", &h, &m) == 2) {
            schedule_set_clock(h, m);
            printf("Simulated clock set to %02d:%02d:00\n", h, m);
        } else {
            printf("Usage: clock <HH:MM> (e.g. clock 21:05 to simulate curfew)\n");
        }
    } else if (strcmp(cmd, "swipe") == 0) {
        if (n < 3) {
            printf("Usage: swipe <entry|exit> <tag_id> (e.g. swipe entry TAG_1001)\n");
        } else {
            door_direction_t dir = (strcmp(arg1, "exit") == 0) ? DOOR_EXIT : DOOR_ENTRY;
            rfid_simulate_swipe(dir, arg2);
        }
    } else if (strcmp(cmd, "logs") == 0) {
        if (!auth_is_authenticated()) {
            printf("Error: Admin login required to inspect audit logs.\n");
            return;
        }
        int count = (n >= 2) ? atoi(arg1) : 20;
        sd_dump_logs(count);
    } else if (strcmp(cmd, "adduser") == 0) {
        if (!auth_is_authenticated()) {
            printf("Error: Admin login required to register users.\n");
            return;
        }
        if (n < 4) {
            printf("Usage: adduser <tag_id> <name> <role> (e.g. adduser TAG_1006 'Dr. John' staff)\n");
        } else {
            if (sd_add_user(arg1, arg2, arg3)) {
                printf("User '%s' (%s, role: %s) registered in /sdcard/users.csv.\n", arg2, arg1, arg3);
            } else {
                printf("Failed to add user to database.\n");
            }
        }
    } else if (strcmp(cmd, "deluser") == 0) {
        if (!auth_is_authenticated()) {
            printf("Error: Admin login required to deactivate users.\n");
            return;
        }
        if (n < 2) {
            printf("Usage: deluser <tag_id> (e.g. deluser TAG_1003)\n");
        } else {
            if (sd_deactivate_user(arg1)) {
                printf("Tag '%s' deactivated in /sdcard/users.csv.\n", arg1);
            } else {
                printf("Failed to deactivate tag '%s'.\n", arg1);
            }
        }
    } else if (strcmp(cmd, "energy") == 0) {
        cmd_energy();
    } else if (strcmp(cmd, "emergency") == 0) {
        if (!auth_is_authenticated()) {
            printf("Error: Admin login required to trigger campus evacuation.\n");
            return;
        }
        if (strcmp(arg1, "on") == 0 || strcmp(arg1, "activate") == 0) {
            esp_now_gateway_broadcast_emergency(true);
            printf("[EMERGENCY] Evacuation alert broadcasted! Doors unlocked 90 deg, lights ON.\n");
        } else if (strcmp(arg1, "off") == 0 || strcmp(arg1, "reset") == 0) {
            esp_now_gateway_broadcast_emergency(false);
            printf("[EMERGENCY] Evacuation cleared. Regular operations restored.\n");
        } else {
            printf("Usage: emergency <on|off>\n");
        }
    } else if (strcmp(cmd, "override") == 0) {
        if (!auth_is_authenticated()) {
            printf("Error: Admin login required to override room lighting.\n");
            return;
        }
        if (n < 3) {
            printf("Usage: override <room: 0=all, 1, 2> <auto|on|off>\n");
        } else {
            uint8_t room = atoi(arg1);
            uint8_t mode = 0;
            if (strcmp(arg2, "off") == 0) mode = 1;
            else if (strcmp(arg2, "on") == 0) mode = 2;
            esp_now_gateway_send_light_override(room, mode);
            printf("Light override sent for Room %d -> Mode %s.\n", room, arg2);
        }
    } else {
        printf("Unknown command: '%s'. Type 'help' for command reference.\n", cmd);
    }
}

void serial_cli_init(void) {
    uart_config_t uart_config = {
        .baud_rate = UART_CONSOLE_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(CLI_UART_NUM, &uart_config);
    uart_driver_install(CLI_UART_NUM, CLI_BUFFER_SIZE * 2, 0, 0, NULL, 0);

    print_banner();
    print_prompt();
}

void serial_cli_task(void *pvParameters) {
    char rx_buffer[CLI_BUFFER_SIZE];
    int  buf_len = 0;

    while (1) {
        uint8_t ch = 0;
        int len = uart_read_bytes(CLI_UART_NUM, &ch, 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            if (ch == '\r' || ch == '\n') {
                if (buf_len > 0) {
                    rx_buffer[buf_len] = '\0';
                    printf("\n");
                    parse_command(rx_buffer);
                    buf_len = 0;
                } else {
                    printf("\n");
                }
                print_prompt();
            } else if (ch == '\b' || ch == 127) { // Backspace
                if (buf_len > 0) {
                    buf_len--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (buf_len < (CLI_BUFFER_SIZE - 1) && isprint(ch)) {
                rx_buffer[buf_len++] = (char)ch;
                putchar(ch);
                fflush(stdout);
            }
        }
    }
}

