/*
 * StarHack 2026 - Pill Dispenser System
 * Equipe 18
 * 
 * Features:
 * - Temperature/Humidity monitoring (DHT sensor)
 * - LCD display with RTC time
 * - Stepper motor for pill dispensing
 * - Water sensor as capacitive touch
 * - Two warning modes (using 1 LED):
 *   1. LED FAST BLINK + 2 beeps: Temperature/Humidity not ideal
 *   2. LED SOLID + continuous beep: Pills ready (deactivates on touch)
 */

#include <Wire.h>
#include <LiquidCrystal.h>
#include <DS3231.h>
#include <Stepper.h>
#include <DHT.h>  // Adafruit DHT library
#include "pitches.h"

// ==================== PIN DEFINITIONS ====================
// LCD (Parallel connection - 4-bit mode)
#define LCD_RS    12
#define LCD_EN    11
#define LCD_D4    5
#define LCD_D5    4
#define LCD_D6    3
#define LCD_D7    6

// Sensors and Actuators
#define DHT_PIN         22    // DHT sensor
#define BUZZER_PIN      9     // Buzzer (PWM)
#define LED_PIN         13    // Single LED for all warnings
#define WATER_SENSOR_PIN A0   // Water sensor as touch sensor

// Stepper Motor (ULN2003 Driver with 28BYJ-48)
#define STEPPER_IN1     46    // Input 1
#define STEPPER_IN2     47    // Input 2
#define STEPPER_IN3     48    // Input 3
#define STEPPER_IN4     49    // Input 4

// I2C pins for RTC (hardware I2C)
// SDA = Pin 20, SCL = Pin 21 (used automatically by Wire library)

// ==================== COMPONENT INITIALIZATION ====================
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
DS3231 rtc;
Stepper stepper(2048, STEPPER_IN1, STEPPER_IN3, STEPPER_IN2, STEPPER_IN4);
DHT dht(DHT_PIN, DHT11);  // DHT11 sensor on pin 22

// ==================== CONSTANTS ====================
// Temperature and humidity thresholds
const float TEMP_MIN = 18.0;      // Minimum ideal temperature (°C)
const float TEMP_MAX = 25.0;      // Maximum ideal temperature (°C)
const float HUMIDITY_MIN = 30.0;  // Minimum ideal humidity (%)
const float HUMIDITY_MAX = 60.0;  // Maximum ideal humidity (%)

// Water sensor touch threshold (calibrate this value!)
const int TOUCH_THRESHOLD = 100;

// Stepper motor settings (28BYJ-48 with ULN2003)
const int STEPS_PER_PILL = 512;    // 1/4 revolution (adjust based on mechanism)
const int STEPPER_SPEED = 10;      // RPM (28BYJ-48 is slow, max ~15 RPM)

// Warning timing
const unsigned long BEEP_DURATION = 200;        // ms
const unsigned long BEEP_PAUSE = 300;           // ms between beeps
const unsigned long WARNING_CHECK_INTERVAL = 5000;  // Check every 5 seconds
const unsigned long DISPLAY_CYCLE_INTERVAL = 3000;  // Cycle display every 3 seconds

// Medication schedule (24-hour format: HH)
// Add your medication times here (up to 8 times per day)
const float MED_TIMES[] = {8, 12, 18, 22};  // 8am, 12pm, 6pm, 10pm
const int NUM_MED_TIMES = 4;  // Number of medication times

// ==================== STATE VARIABLES ====================
float temperature = 22.0;  // Default temperature
float humidity = 50.0;     // Default humidity
bool tempHumidityOK = true;
bool pillsReady = false;
bool touchDetected = false;
bool dhtWorking = true;  // Will be set to true if DHT reads successfully

unsigned long lastWarningCheck = 0;
unsigned long lastBeepTime = 0;
unsigned long lastDisplayCycle = 0;
int beepCount = 0;
bool isBeeping = false;
bool showDate = false;  // Toggle between temp/humidity and date

// Next medication time
int nextMedHour = -1;
int nextMedMinute = 0;

enum WarningState {
  NO_WARNING,
  TEMP_HUMIDITY_WARNING,
  PILLS_READY_WARNING
};
WarningState currentWarning = NO_WARNING;

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);  // Changed to 115200 for consistency
  setTimeFromSerial();
  
  // Initialize I2C
  Wire.begin();
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("StarHack 2026");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  // Initialize RTC
  rtc.begin();
  // UNCOMMENT THE LINE BELOW TO SET TIME, THEN RE-COMMENT AFTER ONE UPLOAD!
  // rtc.setDateTime(2026, 3, 29, 9, 24, 0);  // Year, Month, Day, Hour, Minute, Second
  
  // Initialize pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WATER_SENSOR_PIN, INPUT);
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize stepper motor
  stepper.setSpeed(STEPPER_SPEED);  // Set RPM
  
  // Turn off LED and buzzer
  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);
  
  delay(2000);
  
  // Calculate next medication time
  updateNextMedTime();
  
  lcd.clear();
  
  Serial.println("System initialized!");
  Serial.println("Commands:");
  Serial.println("  D - Dispense pill");
  Serial.println("  T - Test motor");
  Serial.println("  M - Show next med time");
}

