# Slide Deck & Video Presentation Guide: Kayeera Nathan
## Smart Control and Monitoring System for CoCIS (Makerere University)
### Group Leader, System Architect & RTOS Concurrency Lead (Reg No: 24/U/25913/EVE)

---

## Presentation Metadata & Technical Overview
- **Speaker:** Kayeera Nathan
- **Registration Number:** `24/U/25913/EVE` | **Student Number:** `2400725913`
- **Degree Programme:** Bachelor of Science in Software Engineering (BSE III)
- **Project:** CoCIS Smart Control and Monitoring System (Makerere University)
- **Assigned Subsystems:** System Architecture, Dual-Core FreeRTOS Concurrency, Emergency Evacuation Controller, Master Integration
- **Target Video Duration:** 3 to 5 minutes (compulsory webcam face + voice recording)
- **Color Palette & Theme:** Professional Engineering Dark Theme (Navy `#0A192F`, Cyan `#64FFDA`, Slate White `#CCD6F6`, Alert Amber `#FF6B6B`)

---

```
====================================================================================================
                                      SLIDE 1: TITLE SLIDE
====================================================================================================
```

### [Slide 1: Visual Design & Layout]
```
+--------------------------------------------------------------------------------------------------+
|  MAKERERE UNIVERSITY | COLLEGE OF COMPUTING & INFORMATION SCIENCES (CoCIS)                       |
|  Department of Computer Science & Software Engineering | Academic Year 2026/2027                 |
|                                                                                                  |
|   ============================================================================================   |
|     SMART CONTROL & MONITORING SYSTEM FOR CoCIS                                                  |
|     Consolidated Single-MCU FreeRTOS Dual-Core Architecture                                      |
|   ============================================================================================   |
|                                                                                                  |
|   PRESENTER:                                                                                     |
|   Kayeera Nathan (Reg No: 24/U/25913/EVE | Student No: 2400725913)                               |
|   Role: Group Leader, System Architect & RTOS Concurrency Lead                                   |
|   Group Reference: Group SE-24 (BSE III & CS II)                                                  |
|   Course Supervisor: Dr. Mary Nsabagwa                                                           |
|                                                                                                  |
|   CORE OWNERSHIP:                                                                                |
|   [src/main.c]        - System Integration & FreeRTOS Dual-Core Scheduler                        |
|   [emergency_mgr.c]   - Emergency Evacuation ISR & Asymmetric Egress Routing                     |
|   [project_config.h]  - Master System Configuration & IPC Data Bus Definitions                   |
+--------------------------------------------------------------------------------------------------+
```

### [Slide 1: Presenter Spoken Script]
> *"Good day, Dr. Mary. My name is **Kayeera Nathan**, registration number **24/U/25913/EVE**, pursuing Bachelor of Science in Software Engineering Year III. I am the **Group Leader and System Architect** for Group SE-24.*  
> *In this presentation and technical demonstration, I will explain the overall system architecture, our dual-core FreeRTOS concurrency model, and demonstrate the life-safety emergency evacuation subsystem that I personally engineered."*

---

```
====================================================================================================
                        SLIDE 2: PROBLEM CONTEXT & ARCHITECTURAL RESTRUCTURING
====================================================================================================
```

### [Slide 2: Visual Design & Layout]
```
+--------------------------------------------------------------------------------------------------+
|  ARCHITECTURAL TRANSFORMATION: FROM 3-NODE RF TO SINGLE-MCU RTOS                                 |
+--------------------------------------------------------------------------------------------------+
|                                                                                                  |
|  1. THE PROBLEM:                                                                                 |
|     - CoCIS faces unauthorized access, card passback, and high electricity bills.               |
|     - Initial plan: 3 ESP32 boards communicating via wireless ESP-NOW frames.                    |
|     - Simulation constraint: Free Wokwi tier allows only ONE active microcontroller.             |
|                                                                                                  |
|  2. THE LECTURER'S CLARIFICATION (Dr. Mary Nsabagwa, Oct 29, 2026):                              |
|     "You can use one. What is required is that you use the free version. Let's use one."         |
|                                                                                                  |
|  3. THE DEFINITIVE ENGINEERING RESOLUTION: Dual-Layer Communication                              |
|     +----------------------------------------+----------------------------------------+          |
|     | LAYER 1: Inter-Process Comm (IPC)      | LAYER 2: Hardware Bus Arbitration      |          |
|     | - FreeRTOS Message Queues (FIFO)       | - Shared VSPI Bus (SCK:18, MISO:19,    |          |
|     | - 24-bit Event Groups (Atomic state)   |   MOSI:23) with Mutex Arbitration      |          |
|     | - Mutexes guarding shared state        | - 3 Slaves: RFID1 (21), RFID2 (5),     |          |
|     | - Microsecond deterministic latency    |   MicroSD Card (27)                    |          |
|     +----------------------------------------+----------------------------------------+          |
|                                                                                                  |
|  *Directly satisfies Coursework Section 2.7 (Resource Synchronization & System Reliability)*     |
+--------------------------------------------------------------------------------------------------+
```

