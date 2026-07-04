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

// Global Web UI Intercept System Flags
bool web_control_active = false;
unsigned long web_mode_timeout = 0;
bool robot_sleeping = false;          

// Global States
float last_x = 0, last_y = 0, last_z = 0;
bool motor_running = false;
bool showing_dashboard = false;    
bool showing_pre_sweat = false;    
bool showing_post_sweat = false;   
bool accel_available = false;

// Live API Dynamic Data Registries
String live_location = "UNKNOWN";
String live_condition = "WAITING...";
int live_temp = 0;
int live_humidity = 0;
int live_rain_chance = 0;

// Dynamic Fuel Gauge Variable
int live_battery_percentage = 100;

// Raw Sensor Hardware Data Registries ---
int live_light_level = 0;
float live_accel_x = 0.0;
float live_accel_y = 0.0;
float live_accel_z = 0.0;
bool live_touch_active = false;
bool live_base_connected = false;

// --- LINKER INTEGRATION HOOK ---
// Connects the dual-channel USB/Web serial logger from network_system.cpp
extern void logTerminal(String msg);

void playEyeShutdownAnimation() {
  roboEyes.setMood(DEFAULT); roboEyes.close(); roboEyes.setPosition(S);
  unsigned long anim_start = millis();
  while (millis() - anim_start < 1000) { roboEyes.update(); delay(15); }
}

// Emotion matrix interface bridge execution vector for remote HTTP client requests
void forceWebEmotion(String type) {
  if (type == "happy") { 
    showing_pre_sweat = false; showing_post_sweat = false; roboEyes.setSweat(OFF);
    roboEyes.setMood(HAPPY); roboEyes.anim_laugh(); playSoundAsync(S_SUPER_HAPPY); 
  }
  else if (type == "angry") { 
    showing_pre_sweat = false; showing_post_sweat = false; roboEyes.setSweat(OFF);
    roboEyes.setMood(ANGRY); playSoundAsync(S_OHOOH); 
  }
  else if (type == "confused") { 
    showing_pre_sweat = false; showing_post_sweat = false; roboEyes.setSweat(OFF);
    roboEyes.setMood(TIRED); roboEyes.anim_confused(); playSoundAsync(S_SURPRISE); 
  }
  else if (type == "sweat") { 
    // REFACTORED SAFETY: Route web sweat straight to post-sweat state layout.
    // This runs the full animation sequence, but bypasses opening the OLED dashboard!
    showing_pre_sweat = false;
    showing_post_sweat = true; 
    sweat_timeout = millis() + SWEAT_ANIM_DURATION_MS; 
    roboEyes.setMood(TIRED); 
    roboEyes.setSweat(ON); 
    playSoundAsync(S_CUDDLY);
  }
  else { 
    showing_pre_sweat = false; showing_post_sweat = false; roboEyes.setSweat(OFF);
    roboEyes.setMood(DEFAULT); roboEyes.setPosition(DEFAULT); 
  }
}

