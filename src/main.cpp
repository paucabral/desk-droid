#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <CuteBuzzerSounds.h>
#include <WiFi.h> // Native ESP32 WiFi library

// Modular includes from the include/ folder
#include "config.h"
#include "display_ui.h"
#include "motors.h"

// Instantiate Global Hardware Objects
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire1, OLED_RESET);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

#include <FluxGarage_RoboEyes.h>
RoboEyes<Adafruit_SH1106G> roboEyes(display); 

// Global Timers
unsigned long env_timer;           // Tracks fixed 1-second sensor updates
unsigned long idle_timer;          // Tracks variable pacing for random movements
unsigned long next_idle_interval;  // Dynamically changes to randomize rest duration
unsigned long motor_stop_time = 0; // Precision stopwatch for active movement duration

// Global States
float last_x = 0, last_y = 0, last_z = 0;
bool motor_running = false;

void setup() {
  Serial.begin(115200);
  delay(250);

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
  delay(1000); // Give user a moment to see Part 1 pass completely

  // --- NEW: PART 2 - WIFI NETWORK INITIALIZATION GATE ---
  drawWifiScreen("SEARCHING...", "0.0.0.0");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long wifi_start_time = millis();
  bool wifi_connected = false;

  // Graceful Timeout Loop: Check connection status without locking up permanently
  while (millis() - wifi_start_time < WIFI_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      break;
    }
    delay(100); // Maintain background processor stability
  }

  if (wifi_connected) {
    // If successfully connected
    drawWifiScreen("ONLINE [OK]", WiFi.localIP().toString().c_str());
    Serial.print(F("WiFi Connected successfully. Assigned IP: "));
    Serial.println(WiFi.localIP());
  } else {
    // If connection failed or timed out, disconnect modem gracefully to conserve power
    WiFi.disconnect(true); 
    drawWifiScreen("OFFLINE MODE", "N/A");
    Serial.println(F("WiFi connection failed or timed out. Proceeding in offline mode..."));
  }
  delay(2000); // Hold network diagnostic info on screen before drawing the splash badge

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
  next_idle_interval = random(4000, 9000); 
  randomSeed(analogRead(LDR_PIN)); 
}

void loop() {
  roboEyes.update(); // Update screen drawings continuously

  // --- Real-Time Motor Cutoff ---
  if (motor_running && millis() >= motor_stop_time) {
    moveDroid(STOP);
    motor_running = false;
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
    roboEyes.setMood(TIRED);
    roboEyes.anim_confused();
    cute.play(S_SURPRISE);
  }

  // --- TIMER 1: Fixed Environmental Sampling (Strictly every 1000ms) ---
  if (millis() - env_timer >= 1000) {
    env_timer = millis(); 
    
    // Check Touch Sensor
    int touchState = digitalRead(TOUCH_PIN);
    if (touchState == HIGH) {
      roboEyes.setMood(HAPPY);
      roboEyes.anim_laugh();
      cute.play(S_SUPER_HAPPY);
    }

    // Check Ambient Light levels
    int lightLevel = analogRead(LDR_PIN);
    Serial.print("Light Level: "); Serial.println(lightLevel);
    
    if (lightLevel < LDR_THRESHOLD) {
      roboEyes.close();
      roboEyes.setPosition(S);
      if (motor_running) {
        moveDroid(STOP);
        motor_running = false;
      }
    } else {
      roboEyes.open();
    }
  }

  // --- TIMER 2: Randomized Idle Movement (Occasional, Variable Intervals) ---
  if (millis() - idle_timer >= next_idle_interval) {
    idle_timer = millis(); 
    
    roboEyes.setMood(DEFAULT);
    roboEyes.setPosition(DEFAULT); 

    int currentLight = analogRead(LDR_PIN);
    if (currentLight >= LDR_THRESHOLD) {
      
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