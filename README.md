# 🌱 Next-Gen AI Agricultural Rover
**Autonomous Edge Vision, Early Disease Detection & Precision Soil Telemetry**  
*Developed for the NASA Space Apps Challenge*

---

## 📖 Overview
Smallholder farms frequently lose time, water, and crop yield due to delayed disease diagnosis and inefficient manual resource management. The **Next-Gen AI Agricultural Rover** is a closed-loop autonomous field robotics platform that detects plant health issues early, senses soil conditions locally, and executes targeted irrigation where crops need it most.

### 🎯 Core Capabilities
* **Autonomous Visual Target Lock:** Continuously scans live RGB565 camera frames using an **ESP32-S3-WROOM-1 CAM (N16R8)** to identify blue field markers.
* **Early Disease Detection & Close-Up Imaging:** Executes a precision approach, halts, pans the camera 90° left, and captures high-resolution plant imagery alongside micro-climate stress telemetry to detect early signs of crop disease before infections spread.
* **Automated Soil Telemetry & Irrigation:** Deploys a motorized mechanical probe to measure soil moisture, ambient temperature, and humidity, automatically triggering targeted watering when moisture drops below threshold levels.
* **Zero-Router Wireless Telemetry & Dashboard:** Uses low-latency **ESP-NOW** for rover-to-driver motor control and streams timestamped plant snapshots and per-cycle sensor logs to a local **Flask Web Dashboard**.

---

## 🛠️ Hardware Architecture

| Subsystem | Component | Role |
| :--- | :--- | :--- |
| **Vision & Edge Compute** | ESP32-S3-WROOM-1 CAM (N16R8, 8MB PSRAM) | Autonomous color tracking, camera pan/probe servo control, Wi-Fi dashboard logging |
| **Locomotion & Actuation** | ESP32 Driver Node + L298N Motor Driver | Receives ESP-NOW commands to control drive motors and water pump relay |
| **Soil & Climate Sensing** | Capacitive Soil Moisture Sensor + DHT22 | Measures root-zone moisture percentage, ambient temperature (°C), and humidity (%) |
| **Mechanical Gimbal** | Dual Micro Servos (Pan & Probe) | 90° camera pan for plant inspection and vertical soil probe deployment |
| **Power System** | 2S 18650 Li-ion Pack + BMS + 5V Buck Converter | Regulated high-current power supply for stable camera and servo operation |

---

## ⚙️ System Workflow

1. **Search & Target Lock:** Rover scans its live camera feed for visual markers.
2. **Approach Phase:** Upon locking onto a target, the Brain Node commands the Driver Node via ESP-NOW to drive forward for 2 seconds, then halts.
3. **Inspection & Sampling:** Camera pans 90° toward the crop while the mechanical soil probe lowers into the soil for 5 seconds to record moisture, temperature, and humidity.
4. **Cloud/Local Logging:** A timestamped snapshot and full sensor payload are transmitted to the mobile Flask dashboard for real-time health monitoring.
5. **Reset:** Probe retracts, camera re-centers, and the rover resumes autonomous scanning.

---

## 🚀 Setup & Installation

### 1. Firmware (`firmware/`)
* **Board:** `ESP32S3 Dev Module` (Arduino IDE)
* **PSRAM:** `OPI PSRAM` (Enabled)
* **Partition Scheme:** `Huge APP (3MB No OTA/1MB SPIFFS)`
* **Libraries Required:** `esp_camera`, `ESP32Servo`, `DHT sensor library`
* Flash `brain_s3.ino` to the **ESP32-S3 CAM** and `driver_node.ino` to the **ESP32 Driver Node** (ensure the Driver Node's MAC address is updated in `brain_s3.ino`).

### 2. Mobile Web Dashboard (`dashboard/`)
Install Python dependencies and launch the local telemetry server:
```bash
pip install flask
python dashboard/app.py
