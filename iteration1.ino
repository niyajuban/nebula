#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "esp_bt.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SDA_PIN 21
#define SCL_PIN 22
#define TOUCH_PIN 4
#define TOUCH_ACTIVE HIGH

#define I2S_BCK_PIN 27
#define I2S_WS_PIN 26
#define I2S_DATA_PIN 25

#define LONG_PRESS_TIME 800UL
#define SENSOR_INTERVAL 2000UL
#define BMP_RETRY_INTERVAL 5000UL
#define IDLE_TIME 300000UL
#define GAME_INTERVAL 20UL

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BMP085 bmp;

/*
   Bluetooth section preserved from the working sketch.
*/
I2SStream musicI2S;
BluetoothA2DPSink a2dp_sink(musicI2S);

bool bluetoothStarted = false;
bool phoneConnected = false;
bool phonePlaying = false;

/*
   Sensors.
*/
bool bmpAvailable = false;
float temperature = 0.0f;
float pressure = 0.0f;

unsigned long lastSensorRead = 0;
unsigned long lastBmpRetry = 0;

/*
   Menu and screens.
*/
const char *menu[] = {
  "To-Do",
  "Pomodoro",
  "Game",
  "Pet",
  "Music",
  "E-Reader",
  "ChatBot",
  "Weather"
};

const int menuSize = 8;
int selected = 0;

enum Screen {
  MAIN_MENU,
  TODO_SCREEN,
  POMODORO_SCREEN,
  GAME_SCREEN,
  PET_SCREEN,
  MUSIC_SCREEN,
  EREADER_SCREEN,
  CHATBOT_SCREEN,
  WEATHER_SCREEN,
  SCREENSAVER
};

Screen currentScreen = MAIN_MENU;

/*
   Touch.
*/
bool previousTouch = false;
bool longPressHandled = false;
unsigned long touchStartTime = 0;
unsigned long lastActivityTime = 0;

/*
   Pomodoro.
*/
enum PomoState {
  POMO_READY,
  POMO_RUNNING,
  POMO_PAUSED,
  POMO_BREAK_PROMPT,
  POMO_BREAK_RUNNING
};

PomoState pomoState = POMO_READY;

int workMinutes = 25;
int workSeconds = 0;
int breakMinutes = 5;
int breakSeconds = 0;
int sessionsCompleted = 0;

unsigned long pomoPreviousMillis = 0;

/*
   Game.
*/
enum GameState {
  GAME_READY,
  GAME_RUNNING,
  GAME_OVER
};

GameState gameState = GAME_READY;

float birdY = 30.0f;
float birdVelocity = 0.0f;

const float GRAVITY = 0.20f;
const float FLAP_STRENGTH = -3.5f;

const int PIPE_WIDTH = 12;
const int PIPE_GAP = 27;

float pipeX = 128.0f;
int pipeGapY = 30;
bool pipePassed = false;
int gameScore = 0;

unsigned long lastGameUpdate = 0;

const int TOP_LIMIT = 13;
const int BOTTOM_LIMIT = 59;

/*
   Screensaver.
*/
unsigned long lastAnimation = 0;
bool blink = false;

/*
   Forward declarations.
*/
void setup();
void loop();

void btConnectionStateCallback(
  esp_a2d_connection_state_t state,
  void *ptr
);

void startBluetoothAudio();
void stopBluetoothAudio();
void togglePlayPause();

void handleTouch();
void handleShortPress();
void handleLongPress();

void updateSensor();

void drawScreen();
void drawMainMenu();
void drawMusic();
void drawWeather();
void drawPlaceholder(const char *title);
void drawPomodoro();
void drawGame();
void drawChatbot();
void drawScreensaver();

void resetPomodoro();
void handlePomodoroTouch();

void resetGame();
void handleGameTouch();
void newPipe();
void updateGame();

void bootAnimation();

/*
   Bluetooth logic from the working sketch.
*/
void btConnectionStateCallback(
  esp_a2d_connection_state_t state,
  void *ptr
) {
  (void)ptr;

  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    phoneConnected = true;
    Serial.println("A2DP: PHONE CONNECTED");
  }
  else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    phoneConnected = false;
    phonePlaying = false;
    Serial.println("A2DP: PHONE DISCONNECTED");
  }
  else if (state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
    Serial.println("A2DP: CONNECTING");
  }
  else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTING) {
    Serial.println("A2DP: DISCONNECTING");
  }
}

