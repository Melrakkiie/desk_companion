#include "buzzer.h"
#include <Arduino.h>

static const int BROCHE_BUZZER = 18;
// 12 bits : assez fin pour les tout petits rapports cycliques (fréquence max = 80 MHz / 4096, bien au-dessus des notes).
static const int RESOLUTION_BITS = 12;

static unsigned int frequenceCourante = 0;
static unsigned int dutyCourant = 0;

// Rapport cyclique (sur 4096) par palier de Volume (0, 10, …, 100) : 0 = silence, 100 = 50 % (le volume d'avant).
// Sur le montage, le buzzer passif sonne déjà presque à fond vers 2 % : les paliers 10 à 90 montent donc de 0,1 %
// à ~2 % par pas de ×1,45 (même écart en dB à chaque cran, perçu comme régulier).
// shortcut: réglée à l'oreille sur le vrai buzzer, pas mesurée.
static const unsigned int DUTY_PAR_PALIER[11] = {0, 4, 6, 9, 13, 19, 28, 40, 58, 85, 2048};

static unsigned int dutyDepuisVolume(int volume) {
  int palier = volume / 10;
  if (palier < 0) palier = 0;
  if (palier > 10) palier = 10;
  return DUTY_PAR_PALIER[palier];
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
