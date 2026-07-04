#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

#include <CuteBuzzerSounds.h>
#undef debug 

#include <WiFiManager.h> 
#include <ArduinoJson.h> 

// Modular System Interfaces
#include "config.h"
#include "display_ui.h"
#include "motors.h"
#include "audio_system.h"
#include "network_system.h"
#include "power_system.h"

// Instantiate Global Hardware Objects
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire1, OLED_RESET);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

#undef DEFAULT 
#include <FluxGarage_RoboEyes.h>
RoboEyes<Adafruit_SH1106G> roboEyes(display); 

// Global Timers
unsigned long env_timer;           
unsigned long idle_timer;          
unsigned long next_idle_interval;  
unsigned long motor_stop_time = 0; 
unsigned long weather_timer = 0;   
unsigned long accel_timer = 0;

// Gesture & Page Management Timers
unsigned long touch_start_time = 0;
unsigned long dashboard_timeout = 0; 
unsigned long sweat_timeout = 0;     

// Global States
float last_x = 0, last_y = 0, last_z = 0;
bool motor_running = false;
bool showing_dashboard = false;    
bool showing_pre_sweat = false;    
bool showing_post_sweat = false;   

// Live API Dynamic Data Registries
String live_location = "UNKNOWN";
String live_condition = "WAITING...";
int live_temp = 0;
int live_humidity = 0;
int live_rain_chance = 0;

// Dynamic Fuel Gauge Variable
int live_battery_percentage = 100;

// LINKER BRIDGE WRAPPER: Safely isolates the RoboEyes class call instance away from sub-modules
void playEyeShutdownAnimation() {
  roboEyes.setMood(DEFAULT);
  roboEyes.close();
  roboEyes.setPosition(S);
  
  unsigned long anim_start = millis();
  while (millis() - anim_start < 1000) { 
    roboEyes.update();
    delay(15); 
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println(F("=== DROID OS BOOTING ==="));

  // 1. Initialize Displays & Sensors
  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(OLED_I2C_ADDRESS, true);
  drawChecklist(" ", " ", " ", " ", " ", "INITIALIZING...");
  delay(800); 

  Wire.begin(ACCEL_SDA, ACCEL_SCL);
  if(!accel.begin()) {
    drawChecklist("X", " ", " ", " ", " ", "ACCELEROMETER ERROR!");
    while(1); 
  }
  accel.setRange(ADXL345_RANGE_4_G);
  drawChecklist("*", " ", " ", " ", " ", "   TESTING..."); delay(500);

  pinMode(TOUCH_PIN, INPUT);
  drawChecklist("*", "*", " ", " ", " ", "   TESTING..."); delay(500);

  pinMode(LDR_PIN, INPUT);
  drawChecklist("*", "*", "*", " ", " ", "   TESTING..."); delay(500);

  cute.init(BUZZER_PIN);
  drawChecklist("*", "*", "*", "*", " ", "   TESTING..."); delay(500);

  // 2. Initialize Subsystems
  initAudioSystem();
  initMotors();
  drawChecklist("*", "*", "*", "*", "*", "       READY!");
  cute.play(S_CONNECTION); delay(1000); 

  // 3. Captive Portal Network Routines
  drawWifiScreen("NETWORK CHECK", "SEARCHING...", "Checking memory for\nsaved network connections");
  
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SEC); 
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);

  if (wm.autoConnect(WIFI_AP_NAME)) {
    String ipMsg = "IP: " + WiFi.localIP().toString();
    drawWifiScreen("NETWORK CHECK", "ONLINE [OK]", ipMsg.c_str());
    live_location = "LOCALIZING..."; 
    fetchLocationAndWeather(); 
  } else {
    drawWifiScreen("NETWORK CHECK", "OFFLINE MODE", "No connection found.\nBypassing network boot.");
    live_location = "OFFLINE"; live_condition = "N/A";
  }
  delay(2500); 

  drawSplashArt(); delay(3000); 
  for(int i = 0; i < 3; i++) {
    display.clearDisplay(); display.display(); delay(40);
    drawSplashArt(); delay(60);
  }
  playSoundAsync(S_HAPPY_SHORT);

  // 4. Anchor Run Configurations
  roboEyes.begin(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  env_timer = millis(); idle_timer = millis(); weather_timer = millis(); accel_timer = millis();
  next_idle_interval = random(MOTOR_MOVE_INTERVAL_MIN, MOTOR_MOVE_INTERVAL_MAX); 
  randomSeed(analogRead(LDR_PIN)); 
}

