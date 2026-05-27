#include <Keypad.h>

// 出力ピン
const uint8_t PIN_MOTOR = 9;
const uint8_t PIN_LED = 10;

// 4x4テンキーパッド
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 12};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// システム状態
enum State {
  ST_WAIT = 0,
  ST_RUN = 1,
  ST_STOP = 2
};

// 入力モード
enum Mode {
  MD_WAIT_A = 0,
  MD_PASS = 1,
  MD_SPEED = 2,
  MD_TIMER = 3,
  MD_MAIN = 4
};

State state = ST_WAIT;
Mode mode = MD_WAIT_A;

bool auth = false;
bool power = false;
uint8_t speed = 0;
uint16_t timerSec = 0;

// パスワード設定
const uint8_t PASSWORD_LENGTH = 4;
const char PASSWORD_CORRECT[PASSWORD_LENGTH + 1] = "1234";
char buf[8] = "";
uint8_t len = 0;

// チャタリング防止と1秒タイマー
unsigned long lastDebounce = 0;
const unsigned long DEBOUNCE_DELAY = 50;
unsigned long lastSecondTick = 0;

// 入力バッファを初期化
void clearBuf() {
  len = 0;
  buf[0] = '\0';
}

// 数字キーか判定
bool isDigit(char k) {
  return k >= '0' && k <= '9';
}

// 無効操作時のLEDフィードバック
void invalid() {
  digitalWrite(PIN_LED, HIGH);
  delay(120);
  digitalWrite(PIN_LED, LOW);
}

// 入力バッファに1文字追加
void pushDigit(char k) {
  if (len >= sizeof(buf) - 1) {
    invalid();
    return;
  }
  buf[len++] = k;
  buf[len] = '\0';
}

// 入力バッファを整数化（不正なら-1）
int toIntBuf() {
  if (len == 0) {
    return -1;
  }

  int v = 0;
  for (uint8_t i = 0; i < len; i++) {
    if (!isDigit(buf[i])) {
      return -1;
    }
    v = v * 10 + (buf[i] - '0');
  }
  return v;
}

// 認証済みかつ電源ONのときのみモーター駆動
void setMotor() {
  int pwm = (auth && power) ? map(speed, 0, 9, 0, 255) : 0;
  analogWrite(PIN_MOTOR, pwm);
}

// LED表示: A待ち/パス入力は点滅、動作中は点灯
void setLed() {
  // パスワード入力画面（A押下時）やモード入力中（速度・タイマー設定時）はLED消灯
  if ((mode == MD_PASS && !auth) || mode == MD_SPEED || mode == MD_TIMER) {
    digitalWrite(PIN_LED, LOW);
    return;
  }
  // パスワード認証成功後やモード入力完了後（MD_MAIN）は常時点灯
  if (auth || mode == MD_MAIN) {
    digitalWrite(PIN_LED, HIGH);
    return;
  }
  // 電源OFF時は消灯
  if (!power) {
    digitalWrite(PIN_LED, LOW);
    return;
  }
  // 電源ON時は1秒ごとに点滅
  digitalWrite(PIN_LED, ((millis() / 500UL) % 2UL) ? LOW : HIGH);
}

// パスワード入力処理
void handlePass(char k) {
  if (isDigit(k)) {
    pushDigit(k);
    // 数字入力時にLEDを一瞬点灯
    digitalWrite(PIN_LED, HIGH);
    delay(120);
    digitalWrite(PIN_LED, LOW);
    return;
  }
  if (k == '*') {
    clearBuf();
    return;
  }
  if (k == '#') {
    if (len != PASSWORD_LENGTH) {
      invalid();
      clearBuf();
      return;
    }

    if (strcmp(buf, PASSWORD_CORRECT) == 0) {
      auth = true;
      mode = MD_MAIN;
      state = ST_STOP;
      Serial.println("Password OK");
    } else {
      auth = false;
      mode = MD_WAIT_A;
      invalid();
      Serial.println("Password NG");
    }
    clearBuf();
    return;
  }

  invalid();
}

