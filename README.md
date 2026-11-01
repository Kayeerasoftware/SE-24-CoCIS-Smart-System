# Smart Control and Monitoring System for CoCIS (Makerere University)

[![Platform](https://img.shields.io/badge/Platform-ESP--IDF%20%7C%20FreeRTOS-blue.svg)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![Target MCU](https://img.shields.io/badge/MCU-ESP32%20DevKit%20V1-red.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Simulation](https://img.shields.io/badge/Simulator-Wokwi%20(Free%20Tier)-green.svg)](https://wokwi.com/)
[![Academic Year](https://img.shields.io/badge/Academic%20Year-2026%2F2027-orange.svg)]()
[![Group](https://img.shields.io/badge/Group-SE--24-purple.svg)]()

> **Makerere University** — Department of Computer Science & Software Engineering  
> **Programmes:** BSE III & CS II  
> **Course Supervisor:** Dr. Mary Nsabagwa (`mnsabagwa@cit.ac.ug`)  
> **Architecture:** Single-MCU FreeRTOS Dual-Core Edge Controller  

---

## Table of Contents
1. [System Overview & Key Capabilities](#1-system-overview--key-capabilities)
2. [Hardware Pinout & Wiring Reference](#2-hardware-pinout--wiring-reference)
3. [Prerequisites & Environment Setup](#3-prerequisites--environment-setup)
4. [How to Build & Run the Simulation](#4-how-to-build--run-the-simulation)
5. [Real Studio Audio Companion (PC Speaker Voice)](#5-real-studio-audio-companion-pc-speaker-voice)
6. [Interactive Usage & Testing Guide](#6-interactive-usage--testing-guide)
   - [6.1 Access Control & Anti-Passback](#61-access-control--anti-passback)
   - [6.2 Smart Lighting & Daylight Harvesting](#62-smart-lighting--daylight-harvesting)
   - [6.3 Emergency Evacuation Override](#63-emergency-evacuation-override)
   - [6.4 Persistent Storage & Crash Recovery](#64-persistent-storage--crash-recovery)
   - [6.5 Serial Management CLI (UART0)](#65-serial-management-cli-uart0)
7. [Registered User Database & Credentials](#7-registered-user-database--credentials)
8. [Project File Structure](#8-project-file-structure)
9. [Group SE-24 Member Allocations](#9-group-se-24-member-allocations)

---

## 1. System Overview & Key Capabilities

The **CoCIS Smart Control & Monitoring System** integrates automated access control, occupancy tracking, and intelligent daylight-harvesting classroom energy management on a single **ESP32 Dual-Core microcontroller**:

- **Dual-Gate RFID Access Control:** Dedicated Entry (`rfid1`) and Exit (`rfid2`) MFRC522 SPI readers with SG90 servo barrier gates (0° closed $\rightarrow$ 90° open for 5 seconds), visual LED feedback, and 1000Hz acoustic buzzers.
- **Strict Anti-Passback Algorithm:** Prevents card sharing and tailgating by maintaining bidirectional presence states (`STATE_OUTSIDE` $\leftrightarrow$ `STATE_INSIDE`). Duplicate entries or exits are rejected and flagged.
- **3-Strike Intrusion Lockout:** 3 consecutive unknown/invalid card taps trigger a 60-second hardware lockout with acoustic alarms.
- **After-Hours Curfew Schedule:** Configurable building operating window (default: 07:00 to 20:00). Prohibits unauthorized entry and traps after-hours occupants to stop unmonitored exit.
- **Daylight-Harvesting Room Lighting:** Dual lecture rooms (Room 1 & Room 2) with independent PIR motion sensors and analog LDR sensors (ADC1). Classroom lighting relays (driving 8 LEDs) only activate if **Motion == TRUE AND Ambient Light < 250 Lux**.
- **30-Second Vacancy Auto-Shutoff:** Non-blocking countdown timer switches off lights after 30 seconds of no detected motion, accumulating real-time energy savings (kWh).
- **One-Touch Emergency Evacuation Override:** Physical red pushbutton (`GPIO 32`) immediately opens the Exit gate to 90°, locks the Entry gate at 0°, forces all classroom lights ON, and pulses dual sirens.
- **Persistent Storage & Power-Loss Crash Recovery:** MicroSD FATFS over shared VSPI bus logs forensic records to `/sdcard/access_log.csv` and atomically snapshots headcount to `/sdcard/occupancy.csv` to survive reboots.
- **Interactive Management CLI:** Role-Based Access Control (RBAC) over UART0 at 115,200 baud with admin authentication (`login admin se24`), simulated card taps (`tap in <tag>`), and live telemetry tables.
- **Real Studio Audio Companion:** Python bridge (`audio_companion.py`) listening over RFC2217 port 4000 to speak uncompressed voice clips (*"Access granted"* / *"Access denied"*) through PC speakers.

---

## 2. Hardware Pinout & Wiring Reference

Synchronized between [`diagram.json`](file:///c:/Users/ASUS/Desktop/SE-24-CoCIS-Smart-System/diagram.json) and [`include/project_config.h`](file:///c:/Users/ASUS/Desktop/SE-24-CoCIS-Smart-System/include/project_config.h):

| ESP32 Pin | Mode / Peripheral | Function / Subsystem | Description |
| :---: | :---: | :---: | :--- |
| **GPIO 0** | LEDC Timer 1 CH3 | Exit Buzzer (`buzzer2`) | 1000Hz clean acoustic alarm tone. |
| **GPIO 1** | UART0 TXD | Serial CLI TX | 115,200 baud console transmit to host PC. |
| **GPIO 2** | Digital Output | Entry Green LED | Access Granted visual indicator. Safe strapping pin. |
| **GPIO 3** | UART0 RXD | Serial CLI RX | 115,200 baud console receive from keyboard. |
| **GPIO 4** | Digital Output | Entry Red LED | Access Denied / Lockout visual indicator. |
| **GPIO 5** | SPI Chip Select | Exit RFID CS (`rfid2`) | Dedicated active-LOW CS for Exit MFRC522 reader. |
| **GPIO 12** | LEDC Timer 1 CH2 | Entry Buzzer (`buzzer1`) | 1000Hz clean acoustic alarm tone. |
| **GPIO 13** | LEDC Timer 0 CH0 | Entry Servo Motor | 50Hz PWM: 0° closed (500μs), 90° open (1450μs for 5s). |
| **GPIO 14** | Input (Pull-down) | Room 1 PIR Motion | Digital motion detection; resets 30s countdown. |
| **GPIO 15** | LEDC Timer 0 CH1 | Exit Servo Motor | 50Hz PWM: 0° closed (500μs), 90° open (1450μs for 5s). |
| **GPIO 16** | Digital Output | Exit Green LED | Access Granted visual indicator at exit corridor. |
| **GPIO 17** | Digital Output | Exit Red LED | Access Denied visual indicator at exit corridor. |
| **GPIO 18** | VSPI SCK | Shared SPI Clock | Clock line for Entry RFID, Exit RFID, and MicroSD card. |
| **GPIO 19** | VSPI MISO | Shared SPI MISO | Master In Slave Out line shared across all 3 SPI devices. |
| **GPIO 21** | SPI Chip Select | Entry RFID CS (`rfid1`) | Dedicated active-LOW CS for Entry MFRC522 reader. |
| **GPIO 22** | Digital Output | Shared RFID Reset | Hard reset line connected to both MFRC522 readers. |
| **GPIO 23** | VSPI MOSI | Shared SPI MOSI | Master Out Slave In line shared across all 3 SPI devices. |
| **GPIO 25** | Digital Output | Room 2 Relay (`relay2`) | Active-LOW PNP relay: LOW = ON (drives 4 blue LEDs). |
| **GPIO 26** | Digital Output | Room 1 Relay (`relay1`) | Active-LOW PNP relay: LOW = ON (drives 4 purple LEDs). |
| **GPIO 27** | SPI Chip Select | MicroSD Card CS (`sd1`)| Dedicated active-LOW CS for MicroSD FATFS file system. |
| **GPIO 32** | Input (Pull-up) | Emergency Pushbutton | Red button: debounced falling edge triggers evacuation. |
| **GPIO 33** | Input (Pull-down) | Room 2 PIR Motion | Digital motion detection; resets 30s countdown. |
| **GPIO 34** | Analog (ADC1_CH6)| Room 1 LDR Sensor | 12-bit SAR ADC: ambient lux measurement (0–1000 lux). |
| **GPIO 35** | Analog (ADC1_CH7)| Room 2 LDR Sensor | 12-bit SAR ADC: ambient lux measurement (0–1000 lux). |

---

## 3. Prerequisites & Environment Setup

Ensure the following tools are installed:

1. **VS Code** with the following extensions:
   - **PlatformIO IDE** (`platformio.platformio-ide`)
   - **Wokwi Simulator** (`42yeah.wokwi-vscode` or official Wokwi extension)
2. **Python 3.8+** (for running `audio_companion.py` with sound drivers)
3. **Optional Python Packages:**
   ```bash
   pip install soundfile
   ```
   *(Windows includes `winsound` natively; macOS/Linux fallback to `afplay`/`aplay`)*

---

## 4. How to Build & Run the Simulation

### Step 1: Open the Project in VS Code
Open the project root directory:
```bash
cd c:\Users\ASUS\Desktop\SE-24-CoCIS-Smart-System
code .
```

### Step 2: Build Firmware via PlatformIO
Use the PlatformIO CLI or the VS Code status bar **Build** (✓) button:
```bash
pio run
```
This compiles the ESP-IDF components and generates `.pio/build/esp32dev/firmware.bin` and `.elf`.

### Step 3: Start the Wokwi Simulation
1. Press `F1` or `Ctrl + Shift + P` in VS Code.
2. Select **`Wokwi: Start Simulator`**.
3. Alternatively, open [`diagram.json`](file:///c:/Users/ASUS/Desktop/SE-24-CoCIS-Smart-System/diagram.json) and click the **Play** button at the top.
4. The simulation boots, runs the hardware self-test (LEDs flash, buzzers chirp, servos calibrate to 0°), and starts all 7 FreeRTOS tasks.

---

## 5. Real Studio Audio Companion (PC Speaker Voice)

The system includes a Python bridge that connects to Wokwi's serial server and plays studio vocal announcements through your PC speakers:

### Starting the Audio Companion:
Open a second terminal window and run:
```bash
python audio_companion.py
```

- Automatically connects to Wokwi at `127.0.0.1:4000` (configured in [`wokwi.toml`](file:///c:/Users/ASUS/Desktop/SE-24-CoCIS-Smart-System/wokwi.toml)).
- Whenever access events trigger, your PC speakers immediately announce:
  - **Access Granted:** Crystal-clear female studio voice: *"Access granted"*.
  - **Access Denied:** Crystal-clear studio voice: *"Access denied"* + security alarm siren.
- **Standalone Audio Test:** Press `T` in the companion terminal to test PC sound output anytime.

---

## 6. Interactive Usage & Testing Guide

### 6.1 Access Control & Anti-Passback

#### Method A: Physical Simulation via Wokwi Components
1. Click the **Entry RFID reader (`rfid1`)**. Its default simulated UID is `01:02:03:04` (Kayeera Nathan, Admin).
2. **First Tap (Granted):**
   - Speaker says: *"Access granted"*.
   - Entry Green LED (`GPIO 2`) illuminates.
   - Entry Servo (`GPIO 13`) rotates from 0° (closed barrier) to 90° (open) for 5 seconds, then returns to 0°.
   - User state becomes `STATE_INSIDE` and building occupancy increments to 1.
3. **Immediate Re-Tap at Entry (Anti-Passback Rejection):**
   - Click `rfid1` again.
   - Speaker says: *"Access denied"*.
   - Entry Red LED (`GPIO 4`) illuminates; buzzer emits 3 warning beeps.
   - Console logs: `Anti-Passback Violation (Already Inside)`.
4. **Exit Tap (Granted):**
   - Click the **Exit RFID reader (`rfid2`)**.
   - Speaker says: *"Access granted"*.
   - Exit Green LED (`GPIO 16`) illuminates; Exit Servo (`GPIO 15`) swings open to 90° for 5 seconds.
   - User state becomes `STATE_OUTSIDE` and building occupancy decrements to 0.

#### Method B: Fast Simulation via CLI `tap` Command
In the serial terminal, type:
```text
CoCIS> tap in TAG_1001
CoCIS> tap in TAG_1001   <-- Rejects (Anti-passback)
CoCIS> tap out TAG_1001  <-- Grants exit
```

#### Testing 3-Strike Intrusion Lockout
In the terminal, tap an unregistered tag 3 times:
```text
CoCIS> tap in UNKNOWN_1
CoCIS> tap in UNKNOWN_2
CoCIS> tap in UNKNOWN_3
```
- On the 3rd attempt, terminal enters a **60-second hardware lockout lockdown**.
- All subsequent scans are denied until the 60-second penalty timer elapses.

---

### 6.2 Smart Lighting & Daylight Harvesting

#### Testing Daylight Harvesting Inhibit ($\ge 250$ Lux):
1. Locate **`ldr1` (Room 1 LDR)** in Wokwi. Drag the slider to the right (bright sunlight, e.g. 500–800 lux).
2. Click **`pir1` (Room 1 PIR)** to simulate human motion.
3. **Result:** The 4 purple LEDs **remain OFF**. The console confirms:
   ```text
   Room 1: Motion detected! Occupancy = YES
   [Ambient Lux >= 250 -> Daylight Inhibit Active, Relay OFF, Power Conserved]
   ```

#### Testing Low-Light Motion Activation ($< 250$ Lux):
1. Drag the **`ldr1` slider** to the left (darkness, e.g. 50–150 lux).
2. Click **`pir1`** to trigger motion.
3. **Result:** Relay 1 (`GPIO 26`) energizes (drives LOW) and **all 4 purple LEDs turn ON**.
4. The 30-second vacancy countdown begins. Each new motion click resets the timer to 30s.

#### Testing 30-Second Auto-Shutoff:
1. Stop clicking `pir1` and wait 30 seconds.
2. The countdown reaches 0: Relay 1 de-energizes and lights turn OFF.
3. Type `energy` in the CLI to view cumulative kWh energy conserved.

*(Room 2 operates identically with `pir2`, `ldr2`, Relay 2 on `GPIO 25`, and 4 blue LEDs).*

---

### 6.3 Emergency Evacuation Override

#### Triggering via Physical Pushbutton:
1. Click the **Red Emergency Button (`btn1` / `GPIO 32`)** in Wokwi.
2. **Immediate Evacuation Actions:**
   - **Exit Gate (`GPIO 15`):** Immediately opens to 90° and locks open. Exit Green LED stays ON.
   - **Entry Gate (`GPIO 13`):** Locked at 0° (barrier closed) to block inward entry.
   - **Room Lights:** Room 1 (`GPIO 26`) and Room 2 (`GPIO 25`) relays are forced ON (all 8 LEDs illuminate) for visibility.
   - **Sirens:** Both entry and exit buzzers sound in a 200ms ON / 150ms OFF emergency rhythm.
3. Click `btn1` again to reset the emergency state and resume automated operation.

#### Triggering via Serial CLI:
```text
CoCIS> login admin se24
CoCIS> emergency on     <-- Triggers evacuation override
CoCIS> emergency off    <-- Restores normal automated control
```

---

### 6.4 Persistent Storage & Crash Recovery

1. Tap an entry card (`tap in TAG_1001` and `tap in TAG_1002`). The building headcount is now `2`.
2. Inspect the audit log on the SD card:
   ```text
   CoCIS> login admin se24
   CoCIS> logs
   ```
3. **Simulate a Power Loss / Hardware Reset:**
   - In Wokwi, click the ESP32 chip's **Restart** button or type reset in console.
   - Observe the boot log in terminal:
     ```text
     STORAGE_MGR: Executing Startup Crash Recovery Evaluation...
     STORAGE_MGR: CRASH RECOVERY SUCCESS: Restored building occupancy count = 2
     ```
   - Type `occupancy` — the system has recovered the exact pre-crash headcount without data loss.

---

### 6.5 Serial Management CLI (UART0)

The serial console runs at **115,200 baud** over USB-UART0. Type `help` to see all commands:

```text
======================================================================
       CoCIS SMART CONTROL & MONITORING SYSTEM - MAKERERE UNIVERSITY   
       Group SE-24 (Single-MCU FreeRTOS Edge Controller)              
======================================================================
Type 'help' for available commands. Welcome, Dr. Mary Nsabagwa!

CoCIS> help

--- CoCIS Management CLI Command Reference ---
  help                         - Displays this reference list
  status                       - System health, uptime & RTOS memory
  occupancy                    - Current building headcount & presence
  rooms                        - Live telemetry for Lecture Rooms 1 & 2
  energy                       - Cumulative kWh energy saved
  schedule show                - View configured building access hours
  login <user> <pass>          - Admin authentication (e.g. login admin se24)
  logout                       - Relinquish admin session
  tap <in|out> <tag_id>        - Simulate card tap (e.g. tap in TAG_1001)

--- Admin Privileged Commands (Login Required) ---
  schedule set <SH:SM> <EH:EM> - Update access hours (e.g. schedule set 07:00 20:00)
  curfew <on|off>              - Enforce after-hours curfew lockdown
  emergency <on|off>           - Force global evacuation gates & lights override
  logs                         - View recent security audit logs
```

#### Admin Login & Timeout:
- Username: `admin` | Password: `se24`
- Command: `login admin se24`
- If no command is typed for **5 minutes (300 seconds)**, the admin session automatically expires.

---

## 7. Registered User Database & Credentials

The system stores 6 pre-registered individuals with both 4-byte hex UIDs and mnemonic `TAG_xxx` codes:

| UID Code | Mnemonic Alias | Full Name | Role | Simulated Card |
| :---: | :---: | :--- | :---: | :---: |
| `01020304` | `TAG_1001` | Kayeera Nathan | Admin | Wokwi Blue Card (Default) |
| `11223344` | `TAG_1002` | Kasozi David | Staff | Green Card |
| `55667788` | `TAG_1003` | Zziwa Mark | Student | Yellow Card |
| `C0FFEE99` | `TAG_1004` | Sseku Jovan | Student | Key Fob |
| `04112233` | `TAG_1005` | Namuzibwa Laurinda | Staff | NFC Tag |
| `AABBCCDD` | `TAG_1006` | Dr. Mary Nsabagwa | Staff / Supervisor | Red Card |

---

## 8. Project File Structure

```
c:\Users\ASUS\Desktop\SE-24-CoCIS-Smart-System\
│
├── diagram.json                 # Complete Wokwi simulation schematic (26 parts)
├── wokwi.toml                   # Wokwi simulator config (RFC2217 port 4000)
├── platformio.ini               # PlatformIO build settings & flags
├── CMakeLists.txt               # ESP-IDF top-level CMake project definition
├── audio_companion.py           # Real crystal-clear studio voice PC player
├── access_granted.wav           # Uncompressed studio voice: "Access granted"
├── access_denied.wav            # Uncompressed studio voice: "Access denied" + siren
├── README.md                    # System usage guide & documentation (this file)
│
├── src/
│   ├── main.c                   # System startup & FreeRTOS dual-core task scheduler
│   ├── access_entrance.c        # MFRC522 driver, anti-passback, lockout & servos
│   ├── room_lighting.c          # PIR, LDR ADC1, 250 lux daylighting & PNP relays
│   ├── emergency_mgr.c          # Emergency button debounce, evacuation & heartbeat
│   ├── storage_manager.c        # MicroSD SPI FATFS driver & crash recovery snapshot
│   ├── cli_console.c            # UART0 115200 baud command parser & RBAC
│   └── CMakeLists.txt           # ESP-IDF component registration
│
├── include/
│   ├── project_config.h         # Master pin assignments, timings & FreeRTOS handles
│   ├── access_entrance.h        # Access control API declarations
│   ├── room_lighting.h          # Room automation API declarations
│   ├── emergency_mgr.h          # Emergency manager API declarations
│   ├── storage_manager.h        # Storage & crash recovery API declarations
│   ├── cli_console.h            # CLI console API declarations
│   └── voice_samples.h          # Embedded 8kHz 8-bit mono PCM voice tables
│
└── Docs/
    ├── Coursework FOR embedded systems.pdf          # Official coursework brief
    ├── TEAM_EXECUTIVE_SUMMARY_AND_SPECIFICATION.md  # Master team brief & video checklists
    └── SYSTEM_ARCHITECTURE.md                      # Detailed technical architecture report
```

---

## 9. Group SE-24 Member Allocations

| Group Member | Reg No / Student No | Role | Primary Modules & Deliverables |
| :--- | :---: | :--- | :--- |
| **Kayeera Nathan** | `24/U/25913/EVE` | **Group Leader & System Architect** | `main.c`, `emergency_mgr.c`, Dual-Core Scheduling, Emergency Evacuation ISR, Master Integration. |
| **Kasozi David Paul** | `24/U/25905/EVE` | **Access Control Specialist** | `access_entrance.c`, MFRC522 SPI Driver, Dual Gate Servos, Anti-Passback Algorithm. |
| **Zziwa Mark Alex** | `24/U/11978/EVE` | **Smart Energy Management Lead** | `room_lighting.c`, PIR/LDR Interfacing, Daylight Harvesting, 30s Countdown, Energy Savings Meter. |
| **Sseku Jovan** | `24/U/11172/EVE` | **Persistent Storage Lead** | `storage_manager.c`, MicroSD FATFS Driver, Audit CSV Logging, Dynamic Crash Recovery Engine. |
| **Namuzibwa Laurinda** | `24/U/1472` | **Interface, Security & QA Lead** | `cli_console.c`, UART0 CLI Engine, Admin Authentication, Curfew Schedule, Intrusion Lockout. |

---

*Makerere University — College of Computing & Information Sciences (CoCIS) — Group SE-24 — 2026/2027*