void loop() {
  // --- Page Layout Router Engine ---
  if (showing_dashboard) {
    if (millis() >= dashboard_timeout) {
      showing_dashboard = false; 
      if (live_temp >= SWEAT_TEMP_THRESHOLD_C) {
        showing_post_sweat = true;
        sweat_timeout = millis() + SWEAT_ANIM_DURATION_MS;
        roboEyes.setMood(TIRED); roboEyes.setSweat(ON);
      } else {
        roboEyes.setMood(DEFAULT); roboEyes.setPosition(DEFAULT);
      }
    } else {
      String ipStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "OFFLINE";
      drawDashboard(live_condition.c_str(), live_temp, live_humidity, live_rain_chance, ipStr.c_str(), live_battery_percentage, live_location.c_str());
    }
  } 
  else if (showing_pre_sweat) {
    if (millis() >= sweat_timeout) {
      showing_pre_sweat = false; roboEyes.setSweat(OFF); 
      showing_dashboard = true; dashboard_timeout = millis() + DASHBOARD_DURATION_MS;
    } else { roboEyes.update(); }
  } 
  else if (showing_post_sweat) {
    if (millis() >= sweat_timeout) {
      showing_post_sweat = false; roboEyes.setSweat(OFF);
      roboEyes.setMood(DEFAULT); roboEyes.setPosition(DEFAULT);
    } else { roboEyes.update(); }
  } 
  else { roboEyes.update(); }

  // --- Real-Time Safety Intercept Flags ---
  if (motor_running && millis() >= motor_stop_time) {
    moveDroid(STOP); motor_running = false;
  }

  if (WiFi.status() == WL_CONNECTED && (millis() - weather_timer >= WEATHER_UPDATE_INTERVAL)) {
    weather_timer = millis(); fetchLocationAndWeather(); 
  }

  // --- NOISE-BRIDGED ASYNCHRONOUS TOUCH SAMPLING ENGINE ---
  int rawTouchReading = digitalRead(TOUCH_PIN);
  static bool is_pressing = false;
  static unsigned long true_touch_start = 0;
  static unsigned long last_active_high_time = 0;
  static bool dashboard_triggered_this_press = false;

  if (rawTouchReading == HIGH) {
    if (!is_pressing) {
      is_pressing = true; true_touch_start = millis(); dashboard_triggered_this_press = false;
    }
    last_active_high_time = millis(); 
  }

  if (is_pressing) {
    unsigned long current_hold_duration = millis() - true_touch_start;

    if (!dashboard_triggered_this_press && current_hold_duration >= TRIGGER_DASHBOARD_MS && current_hold_duration < TRIGGER_SLEEP_MS) {
      dashboard_triggered_this_press = true;
      playSoundAsync(S_BUTTON_PUSHED); 
      if (live_temp >= SWEAT_TEMP_THRESHOLD_C) {
        showing_pre_sweat = true; sweat_timeout = millis() + SWEAT_ANIM_DURATION_MS;
        roboEyes.setMood(TIRED); roboEyes.setSweat(ON);
      } else {
        showing_dashboard = true; dashboard_timeout = millis() + DASHBOARD_DURATION_MS; 
      }
    }

    if (current_hold_duration >= TRIGGER_SLEEP_MS) {
      is_pressing = false; enterSystemDeepSleep(); 
    }

    if (!dashboard_triggered_this_press) {
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
    } else {
      if (rawTouchReading == LOW && (millis() - last_active_high_time > 1500)) { is_pressing = false; }
    }
  }

  // --- REFACTORED: Throttled Accelerometer Sampling Loop (Runs at 40Hz / 25ms) ---
  if (millis() - accel_timer >= 25) {
    accel_timer = millis();
    sensors_event_t event; 
    accel.getEvent(&event);

    float delta_x = abs(event.acceleration.x - last_x);
    float delta_y = abs(event.acceleration.y - last_y); 
    float delta_z = abs(event.acceleration.z - last_z);

    last_x = event.acceleration.x; 
    last_y = event.acceleration.y; 
    last_z = event.acceleration.z;

    if (delta_x > SHAKE_THRESHOLD || delta_y > SHAKE_THRESHOLD || delta_z > SHAKE_THRESHOLD) {
      if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
        roboEyes.setMood(TIRED); roboEyes.anim_confused();
      }
      playSoundAsync(S_SURPRISE); 
    }
  }

  // --- TIMER 1: Environmental Sampling & Battery Updates (Every 1000ms) ---
  if (millis() - env_timer >= 1000) {
    env_timer = millis(); 
    updateBatteryTelemetry();

    int light_level = analogRead(LDR_PIN);
    if (light_level < LDR_THRESHOLD) {
      if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
        roboEyes.close(); roboEyes.setPosition(S);
      }
      if (motor_running) { moveDroid(STOP); motor_running = false; }
    } else {
      if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) { roboEyes.open(); }
    }
  }

  // --- TIMER 2: Randomized Idle Movement ---
  if (millis() - idle_timer >= next_idle_interval) {
    idle_timer = millis(); 
    if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
      roboEyes.setMood(DEFAULT); roboEyes.setPosition(DEFAULT); 
    }

    int current_light = analogRead(LDR_PIN);
    if (current_light >= LDR_THRESHOLD && !showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
      if (random(100) < 40) {
        DroidMovement options[] = {FORWARD, BACKWARD, TURN_LEFT, TURN_RIGHT};
        moveDroid(options[random(0, 4)]);
        motor_stop_time = millis() + random(MOTOR_MOVE_DURATION_MIN, MOTOR_MOVE_DURATION_MAX); 
        motor_running = true;
      }
    }
    next_idle_interval = random(MOTOR_MOVE_INTERVAL_MIN, MOTOR_MOVE_INTERVAL_MAX); 
  }
}