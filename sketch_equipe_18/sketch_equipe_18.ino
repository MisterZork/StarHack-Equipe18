#include <LiquidCrystal.h>
#include <dht_nonblocking.h>

const byte STEPPER_PINS[4] = {22, 23, 24, 25};  // ULN2003 IN1..IN4
const byte BUZZER_PIN = 26;                      // passive buzzer
const byte ALERT_LED_PIN = 27;                   // LED or relay input
const byte DHT_PIN = 28;
const byte WATER_SENSOR_PIN = A0;

const byte LCD_RS = 30;
const byte LCD_EN = 31;
const byte LCD_D4 = 32;
const byte LCD_D5 = 33;
const byte LCD_D6 = 34;
const byte LCD_D7 = 35;

const unsigned long EVENT_WAIT_MS = 30000UL;
const unsigned long STEPPER_STEP_INTERVAL_MS = 3UL;
const unsigned long ALERT_HALF_PERIOD_MS = 1000UL;  // 0.5 Hz full cycle
const uint16_t STEPS_PER_EVENT = 1024;
const unsigned int BUZZER_TONE_HZ = 2000;           // audible buzz for a passive buzzer

const byte HALF_STEP_SEQUENCE[8][4] = {
  {HIGH, LOW,  LOW,  LOW},
  {HIGH, HIGH, LOW,  LOW},
  {LOW,  HIGH, LOW,  LOW},
  {LOW,  HIGH, HIGH, LOW},
  {LOW,  LOW,  HIGH, LOW},
  {LOW,  LOW,  HIGH, HIGH},
  {LOW,  LOW,  LOW,  HIGH},
  {HIGH, LOW,  LOW,  HIGH}
};

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
DHT_nonblocking dht(DHT_PIN, DHT_TYPE_11);

byte stepIndex = 0;
bool eventActive = false;
bool alertOn = false;
bool displayDirty = true;
uint16_t stepsRemaining = 0;
unsigned long lastStepperStepMs = 0;
unsigned long lastAlertToggleMs = 0;
unsigned long nextEventAtMs = EVENT_WAIT_MS;
unsigned long lastDisplayRefreshMs = 0;
float lastTemperature = 0.0f;
float lastHumidity = 0.0f;
bool haveDhtReading = false;

void printTwoDigits(int value) {
  if (value < 0) {
    value = 0;
  } else if (value > 99) {
    value = 99;
  }

  if (value < 10) {
    lcd.print('0');
  }
  lcd.print(value);
}

void printValueOrDash(bool valid, int value) {
  if (!valid) {
    lcd.print(F("--"));
    return;
  }

  printTwoDigits(value);
}

bool timeReached(unsigned long nowMs, unsigned long targetMs) {
  return (long)(nowMs - targetMs) >= 0;
}

unsigned long msUntil(unsigned long nowMs, unsigned long targetMs) {
  if (timeReached(nowMs, targetMs)) {
    return 0;
  }

  return targetMs - nowMs;
}

void writeStepperPhase(byte phase) {
  for (byte pin = 0; pin < 4; pin++) {
    digitalWrite(STEPPER_PINS[pin], HALF_STEP_SEQUENCE[phase][pin]);
  }
}

void releaseStepper() {
  for (byte pin = 0; pin < 4; pin++) {
    digitalWrite(STEPPER_PINS[pin], LOW);
  }
}

void stepForward() {
  stepIndex = (stepIndex + 1) & 0x07;
  writeStepperPhase(stepIndex);
}

void setAlertOutputs(bool enabled) {
  digitalWrite(ALERT_LED_PIN, enabled ? HIGH : LOW);
  if (enabled) {
    tone(BUZZER_PIN, BUZZER_TONE_HZ);
  } else {
    noTone(BUZZER_PIN);
  }
}

void scheduleNextEvent(unsigned long nowMs) {
  nextEventAtMs = nowMs + EVENT_WAIT_MS;
}

void startEvent(unsigned long nowMs) {
  eventActive = true;
  alertOn = true;
  stepsRemaining = STEPS_PER_EVENT;
  lastStepperStepMs = nowMs;
  lastAlertToggleMs = nowMs;
  setAlertOutputs(true);
  displayDirty = true;
}

void stopEvent(unsigned long nowMs) {
  eventActive = false;
  alertOn = false;
  stepsRemaining = 0;
  setAlertOutputs(false);
  releaseStepper();
  scheduleNextEvent(nowMs);
  displayDirty = true;
}

void updateEvent(unsigned long nowMs) {
  if (!eventActive) {
    return;
  }

  if (analogRead(WATER_SENSOR_PIN) > 0) {
    stopEvent(nowMs);
    return;
  }

  while (timeReached(nowMs, lastAlertToggleMs + ALERT_HALF_PERIOD_MS)) {
    lastAlertToggleMs += ALERT_HALF_PERIOD_MS;
    alertOn = !alertOn;
    setAlertOutputs(alertOn);
  }

  while (stepsRemaining > 0 && timeReached(nowMs, lastStepperStepMs + STEPPER_STEP_INTERVAL_MS)) {
    lastStepperStepMs += STEPPER_STEP_INTERVAL_MS;
    stepForward();
    stepsRemaining--;

    if (analogRead(WATER_SENSOR_PIN) > 0) {
      stopEvent(nowMs);
      return;
    }
  }

  if (stepsRemaining == 0) {
    stopEvent(nowMs);
  }
}

void renderDisplay(unsigned long nowMs) {
  lcd.setCursor(0, 0);
  lcd.print(F("IMP:"));
  printValueOrDash(haveDhtReading, (int)lastTemperature);
  lcd.print('C');
  lcd.print(F("  HMD:"));
  printValueOrDash(haveDhtReading, (int)lastHumidity);
  lcd.print('%');

  lcd.setCursor(0, 1);
  lcd.print(F("PROCH MED: "));

  unsigned long remainingSec = eventActive ? 0 : msUntil(nowMs, nextEventAtMs) / 1000UL;
  printTwoDigits((int)(remainingSec / 60UL));
  lcd.print(':');
  printTwoDigits((int)(remainingSec % 60UL));
}

void setup() {
  for (byte i = 0; i < 4; i++) {
    pinMode(STEPPER_PINS[i], OUTPUT);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ALERT_LED_PIN, OUTPUT);
  pinMode(WATER_SENSOR_PIN, INPUT);

  releaseStepper();
  setAlertOutputs(false);

  lcd.begin(16, 2);
  unsigned long nowMs = millis();
  nextEventAtMs = nowMs + EVENT_WAIT_MS;
  renderDisplay(nowMs);
  lastDisplayRefreshMs = nowMs;
  displayDirty = false;
}

void loop() {
  unsigned long nowMs = millis();

  float temperature = 0.0f;
  float humidity = 0.0f;
  if (dht.measure(&temperature, &humidity)) {
    lastTemperature = temperature;
    lastHumidity = humidity;
    haveDhtReading = true;
    displayDirty = true;
  }

  updateEvent(nowMs);

  if (!eventActive && timeReached(nowMs, nextEventAtMs)) {
    if (analogRead(WATER_SENSOR_PIN) > 0) {
      scheduleNextEvent(nowMs);
      displayDirty = true;
    } else {
      startEvent(nowMs);
    }
  }

  if (displayDirty || (nowMs - lastDisplayRefreshMs) >= 1000UL) {
    renderDisplay(nowMs);
    lastDisplayRefreshMs = nowMs;
    displayDirty = false;
  }
}
