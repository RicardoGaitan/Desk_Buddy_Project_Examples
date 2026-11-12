#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// =====================================================
// DISPLAY AND HARDWARE CONFIGURATION
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// XIAO ESP32-C3 GPIO mappings
#define OLED_SDA 8      // D8 = GPIO8
#define OLED_SCL 9      // D9 = GPIO9
#define TOUCH_PIN 7     // D1 = GPIO7 (TTP223B touch sensor)
#define BUZZER_PIN 2    // D0 = GPIO2

// TTP223B touch module: output goes HIGH when touched
const int PRESSED = HIGH;

Adafruit_SH1106G display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// MOOD DEFINITIONS
// =====================================================

enum Mood {
  NEUTRAL,
  HAPPY,
  SAD,
  ANGRY,
  SLEEPY,
  SURPRISED,
  LOVE,
  WINK,
  CONFUSED,
  CRYING,
  DIZZY,
  COOL,
  EXCITED,
  MISCHIEVOUS,
  WORRIED,
  BORED,
  SHOCKED,
  LAUGHING,
  SUSPICIOUS,
  SLEEPING
};

Mood mood = NEUTRAL;

// =====================================================
// DISPLAY GEOMETRY
// =====================================================

const int LEFT_X = 34;
const int RIGHT_X = 94;

const int EYE_Y = 29;
const int EYE_W = 28;
const int EYE_H = 32;

// =====================================================
// TIMING AND ANIMATION VARIABLES
// =====================================================

unsigned long nextBlink = 0;
unsigned long blinkStart = 0;
unsigned long nextLook = 0;

unsigned long lastInteraction = 0;
unsigned long lastButtonTime = 0;

unsigned long lastTear = 0;
unsigned long nextWink = 0;
unsigned long winkStart = 0;

const unsigned long DEBOUNCE_TIME = 40;

const unsigned long WINK_DURATION = 900;

const unsigned long SLEEP_TIMEOUT = 20000;
const unsigned long SLEEP_TRANSITION_TIME = 3500;
const unsigned long WAKE_TRANSITION_TIME = 1200;

bool blinking = false;
bool winkActive = false;

bool sleepTransition = false;
bool wakeTransition = false;
bool sleepingAutomatically = false;

bool lastButtonState = false;

int lookX = 0;
int tearPosition = 0;
int animationFrame = 0;
int sleepProgress = 0;

Mood sleepOriginMood = NEUTRAL;

unsigned long transitionStart = 0;

// =====================================================
// HELPER FUNCTIONS
// =====================================================

void drawHeart(int x, int y, int size) {

  display.fillCircle(x - size / 4, y, size / 4, SH110X_WHITE);
  display.fillCircle(x + size / 4, y, size / 4, SH110X_WHITE);

  display.fillTriangle(
    x - size / 2, y,
    x + size / 2, y,
    x, y + size / 2 + 2,
    SH110X_WHITE
  );
}


void drawSpiral(int x, int y) {

  display.drawCircle(x, y, 10, SH110X_WHITE);
  display.drawCircle(x, y, 6, SH110X_WHITE);
  display.drawPixel(x, y, SH110X_WHITE);
}


void drawHappyEye(int cx) {

  // Draw a curved, happy-looking closed eye
  display.drawLine(
    cx - 12,
    EYE_Y + 3,
    cx - 9,
    EYE_Y - 1,
    SH110X_WHITE
  );

  display.drawLine(
    cx - 9,
    EYE_Y - 1,
    cx - 5,
    EYE_Y - 4,
    SH110X_WHITE
  );

  display.drawLine(
    cx - 5,
    EYE_Y - 4,
    cx,
    EYE_Y - 5,
    SH110X_WHITE
  );

  display.drawLine(
    cx,
    EYE_Y - 5,
    cx + 5,
    EYE_Y - 4,
    SH110X_WHITE
  );

  display.drawLine(
    cx + 5,
    EYE_Y - 4,
    cx + 9,
    EYE_Y - 1,
    SH110X_WHITE
  );

  display.drawLine(
    cx + 9,
    EYE_Y - 1,
    cx + 12,
    EYE_Y + 3,
    SH110X_WHITE
  );
}


