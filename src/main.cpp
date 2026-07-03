#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

#include <CuteBuzzerSounds.h>
#undef debug // Clears the macro collision before WiFiManager loads

#include <WiFiManager.h> // Includes WiFi.h, WebServer.h, DNSServer.h, and Preferences.h internally
#include <HTTPClient.h>  // Native ESP32 web network client stack
#include <ArduinoJson.h> // Structured JSON parsing library
#include <esp_sleep.h>   // Native ESP32 Deep Sleep Core Controls

// Modular includes from the include/ folder
#include "config.h"
#include "display_ui.h"
#include "motors.h"

// Instantiate Global Hardware Objects
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire1, OLED_RESET);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

#undef DEFAULT // Wipes Arduino core's DEFAULT macro so RoboEyes can use its own
#include <FluxGarage_RoboEyes.h>
RoboEyes<Adafruit_SH1106G> roboEyes(display); 

// Global Timers
unsigned long env_timer;           // Tracks fixed 1-second sensor updates
unsigned long idle_timer;          // Tracks variable pacing for random movements
unsigned long next_idle_interval;  // Dynamically changes to randomize rest duration
unsigned long motor_stop_time = 0; // Precision stopwatch for active movement duration
unsigned long weather_timer = 0;   // Non-blocking 15-minute background web polling clock

// Gesture & Page Management Timers
unsigned long touch_start_time = 0;
unsigned long dashboard_timeout = 0; // Tracks visibility length via config macro
unsigned long sweat_timeout = 0;     // Non-blocking transitional sweat animation clock

// Global States
float last_x = 0, last_y = 0, last_z = 0;
bool motor_running = false;
bool showing_dashboard = false;    // Controls screen routing states
bool showing_pre_sweat = false;    // Handles looking ahead at hot weather alert states
bool showing_post_sweat = false;   // Handles post-menu cooldown states

// Live API Dynamic Data Registries
String live_location = "UNKNOWN";
String live_condition = "WAITING...";
int live_temp = 0;
int live_humidity = 0;
int live_rain_chance = 0;

// Dynamic Fuel Gauge Variable
int live_battery_percentage = 100;

// FreeRTOS Asynchronous Audio Core Elements
QueueHandle_t buzzerQueue; 

void buzzerCore0Task(void *pvParameters) {
  int soundId;
  for (;;) {
    if (xQueueReceive(buzzerQueue, &soundId, portMAX_DELAY) == pdPASS) {
      cute.play(soundId); 
    }
  }
}

void playSoundAsync(int soundId) {
  xQueueSend(buzzerQueue, &soundId, 0);
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println(F("Entered Configuration Portal Mode!"));
  Serial.print(F("AP IP Address: "));
  Serial.println(WiFi.softAPIP());
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
  float lat = 0.0;
  float lon = 0.0;

  Serial.println(F("[API-DEBUG] Starting Stage 1: Requesting IP Geolocation..."));
  http.begin("http://ip-api.com/json/");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      live_location = doc["city"].as<String>();
      lat = doc["lat"].as<float>();
      lon = doc["lon"].as<float>();
      Serial.print(F("[API-DEBUG] Geolocation Success! City: ")); Serial.println(live_location);
    }
  }
  http.end(); 

  if (lat == 0.0 && lon == 0.0) return;

  Serial.println(F("[API-DEBUG] Starting Stage 2: Requesting Meteorological Telemetry..."));
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

