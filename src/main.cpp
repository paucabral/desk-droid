#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <CuteBuzzerSounds.h>

// OLED VARS
#define OLED_I2C_ADDRESS   0x3c
#define OLED_SCL           13
#define OLED_SDA           12
#define OLED_SCREEN_WIDTH  128 // OLED display width, in pixels
#define OLED_SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET         -1   //   QT-PY / XIAO

// BUZZER VARS
#define BUZZER_PIN         11

// TOUCH SENSOR VARS
#define TOUCH_PIN          10

// LIGHT SENSOR VARS
#define LDR_PIN            8
#define LDR_THRESHOLD      300 // Tune according to preference

// ACCELEROMETER VARS
#define ACCEL_SCL          7
#define ACCEL_SDA          6
#define SHAKE_THRESHOLD    8.0
float last_x = 0, last_y = 0, last_z = 0;


// OBJECTS
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire1, OLED_RESET);

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

#include <FluxGarage_RoboEyes.h>
RoboEyes<Adafruit_SH1106G> roboEyes(display); // create RoboEyes instance

// EVENTS VARS
unsigned long event_timer; // will save the timestamps


void drawChecklist(const char* accel, const char* touch, const char* light, const char* audio, const char* sysMsg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  // Header
  display.setCursor(0, 0);
  display.println(F("=== SYSTEMS CHECK ==="));
  display.println(F("---------------------"));
  
  // Checklist Items (Max ~21 chars per line for 128 width)
  display.print(F("[")); display.print(accel); display.println(F("]     ACCELEROMETER"));
  display.print(F("[")); display.print(touch); display.println(F("]      TOUCH SENSOR"));
  display.print(F("[")); display.print(light); display.println(F("]      LIGHT SENSOR"));
  display.print(F("[")); display.print(audio); display.println(F("]      BUZZER AUDIO"));
  
  // Footer / System Status
  display.println(F("---------------------"));
  display.print(F("STATUS: ")); display.println(sysMsg);
  
  display.display();
}


void setup()   {
  Serial.begin(115200);

  delay(250);
  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(OLED_I2C_ADDRESS, true);

  // Phase 1: Show blank checklist / Initializing
  drawChecklist(" ", " ", " ", " ", "INITIALIZING...");
  delay(800); // Dramatic pause

  // Phase 2: Check Accelerometer
  Wire.begin(ACCEL_SDA, ACCEL_SCL);
  if(!accel.begin()) {
    drawChecklist("X", " ", " ", " ", "ACCELEROMETER ERROR!");
    Serial.println("Ooops, no ADXL345 detected ... Check your wiring!");
    while(1); // Halt system on error
  }
  accel.setRange(ADXL345_RANGE_4_G);
  drawChecklist("*", " ", " ", " ", "TESTING...");
  delay(400);

  // Phase 3: Check Touch Sensor
  pinMode(TOUCH_PIN, INPUT);
  drawChecklist("*", "*", " ", " ", "TESTING...");
  delay(400);

  // Phase 4: Check Light Sensor
  pinMode(LDR_PIN, INPUT);
  drawChecklist("*", "*", "*", " ", "TESTING...");
  delay(400);

  // Phase 5: Check Audio Engine
  cute.init(BUZZER_PIN);
  drawChecklist("*", "*", "*", "*", "       READY!");
  
  // Play startup sound while "READY!" is displayed
  cute.play(S_CONNECTION); 
  delay(1000); 

  // Phase 6: Clear and Handover to RoboEyes
  roboEyes.begin(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  // Event Timer
  event_timer = millis();
}


void loop() {
  roboEyes.update(); // update eyes drawings


  sensors_event_t event; 
  accel.getEvent(&event);

  float delta_x = abs(event.acceleration.x - last_x);
  float delta_y = abs(event.acceleration.y - last_y);
  float delta_z = abs(event.acceleration.z - last_z);

  /* Display the results (acceleration is measured in m/s^2) */
  Serial.print("X: "); Serial.print(delta_x); Serial.print("  ");
  Serial.print("Y: "); Serial.print(delta_y); Serial.print("  ");
  Serial.print("Z: "); Serial.print(delta_z); Serial.print("  ");
  Serial.println("m/s^2");

  if(millis() >= event_timer+1000){
    // Touch reading
    int touchState = digitalRead(TOUCH_PIN);
    if (touchState == HIGH) {
      Serial.println(touchState);
      roboEyes.setMood(HAPPY);
      roboEyes.anim_laugh();
      cute.play(S_SUPER_HAPPY);
    }

    // Shake reading
    if (delta_x > SHAKE_THRESHOLD || delta_y > SHAKE_THRESHOLD || delta_z > SHAKE_THRESHOLD) {
      roboEyes.setMood(TIRED);
      roboEyes.anim_confused();
      cute.play(S_SURPRISE);
    }

    last_x = event.acceleration.x;
    last_y = event.acceleration.y;
    last_z = event.acceleration.z;

    // Light level reading
    int lightLevel = analogRead(LDR_PIN);
    Serial.print(F("Light level: "));
    Serial.println(lightLevel);

    if (lightLevel < LDR_THRESHOLD) {
      roboEyes.close();
      roboEyes.setPosition(S);
      // cute.play(S_SLEEPING);
    }
    else {
      roboEyes.open();
    }

    // Do once after defined number of milliseconds, then reset timer and flags to restart the whole animation sequence
    if(millis() >= event_timer+5000){
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT); // eye position: middle center
      
      // Reset the timer and the event flags to restart the whole "complex animation loop"
      event_timer = millis(); // reset timer
    }
  }
}