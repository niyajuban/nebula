#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>
#include "AudioTools.h"

const char* WIFI_SSID = "IceApple";
const char* WIFI_PASSWORD = "123456789";
const char* SERVER = "http://10.48.221.62:3000";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define SDA_PIN 4
#define SCL_PIN 15
#define TOUCH_PIN 13
#define TOUCH_ACTIVE HIGH

#define MIC_BCLK 26
#define MIC_WS 25
#define MIC_DIN 32
#define SPK_BCLK 14
#define SPK_WS 27
#define SPK_DOUT 33

#define LONG_PRESS_TIME 800UL
#define IDLE_TIME 30000UL
#define SENSOR_INTERVAL 2000UL
#define BMP_RETRY_INTERVAL 5000UL
#define GAME_INTERVAL 90UL
#define CAT_FRAME_TIME 500UL
#define MIC_SAMPLE_RATE 16000
#define TTS_SAMPLE_RATE 24000
#define RECORD_SECONDS 2
#define PCM_SAMPLES (MIC_SAMPLE_RATE * RECORD_SECONDS)
#define WAV_BYTES (44 + PCM_SAMPLES * 2)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BMP085 bmp;
I2SStream micI2S;
I2SStream ttsI2S;

const char* menu[] = {"To-Do", "Pomodoro", "Game", "Weather", "ChatBot"};
const int menuSize = 5;
int selected = 0;

enum Screen { MAIN_MENU, TODO_SCREEN, POMODORO_SCREEN, GAME_SCREEN, WEATHER_SCREEN, CHATBOT_SCREEN, SCREENSAVER };
Screen currentScreen = SCREENSAVER;

bool previousTouch = false;
bool longPressHandled = false;
unsigned long touchStartTime = 0;
unsigned long lastActivityTime = 0;

bool todos[] = {false, false, false};
const char* todoItems[] = {"Build CyberDeck", "Finish task", "Take a break"};
int todoSelected = 0;

enum PomoState { POMO_READY, POMO_RUNNING, POMO_PAUSED, POMO_BREAK_PROMPT, POMO_BREAK_RUNNING };
PomoState pomoState = POMO_READY;
int workMinutes = 25, workSeconds = 0, breakMinutes = 5, breakSeconds = 0, sessionsCompleted = 0;
unsigned long pomoPreviousMillis = 0;

enum GameState { GAME_READY, GAME_RUNNING, GAME_OVER };
GameState gameState = GAME_READY;
const int RACER_LANES = 3;
const int RACER_LANE_X[RACER_LANES] = {28, 64, 100};
int racerPlayerLane = 1, racerObstacleLane = 0, racerObstacleY = 14, racerScore = 0, racerSpeed = 3;
unsigned long lastGameUpdate = 0;

bool bmpAvailable = false;
float temperature = 0.0f, pressure = 0.0f;
unsigned long lastSensorRead = 0, lastBmpRetry = 0;

uint8_t* wavBuffer = nullptr;
bool micReady = false, ttsReady = false;
String transcript;
String chatbotReply = "Tap to ask";

uint8_t catFrame = 0;
unsigned long previousCatFrameMillis = 0;

void drawHeader(const char* title);
void printWrappedText(const String& text, int y, int maxLines);
void drawScreen();
void drawMainMenu();
void drawTodo();
void drawPomodoro();
void drawGame();
void drawWeather();
void drawChatbot();
void drawScreensaver();
void drawCatFrame(uint8_t frame);
void updateSensor();
void updateGame();
void handleTouch();
void handleShortPress();
void handleLongPress();
void resetPomodoro();
void handlePomodoroTouch();
void resetGame();
void handleGameTouch();
void drawTomato(int x, int y);
void drawCar(int x, int y, bool filled);
void drawCatBase(int horizontalShift, int earBounce);
void drawCatEars(int x, int bounce);
void drawLargeEye(int x, int y, int pupilShift, bool excited);
void drawBlinkEye(int x, int y);
void drawHappyEye(int x, int y);
void drawNoseAndMouth(bool extraHappy);
void drawOpenMouth();
void drawWhiskers(int x);
void drawCheeks(int x);
void drawBottomPaws(int x);
void drawPaw(int x, int y);
void drawSparkle(int x, int y);
void drawHeart(int x, int y);
bool connectWiFi();
void sendTelemetry();
void startChatbotHardware();
void stopChatbotHardware();
void writeWavHeader(uint8_t* h, uint32_t pcmBytes);
bool recordWav();
String transcribeWav();
String askChatbot(const String& question);
bool playTts(const String& text);
void runVoiceTurn();
bool isVoiceError(const String& value);

