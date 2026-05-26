#include <Keypad.h>

// ===== キーパッド設定 =====
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {2,3,4,5};
byte colPins[COLS] = {6,7,8,9};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== ピン =====
#define PIN_LED 10
#define PIN_BUZZER 11

// ===== 状態 =====
#define STATE_IDLE    0
#define STATE_INPUT   1
#define STATE_CHECK   2
#define STATE_SUCCESS 3
#define STATE_ERROR   4
#define STATE_LOCK    5

int currentState = STATE_IDLE;

// ===== パスワード =====
char inputCode[5] = "";
char correctCode[5] = "1234";
int inputIndex = 0;

// ===== ミス管理 =====
int errorCount = 0;
bool isLocked = false;

// ===== タイマー =====
unsigned long lockStart = 0;

const unsigned long LOCK_TIME = 10000;

// ===== 関数プロトタイプ（これが今回の修正ポイント）=====
void inputNumber(char key);
void confirmInput();
bool checkPassword();
void successAction();
void errorAction();
void resetInput();

// =========================
// 初期化
// =========================
void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  Serial.begin(9600);
  Serial.println("System Start");
}

// =========================
// メイン
// =========================
void loop() {

  char key = keypad.getKey();

  // ===== ロック処理 =====
  if (isLocked) {
    if (millis() - lockStart >= LOCK_TIME) {
      isLocked = false;
      errorCount = 0;
      currentState = STATE_IDLE;
      Serial.println("LOCK解除");
    }
    return;
  }

  // ===== 入力処理 =====
  if (key) {
    Serial.print("Key: ");
    Serial.println(key);

    if (key >= '0' && key <= '9') {
      inputNumber(key);
      currentState = STATE_INPUT;

      Serial.print("Input: ");
      Serial.println(inputCode);
    }

    if (key == '#') {
      confirmInput();
    }
  }

  // ===== 判定 =====
  if (currentState == STATE_CHECK) {
    if (checkPassword()) {
      Serial.println("SUCCESS");
      currentState = STATE_SUCCESS;
    } else {
      Serial.println("ERROR");
      currentState = STATE_ERROR;
    }
  }

  // ===== 出力 =====
  if (currentState == STATE_SUCCESS) {
    successAction();
  }

  if (currentState == STATE_ERROR) {
    errorAction();
  }
}

// =========================
// 数字入力
// =========================
void inputNumber(char key) {
  if (inputIndex < 4) {
    inputCode[inputIndex] = key;
    inputIndex++;
    inputCode[inputIndex] = '\0';
  }
}

// =========================
// 入力確定
// =========================
void confirmInput() {
  if (inputIndex == 0) return;

  Serial.println("CONFIRM");
  currentState = STATE_CHECK;
}

// =========================
// 判定
// =========================
bool checkPassword() {
  return strcmp(inputCode, correctCode) == 0;
}

// =========================
// 正解処理
// =========================
void successAction() {
  digitalWrite(PIN_LED, HIGH);
  delay(1000);
  digitalWrite(PIN_LED, LOW);

  errorCount = 0;
  resetInput();
  currentState = STATE_IDLE;
}

// =========================
// エラー処理
// =========================
void errorAction() {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(300);
  digitalWrite(PIN_BUZZER, LOW);

  errorCount++;

  Serial.print("ErrorCount: ");
  Serial.println(errorCount);

  if (errorCount >= 3) {
    isLocked = true;
    lockStart = millis();
    currentState = STATE_LOCK;
    Serial.println("LOCK状態");
  } else {
    currentState = STATE_IDLE;
  }

  resetInput();
}

// =========================
// 入力リセット
// =========================
void resetInput() {
  inputIndex = 0;
  inputCode[0] = '\0';
}