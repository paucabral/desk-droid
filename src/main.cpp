#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
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
#define ACCEL_I2C_ADDRESS  0x53
#define ACCEL_POWER_CTL    0x2D
#define ACCEL_DATAX0       0x32
#define ACCEL_SCL          7
#define ACCEL_SDA          6
float X_out, Y_out, Z_out;


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
  display.begin(OLED_I2C_ADDRESS, true);

  delay(250);
  Wire.begin(ACCEL_SDA, ACCEL_SCL);
  Wire.beginTransmission(ACCEL_I2C_ADDRESS);
  Wire.write(ACCEL_POWER_CTL); // Access/ talk to POWER_CTL Register - 0x2D
  // Enable measurement
  Wire.write(8); // (8dec -> 0000 1000 binary) Bit D3 High for measuring enable 
  Wire.endTransmission();
  delay(10);

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


  Wire.beginTransmission(ACCEL_I2C_ADDRESS);
  Wire.write(ACCEL_DATAX0); // Access/ talk to POWER_CTL Register - 0x2D
  Wire.endTransmission(false);
  Wire.requestFrom(ACCEL_I2C_ADDRESS, 6);
  X_out = (Wire.read() | Wire.read() << 8) / 256;
  Y_out = (Wire.read() | Wire.read() << 8) / 256;
  Z_out = (Wire.read() | Wire.read() << 8) / 256;
  Serial.print("Xa= "); Serial.print(X_out);
  Serial.print(" Ya= "); Serial.print(Y_out);
  Serial.print(" Za= "); Serial.println(Z_out);

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