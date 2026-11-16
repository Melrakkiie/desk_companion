// Module Expressions : dessine sur l'écran OLED (dessin uniquement, aucune logique d'Événement).
#pragma once

#include "etat.h"

// À appeler une fois dans setup(). Renvoie faux si l'écran ne répond pas.
bool expressionsInit();

// Efface tout l'écran puis dessine le visage de l'Expression de l'État dans la Zone visage
// et le texte du Bandeau (nullptr ou "" = Bandeau vide). maintenantMs (millis()) sert
// aux animations ; loop() redessine à cadence fixe, donc l'appel doit rester rapide.
void expressionsAfficher(const EtatCourant& e, const char* texteBandeau, unsigned long maintenantMs);