void drawHeader(const char* title) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(title);
  display.drawLine(0, 10, 127, 10, WHITE);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("ERROR: OLED init failed");
    while (true) delay(1000);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(18, 25);
  display.println("CYBERDECK");
  display.display();
  delay(900);

  bmpAvailable = bmp.begin();
  Serial.println(bmpAvailable ? "BMP180 detected" : "BMP180 not detected");
  connectWiFi();
  currentScreen = SCREENSAVER;
  lastActivityTime = millis();
}

void loop() {
  handleTouch();
  updateSensor();
  sendTelemetry();
  if (currentScreen != SCREENSAVER && millis() - lastActivityTime >= IDLE_TIME) {
    stopChatbotHardware();
    currentScreen = SCREENSAVER;
  }
  drawScreen();
  delay(20);
}

void handleTouch() {
  bool touching = digitalRead(TOUCH_PIN) == TOUCH_ACTIVE;
  if (touching && !previousTouch) {
    touchStartTime = millis();
    longPressHandled = false;
    lastActivityTime = millis();
    if (currentScreen == SCREENSAVER) {
      currentScreen = MAIN_MENU;
      previousTouch = touching;
      return;
    }
  }
  if (touching && !longPressHandled && millis() - touchStartTime >= LONG_PRESS_TIME) {
    longPressHandled = true;
    lastActivityTime = millis();
    handleLongPress();
  }
  if (!touching && previousTouch) {
    if (!longPressHandled) handleShortPress();
    lastActivityTime = millis();
  }
  previousTouch = touching;
}

void handleShortPress() {
  if (currentScreen == MAIN_MENU) selected = (selected + 1) % menuSize;
  else if (currentScreen == TODO_SCREEN) {
    todos[todoSelected] = !todos[todoSelected];
    todoSelected = (todoSelected + 1) % 3;
  } else if (currentScreen == POMODORO_SCREEN) handlePomodoroTouch();
  else if (currentScreen == GAME_SCREEN) handleGameTouch();
  else if (currentScreen == WEATHER_SCREEN) { lastSensorRead = 0; updateSensor(); }
  else if (currentScreen == CHATBOT_SCREEN) runVoiceTurn();
}

