#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

// ==============================================================================
// Operational Schedule Defaults (Coursework Section 2.6)
// ==============================================================================
#define DEFAULT_SCHEDULE_START_HOUR   7
#define DEFAULT_SCHEDULE_START_MIN    0
#define DEFAULT_SCHEDULE_END_HOUR     20
#define DEFAULT_SCHEDULE_END_MIN      0

// ==============================================================================
// Security, Lockout & Timeout Thresholds (Coursework Section 2.5 & 2.7)
// ==============================================================================
#define MAX_FAILED_ATTEMPTS           3      // 3 invalid attempts trigger lockout
#define LOCKOUT_DURATION_SEC          60     // Intrusion lockout duration (seconds)
#define SESSION_TIMEOUT_SEC           300    // Admin CLI session inactivity timeout
#define NO_MOTION_TIMEOUT_SEC         30     // PIR motion vacancy delay
#define HEARTBEAT_INTERVAL_SEC        2      // ESP-NOW telemetry dispatch interval
#define NODE_OFFLINE_TIMEOUT_SEC      10     // Mark room node OFFLINE if no packet in 10s
#define DOOR_UNLOCK_DURATION_SEC      5      // Servo door unlock dwell time
#define DAYLIGHT_LUX_THRESHOLD        300    // Lux threshold for daylight-aware lighting

// ==============================================================================
// Hardware Pin Allocations - ENTERANCE_MASTER (ESP32 #1)
// Frozen Interface Contract Section 2
// ==============================================================================
#define PIN_SERVO_ENTRY               13     // PWM Servo: 0 deg = Closed, 90 deg = Open
#define PIN_SERVO_EXIT                15     // PWM Servo: 0 deg = Closed, 90 deg = Open
#define PIN_LED_GREEN                 2      // Access Granted LED (Active High)
#define PIN_LED_RED                   4      // Access Denied / Lockout LED (Active High)
#define PIN_BUZZER                    12     // Alarm Buzzer (Active High)
#define PIN_EMERGENCY_BTN             32     // Physical Emergency Pushbutton (Pull-up, Active Low)

// MicroSD Card SPI Bus Pins (VSPI)
#define PIN_SD_CS                     27     // SD Card Chip Select
#define PIN_SPI_MOSI                  23     // Shared SPI MOSI
#define PIN_SPI_MISO                  19     // Shared SPI MISO
#define PIN_SPI_SCK                   18     // Shared SPI SCK

// Dual RC522 RFID SPI Pins
#define PIN_RFID_ENTRY_CS             21     // RFID Entry Reader Chip Select (SDA)
#define PIN_RFID_EXIT_CS              5      // RFID Exit Reader Chip Select (SDA)
#define PIN_RFID_RST                  22     // RFID Reader Reset

// Serial Console UART0
#define UART_CONSOLE_TX               1
#define UART_CONSOLE_RX               3
#define UART_CONSOLE_BAUD             115200

// ==============================================================================
// Hardware Pin Allocations - ROOM_NODE_1 & ROOM_NODE_2 (ESP32 #2 & #3)
// Frozen Interface Contract Section 2
// ==============================================================================
#define PIN_PIR_MOTION                14     // Digital Input (Interrupt / Polled Debounce)
#define PIN_LDR_ADC                   34     // Analog Input (ADC1 Channel 6)
#define PIN_LIGHT_RELAY               12     // Digital Output (Relay: 1 = ON, 0 = OFF)
#define PIN_STATUS_LED                2      // Status LED (Blinks on wireless TX)

#endif // CONFIG_DEFAULTS_H