void startBluetoothAudio() {
  if (bluetoothStarted) {
    return;
  }

  Serial.println("Starting Bluetooth A2DP sink...");

  auto config = musicI2S.defaultConfig();

  config.pin_bck = I2S_BCK_PIN;
  config.pin_ws = I2S_WS_PIN;
  config.pin_data = I2S_DATA_PIN;
  config.sample_rate = 44100;
  config.channels = 2;
  config.bits_per_sample = 16;

  if (!musicI2S.begin(config)) {
    Serial.println("ERROR: I2S output failed");
    return;
  }

  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.set_on_connection_state_changed(
    btConnectionStateCallback
  );

  a2dp_sink.start("CyberDeck-Music", false);

  delay(500);

  esp_err_t result = esp_bt_gap_set_scan_mode(
    ESP_BT_CONNECTABLE,
    ESP_BT_GENERAL_DISCOVERABLE
  );

  Serial.printf("Discoverable result: %d\n", result);
  Serial.println("Pair phone with: CyberDeck-Music");

  bluetoothStarted = true;
  phoneConnected = false;
  phonePlaying = false;
}

void stopBluetoothAudio() {
  if (!bluetoothStarted) {
    return;
  }

  Serial.println("Stopping Bluetooth A2DP...");

  phoneConnected = false;
  phonePlaying = false;

  esp_bt_gap_set_scan_mode(
    ESP_BT_NON_CONNECTABLE,
    ESP_BT_NON_DISCOVERABLE
  );

  a2dp_sink.end(false);

  delay(300);

  musicI2S.end();

  bluetoothStarted = false;

  Serial.println("Bluetooth OFF");
}

void togglePlayPause() {
  if (!bluetoothStarted) {
    return;
  }

  if (!phoneConnected) {
    Serial.println("No phone connected yet");
    return;
  }

  if (phonePlaying) {
    a2dp_sink.pause();
    phonePlaying = false;
    Serial.println("Pause command sent");
  }
  else {
    a2dp_sink.play();
    phonePlaying = true;
    Serial.println("Play command sent");
  }
}

/*
   Setup.
*/
void setup() {
  Serial.begin(115200);
  delay(700);

  Serial.println("=== CYBERDECK START ===");

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED FAILED");

    while (true) {
      delay(100);
    }
  }

  bmpAvailable = bmp.begin();

  if (bmpAvailable) {
    Serial.println("BMP180 detected");
  }
  else {
    Serial.println("BMP180 not detected");
  }

  bootAnimation();

  lastActivityTime = millis();
}

/*
   Main loop.
*/
void loop() {
  handleTouch();
  updateSensor();

  if (
    currentScreen != SCREENSAVER &&
    millis() - lastActivityTime >= IDLE_TIME
  ) {
    if (currentScreen == MUSIC_SCREEN) {
      stopBluetoothAudio();
    }

    currentScreen = SCREENSAVER;
  }

  drawScreen();

  delay(20);
}

/*
   Touch handling.
*/
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

  if (
    touching &&
    !longPressHandled &&
    millis() - touchStartTime >= LONG_PRESS_TIME
  ) {
    longPressHandled = true;
    lastActivityTime = millis();
    handleLongPress();
  }

  if (!touching && previousTouch) {
    if (!longPressHandled) {
      handleShortPress();
    }

    lastActivityTime = millis();
  }

  previousTouch = touching;
}

void handleShortPress() {
  if (currentScreen == MAIN_MENU) {
    selected = (selected + 1) % menuSize;
  }
  else if (currentScreen == POMODORO_SCREEN) {
    handlePomodoroTouch();
  }
  else if (currentScreen == GAME_SCREEN) {
    handleGameTouch();
  }
  else if (currentScreen == MUSIC_SCREEN) {
    togglePlayPause();
  }
  else if (currentScreen == CHATBOT_SCREEN) {
    /*
       Chatbot input can be connected here later.
       For now, this only displays a status message.
    */
    Serial.println("Chatbot short press");
  }
}