void handleLongPress() {
  if (currentScreen == MAIN_MENU) {
    switch (selected) {
      case 0: currentScreen = TODO_SCREEN; break;
      case 1: resetPomodoro(); currentScreen = POMODORO_SCREEN; break;
      case 2: resetGame(); currentScreen = GAME_SCREEN; break;
      case 3: currentScreen = WEATHER_SCREEN; break;
      case 4: currentScreen = CHATBOT_SCREEN; startChatbotHardware(); break;
    }
    return;
  }
  if (currentScreen == CHATBOT_SCREEN) stopChatbotHardware();
  currentScreen = MAIN_MENU;
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.print("Connecting Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi failed");
    return false;
  }
  Serial.print("Wi-Fi IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

void startChatbotHardware() {
  if (!wavBuffer) {
    wavBuffer = (uint8_t*)malloc(WAV_BYTES);
    if (!wavBuffer) {
      Serial.println("Voice RAM allocation failed");
      chatbotReply = "RAM allocation failed";
      return;
    }
  }

  if (!micReady) {
    auto mic = micI2S.defaultConfig(RX_MODE);
    mic.port_no = I2S_NUM_1;
    mic.pin_bck = MIC_BCLK;
    mic.pin_ws = MIC_WS;
    mic.pin_data_rx = MIC_DIN;
    mic.sample_rate = MIC_SAMPLE_RATE;
    mic.bits_per_sample = 32;
    mic.channels = 1;
    mic.i2s_format = I2S_STD_FORMAT;
    micReady = micI2S.begin(mic);
  }

  if (!ttsReady) {
    auto speaker = ttsI2S.defaultConfig(TX_MODE);
    speaker.port_no = I2S_NUM_0;
    speaker.pin_bck = SPK_BCLK;
    speaker.pin_ws = SPK_WS;
    speaker.pin_data = SPK_DOUT;
    speaker.sample_rate = TTS_SAMPLE_RATE;
    speaker.bits_per_sample = 16;
    speaker.channels = 1;
    speaker.i2s_format = I2S_STD_FORMAT;
    ttsReady = ttsI2S.begin(speaker);
  }

  Serial.println(micReady ? "Mic ready" : "Mic failed");
  Serial.println(ttsReady ? "Speaker ready" : "Speaker failed");
  chatbotReply = (micReady && ttsReady) ? "Tap to ask" : "Audio init failed";
}

void stopChatbotHardware() {
  if (micReady) micI2S.end();
  if (ttsReady) ttsI2S.end();
  micReady = false;
  ttsReady = false;
  if (wavBuffer) { free(wavBuffer); wavBuffer = nullptr; }
  transcript = "";
  chatbotReply = "Tap to ask";
}

void writeWavHeader(uint8_t* h, uint32_t pcmBytes) {
  uint32_t fileSize = 36 + pcmBytes;
  uint32_t byteRate = MIC_SAMPLE_RATE * 2;
  memset(h, 0, 44);
  memcpy(h, "RIFF", 4);
  h[4] = fileSize & 255; h[5] = (fileSize >> 8) & 255; h[6] = (fileSize >> 16) & 255; h[7] = (fileSize >> 24) & 255;
  memcpy(h + 8, "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);
  h[16] = 16; h[20] = 1; h[22] = 1;
  h[24] = MIC_SAMPLE_RATE & 255; h[25] = (MIC_SAMPLE_RATE >> 8) & 255;
  h[26] = (MIC_SAMPLE_RATE >> 16) & 255; h[27] = (MIC_SAMPLE_RATE >> 24) & 255;
  h[28] = byteRate & 255; h[29] = (byteRate >> 8) & 255;
  h[30] = (byteRate >> 16) & 255; h[31] = (byteRate >> 24) & 255;
  h[32] = 2; h[34] = 16;
  memcpy(h + 36, "data", 4);
  h[40] = pcmBytes & 255; h[41] = (pcmBytes >> 8) & 255;
  h[42] = (pcmBytes >> 16) & 255; h[43] = (pcmBytes >> 24) & 255;
}

bool recordWav() {
  if (!micReady || !wavBuffer) return false;
  writeWavHeader(wavBuffer, PCM_SAMPLES * 2);
  int16_t* output = (int16_t*)(wavBuffer + 44);
  int32_t raw[256];
  size_t samples = 0;
  while (samples < PCM_SAMPLES) {
    size_t bytesRead = micI2S.readBytes((uint8_t*)raw, sizeof(raw));
    if (bytesRead == 0) return false;
    size_t count = bytesRead / sizeof(int32_t);
    for (size_t i = 0; i < count && samples < PCM_SAMPLES; i++) output[samples++] = (int16_t)(raw[i] >> 14);
  }
  return true;
}

bool isVoiceError(const String& value) {
  return value == "Wi-Fi failed" || value == "Upload failed" || value == "Bad STT JSON" ||
         value == "No speech heard" || value.indexOf("STT HTTP") >= 0 || value.indexOf("failed") >= 0;
}

String transcribeWav() {
  if (!connectWiFi()) return "Wi-Fi failed";
  HTTPClient http;
  http.setReuse(false);
  http.begin(String(SERVER) + "/transcribe");
  http.addHeader("Content-Type", "audio/wav");
  http.addHeader("Connection", "close");
  http.setTimeout(60000);
  Serial.print("Uploading WAV bytes: ");
  Serial.println(WAV_BYTES);
  int code = http.POST(wavBuffer, WAV_BYTES);
  String body = http.getString();
  http.end();
  Serial.print("Transcribe HTTP code: ");
  Serial.println(code);
  if (body.length()) Serial.println(body);
  if (code <= 0) return "Upload failed";
  if (code != 200) return "STT HTTP " + String(code);
  StaticJsonDocument<2048> json;
  if (deserializeJson(json, body)) return "Bad STT JSON";
  if (!(json["ok"] | false)) return String(json["error"] | "STT failed");
  String text = String(json["transcript"] | "");
  return text.length() ? text : "No speech heard";
}

String askChatbot(const String& question) {
  if (!connectWiFi()) return "Wi-Fi failed";
  HTTPClient http;
  http.setReuse(false);
  http.begin(String(SERVER) + "/ask");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setTimeout(30000);
  StaticJsonDocument<512> request;
  request["question"] = question;
  String requestBody;
  serializeJson(request, requestBody);
  int code = http.POST(requestBody);
  String body = http.getString();
  http.end();
  if (code != 200) {
    Serial.printf("Ask HTTP %d: %s\n", code, body.c_str());
    return "Ask HTTP " + String(code);
  }
  StaticJsonDocument<2048> json;
  if (deserializeJson(json, body)) return "Bad chat JSON";
  if (!(json["ok"] | false)) return String(json["error"] | "Chat failed");
  String answer = String(json["reply"] | "");
  return answer.length() ? answer : "Empty reply";
}

bool playTts(const String& text) {
  if (!ttsReady || !connectWiFi()) return false;
  HTTPClient http;
  http.setReuse(false);
  http.begin(String(SERVER) + "/tts");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setTimeout(60000);
  StaticJsonDocument<512> request;
  request["text"] = text;
  String requestBody;
  serializeJson(request, requestBody);
  int code = http.POST(requestBody);
  if (code != 200) {
    String error = http.getString();
    Serial.printf("TTS HTTP %d: %s\n", code, error.c_str());
    http.end();
    return false;
  }
  int totalBytes = http.getSize();
  if (totalBytes <= 44) {
    Serial.println("TTS response too short");
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  uint8_t header[44];
  size_t received = 0;
  while (received < sizeof(header)) {
    int n = stream->readBytes(header + received, sizeof(header) - received);
    if (n <= 0) { http.end(); return false; }
    received += n;
  }
  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    Serial.println("TTS response is not WAV");
    http.end();
    return false;
  }
  uint8_t audio[1024];
  int remaining = totalBytes - 44;
  while (remaining > 0) {
    int wanted = min((int)sizeof(audio), remaining);
    int n = stream->readBytes(audio, wanted);
    if (n <= 0) break;
    ttsI2S.write(audio, n);
    remaining -= n;
  }
  ttsI2S.flush();
  http.end();
  return remaining == 0;
}

void runVoiceTurn() {
  if (!micReady || !ttsReady || !wavBuffer) { chatbotReply = "Audio not ready"; return; }
  chatbotReply = "Listening...";
  drawChatbot();
  if (!recordWav()) { chatbotReply = "Mic read failed"; drawChatbot(); delay(1500); return; }
  chatbotReply = "Transcribing...";
  drawChatbot();
  transcript = transcribeWav();
  if (isVoiceError(transcript)) { chatbotReply = transcript; drawChatbot(); delay(1800); return; }
  drawHeader("VOICE CHATBOT");
  display.setCursor(0, 14);
  display.println("You said:");
  printWrappedText(transcript, 26, 2);
  display.display();
  delay(600);
  chatbotReply = "Thinking...";
  drawChatbot();
  String answer = askChatbot(transcript);
  if (answer.length() == 0 || answer.indexOf("HTTP") >= 0 || answer.indexOf("failed") >= 0 || answer.indexOf("error") >= 0) {
    chatbotReply = answer.length() ? answer : "Chat failed";
    drawChatbot();
    delay(1800);
    return;
  }
  chatbotReply = answer;
  drawChatbot();
  delay(250);
  if (!playTts(answer)) {
    chatbotReply = "TTS playback failed";
    drawChatbot();
    delay(1800);
    return;
  }
  chatbotReply = "Tap to ask again";
  drawChatbot();
}

unsigned long lastTelemetrySync = 0;
void sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastTelemetrySync < 4000) return;
  lastTelemetrySync = millis();

  HTTPClient http;
  http.setReuse(false);
  http.begin(String(SERVER) + "/api/telemetry");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setTimeout(1500);

  StaticJsonDocument<256> doc;
  doc["temperatureC"] = temperature;
  doc["pressureHpa"]  = pressure;
  doc["bmpAvailable"] = bmpAvailable;
  doc["rssi"]         = WiFi.RSSI();
  doc["freeHeap"]     = ESP.getFreeHeap();
  if (selected >= 0 && selected < menuSize) {
    doc["screen"] = menu[selected];
  }

  String payload;
  serializeJson(doc, payload);
  http.POST(payload);
  http.end();
}

