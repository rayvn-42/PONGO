#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr uint8_t UP_PIN = 2;
constexpr uint8_t DOWN_PIN = 3;
constexpr uint8_t LEFT_PIN = 4;
constexpr uint8_t RIGHT_PIN = 5;
constexpr uint8_t BUZZER_PIN = 9;

constexpr float PLAY_TOP = 12.0f;
constexpr float PLAY_BOTTOM = 63.0f;
constexpr float BALL_RADIUS = 2.0f;
constexpr float PADDLE_HEIGHT = 14.0f;
constexpr float PADDLE_WIDTH = 3.0f;
constexpr float LEFT_PADDLE_X = 4.0f;
constexpr float RIGHT_PADDLE_X = 121.0f;
constexpr float PADDLE_SPEED = 115.0f;
constexpr float AI_SPEED = 88.0f;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

struct Ball {
  float x;
  float y;
  float vx;
  float vy;
};

struct Buttons {
  bool up;
  bool down;
  bool left;
  bool right;
  bool upPressed;
  bool leftPressed;
  bool rightPressed;
};

enum ScreenState {
  MENU,
  GAME
};

ScreenState screenState = MENU;
Ball ball;

float leftPaddleY = 37.5f;
float rightPaddleY = 37.5f;

uint16_t leftScore = 0;
uint16_t rightScore = 0;

bool twoPlayer = false;

unsigned long lastUpdate = 0;
unsigned long lastRender = 0;

float clampFloat(float value, float low, float high) {
  return value < low ? low : value > high ? high : value;
}

float minFloat(float a, float b) {
  return a < b ? a : b;
}

Buttons readButtons() {
  static bool previousUp = false;
  static bool previousLeft = false;
  static bool previousRight = false;

  Buttons b;

  b.up = digitalRead(UP_PIN) == LOW;
  b.down = digitalRead(DOWN_PIN) == LOW;
  b.left = digitalRead(LEFT_PIN) == LOW;
  b.right = digitalRead(RIGHT_PIN) == LOW;

  b.upPressed = b.up && !previousUp;
  b.leftPressed = b.left && !previousLeft;
  b.rightPressed = b.right && !previousRight;

  previousUp = b.up;
  previousLeft = b.left;
  previousRight = b.right;

  return b;
}

void beep(uint16_t frequency, uint16_t duration) {
  tone(BUZZER_PIN, frequency, duration);
}

void showSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(34, 19);
  display.print(F("PONGO"));

  display.setTextSize(1);
  display.setCursor(22, 44);
  display.print(F("PADDLE CONSOLE"));

  display.display();
  delay(1200);
}

void drawMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(34, 2);
  display.print(F("PONGO"));

  display.setTextSize(1);
  display.setCursor(22, 24);
  display.print(F("PADDLE CONSOLE"));

  display.drawFastHLine(14, 35, 100, SSD1306_WHITE);

  display.setCursor(43, 38);
  display.print(twoPlayer ? F("2 PLAYERS") : F("1 PLAYER"));

  display.setCursor(16, 47);
  display.print(F("L/R: CHANGE MODE"));

  display.setCursor(44, 56);
  display.print(F("UP: START"));

  display.display();
}

void resetBall(int direction) {
  ball.x = SCREEN_WIDTH / 2.0f;
  ball.y = (PLAY_TOP + PLAY_BOTTOM) / 2.0f;
  ball.vx = direction * 95.0f;
  ball.vy = static_cast<float>(random(-45, 46));

  if (fabsf(ball.vy) < 24.0f) {
      ball.vy = ball.vy < 0 ? -24.0f : 24.0f;
  }
}

void startGame() {
  leftScore = 0;
  rightScore = 0;
  leftPaddleY = (PLAY_TOP + PLAY_BOTTOM) / 2.0f;
  rightPaddleY = (PLAY_TOP + PLAY_BOTTOM) / 2.0f;

  resetBall(random(0, 2) == 0 ? -1 : 1);

  screenState = GAME;
  lastUpdate = millis();
  lastRender = lastUpdate;
  drawMenu();
}

void movePaddle(float &y, float direction, float speed, float dt) {
  y += direction * speed * dt;

  y = clampFloat(
      y,
      PLAY_TOP + PADDLE_HEIGHT / 2.0f,
      PLAY_BOTTOM - PADDLE_HEIGHT / 2.0f
  );
}

float reflectY(float y) {
  const float top = PLAY_TOP + BALL_RADIUS;
  const float bottom = PLAY_BOTTOM - BALL_RADIUS;
  const float span = bottom - top;
  const float period = span * 2.0f;

  float folded = fmodf(y - top, period);

  if (folded < 0.0f) {
    folded += period;
  }

  if (folded > span) {
    folded = period - folded;
  }

  return top + folded;
}

float predictAIY() {
  if (ball.vx <= 0.0f) {
    return (PLAY_TOP + PLAY_BOTTOM) / 2.0f;
  }

  float travelTime = (RIGHT_PADDLE_X - ball.x) / ball.vx;

  travelTime = travelTime < 0.0f ? 0.0f : travelTime;

  return reflectY(ball.y + ball.vy * travelTime);
}

