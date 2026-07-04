#include "network_system.h"
#include "config.h"
#include "display_ui.h"
#include "audio_system.h"
#include "motors.h"
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <CuteBuzzerSounds.h>

WebServer server(80);

// Global Sliding System Log Buffer Assets
String webTerminalLog = "";

// Bind variables and states instantiated in main.cpp
extern String live_location, live_condition;
extern int live_temp, live_humidity, live_rain_chance, live_battery_percentage;
extern bool showing_dashboard, web_control_active, robot_sleeping; 
extern bool accel_available; 
extern unsigned long web_mode_timeout, dashboard_timeout;

// Extern Sensor Hardware Bindings
extern int live_light_level;
extern float live_accel_x, live_accel_y, live_accel_z;
extern bool live_touch_active; 
extern bool live_base_connected;
extern String live_time_long;
extern bool live_use_military_time;

extern int live_hour_offset;
extern long live_timezone_offset_sec;
extern void updateTimeStrings();

// FIXED: Added 'extern' to prevent compilation linker collisions
extern long live_timezone_offset_sec;

extern void updateTimeStrings();
extern void forceWebEmotion(String type);
extern void playSoundAsync(int soundId);

// --- COMPILER ASSET LINHER HOOKS ---
extern const uint8_t dashboard_html_start[] asm("_binary_src_dashboard_html_start");
extern const uint8_t dashboard_html_end[]   asm("_binary_src_dashboard_html_end");

void logTerminal(String msg) {
  Serial.println(msg); 
  webTerminalLog += msg + "\n";
  if (webTerminalLog.length() > 1500) {
    webTerminalLog = webTerminalLog.substring(webTerminalLog.length() - 750);
  }
}

void refreshWebHeartbeat() {
  web_control_active = true;
  web_mode_timeout = millis() + 30000;
  if (robot_sleeping) {
    robot_sleeping = false;
    playSoundAsync(S_CONNECTION);
  }
}