void updateSensor() {
  if (!bmpAvailable) {
    if (millis() - lastBmpRetry >= BMP_RETRY_INTERVAL) { lastBmpRetry = millis(); bmpAvailable = bmp.begin(); }
    return;
  }
  if (millis() - lastSensorRead < SENSOR_INTERVAL) return;
  lastSensorRead = millis();
  float t = bmp.readTemperature();
  float p = bmp.readPressure() / 100.0f;
  if (isnan(t) || t < -40 || t > 85 || p < 300 || p > 1100) { bmpAvailable = false; lastBmpRetry = millis(); return; }
  temperature = t;
  pressure = p;
}

void drawMainMenu() {
  drawHeader("CYBERDECK");
  int first = selected - 2;
  if (first < 0) first = 0;
  if (first > menuSize - 5) first = menuSize - 5;
  for (int i = 0; i < 5; i++) {
    int index = first + i;
    if (index >= menuSize) break;
    int y = 14 + i * 9;
    if (index == selected) { display.fillRect(0, y - 1, 128, 10, WHITE); display.setTextColor(BLACK); }
    else display.setTextColor(WHITE);
    display.setCursor(5, y);
    display.print(menu[index]);
  }
  display.setTextColor(WHITE);
  display.setCursor(0, 56);
  display.print("Tap:Next Hold:Enter");
  display.display();
}

