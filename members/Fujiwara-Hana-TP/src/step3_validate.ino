// Step 3: 入力判定確認
// このコードはキーパッドからの入力が有効か無効かを判定し、シリアルモニタに表示するためのものです。

// キーパッドのピン定義
const int PIN_KEYPAD_ROW[4] = {2, 3, 4, 5};   // Row: D2-D5
const int PIN_KEYPAD_COL[4] = {6, 7, 8, 9};   // Col: D6-D9

// キーパッドのキー配列
const char KEYMAP[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

void setup() {
  // シリアル通信を開始
  Serial.begin(9600);

  // 行ピンを出力に設定し、HIGHに初期化
  for (int i = 0; i < 4; i++) {
    pinMode(PIN_KEYPAD_ROW[i], OUTPUT);
    digitalWrite(PIN_KEYPAD_ROW[i], HIGH);
  }

  // 列ピンを入力プルアップに設定
  for (int i = 0; i < 4; i++) {
    pinMode(PIN_KEYPAD_COL[i], INPUT_PULLUP);
  }

  Serial.println("入力判定確認開始");
}

void loop() {
  int speedInput = handleSpeedInput();
  static unsigned long lastNoInputLog = 0;

  // -2: 未入力, -1: 無効入力, 0-9: 有効入力
  if (speedInput == -2) {
    // 未入力の確認用: 1秒ごとにだけ表示
    unsigned long now = millis();
    if (now - lastNoInputLog >= 1000) {
      Serial.println("未入力");
      lastNoInputLog = now;
    }
    return;
  }

  if (speedInput == -1) {
    Serial.println("無効な入力");
  } else {
    Serial.print("有効な入力(段階): ");
    Serial.println(speedInput);
  }
}

// キーパッドからキーを読み取る関数
char readKeypad() {
  for (int row = 0; row < 4; row++) {
    // すべての行をHIGHに設定
    for (int i = 0; i < 4; i++) {
      digitalWrite(PIN_KEYPAD_ROW[i], HIGH);
    }

    // 現在の行をLOWに設定
    digitalWrite(PIN_KEYPAD_ROW[row], LOW);

    // 各列をチェック
    for (int col = 0; col < 4; col++) {
      if (digitalRead(PIN_KEYPAD_COL[col]) == LOW) {
        return KEYMAP[row][col];
      }
    }
  }

  // 入力がない場合は '\0' を返す
  return '\0';
}

// 入力が有効か無効かを判定する関数
bool validateInput(char key) {
  // '0'〜'9'の範囲なら有効
  if (key >= '0' && key <= '9') {
    return true;
  }

  // それ以外は無効
  return false;
}

// 数字入力を読み取り、段階値を返す関数
// 戻り値: 0〜9(有効入力), -1(無効入力), -2(未入力)
int handleSpeedInput() {
  char key = readKeypad();

  if (key == '\0') {
    return -2;
  }

  if (!validateInput(key)) {
    return -1;
  }

  return (key - '0');
}