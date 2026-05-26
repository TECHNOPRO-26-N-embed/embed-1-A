// Step 4: L293DでPWM出力と方向制御を確認
// EN1にPWM、IN1/IN2に方向信号を与えてモーターの動作を確認します。

const int PIN_MOTOR_EN1 = 10; // EN1 (PWM出力)
const int PIN_MOTOR_IN1 = 11; // IN1 (方向制御)
const int PIN_MOTOR_IN2 = 12; // IN2 (方向制御)

void setup() {
  Serial.begin(9600);
  pinMode(PIN_MOTOR_EN1, OUTPUT);
  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  Serial.println("L293Dモーター制御テスト開始");
}

void loop() {
  // 正転: 0〜9の段階値を順にPWM出力
  digitalWrite(PIN_MOTOR_IN1, HIGH);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  Serial.println("正転: 速度を段階的に上げる");
  for (int level = 0; level <= 9; level++) {
    int pwm = convertToPwm(level);
    analogWrite(PIN_MOTOR_EN1, pwm);
    Serial.print("level=");
    Serial.print(level);
    Serial.print(", pwm=");
    Serial.println(pwm);
    delay(1000);
  }

  // 逆転: 9〜0の段階値を順にPWM出力
  digitalWrite(PIN_MOTOR_IN1, LOW);
  digitalWrite(PIN_MOTOR_IN2, HIGH);
  Serial.println("逆転: 速度を段階的に下げる");
  for (int level = 9; level >= 0; level--) {
    int pwm = convertToPwm(level);
    analogWrite(PIN_MOTOR_EN1, pwm);
    Serial.print("level=");
    Serial.print(level);
    Serial.print(", pwm=");
    Serial.println(pwm);
    delay(1000);
  }

  // モーター停止
  analogWrite(PIN_MOTOR_EN1, 0);
  digitalWrite(PIN_MOTOR_IN1, LOW);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  Serial.println("モーター停止");
  delay(2000);
}

// 段階値（0〜9）をPWM値（0〜255）へ変換
int convertToPwm(int level) {
  if (level < 0 || level > 9) {
    return 0;
  }
  // level=0は0、level=1が50、level=2が78、...、level=9が255
  const int pwmValues[10] = {0, 50, 78, 106, 134, 162, 190, 218, 246, 255};
  return pwmValues[level];
}