void drawTodo() {
  drawHeader("TO-DO");
  for (int i = 0; i < 3; i++) {
    int y = 17 + i * 12;
    if (i == todoSelected) { display.fillRect(0, y - 1, 128, 10, WHITE); display.setTextColor(BLACK); }
    else display.setTextColor(WHITE);
    display.setCursor(2, y);
    display.print(todos[i] ? "[x] " : "[ ] ");
    display.print(todoItems[i]);
  }
  display.setTextColor(WHITE);
  display.setCursor(0, 55);
  display.print("Tap:Done Hold:Back");
  display.display();
}

void drawTomato(int x, int y) {
  display.drawCircle(x, y + 3, 4, WHITE);
  display.drawLine(x - 3, y + 1, x, y - 2, WHITE);
  display.drawLine(x, y - 2, x + 3, y + 1, WHITE);
  display.drawLine(x, y - 2, x, y - 5, WHITE);
  display.drawLine(x, y - 4, x - 3, y - 5, WHITE);
  display.drawLine(x, y - 4, x + 3, y - 5, WHITE);
}

void resetPomodoro() {
  pomoState = POMO_READY; workMinutes = 25; workSeconds = 0; breakMinutes = 5; breakSeconds = 0;
  sessionsCompleted = 0; pomoPreviousMillis = millis();
}

void handlePomodoroTouch() {
  if (pomoState == POMO_BREAK_PROMPT) { breakMinutes = 5; breakSeconds = 0; pomoPreviousMillis = millis(); pomoState = POMO_BREAK_RUNNING; }
  else if (pomoState == POMO_READY) { pomoPreviousMillis = millis(); pomoState = POMO_RUNNING; }
  else if (pomoState == POMO_RUNNING || pomoState == POMO_BREAK_RUNNING) pomoState = POMO_PAUSED;
  else { pomoPreviousMillis = millis(); pomoState = POMO_RUNNING; }
}

void drawPomodoro() {
  unsigned long now = millis();
  if (pomoState == POMO_RUNNING && now - pomoPreviousMillis >= 1000) {
    pomoPreviousMillis = now;
    if (workSeconds == 0) { if (workMinutes > 0) { workMinutes--; workSeconds = 59; } else { sessionsCompleted++; pomoState = POMO_BREAK_PROMPT; } }
    else workSeconds--;
  }
  if (pomoState == POMO_BREAK_RUNNING && now - pomoPreviousMillis >= 1000) {
    pomoPreviousMillis = now;
    if (breakSeconds == 0) { if (breakMinutes > 0) { breakMinutes--; breakSeconds = 59; } else { workMinutes = 25; workSeconds = 0; pomoState = POMO_READY; } }
    else breakSeconds--;
  }
  drawHeader("POMODORO");
  display.setCursor(77, 0); display.print(sessionsCompleted); drawTomato(119, 5);
  if (pomoState == POMO_BREAK_PROMPT) {
    display.setCursor(11, 22); display.println("SESSION COMPLETE!");
    display.setCursor(20, 36); display.println("Tap: 5 min break");
    display.setCursor(0, 55); display.print("Hold = Back");
  } else {
    int mins = pomoState == POMO_BREAK_RUNNING ? breakMinutes : workMinutes;
    int secs = pomoState == POMO_BREAK_RUNNING ? breakSeconds : workSeconds;
    char timerText[6]; snprintf(timerText, sizeof(timerText), "%02d:%02d", mins, secs);
    display.setTextSize(3); display.setCursor(18, 27); display.println(timerText);
    display.setTextSize(1); display.setCursor(0, 55);
    if (pomoState == POMO_READY) display.print("Tap = Start");
    else if (pomoState == POMO_PAUSED) display.print("Paused");
    else if (pomoState == POMO_BREAK_RUNNING) display.print("Break running");
    else display.print("Running");
  }
  display.display();
}

