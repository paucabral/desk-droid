#pragma once
#include <Adafruit_SH110X.h>

extern Adafruit_SH1106G display;

void drawChecklist(const char* accel, const char* touch, const char* light, const char* audio, const char* motors, const char* sysMsg);
void drawSplashArt();
void drawWifiScreen(const char* status, const char* ipAddress);