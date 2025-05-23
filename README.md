# Smart MediBox

The **Smart MediBox** is an embedded systems project designed to provide an intelligent medicine storage solution with features such as time-synced alarm notifications, environmental monitoring, and remote interaction through MQTT and Node-RED.

## 📦 Project Overview

This project leverages the ESP32 microcontroller to build a smart medicine box that manages alarms, monitors environmental conditions, and adjusts light exposure. It also integrates with **Node-RED** for real-time data visualization and remote interaction.

## 🛠️ Features

### 🎯 Core Functionalities
- **Time Synchronization:** Syncs with NTP servers with timezone configuration.
- **Alarm Management:** Create, manage, and disable medication alarms.
- **OLED Display:** Shows time, system status, and alarm notifications.
- **Audible Alerts:** Buzzer activates during alarms; stopped by push buttons.
- **Environmental Monitoring:** Tracks temperature and humidity levels.
- **Light Monitoring:** LDRs measure ambient light intensity.
- **Servo Motor Control:** Adjusts a shaded window based on light intensity and medicine requirements.

### 🚀 Advanced Functionalities
- **Persistent Storage:** Saves alarms and user preferences to non-volatile memory.
- **User Interface:** Menu-based navigation via OLED and push buttons.
- **Change Detection:** Efficient sensor polling to reduce power usage.
- **Continuous Monitoring:** Always-on sensors with real-time dashboard updates.

## ⚙️ System Architecture

### 🧩 Modules
- **Hardware Abstraction Layer:** Simplifies interactions with physical components.
- **Sensor Management:** Handles data from DHT (Temp/Humidity) and LDRs.
- **Alarm Management:** Stores and triggers alarms.
- **Time Management:** Handles time sync and local offset.
- **User Interface:** Menu-driven interaction via buttons and OLED.
- **Communication Management:** Publishes/subscribes MQTT topics for remote control and monitoring.

## 🔧 Hardware Requirements

| Component                 | Description                          |
|--------------------------|--------------------------------------|
| ESP32 Development Board  | Microcontroller with WiFi/Bluetooth  |
| OLED Display             | 128x64 display for interface         |
| Buzzer                   | Audible notification device          |
| Push Buttons             | User input for control               |
| LDRs                     | Light intensity sensors              |
| Servo Motor              | Controls shaded window               |

## 💻 Software Requirements

- [Arduino IDE](https://www.arduino.cc/en/software)
- [Wokwi Simulator](https://wokwi.com/)
- [Node-RED](https://nodered.org/)
- MQTT Broker (e.g. `test.mosquitto.org`)
- VSCode (Optional for advanced editing)

## 🧪 Installation & Setup

### 1. Clone the Repository
```bash
git clone https://github.com/MihiruthS/Smart-Medibox
cd Smart-Medibox
