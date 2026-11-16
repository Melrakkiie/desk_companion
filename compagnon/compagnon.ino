// Compagnon de bureau : programme principal.
// Assemble les modules Réseau, API, État et Expressions. Aucun delay() : la boucle tourne librement.
#include "api.h"
#include "etat.h"
#include "expressions.h"
#include "reseau.h"

static bool dernierEtatWifi = true;  // forcé à l'opposé au premier tour pour dessiner

static void redessiner() {
  const EtatCourant& e = etatCourant();
  // Au Repos, le Bandeau signale seulement une coupure du Wi-Fi
  const char* texte = e.texte;
  if (e.expression == EXPR_REPOS && !reseauConnecte()) texte = "pas de Wi-Fi";
  expressionsAfficher(e.expression, texte);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Compagnon : démarrage");

  if (!expressionsInit()) {
    Serial.println("Écran : introuvable (voir câblage, adresse 0x3C)");
  }
  etatInit();
  reseauInit();
  apiInit();
  dernierEtatWifi = !reseauConnecte();  // force un premier affichage
}

void loop() {
  reseauMiseAJour();
  bool aRedessiner = apiMiseAJour();
  aRedessiner |= etatMiseAJour(millis());

  bool connecte = reseauConnecte();
  if (connecte != dernierEtatWifi) {
    dernierEtatWifi = connecte;
    aRedessiner = true;
  }

  // On ne redessine que quand quelque chose a changé
  if (aRedessiner) redessiner();
}
