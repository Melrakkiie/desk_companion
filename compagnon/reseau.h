// Module Réseau : connexion Wi-Fi non bloquante et nom compagnon.local (mDNS).
#pragma once

// À appeler une fois dans setup().
void reseauInit();

// À appeler à chaque tour de loop() : surveille le Wi-Fi, retente toutes les 10 s
// si besoin, republie compagnon.local après chaque reconnexion. Ne bloque jamais.
void reseauMiseAJour();

// Vrai si le Compagnon est connecté au Wi-Fi.
bool reseauConnecte();