void drawNormalEye(int cx, int cy, int height) {

  height = constrain(height, 2, EYE_H);

  if (height <= 5) {

    display.drawLine(
      cx - 12 + lookX,
      cy,
      cx + 12 + lookX,
      cy,
      SH110X_WHITE
    );

    return;
  }

  display.fillRoundRect(
    cx - EYE_W / 2 + lookX,
    cy - height / 2,
    EYE_W,
    height,
    8,
    SH110X_WHITE
  );

  // Pupil
  display.fillCircle(
    cx + lookX,
    cy,
    5,
    SH110X_BLACK
  );

  display.fillCircle(
    cx + lookX + 1,
    cy - 1,
    2,
    SH110X_WHITE
  );
}


void drawAngryEye(int cx) {

  display.fillRoundRect(
    cx - 13 + lookX,
    EYE_Y - 10,
    26,
    22,
    5,
    SH110X_WHITE
  );

  display.fillCircle(
    cx + lookX,
    EYE_Y,
    5,
    SH110X_BLACK
  );

  display.fillTriangle(
    cx - 17,
    EYE_Y - 17,
    cx + 14,
    EYE_Y - 9,
    cx - 15,
    EYE_Y - 5,
    SH110X_BLACK
  );
}


// =====================================================
// EYE DRAWING
// =====================================================

void drawEye(int cx, int height, bool leftEye) {

  switch (mood) {

    case HAPPY:
    case LAUGHING:
      drawHappyEye(cx);
      break;


    case ANGRY:

      drawAngryEye(cx);
      break;


    case LOVE:

      drawHeart(cx + lookX, EYE_Y, 25);
      break;


    case DIZZY:

      drawSpiral(cx + lookX, EYE_Y);
      break;


    case COOL:

      display.fillRoundRect(
        cx - 17,
        EYE_Y - 9,
        34,
        19,
        4,
        SH110X_WHITE
      );

      display.drawLine(
        cx - 17,
        EYE_Y,
        cx + 17,
        EYE_Y,
        SH110X_BLACK
      );

      break;


    case SLEEPY:
    case BORED:

      display.fillRoundRect(
        cx - 14 + lookX,
        EYE_Y - 5,
        28,
        12,
        5,
        SH110X_WHITE
      );

      display.fillRect(
        cx - 15,
        EYE_Y - 12,
        30,
        8,
        SH110X_BLACK
      );

      break;


    case SLEEPING:

      display.drawLine(
        cx - 13,
        EYE_Y,
        cx + 13,
        EYE_Y,
        SH110X_WHITE
      );

      display.drawLine(
        cx - 9,
        EYE_Y + 4,
        cx + 9,
        EYE_Y + 4,
        SH110X_WHITE
      );

      break;


    case SURPRISED:
    case SHOCKED:

      display.drawRoundRect(
        cx - 10,
        EYE_Y - 14,
        20,
        29,
        9,
        SH110X_WHITE
      );

      display.fillCircle(
        cx + lookX,
        EYE_Y,
        4,
        SH110X_WHITE
      );

      break;


    case WINK:

      if (leftEye) {

        drawNormalEye(cx, EYE_Y, EYE_H);

      } else {

        int winkHeight = EYE_H;

        if (winkActive) {

          unsigned long elapsed = millis() - winkStart;

          if (elapsed < 250) {

            // Close the eye
            winkHeight = map(
              elapsed,
              0,
              250,
              EYE_H,
              3
            );

          } else if (elapsed < 600) {

            // Keep the eye closed
            winkHeight = 3;

          } else if (elapsed < WINK_DURATION) {

            // Reopen the eye
            winkHeight = map(
              elapsed,
              600,
              WINK_DURATION,
              3,
              EYE_H
            );

          } else {

            winkHeight = EYE_H;
            winkActive = false;
          }
        }

        drawNormalEye(cx, EYE_Y, winkHeight);
      }

      break;


    case CONFUSED:
    case SUSPICIOUS:

      display.fillRoundRect(
        cx - 13 + lookX,
        EYE_Y - 8,
        26,
        20,
        7,
        SH110X_WHITE
      );

      display.fillCircle(
        cx + lookX,
        EYE_Y,
        5,
        SH110X_BLACK
      );

      display.drawLine(
        cx - 15,
        EYE_Y - 15,
        cx + 13,
        EYE_Y - 19,
        SH110X_WHITE
      );

      break;


    default:

      drawNormalEye(cx, EYE_Y, height);
      break;
  }
}


// =====================================================
// MOUTH DRAWING
// =====================================================

