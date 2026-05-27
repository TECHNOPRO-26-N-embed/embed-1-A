// Step 2: キーパッド入力確認
// このコードはキーパッドの入力を読み取り、シリアルモニタに表示するためのものです。

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

  Serial.println("キーパッド入力確認開始");
}

void loop() {
  char key = readKeypad();

  // 入力があれば表示
  if (key != '\0') {
    Serial.print("入力されたキー: ");
    Serial.println(key);
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