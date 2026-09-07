#include <Arduino.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "esp_bt.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"

// ---------------- Speaker I2S pins (same as CyberDeck.ino MUSIC_* pins) ----------------
#define MUSIC_BCLK 14
#define MUSIC_WS   27
#define MUSIC_DOUT 33

I2SStream musicI2S;
BluetoothA2DPSink a2dpSink(musicI2S);

bool phoneConnected = false;
bool phonePlaying = false;

void btConnectionStateCallback(esp_a2d_connection_state_t state, void* ptr) {
  (void)ptr;
  switch (state) {
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
      phoneConnected = true;
      Serial.println("A2DP: PHONE CONNECTED");
      break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
      phoneConnected = false;
      phonePlaying = false;
      Serial.println("A2DP: PHONE DISCONNECTED");
      break;
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
      Serial.println("A2DP: CONNECTING");
      break;
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
      Serial.println("A2DP: DISCONNECTING");
      break;
  }
}

void btAudioStateCallback(esp_a2d_audio_state_t state, void* ptr) {
  (void)ptr;
  phonePlaying = (state == ESP_A2D_AUDIO_STATE_STARTED);
  Serial.print("A2DP audio state: ");
  Serial.println(phonePlaying ? "PLAYING" : "STOPPED/PAUSED");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- Bluetooth speaker test ---");

  auto config = musicI2S.defaultConfig();
  config.pin_bck = MUSIC_BCLK;
  config.pin_ws = MUSIC_WS;
  config.pin_data = MUSIC_DOUT;
  config.sample_rate = 44100;
  config.channels = 2;
  config.bits_per_sample = 16;

  if (!musicI2S.begin(config)) {
    Serial.println("ERROR: I2S output init failed — check wiring/pins");
    while (true) delay(1000);
  }
  Serial.println("I2S output ready");

  a2dpSink.set_auto_reconnect(false);
  a2dpSink.set_on_connection_state_changed(btConnectionStateCallback);
  a2dpSink.set_on_audio_state_changed(btAudioStateCallback);
  a2dpSink.start("CyberDeck-Music", false);
  delay(500);

  esp_err_t result = esp_bt_gap_set_scan_mode(
    ESP_BT_CONNECTABLE,
    ESP_BT_GENERAL_DISCOVERABLE
  );
  Serial.printf("Discoverable result: %d (0 = OK)\n", result);
  Serial.println("Pair your phone with: CyberDeck-Music");
  Serial.println("Then play any audio — it should come out the speaker.");
}

void loop() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 3000) {
    lastPrint = millis();
    Serial.print("Status -> connected: ");
    Serial.print(phoneConnected ? "yes" : "no");
    Serial.print(", playing: ");
    Serial.println(phonePlaying ? "yes" : "no");
  }
  delay(20);
}
