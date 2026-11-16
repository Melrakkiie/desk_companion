// Compagnon de bureau : programme principal.
// Assemble les modules Réseau, API, État et Expressions. Aucun delay() : la boucle tourne librement.
#include "api.h"
#include "etat.h"
#include "expressions.h"
#include "reseau.h"

static bool dernierEtatWifi = true;  // forcé à l'opposé au premier tour pour dessiner

// Cadence du redessin périodique (~30 images/s), pour les animations des Expressions.
static const unsigned long PERIODE_ANIM_MS = 33;
static unsigned long dernierDessinMs = 0;

static void redessiner(unsigned long maintenantMs) {
  dernierDessinMs = maintenantMs;
  const EtatCourant& e = etatCourant();
  // Au Repos, le Bandeau signale seulement une coupure du Wi-Fi
  const char* texte = e.texte;
  if (e.expression == EXPR_REPOS && !reseauConnecte()) texte = "pas de Wi-Fi";
  expressionsAfficher(e, texte, maintenantMs);
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

  // Redessin sur changement, et périodique pour les animations
  // shortcut: on redessine toujours à cadence fixe, même sans animation ; affiner si l'écran coûte trop.
  unsigned long maintenant = millis();
  if (aRedessiner || maintenant - dernierDessinMs >= PERIODE_ANIM_MS) redessiner(maintenant);
}
