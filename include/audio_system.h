#pragma once
#include <Arduino.h>

// Initialize the FreeRTOS Audio Core task and queue
void initAudioSystem();

// Post a sound request to Core 0 asynchronously
void playSoundAsync(int soundId);