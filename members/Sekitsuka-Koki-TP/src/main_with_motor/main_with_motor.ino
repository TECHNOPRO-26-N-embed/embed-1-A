// 4桁コード推理ゲーム
// 詳細設計書に基づく Arduino 実装 + L293Dモーター制御追加

#include <Keypad.h>

// ピン定義
const byte ROWS = 4; // 行数
const byte COLS = 4; // 列数
const char keys[ROWS][COLS] = {
	{'1', '2', '3', 'A'},
	{'4', '5', '6', 'B'},
	{'7', '8', '9', 'C'},
	{'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6}; // 行ピン
byte colPins[COLS] = {5, 4, 3, 2}; // 列ピン
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LED ピン
const int PIN_LED_RED = 10;
const int PIN_LED_YELLOW = 11;
const int PIN_LED_GREEN = 12;
const int PIN_BUZZER = A0;

// モーター制御ピン
const int MOTOR_EN_PIN = A2;
const int MOTOR_IN1_PIN = A3;
const int MOTOR_IN2_PIN = A4;

// モーター制御用タイマー
unsigned long motorStartMillis = 0;
bool motorRunning = false;
bool clearEffect_done = false;

// 状態管理
byte currentState = 0; // 0:入力待ち, 1:判定表示, 2:クリア, 3:ミス入力通知

// データ管理
byte secretCode[4] = {0, 0, 0, 0};
byte inputCode[4] = {0, 0, 0, 0};
byte judgeResult[4] = {0, 0, 0, 0};
bool judgeDisplayDone = false;
byte inputLength = 0;
byte attemptCount = 0;
byte ledIndex = 0;

// タイマー管理
unsigned long lastKeyMillis = 0;
unsigned long lastLedMillis = 0;
unsigned long lastDebounceTime = 0;

// 定数
const int DEBOUNCE_DELAY = 50;
const int KEYPAD_INTERVAL = 10;
const int INVALID_BLINK_MS = 500;
const int JUDGE_LED_ON_MS = 1000;
const int JUDGE_LED_OFF_MS = 500;
const int MAX_ATTEMPTS = 10;

void setup() {
	Serial.begin(9600); // デバッグ用
	Serial.println("===== 4桁コード推理ゲーム開始 =====");

	// 未接続アナログピンのノイズをシードに使い、起動ごとに乱数列を変える
	randomSeed(analogRead(A1));

	// ピンモード設定
	pinMode(PIN_LED_RED, OUTPUT);
	pinMode(PIN_LED_YELLOW, OUTPUT);
	pinMode(PIN_LED_GREEN, OUTPUT);
	pinMode(PIN_BUZZER, OUTPUT);

	// モーター制御ピン
	pinMode(MOTOR_EN_PIN, OUTPUT);
	pinMode(MOTOR_IN1_PIN, OUTPUT);
	pinMode(MOTOR_IN2_PIN, OUTPUT);
	motorStop();
	motorRunning = false;

	// 初期化
	resetGame();
}

void loop() {
	unsigned long now = millis();

	// キーパッド入力処理
	char key = readKeypad(now);
	if (key != '\0') {
		handleInputKey(key);
	}

	// モーター処理はゲーム状態と分離して監視する
	if (motorRunning && (now - motorStartMillis >= 10000)) {
		motorStop();
		motorRunning = false;
		clearEffect_done = false;
		resetGame();
		currentState = 0;
	}

	// 状態ごとの処理
	switch (currentState) {
		case 0: // 入力待ち
			break;

		case 1: // 判定表示
			updateLedOutput(now);
			if (judgeDisplayDone) {
				judgeDisplayDone = false;
				if (isAllGreenResult()) {
					playClearTone();
					Serial.print("【クリア】");
					Serial.print(attemptCount);
					Serial.println("回目で正解！");
					currentState = 2; // クリア演出へ
				} else {
					if (attemptCount >= MAX_ATTEMPTS) {
						playGameOverEffect();
						Serial.println("【ゲームオーバー】規定回数に到達しました。リセットします。");
						resetGame();
					} else {
						clearInputBuffer();
						currentState = 0; // 表示完了後、入力待ちに戻る
					}
				}
			}
			break;

		case 2: // クリア
			if (!clearEffect_done) {
				playClearWave(); // LED演出のみ
				motorForward(200);
				motorStartMillis = now;
				motorRunning = true;
				clearEffect_done = true;
			}
			break;

		case 3: // ミス入力通知
			notifyInvalidInput(now);
			break;
	}
}

// --- 以降はmain.inoの関数をそのまま貼り付け ---

char readKeypad(unsigned long now) {
	if (now - lastKeyMillis < KEYPAD_INTERVAL) return '\0';
	char key = keypad.getKey();
	if (key && now - lastDebounceTime >= DEBOUNCE_DELAY) {
		lastDebounceTime = now;
		lastKeyMillis = now;
		return key;
	}
	return '\0';
}

void handleInputKey(char key) {
	if (currentState != 0) {
		return;
	}
	if (key >= '0' && key <= '9') {
		updateInputBuffer(key);
	} else if (key == '*') {
		applyEditKey();
	} else if (key == '#') {
		startJudgeIfReady();
	}
}

void updateInputBuffer(char key) {
	if (inputLength >= 4) {
		playWarningTone();
		blinkLed(PIN_LED_RED, 100);
		return;
	}
	inputCode[inputLength++] = key - '0';
	playInputTone();
	Serial.print("入力：");
	for (int i = 0; i < inputLength; i++) {
		Serial.print(inputCode[i]);
	}
	Serial.println();
	blinkLed(PIN_LED_GREEN, 100);
}

void applyEditKey() {
	if (inputLength > 0) {
		inputCode[--inputLength] = 0;
		playDeleteTone();
		Serial.print("入力：");
		for (int i = 0; i < inputLength; i++) {
			Serial.print(inputCode[i]);
		}
		Serial.println(" (削除)");
		blinkLed(PIN_LED_YELLOW, 100);
	}
}

void startJudgeIfReady() {
	if (inputLength == 4) {
		attemptCount++;
		judgeAndStoreResult();
		currentState = 1;
	} else {
		playWarningTone();
		currentState = 3;
	}
}

void judgeAndStoreResult() {
	bool usedSecret[4] = {false, false, false, false};
	bool usedInput[4] = {false, false, false, false};
	for (int i = 0; i < 4; i++) {
		judgeResult[i] = 0;
	}
	for (int i = 0; i < 4; i++) {
		if (inputCode[i] == secretCode[i]) {
			judgeResult[i] = 2;
			usedSecret[i] = true;
			usedInput[i] = true;
		}
	}
	for (int i = 0; i < 4; i++) {
		if (!usedInput[i]) {
			for (int j = 0; j < 4; j++) {
				if (!usedSecret[j] && inputCode[i] == secretCode[j]) {
					judgeResult[i] = 1;
					usedSecret[j] = true;
					break;
				}
			}
		}
	}
	Serial.print("[");
	Serial.print(attemptCount);
	Serial.print("回目] 判定結果：");
	for (int i = 0; i < 4; i++) {
		char symbol = 'X';
		if (judgeResult[i] == 2) {
			symbol = 'O';
		} else if (judgeResult[i] == 1) {
			symbol = 'A';
		}
		Serial.print(symbol);
		if (i < 3) {
			Serial.print(' ');
		}
	}
	Serial.println();
	ledIndex = 0;
	judgeDisplayDone = false;
	lastLedMillis = 0;
}

void updateLedOutput(unsigned long now) {
	static bool ledOnPhase = false;
	if (ledOnPhase) {
		if (now - lastLedMillis < JUDGE_LED_ON_MS) return;
		digitalWrite(PIN_LED_RED, LOW);
		digitalWrite(PIN_LED_YELLOW, LOW);
		digitalWrite(PIN_LED_GREEN, LOW);
		ledOnPhase = false;
		lastLedMillis = now;
		ledIndex++;
		if (ledIndex >= 4) {
			judgeDisplayDone = true;
			ledIndex = 0;
			lastLedMillis = 0;
		}
		return;
	}
	if (lastLedMillis != 0 && now - lastLedMillis < JUDGE_LED_OFF_MS) return;
	digitalWrite(PIN_LED_RED, LOW);
	digitalWrite(PIN_LED_YELLOW, LOW);
	digitalWrite(PIN_LED_GREEN, LOW);
	if (judgeResult[ledIndex] == 2) {
		digitalWrite(PIN_LED_GREEN, HIGH);
	} else if (judgeResult[ledIndex] == 1) {
		digitalWrite(PIN_LED_YELLOW, HIGH);
	} else {
		digitalWrite(PIN_LED_RED, HIGH);
	}
	playJudgeTone(judgeResult[ledIndex]);
	ledOnPhase = true;
	lastLedMillis = now;
}

void notifyInvalidInput(unsigned long now) {
	static unsigned long startTime = 0;
	if (startTime == 0) startTime = now;
	if (now - startTime < INVALID_BLINK_MS) {
		digitalWrite(PIN_LED_RED, HIGH);
		delay(100);
		digitalWrite(PIN_LED_RED, LOW);
		delay(100);
	} else {
		startTime = 0;
		currentState = 0;
	}
}

void playClearWave() {
	for (int i = 0; i < 3; i++) {
		digitalWrite(PIN_LED_RED, HIGH);
		delay(100);
		digitalWrite(PIN_LED_RED, LOW);
		digitalWrite(PIN_LED_YELLOW, HIGH);
		delay(100);
		digitalWrite(PIN_LED_YELLOW, LOW);
		digitalWrite(PIN_LED_GREEN, HIGH);
		delay(100);
		digitalWrite(PIN_LED_GREEN, LOW);
	}
}

void blinkLed(int pin, int durationMs) {
	digitalWrite(pin, HIGH);
	delay(durationMs);
	digitalWrite(pin, LOW);
}

void clearInputBuffer() {
	for (int i = 0; i < 4; i++) {
		inputCode[i] = 0;
	}
	inputLength = 0;
}

bool isAllGreenResult() {
	for (int i = 0; i < 4; i++) {
		if (judgeResult[i] != 2) {
			return false;
		}
	}
	return true;
}

void playInputTone() {
	tone(PIN_BUZZER, 1760, 60);
	delay(80);
	noTone(PIN_BUZZER);
}

void playDeleteTone() {
	tone(PIN_BUZZER, 440, 70);
	delay(90);
	noTone(PIN_BUZZER);
}

void playWarningTone() {
	tone(PIN_BUZZER, 220, 120);
	delay(150);
	tone(PIN_BUZZER, 220, 120);
	delay(170);
	noTone(PIN_BUZZER);
}

void playClearTone() {
	tone(PIN_BUZZER, 988, 120);
	delay(150);
	tone(PIN_BUZZER, 1319, 120);
	delay(150);
	tone(PIN_BUZZER, 1760, 260);
	delay(290);
	noTone(PIN_BUZZER);
}

void playJudgeTone(byte result) {
	if (result == 2) {
		tone(PIN_BUZZER, 1319, 80);
		delay(100);
		tone(PIN_BUZZER, 1760, 220);
		delay(240);
	} else if (result == 1) {
		tone(PIN_BUZZER, 1175, 100);
		delay(120);
	} else {
		tone(PIN_BUZZER, 262, 80);
		delay(100);
		tone(PIN_BUZZER, 196, 240);
		delay(260);
	}
	noTone(PIN_BUZZER);
}

void playGameOverEffect() {
	for (int i = 0; i < 3; i++) {
		digitalWrite(PIN_LED_RED, HIGH);
		tone(PIN_BUZZER, 196, 180);
		delay(220);
		digitalWrite(PIN_LED_RED, LOW);
		noTone(PIN_BUZZER);
		delay(140);
	}
}

void resetGame() {
	bool usedDigits[10] = {false, false, false, false, false, false, false, false, false, false};
	for (int i = 0; i < 4; i++) {
		byte d;
		do {
			d = (byte)random(0, 10);
		} while (usedDigits[d]);
		usedDigits[d] = true;
		secretCode[i] = d;
		inputCode[i] = 0;
		judgeResult[i] = 0;
	}
	judgeDisplayDone = false;
	ledIndex = 0;
	inputLength = 0;
	attemptCount = 0;
	currentState = 0;
}

// --- モーター制御関数 ---
void motorForward(int speed) {
	digitalWrite(MOTOR_IN1_PIN, HIGH);
	digitalWrite(MOTOR_IN2_PIN, LOW);
	analogWrite(MOTOR_EN_PIN, speed); // 0-255
}

void motorStop() {
	analogWrite(MOTOR_EN_PIN, 0);
}
