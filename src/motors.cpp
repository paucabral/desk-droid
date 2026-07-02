#include <Arduino.h>
#include "motors.h"
#include "config.h"

// Set up motor pins as outputs
void initMotors() {
  pinMode(MOTOR_IA1, OUTPUT);
  pinMode(MOTOR_IB1, OUTPUT);
  pinMode(MOTOR_IA2, OUTPUT);
  pinMode(MOTOR_IB2, OUTPUT);
  
  // Ensure droid starts completely still
  moveDroid(STOP);
}

// Single handler function that accepts the movement argument
void moveDroid(DroidMovement movement) {
  switch (movement) {
    case FORWARD:
      digitalWrite(MOTOR_IA1, HIGH);
      digitalWrite(MOTOR_IB1, LOW);
      digitalWrite(MOTOR_IA2, HIGH);
      digitalWrite(MOTOR_IB2, LOW);
      break;

    case BACKWARD:
      digitalWrite(MOTOR_IA1, LOW);
      digitalWrite(MOTOR_IB1, HIGH);
      digitalWrite(MOTOR_IA2, LOW);
      digitalWrite(MOTOR_IB2, HIGH);
      break;

    case TURN_LEFT:
      // Left motor backward, Right motor forward (Pivot turn)
      digitalWrite(MOTOR_IA1, LOW);
      digitalWrite(MOTOR_IB1, HIGH);
      digitalWrite(MOTOR_IA2, HIGH);
      digitalWrite(MOTOR_IB2, LOW);
      break;

    case TURN_RIGHT:
      // Left motor forward, Right motor backward (Pivot turn)
      digitalWrite(MOTOR_IA1, HIGH);
      digitalWrite(MOTOR_IB1, LOW);
      digitalWrite(MOTOR_IA2, LOW);
      digitalWrite(MOTOR_IB2, HIGH);
      break;

    case STOP:
    default:
      // All pins LOW cuts power to the motors
      digitalWrite(MOTOR_IA1, LOW);
      digitalWrite(MOTOR_IB1, LOW);
      digitalWrite(MOTOR_IA2, LOW);
      digitalWrite(MOTOR_IB2, LOW);
      break;
  }
}