#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <CuteBuzzerSounds.h>

// OLED VARS
#define i2c_Address        0x3c
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


// OBJECTS
Adafruit_SH1106G display = Adafruit_SH1106G(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire1, OLED_RESET);

#include <FluxGarage_RoboEyes.h>
RoboEyes<Adafruit_SH1106G> roboEyes(display); // create RoboEyes instance

// EVENTS VARS
unsigned long event_timer; // will save the timestamps

void setup()   {
  Serial.begin(115200);

  delay(250);
  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(i2c_Address, true);

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
  
  if(millis() >= event_timer+1000){
    int touchState = digitalRead(TOUCH_PIN);
    if (touchState == HIGH) {
      Serial.println(touchState);
      roboEyes.setMood(HAPPY);
      roboEyes.anim_laugh();
      cute.play(S_HAPPY);
    }

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