void handleLongPress() {
  if (currentScreen == MUSIC_SCREEN) {
    stopBluetoothAudio();
    currentScreen = MAIN_MENU;
    return;
  }

  if (currentScreen == CHATBOT_SCREEN) {
    currentScreen = MAIN_MENU;
    return;
  }

  if (currentScreen == MAIN_MENU) {
    switch (selected) {
      case 0:
        currentScreen = TODO_SCREEN;
        break;

      case 1:
        resetPomodoro();
        currentScreen = POMODORO_SCREEN;
        break;

      case 2:
        resetGame();
        currentScreen = GAME_SCREEN;
        break;

      case 3:
        currentScreen = PET_SCREEN;
        break;

      case 4:
        currentScreen = MUSIC_SCREEN;
        startBluetoothAudio();
        break;

      case 5:
        currentScreen = EREADER_SCREEN;
        break;

      case 6:
        currentScreen = CHATBOT_SCREEN;
        break;

      case 7:
        currentScreen = WEATHER_SCREEN;
        break;
    }

    return;
  }

  currentScreen = MAIN_MENU;
}

/*
   BMP180 update.
*/
void updateSensor() {
  if (!bmpAvailable) {
    if (millis() - lastBmpRetry >= BMP_RETRY_INTERVAL) {
      lastBmpRetry = millis();
      bmpAvailable = bmp.begin();
    }

    return;
  }

  if (millis() - lastSensorRead < SENSOR_INTERVAL) {
    return;
  }

  lastSensorRead = millis();

  float t = bmp.readTemperature();
  float p = bmp.readPressure() / 100.0f;

  if (
    isnan(t) ||
    t < -40.0f ||
    t > 85.0f ||
    p < 300.0f ||
    p > 1100.0f
  ) {
    bmpAvailable = false;
    lastBmpRetry = millis();
    return;
  }

  temperature = t;
  pressure = p;
}

/*
   Screen dispatcher.
*/
void drawScreen() {
  switch (currentScreen) {
    case MAIN_MENU:
      drawMainMenu();
      break;

    case TODO_SCREEN:
      drawPlaceholder("TO-DO");
      break;

    case POMODORO_SCREEN:
      drawPomodoro();
      break;

    case GAME_SCREEN:
      drawGame();
      break;

    case PET_SCREEN:
      drawPlaceholder("PET");
      break;

    case MUSIC_SCREEN:
      drawMusic();
      break;

    case EREADER_SCREEN:
      drawPlaceholder("E-READER");
      break;

    case CHATBOT_SCREEN:
      drawChatbot();
      break;

    case WEATHER_SCREEN:
      drawWeather();
      break;

    case SCREENSAVER:
      drawScreensaver();
      break;
  }
}

/*
   Main menu.
*/
void drawMainMenu() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("CYBERDECK");
  display.drawLine(0, 10, 127, 10, WHITE);

  int firstItem = selected - 2;

  if (firstItem < 0) {
    firstItem = 0;
  }

  if (firstItem > menuSize - 5) {
    firstItem = menuSize - 5;
  }

  for (int i = 0; i < 5; i++) {
    int index = firstItem + i;

    if (index >= menuSize) {
      break;
    }

    int y = 14 + i * 9;

    if (index == selected) {
      display.fillRect(0, y - 1, 128, 10, WHITE);
      display.setTextColor(BLACK);
    }
    else {
      display.setTextColor(WHITE);
    }

    display.setCursor(5, y);
    display.print(menu[index]);
  }

  display.setTextColor(WHITE);
  display.setCursor(0, 56);
  display.print("Tap:Next Hold:Enter");

  display.display();
}

/*
   Music screen.
*/
void drawMusic() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("MUSIC");
  display.drawLine(0, 10, 127, 10, WHITE);

  display.setCursor(0, 16);

  if (!bluetoothStarted) {
    display.println("Bluetooth starting...");
  }
  else if (!phoneConnected) {
    display.println("Pair CyberDeck-Music");
  }
  else if (phonePlaying) {
    display.println("Phone connected: PLAYING");
  }
  else {
    display.println("Phone connected: PAUSED");
  }

  display.setCursor(0, 30);
  display.println("BT audio receiver");

  display.setCursor(0, 43);
  display.println("Tap: Play/Pause");

  display.setCursor(0, 55);
  display.println("Hold: Exit + BT off");

  display.display();
}

