<p align="center">
  <img src="./assets/logo.jpg" alt="Nebula Logo" width="600"/>
</p>

<h1 align="center">Nebula</h1>

> A hardware event project featuring a custom ESP32 CyberDeck with an OLED UI, sensors, Bluetooth audio, voice chatbot, and a web dashboard.

---

## Components List

| # | Item |
|---|---|
| 1 | ESP32 C-type |
| 2 | Microphone INMP441 |
| 3 | Speaker driver MAX98357A |
| 4 | Speaker (8 ohms, 2 W) |
| 5 | OLED display 0.96 inch |
| 6 | Capacitive Touch Sensor Module TTP223B |
| 7 | BMP180 temperature and barometric pressure sensor |
| 8 | Box enclosure |
| 9 | Jumper wires M-M |
| 10 | Jumper wires M-F |
| 11 | Breadboard |
## Features

- Animated six-frame OLED cat screensaver with Nebula branding.
- Touch-controlled CyberDeck menu navigation using the TTP223B capacitive touch sensor.
- To-Do list for simple task tracking and completion.
- Pomodoro timer with work, pause, and break modes.
- Micro Racer, a compact OLED lane-dodging game.
- Live temperature and barometric-pressure monitoring using the BMP180 sensor.
- Voice chatbot using the INMP441 microphone, cloud speech-to-text, AI response generation, and text-to-speech output.
- Audio playback through the MAX98357A I2S amplifier and 8 Ω speaker.
- Web dashboard for monitoring and customizing selected CyberDeck features.
- Dashboard-to-device synchronization for To-Do items and Pomodoro configuration, with OLED updates reflected on the CyberDeck.

---

## System Architecture

```text
                         ┌───────────────────────┐
                         │   Web Dashboard        │
                         │ To-Do / Pomodoro Sync  │
                         └───────────┬───────────┘
                                     │
                                     │ Wi-Fi / Local Network
                                     │
┌────────────────────────────────────▼───────────────────────────────────┐
│                              ESP32 CYBERDECK                            │
│                                                                         │
│  TTP223B Touch ──► Menu Navigation ──► OLED User Interface             │
│                                                                         │
│  BMP180 Sensor ──► Temperature and Pressure Display                    │
│                                                                         │
│  To-Do / Pomodoro ◄──────────── Dashboard Synchronization              │
│                                                                         │
│  INMP441 Microphone ──► 16 kHz WAV Recording                           │
└────────────────────────────────────┬───────────────────────────────────┘
                                     │
                                     │ HTTP / Wi-Fi
                                     ▼
                         ┌───────────────────────┐
                         │ Local Node.js Server  │
                         │  /transcribe          │
                         │  /ask                 │
                         │  /tts                 │
                         └───────────┬───────────┘
                                     │
                    ┌────────────────┼────────────────┐
                    ▼                ▼                ▼
          Speech-to-Text API     AI Chat API    Text-to-Speech API
                    │                │                │
                    └────────────────┴────────────────┘
                                     │
                                     ▼
                         ┌───────────────────────┐
                         │ ESP32 I2S Audio Output │
                         └───────────┬───────────┘
                                     │
                                     ▼
                         MAX98357A + 8 Ω Speaker
```

### Voice Chatbot Flow

```text
INMP441 Microphone
        ↓
ESP32 records 16 kHz WAV audio
        ↓
Local Node.js server
        ↓
Speech-to-Text API
        ↓
AI Chatbot API
        ↓
Text-to-Speech API
        ↓
ESP32 I2S audio output
        ↓
MAX98357A amplifier and speaker
```

---

## Pin Configuration

| Module | Pin / Signal | ESP32 GPIO |
|---|---|---:|
| OLED SSD1306 | SDA | GPIO 4 |
| OLED SSD1306 | SCL | GPIO 15 |
| BMP180 | SDA | GPIO 4 |
| BMP180 | SCL | GPIO 15 |
| TTP223B Touch Sensor | OUT | GPIO 13 |
| INMP441 Microphone | SCK / BCLK | GPIO 26 |
| INMP441 Microphone | WS / LRCL | GPIO 25 |
| INMP441 Microphone | SD / DOUT | GPIO 32 |
| MAX98357A Amplifier | BCLK | GPIO 14 |
| MAX98357A Amplifier | LRC / WS | GPIO 27 |
| MAX98357A Amplifier | DIN | GPIO 33 |
| MAX98357A Amplifier | VIN | 5 V / VIN |
| MAX98357A Amplifier | GND | GND |
| Speaker | SPK+ and SPK- | MAX98357A output |

> All modules must share a common GND connection.

---

## Controls

| Action | Function |
|---|---|
| Tap on screensaver | Open the main menu |
| Tap in main menu | Move to the next menu item |
| Hold in main menu | Enter the selected mode |
| Tap in To-Do mode | Mark the selected task complete and move to the next task |
| Tap in Pomodoro mode | Start, pause, resume, or begin a break |
| Tap in Micro Racer | Start the game, change lane, or retry after a crash |
| Tap in Weather mode | Refresh BMP180 sensor readings |
| Tap in Voice Chatbot mode | Record a short voice question and receive an audio response |
| Hold in any feature mode | Return to the main menu |

---

## Software Requirements

### Arduino IDE

Install the following through the Arduino Library Manager:

```text
Adafruit GFX Library
Adafruit SSD1306
Adafruit BMP085 Library
ArduinoJson
Arduino Audio Tools
```