### [Slide 2: Presenter Spoken Script]
> *"Following Dr. Mary's formal clarification permitting a single microcontroller for the Wokwi free tier, our team answered the critical question: **'Does communication still exist in a single-MCU architecture?'**  
> The answer is a definitive **YES**. In modern embedded engineering, such as automotive AUTOSAR or avionics, subsystems are partitioned across RTOS tasks rather than separate circuit boards.  
> We transitioned from inter-board radio frames to a **Dual-Layer Communication Model**:  
> First, **Inter-Process Communication** using FreeRTOS message queues, event groups, and semaphores for microsecond-level deterministic data exchange.  
> Second, **Hardware Bus Communication**, where the single ESP32 arbitrates three SPI peripherals—two RFID readers and a MicroSD card—over a shared high-speed VSPI bus."*

---

```
====================================================================================================
                     SLIDE 3: DUAL-CORE TASK PARTITIONING & HARDWARE AFFINITY
====================================================================================================
```

### [Slide 3: Visual Design & Layout]
```
+--------------------------------------------------------------------------------------------------+
|  DUAL-CORE RTOS TASK PARTITIONING (src/main.c)                                                   |
+--------------------------------------------------------------------------------------------------+
|                                                                                                  |
|  ESP32 XTENSA LX6 DUAL-CORE PROCESSOR (240 MHz)                                                  |
|                                                                                                  |
|  CORE 0: Real-Time Safety & Actuation Engine      CORE 1: Storage, Management & Maintenance      |
|  +--------------------------------------------+   +--------------------------------------------+ |
|  | TaskEmergency (Priority 5, 3072 B Stack)   |   | TaskStorage   (Priority 3, 4096 B Stack)   | |
|  | - 50ms debounced GPIO 32 evacuation monitor|   | - FATFS SD card logging & crash snapshots  | |
|  |                                            |   |                                            | |
|  | TaskEntrance  (Priority 5, 4096 B Stack)   |   | TaskCLI       (Priority 2, 4096 B Stack)   | |
|  | - 150ms MFRC522 SPI scan, Anti-Passback    |   | - UART0 115200 baud interactive console    | |
|  |                                            |   |                                            | |
|  | TaskRoom1     (Priority 4, 3072 B Stack)   |   | TaskHeartbeat (Priority 1, 2048 B Stack)   | |
|  | - 1000ms loop: PIR, LDR ADC1, PNP Relay   |   | - 5000ms watchdog monitoring free heap     | |
|  |                                            |   +--------------------------------------------+ |
|  | TaskRoom2     (Priority 4, 3072 B Stack)   |                                                  |
|  | - 1000ms loop: PIR, LDR ADC1, PNP Relay   |   KEY ARCHITECTURAL ADVANTAGE:                   |
|  +--------------------------------------------+   Zero Jitter on Actuators! Heavy FATFS writes   |
|                                                   on Core 1 never block servos or ISR on Core 0. |
+--------------------------------------------------------------------------------------------------+
```

### [Slide 3: Presenter Spoken Script]
> *"Here on Slide 3, you can see our hardware core partitioning implemented in `src/main.c`.  
> The ESP32 provides two Xtensa LX6 cores. I partitioned our tasks by temporal criticality:  
> On **Core 0**, we pinned our real-time safety, sensor loops, and motor actuators: `TaskEmergency` and `TaskEntrance` at Priority 5, and `TaskRoom1` and `TaskRoom2` at Priority 4.  
> On **Core 1**, we pinned management and background storage tasks: `TaskStorage` at Priority 3, `TaskCLI` at Priority 2, and `TaskHeartbeat` at Priority 1.  
> **Why this design?** SD card flash file operations take tens of milliseconds. By isolating `TaskStorage` on Core 1, heavy SD writes can never introduce stutter or latency into our servo PWM barriers or emergency evacuation response on Core 0."*