void drawMouth() {

  switch (mood) {

    case HAPPY:

      display.drawLine(49, 49, 57, 56, SH110X_WHITE);
      display.drawLine(57, 56, 70, 56, SH110X_WHITE);
      display.drawLine(70, 56, 79, 49, SH110X_WHITE);

      break;


    case LAUGHING:

      display.fillRoundRect(
        48,
        46,
        32,
        15,
        7,
        SH110X_WHITE
      );

      display.drawLine(
        52,
        52,
        76,
        52,
        SH110X_BLACK
      );

      break;


    case SAD:
    case CRYING:

      display.drawLine(49, 56, 57, 50, SH110X_WHITE);
      display.drawLine(57, 50, 70, 50, SH110X_WHITE);
      display.drawLine(70, 50, 79, 56, SH110X_WHITE);

      break;


    case ANGRY:

      display.drawLine(51, 55, 77, 49, SH110X_WHITE);
      display.drawLine(51, 56, 77, 56, SH110X_WHITE);

      break;


    case SURPRISED:
    case SHOCKED:

      display.drawEllipse(
        64,
        53,
        7,
        9,
        SH110X_WHITE
      );

      break;


    case LOVE:

      display.drawLine(51, 51, 57, 56, SH110X_WHITE);
      display.drawLine(57, 56, 70, 56, SH110X_WHITE);
      display.drawLine(70, 56, 77, 51, SH110X_WHITE);

      break;


    case CONFUSED:

      display.drawLine(52, 54, 62, 54, SH110X_WHITE);
      display.drawLine(62, 54, 72, 50, SH110X_WHITE);

      break;


    case SLEEPY:
    case SLEEPING:

      display.drawLine(55, 55, 73, 55, SH110X_WHITE);

      break;


    case BORED:

      display.drawLine(54, 55, 74, 55, SH110X_WHITE);

      break;


    case EXCITED:

      display.drawLine(48, 49, 56, 57, SH110X_WHITE);
      display.drawLine(56, 57, 72, 57, SH110X_WHITE);
      display.drawLine(72, 57, 80, 49, SH110X_WHITE);

      break;


    case WORRIED:

      display.drawLine(52, 53, 58, 57, SH110X_WHITE);
      display.drawLine(58, 57, 70, 57, SH110X_WHITE);
      display.drawLine(70, 57, 76, 53, SH110X_WHITE);

      break;


    case MISCHIEVOUS:

      display.drawLine(51, 53, 62, 56, SH110X_WHITE);
      display.drawLine(62, 56, 77, 49, SH110X_WHITE);

      break;


    case DIZZY:

      display.drawLine(55, 54, 73, 54, SH110X_WHITE);

      break;


    case COOL:

      display.drawLine(53, 54, 75, 54, SH110X_WHITE);

      break;


    case SUSPICIOUS:

      display.drawLine(55, 54, 72, 52, SH110X_WHITE);

      break;


    case NEUTRAL:
    case WINK:

      display.drawLine(55, 54, 73, 54, SH110X_WHITE);

      break;
  }
}


// =====================================================
// TEARS, STARS AND SLEEP SYMBOLS
// =====================================================

void drawTears() {

  if (millis() - lastTear > 120) {

    tearPosition++;

    if (tearPosition > 20) {
      tearPosition = 0;
    }

    lastTear = millis();
  }

  display.fillTriangle(
    LEFT_X + 12,
    EYE_Y + 12 + tearPosition,
    LEFT_X + 8,
    EYE_Y + 19 + tearPosition,
    LEFT_X + 16,
    EYE_Y + 19 + tearPosition,
    SH110X_WHITE
  );

  display.fillTriangle(
    RIGHT_X + 12,
    EYE_Y + 12 + tearPosition,
    RIGHT_X + 8,
    EYE_Y + 19 + tearPosition,
    RIGHT_X + 16,
    EYE_Y + 19 + tearPosition,
    SH110X_WHITE
  );
}


void drawStars() {

  display.drawLine(18, 15, 18, 23, SH110X_WHITE);
  display.drawLine(14, 19, 22, 19, SH110X_WHITE);

  display.drawLine(109, 13, 109, 21, SH110X_WHITE);
  display.drawLine(105, 17, 113, 17, SH110X_WHITE);

  display.drawPixel(30, 42, SH110X_WHITE);
  display.drawPixel(99, 42, SH110X_WHITE);
}


void drawSleepingZ() {

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(93, 10);
  display.print("z");

  display.setCursor(103, 3);
  display.print("Z");

  display.setCursor(113, 0);
  display.print("z");
}


