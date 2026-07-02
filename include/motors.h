#pragma once

// Define readable movement arguments
enum DroidMovement {
  FORWARD,
  BACKWARD,
  TURN_LEFT,
  TURN_RIGHT,
  STOP
};

// Function declarations
void initMotors();
void moveDroid(DroidMovement movement);