---

```
====================================================================================================
                        SLIDE 4: IPC DATA BUS & CONCURRENCY SYNCHRONIZATION
====================================================================================================
```

### [Slide 4: Visual Design & Layout]
```
+--------------------------------------------------------------------------------------------------+
|  FREE-RTOS IPC DATA BUS & RESOURCE SYNCHRONIZATION (Section 2.7 Compliance)                      |
+--------------------------------------------------------------------------------------------------+
|                                                                                                  |
|   [Producers: Core 0]                                             [Consumers: Core 1]            |
|   TaskEntrance  --------+                                                                        |
|                         |                                                                        |
|   TaskRoom1     ---+    |--->  [xQueueAccessEvents]  (16 msgs)  --> Diagnostics                  |
|                    |    |                                                                        |
|   TaskRoom2     ---+----+---->  [xQueueTelemetry]     (16 msgs)  --> TaskCLI (Live Display)       |
|                         |                                                                        |
|   TaskEntrance  --------+--->  [xQueueLogStorage]    (32 msgs)  --> TaskStorage (FATFS SD)       |
|                                                                                                  |
|   --------------------------------------------------------------------------------------------   |
|   SYSTEM-WIDE EVENT FLAGS: xEventGroupSystem (24-bit atomic broadcast)                           |
|     - BIT 0: FLAG_EMERGENCY_ACTIVE  -> Forces gates open & all lights ON                         |
|     - BIT 1: FLAG_CURFEW_LOCKED     -> Blocks all entry & exit attempts                          |
|     - BIT 2: FLAG_INTRUSION_ALARM   -> Triggers 60s reader lockout                               |
|     - BIT 3: FLAG_ADMIN_LOGGED_IN   -> Grants elevated CLI privileges                            |
|   --------------------------------------------------------------------------------------------   |
|   MUTEX PROTECTION:                                                                              |
|     - xSemaphoreSpiMutex : Guards shared VSPI bus between RFID1, RFID2, and SD card              |
|     - xSemaphoreDbMutex  : Eliminates race conditions on building occupancy counter & DB         |
+--------------------------------------------------------------------------------------------------+
```

### [Slide 4: Presenter Spoken Script]
> *"Slide 4 shows how we eliminated race conditions to satisfy Coursework Section 2.7.  
> Instead of dangerous shared global variables, we implemented three dedicated FreeRTOS queues:  
> `xQueueAccessEvents` for card taps, `xQueueTelemetry` for classroom lux and occupancy telemetry, and `xQueueLogStorage` as an asynchronous FIFO buffer for the SD card writer.  
> Critical system states are broadcast atomically across all tasks using `xEventGroupSystem`, including emergency active, curfew locked, and intrusion alarm flags.  
> Finally, two FreeRTOS mutexes prevent resource contention: `xSemaphoreSpiMutex` arbitrates the shared SPI lines so two tasks never clock data simultaneously, and `xSemaphoreDbMutex` guarantees that the building headcount is atomically updated without data corruption."*

---

```
====================================================================================================
                    SLIDE 5: EMERGENCY EVACUATION & LIFE-SAFETY SUBSYSTEM
====================================================================================================
```

### [Slide 5: Visual Design & Layout]
```
+--------------------------------------------------------------------------------------------------+
|  EMERGENCY EVACUATION & LIFE-SAFETY OVERRIDE (src/emergency_mgr.c)                               |
+--------------------------------------------------------------------------------------------------+
|                                                                                                  |
|  HARDWARE INTERFACE:                                                                             |
|  - Red Pushbutton on GPIO 32 (Active-LOW, Internal Pull-Up, 50ms Software Debounce Filter)       |
|                                                                                                  |
|  ATOMIC EVACUATION PROTOCOL: trigger_emergency_evacuation(true, "RED_EMERGENCY_BUTTON")          |
|                                                                                                  |
|  1. ASYMMETRIC BARRIER ROUTING (Life-Safety Innovation):                                         |
|     - EXIT GATE (GPIO 15)  --> Swings UPWARDS to 90° permanently (Exit Green LED ON)             |
|     - ENTRY GATE (GPIO 13) --> LOCKED at 0° closed barrier (Entry Green LED OFF)                 |
|     *Prevents crowd collisions by blocking inward entry while providing unhindered egress.*     |
|                                                                                                  |
|  2. CLASSROOM LIGHTING VISIBILITY OVERRIDE:                                                      |
|     - Room 1 Relay (GPIO 26) & Room 2 Relay (GPIO 25) forced LOW (Energized)                     |
|     - All 8 Purple and Blue LEDs illuminate immediately, bypassing daylight sensors.             |
|                                                                                                  |
|  3. AUDIBLE SIREN WARNING:                                                                       |
|     - Entry (GPIO 12) & Exit (GPIO 0) buzzers pulse in emergency siren cadence (200ms ON/150ms OFF)|
|                                                                                                  |
|  4. ATOMIC AUDIT LOGGING & SYSTEM RESET:                                                         |
|     - Flushes alert to SD card: "SYSTEM,RED_EMERGENCY_BUTTON,EMERGENCY,ACTIVATED"                |
|     - Second button press cleanly clears emergency state and restores automated sensor control.  |
+--------------------------------------------------------------------------------------------------+
```

