// Step 1: 最小起動確認
// このコードはArduinoが正しく起動し、シリアル通信が動作するかを確認するためのものです。

void setup() {
  // シリアル通信を開始
  Serial.begin(9600);

  // 起動メッセージを送信
  Serial.println("Arduino 起動完了");
}

void loop() {
  // メインループは空のまま
}