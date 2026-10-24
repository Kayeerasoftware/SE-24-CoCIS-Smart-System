# SE-24 CoCIS Smart Control and Monitoring System
**Makerere University | College of Computing & Information Sciences**  
**Course:** Embedded Systems Coursework (Academic Year 2026/2027)  
**Group:** SE-24 | **Group Leader & Architect:** Kayeera Nathan (24/U/25913/EVE)  
**Target Platform:** ESP32 (ESP-IDF) + PlatformIO in VS Code  
**Communication:** Wireless ESP-NOW (2.4 GHz Peer-to-Peer Wi-Fi)

---

## 🚀 Quick Start Guide (VS Code + PlatformIO)

1. Open this folder in VS Code: `File -> Open Folder... -> SE24_CoCIS_Smart_System`.
2. Ensure you have the **PlatformIO IDE** and **Wokwi Simulator** extensions installed.
3. Build any firmware target:
   - `pio run -e ENTERANCE_MASTER`
   - `pio run -e ROOM_NODE_1`
   - `pio run -e ROOM_NODE_2`
4. Start the 3-board wireless simulation:
   - Press `F1` in VS Code -> Type `Wokwi: Start Simulator` -> Press Enter.

---

## 📂 Project Architecture
- `components/common_protocol/`: Shared packet structures, synchronization objects, and MAC address mappings.
- `src/ENTERANCE_MASTER/`: Entrance Master controller (RFID, door servos, SD card, Serial CLI, ESP-NOW gateway).
- `src/ROOM_NODE_1/`: Lecture Room 1 controller (PIR motion, LDR daylight sensor, relay, ESP-NOW sender).
- `src/ROOM_NODE_2/`: Lecture Room 2 controller (ESP32 #3).
- `simulation/sdcard/`: Pre-loaded virtual SD card files (`users.csv`, `access_log.csv`, etc.).
- `docs/`: Full system architecture diagrams and PDF handbooks.
