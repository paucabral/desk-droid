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

extern void forceWebEmotion(String type);
extern void playSoundAsync(int soundId);

const char* HTML_DASHBOARD = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
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
            --term-bg: #0a0a0c;
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
            --term-bg: #1e293b;
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
            position: relative; display: inline-block; width: 50px; height: 26px; flex-shrink: 0;
        }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider {
            position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
            background-color: var(--toggle-bg); transition: .3s cubic-bezier(0.4, 0, 0.2, 1);
            border-radius: 34px; border: 1px solid var(--border);
        }
        .slider:before {
            position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px;
            background-color: var(--text-muted); transition: .3s cubic-bezier(0.4, 0, 0.2, 1); border-radius: 50%;
        }
        input:checked + .slider { background-color: var(--accent); border-color: var(--accent); }
        input:checked + .slider:before { transform: translateX(24px); background-color: var(--btn-active-text); }

        .metrics-wrapper { display: flex; flex-direction: column; gap: 20px; max-width: 720px; margin: 0 auto 20px auto; }
        .metrics-wrapper .section { flex: 1; margin-bottom: 0; }

        @media (min-width: 640px) { .metrics-wrapper { flex-direction: row; } }

        .telemetry-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; font-size: 0.95rem; }
        .tele-item { background: var(--btn-bg); padding: 10px; border-radius: 6px; border: 1px solid var(--border); }
        .tele-label { color: var(--text-muted); font-size: 0.75rem; text-transform: uppercase; margin-bottom: 2px; font-weight: bold; }
        .tele-value { color: var(--text); font-weight: 600; }
        .highlight { color: var(--accent); }
        .tele-toggle-container { display: flex; justify-content: space-between; align-items: center; grid-column: span 2; }

        .check-list { display: flex; flex-direction: column; gap: 8px; font-size: 0.9rem; font-weight: 600; }
        .check-item { display: flex; justify-content: space-between; align-items: center; background: var(--btn-bg); padding: 8px 12px; border-radius: 6px; border: 1px solid var(--border); }
        .check-item-detail { font-size: 0.8rem; color: var(--text-muted); font-weight: 500; display: block; margin-top: 2px; font-family: monospace; transition: color 0.1s ease; }
        
        .badge { font-size: 0.75rem; padding: 3px 8px; border-radius: 4px; color: #ffffff; text-transform: uppercase; font-weight: bold; transition: all 0.1s ease; }
        .badge-ok { background: var(--badge-ok); }
        .badge-fail { background: var(--badge-fail); }
        .badge-highlight { background: var(--accent); color: var(--btn-active-text); box-shadow: 0 0 8px var(--accent-shadow); }

        .control-block { text-align: center; }
        .flex-row { display: flex; flex-wrap: wrap; gap: 10px; justify-content: center; }
        .flex-row .btn { flex: 1; min-width: 90px; }
        
        .pill-standby-box { display: flex; justify-content: space-between; align-items: center; background: var(--btn-bg); padding: 12px 20px; border-radius: 8px; border: 1px solid var(--border); max-width: 280px; margin: 0 auto; }

        .terminal-toggle-container { display: flex; justify-content: space-between; align-items: center; margin-bottom: 5px; }
        .terminal-box { 
            display: none; background: var(--term-bg); border: 1px solid var(--border); 
            color: #00ff66; font-family: 'Courier New', Courier, monospace; font-size: 0.8rem; 
            padding: 12px; height: 160px; overflow-y: auto; text-align: left; 
            border-radius: 8px; white-space: pre-wrap; box-shadow: inset 0 2px 8px rgba(0,0,0,0.5); 
        }

        .remote-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; max-width: 240px; margin: 0 auto; }
        .remote-grid .btn { padding: 18px 10px; font-size: 1.2rem; display: flex; align-items: center; justify-content: center; }
        .btn-stop { background: var(--stop-btn); color: #ffffff; font-weight: bold; font-size: 0.9rem !important; }
        .btn-stop:active { background: var(--stop-btn-active); color: #ffffff; box-shadow: 0 0 12px rgba(229,73,73,0.3); }
        .empty-cell { visibility: hidden; }
    </style>
</head>
<body>
    <h1>DESK DROID</h1>
    <div class="subtitle">Pilot Interface Terminal</div>

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
                
                <div class="check-item"><div><span>🤖 ACCELEROMETER</span><span id="lbl-accel-raw" class="check-item-detail">X: 0.00 | Y: 0.00 | Z: 0.00</span></div><span id="chk-accel" class="badge badge-fail">--</span></div>
                <div class="check-item"><div><span>👉 TOUCH SENSOR</span><span id="lbl-touch-raw" class="check-item-detail">State: IDLE</span></div><span id="chk-touch" class="badge badge-ok">READY</span></div>
                <div class="check-item"><div><span>☀️ LIGHT SENSOR (LDR)</span><span id="lbl-light-raw" class="check-item-detail">Raw ADC: 0</span></div><span id="chk-light" class="badge badge-ok">READY</span></div>
                
                <div class="check-item"><span>🔊 BUZZER AUDIO CORE</span><span id="chk-audio" class="badge badge-ok">READY</span></div>
                <div class="check-item"><span>⚙️ L9110S MOTOR DRIVER</span><span id="chk-motors" class="badge badge-ok">READY</span></div>
            </div>
        </div>
    </div>

    <div class="section" style="max-width: 686px; margin: 0 auto 20px auto;">
        <div class="terminal-toggle-container">
            <div>
                <span style="font-weight: 600; font-size: 0.95rem; color: var(--text); display: block;">📟 Live System Terminal Log</span>
                <span style="font-size: 0.75rem; color: var(--text-muted); font-weight: bold;">STREAM SYSTEM EVENTS AND EXCEPTIONS WIRELESSLY</span>
            </div>
            <label class="switch">
                <input type="checkbox" id="terminal-checkbox" onchange="toggleTerminal()">
                <span class="slider"></span>
            </label>
        </div>
        <pre id="terminal-box" class="terminal-box">--- TERMINAL MATRIX STREAM INITIALIZED. WAITING FOR INCOMING PACKETS... ---&#10;</pre>
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
        let terminalInterval = null; 

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

        function toggleTerminal() {
            const isChecked = document.getElementById('terminal-checkbox').checked;
            const termBox = document.getElementById('terminal-box');
            if (isChecked) {
                termBox.style.display = 'block';
                fetchTerminalDelta();
                terminalInterval = setInterval(fetchTerminalDelta, 1000); 
            } else {
                termBox.style.display = 'none';
                clearInterval(terminalInterval);
            }
        }

        function fetchTerminalDelta() {
            fetch('/serial_data')
                .then(response => response.text())
                .then(text => {
                    if (text.length > 0) {
                        const termBox = document.getElementById('terminal-box');
                        termBox.innerText += text;
                        termBox.scrollTop = termBox.scrollHeight; 
                    }
                })
                .catch(err => console.error('Terminal buffer drop:', err));
        }

        // --- NEW REFACTORED: THE SENSOR FAST LANE (Runs every 300ms) ---
        // Dynamically updates local hardware values on the page nearly in real-time
        // without reloading or over-allocating massive string arrays.
        function fetchFastSensorData() {
            fetch('/sensor_data')
                .then(response => response.json())
                .then(data => {
                    try {
                        // 1. Touch Processing
                        const touchBadge = document.getElementById('chk-touch');
                        const touchText = document.getElementById('lbl-touch-raw');
                        if (data.touch_active) {
                            touchBadge.innerText = "ACTIVE";
                            touchBadge.className = "badge badge-highlight";
                            touchText.innerText = "State: TOUCHED 💥";
                            touchText.style.color = "var(--accent)";
                        } else {
                            touchBadge.innerText = "READY";
                            touchBadge.className = "badge badge-ok";
                            touchText.innerText = "State: IDLE";
                            touchText.style.color = "var(--text-muted)";
                        }

                        // 2. Light Sensor (LDR) Processing
                        document.getElementById('lbl-light-raw').innerText = "Raw ADC: " + data.raw_light;

                        // 3. Accelerometer Processing
                        const accelBadge = document.getElementById('chk-accel');
                        if (data.accel_ok) {
                            accelBadge.innerText = "READY";
                            accelBadge.className = "badge badge-ok";
                            document.getElementById('lbl-accel-raw').innerText = 
                                "X: " + data.raw_ax.toFixed(2) + " | Y: " + data.raw_ay.toFixed(2) + " | Z: " + data.raw_az.toFixed(2);
                        } else {
                            accelBadge.innerText = "BYPASSED";
                            accelBadge.className = "badge badge-fail";
                            document.getElementById('lbl-accel-raw').innerText = "X: 0.00 | Y: 0.00 | Z: 0.00 (OFFLINE)";
                        }
                    } catch(e) {}
                })
                .catch(err => console.error('Fast sensor link dropped:', err));
        }

        // --- THE WEATHER SLOW LANE (Runs every 10000ms / 10 seconds) ---
        // Keeps heavy cloud updates running at a relaxed pace to insulate memory spaces.
        function fetchHeavyTelemetry() {
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
                    } catch(e) { console.warn("Cloud metrics card mapping deferred:", e); }

                    try {
                        document.getElementById('val-net-ip').innerText = "IP: " + data.net_ip;
                        const netStatusBadge = document.getElementById('val-net-status');
                        netStatusBadge.innerText = data.net_status;
                        netStatusBadge.className = data.net_status === "ONLINE" ? "badge badge-ok" : "badge badge-fail";
                    } catch(e) { console.warn("Network elements mapping deferred:", e); }

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
                    } catch(e) { console.warn("Standby tracking map warning:", e); }
                })
                .catch(err => console.error('Heavy telemetry loop drop:', err));
        }

        // Run baseline execution sweeps immediately on load
        fetchFastSensorData();
        fetchHeavyTelemetry();
        
        // Multi-tier decoupled timing intervals assignment
        setInterval(fetchFastSensorData, 300);   // 300ms: Live, blazing fast hardware response lane
        setInterval(fetchHeavyTelemetry, 10000); // 10s: Keeps network clean and preserves heap allocations
    </script>
