 // ======================================================
// PROJECT INFORMATION
// ======================================================
/*
Author: Andrew Britten
Date: 07 May 2026
Title: Battery monitor (16S LFP Voltage Monitor)

Description:
Simple battery voltage monitor for 16S LiFePO₄ pack.
Displays voltage + SOC on OLED and triggers alarm
when battery drops below threshold.

Hardware:
 - Arduino Nano
 - SSD1306 OLED (I2C)
 - Buzzer + Button input
*/

// ======================================================
// LIBRARIES
// ======================================================
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======================================================
// PIN MAP
// ======================================================
#define PB1 4
#define powerPin 8
#define buzz 9

#define VBATPin A1
#define currentPin A2

#define freq 4125

// ======================================================
// OLED
// ======================================================
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// ======================================================
// BATTERY LIMITS
// ======================================================
#define lowVoltageOFF 44.0
#define highVoltageOFF 54.4
#define LOW_PERCENT 26.0

// ======================================================
// ADC SETTINGS
// ======================================================
#define ADC_MAX 1023.0
#define VREF 5.0

#define VOLTAGE_DIVIDER_RATIO 20.0

// small calibration trim (your 48V fix)
#define CAL_FACTOR 1.0058

// ======================================================
// LPF
// ======================================================
#define LPF 150

// ======================================================
// GLOBALS
// ======================================================
float VBAT = 0;

bool lowVoltageAlarm = false;
bool alarmSilenced = false;

bool buzzerState = false;
bool buttonBuzz = false;

unsigned long buzzerTimer = 0;

bool buttonPressed = false;

// ======================================================
// SETUP
// ======================================================
void setup() {

  Serial.begin(9600);

  pinMode(PB1, INPUT_PULLUP);
  pinMode(buzz, OUTPUT);
  pinMode(powerPin, OUTPUT);

  noTone(buzz);
  digitalWrite(powerPin, HIGH);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  drawlogo();
  delay(500);
  tone(buzz, freq);
  delay(500);
  noTone(buzz);
  delay(1000);

}

// ======================================================
// LOOP
// ======================================================
void loop() {

  readVBAT();
  buttonInputs();
  lowVoltageCheck();
  buzzerAlarm();
  drawVoltage();

  delay(50);
}

// ======================================================
// VOLTAGE READ (150 SAMPLE LPF)
// ======================================================
void readVBAT() {

  float sum = 0;

  for (int i = 0; i < LPF; i++) {
    sum += analogRead(VBATPin);
  }

  float raw = sum / LPF;

  float voltageAtPin = (raw * VREF) / ADC_MAX;

  VBAT = voltageAtPin * VOLTAGE_DIVIDER_RATIO * CAL_FACTOR;
}

// ======================================================
// BUTTON
// ======================================================
void buttonInputs() {

  buttonPressed = (digitalRead(PB1) == LOW);
}

// ======================================================
// SOC
// ======================================================
float getSOC() {

  float percent =
    ((VBAT - lowVoltageOFF) / (highVoltageOFF - lowVoltageOFF)) * 100.0;

  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;

  return percent;
}

// ======================================================
// LOW VOLTAGE CHECK
// ======================================================
void lowVoltageCheck() {

  float percent = getSOC();

  if (percent <= LOW_PERCENT) {
    lowVoltageAlarm = true;
  } else {
    lowVoltageAlarm = false;
    alarmSilenced = false;
  }

  if (buttonPressed && lowVoltageAlarm) {
    alarmSilenced = true;
  }
}

// ======================================================
// BUZZER (CLEAN + NO FLICKER)
// ======================================================
void buzzerAlarm() {

  unsigned long now = millis();

  // -------------------------
  // 1. BUTTON HAS TOP PRIORITY
  // -------------------------
  if (buttonPressed) {

    if (!buttonBuzz) {
      buttonBuzz = true;
      tone(buzz, freq);
    }

    buzzerState = false;  // reset alarm state so it doesn't interfere
    buzzerTimer = now;    // prevent alarm catch-up

    return;  // IMPORTANT: block alarm logic completely
  }

  // button released
  if (buttonBuzz) {
    buttonBuzz = false;
    noTone(buzz);
  }

  // -------------------------
  // 2. LOW VOLTAGE ALARM (only if button NOT active)
  // -------------------------
  if (lowVoltageAlarm && !alarmSilenced) {

    if (now - buzzerTimer >= 1000) {
      buzzerTimer = now;
      buzzerState = !buzzerState;

      if (buzzerState) {
        tone(buzz, freq);
      } else {
        noTone(buzz);
      }
    }

    return;
  }

  // -------------------------
  // 3. DEFAULT OFF STATE
  // -------------------------
  noTone(buzz);
}

// ======================================================
// DISPLAY
// ======================================================
void drawVoltage() {

  display.clearDisplay();

  float percent = getSOC();

  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;

  // BAR
  int barW = 128;
  int fillWidth = (percent / 100.0) * (barW - 2);

  display.drawRect(0, 0, 128, 12, SSD1306_WHITE);
  display.fillRect(1, 1, fillWidth, 10, SSD1306_WHITE);

  // TEXT
  bool flash = (millis() / 1000) % 2;

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (lowVoltageAlarm) {

    if (flash) {
      display.setCursor(0, 16);
      display.print("CHARGE");
    }

    if (flash) {
      display.setCursor(0, 28);
      display.print("LOW BAT!");
    }
  }

  display.setTextSize(2);

  display.setCursor(80, 18);
  display.print((int)percent);
  display.print("%");

  display.setCursor(0, 45);
  display.print("BAT=");

  display.setCursor(55, 45);
  display.print(VBAT, 2);
  display.print("V");

  display.display();
}

// ======================================================
// LOGO
// ======================================================
void drawlogo() {

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.print("TRITON AI");
  display.display();
}