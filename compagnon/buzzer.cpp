#include "buzzer.h"
#include <Arduino.h>

static const int BROCHE_BUZZER = 18;
static const int RESOLUTION_BITS = 8;
static const int DUTY_50_POURCENT = 1 << (RESOLUTION_BITS - 1);  // plein volume

static unsigned int frequenceCourante = 0;
static unsigned int dutyCourant = 0;

// Volume 0-100 -> rapport cyclique : 0 = silence, 100 = 50 %. Courbe quadratique (l'oreille est
// logarithmique, la moitié de la course doit donner à peu près la moitié du volume perçu).
// Le rapport cyclique d'un buzzer passif agit mal en dessous de 50 % ; à régler à l'oreille sur le montage.
static unsigned int dutyDepuisVolume(int volume) {
  if (volume <= 0) return 0;
  if (volume >= 100) return DUTY_50_POURCENT;
  unsigned int duty = (unsigned int)(DUTY_50_POURCENT * volume * volume / 10000);
  return duty < 1 ? 1 : duty;
}

// API ledc du core ESP32 Arduino 3.x (ledcAttach/ledcWriteTone) ; en 2.x : ledcSetup + ledcAttachPin.
void buzzerInit() {
  ledcAttach(BROCHE_BUZZER, 1000, RESOLUTION_BITS);
  ledcWrite(BROCHE_BUZZER, 0);
}

void buzzerJouer(unsigned int frequenceHz, int volume) {
  unsigned int duty = frequenceHz == 0 ? 0 : dutyDepuisVolume(volume);
  if (frequenceHz == frequenceCourante && duty == dutyCourant) return;
  if (frequenceHz != 0 && frequenceHz != frequenceCourante) {
    ledcChangeFrequency(BROCHE_BUZZER, frequenceHz, RESOLUTION_BITS);
  }
  frequenceCourante = frequenceHz;
  dutyCourant = duty;
  ledcWrite(BROCHE_BUZZER, duty);
}