void resetGame() {
  gameState = GAME_READY; racerPlayerLane = 1; racerObstacleLane = random(0, RACER_LANES);
  racerObstacleY = 14; racerScore = 0; racerSpeed = 3; lastGameUpdate = millis();
}

void handleGameTouch() {
  if (gameState == GAME_OVER) { resetGame(); return; }
  if (gameState == GAME_READY) { gameState = GAME_RUNNING; return; }
  racerPlayerLane = (racerPlayerLane + 1) % RACER_LANES;
}

void updateGame() {
  if (gameState != GAME_RUNNING || millis() - lastGameUpdate < GAME_INTERVAL) return;
  lastGameUpdate = millis();
  racerObstacleY += racerSpeed;
  if (racerObstacleY >= 50) {
    if (racerObstacleLane == racerPlayerLane) { gameState = GAME_OVER; return; }
    racerScore++; racerObstacleLane = random(0, RACER_LANES); racerObstacleY = 14;
    if (racerScore % 5 == 0 && racerSpeed < 7) racerSpeed++;
  }
}

void drawCar(int x, int y, bool filled) {
  if (filled) {
    display.fillRoundRect(x - 6, y - 5, 12, 12, 2, WHITE); display.fillRect(x - 8, y - 2, 16, 6, WHITE);
    display.fillCircle(x - 6, y + 6, 2, WHITE); display.fillCircle(x + 6, y + 6, 2, WHITE);
  } else {
    display.drawRoundRect(x - 6, y - 5, 12, 12, 2, WHITE); display.drawRect(x - 8, y - 2, 16, 6, WHITE);
    display.fillCircle(x - 6, y + 6, 2, WHITE); display.fillCircle(x + 6, y + 6, 2, WHITE);
  }
}

void drawGame() {
  updateGame();
  drawHeader("MICRO RACER");
  display.setCursor(93, 0); display.print(racerScore);
  if (gameState == GAME_READY) {
    display.setCursor(20, 23); display.println("DODGE THE CARS");
    display.setCursor(15, 37); display.println("Tap: Start / Lane");
    display.setCursor(5, 55); display.println("Hold = Back");
    display.display(); return;
  }
  display.drawLine(10, 12, 10, 63, WHITE); display.drawLine(118, 12, 118, 63, WHITE);
  display.drawLine(46, 12, 46, 63, WHITE); display.drawLine(82, 12, 82, 63, WHITE);
  for (int y = 14; y < 54; y += 12) {
    display.drawLine(27, y, 27, y + 5, WHITE); display.drawLine(64, y, 64, y + 5, WHITE); display.drawLine(101, y, 101, y + 5, WHITE);
  }
  drawCar(RACER_LANE_X[racerObstacleLane], racerObstacleY, false);
  drawCar(RACER_LANE_X[racerPlayerLane], 51, true);
  display.setCursor(0, 55); display.print("Tap: Lane");
  if (gameState == GAME_OVER) {
    display.fillRect(15, 21, 98, 28, BLACK); display.drawRect(15, 21, 98, 28, WHITE);
    display.setCursor(33, 26); display.println("CRASHED!"); display.setCursor(20, 39); display.println("Tap: Retry");
  }
  display.display();
}

void drawWeather() {
  drawHeader("ENVIRONMENT");
  if (!bmpAvailable) display.setCursor(17, 28), display.println("BMP180 NOT FOUND");
  else {
    display.setCursor(2, 18); display.println("TEMP"); display.setTextSize(2); display.setCursor(2, 30);
    display.print(temperature, 1); display.print("C"); display.setTextSize(1);
    display.setCursor(78, 18); display.println("PRESSURE"); display.setCursor(78, 31); display.print(pressure, 0); display.println("hPa");
  }
  display.setCursor(0, 55); display.print("Tap:Refresh Hold:Back");
  display.display();
}

