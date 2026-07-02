#pragma once
#include <Adafruit_SH110X.h>

// Use 'extern' to let other files reference the master display object instantiated in main.cpp
extern Adafruit_SH1106G display;

// Function declarations for our UI views
void drawChecklist(const char* accel, const char* touch, const char* light, const char* audio, const char* sysMsg);
void drawSplashArt();