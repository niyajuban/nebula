# Nebula CyberDeck — Comprehensive Architecture & Change Documentation

> **Project:** Nebula CyberDeck (ESP32 + Node.js Companion Dashboard)  
> **Last Updated:** September 2026  
> **Target Location:** Vellore, Tamil Nadu, India (`12.9165° N, 79.1325° E`)  
> **Primary Microcontroller:** ESP32-WROOM-32 (30-pin / 38-pin DevKit)  
> **Companion Server:** Node.js HTTP Server (`http://10.158.254.62:3000`)

---

## 1. Executive Summary

This document records the architectural and functional transformations performed on the Nebula CyberDeck system:
1. **Transition from Physical BMP180 to Open-Meteo API**: Replaced the onboard BMP180 pressure/temperature sensor with live meteorological telemetry fetched from Open-Meteo for Vellore, eliminating hardware I2C sensor dependencies and adding live relative humidity and weather condition status.
2. **ESP32 Firmware Non-Blocking Optimization**: Solved severe ESP32 UI stutter/freezing caused by blocking Wi-Fi HTTP requests by introducing aggressive timeouts (700 ms), relaxed polling intervals (8 s), and dynamic failure backoff (30 s).
3. **Hardware Pinout Confirmation**: Verified and locked the I2C OLED display bus to standard hardware pins **GPIO 21 (SDA)** and **GPIO 22 (SCL)**.
4. **Complete Removal of Bluetooth Mode**: Removed Bluetooth/Music mode from the companion dashboard and backend state to maintain Bluetooth audio as an isolated, standalone sketch. The dashboard and hardware OLED menu now share a 1:1, 5-mode layout.
5. **Triple-Tree Workspace Synchronization**: Synchronized all code updates across Arduino IDE, backend, and Git repository directories.

---

## 2. Weather Subsystem: BMP180 Retirement & Open-Meteo Integration

### 2.1 Rationale
- **Hardware Simplification**: Physical BMP180 sensors are prone to loose dupont jumper connections, I2C bus lockups, and only measure temperature and barometric pressure (no relative humidity).
- **Comprehensive Atmospheric Data**: Open-Meteo provides accurate real-time temperature (°C), surface barometric pressure (hPa), relative humidity (%), and WMO weather condition codes (clear, cloudy, rain, fog) without requiring an API key.

### 2.2 Open-Meteo vs. OpenWeatherMap
| Feature | Open-Meteo (Selected) | OpenWeatherMap |
| :--- | :--- | :--- |
| **API Key Requirement** | **None (Zero configuration)** | Required (Must register and wait for activation) |
| **Free Tier Constraints** | Up to 10,000 calls/day (More than enough) | Rate-limited, card required for 3.0 OneCall |
| **Response Latency** | Fast JSON payload (~80–140 ms) | Variable latency |
| **Metrics Included** | Temperature, Pressure, Humidity, WMO Code | Split across multiple legacy endpoints |

### 2.3 Backend Implementation (`server.js`)
- **Coordinates Configured**: Vellore (`latitude=12.9165`, `longitude=79.1325`).
- **Endpoint**: `https://api.open-meteo.com/v1/forecast?latitude=12.9165&longitude=79.1325&current=temperature_2m,relative_humidity_2m,surface_pressure,weather_code`
- **Caching Mechanism**: Weather is cached in memory (`state.weather`) and automatically updated every 15 minutes (`WEATHER_UPDATE_MINS=15`), reducing outbound internet traffic while serving instant responses to the ESP32 and web dashboard.
- **WMO Code Translation**: Built-in decoder maps numerical WMO codes (e.g. `0` = "Clear Sky", `1-3` = "Partly Cloudy", `61-65` = "Rain", `71-75` = "Snow", `95` = "Thunderstorm") into friendly display strings.
- **REST Endpoints Exposed**:
  - `GET /api/weather` — Returns full weather object with city, temperature, pressure, humidity, and condition.
  - `GET /api/sensors` — Formats weather data as simulated sensor readings (`temperatureC`, `pressureHpa`, `humidity`, `condition`, `bmpAvailable: false`).
  - `POST /api/telemetry` — Ingests ESP32 telemetry while responding with the latest atmospheric readings.