void drawChatbot() {
  drawHeader("VOICE CHATBOT");
  printWrappedText(chatbotReply, 16, 3);
  display.setCursor(0, 55); display.print("Tap:Ask Hold:Back");
  display.display();
}

void drawScreen() {
  switch (currentScreen) {
    case MAIN_MENU: drawMainMenu(); break;
    case TODO_SCREEN: drawTodo(); break;
    case POMODORO_SCREEN: drawPomodoro(); break;
    case GAME_SCREEN: drawGame(); break;
    case WEATHER_SCREEN: drawWeather(); break;
    case CHATBOT_SCREEN: drawChatbot(); break;
    case SCREENSAVER: drawScreensaver(); break;
  }
}

void drawScreensaver() {
  if (millis() - previousCatFrameMillis >= CAT_FRAME_TIME) { previousCatFrameMillis = millis(); catFrame = (catFrame + 1) % 6; }
  drawCatFrame(catFrame);
}

void drawCatFrame(uint8_t frame) {
  display.clearDisplay();
  switch (frame) {
    case 0: drawCatBase(-1, 0); drawLargeEye(40, 41, -4, false); drawLargeEye(88, 41, -4, false); drawNoseAndMouth(false); drawHeart(12, 17); break;
    case 1: drawCatBase(0, -1); drawLargeEye(40, 41, -2, false); drawLargeEye(88, 41, -2, false); drawNoseAndMouth(false); drawSparkle(116, 18); break;
    case 2: drawCatBase(0, 0); drawBlinkEye(40, 42); drawBlinkEye(88, 42); drawNoseAndMouth(false); drawSparkle(13, 25); drawSparkle(115, 25); break;
    case 3: drawCatBase(1, 0); drawLargeEye(40, 41, 2, false); drawLargeEye(88, 41, 2, false); drawNoseAndMouth(false); drawHeart(116, 17); break;
    case 4: drawCatBase(1, 1); drawLargeEye(40, 40, 4, true); drawLargeEye(88, 40, 4, true); drawOpenMouth(); drawSparkle(8, 17); drawSparkle(120, 17); drawSparkle(5, 34); drawSparkle(123, 34); break;
    case 5: drawCatBase(0, 0); drawHappyEye(40, 42); drawHappyEye(88, 42); drawNoseAndMouth(true); drawHeart(64, 15); break;
  }
  display.setTextColor(WHITE); display.setTextSize(1);
  display.setCursor(0, 0); display.print("NEBULA"); drawSparkle(72, 3); drawSparkle(82, 6);
  display.setCursor(91, 0); display.print("*2026"); display.setCursor(28, 55); display.print("Tap to start");
  display.display();
}

void drawCatBase(int horizontalShift, int earBounce) {
  int x = horizontalShift;
  display.drawCircle(64 + x, 88, 56, WHITE);
  display.drawLine(10 + x, 62, 13 + x, 51, WHITE); display.drawLine(13 + x, 51, 18 + x, 42, WHITE);
  display.drawLine(118 + x, 62, 115 + x, 51, WHITE); display.drawLine(115 + x, 51, 110 + x, 42, WHITE);
  drawCatEars(x, earBounce); drawWhiskers(x); drawCheeks(x); drawBottomPaws(x);
}

void drawCatEars(int x, int bounce) {
  int leftTipX = 29 + x, leftTipY = 5 + bounce, rightTipX = 99 + x, rightTipY = 5 + bounce;
  display.drawLine(18 + x, 39, leftTipX, leftTipY, WHITE); display.drawLine(leftTipX, leftTipY, 52 + x, 25, WHITE);
  display.drawLine(76 + x, 25, rightTipX, rightTipY, WHITE); display.drawLine(rightTipX, rightTipY, 110 + x, 39, WHITE);
  display.drawLine(52 + x, 25, 58 + x, 22, WHITE); display.drawLine(58 + x, 22, 70 + x, 22, WHITE); display.drawLine(70 + x, 22, 76 + x, 25, WHITE);
  display.drawLine(24 + x, 31, leftTipX + 2, leftTipY + 8, WHITE); display.drawLine(leftTipX + 2, leftTipY + 8, 43 + x, 26, WHITE);
  display.drawLine(85 + x, 26, rightTipX - 2, rightTipY + 8, WHITE); display.drawLine(rightTipX - 2, rightTipY + 8, 104 + x, 31, WHITE);
}