void initWebServer() {
  // Route 1: Serves your raw asset file directly over the local network space
  server.on("/", HTTP_GET, []() {
    size_t dataLength = (size_t)(dashboard_html_end - dashboard_html_start);
    server.setContentLength(dataLength);                               
    server.send(200, "text/html", "");                                
    server.sendContent((const char*)dashboard_html_start, dataLength); 
  });

  server.on("/serial_data", HTTP_GET, []() {
    server.send(200, "text/plain", webTerminalLog);
    webTerminalLog = ""; 
  });

  server.on("/toggle_time", HTTP_GET, []() {
    refreshWebHeartbeat();
    live_use_military_time = !live_use_military_time;
    
    // Force an immediate recalculation of the time string right now
    updateTimeStrings(); 
    
    logTerminal("[SYSTEM] Formatting update processed. Military mode state: " + String(live_use_military_time ? "ACTIVE" : "DISABLED"));
    server.send(200, "text/plain", "OK");
  });

  // MANUAL HOUR OFFSET INTERCEPT ENDPOINT ---
  server.on("/set_offset", HTTP_GET, []() {
    refreshWebHeartbeat();
    if (server.hasArg("hours")) {
      live_hour_offset = server.arg("hours").toInt();
      
      // Calculate seconds from hour input and push to core OS clock engine
      live_timezone_offset_sec = (long)live_hour_offset * 3600;
      configTime(live_timezone_offset_sec, 0, NTP_SERVER);
      
      // Force an immediate regeneration of the text clocks
      updateTimeStrings(); 
      
      logTerminal("[SYSTEM] Manual time shift adjusted to: UTC " + String(live_hour_offset >= 0 ? "+" : "") + String(live_hour_offset));
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/sensor_data", HTTP_GET, []() {
    JsonDocument doc;
    doc["touch_active"] = live_touch_active;
    doc["raw_light"] = live_light_level;
    doc["raw_ax"] = live_accel_x;
    doc["raw_ay"] = live_accel_y;
    doc["raw_az"] = live_accel_z;
    doc["accel_ok"] = accel_available;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
  });

  server.on("/telemetry", HTTP_GET, []() {
    JsonDocument doc;
    doc["location"] = live_location;
    doc["battery"] = live_battery_percentage;
    doc["condition"] = live_condition;
    doc["local_date_time"] = live_time_long;
    doc["military_time"] = live_use_military_time;
    doc["hour_offset"] = live_hour_offset;
    doc["temp"] = live_temp;
    doc["humidity"] = live_humidity;
    doc["rain_chance"] = live_rain_chance;
    doc["showing_dashboard"] = showing_dashboard;
    doc["robot_sleeping"] = robot_sleeping;
    doc["base_connected"] = live_base_connected; 

    doc["net_status"] = (WiFi.status() == WL_CONNECTED) ? "ONLINE" : "OFFLINE";
    doc["net_ip"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "N/A";

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
  });

  server.on("/drive", HTTP_GET, []() {
    refreshWebHeartbeat();
    String direction = server.arg("dir");
    if (direction == "forward")       { moveDroid(FORWARD); logTerminal(F("[DRIVE] Locomotion packet: FORWARD")); }
    else if (direction == "backward") { moveDroid(BACKWARD); logTerminal(F("[DRIVE] Locomotion packet: BACKWARD")); }
    else if (direction == "left")     { moveDroid(TURN_LEFT); logTerminal(F("[DRIVE] Locomotion packet: PIVOT_LEFT")); }
    else if (direction == "right")    { moveDroid(TURN_RIGHT); logTerminal(F("[DRIVE] Locomotion packet: PIVOT_RIGHT")); }
    else                              { moveDroid(STOP); logTerminal(F("[DRIVE] Locomotion packet: HALT_ALL_ENGINES")); }
    server.send(200, "text/plain", "OK");
  });

  server.on("/emotion", HTTP_GET, []() {
    refreshWebHeartbeat();
    String emotionType = server.arg("type");
    logTerminal("[EMOTION] Request processed: " + emotionType);
    forceWebEmotion(emotionType);
    server.send(200, "text/plain", "OK");
  });

  server.on("/ui", HTTP_GET, []() {
    refreshWebHeartbeat();
    String viewMode = server.arg("view");
    logTerminal("[DISPLAY-UI] Direct page layer swap requested: " + viewMode);
    if (viewMode == "dashboard") {
      showing_dashboard = true;
      dashboard_timeout = millis() + 10000;
    } else {
      showing_dashboard = false;
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/sleep", HTTP_GET, []() {
    String state = server.arg("state");
    if (state == "on") {
      robot_sleeping = true;
      moveDroid(STOP); 
      playSoundAsync(S_DISCONNECTION);
      logTerminal(F("[STANDBY] Droid entered sleep state. Radars offline."));
    } else {
      robot_sleeping = false;
      playSoundAsync(S_CONNECTION);
      logTerminal(F("[STANDBY] Droid soft awoken. Orchestrator core running."));
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println(F("[WEB-SERVER] Droid remote terminal successfully online on Port 80."));
}

void handleWebClient() {
  server.handleClient();
}

void configModeCallback(WiFiManager *myWiFiManager) {
  drawWifiScreen("PORTAL ACTIVE", "CONFIG MODE", "Connect your device\nto: Desk-Droid-Setup");
  playSoundAsync(S_MODE1); 
}

void fetchLocationAndWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    logTerminal(F("[API-WARN] Cannot update weather logs. WiFi connection missing."));
    return;
  }

  HTTPClient http; 
  JsonDocument doc; 
  float lat = 0.0, lon = 0.0;
  
  logTerminal(F("[API-SYSTEM] Initializing IP Geolocation check packet..."));
  http.begin("http://ip-api.com/json/");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString(); 
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) { 
      if (doc["status"] == "success") {
        live_location = doc["city"].as<String>(); 
        lat = doc["lat"].as<float>(); 
        lon = doc["lon"].as<float>(); 
        
        logTerminal("[API-SUCCESS] Timezone synchronized and calibrated: " + String(live_timezone_offset_sec) + "s");
      } else {
        logTerminal("[API-WARN] Geolocation throttled or private network. Preserving default UTC+8 offset.");
      }
    }
  }
  http.end(); 
  
  if (lat == 0.0 && lon == 0.0) return;
  doc.clear(); 
  
  logTerminal("[API-SYSTEM] Localizing to coordinate space matrix: Lat=" + String(lat, 2) + ", Lon=" + String(lon, 2));
  String weatherUrl = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat, 4) + "&lon=" + String(lon, 4) + "&appid=" + String(OPENWEATHER_API_KEY) + "&units=metric";
  
  http.begin(weatherUrl);
  httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString(); 
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) { 
      live_temp = (int)doc["main"]["temp"].as<float>();
      live_humidity = doc["main"]["humidity"].as<int>();
      live_condition = doc["weather"][0]["main"].as<String>();
      
      if (doc.containsKey("clouds")) {
        live_rain_chance = doc["clouds"]["all"].as<int>();
      }
      logTerminal("[API-SUCCESS] OpenWeather metrics synchronized successfully.");
    }
  }
  http.end();
}