// =====================================================
// MAIN FACE DRAWING
// =====================================================

void drawFace() {

  int eyeHeight = EYE_H;

  if (blinking && mood != WINK) {

    unsigned long elapsed = millis() - blinkStart;

    if (elapsed < 100) {

      eyeHeight = map(
        elapsed,
        0,
        100,
        EYE_H,
        3
      );

    } else if (elapsed < 200) {

      eyeHeight = map(
        elapsed,
        100,
        200,
        3,
        EYE_H
      );

    } else {

      blinking = false;
      eyeHeight = EYE_H;
    }
  }

  drawEye(LEFT_X, eyeHeight, true);
  drawEye(RIGHT_X, eyeHeight, false);

  drawMouth();

  if (mood == CRYING) {
    drawTears();
  }

  if (mood == EXCITED) {
    drawStars();
  }

  if (mood == SLEEPING) {
    drawSleepingZ();
  }
}


// =====================================================
// ANIMATION UPDATE
// =====================================================

void updateAnimation() {

  unsigned long now = millis();

  // Do not run normal facial animations during transitions
  if (
    sleepTransition ||
    wakeTransition ||
    sleepingAutomatically ||
    mood == SLEEPING
  ) {
    return;
  }

  // -------------------------------------------------
  // WINK ANIMATION
  // -------------------------------------------------

  if (mood == WINK) {

    if (!winkActive && now >= nextWink) {

      winkActive = true;
      winkStart = now;

      nextWink = now + random(2500, 4000);
    }

    return;
  }

  // -------------------------------------------------
  // RANDOM BLINKING
  // -------------------------------------------------

  if (!blinking && now >= nextBlink) {

    blinking = true;
    blinkStart = now;

    nextBlink = now + random(2500, 5500);
  }

  // -------------------------------------------------
  // RANDOM EYE MOVEMENT
  // -------------------------------------------------

  if (now >= nextLook) {

    lookX = random(-4, 5);

    nextLook = now + random(1000, 3000);
  }
}


// =====================================================
// SLEEP AND WAKE TRANSITIONS
// =====================================================

void updateSleepTransition() {

  unsigned long now = millis();

  // -------------------------------------------------
  // ENTERING SLEEP
  // -------------------------------------------------

  if (sleepTransition) {

    unsigned long elapsed = now - transitionStart;

    sleepProgress = constrain(
      map(
        elapsed,
        0,
        SLEEP_TRANSITION_TIME,
        0,
        100
      ),
      0,
      100
    );

    if (elapsed >= SLEEP_TRANSITION_TIME) {

      sleepTransition = false;
      sleepingAutomatically = true;

      mood = SLEEPING;

      sleepProgress = 100;

      Serial.println("Going to sleep...");
    }
  }

  // -------------------------------------------------
  // WAKING UP
  // -------------------------------------------------

  if (wakeTransition) {

    unsigned long elapsed = now - transitionStart;

    sleepProgress = 100 - constrain(
      map(
        elapsed,
        0,
        WAKE_TRANSITION_TIME,
        0,
        100
      ),
      0,
      100
    );

    if (elapsed >= WAKE_TRANSITION_TIME) {

      wakeTransition = false;
      sleepingAutomatically = false;

      mood = NEUTRAL;

      sleepProgress = 0;

      nextBlink = now + 2000;
      nextLook = now + 1000;

      Serial.println("Waking up...");
    }
  }
}


// =====================================================
// DRAW THE TRANSITION BETWEEN EXPRESSIONS AND SLEEP
// =====================================================