void enterSystemDeepSleep() {
  Serial.println(F("[POWER-MANAGEMENT] Executing multi-stage designed shutdown protocol..."));
  
  moveDroid(STOP);
  
  display.clearDisplay();
  display.display();
  delay(100); 

  roboEyes.setMood(DEFAULT);
  roboEyes.close();
  roboEyes.setPosition(S);
  
  unsigned long anim_start = millis();
  while (millis() - anim_start < 1000) { 
    roboEyes.update();
    delay(15); 
  }
  
  delay(1000); 
  
  playSoundAsync(S_DISCONNECTION);
  
  for (int frame = 0; frame <= 100; frame += 5) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    display.setCursor(7, 44);
    display.println(F("ENTERING SLEEP MODE"));

    display.setCursor(16, 54);
    display.println(F("POWERING DOWN..."));

    display.drawFastHLine(2, 2, 10, SH110X_WHITE);
    display.drawFastVLine(2, 2, 10, SH110X_WHITE);
    display.drawFastHLine(115, 2, 10, SH110X_WHITE);
    display.drawFastVLine(125, 2, 10, SH110X_WHITE);
    display.drawFastHLine(2, 61, 10, SH110X_WHITE);
    display.drawFastVLine(2, 51, 10, SH110X_WHITE);
    display.drawFastHLine(115, 61, 10, SH110X_WHITE);
    display.drawFastVLine(125, 51, 10, SH110X_WHITE);

    display.drawCircle(64, 22, 15, SH110X_WHITE);
    display.drawRect(59, 15, 10, 15, SH110X_WHITE); 
    display.drawFastHLine(62, 13, 4, SH110X_WHITE);  

    int powerFillHeight = map(100 - frame, 0, 100, 0, 11);
    if (powerFillHeight > 0) {
      display.fillRect(61, 28 - powerFillHeight, 6, powerFillHeight, SH110X_WHITE);
    }

    display.display();
    delay(100); 
  }

  display.clearDisplay();
  display.display();

  esp_sleep_enable_ext1_wakeup(1ULL << TOUCH_PIN, ESP_EXT1_WAKEUP_ANY_HIGH);
  
  Serial.println(F("[POWER-MANAGEMENT] Droid entering deep sleep state now. Goodnight!"));
  delay(100);
  
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(2000); 

  Serial.println(F("=== DROID OS BOOTING ==="));

  // 1. Initialize Display
  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(OLED_I2C_ADDRESS, true);

  // Splash Checklist Phase 1
  drawChecklist(" ", " ", " ", " ", " ", "INITIALIZING...");
  delay(800); 

  // 2. Initialize Accelerometer
  Wire.begin(ACCEL_SDA, ACCEL_SCL);
  if(!accel.begin()) {
    drawChecklist("X", " ", " ", " ", " ", "ACCELEROMETER ERROR!");
    Serial.println(F("Ooops, no ADXL345 detected ... Check wiring!"));
    while(1); 
  }
  accel.setRange(ADXL345_RANGE_4_G);
  drawChecklist("*", " ", " ", " ", " ", "   TESTING...");
  delay(500);

  // 3. Initialize Touch GPIO Pin
  pinMode(TOUCH_PIN, INPUT);
  drawChecklist("*", "*", " ", " ", " ", "   TESTING...");
  delay(500);

  // 4. Initialize LDR Pin
  pinMode(LDR_PIN, INPUT);
  drawChecklist("*", "*", "*", " ", " ", "   TESTING...");
  delay(500);

  // 5. Initialize Audio Engine
  cute.init(BUZZER_PIN);
  drawChecklist("*", "*", "*", "*", " ", "   TESTING...");
  delay(500);

  buzzerQueue = xQueueCreate(2, sizeof(int));

  xTaskCreatePinnedToCore(
    buzzerCore0Task, "BuzzerTask", 3072, NULL, 1, NULL, 0
  );

  // 6. Initialize Motors
  initMotors();
  drawChecklist("*", "*", "*", "*", "*", "       READY!");
  cute.play(S_CONNECTION); 
  delay(1000); 

  // --- PART 2 - AUTOMATIC WIFI CAPTIVE PORTAL ENGINE ---
  drawWifiScreen("NETWORK CHECK", "SEARCHING...", "Checking memory for\nsaved network connections");
  
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC); 
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);

  bool wifi_success = wm.autoConnect(WIFI_AP_NAME);

  if (wifi_success) {
    String ipMsg = "IP: " + WiFi.localIP().toString();
    drawWifiScreen("NETWORK CHECK", "ONLINE [OK]", ipMsg.c_str());
    live_location = "LOCALIZING..."; 
    fetchLocationAndWeather(); 
  } else {
    drawWifiScreen("NETWORK CHECK", "OFFLINE MODE", "No connection found.\nBypassing network boot.");
    live_location = "OFFLINE"; 
    live_condition = "N/A";
  }
  delay(2500); 

  // 7. Display RoboEyes Custom Splash Card
  drawSplashArt();
  delay(3000); 

  for(int i = 0; i < 3; i++) {
    display.clearDisplay(); display.display(); delay(40);
    drawSplashArt(); delay(60);
  }
  playSoundAsync(S_HAPPY_SHORT);

  // Transition Control to RoboEyes Engine
  roboEyes.begin(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  // Initialize Timers
  env_timer = millis();
  idle_timer = millis();
  weather_timer = millis(); 
  next_idle_interval = random(MOTOR_MOVE_INTERVAL_MIN, MOTOR_MOVE_INTERVAL_MAX); 
  randomSeed(analogRead(LDR_PIN)); 
}