void drawLargeEye(int x, int y, int pupilShift, bool excited) {
  display.fillCircle(x, y - 2, 12, WHITE); display.fillCircle(x, y + 4, 10, WHITE); display.fillCircle(x + pupilShift, y + 2, 8, BLACK);
  display.fillCircle(x + pupilShift - 3, y - 2, 3, WHITE); display.fillCircle(x + pupilShift + 3, y + 6, 1, WHITE);
  if (excited) { display.drawLine(x + pupilShift - 5, y + 2, x + pupilShift + 5, y + 2, WHITE); display.drawLine(x + pupilShift, y - 3, x + pupilShift, y + 7, WHITE); }
}

void drawBlinkEye(int x, int y) {
  display.drawLine(x - 11, y, x - 6, y - 3, WHITE); display.drawLine(x - 6, y - 3, x, y + 1, WHITE);
  display.drawLine(x, y + 1, x + 6, y - 3, WHITE); display.drawLine(x + 6, y - 3, x + 11, y, WHITE);
}

void drawHappyEye(int x, int y) {
  display.drawLine(x - 10, y, x - 5, y - 4, WHITE); display.drawLine(x - 5, y - 4, x, y - 1, WHITE);
  display.drawLine(x, y - 1, x + 5, y - 4, WHITE); display.drawLine(x + 5, y - 4, x + 10, y, WHITE);
}

void drawNoseAndMouth(bool extraHappy) {
  display.fillTriangle(60, 51, 68, 51, 64, 55, WHITE); display.drawLine(64, 55, 61, 58, WHITE); display.drawLine(61, 58, 57, 56, WHITE);
  display.drawLine(64, 55, 67, 58, WHITE); display.drawLine(67, 58, 71, 56, WHITE);
  if (extraHappy) { display.drawLine(57, 56, 55, 54, WHITE); display.drawLine(71, 56, 73, 54, WHITE); }
}

void drawOpenMouth() { display.fillCircle(64, 55, 7, WHITE); display.fillCircle(64, 56, 3, BLACK); display.fillTriangle(61, 49, 67, 49, 64, 52, WHITE); }

void drawWhiskers(int x) {
  display.drawLine(29 + x, 45, 3 + x, 40, WHITE); display.drawLine(28 + x, 50, 1 + x, 50, WHITE); display.drawLine(29 + x, 55, 4 + x, 61, WHITE);
  display.drawLine(99 + x, 45, 125 + x, 40, WHITE); display.drawLine(100 + x, 50, 127 + x, 50, WHITE); display.drawLine(99 + x, 55, 124 + x, 61, WHITE);
}

void drawCheeks(int x) { display.fillRoundRect(25 + x, 51, 12, 4, 2, WHITE); display.fillRoundRect(91 + x, 51, 12, 4, 2, WHITE); }
void drawBottomPaws(int x) { drawPaw(18 + x, 63); drawPaw(110 + x, 63); }
void drawPaw(int x, int y) { display.drawCircle(x, y, 9, WHITE); display.fillCircle(x - 4, y - 3, 2, WHITE); display.fillCircle(x, y - 5, 2, WHITE); display.fillCircle(x + 4, y - 3, 2, WHITE); }
void drawSparkle(int x, int y) { display.drawLine(x - 3, y, x + 3, y, WHITE); display.drawLine(x, y - 3, x, y + 3, WHITE); }
void drawHeart(int x, int y) { display.fillCircle(x - 3, y, 3, WHITE); display.fillCircle(x + 3, y, 3, WHITE); display.fillTriangle(x - 6, y + 1, x + 6, y + 1, x, y + 8, WHITE); }

void printWrappedText(const String& text, int y, int maxLines) {
  const int charsPerLine = 21;
  int start = 0;
  for (int line = 0; line < maxLines && start < text.length(); line++) {
    int end = min(start + charsPerLine, (int)text.length());
    if (end < text.length()) { int space = text.lastIndexOf(' ', end); if (space > start) end = space; }
    display.setCursor(0, y + line * 10); display.println(text.substring(start, end));
    start = end;
    while (start < text.length() && text.charAt(start) == ' ') start++;
  }
}