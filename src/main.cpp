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

// Optimized Multi-Chapter System Power Shutdown Routine
void enterSystemDeepSleep() {
  Serial.println(F("[POWER-MANAGEMENT] Executing multi-stage animated shutdown protocol..."));
  
  // 1. Safe-stop the drive motors completely
  moveDroid(STOP);
  
  // CHAPTER 1: Execute full downward closing animation loop
  roboEyes.setMood(DEFAULT);
  roboEyes.close();
  roboEyes.setPosition(S);
  
  unsigned long anim_start = millis();
  while (millis() - anim_start < 1000) { 
    roboEyes.update();
    delay(15); 
  }
  
  // CHAPTER 2: Lock and hold the fully closed eyes on the screen for a moment
  delay(1000); 
  
  // CHAPTER 3: Wipe eyes to display the custom text splash card panel
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(12, 24);
  display.println(F("ENTERING SLEEP MODE"));
  display.setCursor(30, 40);
  display.println(F("POWERING DOWN..."));
  display.display();
  
  cute.play(S_DISCONNECTION);
  
  delay(2000); 

  display.clearDisplay();
  display.display();

  // Register TOUCH_PIN (GPIO 10) as Wakeup Trigger
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

  // --- TIME-BRACKETED SINGLE INTERACTION SAMPLING ---
  int rawTouchReading = digitalRead(TOUCH_PIN);
  static int lastRawTouchState = LOW;
  static int debouncedTouchState = LOW;
  static unsigned long lastDebounceTime = 0;
  static bool hold_triggered = false;          

  if (rawTouchReading != lastRawTouchState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) >= DEBOUNCE_DELAY_MS) {
    if (rawTouchReading != debouncedTouchState) {
      debouncedTouchState = rawTouchReading;

      Serial.print(F("[TOUCH-DEBUG] Stable Debounced State Changed To: "));
      Serial.println(debouncedTouchState == HIGH ? F("HIGH (PRESSED)") : F("LOW (RELEASED)"));

      if (debouncedTouchState == HIGH) {
        touch_start_time = millis();
        hold_triggered = false;
      } else {
        if (!hold_triggered && !showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
          unsigned long press_duration = millis() - touch_start_time;
          
          Serial.print(F("[TOUCH-DEBUG] Release Event Logged. Total Duration: "));
          Serial.print(press_duration);
          Serial.println(F(" ms"));

          if (press_duration >= TRIGGER_HAPPY_HOLD_MS && press_duration < TRIGGER_ANGRY_HOLD_MS) {
            Serial.println(F("[TOUCH-DEBUG] Context: Short Tap. Executing HAPPY emotion & shuffle."));
            roboEyes.setMood(HAPPY);
            roboEyes.anim_laugh();
            playSoundAsync(S_SUPER_HAPPY); 
            moveDroid(FORWARD);
            motor_stop_time = millis() + 150;
            motor_running = true;
          } 
          else if (press_duration >= TRIGGER_ANGRY_HOLD_MS && press_duration < TRIGGER_DASHBOARD_MS) {
            Serial.println(F("[TOUCH-DEBUG] Context: Medium Hold. Executing ANGRY emotion & retreat."));
            roboEyes.setMood(ANGRY);
            playSoundAsync(S_OHOOH); 
            moveDroid(BACKWARD);
            motor_stop_time = millis() + 300;
            motor_running = true;
          }
        }
      }
    }
  }
  lastRawTouchState = rawTouchReading;

  // Real-time Dashboard Threshold Intercept
  if (debouncedTouchState == HIGH && !hold_triggered && !showing_dashboard && !showing_pre_sweat) {
    unsigned long current_hold_duration = millis() - touch_start_time;

    if (current_hold_duration >= TRIGGER_DASHBOARD_MS && current_hold_duration < TRIGGER_SLEEP_MS) {
      Serial.println(F("[TOUCH-DEBUG] Context: Long Hold Target Met! Route Evaluation..."));
      playSoundAsync(S_BUTTON_PUSHED); 
      
      Serial.print(F("[SWEAT-DEBUG] Current Live Temp: ")); Serial.print(live_temp);
      Serial.print(F(" C | Sweat Threshold: ")); Serial.print(SWEAT_TEMP_THRESHOLD_C); Serial.println(F(" C"));

      if (live_temp >= SWEAT_TEMP_THRESHOLD_C) {
        Serial.println(F("[EMOTION-DEBUG] High Temp Detected! Running pre-dashboard sweat check..."));
        showing_pre_sweat = true;
        sweat_timeout = millis() + SWEAT_ANIM_DURATION_MS;
        roboEyes.setMood(TIRED); 
        roboEyes.setSweat(ON);
      } else {
        showing_dashboard = true;
        dashboard_timeout = millis() + DASHBOARD_DURATION_MS; 
      }
      hold_triggered = true; 
    }
  }

  // FIXED: Real-time Shutdown Intercept (UI state guards completely removed)
  if (debouncedTouchState == HIGH) {
    if (millis() - touch_start_time >= TRIGGER_SLEEP_MS) {
      enterSystemDeepSleep(); 
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