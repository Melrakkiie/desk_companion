// Module Buzzer : pilote le buzzer passif (GPIO 18) en PWM. Aucune logique de Son : la fréquence vient de l'État.
#pragma once

// À appeler une fois dans setup().
void buzzerInit();

// À appeler à chaque tour de loop() avec la fréquence voulue (0 = silence) et le Volume (0-100 ; 0 = silence total).
// Ne touche au PWM que si la fréquence ou le Volume change ; ne bloque jamais.
void buzzerJouer(unsigned int frequenceHz, int volume);
