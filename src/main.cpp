#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

#include <CuteBuzzerSounds.h>
#undef debug // Clears the macro collision before WiFiManager loads

#include <WiFiManager.h> // Includes WiFi.h, WebServer.h, DNSServer.h, and Preferences.h internally

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

// Gesture & Page Management Timers
unsigned long touch_start_time = 0;
unsigned long dashboard_timeout = 0; // Tracks visibility length via config macro

// Global States
float last_x = 0, last_y = 0, last_z = 0;
bool motor_running = false;
bool showing_dashboard = false;    // Controls screen routing states

// NEW: Dynamic mutable location variable (Ready to be overwritten by your future API call)
String live_location = "UNKNOWN";

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println(F("Entered Configuration Portal Mode!"));
  Serial.print(F("AP IP Address: "));
  Serial.println(WiFi.softAPIP());
  drawWifiScreen("PORTAL ACTIVE", "CONFIG MODE", "Connect your device\nto: Desk-Droid-Setup");
  cute.play(S_MODE1); 
}

void setup() {
  Serial.begin(115200);
  delay(2000); // Expanded wait: Gives native USB CDC serial link time to bind to PC

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
    Serial.print(F("WiFi Connected successfully. Assigned IP: "));
    Serial.println(WiFi.localIP());
    
    // Set placeholder indicating network is available for API lookups
    live_location = "LOCALIZING..."; 
  } else {
    drawWifiScreen("NETWORK CHECK", "OFFLINE MODE", "No connection found.\nBypassing network boot.");
    Serial.println(F("WiFi Portal timed out or failed. Proceeding to Offline Mode safely..."));
    
    // Fallback if the robot boots completely without internet access
    live_location = "OFFLINE"; 
  }
  delay(2500); 

  // 7. Display RoboEyes Custom Splash Card
  drawSplashArt();
  delay(3000); 

  // Sci-Fi Screen Flicker Effect
  for(int i = 0; i < 3; i++) {
    display.clearDisplay(); display.display(); delay(40);
    drawSplashArt(); delay(60);
  }
  cute.play(S_HAPPY_SHORT);

  // Transition Control to RoboEyes Engine
  roboEyes.begin(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  // Initialize Timers
  env_timer = millis();
  idle_timer = millis();
  next_idle_interval = random(MOTOR_MOVE_INTERVAL_MIN, MOTOR_MOVE_INTERVAL_MAX); 
  randomSeed(analogRead(LDR_PIN)); 
}

void loop() {
  // --- Dynamic Screen Page Router ---
  if (!showing_dashboard) {
    roboEyes.update(); 
  } else {
    if (millis() >= dashboard_timeout) {
      showing_dashboard = false; 
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
    } else {
      String ipStr = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "OFFLINE";
      int mockBattery = 85; 

      // Render Dashboard panel passing the dynamic runtime string string
      drawDashboard("PARTLY CLOUDY", 27, 62, 20, ipStr.c_str(), mockBattery, live_location.c_str());
    }
  }

  // --- Real-Time Motor Cutoff ---
  if (motor_running && millis() >= motor_stop_time) {
    moveDroid(STOP);
    motor_running = false;
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
        if (!hold_triggered && !showing_dashboard) {
          unsigned long press_duration = millis() - touch_start_time;
          
          Serial.print(F("[TOUCH-DEBUG] Release Event Logged. Total Duration: "));
          Serial.print(press_duration);
          Serial.println(F(" ms"));

          if (press_duration >= TRIGGER_HAPPY_HOLD_MS && press_duration < TRIGGER_ANGRY_HOLD_MS) {
            Serial.println(F("[TOUCH-DEBUG] Context: Short Tap. Executing HAPPY emotion."));
            roboEyes.setMood(HAPPY);
            roboEyes.anim_laugh();
            cute.play(S_SUPER_HAPPY);
          } 
          else if (press_duration >= TRIGGER_ANGRY_HOLD_MS && press_duration < TRIGGER_DASHBOARD_MS) {
            Serial.println(F("[TOUCH-DEBUG] Context: Medium Hold. Executing ANGRY emotion."));
            roboEyes.setMood(ANGRY);
            cute.play(S_OHOOH); 
          }
        }
      }
    }
  }
  lastRawTouchState = rawTouchReading;

  // Real-time Dashboard Threshold Intercept
  if (debouncedTouchState == HIGH && !hold_triggered && !showing_dashboard) {
    if (millis() - touch_start_time >= TRIGGER_DASHBOARD_MS) {
      Serial.println(F("[TOUCH-DEBUG] Context: Long Hold Target Met! Opening Dashboard panel."));
      showing_dashboard = true;
      dashboard_timeout = millis() + DASHBOARD_DURATION_MS; 
      cute.play(S_BUTTON_PUSHED);               
      hold_triggered = true; 
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
    if (!showing_dashboard) {
      roboEyes.setMood(TIRED);
      roboEyes.anim_confused();
    }
    cute.play(S_SURPRISE);
  }

  // --- TIMER 1: Fixed Environmental Sampling (Strictly every 1000ms) ---
  if (millis() - env_timer >= 1000) {
    env_timer = millis(); 
    
    int light_level = analogRead(LDR_PIN);
    Serial.print("Light Level: "); Serial.println(light_level);
    
    if (light_level < LDR_THRESHOLD) {
      if (!showing_dashboard) {
        roboEyes.close();
        roboEyes.setPosition(S);
      }
      if (motor_running) {
        moveDroid(STOP);
        motor_running = false;
      }
    } else {
      if (!showing_dashboard) {
        roboEyes.open();
      }
    }
  }

  // --- TIMER 2: Randomized Idle Movement (Occasional, Variable Intervals) ---
  if (millis() - idle_timer >= next_idle_interval) {
    idle_timer = millis(); 
    
    if (!showing_dashboard) {
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT); 
    }

    int current_light = analogRead(LDR_PIN);
    if (current_light >= LDR_THRESHOLD && !showing_dashboard) {
      
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