### [Slide 5: Presenter Spoken Script]
> *"Slide 5 covers the emergency evacuation subsystem in `src/emergency_mgr.c`.  
> When the red emergency button on GPIO 32 is pressed, `TaskEmergency` detects the debounced falling edge and executes our atomic evacuation protocol:  
> First, **Asymmetric Barrier Routing**: The Exit Servo on GPIO 15 immediately swings to 90° and locks open with its Green LED ON, creating an unhindered egress path. Meanwhile, the Entry Servo on GPIO 13 is held locked at 0° to prevent outsiders from surging into a burning or hazardous building.  
> Second, **Classroom Visibility Override**: Both lighting relays on GPIO 26 and GPIO 25 are driven LOW, forcing all 8 classroom LEDs ON for maximum evacuation visibility, completely bypassing ambient daylight sensors.  
> Third, **Acoustic Sirens**: Dual buzzers on GPIO 12 and GPIO 0 sound a 200ms on, 150ms off emergency pulse, while a critical audit log is saved to storage."*

---

```
====================================================================================================
                        SLIDE 6: LIVE WOKWI SIMULATION DEMONSTRATION GUIDE
====================================================================================================
```

### [Slide 6: Visual Design & Layout]
```
+--------------------------------------------------------------------------------------------------+
|  LIVE SIMULATION DEMONSTRATION SCRIPT (Wokwi & VS Code)                                          |
+--------------------------------------------------------------------------------------------------+
|                                                                                                  |
|  STEP 1: SYSTEM BOOT & POST (Power-On Self-Test)                                                 |
|  - Boot output: "STARTING CoCIS SMART CONTROL & MONITORING SYSTEM (GROUP SE-24)"                 |
|  - Hardware self-test: Green & Red LEDs blink, buzzers chirp, servos calibrate to 0°            |
|                                                                                                  |
|  STEP 2: TRIGGER PHYSICAL EMERGENCY BUTTON (btn1 / GPIO 32)                                      |
|  +-----------------------------+---------------------------------------------------------------+ |
|  | Subsystem                   | Observable Real-Time Reaction in Wokwi Simulation            | |
|  +-----------------------------+---------------------------------------------------------------+ |
|  | Exit Servo (servo_exit)     | Rotates from 0° horizontal to 90° vertical (OPEN)             | |
|  | Exit Green LED              | Illuminates SOLID GREEN                                       | |
|  | Entry Servo (servo_entry)   | Held firmly at 0° horizontal (LOCKED CLOSED)                  | |
|  | Entry Green LED             | Held OFF                                                      | |
|  | Room 1 Lighting (led1,3,4,5)| All 4 Purple LEDs turn ON instantly                           | |
|  | Room 2 Lighting (led2,6,7,8)| All 4 Blue LEDs turn ON instantly                             | |
|  | Piezo Buzzers (buzzer1 & 2) | Continuous synchronized 200ms ON / 150ms OFF siren pulses     | |
|  | Terminal Console Output     | "*** [EMERGENCY EVACUATION ACTIVATED] Source: RED_BUTTON ***" | |
|  +-----------------------------+---------------------------------------------------------------+ |
|                                                                                                  |
|  STEP 3: DE-ESCALATE & RESTORE NORMAL OPERATION                                                  |
|  - Press btn1 a second time: Exit servo closes to 0°, buzzers silence, lights return to auto.    |
+--------------------------------------------------------------------------------------------------+
```

