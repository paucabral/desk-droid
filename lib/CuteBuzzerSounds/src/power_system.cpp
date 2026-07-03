#include "power_system.h"
#include "config.h"
#include "motors.h"
#include "audio_system.h"
#include <Adafruit_SH110X.h>
#include <CuteBuzzerSounds.h>
#include <esp_sleep.h>

// Direct binding access pointers to objects instantiated in main.cpp
extern Adafruit_SH1106G display;
extern int live_battery_percentage;

void updateBatteryTelemetry() {
  int rawADC = analogRead(FREE_BATTERY_PIN);
  float pinVoltage = (rawADC / 4095.0) * ADC_VREF_VOLTAGE;
  float calculatedBatteryVoltage = pinVoltage * 2.0; 

  int calculatedPct = map(round(calculatedBatteryVoltage * 100), 3500, 4200, 0, 100);
  live_battery_percentage = constrain(calculatedPct, 0, 100);

  Serial.print(F("Battery Voltage: ")); Serial.print(calculatedBatteryVoltage);
  Serial.print(F("V | Percentage: ")); Serial.print(live_battery_percentage); Serial.println(F("%"));
}

void enterSystemDeepSleep() {
  Serial.println(F("[POWER-MANAGEMENT] Executing multi-stage designed shutdown protocol..."));
  moveDroid(STOP);
  
  display.clearDisplay();
  display.display();
  
  delay(100); 
  playSoundAsync(S_DISCONNECTION);
  
  for (int frame = 0; frame <= 100; frame += 5) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    display.setCursor(7, 44);
    display.println(F("ENTERING SLEEP MODE"));
    display.setCursor(16, 54);
    display.println(F("POWERING DOWN..."));

    // Draw Corner Brackets
    display.drawFastHLine(2, 2, 10, SH110X_WHITE); display.drawFastVLine(2, 2, 10, SH110X_WHITE);
    display.drawFastHLine(115, 2, 10, SH110X_WHITE); display.drawFastVLine(125, 2, 10, SH110X_WHITE);
    display.drawFastHLine(2, 61, 10, SH110X_WHITE); display.drawFastVLine(2, 51, 10, SH110X_WHITE);
    display.drawFastHLine(115, 61, 10, SH110X_WHITE); display.drawFastVLine(125, 51, 10, SH110X_WHITE);

    display.drawCircle(64, 22, 15, SH110X_WHITE);
    display.drawRect(59, 15, 10, 15, SH110X_WHITE); 
    display.drawFastHLine(62, 13, 4, SH110X_WHITE);  

    int powerFillHeight = map(100 - frame, 0, 100, 0, 11);
    if (powerFillHeight > 0) {
      display.fillRect(61, 28 - powerFillHeight, 6, powerFillHeight, SH110X_WHITE);
    }

    display.display();
    delay(100); 
  }

  display.clearDisplay();
  display.display();

  esp_sleep_enable_ext1_wakeup(1ULL << TOUCH_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);
  Serial.println(F("[POWER-MANAGEMENT] Droid entering deep sleep state now. Goodnight!"));
  delay(100);
  esp_deep_sleep_start();
}