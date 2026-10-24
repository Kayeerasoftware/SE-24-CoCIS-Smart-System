#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

// Schedule Defaults
#define DEFAULT_SCHEDULE_START_HOUR 7
#define DEFAULT_SCHEDULE_START_MIN  0
#define DEFAULT_SCHEDULE_END_HOUR   20
#define DEFAULT_SCHEDULE_END_MIN    0

// Security & Timeout Thresholds
#define MAX_FAILED_ATTEMPTS         3
#define LOCKOUT_DURATION_SEC        60
#define SESSION_TIMEOUT_SEC         300
#define NO_MOTION_TIMEOUT_SEC       30
#define HEARTBEAT_INTERVAL_SEC      2
#define NODE_OFFLINE_TIMEOUT_SEC    10

// Hardware Pin Definitions - ENTERANCE_MASTER
#define PIN_SERVO_ENTRY             13
#define PIN_SERVO_EXIT              15
#define PIN_LED_GREEN               2
#define PIN_LED_RED                 4
#define PIN_BUZZER                  12
#define PIN_EMERGENCY_BTN           32
#define PIN_SD_CS                   27

// Hardware Pin Definitions - ROOM NODES
#define PIN_PIR_MOTION              14
#define PIN_LDR_ADC                 34
#define PIN_LIGHT_RELAY             12
#define PIN_STATUS_LED              2

#endif // CONFIG_DEFAULTS_H