// ==================== MAIN LOOP ====================
void loop() {
  unsigned long currentTime = millis();
  
  // Read sensors periodically
  if (currentTime - lastWarningCheck >= WARNING_CHECK_INTERVAL) {
    updateSensors();
    checkWarningConditions();
    lastWarningCheck = currentTime;
  }
  
  // Cycle display between temp/humidity and date
  if (currentTime - lastDisplayCycle >= DISPLAY_CYCLE_INTERVAL) {
    showDate = !showDate;  // Toggle display
    lastDisplayCycle = currentTime;
  }
  
  // Update display
  updateDisplay();
  
  // Handle warnings
  handleWarnings(currentTime);
  
  // Check for serial commands
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'D' || cmd == 'd') {
      dispensePill();
    }
    else if (cmd == 'T' || cmd == 't') {
      testMotor();
    }
    else if (cmd == 'M' || cmd == 'm') {
      showNextMedTime();
    }
  }
  
  delay(50);  // Small delay to prevent overwhelming the system
}

// ==================== SENSOR FUNCTIONS ====================
void updateSensors() {
  // Read temperature and humidity from DHT11
  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();
  
  // Check if readings are valid
  if (!isnan(newTemp) && !isnan(newHum)) {
    temperature = newTemp;
    humidity = newHum;
    dhtWorking = true;
    
    Serial.print("DHT OK - Temp: ");
    Serial.print(temperature);
    Serial.print("°C, Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  } else {
    // DHT reading failed, keep previous values
    dhtWorking = false;
    Serial.println("DHT read failed - using defaults");
  }
  
  // Read water sensor (touch detection)
  int sensorValue = analogRead(WATER_SENSOR_PIN);
  touchDetected = (sensorValue > TOUCH_THRESHOLD);
  
  if (touchDetected) {
    Serial.print("Touch detected! Value: ");
    Serial.println(sensorValue);
  }
}

void checkWarningConditions() {
  // Only check temp/humidity if DHT is working
  if (dhtWorking) {
    tempHumidityOK = (temperature >= TEMP_MIN && temperature <= TEMP_MAX &&
                      humidity >= HUMIDITY_MIN && humidity <= HUMIDITY_MAX);
  } else {
    tempHumidityOK = true;  // Skip warning if sensor not working
  }
  
  // Determine warning state
  if (pillsReady && !touchDetected) {
    currentWarning = PILLS_READY_WARNING;
  } else if (!tempHumidityOK) {
    currentWarning = TEMP_HUMIDITY_WARNING;
  } else {
    currentWarning = NO_WARNING;
  }
}

// ==================== WARNING HANDLING ====================
void handleWarnings(unsigned long currentTime) {
  switch (currentWarning) {
    case TEMP_HUMIDITY_WARNING:
      handleTempHumidityWarning(currentTime);
      break;
      
    case PILLS_READY_WARNING:
      handlePillsReadyWarning(currentTime);
      break;
      
    case NO_WARNING:
      // Turn off all warnings
      digitalWrite(LED_PIN, LOW);
      noTone(BUZZER_PIN);
      beepCount = 0;
      isBeeping = false;
      break;
  }
}

// LED fast blink + 2 beeps for temperature/humidity warning
void handleTempHumidityWarning(unsigned long currentTime) {
  // Fast blink LED (250ms on/off)
  static unsigned long lastBlinkTime = 0;
  if (currentTime - lastBlinkTime >= 250) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    lastBlinkTime = currentTime;
  }
  
  // Generate 2 beeps pattern
  if (!isBeeping && beepCount < 2) {
    // Start beeping
    tone(BUZZER_PIN, NOTE_C5);
    isBeeping = true;
    lastBeepTime = currentTime;
  } 
  else if (isBeeping && (currentTime - lastBeepTime >= BEEP_DURATION)) {
    // Stop beeping
    noTone(BUZZER_PIN);
    isBeeping = false;
    beepCount++;
    lastBeepTime = currentTime;
  }
  else if (!isBeeping && beepCount >= 2 && (currentTime - lastBeepTime >= BEEP_PAUSE)) {
    // Reset for next cycle
    beepCount = 0;
  }
}

// LED solid + continuous beep for pills ready (stops on touch)
void handlePillsReadyWarning(unsigned long currentTime) {
  // Solid LED
  digitalWrite(LED_PIN, HIGH);
  
  // Continuous tone
  tone(BUZZER_PIN, NOTE_E5);
  
  // Check if touched to deactivate
  if (touchDetected) {
    pillsReady = false;
    noTone(BUZZER_PIN);
    digitalWrite(LED_PIN, LOW);
    Serial.println("Pills taken!");
  }
}

