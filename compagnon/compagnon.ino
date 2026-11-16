// Compagnon de bureau : programme principal.
// Assemble les modules Réseau et Expressions. Aucun delay() : la boucle tourne librement.
#include "expressions.h"
#include "reseau.h"

static bool dernierEtatAffiche = true;  // forcé à l'opposé au premier tour pour dessiner

void setup() {
  Serial.begin(115200);
  Serial.println("Compagnon : démarrage");

  if (!expressionsInit()) {
    Serial.println("Écran : introuvable (voir câblage, adresse 0x3C)");
  }
  reseauInit();
  dernierEtatAffiche = !reseauConnecte();  // force un premier affichage
}

void loop() {
  reseauMiseAJour();

  // On ne redessine que quand l'état du Wi-Fi change
  bool connecte = reseauConnecte();
  if (connecte != dernierEtatAffiche) {
    expressionsAfficherNeutre(connecte ? nullptr : "pas de Wi-Fi");
    dernierEtatAffiche = connecte;
  }
}
