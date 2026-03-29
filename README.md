# Pill Dispenser System - README
## StarHack 2026 - Equipe 18

---

## 🎯 Project Overview

An intelligent pill dispenser system using Arduino Mega 2560 that monitors environmental conditions and provides visual/audible alerts for two scenarios:

1. **Temperature/Humidity Warning** (LED fast blink + 2 beeps)
   - Triggers when temperature or humidity is outside ideal range
   - Ideal temp: 18-25°C
   - Ideal humidity: 30-60%

2. **Pills Ready Alert** (LED solid + continuous beep)
   - Activates when pills are dispensed
   - Deactivates when pills are taken (detected via touch sensor)

---

## 📦 Components Required

### Hardware:
- 1× Arduino Mega 2560 (Elegoo ATmega2560)
- 1× LCD Screen 16x2 (LiquidCrystal library compatible)
- 1× DHT22 Temperature/Humidity Sensor (or DHT11)
- 1× DS3231 RTC Module (I2C)
- 1× Stepper Motor (NEMA 17 or similar)
- 1× A4988 Stepper Driver
- 1× Buzzer (passive or active)
- 1× LED (Red or any color)
- 1× Water Sensor (used as capacitive touch)
- 1× 220Ω Resistor (for LED)
- 1× 10kΩ Resistor (pull-up for DHT)
- 1× 10kΩ Potentiometer (LCD contrast)
- 1× Breadboard + Jumper Wires
- 1× 12V Power Supply (for stepper motor)

### Libraries (StarHack Library):
Located in: `Librairies_StarHACK2026/`

- `LiquidCrystal` - LCD display control
- `dht_nonblocking` - Non-blocking DHT sensor reading
- `DS3231` - Real-time clock
- `AccelStepper` - Stepper motor control with acceleration
- `pitches` - Musical note definitions for buzzer

---

## 🔌 Quick Setup Guide

### 1. Hardware Connections
See **WIRING_DIAGRAM.md** for detailed pin connections.

**Quick Reference:**
- LCD: Pins 3, 4, 5, 6, 11, 12
- DHT22: Pin 22
- Buzzer: Pin 9
- LED: Pin 13
- Stepper: Pins 7, 8, 10
- Water Sensor: Pin A0
- RTC: Pins 20 (SDA), 21 (SCL)

### 2. Library Installation
Add the StarHack libraries to your Arduino IDE:
1. Open Arduino IDE
2. Sketch → Include Library → Add .ZIP Library
3. Navigate to `Librairies_StarHACK2026/`
4. Add each library folder

