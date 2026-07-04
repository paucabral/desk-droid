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

// Bind variables and states instantiated in main.cpp
extern String live_location, live_condition;
extern int live_temp, live_humidity, live_rain_chance, live_battery_percentage;
extern bool showing_dashboard, web_control_active, robot_sleeping; 
extern bool accel_available; 
extern unsigned long web_mode_timeout, dashboard_timeout;

extern void forceWebEmotion(String type);
extern void playSoundAsync(int soundId);

const char* HTML_DASHBOARD = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8"> <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>DeskDroid Pilot Terminal</title>
    <style>
        :root {
            --bg: #121214;
            --card-bg: #1a1a1e;
            --border: #29292e;
            --text: #e1e1e6;
            --text-muted: #7c7c8a;
            --accent: #4af6c3;
            --accent-shadow: rgba(74,246,195,0.2);
            --btn-bg: #29292e;
            --btn-active-text: #121214;
            --stop-btn: #e54949;
            --stop-btn-active: #ff6b6b;
            --utility-btn: #34495e;
            --utility-btn-active: #3498db;
            --badge-ok: #27ae60;
            --badge-fail: #e74c3c;
            --toggle-bg: #29292e;
        }

        body.light-theme {
            --bg: #f4f5f6;
            --card-bg: #ffffff;
            --border: #e2e4e8;
            --text: #1a1a1e;
            --text-muted: #64748b;
            --accent: #00bfa5;
            --accent-shadow: rgba(0,191,165,0.2);
            --btn-bg: #eaedf2;
            --btn-active-text: #ffffff;
            --stop-btn: #d93838;
            --stop-btn-active: #f87171;
            --utility-btn: #64748b;
            --utility-btn-active: #3b82f6;
            --badge-ok: #2ed573;
            --badge-fail: #ff4757;
            --toggle-bg: #eaedf2;
        }

        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: var(--bg); color: var(--text); text-align: center; padding: 20px; margin: 0; transition: background 0.2s, color 0.2s; }
        h1 { color: var(--accent); margin-bottom: 5px; font-size: 1.8rem; text-shadow: 0 0 10px var(--accent-shadow); }
        .subtitle { color: var(--text-muted); font-size: 0.9rem; margin-bottom: 15px; }
        
        .section { background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 15px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); text-align: left; }
        .section-title { font-size: 0.9rem; color: var(--text-muted); text-transform: uppercase; margin-top: 0; margin-bottom: 12px; font-weight: 600; letter-spacing: 0.05em; text-align: center; }
        
        .btn { background: var(--btn-bg); border: none; color: var(--text); padding: 12px; font-size: 1rem; border-radius: 8px; cursor: pointer; transition: all 0.15s ease; font-weight: 500; }
        .btn:active { background: var(--accent); color: var(--btn-active-text); transform: scale(0.95); box-shadow: 0 0 12px var(--accent-shadow); }
        
        .theme-toggle-container { margin-bottom: 25px; text-align: center; display: flex; justify-content: center; align-items: center; gap: 10px; }
        .theme-label { font-size: 0.9rem; font-weight: 600; color: var(--text); }

        .switch {
            position: relative;
            display: inline-block;
            width: 50px;
            height: 26px;
            flex-shrink: 0;
        }
        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0; left: 0; right: 0; bottom: 0;
            background-color: var(--toggle-bg);
            transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
            border-radius: 34px;
            border: 1px solid var(--border);
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 18px;
            width: 18px;
            left: 3px;
            bottom: 3px;
            background-color: var(--text-muted);
            transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: var(--accent);
            border-color: var(--accent);
        }
        input:checked + .slider:before {
            transform: translateX(24px);
            background-color: var(--btn-active-text);
        }

        .metrics-wrapper { display: flex; flex-direction: column; gap: 20px; max-width: 720px; margin: 0 auto 20px auto; }
        .metrics-wrapper .section { flex: 1; margin-bottom: 0; }

        @media (min-width: 640px) {
            .metrics-wrapper { flex-direction: row; }
        }

        .telemetry-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; font-size: 0.95rem; }
        .tele-item { background: var(--btn-bg); padding: 10px; border-radius: 6px; border: 1px solid var(--border); }
        .tele-label { color: var(--text-muted); font-size: 0.75rem; text-transform: uppercase; margin-bottom: 2px; font-weight: bold; }
        .tele-value { color: var(--text); font-weight: 600; }
        .highlight { color: var(--accent); }

        .tele-toggle-container { display: flex; justify-content: space-between; align-items: center; grid-column: span 2; }

        .check-list { display: flex; flex-direction: column; gap: 8px; font-size: 0.9rem; font-weight: 600; }
        .check-item { display: flex; justify-content: space-between; align-items: center; background: var(--btn-bg); padding: 8px 12px; border-radius: 6px; border: 1px solid var(--border); }
        .badge { font-size: 0.75rem; padding: 3px 8px; border-radius: 4px; color: #ffffff; text-transform: uppercase; font-weight: bold; }
        .badge-ok { background: var(--badge-ok); }
        .badge-fail { background: var(--badge-fail); }

        .control-block { text-align: center; }
        .flex-row { display: flex; flex-wrap: wrap; gap: 10px; justify-content: center; }
        .flex-row .btn { flex: 1; min-width: 90px; }
        
        .pill-standby-box { display: flex; justify-content: space-between; align-items: center; background: var(--btn-bg); padding: 12px 20px; border-radius: 8px; border: 1px solid var(--border); max-width: 280px; margin: 0 auto; }

        .remote-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; max-width: 240px; margin: 0 auto; }
        .remote-grid .btn { padding: 18px 10px; font-size: 1.2rem; display: flex; align-items: center; justify-content: center; }
        .btn-stop { background: var(--stop-btn); color: #ffffff; font-weight: bold; font-size: 0.9rem !important; }
        .btn-stop:active { background: var(--stop-btn-active); color: #ffffff; box-shadow: 0 0 12px rgba(229,73,73,0.3); }
        .empty-cell { visibility: hidden; }
    </style>
</head>
<body>
    <h1>DESK DROID</h1>
    <div class="subtitle">Pilot Interface Terminal v1.6</div>

    <div class="theme-toggle-container">
        <span class="theme-label">🌓 Light Mode</span>
        <label class="switch">
            <input type="checkbox" id="theme-checkbox" onchange="toggleTheme()">
            <span class="slider"></span>
        </label>
    </div>

    <div class="metrics-wrapper">
        
        <div class="section">
            <div class="section-title">Live Telemetry Feeds</div>
            <div class="telemetry-grid">
                <div class="tele-item" style="grid-column: span 2;"><div class="tele-label">📍 Location</div><div id="val-loc" class="tele-value">Loading...</div></div>
                <div class="tele-item"><div class="tele-label">🔋 Battery Level</div><div id="val-batt" class="tele-value highlight">--%</div></div>
                <div class="tele-item"><div class="tele-label">☁️ Condition</div><div id="val-cond" class="tele-value">--</div></div>
                <div class="tele-item"><div class="tele-label">🌡️ Temperature</div><div id="val-temp" class="tele-value">--°C</div></div>
                <div class="tele-item"><div class="tele-label">💧 Humidity</div><div id="val-hum" class="tele-value">--%</div></div>
                <div class="tele-item" style="grid-column: span 2;"><div class="tele-label">🌧️ Precipitation Chance</div><div id="val-rain" class="tele-value">--%</div></div>
                
                <div class="tele-item tele-toggle-container">
                    <div>
                        <div class="tele-label" style="margin: 0;">🖥️ OLED Dashboard HUD</div>
                        <div id="txt-hud-status" style="font-size: 0.8rem; font-weight: bold; color: var(--text-muted);">Syncing...</div>
                    </div>
                    <label class="switch">
                        <input type="checkbox" id="hud-checkbox" onchange="toggleHUD()">
                        <span class="slider"></span>
                    </label>
                </div>
            </div>
        </div>

        <div class="section">
            <div class="section-title">Systems Check</div>
            <div class="check-list">
                <div class="check-item"><div><span style="color:var(--text-muted); font-size:0.75rem; display:block; font-weight:bold; text-transform:uppercase;">🌐 Network Connectivity</span><span id="val-net-ip" style="font-size:0.95rem; color:var(--text);">Syncing IP...</span></div><span id="val-net-status" class="badge badge-fail">OFFLINE</span></div>
                <div class="check-item"><span>🤖 ACCELEROMETER</span><span id="chk-accel" class="badge badge-fail">--</span></div>
                <div class="check-item"><span>👉 TOUCH SENSOR</span><span id="chk-touch" class="badge badge-ok">READY</span></div>
                <div class="check-item"><span>☀️ LIGHT SENSOR (LDR)</span><span id="chk-light" class="badge badge-ok">READY</span></div>
                <div class="check-item"><span>🔊 BUZZER AUDIO CORE</span><span id="chk-audio" class="badge badge-ok">READY</span></div>
                <div class="check-item"><span>⚙️ L9110S MOTOR DRIVER</span><span id="chk-motors" class="badge badge-ok">READY</span></div>
            </div>
        </div>

    </div>

    <div class="section control-block">
        <div class="section-title">Emotion Engine Trigger</div>
        <div class="flex-row">
            <button class="btn" onclick="sendCmd('/emotion?type=happy')">Happy 😊</button>
            <button class="btn" onclick="sendCmd('/emotion?type=angry')">Angry 😡</button>
            <button class="btn" onclick="sendCmd('/emotion?type=confused')">Surprise 😲</button>
            <button class="btn" onclick="sendCmd('/emotion?type=sweat')">Sweat 💦</button>
            <button class="btn" onclick="sendCmd('/emotion?type=default')">Reset 🔄</button>
        </div>
    </div>

    <div class="section control-block">
        <div class="section-title">System Standby Control</div>
        <div class="pill-standby-box">
            <span id="txt-sleep-status" style="font-weight: 600; font-size: 0.95rem; color: var(--text);">Droid Awake ☀️</span>
            <label class="switch">
                <input type="checkbox" id="sleep-checkbox" onchange="toggleSleep()">
                <span class="slider"></span>
            </label>
        </div>
    </div>

    <div class="section control-block">
        <div class="section-title">Manual Drive Core</div>
        <div class="remote-grid">
            <div class="empty-cell"></div>
            <button class="btn" onclick="sendCmd('/drive?dir=forward')">▲</button>
            <div class="empty-cell"></div>
            
            <button class="btn" onclick="sendCmd('/drive?dir=left')">◀</button>
            <button class="btn btn-stop" onclick="sendCmd('/drive?dir=stop')">STOP</button>
            <button class="btn" onclick="sendCmd('/drive?dir=right')">▶</button>
            
            <div class="empty-cell"></div>
            <button class="btn" onclick="sendCmd('/drive?dir=backward')">▼</button>
            <div class="empty-cell"></div>
        </div>
    </div>

    <script>
        let serverShowingDashboard = false;

        function sendCmd(endpoint) {
            fetch(endpoint).catch(err => console.error('Transmission fault:', err));
        }

        function toggleTheme() {
            const isLight = document.getElementById('theme-checkbox').checked;
            document.body.classList.toggle('light-theme', isLight);
        }

        function toggleHUD() {
            const isChecked = document.getElementById('hud-checkbox').checked;
            sendCmd('/ui?view=' + (isChecked ? 'dashboard' : 'eyes'));
        }

        function toggleSleep() {
            const isSleeping = document.getElementById('sleep-checkbox').checked;
            sendCmd('/sleep?state=' + (isSleeping ? 'on' : 'off'));
        }

        function fetchTelemetry() {
            fetch('/telemetry')
                .then(response => response.json())
                .then(data => {
                    try {
                        document.getElementById('val-loc').innerText = data.location;
                        document.getElementById('val-batt').innerText = data.battery + "%";
                        document.getElementById('val-cond').innerText = data.condition;
                        document.getElementById('val-temp').innerText = data.temp + " °C";
                        document.getElementById('val-hum').innerText = data.humidity + "%";
                        document.getElementById('val-rain').innerText = data.rain_chance + "%";
                        
                        serverShowingDashboard = data.showing_dashboard;
                        const hudCheckbox = document.getElementById('hud-checkbox');
                        const statusText = document.getElementById('txt-hud-status');
                        
                        hudCheckbox.checked = serverShowingDashboard;
                        if (serverShowingDashboard) {
                            statusText.innerText = "ACTIVE ON GLASS";
                            statusText.style.color = "var(--accent)";
                        } else {
                            statusText.innerText = "EYES RENDERING";
                            statusText.style.color = "var(--text-muted)";
                        }
                    } catch(e) { console.warn("Left column processing deferred:", e); }

                    try {
                        document.getElementById('val-net-ip').innerText = "IP: " + data.net_ip;
                        const netStatusBadge = document.getElementById('val-net-status');
                        netStatusBadge.innerText = data.net_status;
                        netStatusBadge.className = data.net_status === "ONLINE" ? "badge badge-ok" : "badge badge-fail";

                        const accelBadge = document.getElementById('chk-accel');
                        if (data.accel_ok) {
                            accelBadge.innerText = "READY";
                            accelBadge.className = "badge badge-ok";
                        } else {
                            accelBadge.innerText = "BYPASSED";
                            accelBadge.className = "badge badge-fail";
                        }
                    } catch(e) { console.warn("Right column diagnostics processing deferred:", e); }

                    try {
                        const sleepCheckbox = document.getElementById('sleep-checkbox');
                        const sleepText = document.getElementById('txt-sleep-status');
                        sleepCheckbox.checked = data.robot_sleeping;
                        if (data.robot_sleeping) {
                            sleepText.innerText = "Droid Sleeping 💤";
                            sleepText.style.color = "var(--text-muted)";
                        } else {
                            sleepText.innerText = "Droid Awake ☀️";
                            sleepText.style.color = "var(--text)";
                        }
                    } catch(e) { console.warn("Standby switch syncing deferred:", e); }
                })
                .catch(err => console.error('Telemetry stream dropped:', err));
        }

        fetchTelemetry();
        setInterval(fetchTelemetry, 2500);
    </script>
</body>
</html>
)rawliteral";

void refreshWebHeartbeat() {
  web_control_active = true;
  web_mode_timeout = millis() + 30000;
  if (robot_sleeping) {
    robot_sleeping = false;
    playSoundAsync(S_CONNECTION);
  }
}

void initWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", HTML_DASHBOARD);
  });

  server.on("/telemetry", HTTP_GET, []() {
    JsonDocument doc;
    doc["location"] = live_location;
    doc["battery"] = live_battery_percentage;
    doc["condition"] = live_condition;
    doc["temp"] = live_temp;
    doc["humidity"] = live_humidity;
    doc["rain_chance"] = live_rain_chance;
    doc["showing_dashboard"] = showing_dashboard;
    doc["robot_sleeping"] = robot_sleeping; 
    
    doc["net_status"] = (WiFi.status() == WL_CONNECTED) ? "ONLINE" : "OFFLINE";
    doc["net_ip"] = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "N/A";
    doc["accel_ok"] = accel_available; 

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
  });

  server.on("/drive", HTTP_GET, []() {
    refreshWebHeartbeat();
    String direction = server.arg("dir");
    if (direction == "forward")       { moveDroid(FORWARD); }
    else if (direction == "backward") { moveDroid(BACKWARD); }
    else if (direction == "left")     { moveDroid(TURN_LEFT); }
    else if (direction == "right")    { moveDroid(TURN_RIGHT); }
    else                              { moveDroid(STOP); }
    server.send(200, "text/plain", "OK");
  });

  server.on("/emotion", HTTP_GET, []() {
    refreshWebHeartbeat();
    String emotionType = server.arg("type");
    forceWebEmotion(emotionType);
    server.send(200, "text/plain", "OK");
  });

  server.on("/ui", HTTP_GET, []() {
    refreshWebHeartbeat();
    String viewMode = server.arg("view");
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
    } else {
      robot_sleeping = false;
      playSoundAsync(S_CONNECTION);
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
  Serial.println(F("Entered Configuration Portal Mode!"));
  drawWifiScreen("PORTAL ACTIVE", "CONFIG MODE", "Connect your device\nto: Desk-Droid-Setup");
  playSoundAsync(S_MODE1); 
}

void fetchLocationAndWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[API-DEBUG] Cannot update weather. WiFi disconnected."));
    return;
  }

  HTTPClient http; JsonDocument doc; float lat = 0.0, lon = 0.0;
  http.begin("http://ip-api.com/json/");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString(); DeserializationError error = deserializeJson(doc, payload);
    if (!error) { live_location = doc["city"].as<String>(); lat = doc["lat"].as<float>(); lon = doc["lon"].as<float>(); }
  }
  http.end(); if (lat == 0.0 && lon == 0.0) return;
  doc.clear(); 
  
  String weatherUrl = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat, 4) + "&lon=" + String(lon, 4) + "&appid=" + String(OPENWEATHER_API_KEY) + "&units=metric";
  http.begin(weatherUrl);
  httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString(); DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      live_condition = doc["weather"][0]["main"].as<String>(); live_temp = round(doc["main"]["temp"].as<float>());
      live_humidity = doc["main"]["humidity"].as<int>(); live_rain_chance = doc["clouds"]["all"].as<int>(); live_condition.toUpperCase(); 
      Serial.println(F("[API-DEBUG] Weather updated successfully!"));
    }
  }
  http.end();
}