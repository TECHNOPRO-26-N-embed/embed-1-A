#include <Arduino.h>

const int PIN_LED_GREEN = 10;
const int PIN_LED_RED = 11;

const byte ROWS = 4;
const byte COLS = 4;

const char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

const int STATE_IDLE = 0;
const int STATE_INPUT = 1;
const int STATE_CHECK = 2;
const int STATE_RESULT = 3;

int currentState = STATE_IDLE;

char inputBuffer[5] = "";
int inputLength = 0;
const char password[5] = "1234";
bool result = false;

unsigned long lastInputMillis = 0;
unsigned long lastResultMillis = 0;
unsigned long lastKeyMillis = 0;

const unsigned long INPUT_TIMEOUT_MS = 5000UL;
const unsigned long RESULT_HOLD_MS = 1000UL;
const unsigned long DEBOUNCE_DELAY = 50UL;

char lastKey = '\0';

char rawReadKeypad();
char readKeypad();
void limitInputLength(char key);
bool checkPassword(const char* input, const char* correct);
void showResult(bool isCorrect);
void autoLedOff(unsigned long now);
void resetInputBuffer();
void turnOffLeds();
bool isDigitKey(char key);

void setup() {
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  turnOffLeds();

  for (byte r = 0; r < ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH);
  }

  for (byte c = 0; c < COLS; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }

  resetInputBuffer();
  currentState = STATE_IDLE;

  Serial.begin(9600);
  Serial.println("Password judge device ready.");

  digitalWrite(PIN_LED_GREEN, HIGH);
  delay(200);
  digitalWrite(PIN_LED_GREEN, LOW);
}

void loop() {
  unsigned long now = millis();
  char key = readKeypad();

  autoLedOff(now);

  switch (currentState) {
    case STATE_IDLE:
      turnOffLeds();

      if (isDigitKey(key)) {
        limitInputLength(key);
        currentState = STATE_INPUT;
        lastInputMillis = now;
      }
      break;

    case STATE_INPUT:
      if (isDigitKey(key)) {
        limitInputLength(key);
        lastInputMillis = now;
      } else if (key == '*') {
        resetInputBuffer();
        currentState = STATE_IDLE;
        Serial.println("Input canceled.");
      } else if (key == '#') {
        if (inputLength == 4) {
          currentState = STATE_CHECK;
        } else {
          Serial.println("Need 4 digits before #.");
        }
      }

      if (now - lastInputMillis >= INPUT_TIMEOUT_MS) {
        resetInputBuffer();
        currentState = STATE_IDLE;
        Serial.println("Input timeout.");
      }
      break;

    case STATE_CHECK:
      result = checkPassword(inputBuffer, password);
      showResult(result);
      lastResultMillis = now;
      currentState = STATE_RESULT;
      Serial.println(result ? "Result: CORRECT" : "Result: WRONG");
      break;

    case STATE_RESULT:
      // Result hold and return are handled by autoLedOff().
      break;

    default:
      resetInputBuffer();
      turnOffLeds();
      currentState = STATE_IDLE;
      break;
  }
}

char rawReadKeypad() {
  for (byte r = 0; r < ROWS; r++) {
    digitalWrite(rowPins[r], LOW);

    for (byte c = 0; c < COLS; c++) {
      if (digitalRead(colPins[c]) == LOW) {
        digitalWrite(rowPins[r], HIGH);
        return hexaKeys[r][c];
      }
    }

    digitalWrite(rowPins[r], HIGH);
  }

  return '\0';
}

char readKeypad() {
  char key = rawReadKeypad();
  if (key == '\0') {
    // Key release resets hold state so next press is accepted once.
    lastKey = '\0';
    return '\0';
  }

  if (!(isDigitKey(key) || key == '*' || key == '#')) {
    return '\0';
  }

  unsigned long now = millis();
  if (key == lastKey) {
    return '\0';
  }

  if ((now - lastKeyMillis) < DEBOUNCE_DELAY) {
    return '\0';
  }

  lastKey = key;
  lastKeyMillis = now;
  return key;
}

void limitInputLength(char key) {
  if (!isDigitKey(key)) {
    return;
  }

  if (inputLength >= 4) {
    return;
  }

  inputBuffer[inputLength] = key;
  inputLength++;
  inputBuffer[inputLength] = '\0';

  Serial.print("Input: ");
  Serial.println(inputBuffer);
}

bool checkPassword(const char* input, const char* correct) {
  if (input == nullptr || correct == nullptr) {
    return false;
  }

  if (inputLength != 4) {
    return false;
  }

  for (int i = 0; i < 4; i++) {
    if (input[i] != correct[i]) {
      return false;
    }
  }

  return true;
}

void showResult(bool isCorrect) {
  turnOffLeds();

  if (isCorrect) {
    digitalWrite(PIN_LED_GREEN, HIGH);
  } else {
    digitalWrite(PIN_LED_RED, HIGH);
  }

  lastResultMillis = millis();
}

void autoLedOff(unsigned long now) {
  if (currentState != STATE_RESULT) {
    return;
  }

  if (now - lastResultMillis >= RESULT_HOLD_MS) {
    turnOffLeds();
    resetInputBuffer();
    currentState = STATE_IDLE;
    Serial.println("Back to IDLE.");
  }
}

void resetInputBuffer() {
  for (int i = 0; i < 5; i++) {
    inputBuffer[i] = '\0';
  }

  inputLength = 0;
  lastKey = '\0';
}

void turnOffLeds() {
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
}

bool isDigitKey(char key) {
  return key >= '0' && key <= '9';
}
