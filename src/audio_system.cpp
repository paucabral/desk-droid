#include "audio_system.h"
#include <CuteBuzzerSounds.h>

QueueHandle_t buzzerQueue;

void buzzerCore0Task(void *pvParameters) {
  int soundId;
  for (;;) {
    if (xQueueReceive(buzzerQueue, &soundId, portMAX_DELAY) == pdPASS) {
      cute.play(soundId); 
    }
  }
}

void initAudioSystem() {
  buzzerQueue = xQueueCreate(2, sizeof(int));
  xTaskCreatePinnedToCore(
    buzzerCore0Task, "BuzzerTask", 3072, NULL, 1, NULL, 0
  );
}

void playSoundAsync(int soundId) {
  if (buzzerQueue != NULL) {
    xQueueSend(buzzerQueue, &soundId, 0);
  }
}