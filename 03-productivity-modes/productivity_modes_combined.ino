#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>

const char* WIFI_SSID = "<wifi name>";
const char* WIFI_PASSWORD = "<wifi password>";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define SDA_PIN 4
#define SCL_PIN 15
#define TOUCH_PIN 13
#define TOUCH_ACTIVE HIGH

#define LONG_PRESS_TIME 800UL
#define IDLE_TIME 30000UL
#define SENSOR_INTERVAL 2000UL
#define BMP_RETRY_INTERVAL 5000UL
#define GAME_INTERVAL 90UL
#define CAT_FRAME_TIME 500UL

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BMP085 bmp;

const char* menu[] = {"To-Do", "Pomodoro", "Game", "Weather"};
const int menuSize = 4;
int selected = 0;

enum Screen { MAIN_MENU, TODO_SCREEN, POMODORO_SCREEN, GAME_SCREEN, WEATHER_SCREEN, SCREENSAVER };
Screen currentScreen = SCREENSAVER;

bool previousTouch = false;
bool longPressHandled = false;
unsigned long touchStartTime = 0;
unsigned long lastActivityTime = 0;

struct TodoItem {
  int id;
  char text[22];
  bool completed;
};
TodoItem todoList[6];
int todoCount = 0;
int todoSelected = 0;

String pomoServerState = "IDLE"; // "IDLE", "RUNNING", "PAUSED", "BREAK"
String pomoServerMode = "WORK";  // "WORK", "BREAK"
int pomoTimeLeft = 25 * 60;
int sessionsCompleted = 0;

enum GameState { GAME_READY, GAME_RUNNING, GAME_OVER };
GameState gameState = GAME_READY;
const int RACER_LANES = 3;
const int RACER_LANE_X[RACER_LANES] = {28, 64, 100};
int racerPlayerLane = 1, racerObstacleLane = 0, racerObstacleY = 14, racerScore = 0, racerSpeed = 3;
unsigned long lastGameUpdate = 0;

bool bmpAvailable = false;
float temperature = 0.0f, pressure = 0.0f;
unsigned long lastSensorRead = 0, lastBmpRetry = 0;

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

// Cat drawing helpers
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

// Stubbed server functions (no real HTTP in this stage)
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

void sendTelemetry() {
  // No server in this stage; do nothing.
}

void toggleTodoRemote(int /*id*/) {
  // No server in this stage; do nothing.
}

void sendPomoActionRemote(const char* /*act*/) {
  // No server in this stage; do nothing.
}

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

  connectWiFi(); // optional, just to init WiFi

  // Seed some dummy todos for testing
  todoCount = 2;
  todoList[0].id = 1; strncpy(todoList[0].text, "Test task 1", 21); todoList[0].completed = false;
  todoList[1].id = 2; strncpy(todoList[1].text, "Test task 2", 21); todoList[1].completed = false;
  todoSelected = 0;

  currentScreen = SCREENSAVER;
  lastActivityTime = millis();
}

void loop() {
  handleTouch();
  updateSensor();
  sendTelemetry();

  if (currentScreen != SCREENSAVER && millis() - lastActivityTime >= IDLE_TIME) {
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
    if (todoCount > 0) {
      todoList[todoSelected].completed = !todoList[todoSelected].completed;
      toggleTodoRemote(todoList[todoSelected].id);
      todoSelected = (todoSelected + 1) % todoCount;
    }
  } else if (currentScreen == POMODORO_SCREEN) handlePomodoroTouch();
  else if (currentScreen == GAME_SCREEN) handleGameTouch();
  else if (currentScreen == WEATHER_SCREEN) { lastSensorRead = 0; updateSensor(); }
}

void handleLongPress() {
  if (currentScreen == MAIN_MENU) {
    switch (selected) {
      case 0: currentScreen = TODO_SCREEN; break;
      case 1: currentScreen = POMODORO_SCREEN; break;
      case 2: resetGame(); currentScreen = GAME_SCREEN; break;
      case 3: currentScreen = WEATHER_SCREEN; break;
    }
    return;
  }
  currentScreen = MAIN_MENU;
}

