#include "network_system.h"
#include "config.h"
#include "display_ui.h"
#include "audio_system.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <CuteBuzzerSounds.h>

// Share global string registries instantiated in main.cpp
extern String live_location;
extern String live_condition;
extern int live_temp;
extern int live_humidity;
extern int live_rain_chance;

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println(F("Entered Configuration Portal Mode!"));
  drawWifiScreen("PORTAL ACTIVE", "CONFIG MODE", "Connect your device\nto: Desk-Droid-Setup");
  playSoundAsync(S_MODE1); 
}

void fetchLocationAndWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[API-DEBUG] Cannot update weather. WiFi disconnected."));
    return;
  }

  HTTPClient http;
  JsonDocument doc; 
  float lat = 0.0, lon = 0.0;

  Serial.println(F("[API-DEBUG] Requesting IP Geolocation..."));
  http.begin("http://ip-api.com/json/");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      live_location = doc["city"].as<String>();
      lat = doc["lat"].as<float>();
      lon = doc["lon"].as<float>();
    }
  }
  http.end(); 

  if (lat == 0.0 && lon == 0.0) return;

  Serial.println(F("[API-DEBUG] Requesting Meteorological Telemetry..."));
  doc.clear(); 

  String weatherUrl = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat, 4) + 
                      "&lon=" + String(lon, 4) + 
                      "&appid=" + String(OPENWEATHER_API_KEY) + 
                      "&units=metric";

  http.begin(weatherUrl);
  httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      live_condition = doc["weather"][0]["main"].as<String>();
      live_temp = round(doc["main"]["temp"].as<float>());
      live_humidity = doc["main"]["humidity"].as<int>();
      live_rain_chance = doc["clouds"]["all"].as<int>(); 
      live_condition.toUpperCase(); 
      Serial.println(F("[API-DEBUG] Weather updated successfully!"));
    }
  }
  http.end();
}