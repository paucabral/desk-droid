#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <CuteBuzzerSounds.h>

// Modular includes from the include/ folder
#include "config.h"
#include "display_ui.h"
#include "motors.h"

// Instantiate Global Hardware Objects
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire1, OLED_RESET);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

#include <FluxGarage_RoboEyes.h>
RoboEyes<Adafruit_SH1106G> roboEyes(display); 

// Global States
unsigned long event_timer; 
float last_x = 0, last_y = 0, last_z = 0;

void setup() {
  Serial.begin(115200);
  delay(250);

  // 1. Initialize Display
  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(OLED_I2C_ADDRESS, true);

  // Splash Checklist Phase 1
  drawChecklist(" ", " ", " ", " ", "INITIALIZING...");
  delay(800); 

  // 2. Initialize Accelerometer
  Wire.begin(ACCEL_SDA, ACCEL_SCL);
  if(!accel.begin()) {
    drawChecklist("X", " ", " ", " ", "ACCELEROMETER ERROR!");
    Serial.println(F("Ooops, no ADXL345 detected ... Check wiring!"));
    while(1); 
  }
  accel.setRange(ADXL345_RANGE_4_G);
  drawChecklist("*", " ", " ", " ", "   TESTING...");
  delay(500);

  // 3. Initialize Touch GPIO Pin
  pinMode(TOUCH_PIN, INPUT);
  drawChecklist("*", "*", " ", " ", "   TESTING...");
  delay(500);

  // 4. Initialize LDR Pin
  pinMode(LDR_PIN, INPUT);
  drawChecklist("*", "*", "*", " ", "   TESTING...");
  delay(500);

  // 5. Initialize Audio Engine
  cute.init(BUZZER_PIN);
  drawChecklist("*", "*", "*", "*", "       READY!");
  cute.play(S_CONNECTION); 
  delay(600); 

  // 6. Initialize Motors
  initMotors();

  // 7. Display RoboEyes Custom Splash Card
  drawSplashArt();
  delay(3000); 

  // Sci-Fi Screen Flicker Effect
  for(int i = 0; i < 3; i++) {
    display.clearDisplay(); display.display(); delay(40);
    drawSplashArt(); delay(60);
  }
  cute.play(S_HAPPY_SHORT);

  // 7. Transition Control to RoboEyes Engine
  roboEyes.begin(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  event_timer = millis();
}

void loop() {
  roboEyes.update(); // Update screen drawings continuously

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

  // --- Slow Timed Environmental Sampling Intermittent Tasks ---
  if (millis() - event_timer >= 1000) {
    
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
    } else {
      roboEyes.open();
    }

    // Check Cumulative Mood Timeout (Every 5 seconds)
    if (millis() - event_timer >= 5000) {
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT); 
      event_timer = millis(); // Reset interval track
    }
  }
}