**Or** copy the folders directly to:
- Windows: `Documents\Arduino\libraries\`
- Mac: `~/Documents/Arduino/libraries/`
- Linux: `~/Arduino/libraries/`

### 3. Upload Code
1. Open `pill_dispenser_main.ino`
2. Select **Tools → Board → Arduino Mega or Mega 2560**
3. Select **Tools → Processor → ATmega2560**
4. Select correct COM port
5. Click Upload

---

## 🎮 How to Use

### Initial Setup

1. **Set RTC Time (First time only):**
   - Option A: Uncomment line in setup():
     ```cpp
     rtc.setDateTime(__DATE__, __TIME__);
     ```
     Upload, then re-comment and re-upload.
   
   - Option B: Use Serial Monitor (9600 baud):
     Send: `T20260329123045` (format: TYYYYMMDDHHMMSS)

2. **Calibrate Water Sensor:**
   - Open Serial Monitor (9600 baud)
   - Note the analog value when NOT touching
   - Note the analog value when touching
   - Adjust `TOUCH_THRESHOLD` constant in code (line 33)
   - Example: If untouched = 20, touched = 350, set threshold to 100-150

3. **Adjust Temperature/Humidity Thresholds:**
   Edit these constants in code (lines 27-30):
   ```cpp
   const float TEMP_MIN = 18.0;      // Minimum temp (°C)
   const float TEMP_MAX = 25.0;      // Maximum temp (°C)
   const float HUMIDITY_MIN = 30.0;  // Minimum humidity (%)
   const float HUMIDITY_MAX = 60.0;  // Maximum humidity (%)
   ```

4. **Calibrate Stepper Motor:**
   - Test dispense with Serial command: `D`
   - Adjust `STEPS_PER_PILL` (line 36) until one pill is dispensed
   - Adjust speed/acceleration if needed (lines 37-38)

### Operation

**Automatic Monitoring:**
- System continuously monitors temperature and humidity
- LCD displays time, date, temp, and humidity
- Checks conditions every 5 seconds

**Warning System:**

1. **LED Fast Blink + 2 Beeps:**
   - Activates when temp/humidity outside ideal range
   - Repeats every few seconds
   - Check environment and adjust if needed

2. **LED Solid + Continuous Beep:**
   - Activates when pills are dispensed
   - Beep continues until pills are taken
   - Touch water sensor to acknowledge and stop beep

**Serial Commands:**
- `D` or `d` - Manually dispense pill
- `T` followed by 14 digits - Set RTC time (TYYYYMMDDHHMMSS)

---

## 📊 LCD Display Format

```
Line 1: HH:MM:SS DD/MM
Line 2: 22.5C 45% OK
```

**Status Indicators:**
- `OK` - All systems normal
- `WARN` - Temperature/humidity warning
- `READY` - Pills ready to be taken

---

## ⚙️ Configuration Options

### Timing Settings (in code):
```cpp
const unsigned long BEEP_DURATION = 200;              // Beep length (ms)
const unsigned long BEEP_PAUSE = 300;                 // Pause between beeps (ms)
const unsigned long WARNING_CHECK_INTERVAL = 5000;    // Check interval (ms)
```

### Stepper Motor Settings:
```cpp
const int STEPS_PER_PILL = 200;   // Steps per pill dispense
const int STEPPER_SPEED = 500;    // Max speed (steps/sec)
const int STEPPER_ACCEL = 300;    // Acceleration (steps/sec²)
```

### DHT Sensor Type:
Change on line 19 if using DHT11:
```cpp
DHT_nonblocking dht(DHT_PIN, DHT_TYPE_11);  // For DHT11
DHT_nonblocking dht(DHT_PIN, DHT_TYPE_22);  // For DHT22 (default)
```

---

## 🐛 Troubleshooting

### LCD is blank:
- Check contrast potentiometer (V0 pin)
- Verify all connections (RS, E, D4-D7)
- Test with LCD example sketch first
- **If dim:** Add 100µF capacitor, use separate power pins (see POWER_TROUBLESHOOTING.md)

### DHT sensor shows 0.0 or NaN:
- Check pull-up resistor (10kΩ)
- Wait 2+ seconds after startup
- Verify DHT_TYPE matches your sensor

### Time keeps resetting:
- Replace DS3231 coin cell battery (CR2032)
- Set time again using Serial command

### Stepper motor doesn't move:
- Verify 12V external power to A4988
- Check ENABLE pin is connected (pin 7)
- Adjust A4988 current pot (clockwise slightly)
- Test connections: STEP, DIR pins

### Touch sensor always triggered:
- Lower TOUCH_THRESHOLD value
- Check wiring (Signal to A0)
- Keep sensor away from metal objects

### Buzzer doesn't sound:
- Check polarity (+ to pin 9)
- Try swapping connections if passive buzzer
- Test with simple tone() in Arduino examples

---

## 📈 Serial Monitor Output

When connected (9600 baud), you'll see:
```
System initialized!
Temp: 22.5°C, Humidity: 45%
Touch detected!
Dispensing pill...
Pills dispensed!
```

---

## 🔐 Pin Usage Details

**Used Resources:**
- Digital pins: 13 of 14 max ✅
- PWM pins: 2 of 6 max ✅
- Analog pins: 1 of 6 max ✅
- I2C: Shared between RTC ✅
- Interrupts: 0 used (2 available for expansion) ✅

**Reserved for Future:**
- Pins 2, 3: Hardware interrupts
- Pins 0, 1: Serial communication (USB)

---

## 🚀 Future Enhancements

Potential additions with available resources:
- Add rotary encoder (pins 2, 3 with interrupts)
- Add push buttons for manual control
- Add SD card logging
- Add RFID for user authentication
- Display graphs on larger LCD/TFT

---

## 📝 Code Structure

```
pill_dispenser_main.ino
├── Pin Definitions
├── Component Initialization
├── Constants (adjustable thresholds)
├── State Variables
├── setup()
├── loop()
├── Sensor Functions
│   ├── updateSensors()
│   └── checkWarningConditions()
├── Warning Handling
│   ├── handleWarnings()
│   ├── handleTempHumidityWarning()
│   └── handlePillsReadyWarning()
├── Display Functions
│   └── updateDisplay()
├── Stepper Functions
│   └── dispensePill()
└── Utility Functions
    └── setTimeFromSerial()
```

---

## 🛠️ Development Notes

**Design Decisions:**
1. **Non-blocking DHT:** Uses `dht_nonblocking` library to prevent delays
2. **AccelStepper:** Provides smooth acceleration/deceleration
3. **Optimized pins:** DHT on pin 22 (not pin 2) to preserve interrupts
4. **Parallel LCD:** Uses LiquidCrystal library (not I2C) per StarHack library

**Testing Checklist:**
- [ ] LCD displays correctly
- [ ] RTC keeps accurate time
- [ ] DHT reads temperature/humidity
- [ ] Red LED + 2 beeps on temp warning
- [ ] Green LED + continuous beep on pills ready
- [ ] Touch sensor stops beep
- [ ] Stepper dispenses pills
- [ ] Serial commands work

---

## 📞 Support

For StarHack 2026 support:
- Check WIRING_DIAGRAM.md for connections
- Review library examples in `Librairies_StarHACK2026/`
- Use Serial Monitor (9600 baud) for debugging

---

## 📄 License

This project uses libraries under various open-source licenses:
- AccelStepper: GPL v3
- DHT nonblocking: GPL v3
- DS3231: GPL v3
- LiquidCrystal: Public domain

---

**Built with ❤️ for StarHack 2026 - Equipe 18**