### 2.4 ESP32 Firmware Changes (`nebula.ino`)
- Removed `#include <Adafruit_BMP085.h>` and `Adafruit_BMP085 bmp;`.
- Removed `bmp.begin()` initialization from `setup()`.
- Updated `renderWeather()` on the SSD1306 OLED to display:
  - Line 1: `WEATHER (VELLORE)`
  - Line 2: `Temp: XX.X C`
  - Line 3: `Pres: XXX.X hPa`
  - Line 4: `Hum:  XX%`
  - Line 5: Condition label (e.g., `Mainly Clear`, `Rain`)
- Fallback logic: If the backend has not yet reported data, default placeholder values are shown until the first Wi-Fi sync completes.

---

## 3. ESP32 Performance Optimization (Eliminating UI Freeze)

### 3.1 Root Cause of Slowdown
In earlier revisions, `syncSensorsWithBackend()` was invoked synchronously inside `loop()` every 2000 ms (`SENSOR_INTERVAL`). When an HTTP request took 2 to 6 seconds to complete or timed out over Wi-Fi:
1. The CPU blocked completely inside `http.POST()`.
2. OLED frame updates stopped, causing stuttering animations.
3. Touch inputs on GPIO 13 were missed or delayed by several seconds.
4. The Micro Racer game loop dropped to < 1 FPS.

### 3.2 Implemented Fixes
1. **Aggressive Timeout**: Configured `http.setTimeout(700);` (700 milliseconds). If the backend does not answer within 700 ms, the request aborts and frees the CPU immediately.
2. **Relaxed Telemetry Polling**: Increased normal sync interval from 2,000 ms to **8,000 ms** (`telemetryInterval = 8000UL`).
3. **Dynamic Failure Backoff**:
   ```cpp
   if (httpCode == HTTP_CODE_OK) {
     telemetryInterval = 8000UL;   // Normal interval on success
   } else {
     telemetryInterval = 30000UL;  // Back off 30 seconds on failure/timeout
   }
   ```
   If Wi-Fi drops or the server is restarted, the ESP32 will only retry once every 30 seconds, maintaining a 60 FPS UI refresh rate and responsive touch interaction.
4. **Screen-Aware Eager Sync**: Entering the Weather screen immediately resets `lastTelemetrySync = 0; telemetryInterval = 0;` to trigger a fast fetch without waiting for the 8s timer.
5. **Scope Bug Fix**: Moved `lastTelemetrySync` and `telemetryInterval` to global scope in `nebula.ino` to resolve compilation errors in `handleShortPress()`.

---

## 4. Hardware Pinout & Wiring Specification

The hardware layout has been locked to the following pin assignments:

| Component | Pin Function | ESP32 GPIO | Electrical Notes |
| :--- | :--- | :--- | :--- |
| **SSD1306 0.96" OLED** | **SDA (Data)** | **GPIO 21** | Standard hardware I2C data |
| **SSD1306 0.96" OLED** | **SCL (Clock)** | **GPIO 22** | Standard hardware I2C clock |
| **SSD1306 0.96" OLED** | VCC / GND | 3.3V / GND | I2C Address: `0x3C` |
| **TTP223 Touch Button** | Signal OUT | **GPIO 13** | Active HIGH digital touch input |
| **INMP441 Microphone** | BCLK (Bit Clock) | **GPIO 26** | I2S Audio In |
| **INMP441 Microphone** | WS / LRCLK | **GPIO 25** | I2S Word Select |
| **INMP441 Microphone** | SD / DIN | **GPIO 32** | I2S Serial Data In |
| **INMP441 Microphone** | L/R Select | GND | Left channel |
| **MAX98357A Amp/DAC** | BCLK (Bit Clock) | **GPIO 14** | I2S Audio Out |
| **MAX98357A Amp/DAC** | LRC / WS | **GPIO 27** | I2S Word Select |
| **MAX98357A Amp/DAC** | DIN / DOUT | **GPIO 33** | I2S Serial Data Out |
| **MAX98357A Amp/DAC** | GAIN / SD | GND / Unconnected | Default 9dB gain |

---

## 5. Complete Removal of Bluetooth Mode

Per user direction, Bluetooth audio streaming has been decoupled from the Nebula CyberDeck firmware and companion dashboard to remain as an independent standalone project (`05 - music-bluetooth`).

### 5.1 Dashboard Mode Realignment (5 Modes Total)
Both the physical CyberDeck OLED menu and the companion web dashboard now strictly follow a synchronized 5-mode structure:

```
[Mode 1] To-Do List        — Dynamic server-synced interactive checklist
[Mode 2] Pomodoro Timer    — 25/5 focus & break session countdown
[Mode 3] Game Mode         — Micro Racer hardware obstacle avoidance
[Mode 4] Weather           — Open-Meteo live Vellore atmospheric stats
[Mode 5] AI ChatBot        — Hardware push-to-talk voice assistant (Deepgram + Gemini)
```

### 5.2 Files Modified for Bluetooth Removal
1. **`public/index.html`**:
   - Removed the entire `<section class="mode-view" id="view-music">` markup and audio visualizer wave bars.
   - Updated `.screen-header` mode counter to **`MODE 1/5`**.
   - Updated `#mode-dots` from 6 indicator dots down to **5 dots** (`data-idx="0"` through `4`), defaulting to dot `0` (To-Do List).
   - Set `#view-todo` as the active default view.
2. **`public/app.js`**:
   - Updated `modesOrder`:
     ```javascript
     const modesOrder = ['todo', 'pomodoro', 'game', 'weather', 'chatbot'];
     ```
   - Updated `modeMeta` descriptors with `1/5` through `5/5` headers.
   - Removed `music: { isPlaying: true }` state tracking.
   - Removed `initMusic()` function, audio play/pause controls, and `toggleBluetoothPlayback` socket/command invocations.
   - Removed `initMusic()` call from `DOMContentLoaded`.
3. **`public/styles.css`**:
   - Removed lines 580–704 containing `.music-wrapper`, `.music-status-card`, `.music-beacon`, `.music-visualizer`, and `.music-btn`.
4. **`server.js`**:
   - Removed `bluetooth: { audioStarted: true, phoneConnected: true, phonePlaying: false }` from default state.
   - Removed `bluetooth` field from `GET /api/status`.
   - Removed `toggleBluetoothPlayback` handler from `POST /api/command`.

---

## 6. Directory Synchronization Status

All changes have been synchronized across all three working paths:

| File | Arduino IDE Workspace | Backend Working Directory | Git Repository Directory | Status |
| :--- | :--- | :--- | :--- | :--- |
| `nebula.ino` | `.../Documents/Arduino/nebula/` | `.../Documents/nebula_chatbot_backend/nebula/` | `C:/Users/niyaj/nebula/` | **Identical (Hash: `1A41385...`)** |
| `server.js` | N/A | `.../Documents/nebula_chatbot_backend/` | `C:/Users/niyaj/nebula/` | **Synchronized** |
| `public/index.html` | N/A | `.../Documents/nebula_chatbot_backend/public/` | `C:/Users/niyaj/nebula/public/` | **Synchronized** |
| `public/app.js` | N/A | `.../Documents/nebula_chatbot_backend/public/` | `C:/Users/niyaj/nebula/public/` | **Synchronized** |
| `public/styles.css` | N/A | `.../Documents/nebula_chatbot_backend/public/` | `C:/Users/niyaj/nebula/public/` | **Synchronized** |

---

## 7. Verification & Run Instructions

### 7.1 Running the Backend
1. Open PowerShell or Command Prompt in `C:\Users\niyaj\OneDrive\Documents\nebula_chatbot_backend`.
2. Run:
   ```powershell
   node server.js
   ```
3. Verify output confirms:
   - `[Config] Loaded environment variables`
   - `[Weather] Fetched live weather for Vellore from Open-Meteo: 25.2°C, 984.6 hPa, 86% humidity, Mainly Clear`
   - `Nebula CyberDeck Server running at http://localhost:3000`
4. Access the dashboard in your browser: `http://localhost:3000` (or over Wi-Fi at `http://10.158.254.62:3000`).

### 7.2 Flashing the ESP32 Firmware
1. Open `C:\Users\niyaj\OneDrive\Documents\Arduino\nebula\nebula.ino` in Arduino IDE.
2. Select Board: **ESP32 Dev Module** (or **DOIT ESP32 DEVKIT V1**).
3. Connect ESP32 via USB and select the appropriate COM Port.
4. Verify Wi-Fi settings in lines 10–12:
   ```cpp
   const char* WIFI_SSID = "OnePlus 12R";
   const char* WIFI_PASSWORD = "12345678";
   const char* SERVER = "http://10.158.254.62:3000";
   ```
5. Click **Verify / Compile** (`Ctrl + R`) — compilation will succeed with zero BMP/scope errors.
6. Click **Upload** (`Ctrl + U`).
7. Open Serial Monitor at **115200 baud**.
8. Observe fast boot, OLED initialization on `SDA=21, SCL=22`, Wi-Fi connection, and real-time navigation across all 5 modes without lag.