### [Slide 6: Presenter Spoken Script]
*(At this point in your video, switch your screen capture to the active Wokwi simulation tab)*:
> *"Now, let us watch the live simulation in Wokwi.  
> Notice that when the system boots, it passes the hardware self-test: the status LEDs flash, the buzzers chirp, and both gate servos calibrate to 0° closed.  
> Now, watch closely as I click the red Emergency Pushbutton on GPIO 32:  
> Immediately, look at the Exit gate—the servo swings open to 90° and the Exit Green LED illuminates. Look at the Entry gate—it remains strictly locked at 0°. Look at Lecture Rooms 1 and 2—all 8 purple and blue LEDs illuminate instantly for safe egress, while both buzzers sound their emergency sirens.  
> In the console, you can see the event logged: `EMERGENCY EVACUATION ACTIVATED`.  
> Now I click the button a second time: the exit gate rotates smoothly back to 0°, the sirens silence, and the room lights return to automated sensor control. The system has safely de-escalated."*

---

```
====================================================================================================
                        SLIDE 7: RUBRIC COMPLIANCE, INNOVATIONS & CONCLUSION
====================================================================================================
```

### [Slide 7: Visual Design & Layout]
```
+--------------------------------------------------------------------------------------------------+
|  COURSEWORK COMPLIANCE, FLAGSHIP INNOVATIONS & CONCLUSION                                        |
+--------------------------------------------------------------------------------------------------+
|                                                                                                  |
|  COURSEWORK SPECIFICATION TRACEABILITY:                                                          |
|  - Section 2.3 (Microcontroller Core) : Dual-core Xtensa LX6 task scheduling (src/main.c)       |
|  - Section 2.7 (Resource Sync)        : 3 Queues, 1 Event Group, 2 Mutexes (zero race conditions)|
|  - Section 2.8 (Emergency Override)   : Sub-100ms hardware life-safety evacuation protocol       |
|                                                                                                  |
|  FLAGSHIP ARCHITECTURAL INNOVATIONS (Section 6 Compliance):                                      |
|  1. Dual-Core Task Affinity: High-speed motor PWM on Core 0; heavy SD file I/O on Core 1        |
|  2. Asymmetric Door Policy: Prevents crowd collision during building evacuation                  |
|  3. Atomic MFRC522 Driver: Custom SPI anti-collision resolving Wokwi card reset bugs             |
|  4. Daylight Harvesting: ADC1 lux sensor inhibiting lighting in bright rooms (>= 250 lux)        |
|  5. Power Crash Recovery: Atomically restores building occupancy from MicroSD after power loss   |
|  6. Real Studio Audio Companion: Speaks crystal-clear voice announcements over PC speakers       |
|                                                                                                  |
|  SUBMISSION STATUS (MUELE DEADLINE: NOVEMBER 2, 2026):                                           |
|  - GitHub Repository : Kayeerasoftware/SE-24-CoCIS-Smart-System (Branch: main)                   |
|  - Deliverable 1     : Complete System Operation Report (Docs/SYSTEM_OPERATION_REPORT.md)        |
|  - Deliverable 2     : Wokwi Full Simulation Project Link                                        |
|  - Deliverable 3     : 5 Recorded Individual Video Demonstrations                                |
+--------------------------------------------------------------------------------------------------+
```

### [Slide 7: Presenter Spoken Script]
> *"To conclude, our single-MCU FreeRTOS architecture rigorously fulfills Coursework Sections 2.3, 2.7, and 2.8, while delivering six flagship innovations that exceed the course requirements.  
> As Group Leader, I have verified our GitHub repository on branch `main`, compiled our System Operation Report, and coordinated our team's deliverables for submission on MUELE before the November 2nd deadline.  
> Thank you Dr. Mary, and I look forward to our in-person defense."*

---

## 3. Quick Checklist Before You Hit "Record"

1. **Camera Position:** Ensure your webcam is positioned at eye level in the corner of your screen recording software (e.g., OBS Studio, Loom, or PowerPoint Record).
2. **Audio Check:** Test your microphone so your voice is crisp and clear.
3. **Wokwi Tab Ready:** Have Wokwi running in one window and VS Code / your slides in another so switching is instantaneous.
4. **Time Yourself:** Aim for **3 minutes 30 seconds to 4 minutes**—this keeps you well within the 3–5 minute coursework requirement.
5. **Demonstrate the Reset:** Make sure you show both **triggering** the emergency button and **resetting** it so the lecturer sees the complete life cycle.