/*
   Weather screen.
*/
void drawWeather() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("WEATHER");
  display.drawLine(0, 10, 127, 10, WHITE);

  if (!bmpAvailable) {
    display.setCursor(10, 27);
    display.println("BMP180 ERROR");
  }
  else {
    display.setCursor(3, 17);
    display.println("TEMP");

    display.setTextSize(2);
    display.setCursor(3, 29);
    display.print(temperature, 1);
    display.print(" C");

    display.setTextSize(1);
    display.setCursor(76, 17);
    display.println("PRESSURE");

    display.setCursor(76, 30);
    display.print(pressure, 1);
    display.println(" hPa");
  }

  display.setCursor(5, 55);
  display.println("Hold = Back");

  display.display();
}

/*
   Placeholder modes.
*/
void drawPlaceholder(const char *title) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println(title);
  display.drawLine(0, 10, 127, 10, WHITE);

  display.setCursor(20, 28);
  display.println("Coming Soon");

  display.setCursor(5, 55);
  display.println("Hold = Back");

  display.display();
}

/*
   Chatbot screen.
*/
void drawChatbot() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("CHATBOT");
  display.drawLine(0, 10, 127, 10, WHITE);

  display.setCursor(0, 16);
  display.println("Chatbot mode");

  display.setCursor(0, 29);
  display.println("Network audio");

  display.setCursor(0, 42);
  display.println("Not connected");

  display.setCursor(0, 55);
  display.println("Hold = Back");

  display.display();
}

/*
   Pomodoro.
*/
void resetPomodoro() {
  pomoState = POMO_READY;

  workMinutes = 25;
  workSeconds = 0;

  breakMinutes = 5;
  breakSeconds = 0;

  sessionsCompleted = 0;
  pomoPreviousMillis = millis();
}

void handlePomodoroTouch() {
  if (pomoState == POMO_BREAK_PROMPT) {
    breakMinutes = 5;
    breakSeconds = 0;
    pomoPreviousMillis = millis();
    pomoState = POMO_BREAK_RUNNING;
  }
  else if (pomoState == POMO_READY) {
    pomoPreviousMillis = millis();
    pomoState = POMO_RUNNING;
  }
  else if (
    pomoState == POMO_RUNNING ||
    pomoState == POMO_BREAK_RUNNING
  ) {
    pomoState = POMO_PAUSED;
  }
  else if (pomoState == POMO_PAUSED) {
    pomoPreviousMillis = millis();
    pomoState = POMO_RUNNING;
  }
}

