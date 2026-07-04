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

void drawDashboard(const char* condition, int temp, int humidity, int rain_chance, const char* ip, int battery, const char* location, const char* localTime) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  
  // ─── 1. TOP BAR: TIME, DATE, & BATTERY STATUS ───
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(localTime); // Renders "07:37PM 07/04"
  
  String battStr = String(battery) + "%";
  // Dynamically right-align battery string based on character length
  int battX = 128 - (battStr.length() * 6) - 12; 
  display.setCursor(battX, 0);
  display.print("B:" + battStr);
  
  // Clean horizontal divider rule below top header bar
  display.drawLine(0, 9, 128, 9, SH110X_WHITE);
  
  // ─── 2. MIDDLE AREA: THERMAL CORE & ATMOSPHERE GRID ───
  // Large Font Temperature Readout (Size 2 = 12x16px per char)
  display.setTextSize(2);
  display.setCursor(0, 13);
  display.print(String(temp));
  
  // Custom tiny superscript degree circle workaround positioning
  display.setTextSize(1);
  display.print("o"); 
  display.setTextSize(2);
  display.print("C");
  
  // Right Column Secondary Environmental Sub-metrics
  display.setTextSize(1);
  display.setCursor(76, 13);
  display.print("H: "); display.print(humidity); display.print("%");
  display.setCursor(76, 22);
  display.print("R: "); display.print(rain_chance); display.print("%");
  
  // ─── 3. LOWER AREA: WEATHER CONDITION & GEOLOCATION ───
  display.setCursor(0, 32);
  display.print("SKY: "); display.print(condition);
  
  display.setCursor(0, 41);
  display.print("LOC: "); display.print(location);
  
  // Clean horizontal divider rule above bottom network bar
  display.drawLine(0, 50, 128, 50, SH110X_WHITE);
  
  // ─── 4. BOTTOM BAR: SYSTEM IP ADDRESS RAIL ───
  display.setCursor(0, 53);
  display.print("IP : "); display.print(ip);
  
  display.display();
}