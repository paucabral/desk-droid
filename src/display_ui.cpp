#include "display_ui.h"
#include "config.h"

void drawChecklist(const char* accel, const char* touch, const char* light, const char* audio, const char* sysMsg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  display.setCursor(0, 0);
  display.println(F("=== SYSTEMS CHECK ==="));
  display.println(F("---------------------"));
  
  display.print(F("[")); display.print(accel); display.println(F("]     ACCELEROMETER"));
  display.print(F("[")); display.print(touch); display.println(F("]      TOUCH SENSOR"));
  display.print(F("[")); display.print(light); display.println(F("]      LIGHT SENSOR"));
  display.print(F("[")); display.print(audio); display.println(F("]      BUZZER AUDIO"));
  
  display.println(F("---------------------"));
  display.print(F("STATUS: ")); display.println(sysMsg);
  
  display.display();
}

void drawSplashArt() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  // Outer Tech-Frame Brackets
  display.drawRect(0, 0, 128, 64, SH110X_WHITE);      
  display.fillRect(0, 12, 2, 40, SH110X_BLACK);       
  display.fillRect(126, 12, 2, 40, SH110X_BLACK);
  
  // Left Side: Face Outline 
  display.drawRoundRect(15, 18, 32, 28, 6, SH110X_WHITE); 
  
  // Big Square Eyes (RoboEyes Style)
  display.fillRect(19, 27, 10, 10, SH110X_WHITE);       
  display.fillRect(33, 27, 10, 10, SH110X_WHITE);

  // Vertical Separator Line
  display.drawFastVLine(55, 8, 48, SH110X_WHITE);

  // Right Side: Typography
  display.setTextSize(1);
  display.setCursor(63, 14);
  display.print(F("DESK"));
  
  display.setTextSize(2); 
  display.setCursor(63, 25);
  display.print(F("DROID"));
  
  display.setTextSize(1);
  display.setCursor(63, 44);
  display.print(F("v1.0 READY"));

  display.display();
}