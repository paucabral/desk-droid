#include "display_ui.h"
#include "config.h"

void drawChecklist(const char* accel, const char* touch, const char* light, const char* audio, const char* motors, const char* sysMsg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  // Header (Lines 1 & 2)
  display.setCursor(0, 0);
  display.println(F("=== SYSTEMS CHECK ==="));
  display.println(F("---------------------"));
  
  // Checklist Items (Lines 3, 4, 5, 6, & 7)
  display.print(F("[")); display.print(accel); display.println(F("]     ACCELEROMETER"));
  display.print(F("[")); display.print(touch); display.println(F("]      TOUCH SENSOR"));
  display.print(F("[")); display.print(light); display.println(F("]      LIGHT SENSOR"));
  display.print(F("[")); display.print(audio); display.println(F("]      BUZZER AUDIO"));
  display.print(F("[")); display.print(motors); display.println(F("]      MOTOR DRIVER"));
  
  // Footer Status (Line 8) - Divider removed to prevent overflow scrolling
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

void drawWifiScreen(const char* header, const char* status, const char* ipOrInstruction) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // Header Layout
  display.setCursor(0, 0);
  display.print(F("=== ")); display.print(header); display.println(F(" ==="));
  display.println(F("---------------------"));
  
  // Dynamic status details
  display.print(F("STATUS: ")); display.println(status);
  display.println(F("---------------------"));
  
  // Detail message row
  display.println(ipOrInstruction);

  display.display();
}

void drawDashboard(const char* condition, int temp, int humidity, int rainChance, const char* ipAddress, int batteryPercent) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  // 1. Header Text
  display.setCursor(0, 0);
  display.print(F("=== DASHBOARD ==="));

  // 2. Graphic Battery Icon Element (Top Right Corner Area)
  display.drawRect(106, 0, 16, 8, SH110X_WHITE);       // Main cell block shell
  display.drawFastVLine(122, 2, 4, SH110X_WHITE);      // Battery positive cathode node tip
  
  // Constrain percentage and map it to internal fill pixels (Max 12 pixels across)
  int clampedBattery = (batteryPercent > 100) ? 100 : ((batteryPercent < 0) ? 0 : batteryPercent);
  int fillWidth = (clampedBattery * 12) / 100;
  if (fillWidth > 0) {
    display.fillRect(108, 2, fillWidth, 4, SH110X_WHITE); // Internal energy level bar fill
  }

  // Divider Line accent rule
  display.drawFastHLine(0, 10, 128, SH110X_WHITE);

  // 3. System Data Content Metrics
  display.setCursor(0, 14);
  display.print(F("ENV:    ")); display.println(condition);
  display.print(F("TEMP:   ")); display.print(temp); display.println(F(" C"));
  display.print(F("HUMID:  ")); display.print(humidity); display.println(F("%"));
  display.print(F("RAIN%:  ")); display.print(rainChance); display.println(F("%"));
  
  // Footer Border Line
  display.drawFastHLine(0, 49, 128, SH110X_WHITE);

  // 4. Network Metadata Row
  display.setCursor(0, 54);
  display.print(F("IP: ")); display.println(ipAddress);

  display.display();
}