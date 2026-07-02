#pragma once

// OLED CONFIGURATION
#define OLED_I2C_ADDRESS        0x3c
#define OLED_SCL                13
#define OLED_SDA                12
#define OLED_SCREEN_WIDTH       128 
#define OLED_SCREEN_HEIGHT      64 
#define OLED_RESET              -1   

// BUZZER PIN
#define BUZZER_PIN              11

// TOUCH SENSOR PIN
#define TOUCH_PIN               10

// LIGHT SENSOR (LDR) CONFIGURATION
#define LDR_PIN                 8
#define LDR_THRESHOLD           300 

// ACCELEROMETER CONFIGURATION
#define ACCEL_SCL               7
#define ACCEL_SDA               6
#define SHAKE_THRESHOLD         8.0

// MOTOR CONFIGURATION (Dual H-Bridge e.g., L9110S / L298N)
#define MOTOR_IA1               3  // Left Motor Forward Control
#define MOTOR_IB1               2  // Left Motor Backward Control
#define MOTOR_IA2               5  // Right Motor Forward Control
#define MOTOR_IB2               4  // Right Motor Backward Control
#define MOTOR_MOVE_DURATION_MIN 250  // Minimum Interval between motor movements (in milliseconds)
#define MOTOR_MOVE_DURATION_MAX 500  // Maximum Interval between motor movements (in milliseconds)
#define MOTOR_MOVE_INTERVAL_MIN 5000  // Minimum Interval between motor movements (in milliseconds)
#define MOTOR_MOVE_INTERVAL_MAX 10000  // Maximum Interval between motor movements (in milliseconds)

// INTERIM WIFI CONFIGURATION --> Target to transition from hardcoded credentials to user based inputs at a later release.
#define WIFI_SSID       "YOUR_SSID_HERE"
#define WIFI_PASSWORD   "YOUR_PASSWORD_HERE"
#define WIFI_TIMEOUT_MS 5000  // Maximum milliseconds to try connecting before bypassing