Install the **ESP32 by Espressif Systems** board package through:

```text
Arduino IDE → File → Preferences → Additional Boards Manager URLs
```

Add the following board manager URL:

```text
[https://espressif.github.io/arduino-esp32/package_esp32_index.json](https://espressif.github.io/arduino-esp32/package_esp32_index.json)
```

Then install:

```text
Tools → Board → Boards Manager → ESP32 by Espressif Systems
```

Recommended board selection:

```text
ESP32 Dev Module
```

Recommended Serial Monitor baud rate:

```text
115200
```

### Visual Studio Code and Server

Install the following on the computer that will run the local voice server:

```text
Node.js 18 or newer
Visual Studio Code
```

Recommended VS Code extensions:

```text
JavaScript and TypeScript Language Features
ESLint
Prettier - Code formatter
Arduino or PlatformIO extension (optional)
```

Open the server folder in Visual Studio Code:

```bash
code .
```

Install Node.js dependencies:

```bash
npm install
```

If dependencies have not yet been defined in `package.json`, install them manually:

```bash
npm install express cors dotenv @google/genai
```

For the Deepgram and Gemini voice-server setup, the project uses:

```text
Express
Cors
Dotenv
@google/genai
Node.js built-in fetch API
```

---

## Server Configuration

Create a file named `.env` in the same folder as `server.js`.

```env
GEMINI_API_KEY=your_gemini_api_key
DEEPGRAM_API_KEY=your_deepgram_api_key

PORT=3000

GEMINI_MODEL=gemini-3.6-flash
DEEPGRAM_STT_MODEL=nova-3
DEEPGRAM_TTS_MODEL=aura-2-thalia-en
```

Keep API keys private. Do not commit `.env` to GitHub.

Add the following to `.gitignore`:

```gitignore
node_modules/
.env
```

Start the local Node.js server:

```bash
node server.js
```

Expected output:

```text
Nebula server running on http://0.0.0.0:3000
Health: http://localhost:3000/health
Transcribe: POST /transcribe
Ask: POST /ask
TTS: POST /tts
```

Test the local server:

```powershell
Invoke-WebRequest `
  -Uri "http://localhost:3000/health" `
  -UseBasicParsing |
  Select-Object -ExpandProperty Content
```

---

## Arduino Setup

1. Assemble the ESP32 CyberDeck using the pin configuration table.
2. Open the Nebula `.ino` file in Arduino IDE.
3. Update the Wi-Fi credentials:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

4. Find the IPv4 address of the computer running `server.js`:

```powershell
ipconfig
```

5. Under the active Wi-Fi adapter, find the IPv4 address, for example:

```text
IPv4 Address . . . . . . . . . . : 192.168.1.25
```

6. Update the server address in the Arduino sketch:

```cpp
const char* SERVER = "http://192.168.1.25:3000";
```

7. Select the correct board and COM port:

```text
Tools → Board → ESP32 Dev Module
Tools → Port → Select ESP32 COM Port
```

8. Click **Verify** to compile the project.
9. Click **Upload** to flash the ESP32.
10. Open Serial Monitor at `115200` baud for debugging.

---

## Dashboard Setup

The Nebula dashboard is designed to extend the CyberDeck interface to a browser-based control panel.

Dashboard functions include:

- Viewing and managing To-Do items.
- Customizing Pomodoro work and break durations.
- Monitoring selected CyberDeck data.
- Synchronizing selected dashboard settings with the OLED interface.

For dashboard synchronization, the ESP32 and dashboard server must be connected to the same Wi-Fi network. The Node.js server can expose REST endpoints for dashboard updates, device status, To-Do data, and Pomodoro settings.

Example dashboard synchronization flow:

```text
Web Dashboard
      ↓
Node.js API Endpoint
      ↓
ESP32 Wi-Fi Request
      ↓
CyberDeck State Update
      ↓
OLED Interface Refresh
```

---

## Project Structure

```text
nebula/
├── assets/
│   └── logo.jpg
├── firmware/
│   └── nebula_full_modes_voice_fixed.ino
├── server/
│   ├── server.js
│   ├── package.json
│   ├── package-lock.json
│   ├── .env
│   └── .gitignore
├── dashboard/
│   └── README.md
└── README.md
```

---

## Notes

- The voice chatbot requires the ESP32 and the computer running `server.js` to be connected to the same Wi-Fi network.
- The local computer IP address can change after reconnecting to Wi-Fi. Update the `SERVER` IP address in the Arduino sketch and re-upload the firmware when this happens.
- The voice chatbot records approximately two seconds of mono 16 kHz WAV audio.
- The server processes the audio through speech-to-text, AI chat, and text-to-speech services before returning WAV audio to the ESP32.
- The MAX98357A must be connected to an external speaker; do not connect the speaker directly to ESP32 GPIO pins.
- OLED and BMP180 share the same I2C bus.
- The BMP180 sensor is used for local environmental readings and does not require an external weather API.
- API keys must remain in `.env` and must never be committed to a public repository.
- Windows Firewall may need to allow Node.js access on private networks for the ESP32 to reach the local server.
---

## Contributors

| Name | GitHub |
|---|---|
| Niya Juban | [@niyajuban](https://github.com/niyajuban) |
| Swathi | [@swathi-o](https://github.com/swathi-o) |
| Aparajita | [@AparajitaY](https://github.com/AparajitaY) |

---

## License

TBD