void drawPomodoro() {
  unsigned long now = millis();

  if (
    pomoState == POMO_RUNNING &&
    now - pomoPreviousMillis >= 1000
  ) {
    pomoPreviousMillis = now;

    if (workSeconds == 0) {
      if (workMinutes > 0) {
        workMinutes--;
        workSeconds = 59;
      }
      else {
        sessionsCompleted++;
        pomoState = POMO_BREAK_PROMPT;
      }
    }
    else {
      workSeconds--;
    }
  }

  if (
    pomoState == POMO_BREAK_RUNNING &&
    now - pomoPreviousMillis >= 1000
  ) {
    pomoPreviousMillis = now;

    if (breakSeconds == 0) {
      if (breakMinutes > 0) {
        breakMinutes--;
        breakSeconds = 59;
      }
      else {
        workMinutes = 25;
        workSeconds = 0;
        pomoState = POMO_READY;
      }
    }
    else {
      breakSeconds--;
    }
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("POMODORO ");
  display.print(sessionsCompleted);

  display.drawLine(0, 10, 127, 10, WHITE);

  if (pomoState == POMO_BREAK_PROMPT) {
    display.setCursor(15, 20);
    display.println("SESSION COMPLETE!");

    display.setCursor(22, 35);
    display.println("Tap: 5 min break");

    display.setCursor(5, 55);
    display.println("Hold = Back");

    display.display();
    return;
  }

  int mins;
  int secs;

  if (pomoState == POMO_BREAK_RUNNING) {
    mins = breakMinutes;
    secs = breakSeconds;
  }
  else {
    mins = workMinutes;
    secs = workSeconds;
  }

  char timerText[6];
  snprintf(timerText, sizeof(timerText), "%02d:%02d", mins, secs);

  display.setTextSize(3);
  display.setCursor(18, 27);
  display.println(timerText);

  display.setTextSize(1);
  display.setCursor(0, 55);

  if (pomoState == POMO_READY) {
    display.print("Tap = Start");
  }
  else if (pomoState == POMO_PAUSED) {
    display.print("Paused");
  }
  else if (pomoState == POMO_BREAK_RUNNING) {
    display.print("Break running");
  }
  else {
    display.print("Running");
  }

  display.display();
}

/*
   Game.
*/
void resetGame() {
  gameState = GAME_READY;

  birdY = 30.0f;
  birdVelocity = 0.0f;

  pipeX = 128.0f;
  pipeGapY = 30;

  pipePassed = false;
  gameScore = 0;

  lastGameUpdate = millis();
}

void handleGameTouch() {
  if (gameState == GAME_OVER) {
    resetGame();
    return;
  }

  gameState = GAME_RUNNING;
  birdVelocity = FLAP_STRENGTH;
}

void newPipe() {
  pipeX = 128.0f;
  pipeGapY = random(22, 39);
  pipePassed = false;
}

void updateGame() {
  if (gameState != GAME_RUNNING) {
    return;
  }

  if (millis() - lastGameUpdate < GAME_INTERVAL) {
    return;
  }

  lastGameUpdate = millis();

  birdVelocity += GRAVITY;
  birdY += birdVelocity;
  pipeX -= 1.0f;

  if (!pipePassed && pipeX + PIPE_WIDTH < 30) {
    gameScore++;
    pipePassed = true;
  }

  if (pipeX < -PIPE_WIDTH) {
    newPipe();
  }

  if (birdY < TOP_LIMIT) {
    birdY = TOP_LIMIT;
    birdVelocity = 0;
  }

  if (birdY > BOTTOM_LIMIT) {
    birdY = BOTTOM_LIMIT;
    gameState = GAME_OVER;
    return;
  }

  int birdTop = (int)birdY;
  int birdBottom = birdTop + 4;

  bool horizontalCollision =
    31 > pipeX &&
    27 < pipeX + PIPE_WIDTH;

  bool verticalCollision =
    birdTop < pipeGapY ||
    birdBottom > pipeGapY + PIPE_GAP;

  if (horizontalCollision && verticalCollision) {
    gameState = GAME_OVER;
  }
}

void drawGame() {
  updateGame();

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(2, 2);
  display.print("SCORE: ");
  display.print(gameScore);

  if (gameState == GAME_READY) {
    display.setCursor(32, 22);
    display.println("FLAPPY BIRD");

    display.setCursor(23, 35);
    display.println("Touch to flap!");

    display.setCursor(5, 55);
    display.println("Hold = Back");

    display.display();
    return;
  }

  int birdPosition = (int)birdY;

  display.fillCircle(29, birdPosition, 3, WHITE);

  display.fillRect(
    (int)pipeX,
    12,
    PIPE_WIDTH,
    pipeGapY - 12,
    WHITE
  );

  display.fillRect(
    (int)pipeX,
    pipeGapY + PIPE_GAP,
    PIPE_WIDTH,
    64 - pipeGapY - PIPE_GAP,
    WHITE
  );

  if (gameState == GAME_OVER) {
    display.fillRect(18, 20, 92, 30, BLACK);
    display.drawRect(18, 20, 92, 30, WHITE);

    display.setCursor(40, 25);
    display.println("GAME OVER");

    display.setCursor(28, 44);
    display.println("Tap = Retry");
  }

  display.display();
}

/*
   Screensaver.
*/
void drawScreensaver() {
  if (millis() - lastAnimation >= 1000) {
    lastAnimation = millis();
    blink = !blink;
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(2);
  display.setCursor(42, 2);
  display.println("--:--");

  display.setCursor(42, 23);

  if (blink) {
    display.println("-_-");
  }
  else {
    display.println("^_^");
  }

  display.setTextSize(1);
  display.setCursor(34, 55);
  display.print("Tap to wake!");

  display.display();
}

/*
   Startup animation.
*/
void bootAnimation() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);

  display.setCursor(10, 18);
  display.println("CYBER");

  display.setCursor(20, 42);
  display.println("DECK");

  display.display();

  delay(1200);
}
