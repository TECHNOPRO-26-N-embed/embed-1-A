// Step 5: キーパッド入力とモーター制御の統合
// 状態遷移・入力バリデーション・エラー表示を含む

// --- ピン定義 ---
const int PIN_KEYPAD_ROW[4] = {2, 3, 4, 5};   // Row: D2-D5
const int PIN_KEYPAD_COL[4] = {6, 7, 8, 9};   // Col: D6-D9
const int PIN_MOTOR_EN1 = 10; // EN1 (PWM出力)
const int PIN_MOTOR_IN1 = 11; // IN1 (方向制御)
const int PIN_MOTOR_IN2 = 12; // IN2 (方向制御)

// --- 状態管理 ---
int currentState = 0; // 0:待機 1:入力判定中 2:回転速度制御中 3:異常入力時
int currentLevel = -1; // 現在の入力段階
bool errorFlag = false;

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 4; i++) {
    pinMode(PIN_KEYPAD_ROW[i], OUTPUT);
    digitalWrite(PIN_KEYPAD_ROW[i], HIGH);
    pinMode(PIN_KEYPAD_COL[i], INPUT_PULLUP);
  }
  pinMode(PIN_MOTOR_EN1, OUTPUT);
  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  Serial.println("step5: キーパッド+モーター統合テスト開始");
}

void loop() {
  switch (currentState) {
    case 0: // 待機
      currentLevel = handleSpeedInput();
      if (currentLevel >= 0 && currentLevel <= 9) {
        currentState = 2;
        errorFlag = false;
      } else if (currentLevel == -1) {
        currentState = 3;
        errorFlag = true;
      }
      // 未入力(-2)は何もしない
      break;
    case 2: // 回転速度制御中
      applyFanSpeed(currentLevel);
      printStatus(currentLevel, false);
      currentState = 0;
      break;
    case 3: // 異常入力時
      applyFanSpeed(0);
      printStatus(currentLevel, true);
      currentLevel = handleSpeedInput();
      if (currentLevel >= 0 && currentLevel <= 9) {
        currentState = 2;
        errorFlag = false;
      }
      // 無効/未入力は3のまま
      break;
  }
}

// --- キーパッド入力取得 ---
char readKeypad() {
  for (int row = 0; row < 4; row++) {
    for (int i = 0; i < 4; i++) digitalWrite(PIN_KEYPAD_ROW[i], HIGH);
    digitalWrite(PIN_KEYPAD_ROW[row], LOW);
    for (int col = 0; col < 4; col++) {
      if (digitalRead(PIN_KEYPAD_COL[col]) == LOW) {
        return "123A456B789C*0#D"[row * 4 + col];
      }
    }
  }
  return '\0';
}

// --- 入力バリデーション ---
bool validateInput(char key) {
  return (key >= '0' && key <= '9');
}

// --- 入力段階取得 ---
// 0-9:有効, -1:無効, -2:未入力
int handleSpeedInput() {
  char key = readKeypad();
  if (key == '\0') return -2;
  if (!validateInput(key)) return -1;
  return key - '0';
}

// --- PWM値変換 ---
int convertToPwm(int level) {
  if (level < 0 || level > 9) return 0;
  const int pwmValues[10] = {0, 50, 78, 106, 134, 162, 190, 218, 246, 255};
  return pwmValues[level];
}

// --- モーター制御 ---
void applyFanSpeed(int level) {
  int pwm = convertToPwm(level);
  // 正転のみ（必要に応じて逆転も可）
  digitalWrite(PIN_MOTOR_IN1, HIGH);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  analogWrite(PIN_MOTOR_EN1, pwm);
}

// --- ステータス表示 ---
void printStatus(int level, bool isError) {
  if (isError) {
    Serial.println("エラー: 無効な入力");
  } else {
    Serial.print("現在段階: ");
    Serial.print(level);
    Serial.print(", pwm=");
    Serial.println(convertToPwm(level));
  }
}