// ==================== DISPLAY FUNCTIONS ====================
void updateDisplay() {
  RTCDateTime dt = rtc.getDateTime();
  
  // Line 1: Time and Next Medication
  lcd.setCursor(0, 0);
  lcd.print(formatTime(dt.hour));
  lcd.print(":");
  lcd.print(formatTime(dt.minute));
  lcd.print(" ");
  
  // Show next medication time
  if (nextMedHour >= 0) {
    lcd.print("Nx:");
    lcd.print(formatTime(nextMedHour));
    lcd.print("h");
    if (nextMedMinute > 0) {
      lcd.print(formatTime(nextMedMinute));  // Show minutes after h
    }
  } else {
    lcd.print("      ");
  }
  
  // Line 2: Cycle between Temperature/Humidity and Date
  lcd.setCursor(0, 1);
  
  if (pillsReady) {
    // Pills ready mode - always show this
    lcd.print("** TAKE PILLS **");
  } else if (showDate) {
    // Show date
    lcd.print(dt.day);
    lcd.print("/");
    lcd.print(dt.month);
    lcd.print("/");
    lcd.print(dt.year);
    lcd.print("    ");  // Clear rest of line
  } else if (dhtWorking) {
    // Show temperature and humidity
    lcd.print(temperature, 1);
    lcd.print("C ");
    lcd.print(humidity, 0);
    lcd.print("%");
    
    // Status indicator
    if (!tempHumidityOK) {
      lcd.print(" WARN");
    } else {
      lcd.print(" OK  ");
    }
  } else {
    // DHT not working, show error message
    lcd.print("DHT Error       ");
  }
}

String formatTime(uint8_t value) {
  if (value < 10) {
    return "0" + String(value);
  }
  return String(value);
}

// ==================== STEPPER MOTOR FUNCTIONS ====================
void dispensePill() {
  Serial.println("Dispensing pill...");
  lcd.clear();
  lcd.print("Dispensing...");
  
  // Move stepper motor
  stepper.step(STEPS_PER_PILL);  // Positive for clockwise
  
  // Set pills ready flag
  pillsReady = true;
  
  lcd.clear();
  lcd.print("Pills Ready!");
  Serial.println("Pills dispensed!");
  
  delay(1000);
}

// ==================== RTC FUNCTIONS ====================
void setTimeFromSerial() {
  // Expected format: TYYYYMMDDHHMMSS
  // Example: T20260329123045 = 2026-03-29 12:30:45
  if (Serial.available() >= 14) {
    String timeStr = Serial.readStringUntil('\n');
    
    uint16_t year = timeStr.substring(0, 4).toInt();
    uint8_t month = timeStr.substring(4, 6).toInt();
    uint8_t day = timeStr.substring(6, 8).toInt();
    uint8_t hour = timeStr.substring(8, 10).toInt();
    uint8_t minute = timeStr.substring(10, 12).toInt();
    uint8_t second = timeStr.substring(12, 14).toInt();
    
    rtc.setDateTime(year, month, day, hour, minute, second);
    
    Serial.println("Time set successfully!");
    lcd.clear();
    lcd.print("Time Updated!");
    delay(1000);
  }
}

// ==================== UTILITY FUNCTIONS ====================

// Calculate next medication time based on current time
void updateNextMedTime() {
  RTCDateTime dt = rtc.getDateTime();
  int currentHour = dt.hour;
  int currentMinute = dt.minute;
  float currentTimeFloat = currentHour + (currentMinute / 60.0);  // Current time as decimal
  
  nextMedHour = -1;  // Reset
  nextMedMinute = 0;
  
  // Find next medication time today
  for (int i = 0; i < NUM_MED_TIMES; i++) {
    if (MED_TIMES[i] > currentTimeFloat) {
      // Extract hour and minute from decimal time
      nextMedHour = (int)MED_TIMES[i];  // Integer part = hours
      float decimalPart = MED_TIMES[i] - nextMedHour;  // Decimal part
      nextMedMinute = (int)(decimalPart * 60);  // Convert to minutes
      return;
    }
  }
  
  // If no more times today, show first time tomorrow
  if (nextMedHour == -1 && NUM_MED_TIMES > 0) {
    nextMedHour = (int)MED_TIMES[0];
    float decimalPart = MED_TIMES[0] - nextMedHour;
    nextMedMinute = (int)(decimalPart * 60);
  }
}

// Show next medication time via Serial
void showNextMedTime() {
  updateNextMedTime();
  
  Serial.print("Next medication time: ");
  if (nextMedHour >= 0) {
    Serial.print(formatTime(nextMedHour));
    Serial.print(":");
    Serial.print(formatTime(nextMedMinute));
    Serial.println("h");
  } else {
    Serial.println("Not scheduled");
  }
}

// Test motor function
void testMotor() {
  Serial.println("=== MOTOR TEST ===");
  lcd.clear();
  lcd.print("Testing Motor...");
  
  Serial.println("Step 1: Forward 1/4 turn (512 steps)");
  stepper.step(512);
  delay(1000);
  
  Serial.println("Step 2: Backward 1/4 turn (-512 steps)");
  stepper.step(-512);
  delay(1000);
  
  Serial.println("Step 3: Forward 1/2 turn (1024 steps)");
  stepper.step(1024);
  delay(1000);
  
  Serial.println("Step 4: Backward 1/2 turn (-1024 steps)");
  stepper.step(-1024);
  delay(1000);
  
  Serial.println("Test complete!");
  lcd.clear();
  lcd.print("Test Complete!");
  delay(2000);
}