void setup() {
  // Divert early boot tracking messages through the unified logging portal
  logTerminal(F("=== DROID OS BOOTING ==="));
  delay(2000); 

  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(OLED_I2C_ADDRESS, true);
  drawChecklist(" ", " ", " ", " ", " ", "INITIALIZING...");
  delay(800); 

  Wire.begin(ACCEL_SDA, ACCEL_SCL);
  if(!accel.begin()) {
    drawChecklist("X", " ", " ", " ", " ", "ACCEL BYPASSED");
    accel_available = false; delay(1500);
  } else {
    accel.setRange(ADXL345_RANGE_4_G);
    drawChecklist("*", " ", " ", " ", " ", "   TESTING...");
    accel_available = true; delay(500);
  }

  pinMode(TOUCH_PIN, INPUT);
  drawChecklist("*", "*", " ", " ", " ", "   TESTING..."); delay(500);
  pinMode(LDR_PIN, INPUT);
  drawChecklist("*", "*", "*", " ", " ", "   TESTING..."); delay(500);
  cute.init(BUZZER_PIN);
  drawChecklist("*", "*", "*", "*", " ", "   TESTING..."); delay(500);

  initAudioSystem();
  initMotors();
  drawChecklist("*", "*", "*", "*", "*", "       READY!");
  cute.play(S_CONNECTION); delay(1000); 

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
    initWebServer();
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

  roboEyes.begin(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  env_timer = millis(); idle_timer = millis(); weather_timer = millis(); accel_timer = millis();
  next_idle_interval = random(MOTOR_MOVE_INTERVAL_MIN, MOTOR_MOVE_INTERVAL_MAX); 
  randomSeed(analogRead(LDR_PIN)); 
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    handleWebClient();
    if (web_control_active && (millis() >= web_mode_timeout)) {
      web_control_active = false;
      logTerminal(F("[WEB-SERVER] Web timeout expired. Returning to autonomous navigation."));
    }
  }

  // --- Page Layout Router Engine ---
  if (robot_sleeping) {
    roboEyes.close();
    roboEyes.update();
  } 
  else if (showing_dashboard) {
    if (millis() >= dashboard_timeout) {
      showing_dashboard = false; 
      if (live_temp >= SWEAT_TEMP_THRESHOLD_C) {
        showing_post_sweat = true; sweat_timeout = millis() + SWEAT_ANIM_DURATION_MS;
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
      showing_pre_sweat = false; roboEyes.setSweat(OFF); playSoundAsync(S_CUDDLY);
      showing_dashboard = true; dashboard_timeout = millis() + DASHBOARD_DURATION_MS;
    } else { roboEyes.update(); }
  } 
  else if (showing_post_sweat) {
    if (millis() >= sweat_timeout) {
      showing_post_sweat = false; roboEyes.setSweat(OFF);
      roboEyes.setMood(DEFAULT); roboEyes.setPosition(DEFAULT); playSoundAsync(S_CUDDLY);
    } else { roboEyes.update(); }
  } 
  else { roboEyes.update(); }

  // --- Real-Time Safety Intercept Flags ---
  if (!web_control_active && motor_running && millis() >= motor_stop_time) {
    moveDroid(STOP); motor_running = false;
  }

  if (WiFi.status() == WL_CONNECTED && (millis() - weather_timer >= WEATHER_UPDATE_INTERVAL)) {
    weather_timer = millis(); fetchLocationAndWeather(); 
  }

  // --- NOISE-BRIDGED ASYNCHRONOUS TOUCH SAMPLING ENGINE ---
  int rawTouchReading = digitalRead(TOUCH_PIN);
  live_touch_active = (rawTouchReading == HIGH);
  static bool is_pressing = false;
  static unsigned long true_touch_start = 0;
  static unsigned long last_active_high_time = 0;
  static bool dashboard_triggered_this_press = false;

  if (rawTouchReading == HIGH) {
    if (!is_pressing) {
      is_pressing = true; true_touch_start = millis(); dashboard_triggered_this_press = false;
      
      // LOG MODIFICATION: Catch physical finger interactions wirelessly
      if (robot_sleeping) { 
        robot_sleeping = false; 
        roboEyes.open(); 
        playSoundAsync(S_CONNECTION); 
        logTerminal(F("[HARDWARE] Physical chassis touched! Waking up..."));
      }
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

  // --- Throttled Accelerometer Sampling Loop ---
  if (!robot_sleeping && accel_available && (millis() - accel_timer >= 25)) { 
    accel_timer = millis();
    sensors_event_t event; accel.getEvent(&event);

    // Update web diagnostic registries
    live_accel_x = event.acceleration.x;
    live_accel_y = event.acceleration.y;
    live_accel_z = event.acceleration.z;

    float delta_x = abs(event.acceleration.x - last_x);
    float delta_y = abs(event.acceleration.y - last_y); 
    float delta_z = abs(event.acceleration.z - last_z);
    last_x = event.acceleration.x; last_y = event.acceleration.y; last_z = event.acceleration.z;

    // LOG MODIFICATION: Intercept high-G threshold physics triggers
    if (delta_x > SHAKE_THRESHOLD || delta_y > SHAKE_THRESHOLD || delta_z > SHAKE_THRESHOLD) {
      if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
        roboEyes.setMood(TIRED); roboEyes.anim_confused();
      }
      playSoundAsync(S_SURPRISE); 
      logTerminal(F("[ALERT] High G-force threshold breached! Droid is shaking."));
    }
  }

  // --- TIMER 1: Environmental Sampling & Battery Updates ---
  if (millis() - env_timer >= 1000) {
    env_timer = millis(); 
    updateBatteryTelemetry();

    // ─── HIGH-SPEED SENSE MULTIPLEXER ───
    // Save the exact state the pin was in right before testing
    bool previous_pin_state = digitalRead(MOTOR_IA1); 
    
    // Flip pin to an input with a weak internal pull-up
    pinMode(MOTOR_IA1, INPUT_PULLUP);
    delayMicroseconds(15); // Wait a fraction of a millisecond for the voltage plane to settle
    
    // Read the line. If detached, it floats HIGH. If docked, L9110S pulls it LOW.
    live_base_connected = (digitalRead(MOTOR_IA1) == LOW);
    
    // Immediately restore the pin back to its proper output drive configuration
    pinMode(MOTOR_IA1, OUTPUT);
    digitalWrite(MOTOR_IA1, previous_pin_state);
    
    // ─── SAFEGUARD FAILSAFE OVERRIDE ───
    // If the base is physically detached, immediately force the state machine to shut down motors
    if (!live_base_connected && motor_running) {
       moveDroid(STOP);
       motor_running = false;
       logTerminal(F("[SYSTEM] Mobility module broken circuit! All active motor drives halted safely."));
    };

    if (!robot_sleeping) { 
      int light_level = analogRead(LDR_PIN);
      live_light_level = light_level;
      if (light_level < LDR_THRESHOLD) {
        if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
          roboEyes.close(); roboEyes.setPosition(S);
        }
        if (motor_running) { moveDroid(STOP); motor_running = false; }
      } else {
        if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) { roboEyes.open(); }
      }
    }
  }

  // --- TIMER 2: Randomized Idle Movement ---
  if (!web_control_active && !robot_sleeping && (millis() - idle_timer >= next_idle_interval)) {
    idle_timer = millis(); 
    if (!showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
      roboEyes.setMood(DEFAULT); roboEyes.setPosition(DEFAULT); 
    }

    int current_light = analogRead(LDR_PIN);
    if (current_light >= LDR_THRESHOLD && !showing_dashboard && !showing_pre_sweat && !showing_post_sweat) {
      
      // LOG MODIFICATION: Track independent locomotion events automatically
      if (random(100) < 40) {
        DroidMovement options[] = {FORWARD, BACKWARD, TURN_LEFT, TURN_RIGHT};
        int choice = random(0, 4);
        moveDroid(options[choice]);
        
        logTerminal("[AUTONOMOUS] Internal AI triggered movement index: " + String(choice));
        
        motor_stop_time = millis() + random(MOTOR_MOVE_DURATION_MIN, MOTOR_MOVE_DURATION_MAX); 
        motor_running = true;
      }
    }
    next_idle_interval = random(MOTOR_MOVE_INTERVAL_MIN, MOTOR_MOVE_INTERVAL_MAX); 
  }
}