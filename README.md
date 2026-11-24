# Agri-Rover — Universal Solar Precision Agriculture & Crop Care Platform

Agri-Rover is a solar-assisted autonomous precision agriculture rover paired with a completely offline Android companion app. It patrols crop rows, captures leaf imagery, measures root-zone soil moisture alongside ambient temperature and humidity, runs offline on-device leaf health classification (`Healthy`, `Diseased`, `Not a Plant`), and automates targeted spot irrigation without requiring internet access or cloud subscriptions.

---

## Key Features

- **Offline On-Device Leaf Classification:** Runs directly on Android using an adjustable sensitivity threshold to classify frames as `Healthy`, `Diseased`, or `Not a Plant` with zero cloud or cellular dependency.
- **Root-Zone Soil & Microclimate Telemetry:** Combines a servo-actuated soil moisture probe with temperature and humidity sensing and streams data over a local Wi-Fi endpoint (such as `NanoHTTPD`).
- **Automated Spot Irrigation:** Triggers a water pump automatically when soil moisture drops below crop thresholds, with manual override options in the app.
- **Solar-Assisted Power Stage:** Uses solar panels with MPPT/charge control and a protected 3S Li-ion battery pack with a BMS to extend field runtimes.
- **Built for Diverse Farming Environments:** Adaptable chassis and sensor configurations designed for smallholder plots, open fields, nurseries, greenhouses/polyhouses, and cooperative pilots.

---

## System Architecture

```text
[Solar Panels] ---> [MPPT / Charge Controller] ---> [3S Li-Ion Pack + 3S BMS]
                                                             |
                                            +----------------+----------------+
                                            |                                 |
                                     [Drive & Pump]                 [Regulated Logic Rail]
                                     [Motor Driver]                         |
                                            |                      +----------+----------+
                                   [Drive Motors +]                |                     |
                                   [Spot Water Pump]         [ESP32-S3]          [Soil Moisture]
                                                             [Camera Node]       [Servo + DHT/Env]
                                                                    |
                                                      (Local Wi-Fi / HTTP)
                                                                    |
                                                          [Offline Android App]
                                                       (Leaf AI + Telemetry UI)
```

---

## Hardware Bill of Materials (BOM) & Setup

### Core Hardware Components
1. **Controller & Vision:** ESP32-S3 module with camera support
2. **Mobility & Actuation:** 4x geared drive motors + motor driver board, servo motor for soil probe actuation, and low-voltage DC water pump
3. **Sensing:** Soil moisture probe, ambient temperature & humidity sensor
4. **Power & Charging:** Dual solar panels (up to `15V` max per panel), MPPT/solar charge controller boards (configured for the target battery voltage), 3S Li-ion battery pack (3x 18650 cells in series) with a 3S BMS, main power switch, and buck converters for logic rails
5. **Chassis & Enclosure:** 3D-printed or fabricated frame/enclosure, wiring harness, connectors, and mounting brackets

### Step-by-Step Hardware Assembly & Startup

> **Safety First:** Always verify polarity (`+` and `-`) with a multimeter before connecting power. Keep extra/unused balance or charger leads safely insulated so they cannot short. Never bypass the BMS charge/discharge protection path.

1. **Assemble the Chassis & Mechanical Subsystems:**
   - Mount the four drive motors, wheels, and motor driver on the rover frame.
   - Mount the servo-actuated soil moisture probe, environmental sensor, and spot-irrigation pump/tubing securely away from electrical wiring.
2. **Wire the 3S Battery Pack & BMS:**
   - Wire the three 18650 cells in series (`3S`, `~11.1V` nominal / `~12.6V` full) to the 3S BMS balance and main terminals (`B-`, `B1`, `B2`, `B+`).
   - Connect the rover load (motor driver and step-down buck regulators) through a main power switch to the BMS output leads (`P+` and `P-`).
3. **Wire the Solar Charging Stage:**
   - Connect each solar panel (`+` / `-`) to its charge controller input (`IN+` / `IN-`).
   - Connect the charge controller battery output (`B+` / `B-`) to the 3S battery pack's charge path (`P+` / `P-` on a common-port BMS, or `P+` / `C-` on a separate-port BMS), verifying polarity and charging behavior in bright light/sunlight.
4. **Wire & Flash the ESP32-S3 Node:**
   - Wire camera, servo, sensor, and pump/relay control pins to the ESP32-S3 using a common ground (`GND`) across logic modules.
   - Flash the ESP32-S3 firmware using **Arduino IDE** or **PlatformIO** (configuring Wi-Fi AP/hotspot or local network credentials and sensor/actuator pins).
5. **Power On:**
   - Flip the main battery power switch **ON**, confirm regulated power rails are healthy, and verify the ESP32-S3 local server/stream is reachable.

---

## Building and Starting the Android App

### Prerequisites
- **Android Studio** (recent Hedgehog / Iguana / Ladybug or newer release) with Android SDK installed
- **JDK 17+** (as configured by Android Studio)
- An **Android device** (or emulator) with Wi-Fi enabled for testing local rover communication

### 1. Open and Build the Project
```bash
git clone <your-repository-url>
cd <your-repository-folder>
```
1. Open the Android app project folder in **Android Studio**.
2. Let **Gradle Sync** finish and download required dependencies.
3. Connect your Android phone via USB (with **USB Debugging** enabled) or start an emulator.
4. Click **Run (`▶`)** in Android Studio to compile and install the app—or build a standalone APK via **Build > Build Bundle(s) / APK(s) > Build APK(s)**.

### 2. Start and Use the App in the Field
1. Power on the **Agri-Rover** hardware and connect your phone's Wi-Fi to the rover's local Wi-Fi network/endpoint.
2. Open the **Agri-Rover** app on your Android device (no mobile data or internet connection required for offline classification).
3. Monitor live telemetry (soil moisture, temperature, humidity) and inspect leaf frames classified into **`Healthy`**, **`Diseased`**, or **`Not a Plant`**.
4. Adjust the classification sensitivity threshold if needed and test manual or automated spot-irrigation pump triggers.

---

## Repository Structure (Suggested)

```text
├── firmware/          # ESP32-S3 camera, sensor, servo & pump control code
├── android-app/       # Offline Android companion app source & Gradle project
├── hardware/          # Wiring notes, BOM, 3D/chassis files & test logs
└── docs/              # Business plan, pitch materials & field validation notes
```

---