// 速度設定処理（0-9）
void handleSpeed(char k) {
  if (isDigit(k)) {
    clearBuf();
    pushDigit(k);
    return;
  }
  if (k == '*') {
    clearBuf();
    return;
  }
  if (k == '#') {
    int v = toIntBuf();
    if (v < 0 || v > 9) {
      invalid();
    } else {
      speed = (uint8_t)v;
      Serial.print("Speed set: ");
      Serial.println(speed);
    }
    clearBuf();
    mode = MD_MAIN;
    return;
  }

  invalid();
}

// タイマー設定処理（1-300分）
void handleTimer(char k) {
  if (isDigit(k)) {
    if (len >= 3) {
      invalid();
      return;
    }
    pushDigit(k);
    return;
  }
  if (k == '*') {
    clearBuf();
    return;
  }
  if (k == '#') {
    int m = toIntBuf();
    if (m < 1) m = 1;
    if (m > 300) m = 300;
    timerSec = (uint16_t)(m * 60);
    Serial.print("Timer set (sec): ");
    Serial.println(timerSec);
    clearBuf();
    mode = MD_MAIN;
    return;
  }

  invalid();
}

// 認証後のメイン操作（B/C/D）
void handleMain(char k) {
  if (k == 'B') {
    mode = MD_SPEED;
    clearBuf();
    return;
  }
  if (k == 'C') {
    mode = MD_TIMER;
    clearBuf();
    return;
  }
  if (k == 'D') {
    power = !power;
    state = power ? ST_RUN : ST_STOP;
    Serial.print("Power: ");
    Serial.println(power ? "ON" : "OFF");
    return;
  }
  invalid();
}

// 1キー入力を現在モードに応じて処理
void processKey(char k) {
  unsigned long now = millis();
  // デバウンス
  if (now - lastDebounce < DEBOUNCE_DELAY) {
    return;
  }
  lastDebounce = now;

  // 起動直後または未認証時はDボタンでON/OFFのみ許可
  if (!power && k == 'D') {
    power = !power;
    state = power ? ST_RUN : ST_STOP;
    Serial.print("Power: ");
    Serial.println(power ? "ON" : "OFF");
    return;
  }

  // 電源ONかつ未認証時はAでパスワード入力のみ許可
  if (power && !auth) {
    if (k == 'A') {
      mode = MD_PASS;
      clearBuf();
      Serial.println("Password input started");
    } else {
      invalid();
    }
    return;
  }

  // 認証後は各モードに遷移可能
  switch (mode) {
    case MD_PASS:
      handlePass(k);
      break;

    case MD_SPEED:
      if (auth) handleSpeed(k); else invalid();
      break;

    case MD_TIMER:
      if (auth) handleTimer(k); else invalid();
      break;

    case MD_MAIN:
      if (auth) handleMain(k); else invalid();
      break;

    default:
      // 認証済みならB/C/Dで各モードやON/OFF
      if (auth) handleMain(k); else invalid();
      break;
  }
}

// 動作中のみ1秒ごとに残り時間を減算
void updateTimer() {
  if (!power || timerSec == 0) {
    return;
  }

  unsigned long now = millis();
  if (now - lastSecondTick < 1000UL) {
    return;
  }

  lastSecondTick = now;
  timerSec--;
  if (timerSec == 0) {
    power = false;
    state = ST_STOP;
    Serial.println("Timer ended: power OFF");
  }
}

// 初期化
void setup() {
  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_MOTOR, 0);
  digitalWrite(PIN_LED, LOW);
  Serial.begin(9600);

  digitalWrite(PIN_LED, HIGH);
  delay(300);
  digitalWrite(PIN_LED, LOW);

  state = ST_WAIT;
  mode = MD_WAIT_A;
  auth = false;
  power = false;
  speed = 0;
  timerSec = 0;
  clearBuf();

  lastDebounce = 0;
  lastSecondTick = millis();

  Serial.println("System ready");
}

// メインループ
void loop() {
  char k = keypad.getKey();
  if (k != NO_KEY) {
    processKey(k);
  }

  updateTimer();
  setMotor();
  setLed();
}
