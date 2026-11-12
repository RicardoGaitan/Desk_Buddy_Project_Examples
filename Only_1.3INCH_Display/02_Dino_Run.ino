/*
  LESSON 09 - FINAL PROJECT: Dino Run (Chrome offline game) on the OLED

  Board   : Seeed XIAO ESP32-C3
  Display : 1.3-inch 128x64 OLED (SH1106 I2C controller)

  LIBRARIES:
    - Adafruit SH110X
    - Adafruit GFX Library
    - Adafruit BusIO

  CONTROLS:
    JUMP  : D1 (TTP223B capacitive touch sensor)
    Touch the sensor on the title screen to start.
    Touch it again after GAME OVER to restart.

  FEATURES:
    - Running dinosaur with animated legs
    - Small, large and grouped cacti
    - Flying birds (appear after 200 points)
    - Increasing game speed
    - Score and HI score
    - Score flashes and beeps every 100 points
    - Day/night screen inversion every 700 points
    - Scrolling ground bumps and clouds
    - GAME OVER screen and restart icon

  WIRING:
    OLED VCC -> 3V3
    OLED GND -> GND
    OLED SDA -> D8 (GPIO8)
    OLED SCL -> D9 (GPIO9)

    TTP223B VCC -> 3V3
    TTP223B GND -> GND
    TTP223B I/O -> D1 (GPIO3)
    Buzzer (+)  -> D0 (GPIO2)
    Buzzer (-)  -> GND
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ---------- OLED DISPLAY ----------

#define SCREEN_W 128
#define SCREEN_H 64

Adafruit_SH1106G display(SCREEN_W, SCREEN_H, &Wire, -1);

// ---------- PINS ----------

// XIAO ESP32-C3 GPIO mappings
const int PIN_JUMP = 7;   // D1 = GPIO7
const int PIN_BUZZ = 2;   // D0 = GPIO2

// TTP223B touch module:
// Output goes HIGH when touched.
// Use pinMode INPUT and PRESSED = HIGH.

const int PRESSED = HIGH;

// ---------- GAME CONSTANTS ----------

const int GROUND = 56;

const int DINO_X = 12;

const int DINO_W = 20;
const int DINO_H = 22;

const float JUMP_V = 5.6;
const float GRAVITY = 0.55;

const unsigned long FRAME_MS = 33;

// ---------- GAME STATE ----------

enum State {
  READY,
  RUNNING,
  DEAD
};

State state = READY;

float dinoH = 0;
float vy = 0;

int animTick = 0;

float speed = 3.2;
float distanceRun = 0;

int score = 0;
int hiScore = 0;

int flashFrames = 0;
int lastHundred = 0;

bool night = false;

unsigned long lastFrame = 0;
unsigned long diedAt = 0;

bool prevJump = false;

// ---------- OBSTACLES ----------

struct Obstacle {
  float x;
  int y;
  int w;
  int h;
  int type;
  bool active;
};

const int MAX_OBS = 3;

Obstacle obs[MAX_OBS];

// Types:
// 0 = small cactus
// 1 = small cactus x2
// 2 = small cactus x3
// 3 = large cactus
// 4 = large cactus x2
// 5 = low bird (jump over it)
// 7 = high bird (run under it)
// (type 6, the mid bird that required ducking, was removed)

float sinceSpawn = 0;
float nextGap = 100;

// ---------- SCENERY ----------

struct Cloud {
  float x;
  int y;
};

Cloud clouds[2];

float bumpX[10];
int bumpY[10];

// ---------- HELPERS ----------

bool isPressed(int pin) {
  return digitalRead(pin) == PRESSED;
}

// ---------- SPAWN OBSTACLE ----------

void spawnObstacle() {

  for (int i = 0; i < MAX_OBS; i++) {

    if (obs[i].active) {
      continue;
    }

    Obstacle &o = obs[i];

    o.active = true;
    o.x = SCREEN_W + 2;

    if (score >= 200 && random(100) < 25) {

      // Bird (low or high only, no duck-height birds)

      o.type = (random(2) == 0) ? 5 : 7;

      o.w = 14;
      o.h = 10;

      o.y = (o.type == 5) ? 44 : 12;

    } else {

      // Cactus group

      o.type = random(5);

      static const int W[5] = {
        7, 16, 25, 9, 20
      };

      static const int H[5] = {
        12, 12, 12, 16, 16
      };

      o.w = W[o.type];
      o.h = H[o.type];

      o.y = GROUND - o.h;
    }

    float minGap = 30 + speed * 18;

    nextGap = o.w + minGap + random(0, 70);

    sinceSpawn = 0;

    return;
  }
}

// ---------- RESET GAME ----------

void resetGame() {

  dinoH = 0;
  vy = 0;

  speed = 3.2;
  distanceRun = 0;

  score = 0;

  flashFrames = 0;
  lastHundred = 0;

  sinceSpawn = 0;
  nextGap = 100;

  for (int i = 0; i < MAX_OBS; i++) {
    obs[i].active = false;
  }

  night = false;

  display.invertDisplay(false);
}

// ---------- DRAWING ----------

void drawDino(
  int x,
  int y,
  bool legsAnim,
  bool inAir,
  bool dead
) {

  // Head
  display.fillRect(
    x + 10, y, 10, 8, SH110X_WHITE
  );

  // Body
  display.fillRect(
    x + 4, y + 8, 12, 9, SH110X_WHITE
  );

  // Tail
  display.fillTriangle(
    x, y + 9,
    x + 4, y + 9,
    x + 4, y + 15,
    SH110X_WHITE
  );

  // Arm
  display.fillRect(
    x + 16, y + 11, 3, 2, SH110X_WHITE
  );

  int legL = (inAir || !legsAnim) ? 5 : 3;
  int legR = (inAir || legsAnim) ? 5 : 3;

  // Legs
  display.fillRect(
    x + 6, y + 17, 3, legL, SH110X_WHITE
  );

  display.fillRect(
    x + 11, y + 17, 3, legR, SH110X_WHITE
  );

  // Mouth
  display.drawFastHLine(
    x + 15, y + 5, 5, SH110X_BLACK
  );

  if (dead) {

    // X eye

    display.drawPixel(x + 14, y + 1, SH110X_BLACK);
    display.drawPixel(x + 16, y + 1, SH110X_BLACK);

    display.drawPixel(x + 15, y + 2, SH110X_BLACK);

    display.drawPixel(x + 14, y + 3, SH110X_BLACK);
    display.drawPixel(x + 16, y + 3, SH110X_BLACK);

  } else {

    // Normal eye

    display.fillRect(
      x + 15, y + 2, 2, 2, SH110X_BLACK
    );
  }
}

// ---------- DRAW CACTUS ----------

void drawCactusUnit(int x, int w, int h) {

  int top = GROUND - h;

  int tw = 3;

  int tx = x + (w - tw) / 2;

  // Trunk
  display.fillRect(
    tx, top, tw, h, SH110X_WHITE
  );

  // Round top
  display.drawPixel(
    tx, top, SH110X_BLACK
  );

  display.drawPixel(
    tx + tw - 1, top, SH110X_BLACK
  );

  // Left arm
  display.fillRect(
    x, top + 3, 2, 4, SH110X_WHITE
  );

  display.fillRect(
    x, top + 6, tx - x + 1, 2, SH110X_WHITE
  );

  // Right arm
  display.fillRect(
    x + w - 2, top + 5, 2, 4, SH110X_WHITE
  );

  display.fillRect(
    tx + tw - 1,
    top + 8,
    (x + w) - (tx + tw - 1),
    2,
    SH110X_WHITE
  );
}

// ---------- DRAW OBSTACLE ----------

void drawObstacle(const Obstacle &o) {

  int x = (int)o.x;

  switch (o.type) {

    case 0:

      drawCactusUnit(x, 7, 12);

      break;

    case 1:

      drawCactusUnit(x, 7, 12);
      drawCactusUnit(x + 9, 7, 12);

      break;

    case 2:

      drawCactusUnit(x, 7, 12);
      drawCactusUnit(x + 9, 7, 12);
      drawCactusUnit(x + 18, 7, 12);

      break;

    case 3:

      drawCactusUnit(x, 9, 16);

      break;

    case 4:

      drawCactusUnit(x, 9, 16);
      drawCactusUnit(x + 11, 9, 16);

      break;

    default: {

      // Bird

      bool up = ((animTick / 6) % 2) == 0;

      // Body
      display.fillRect(
        x + 3, o.y + 4, 9, 3, SH110X_WHITE
      );

      // Beak
      display.fillTriangle(
        x, o.y + 5,
        x + 4, o.y + 3,
        x + 4, o.y + 7,
        SH110X_WHITE
      );

      // Tail
      display.fillRect(
        x + 12, o.y + 3, 2, 2, SH110X_WHITE
      );

      // Wings
      if (up) {

        display.fillTriangle(
          x + 5, o.y + 4,
          x + 10, o.y + 4,
          x + 8, o.y,
          SH110X_WHITE
        );

      } else {

        display.fillTriangle(
          x + 5, o.y + 6,
          x + 10, o.y + 6,
          x + 8, o.y + 9,
          SH110X_WHITE
        );
      }

      // Eye
      display.drawPixel(
        x + 4, o.y + 4, SH110X_BLACK
      );

      break;
    }
  }
}

// ---------- DRAW GROUND ----------

void drawGround() {

  display.drawFastHLine(
    0, GROUND, SCREEN_W, SH110X_WHITE
  );

  for (int i = 0; i < 10; i++) {

    int bx = (int)bumpX[i];

    display.drawFastHLine(
      bx, bumpY[i], 2, SH110X_WHITE
    );
  }
}

// ---------- DRAW CLOUDS ----------

void drawClouds() {

  for (int i = 0; i < 2; i++) {

    int cx = (int)clouds[i].x;
    int cy = clouds[i].y;

    display.drawFastHLine(
      cx + 3, cy, 10, SH110X_WHITE
    );

    display.drawFastHLine(
      cx, cy + 3, 16, SH110X_WHITE
    );

    display.drawPixel(
      cx + 2, cy + 1, SH110X_WHITE
    );

    display.drawPixel(
      cx + 1, cy + 2, SH110X_WHITE
    );

    display.drawPixel(
      cx + 13, cy + 1, SH110X_WHITE
    );

    display.drawPixel(
      cx + 14, cy + 2, SH110X_WHITE
    );

    display.drawFastHLine(
      cx + 6, cy - 1, 4, SH110X_WHITE
    );
  }
}

// ---------- DRAW SCORE ----------

void drawScore() {

  char buf[12];

  display.setTextSize(1);

  display.setTextColor(SH110X_WHITE);

  if (hiScore > 0) {

    snprintf(
      buf, sizeof(buf), "HI %05d", hiScore
    );

    display.setCursor(42, 2);

    display.print(buf);
  }

  bool hide =
    (flashFrames > 0) &&
    (((flashFrames / 4) % 2) == 0);

  if (!hide) {

    snprintf(
      buf, sizeof(buf), "%05d", score
    );

    display.setCursor(96, 2);

    display.print(buf);
  }
}

// ---------- SCROLL SCENERY ----------

void scrollScenery() {

  for (int i = 0; i < 10; i++) {

    bumpX[i] -= speed;

    if (bumpX[i] < -2) {

      bumpX[i] = SCREEN_W + random(0, 20);

      bumpY[i] =
        GROUND + 2 + random(0, 5);
    }
  }

  for (int i = 0; i < 2; i++) {

    clouds[i].x -= 0.5;

    if (clouds[i].x < -18) {

      clouds[i].x =
        SCREEN_W + random(0, 40);

      clouds[i].y =
        12 + random(0, 14);
    }
  }
}

// ---------- COLLISION ----------

bool hit(
  int ax, int ay, int aw, int ah,
  int bx, int by, int bw, int bh
) {

  return
    ax < bx + bw &&
    ax + aw > bx &&
    ay < by + bh &&
    ay + ah > by;
}

// ---------- CHECK COLLISION ----------

bool checkCollision() {

  int top =
    GROUND -
    DINO_H -
    (int)dinoH;

  int ax = DINO_X + 3;
  int ay = top + 2;
  int aw = DINO_W - 6;
  int ah = DINO_H - 4;

  for (int i = 0; i < MAX_OBS; i++) {

    if (!obs[i].active) {
      continue;
    }

    // Slightly smaller obstacle box
    // Makes collision detection more forgiving.

    if (
      hit(
        ax, ay, aw, ah,
        (int)obs[i].x + 1,
        obs[i].y + 1,
        obs[i].w - 2,
        obs[i].h - 2
      )
    ) {

      return true;
    }
  }

  return false;
}

// ---------- GAME LOGIC ----------

void updateRunning(bool jumpP) {

  bool jumpEdge =
    jumpP && !prevJump;

  bool onGround =
    (dinoH == 0 && vy == 0);

  if (jumpEdge && onGround) {

    vy = JUMP_V;

    tone(PIN_BUZZ, 700, 40);
  }

  // Release early = short hop

  if (!jumpP && vy > 2.5) {

    vy = 2.5;
  }

  // In the air

  if (!onGround || vy > 0) {

    dinoH += vy;

    vy -= GRAVITY;

    if (dinoH <= 0) {

      dinoH = 0;
      vy = 0;
    }
  }

  // Move world

  distanceRun += speed;

  score = (int)(distanceRun / 6);

  speed = 3.2f + score * 0.004f;

  if (speed > 8.5f) {

    speed = 8.5f;
  }

  // Score milestone sound

  if (score / 100 > lastHundred) {

    flashFrames = 24;

    tone(PIN_BUZZ, 1200, 80);
  }

  lastHundred = score / 100;

  if (flashFrames > 0) {

    flashFrames--;
  }

  // Day/night cycle

  bool nowNight =
    ((score / 700) % 2) == 1;

  if (nowNight != night) {

    night = nowNight;

    display.invertDisplay(night);
  }

  // Move obstacles

  for (int i = 0; i < MAX_OBS; i++) {

    if (!obs[i].active) {
      continue;
    }

    obs[i].x -= speed;

    if (obs[i].x + obs[i].w < 0) {

      obs[i].active = false;
    }
  }

  // Spawn obstacles

  sinceSpawn += speed;

  if (sinceSpawn >= nextGap) {

    spawnObstacle();
  }

  // Move scenery

  scrollScenery();

  animTick++;

  // Collision detection

  if (checkCollision()) {

    state = DEAD;

    diedAt = millis();

    if (score > hiScore) {

      hiScore = score;
    }

    tone(PIN_BUZZ, 200, 350);
  }
}

// ---------- DRAW PLAYER ----------

void drawPlayer(bool dead) {

  bool legs =
    ((animTick / 5) % 2) == 0;

  bool inAir =
    dinoH > 0;

  drawDino(
    DINO_X,
    GROUND - DINO_H - (int)dinoH,
    legs,
    inAir,
    dead
  );
}

// ---------- GAME OVER SCREEN ----------

void drawGameOver() {

  display.setTextSize(1);

  display.setCursor(35, 22);

  display.print("G A M E  O V E R");

  // Restart icon

  display.drawCircle(
    64, 38, 6, SH110X_WHITE
  );

  display.fillRect(
    64, 31, 8, 4, SH110X_BLACK
  );

  display.fillTriangle(
    66, 29,
    66, 37,
    72, 33,
    SH110X_WHITE
  );
}

// ---------- ARDUINO SETUP ----------

void setup() {

  Serial.begin(115200);

  // Touch sensor configuration
  // (TTP223B drives the pin itself, so no pull-up needed)

  pinMode(PIN_JUMP, INPUT);

  // Buzzer configuration

  pinMode(PIN_BUZZ, OUTPUT);

  // OLED I2C configuration
  // SDA = GPIO8 (D8)
  // SCL = GPIO9 (D9)

  Wire.begin(8, 9);

  Wire.setClock(400000);

  // Initialise SH1106 OLED

  if (!display.begin(0x3C, true)) {

    Serial.println("OLED not found!");

    while (true) {

      delay(1000);
    }
  }

  // Random number seed

  randomSeed(analogRead(A0));

  // Initialise obstacles

  for (int i = 0; i < MAX_OBS; i++) {

    obs[i].active = false;
  }

  // Initialise ground bumps

  for (int i = 0; i < 10; i++) {

    bumpX[i] = random(0, SCREEN_W);

    bumpY[i] =
      GROUND + 2 + random(0, 5);
  }

  // Initialise clouds

  clouds[0] = { 30, 14 };

  clouds[1] = { 100, 24 };

  Serial.println("Dino Run Started!");

}

// ---------- MAIN LOOP ----------

void loop() {

  unsigned long now = millis();

  // Fixed frame rate

  if (now - lastFrame < FRAME_MS) {

    return;
  }

  lastFrame = now;

  // Read controls

  bool jumpP =
    isPressed(PIN_JUMP);

  // Clear screen

  display.clearDisplay();

  // ---------- READY STATE ----------

  if (state == READY) {

    display.setTextSize(1);

    display.setCursor(18, 14);

    display.print("TOUCH TO START");

    drawGround();

    drawDino(
      DINO_X,
      GROUND - DINO_H,
      false,
      false,
      false
    );

    drawScore();

    // Start game when jump is pressed

    if (jumpP && !prevJump) {

      resetGame();

      state = RUNNING;

      vy = JUMP_V;

      tone(PIN_BUZZ, 700, 40);
    }
  }

  // ---------- RUNNING STATE ----------

  else if (state == RUNNING) {

    updateRunning(jumpP);

    drawClouds();

    drawGround();

    // Draw obstacles

    for (int i = 0; i < MAX_OBS; i++) {

      if (obs[i].active) {

        drawObstacle(obs[i]);
      }
    }

    drawPlayer(state == DEAD);

    drawScore();
  }

  // ---------- DEAD STATE ----------

  else {

    drawClouds();

    drawGround();

    // Draw remaining obstacles

    for (int i = 0; i < MAX_OBS; i++) {

      if (obs[i].active) {

        drawObstacle(obs[i]);
      }
    }

    drawPlayer(true);

    drawScore();

    drawGameOver();

    // Restart after a short delay

    if (
      now - diedAt > 600 &&
      jumpP &&
      !prevJump
    ) {

      resetGame();

      state = RUNNING;

      vy = JUMP_V;

      tone(PIN_BUZZ, 700, 40);
    }
  }

  // Update OLED

  display.display();

  // Save previous button state

  prevJump = jumpP;
}

/*
  TRY IT - CHALLENGES FOR STUDENTS:

  1. Make the dinosaur jump higher or lower.
     Change JUMP_V and GRAVITY.

  2. Make the game start faster or slower.
     Change the initial speed.

  3. Store the HI score using Preferences
     so it survives a reset.

  4. Use a vibration motor to provide
     feedback when the dinosaur dies.

  5. Use a tilt sensor to make the dinosaur
     jump when you shake the robot.
*/