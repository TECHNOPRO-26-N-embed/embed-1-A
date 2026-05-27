// Step 6: キーパッドデバウンス付き統合テスト（設計書準拠）
// ----------------------------------------------------------
// 主要な流れ：
//   1. キーパッド入力を50ms周期で監視
//   2. 入力値を段階（0-9）としてPWM値に変換
//   3. モータ回転速度を制御
//   4. 無効入力はエラー状態で停止
//   5. すべての状態遷移・出力はシリアルモニタに表示
// ----------------------------------------------------------

// --- ピン定義 ---
const int PIN_KEYPAD_ROW[4] = {2, 3, 4, 5};   // キーパッド行（D2-D5）
const int PIN_KEYPAD_COL[4] = {6, 7, 8, 9};   // キーパッド列（D6-D9）
const int PIN_MOTOR_PWM = 10;                 // モータPWM出力（D10）
const int PIN_MOTOR_IN1 = 11;                 // モータIN1（方向制御）
const int PIN_MOTOR_IN2 = 12;                 // モータIN2（方向制御）

// --- 状態・入力・出力・タイマー ---
int currentState = 0;       // 0:待機 1:入力判定中 2:回転速度制御中 3:異常入力時
char keyInput = '\0';      // キーパッドからの入力値
int currentLevel = -1;      // 現在保持している入力段階（0〜9:有効入力, -1:無効入力, -2:未入力）
int pwmValue = 0;           // ファン制御用のPWM値
int lastValidValue = 0;     // 直前の有効な入力値
unsigned long lastMillis = 0; // デバウンス・周期管理用
bool errorFlag = false;     // 無効入力や異常値の検出フラグ

const unsigned long debounceDelay = 50; // デバウンス遅延時間（ms）

// --- 初期化処理 ---
void setup() {
  Serial.begin(9600); // シリアルモニタ出力開始
  // キーパッド行・列ピンの初期化
  for (int i = 0; i < 4; i++) {
    pinMode(PIN_KEYPAD_ROW[i], OUTPUT);
    digitalWrite(PIN_KEYPAD_ROW[i], HIGH); // 行は初期値HIGH
    pinMode(PIN_KEYPAD_COL[i], INPUT_PULLUP); // 列はプルアップ入力
  }
  // モータ制御ピンの初期化
  pinMode(PIN_MOTOR_PWM, OUTPUT);
  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  // 変数初期化
  currentState = 0;
  keyInput = '\0';
  currentLevel = -1;
  pwmValue = 0;
  lastValidValue = 0;
  lastMillis = 0;
  errorFlag = false;
  Serial.println("起動");
}

// --- メインループ ---
void loop() {
  unsigned long now = millis();
  // デバウンス：50ms周期でのみ入力判定
  if (now - lastMillis < debounceDelay) return;
  lastMillis = now;

  switch (currentState) {
    case 0: // 【待機状態】
      // キーパッド入力を監視し、0-9なら次状態（case 1）へ、-1は異常（case 3）、-2は未入力（case 0）
      currentLevel = handleSpeedInput();
      if (currentLevel >= 0 && currentLevel <= 9) {
        currentState = 1;
        errorFlag = false;
      } else if (currentLevel == -1) {
        currentState = 3;
        errorFlag = true;
      }
      // 未入力(-2)は何もしない
      break;
    case 1: // 【入力判定中】
      // 入力済みcurrentLevelをそのまま採用し、制御状態へ
      currentState = 2;
      break;
    case 2: // 【回転速度制御中】
      // 入力値0なら停止、それ以外はPWM値を更新
      handleStopRequest(currentLevel);
      if (currentLevel != 0) updateFanSpeedByLevel(currentLevel);
      currentState = 0;
      break;
    case 3: // 【異常入力時】
      // モータ停止＆エラー表示、正常値入力で復帰
      applyFanSpeed(0);
      printStatus(currentLevel, true);
      currentLevel = handleSpeedInput();
      if (currentLevel >= 0 && currentLevel <= 9) {
        errorFlag = false;
        currentState = 1;
      }
      // 無効/未入力は3のまま
      break;
  }
}

// --- キーマップ配列 ---
const char KEYMAP[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// --- キーパッド入力取得（デバウンス付き） ---
// 4x4キーパッドの行・列をスキャンし、押されたキーを返す
// 押されていなければ'\0'を返す
char readKeypad() {
  for (int row = 0; row < 4; row++) {
    // すべての行をHIGHに戻す
    for (int i = 0; i < 4; i++) digitalWrite(PIN_KEYPAD_ROW[i], HIGH);
    // 1行だけLOWにしてスキャン
    digitalWrite(PIN_KEYPAD_ROW[row], LOW);
    for (int col = 0; col < 4; col++) {
      if (digitalRead(PIN_KEYPAD_COL[col]) == LOW) {
        // キーマップ配列から該当文字を返す
        return KEYMAP[row][col];
      }
    }
  }
  return '\0';
}

// --- 入力値が0-9か判定 ---
bool validateInput(char key) {
  return (key >= '0' && key <= '9');
}

// --- キーパッド入力を段階値に変換 ---
// 0-9: 段階値として返す, 無効: -1, 未入力: -2
int handleSpeedInput() {
  char newKey = readKeypad();
  if (newKey == '\0') return -2;      // 未入力
  if (!validateInput(newKey)) return -1; // 無効入力
  keyInput = newKey;
  return keyInput - '0';
}

// --- 段階値をPWM値（0-255）に変換 ---
int convertToPwm(int level) {
  if (level < 0 || level > 9) return 0;
  // 0:停止, 1-9:低速→高速
  const int pwmValues[10] = {0, 74, 78, 95, 115, 135, 160, 190, 220, 255};
  return pwmValues[level];
}

// --- モータにPWM値を適用（正転のみ） ---
// PWM値が範囲外なら0～255に補正して出力
void applyFanSpeed(int pwm) {
  if (pwm < 0) pwm = 0;
  if (pwm > 255) pwm = 255;
  pwmValue = pwm;
  digitalWrite(PIN_MOTOR_IN1, HIGH);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  analogWrite(PIN_MOTOR_PWM, pwm);
}

// --- 現在状態・エラーをシリアル出力 ---
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

// --- 入力値0ならファン停止 ---
void handleStopRequest(int level) {
  if (level == 0) {
    applyFanSpeed(0);
    Serial.println("ファン停止");
  }
}

// --- 入力段階に応じてファン速度を更新 ---
void updateFanSpeedByLevel(int level) {
  lastValidValue = level;
  int pwm = convertToPwm(level);
  applyFanSpeed(pwm);
  printStatus(level, false);
}
