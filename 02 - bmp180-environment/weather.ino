#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>

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
#define CAT_FRAME_TIME 500UL

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BMP085 bmp;

enum Screen { SCREENSAVER, SENSOR_SCREEN };
Screen currentScreen = SCREENSAVER;

bool previousTouch = false;
bool longPressHandled = false;
unsigned long touchStartTime = 0;
unsigned long lastActivityTime = 0;

bool bmpAvailable = false;
float temperature = 0.0f, pressure = 0.0f;
unsigned long lastSensorRead = 0, lastBmpRetry = 0;

uint8_t catFrame = 0;
unsigned long previousCatFrameMillis = 0;

void drawHeader(const char* title);
void drawScreen();
void drawScreensaver();
void drawSensorScreen();
void drawCatFrame(uint8_t frame);
void updateSensor();
void handleTouch();
void handleShortPress();
void handleLongPress();

// Cat drawing functions (same as before)
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

  currentScreen = SCREENSAVER;
  lastActivityTime = millis();
}

void loop() {
  handleTouch();
  updateSensor();

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
      currentScreen = SENSOR_SCREEN;
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
  if (currentScreen == SENSOR_SCREEN) {
    lastSensorRead = 0;
    updateSensor();
  }
}

void handleLongPress() {
  currentScreen = SCREENSAVER;
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
    case SCREENSAVER: drawScreensaver(); break;
    case SENSOR_SCREEN: drawSensorScreen(); break;
  }
}

void drawScreensaver() {
  if (millis() - previousCatFrameMillis >= CAT_FRAME_TIME) {
    previousCatFrameMillis = millis();
    catFrame = (catFrame + 1) % 6;
  }
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

void drawSensorScreen() {
  drawHeader("ENVIRONMENT");
  if (!bmpAvailable) {
    display.setCursor(17, 28);
    display.println("BMP180 NOT FOUND");
  } else {
    display.setCursor(2, 18); display.println("TEMP"); display.setTextSize(2); display.setCursor(2, 30);
    display.print(temperature, 1); display.print("C"); display.setTextSize(1);
    display.setCursor(78, 18); display.println("PRESSURE"); display.setCursor(78, 31); display.print(pressure, 0); display.println("hPa");
  }
  display.setCursor(0, 55); display.print("Tap:Refresh Hold:Back");
  display.display();
}

// === Cat drawing functions (same as in file 1) ===

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
