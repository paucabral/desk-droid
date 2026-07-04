#pragma once
#include <WiFiManager.h>

// Web Request Orchestration Engines
void configModeCallback(WiFiManager *myWiFiManager);
void fetchLocationAndWeather();

// Web UI Control Subsystems
void initWebServer();
void handleWebClient();