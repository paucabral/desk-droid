#pragma once
#include <WiFiManager.h>

// Web Request Orchestration Engines
void configModeCallback(WiFiManager *myWiFiManager);
void fetchLocationAndWeather();