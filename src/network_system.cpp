#include "network_system.h"
#include "config.h"
#include "display_ui.h"
#include "audio_system.h"
#include "motors.h"
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <CuteBuzzerSounds.h>

// Instantiate the localized Web Server on port 80
WebServer server(80);

// Bind variables and states instantiated over in main.cpp
extern String live_location, live_condition;
extern int live_temp, live_humidity, live_rain_chance;
extern bool showing_dashboard, web_control_active;
extern unsigned long dashboard_timeout;
extern unsigned long web_mode_timeout;

// Linker hooks to access RoboEyes wrappers inside main.cpp safely
extern void playSoundAsync(int soundId);

// Responsive HTML/CSS UI Dashboard Asset Dashboard
const char* HTML_DASHBOARD = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>DeskDroid Pilot Terminal</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #121214; color: #e1e1e6; text-align: center; padding: 20px; margin: 0; }
        h1 { color: #4af6c3; margin-bottom: 5px; font-size: 1.8rem; text-shadow: 0 0 10px rgba(74,246,195,0.2); }
        .subtitle { color: #7c7c8a; font-size: 0.9rem; margin-bottom: 25px; }
        .section { background: #1a1a1e; border: 1px solid #29292e; border-radius: 12px; padding: 15px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .section-title { font-size: 1rem; color: #7c7c8a; text-transform: uppercase; margin-top: 0; margin-bottom: 12px; font-weight: 600; letter-spacing: 0.05em; }
        .grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; max-width: 320px; margin: 0 auto; }
        .btn { background: #29292e; border: none; color: #e1e1e6; padding: 12px; font-size: 1rem; border-radius: 8px; cursor: pointer; transition: all 0.15s ease; font-weight: 500; }
        .btn:active { background: #4af6c3; color: #121214; transform: scale(0.95); box-shadow: 0 0 12px rgba(74,246,195,0.4); }
        .btn-stop { background: #e54949; color: white; font-weight: bold; }
        .btn-stop:active { background: #ff6b6b; color: white; box-shadow: 0 0 12px rgba(229,73,73,0.4); }
        .flex-row { display: flex; flex-wrap: wrap; gap: 10px; justify-content: center; }
        .flex-row .btn { flex: 1; min-width: 90px; }
    </style>
</head>
<body>
    <h1>DESK DROID</h1>
    <div class="subtitle">Pilot Interface Terminal v1.0</div>

    <div class="section">
        <div class="section-title">Manual Drive Core</div>
        <div class="grid">
            <td></td><button class="btn" onclick="sendCmd('/drive?dir=forward')">▲</button><td></td>
            <button class="btn" onclick="sendCmd('/drive?dir=left')">◀</button>
            <button class="btn btn-stop" onclick="sendCmd('/drive?dir=stop')">STOP</button>
            <button class="btn" onclick="sendCmd('/drive?dir=right')">▶</button>
            <td></td><button class="btn" onclick="sendCmd('/drive?dir=backward')">▼</button><td></td>
        </div>
    </div>

    <div class="section">
        <div class="section-title">Emotion Engine Trigger</div>
        <div class="flex-row">
            <button class="btn" onclick="sendCmd('/emotion?type=happy')">Happy</button>
            <button class="btn" onclick="sendCmd('/emotion?type=angry')">Angry</button>
            <button class="btn" onclick="sendCmd('/emotion?type=confused')">Surprise</button>
            <button class="btn" onclick="sendCmd('/emotion?type=default')">Reset</button>
        </div>
    </div>

    <div class="section">
        <div class="section-title">UI Routing Matrix</div>
        <div class="flex-row">
            <button class="btn" onclick="sendCmd('/ui?view=dashboard')">Show HUD</button>
            <button class="btn" onclick="sendCmd('/ui?view=eyes')">Close HUD</button>
        </div>
    </div>

    <script>
        function sendCmd(endpoint) {
            fetch(endpoint).catch(err => console.error('Transmission fault:', err));
        }
    </script>
</body>
</html>
)rawliteral";

// Helper macro to instantly flag manual web interaction state overrides
void refreshWebHeartbeat() {
  web_control_active = true;
  web_mode_timeout = millis() + 30000; // Extend manual mode out for 30 seconds
}

void initWebServer() {
  // Route 1: Serve main control graphic dashboard
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", HTML_DASHBOARD);
  });

  // Route 2: Asynchronous Micro Movement Handler Endpoints
  server.on("/drive", HTTP_GET, []() {
    refreshWebHeartbeat();
    String direction = server.arg("dir");
    
    if (direction == "forward")  { moveDroid(FORWARD); }
    else if (direction == "backward") { moveDroid(BACKWARD); }
    else if (direction == "left")     { moveDroid(TURN_LEFT); }
    else if (direction == "right")    { moveDroid(TURN_RIGHT); }
    else                              { moveDroid(STOP); }
    
    server.send(200, "text/plain", "OK");
  });

  // Route 3: Dynamic Async Emotion Core Overrides
  server.on("/emotion", HTTP_GET, []() {
    refreshWebHeartbeat();
    String emotionType = server.arg("type");
    
    // Use direct pointer manipulation via bridge layer in main.cpp
    extern void forceWebEmotion(String type);
    forceWebEmotion(emotionType);
    
    server.send(200, "text/plain", "OK");
  });

  // Route 4: UI System Page Router Controls
  server.on("/ui", HTTP_GET, []() {
    refreshWebHeartbeat();
    String viewMode = server.arg("view");
    
    if (viewMode == "dashboard") {
      showing_dashboard = true;
      dashboard_timeout = millis() + 10000; // Render diagnostic map for 10 seconds
    } else {
      showing_dashboard = false;
    }
    
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println(F("[WEB-SERVER] Droid remote terminal successfully online on Port 80."));
}

void handleWebClient() {
  server.handleClient();
}

// Stubs for original dependencies
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println(F("Entered Configuration Portal Mode!"));
  drawWifiScreen("PORTAL ACTIVE", "CONFIG MODE", "Connect your device\nto: Desk-Droid-Setup");
  playSoundAsync(S_MODE1); 
}

void fetchLocationAndWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http; JsonDocument doc; float lat = 0.0, lon = 0.0;
  http.begin("http://ip-api.com/json/");
  if (http.GET() == HTTP_CODE_OK) {
    String p = http.getString(); DeserializationError e = deserializeJson(doc, p);
    if (!e) { live_location = doc["city"].as<String>(); lat = doc["lat"].as<float>(); lon = doc["lon"].as<float>(); }
  }
  http.end(); if (lat == 0.0 && lon == 0.0) return;
  doc.clear();
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat,4) + "&lon=" + String(lon,4) + "&appid=" + String(OPENWEATHER_API_KEY) + "&units=metric";
  http.begin(url);
  if (http.GET() == HTTP_CODE_OK) {
    String p = http.getString(); DeserializationError e = deserializeJson(doc, p);
    if (!e) {
      live_condition = doc["weather"][0]["main"].as<String>(); live_temp = round(doc["main"]["temp"].as<float>());
      live_humidity = doc["main"]["humidity"].as<int>(); live_rain_chance = doc["clouds"]["all"].as<int>(); live_condition.toUpperCase();
    }
  }
  http.end();
}