void resetPomodoro() {
  pomoServerState = "IDLE";
  pomoServerMode = "WORK";
  pomoTimeLeft = 25 * 60;
  sessionsCompleted = 0;
}

void handlePomodoroTouch() {
  if (pomoServerState == "RUNNING" || pomoServerState == "BREAK") {
    pomoServerState = "PAUSED";
    sendPomoActionRemote("pause");
  } else {
    pomoServerState = "RUNNING";
    sendPomoActionRemote("start");
  }
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

void updateSensor() {
  if (!bmpAvailable) {
    if (millis() - lastBmpRetry >= BMP_RETRY_INTERVAL) {
      lastBmpRetry = millis();
      bmpAvailable = bmp.begin();
    }
    return;
  }
  if (millis() - lastSensorRead < SENSOR_INTERVAL) return;
  lastSensorRead = millis();
  float t = bmp.readTemperature();
  float p = bmp.readPressure() / 100.0f;
  if (isnan(t) || t < -40 || t > 85 || p < 300 || p > 1100) {
    bmpAvailable = false;
    lastBmpRetry = millis();
    return;
  }
  temperature = t;
  pressure = p;
}

void drawScreen() {
  switch (currentScreen) {
    case MAIN_MENU: drawMainMenu(); break;
    case TODO_SCREEN: drawTodo(); break;
    case POMODORO_SCREEN: drawPomodoro(); break;
    case GAME_SCREEN: drawGame(); break;
    case WEATHER_SCREEN: drawWeather(); break;
    case SCREENSAVER: drawScreensaver(); break;
  }
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
  if (todoCount == 0) {
    display.setCursor(14, 25);
    display.println("No tasks yet!");
    display.setCursor(2, 38);
    display.println("Add task on Dash");
  } else {
    int startIdx = 0;
    if (todoSelected >= 3) startIdx = todoSelected - 2;
    for (int i = 0; i < 3 && (startIdx + i) < todoCount; i++) {
      int idx = startIdx + i;
      int y = 16 + i * 13;
      if (idx == todoSelected) {
        display.fillRect(0, y - 1, 128, 11, WHITE);
        display.setTextColor(BLACK);
      } else {
        display.setTextColor(WHITE);
      }
      display.setCursor(2, y);
      display.print(todoList[idx].completed ? "[x] " : "[ ] ");
      display.print(todoList[idx].text);
    }
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

void drawPomodoro() {
  static unsigned long lastLocalPomoTick = 0;
  if ((pomoServerState == "RUNNING" || pomoServerState == "BREAK") && millis() - lastLocalPomoTick >= 1000) {
    lastLocalPomoTick = millis();
    if (pomoTimeLeft > 0) pomoTimeLeft--;
  }

  drawHeader("POMODORO");
  display.setCursor(77, 0); display.print(sessionsCompleted); drawTomato(119, 5);

  int mins = pomoTimeLeft / 60;
  int secs = pomoTimeLeft % 60;
  char timerText[6]; snprintf(timerText, sizeof(timerText), "%02d:%02d", mins, secs);

  if (pomoServerMode == "BREAK" || pomoServerState == "BREAK") {
    display.setCursor(24, 18); display.println("BREAK TIME!");
    display.setTextSize(2); display.setCursor(35, 31); display.println(timerText);
    display.setTextSize(1); display.setCursor(0, 55);
    if (pomoServerState == "PAUSED") display.print("Paused Hold:Back");
    else display.print("Break Running");
  } else {
    display.setTextSize(3); display.setCursor(18, 22); display.println(timerText);
    display.setTextSize(1); display.setCursor(0, 55);
    if (pomoServerState == "RUNNING") display.print("Running Hold:Back");
    else if (pomoServerState == "PAUSED") display.print("Paused Hold:Back");
    else display.print("Tap:Start Hold:Back");
  }
  display.display();
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

// === Cat drawing functions (same as before) ===

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
