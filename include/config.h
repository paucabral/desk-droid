#pragma once

// OLED CONFIGURATION
#define OLED_I2C_ADDRESS   0x3c
#define OLED_SCL           13
#define OLED_SDA           12
#define OLED_SCREEN_WIDTH  128 
#define OLED_SCREEN_HEIGHT 64 
#define OLED_RESET         -1   

// BUZZER PIN
#define BUZZER_PIN         11

// TOUCH SENSOR PIN
#define TOUCH_PIN          10

// LIGHT SENSOR (LDR) CONFIGURATION
#define LDR_PIN            8
#define LDR_THRESHOLD      300 

// ACCELEROMETER CONFIGURATION
#define ACCEL_SCL          7
#define ACCEL_SDA          6
#define SHAKE_THRESHOLD    8.0