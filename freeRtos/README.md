# ESP32 FreeRTOS System Monitor

A real-time system monitoring dashboard built on ESP32 using ESP-IDF and FreeRTOS.
Accessible from any browser on the local network.

## Features
- Live temperature graph updated every 2 seconds
- FreeRTOS task monitor showing stack usage per task
- Threshold-based alert system with alert log
- Free heap memory and system uptime tracking
- Clean modular C architecture

## FreeRTOS Concepts Demonstrated
- Multiple concurrent tasks with different priorities
- Inter-task communication via queues
- Shared resource protection with mutexes
- System-wide state management with event groups
- Stack high watermark monitoring

## Architecture
TempTask (priority 5) → temp_queue → AlertManager (priority 6)

SysMonitor (priority 3) → stats_queue → WebServer

All tasks share data via mutex-protected globals and event groups

## Tech Stack
- ESP32 (Xtensa dual-core 240MHz)
- ESP-IDF v6.0.1
- FreeRTOS
- C

## Project Structure
main/
├── main.c              # Entry point

├── shared.h/c          # Shared queues, events, data structures

├── wifi.c/h            # WiFi connection module

├── temperature.c/h     # Sensor reading task

├── alert_manager.c/h   # Threshold monitoring task

├── system_monitor.c/h  # Heap and stack stats task

└── webserver.c/h       # HTTP server and dashboard

## Setup
1. Install ESP-IDF v6.0.1
2. Clone this repo
3. Update WiFi credentials in `main/wifi.c`
4. Run `idf.py set-target esp32`
5. Run `idf.py build flash monitor`
6. Open `http://<ESP32_IP>` in your browser
