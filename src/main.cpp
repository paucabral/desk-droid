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

void setup()   {
  Serial.begin(115200);

  delay(250);
  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(OLED_I2C_ADDRESS, true);

  Wire.begin(ACCEL_SDA, ACCEL_SCL);
  if(!accel.begin()) {
    Serial.println("Ooops, no ADXL345 detected ... Check your wiring!");
    while(1);
  }
  accel.setRange(ADXL345_RANGE_4_G);

  roboEyes.begin(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  cute.init(BUZZER_PIN);
  cute.play(S_CONNECTION);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

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
      cute.play(S_HAPPY);
    }

    // Shake reading
    if (delta_x > SHAKE_THRESHOLD || delta_y > SHAKE_THRESHOLD || delta_z > SHAKE_THRESHOLD) {
      Serial.println("👋 Shake detected!");
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
    }
    else {
      roboEyes.open();
      // roboEyes.setMood(DEFAULT);
      // roboEyes.setPosition(DEFAULT); // eye position: middle center
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