</body>
</html>
)rawliteral";

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
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", HTML_DASHBOARD);
  });

  server.on("/serial_data", HTTP_GET, []() {
    server.send(200, "text/plain", webTerminalLog);
    webTerminalLog = ""; 
  });

  // --- NEW SENSOR DATA FAST ENDPOINT ROUTE ---
  // Serves a lightweight JSON profile consisting only of current physical sensor registers
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
    doc["temp"] = live_temp;
    doc["humidity"] = live_humidity;
    doc["rain_chance"] = live_rain_chance;
    doc["showing_dashboard"] = showing_dashboard;
    doc["robot_sleeping"] = robot_sleeping; 

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

  HTTPClient http; JsonDocument doc; float lat = 0.0, lon = 0.0;
  logTerminal(F("[API-SYSTEM] Initializing IP Geolocation check packet..."));
  http.begin("http://ip-api.com/json/");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString(); DeserializationError error = deserializeJson(doc, payload);
    if (!error) { live_location = doc["city"].as<String>(); lat = doc["lat"].as<float>(); lon = doc["lon"].as<float>(); }
  }
  http.end(); if (lat == 0.0 && lon == 0.0) return;
  doc.clear(); 
  
  logTerminal("[API-SYSTEM] Localizing to coordinate space matrix: Lat=" + String(lat, 2) + ", Lon=" + String(lon, 2));
  String weatherUrl = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat, 4) + "&lon=" + String(lon, 4) + "&appid=" + String(OPENWEATHER_API_KEY) + "&units=metric";
  http.begin(weatherUrl);
  httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString(); DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      live_condition = doc["weather"][0]["main"].as<String>(); live_temp = round(doc["main"]["temp"].as<float>());
      live_humidity = doc["main"]["humidity"].as<int>(); live_rain_chance = doc["clouds"]["all"].as<int>(); live_condition.toUpperCase(); 
      logTerminal("[API-SUCCESS] Weather telemetry cached: " + live_condition + " | Temp: " + String(live_temp) + "C");
    }
  }
  http.end();
}