void loop() {
  // --- Dynamic Screen Page Router ---
  if (showing_dashboard) {
    if (millis() >= dashboard_timeout) {
      showing_dashboard = false; 
      
      if (live_temp >= SWEAT_TEMP_THRESHOLD_C) {
        Serial.println(F("[EMOTION-DEBUG] Dashboard closed. Entering Post-Sweat mode..."));
        showing_post_sweat = true;
        sweat_timeout = millis() + SWEAT_ANIM_DURATION_MS;
        roboEyes.setMood(TIRED);
        roboEyes.setSweat(ON);
      } else {
        roboEyes.setMood(DEFAULT);
        roboEyes.setPosition(DEFAULT);
      }
    } else {
      String ipStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "OFFLINE";
      drawDashboard(live_condition.c_str(), live_temp, live_humidity, live_rain_chance, ipStr.c_str(), live_battery_percentage, live_location.c_str());
    }
  } 
  else if (showing_pre_sweat) {
    if (millis() >= sweat_timeout) {
      showing_pre_sweat = false;
      roboEyes.setSweat(OFF); 
      showing_dashboard = true;
      dashboard_timeout = millis() + DASHBOARD_DURATION_MS;
    } else {
      roboEyes.update(); 
    }
  } 
  else if (showing_post_sweat) {
    if (millis() >= sweat_timeout) {
      showing_post_sweat = false;
      roboEyes.setSweat(OFF);
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
    } else {
      roboEyes.update();
    }
  } 
  else {
    roboEyes.update(); 
  }

  // --- Real-Time Motor Cutoff ---
  if (motor_running && millis() >= motor_stop_time) {
    moveDroid(STOP);
    motor_running = false;
  }

  // --- Background Web Updating (Every 15 Minutes) ---
  if (WiFi.status() == WL_CONNECTED && (millis() - weather_timer >= WEATHER_UPDATE_INTERVAL)) {
    weather_timer = millis(); 
    fetchLocationAndWeather(); 
  }

  // --- NOISE-BRIDGED ASYNCHRONOUS TOUCH TRACKING ENGINE ---
  int rawTouchReading = digitalRead(TOUCH_PIN);
  static bool is_pressing = false;
  static unsigned long true_touch_start = 0;
  static unsigned long last_active_high_time = 0;
  static bool dashboard_triggered_this_press = false;

  if (rawTouchReading == HIGH) {
    if (!is_pressing) {
      is_pressing = true;
      true_touch_start = millis();
      dashboard_triggered_this_press = false;
      Serial.println(F("[TOUCH-DEBUG] Touch Connection Formed."));
    }
    last_active_high_time = millis(); // Continuously refresh high water mark while finger remains down
  }

  // Handle Release Conditions based on context gates
  if (is_pressing) {
    unsigned long current_hold_duration = millis() - true_touch_start;

    // Gate A: Dashboard intercept threshold checker (Fires at 5 seconds)
    if (!dashboard_triggered_this_press && current_hold_duration >= TRIGGER_DASHBOARD_MS && current_hold_duration < TRIGGER_SLEEP_MS) {
      dashboard_triggered_this_press = true;
      playSoundAsync(S_BUTTON_PUSHED); 
      
      Serial.print(F("[SWEAT-DEBUG] Current Live Temp: ")); Serial.print(live_temp);
      Serial.print(F(" C | Sweat Threshold: ")); Serial.print(SWEAT_TEMP_THRESHOLD_C); Serial.println(F(" C"));

      if (live_temp >= SWEAT_TEMP_THRESHOLD_C) {
        showing_pre_sweat = true;
        sweat_timeout = millis() + SWEAT_ANIM_DURATION_MS;
        roboEyes.setMood(TIRED); 
        roboEyes.setSweat(ON);
      } else {
        showing_dashboard = true;
        dashboard_timeout = millis() + DASHBOARD_DURATION_MS; 
      }
    }

    // Gate B: Master Deep Sleep threshold intercept checker (Fires at 10 seconds)
    if (current_hold_duration >= TRIGGER_SLEEP_MS) {
      is_pressing = false; // Disengage active looping limits instantly
      enterSystemDeepSleep(); 
    }

    // Gate C: Asynchronous Noise-Immune Release Tracker
    if (!dashboard_triggered_this_press) {
      // Before the screen refresh occurs, maintain hyper-responsive click timing
      if (rawTouchReading == LOW && (millis() - last_active_high_time > DEBOUNCE_DELAY_MS)) {
        is_pressing = false;
        unsigned long total_hold = last_active_high_time - true_touch_start;
        
        if (total_hold >= TRIGGER_HAPPY_HOLD_MS && total_hold < TRIGGER_ANGRY_HOLD_MS) {
          if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
            roboEyes.setMood(HAPPY); roboEyes.anim_laugh(); playSoundAsync(S_SUPER_HAPPY); 
            moveDroid(FORWARD); motor_stop_time = millis() + 150; motor_running = true;
          }
        }
        else if (total_hold >= TRIGGER_ANGRY_HOLD_MS && total_hold < TRIGGER_DASHBOARD_MS) {
          if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
            roboEyes.setMood(ANGRY); playSoundAsync(S_OHOOH); 
            moveDroid(BACKWARD); motor_stop_time = millis() + 300; motor_running = true;
          }
        }
      }
    } 
    else {
      // ADVANCED Once the screen updates, allow up to 1.5 seconds of total EMI noise dropping.
      // It will only cancel the sleep hold if the finger is completely away from the pad.
      if (rawTouchReading == LOW && (millis() - last_active_high_time > 1500)) {
        Serial.println(F("[TOUCH-DEBUG] Valid Lift Detected. Resetting Hold Engine State."));
        is_pressing = false;
      }
    }
  }

  // --- Real-time Accelerometer Sampling ---
  sensors_event_t event; 
  accel.getEvent(&event);

  float delta_x = abs(event.acceleration.x - last_x);
  float delta_y = abs(event.acceleration.y - last_y); 
  float delta_z = abs(event.acceleration.z - last_z);

  Serial.print("X: "); Serial.print(delta_x);
  Serial.print(" Y: "); Serial.print(delta_y);
  Serial.print(" Z: "); Serial.println(delta_z);

  last_x = event.acceleration.x;
  last_y = event.acceleration.y;
  last_z = event.acceleration.z;

  if (delta_x > SHAKE_THRESHOLD || delta_y > SHAKE_THRESHOLD || delta_z > SHAKE_THRESHOLD) {
    if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
      roboEyes.setMood(TIRED);
      roboEyes.anim_confused();
    }
    playSoundAsync(S_SURPRISE); 
  }

  // --- TIMER 1: Fixed Environmental Sampling & FUEL GAUGE MATH (Every 1000ms) ---
  if (millis() - env_timer >= 1000) {
    env_timer = millis(); 
    
    int rawADC = analogRead(FREE_BATTERY_PIN);
    float pinVoltage = (rawADC / 4095.0) * ADC_VREF_VOLTAGE;
    float calculatedBatteryVoltage = pinVoltage * 2.0; 

    int calculatedPct = map(round(calculatedBatteryVoltage * 100), 3500, 4200, 0, 100);
    live_battery_percentage = constrain(calculatedPct, 0, 100);

    Serial.print(F("Battery Voltage: ")); Serial.print(calculatedBatteryVoltage);
    Serial.print(F("V | Percentage: ")); Serial.print(live_battery_percentage); Serial.println(F("%"));

    int light_level = analogRead(LDR_PIN);
    Serial.print("Light Level: "); Serial.println(light_level);
    
    if (light_level < LDR_THRESHOLD) {
      if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
        roboEyes.close();
        roboEyes.setPosition(S);
      }
      if (motor_running) {
        moveDroid(STOP);
        motor_running = false;
      }
    } else {
      if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
        roboEyes.open();
      }
    }
  }

  // --- TIMER 2: Randomized Idle Movement (Occasional, Variable Intervals) ---
  if (millis() - idle_timer >= next_idle_interval) {
    idle_timer = millis(); 
    
    if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT); 
    }

    int current_light = analogRead(LDR_PIN);
    if (current_light >= LDR_THRESHOLD && !showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
      if (random(100) < 40) {
        DroidMovement options[] = {FORWARD, BACKWARD, TURN_LEFT, TURN_RIGHT};
        DroidMovement chosenMove = options[random(0, 4)];
        moveDroid(chosenMove);
        motor_stop_time = millis() + random(MOTOR_MOVE_DURATION_MIN, MOTOR_MOVE_DURATION_MAX); 
        motor_running = true;
      }
    }
    next_idle_interval = random(MOTOR_MOVE_INTERVAL_MIN, MOTOR_MOVE_INTERVAL_MAX); 
  }
}