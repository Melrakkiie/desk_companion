// Module Expressions : dessine sur l'écran OLED (dessin uniquement, aucune logique d'Événement).
#pragma once

#include "etat.h"

// Géométrie mesurée sur le matériel (ticket #3) : la bande jaune du haut est le Bandeau.
const int BANDEAU_HAUTEUR = 16;     // Bandeau = lignes 0 à 15
const int ZONE_VISAGE_Y = 16;       // Zone visage = lignes 16 à 63
const int ZONE_VISAGE_HAUTEUR = 48;

// À appeler une fois dans setup(). Renvoie faux si l'écran ne répond pas.
bool expressionsInit();

// Efface tout l'écran puis dessine le visage de l'Expression dans la Zone visage
// et le texte du Bandeau (nullptr ou "" = Bandeau vide).
void expressionsAfficher(Expression expression, const char* texteBandeau);
