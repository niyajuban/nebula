/*
  Nebula CyberDeck - Bluetooth Music Receiver

  ESP32 Bluetooth A2DP music receiver with:
  - SSD1306 OLED display
  - Track title and artist metadata
  - Touch-controlled play/pause
  - MAX98357A I2S amplifier

  OLED:
  SDA -> GPIO 21
  SCL -> GPIO 22

  MAX98357A:
  BCLK -> GPIO 14
  LRC/WS -> GPIO 27
  DIN -> GPIO 33
  VIN -> 5V
  GND -> Common GND
  SD -> 3.3V

  Touch sensor:
  SIG -> GPIO 13
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// -------------------------
// OLED configuration
// -------------------------

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

#define SDA_PIN 21
#define SCL_PIN 22

// -------------------------
// Touch configuration
// -------------------------

#define TOUCH_PIN 13
#define TOUCH_ACTIVE HIGH

// -------------------------
// MAX98357A I2S pins
// -------------------------

#define SPK_BCLK 14
#define SPK_WS   27
#define SPK_DOUT 33

// -------------------------
// Objects
// -------------------------

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

// -------------------------
// State variables
// -------------------------

String trackTitle = "Ready to Connect";
String trackArtist = "Nebula Bluetooth";

bool isPlaying = false;
bool isConnected = false;
bool previousTouch = false;

// -------------------------
// OLED helper
// -------------------------

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("BLUETOOTH AUDIO");

  display.drawLine(0, 10, 127, 10, WHITE);

  display.setCursor(0, 14);

  if (!isConnected) {
    display.println("Status: DISCONNECTED");

    display.setCursor(0, 26);
    display.println("Pair your phone with:");

    display.setCursor(0, 38);
    display.println("Nebula CyberDeck");
  } else {
    display.print("Status: ");
    display.println(isPlaying ? "PLAYING" : "PAUSED");

    display.setCursor(0, 26);
    display.print("Track: ");

    String shortTitle = trackTitle;

    if (shortTitle.length() > 14) {
      shortTitle = shortTitle.substring(0, 14) + "...";
    }

    display.println(shortTitle);

    display.setCursor(0, 38);
    display.print("Artist: ");

    String shortArtist = trackArtist;

    if (shortArtist.length() > 13) {
      shortArtist = shortArtist.substring(0, 13) + "...";
    }

    display.println(shortArtist);
  }

  display.drawLine(0, 52, 127, 52, WHITE);

  display.setCursor(0, 56);

  if (isConnected) {
    display.print("Tap: Play / Pause");
  } else {
    display.print("Waiting for audio...");
  }

  display.display();
}

// -------------------------
// Bluetooth metadata callback
// -------------------------

void avrc_metadata_callback(
  uint8_t attribute,
  const uint8_t *text
) {
  if (text == nullptr) {
    return;
  }

  if (attribute == ESP_AVRC_MD_ATTR_TITLE) {
    trackTitle = String((const char *)text);

    if (trackTitle.length() == 0) {
      trackTitle = "Unknown Track";
    }
  }

  if (attribute == ESP_AVRC_MD_ATTR_ARTIST) {
    trackArtist = String((const char *)text);

    if (trackArtist.length() == 0) {
      trackArtist = "Unknown Artist";
    }
  }

  updateDisplay();
}

// -------------------------
// Bluetooth connection callback
// -------------------------

void connection_state_changed(
  esp_a2d_connection_state_t state,
  void *parameter
) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    isConnected = true;
    trackTitle = "Connected";
    trackArtist = "Press play";
  }

  if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    isConnected = false;
    isPlaying = false;
    trackTitle = "Disconnected";
    trackArtist = "Ready to pair";
  }

  updateDisplay();
}

// -------------------------
// Bluetooth audio callback
// -------------------------

void audio_state_changed(
  esp_a2d_audio_state_t state,
  void *parameter
) {
  isPlaying = (state == ESP_A2D_AUDIO_STATE_STARTED);
  updateDisplay();
}

// -------------------------
// Setup
// -------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {
    Serial.println("OLED initialization failed");

    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(20, 25);
  display.println("NEBULA AUDIO");
  display.display();

  delay(1000);

  updateDisplay();

  // Configure modern AudioTools I2S output
  auto audioConfig = i2s.defaultConfig(TX_MODE);

  audioConfig.pin_bck = SPK_BCLK;
  audioConfig.pin_ws = SPK_WS;
  audioConfig.pin_data = SPK_DOUT;

  audioConfig.sample_rate = 44100;
  audioConfig.bits_per_sample = 16;
  audioConfig.channels = 2;

  if (!i2s.begin(audioConfig)) {
    Serial.println("I2S initialization failed");
  } else {
    Serial.println("I2S initialized");
  }

  // Enable AVRCP track metadata
  a2dp_sink.set_avrc_metadata_attribute_mask(
    ESP_AVRC_MD_ATTR_TITLE |
    ESP_AVRC_MD_ATTR_ARTIST
  );

  a2dp_sink.set_avrc_metadata_callback(
    avrc_metadata_callback
  );

  a2dp_sink.set_on_connection_state_changed(
    connection_state_changed
  );

  a2dp_sink.set_on_audio_state_changed(
    audio_state_changed
  );

  a2dp_sink.start("Nebula CyberDeck");

  Serial.println(
    "Bluetooth A2DP started as: Nebula CyberDeck"
  );

  updateDisplay();
}

// -------------------------
// Main loop
// -------------------------

void loop() {
  bool touching =
    digitalRead(TOUCH_PIN) == TOUCH_ACTIVE;

  // Detect a new touch
  if (touching && !previousTouch) {
    if (isConnected) {
      if (isPlaying) {
        a2dp_sink.pause();
        Serial.println("Music paused");
      } else {
        a2dp_sink.play();
        Serial.println("Music playing");
      }
    }

    delay(200);
  }

  previousTouch = touching;

  delay(50);
}