void moveAI(float dt) {
  float targetY = predictAIY();
  float difference = targetY - rightPaddleY;
  float step = AI_SPEED * dt;

  if (difference > step) {
    rightPaddleY += step;
  } else if (difference < -step) {
    rightPaddleY -= step;
  } else {
    rightPaddleY = targetY;
  }

  rightPaddleY = clampFloat(
    rightPaddleY,
    PLAY_TOP + PADDLE_HEIGHT / 2.0f,
    PLAY_BOTTOM - PADDLE_HEIGHT / 2.0f
  );
}

void updatePaddles(const Buttons &b, float dt) {
  float leftDirection = 0.0f;

  if (b.up != b.down) {
    leftDirection = b.up ? -1.0f : 1.0f;
  }

  movePaddle(leftPaddleY, leftDirection, PADDLE_SPEED, dt);

  if (!twoPlayer) {
    moveAI(dt);
    return;
  }

  float rightDirection = 0.0f;

  if (b.left != b.right) {
    rightDirection = b.left ? -1.0f : 1.0f;
  }

  movePaddle(rightPaddleY, rightDirection, PADDLE_SPEED, dt);
}

void bounceWalls() {
  const float top = PLAY_TOP + BALL_RADIUS;
  const float bottom = PLAY_BOTTOM - BALL_RADIUS;

  if (ball.y <= top) {
    ball.y = top;
    ball.vy = fabsf(ball.vy);
    beep(650, 20);
  } else if (ball.y >= bottom) {
    ball.y = bottom;
    ball.vy = -fabsf(ball.vy);
    beep(650, 20);
  }
}

bool hitPaddle(float paddleY, bool leftSide) {
  if (fabsf(ball.y - paddleY) >
    PADDLE_HEIGHT / 2.0f + BALL_RADIUS) {
    return false;
  }

  if (leftSide) {
    if (ball.vx >= 0.0f ||
      ball.x - BALL_RADIUS > LEFT_PADDLE_X + PADDLE_WIDTH ||
      ball.x + BALL_RADIUS < LEFT_PADDLE_X) {
      return false;
    }

    ball.x = LEFT_PADDLE_X + PADDLE_WIDTH + BALL_RADIUS;
    ball.vx = minFloat(fabsf(ball.vx) + 10.0f, 190.0f);
  } else {
    if (ball.vx <= 0.0f ||
      ball.x + BALL_RADIUS < RIGHT_PADDLE_X ||
      ball.x - BALL_RADIUS > RIGHT_PADDLE_X + PADDLE_WIDTH) {
      return false;
    }

    ball.x = RIGHT_PADDLE_X - BALL_RADIUS;
    ball.vx = -minFloat(fabsf(ball.vx) + 10.0f, 190.0f);
  }

  float impact = clampFloat(
    (ball.y - paddleY) / (PADDLE_HEIGHT / 2.0f),
    -1.0f,
    1.0f
  );

  ball.vy = impact * 145.0f;

  if (fabsf(ball.vy) < 30.0f) {
    ball.vy = ball.vy < 0.0f ? -30.0f : 30.0f;
  }

  beep(leftSide ? 900 : 1100, 30);
  return true;
}

void updateGame(const Buttons &b, float dt) {
  updatePaddles(b, dt);

  ball.x += ball.vx * dt;
  ball.y += ball.vy * dt;

  bounceWalls();

  hitPaddle(leftPaddleY, true);
  hitPaddle(rightPaddleY, false);

  if (ball.x + BALL_RADIUS < 0.0f) {
    rightScore++;
    beep(350, 120);
    resetBall(-1);
    return;
  }

  if (ball.x - BALL_RADIUS > SCREEN_WIDTH) {
    leftScore++;
    beep(1200, 120);
    resetBall(1);
  }
}

void drawGame() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(48, 1);
  display.print(leftScore);

  display.setCursor(74, 1);
  display.print(rightScore);

  for (int y = 13; y < SCREEN_HEIGHT; y += 5) {
    display.drawFastVLine(64, y, 2, SSD1306_WHITE);
  }

  display.fillRect(
    (int)LEFT_PADDLE_X,
    (int)(leftPaddleY - PADDLE_HEIGHT / 2.0f),
    (int)PADDLE_WIDTH,
    (int)PADDLE_HEIGHT,
    SSD1306_WHITE
  );

  display.fillRect(
    (int)RIGHT_PADDLE_X,
    (int)(rightPaddleY - PADDLE_HEIGHT / 2.0f),
    (int)PADDLE_WIDTH,
    (int)PADDLE_HEIGHT,
    SSD1306_WHITE
  );

  display.fillCircle(
    (int)ball.x,
    (int)ball.y,
    (int)BALL_RADIUS,
    SSD1306_WHITE
  );

  display.display();
}

void setup() {
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while (true) {}
  }

  randomSeed(analogRead(A0));

  showSplash();
  drawMenu();

  lastUpdate = millis();
  lastRender = lastUpdate;
}

void loop() {
  Buttons buttons = readButtons();

  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0f;
  lastUpdate = now;

  dt = minFloat(dt, 0.05f);

  if (screenState == MENU) {
    if (buttons.leftPressed != buttons.rightPressed) {
      twoPlayer = !twoPlayer;
      drawMenu();
    }

    if (buttons.upPressed) {
      startGame();
    }

    return;
  }

  updateGame(buttons, dt);

  if (now - lastRender >= 25) {
    lastRender = now;
    drawGame();
  }
}