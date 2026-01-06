#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <CuteBuzzerSounds.h>
#include "DHT.h"

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

// HUM/TEMP SENSOR VARS
#define DHTPIN             9 // for further testing
#define DHTTYPE            DHT11 

// LIGHT SENSOR VARS
#define LDR_PIN            8
#define LDR_THRESHOLD      1000 // Tune according to preference

// AIR QUALITY SENSOR VARS
#define AQI_PIN            7
#define AQI_THRESHOLD      1800 // Tune according to preference

// OBJECTS
DHT dht(DHTPIN, DHTTYPE);

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

  dht.begin();

  roboEyes.begin(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, 100); 
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  cute.init(BUZZER_PIN);
  cute.play(S_CONNECTION);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(AQI_PIN, INPUT);

  // Event Timer
  event_timer = millis();
}


void loop() {
  roboEyes.update(); // update eyes drawings

  int aqiValue = analogRead(AQI_PIN);
  
  if(millis() >= event_timer+1000){
    int touchState = digitalRead(TOUCH_PIN);
    if (touchState == HIGH) {
      Serial.println(touchState);
      roboEyes.setMood(HAPPY);
      roboEyes.anim_laugh();
      cute.play(S_HAPPY);
    }

    int aqiValue = analogRead(AQI_PIN);
    Serial.print(F("Air Quality Index: "));
    Serial.println(aqiValue);
    if (aqiValue > AQI_THRESHOLD) {
      // Poor air quality detected
      roboEyes.setMood(ANGRY);
      roboEyes.setHFlicker(ON, 5);
      cute.play(S_FART1);
    }

    // Light level reading
    int lightLevel = analogRead(LDR_PIN);
    Serial.print(F("Light level: "));
    Serial.println(lightLevel);

    if (lightLevel < LDR_THRESHOLD) {
      roboEyes.close();
    }
    else {
      roboEyes.open();
      // roboEyes.setMood(DEFAULT);
      // roboEyes.setPosition(DEFAULT); // eye position: middle center
    }
  }

  // Measure sensor data every 3.5 seconds
  if(millis() >= event_timer+3500){

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float humidty = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float temperature = dht.readTemperature(false);

    // Check if any reads failed and exit early (to try again).
    // if (isnan(humidty) || isnan(temperature)) {
    //   Serial.println(F("Failed to read from DHT sensor!"));
    //   return;
    // }

    // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(temperature, humidty, false);

    Serial.print(F("Humidity: "));
    Serial.print(humidty);
    Serial.print(F("%  Temperature: "));
    Serial.print(temperature);
    Serial.print(F("°C "));
    Serial.print(F("   Heat index: "));
    Serial.println(hic);

    if (temperature < 15){
      // Robot is freezing if temperature is below 15° Celsius
      roboEyes.setMood(TIRED);
      roboEyes.setHFlicker(ON, 2);
    }
    else if (temperature > 25){
      // Robot is sweating if temperature is above 25° Celsius
      roboEyes.setMood(TIRED);
      roboEyes.setSweat(ON);
      roboEyes.setPosition(S);
    }
    else {
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT); // eye position: middle center
      roboEyes.setHFlicker(OFF);
      roboEyes.setSweat(OFF);
    }

    // Reset the timer and the event flags to restart the whole "complex animation loop"
    event_timer = millis(); // reset timer
  }
}