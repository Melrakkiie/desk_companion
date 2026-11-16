// Module API : serveur HTTP (port 80), POST /event, validation, journal série.
// Transmet les Événements valides au module État.
#pragma once

// À appeler une fois dans setup() (le serveur écoute même avant la connexion Wi-Fi).
void apiInit();

// À appeler à chaque tour de loop() : sert les requêtes en attente, sans bloquer.
// Renvoie vrai si un Événement a changé l'Expression courante.
bool apiMiseAJour();
