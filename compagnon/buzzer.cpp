#include "buzzer.h"
#include <Arduino.h>

static const int BROCHE_BUZZER = 18;
static const int RESOLUTION_BITS = 8;
static const int DUTY_50_POURCENT = 1 << (RESOLUTION_BITS - 1);  // volume fixe

static unsigned int frequenceCourante = 0;

// API ledc du core ESP32 Arduino 3.x (ledcAttach/ledcWriteTone) ; en 2.x : ledcSetup + ledcAttachPin.
void buzzerInit() {
  ledcAttach(BROCHE_BUZZER, 1000, RESOLUTION_BITS);
  ledcWrite(BROCHE_BUZZER, 0);
}

void buzzerJouer(unsigned int frequenceHz) {
  if (frequenceHz == frequenceCourante) return;
  frequenceCourante = frequenceHz;
  if (frequenceHz == 0) {
    ledcWrite(BROCHE_BUZZER, 0);
  } else {
    ledcChangeFrequency(BROCHE_BUZZER, frequenceHz, RESOLUTION_BITS);
    ledcWrite(BROCHE_BUZZER, DUTY_50_POURCENT);
  }
}
