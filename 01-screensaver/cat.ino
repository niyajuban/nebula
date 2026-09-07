#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

const uint16_t FRAME_TIME = 500;

uint8_t frameNumber = 0;
unsigned long previousFrameMillis = 0;

void setup() {
  Serial.begin(115200);

  // ESP32 default I2C pins, if required:
   Wire.begin(4,15);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 OLED initialization failed"));
    while (true);
  }

  display.clearDisplay();
  display.display();

  drawFrame(frameNumber);
}

void loop() {
  unsigned long now = millis();

  if (now - previousFrameMillis >= FRAME_TIME) {
    previousFrameMillis = now;

    frameNumber = (frameNumber + 1) % 6;
    drawFrame(frameNumber);
  }
}

/* =================================================
   SIX-FRAME LOOP
   ================================================= */

void drawFrame(uint8_t frame) {
  display.clearDisplay();

  switch (frame) {
    case 0:
      // Large glossy eyes look strongly left
      drawCatBase(-1, 0);
      drawLargeEye(40, 41, -4, 0);
      drawLargeEye(88, 41, -4, 0);
      drawNoseAndMouth(false);
      drawHeart(12, 17);
      break;

    case 1:
      // Eyes travel gently toward the center
      drawCatBase(0, -1);
      drawLargeEye(40, 41, -2, 0);
      drawLargeEye(88, 41, -2, 0);
      drawNoseAndMouth(false);
      drawSparkle(116, 18);
      break;

    case 2:
      // Centered blink
      drawCatBase(0, 0);
      drawBlinkEye(40, 42);
      drawBlinkEye(88, 42);
      drawNoseAndMouth(false);
      drawSparkle(13, 25);
      drawSparkle(115, 25);
      break;

    case 3:
      // Eyes look right
      drawCatBase(1, 0);
      drawLargeEye(40, 41, 2, 0);
      drawLargeEye(88, 41, 2, 0);
      drawNoseAndMouth(false);
      drawHeart(116, 17);
      break;

    case 4:
      // Excited look right, with large sparkle pupils
      drawCatBase(1, 1);
      drawLargeEye(40, 40, 4, 1);
      drawLargeEye(88, 40, 4, 1);
      drawOpenMouth();
      drawSparkle(8, 17);
      drawSparkle(120, 17);
      drawSparkle(5, 34);
      drawSparkle(123, 34);
      break;

    case 5:
      // Relaxed happy face
      drawCatBase(0, 0);
      drawHappyEye(40, 42);
      drawHappyEye(88, 42);
      drawNoseAndMouth(true);
      drawHeart(64, 15);
      break;
  }

  display.display();
}

/* =================================================
   CAT BASE

   Head circle:
   Center (64, 88), radius 56.

   Only its upper arc appears in the 128x64 area.
   The ears overlap it at their base, so their outline
   reads as one connected cat shape.
   ================================================= */

void drawCatBase(int horizontalShift, int earBounce) {
  int x = horizontalShift;

  // Low, wide head dome / outer cheek contour.
  display.drawCircle(64 + x, 88, 56, SSD1306_WHITE);

  // Reinforce visible lower-side contours.
  display.drawLine(10 + x, 62, 13 + x, 51, SSD1306_WHITE);
  display.drawLine(13 + x, 51, 18 + x, 42, SSD1306_WHITE);

  display.drawLine(118 + x, 62, 115 + x, 51, SSD1306_WHITE);
  display.drawLine(115 + x, 51, 110 + x, 42, SSD1306_WHITE);

  // The ears are drawn after the dome so their bases
  // attach visibly to the curve.
  drawCatEars(x, earBounce);

  // Face decorations.
  drawWhiskers(x);
  drawCheeks(x);
  drawBottomPaws(x);
}

/* =================================================
   CONNECTED CAT EARS

   Wide ear bases:
   Left  base:  (20, 35) to (52, 28)
   Right base:  (76, 28) to (108, 35)

   Tall inward-facing ear tips:
   Left tip:    ~ (28, 5)
   Right tip:   ~ (100, 5)
   ================================================= */

void drawCatEars(int x, int bounce) {
  int leftTipX = 29 + x;
  int leftTipY = 5 + bounce;

  int rightTipX = 99 + x;
  int rightTipY = 5 + bounce;

  // Left outer ear:
  // Side of dome -> ear tip -> forehead/crown connection.
  display.drawLine(18 + x, 39, leftTipX, leftTipY, SSD1306_WHITE);
  display.drawLine(leftTipX, leftTipY, 52 + x, 25, SSD1306_WHITE);

  // Right outer ear:
  // Forehead/crown connection -> ear tip -> side of dome.
  display.drawLine(76 + x, 25, rightTipX, rightTipY, SSD1306_WHITE);
  display.drawLine(rightTipX, rightTipY, 110 + x, 39, SSD1306_WHITE);

  // Small central crown line connects both ears.
  display.drawLine(52 + x, 25, 58 + x, 22, SSD1306_WHITE);
  display.drawLine(58 + x, 22, 70 + x, 22, SSD1306_WHITE);
  display.drawLine(70 + x, 22, 76 + x, 25, SSD1306_WHITE);

  // Left inner ear: a smaller nested triangle.
  display.drawLine(24 + x, 31, leftTipX + 2, leftTipY + 8, SSD1306_WHITE);
  display.drawLine(leftTipX + 2, leftTipY + 8, 43 + x, 26, SSD1306_WHITE);

  // Right inner ear: mirror image.
  display.drawLine(85 + x, 26, rightTipX - 2, rightTipY + 8, SSD1306_WHITE);
  display.drawLine(rightTipX - 2, rightTipY + 8, 104 + x, 31, SSD1306_WHITE);
}