void drawTransitionFace() {

  // Draw the original expression underneath the
  // gradually closing eyelids.
  drawFace();

  int progress = sleepProgress;

  if (progress <= 0) {
    return;
  }

  // -------------------------------------------------
  // EYE CLOSING EFFECT
  // -------------------------------------------------

  int openHeight = map(
    progress,
    0,
    100,
    36,
    2
  );

  openHeight = constrain(openHeight, 2, 36);

  int maskHeight = (36 - openHeight) / 2;

  int leftEyeX = LEFT_X - 18 + lookX;
  int rightEyeX = RIGHT_X - 18 + lookX;

  int eyeTop = EYE_Y - 18;

  // Cover the top and bottom of the original eyes.
  display.fillRect(
    leftEyeX,
    eyeTop,
    36,
    maskHeight,
    SH110X_BLACK
  );

  display.fillRect(
    leftEyeX,
    eyeTop + 36 - maskHeight,
    36,
    maskHeight,
    SH110X_BLACK
  );

  display.fillRect(
    rightEyeX,
    eyeTop,
    36,
    maskHeight,
    SH110X_BLACK
  );

  display.fillRect(
    rightEyeX,
    eyeTop + 36 - maskHeight,
    36,
    maskHeight,
    SH110X_BLACK
  );

  // Draw the eyelids as the eyes close.
  if (progress > 15) {

    display.drawLine(
      leftEyeX + 5,
      EYE_Y,
      leftEyeX + 31,
      EYE_Y,
      SH110X_WHITE
    );

    display.drawLine(
      rightEyeX + 5,
      EYE_Y,
      rightEyeX + 31,
      EYE_Y,
      SH110X_WHITE
    );
  }

  // -------------------------------------------------
  // GRADUALLY RELAX THE MOUTH
  // -------------------------------------------------

  int mouthCover = map(
    progress,
    0,
    100,
    0,
    18
  );

  display.fillRect(
    44,
    47,
    40,
    mouthCover,
    SH110X_BLACK
  );

  // A small relaxed mouth appears near the end.
  if (progress > 75) {

    display.drawLine(
      57,
      55,
      71,
      55,
      SH110X_WHITE
    );
  }

  // -------------------------------------------------
  // SLEEP SYMBOLS APPEAR GRADUALLY
  // -------------------------------------------------

  if (progress > 65) {

    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    display.setCursor(94, 12);
    display.print("z");
  }

  if (progress > 85) {

    display.setCursor(104, 5);
    display.print("Z");
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // Touch sensor (TTP223B drives the pin itself, no pull-up needed)
  pinMode(TOUCH_PIN, INPUT);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  // OLED I2C: SDA = GPIO8 (D8), SCL = GPIO9 (D9)
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);

  // Initialise SH1106 OLED
  if (!display.begin(0x3C, true)) {

    Serial.println("OLED initialization failed!");

    while (true) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.display();

  randomSeed(analogRead(A0));

  nextBlink = millis() + random(2000, 4000);
  nextLook = millis() + random(1000, 2000);

  lastInteraction = millis();

  Serial.println("Compagnon 309 initialized.");
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  unsigned long now = millis();

  // -------------------------------------------------
  // TOUCH INPUT
  // -------------------------------------------------

  bool buttonState = (digitalRead(TOUCH_PIN) == PRESSED);

  if (
    buttonState &&
    !lastButtonState &&
    now - lastButtonTime > DEBOUNCE_TIME
  ) {

    lastButtonTime = now;
    lastInteraction = now;

    // If the robot is sleeping or transitioning
    // into sleep, touching the sensor wakes it.
    if (sleepTransition || sleepingAutomatically) {

      sleepTransition = false;

      wakeTransition = true;
      transitionStart = now;

      sleepingAutomatically = true;

      mood = NEUTRAL;

      blinking = false;
      winkActive = false;

      lookX = 0;

      Serial.println("Wake-up requested.");
    }

    // Ignore additional mood changes during wake-up.
    else if (!wakeTransition) {

      mood = (Mood)((mood + 1) % 20);

      blinking = false;
      winkActive = false;

      lookX = 0;

      if (mood == WINK) {

        winkActive = true;
        winkStart = now;

        nextWink = now + random(2500, 4000);
      }

      Serial.print("Current mood: ");
      Serial.println((int)mood);
    }
  }

  lastButtonState = buttonState;

  // -------------------------------------------------
  // AUTOMATIC SLEEP DETECTION
  // -------------------------------------------------

  if (
    !sleepTransition &&
    !wakeTransition &&
    !sleepingAutomatically &&
    mood != SLEEPING &&
    now - lastInteraction >= SLEEP_TIMEOUT
  ) {

    sleepOriginMood = mood;

    sleepTransition = true;
    transitionStart = now;

    sleepProgress = 0;

    blinking = false;
    winkActive = false;

    lookX = 0;

    Serial.println("Starting sleep transition...");
  }

  // -------------------------------------------------
  // UPDATE ANIMATIONS
  // -------------------------------------------------

  updateSleepTransition();

  updateAnimation();

  // -------------------------------------------------
  // DRAW DISPLAY
  // -------------------------------------------------

  display.clearDisplay();

  if (sleepTransition || wakeTransition) {

    drawTransitionFace();

  } else {

    drawFace();
  }

  display.display();

  delay(15);
}