/* =================================================
   LARGE CUTE EYES
   ================================================= */

void drawLargeEye(int x, int y, int pupilShift, bool excited) {
  // Tall rounded eye: overlapping circles form an oval.
  display.fillCircle(x, y - 2, 12, SSD1306_WHITE);
  display.fillCircle(x, y + 4, 10, SSD1306_WHITE);

  // Dark pupil moves left/right.
  display.fillCircle(x + pupilShift, y + 2, 8, SSD1306_BLACK);

  // Main reflected highlight.
  display.fillCircle(x + pupilShift - 3, y - 2, 3, SSD1306_WHITE);

  // Small lower highlight.
  display.fillCircle(x + pupilShift + 3, y + 6, 1, SSD1306_WHITE);

  if (excited) {
    // Four-point gleam inside the pupil.
    display.drawLine(
      x + pupilShift - 5,
      y + 2,
      x + pupilShift + 5,
      y + 2,
      SSD1306_WHITE
    );

    display.drawLine(
      x + pupilShift,
      y - 3,
      x + pupilShift,
      y + 7,
      SSD1306_WHITE
    );
  }
}

void drawBlinkEye(int x, int y) {
  // Soft, upside-down U blink shape.
  display.drawLine(x - 11, y, x - 6, y - 3, SSD1306_WHITE);
  display.drawLine(x - 6, y - 3, x, y + 1, SSD1306_WHITE);
  display.drawLine(x, y + 1, x + 6, y - 3, SSD1306_WHITE);
  display.drawLine(x + 6, y - 3, x + 11, y, SSD1306_WHITE);
}

void drawHappyEye(int x, int y) {
  display.drawLine(x - 10, y, x - 5, y - 4, SSD1306_WHITE);
  display.drawLine(x - 5, y - 4, x, y - 1, SSD1306_WHITE);
  display.drawLine(x, y - 1, x + 5, y - 4, SSD1306_WHITE);
  display.drawLine(x + 5, y - 4, x + 10, y, SSD1306_WHITE);
}

/* =================================================
   NOSE / MOUTHS
   ================================================= */

void drawNoseAndMouth(bool extraHappy) {
  // Small, high cat nose.
  display.fillTriangle(60, 51, 68, 51, 64, 55, SSD1306_WHITE);

  // W-shaped mouth, a strong cat facial cue.
  display.drawLine(64, 55, 61, 58, SSD1306_WHITE);
  display.drawLine(61, 58, 57, 56, SSD1306_WHITE);

  display.drawLine(64, 55, 67, 58, SSD1306_WHITE);
  display.drawLine(67, 58, 71, 56, SSD1306_WHITE);

  if (extraHappy) {
    display.drawLine(57, 56, 55, 54, SSD1306_WHITE);
    display.drawLine(71, 56, 73, 54, SSD1306_WHITE);
  }
}

void drawOpenMouth() {
  // Rounded open mouth.
  display.fillCircle(64, 55, 7, SSD1306_WHITE);
  display.fillCircle(64, 56, 3, SSD1306_BLACK);

  // Nose remains visible above it.
  display.fillTriangle(61, 49, 67, 49, 64, 52, SSD1306_WHITE);
}

/* =================================================
   WHISKERS / CHEEKS / PAWS
   ================================================= */

void drawWhiskers(int x) {
  // Left whiskers.
  display.drawLine(29 + x, 45, 3 + x, 40, SSD1306_WHITE);
  display.drawLine(28 + x, 50, 1 + x, 50, SSD1306_WHITE);
  display.drawLine(29 + x, 55, 4 + x, 61, SSD1306_WHITE);

  // Right whiskers.
  display.drawLine(99 + x, 45, 125 + x, 40, SSD1306_WHITE);
  display.drawLine(100 + x, 50, 127 + x, 50, SSD1306_WHITE);
  display.drawLine(99 + x, 55, 124 + x, 61, SSD1306_WHITE);
}

void drawCheeks(int x) {
  display.fillRoundRect(25 + x, 51, 12, 4, 2, SSD1306_WHITE);
  display.fillRoundRect(91 + x, 51, 12, 4, 2, SSD1306_WHITE);
}

void drawBottomPaws(int x) {
  drawPaw(18 + x, 63);
  drawPaw(110 + x, 63);
}

void drawPaw(int x, int y) {
  display.drawCircle(x, y, 9, SSD1306_WHITE);

  display.fillCircle(x - 4, y - 3, 2, SSD1306_WHITE);
  display.fillCircle(x, y - 5, 2, SSD1306_WHITE);
  display.fillCircle(x + 4, y - 3, 2, SSD1306_WHITE);
}

/* =================================================
   SIMPLE DECORATIONS
   ================================================= */

void drawSparkle(int x, int y) {
  display.drawLine(x - 3, y, x + 3, y, SSD1306_WHITE);
  display.drawLine(x, y - 3, x, y + 3, SSD1306_WHITE);
}

void drawHeart(int x, int y) {
  display.fillCircle(x - 3, y, 3, SSD1306_WHITE);
  display.fillCircle(x + 3, y, 3, SSD1306_WHITE);
  display.fillTriangle(
    x - 6, y + 1,
    x + 6, y + 1,
    x, y + 8,
    SSD1306_WHITE
  );
}
