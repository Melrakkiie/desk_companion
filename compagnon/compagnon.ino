// Compagnon de bureau : programme principal.
// Assemble les modules Réseau, API, État, Expressions et Buzzer. Aucun delay() : la boucle tourne librement.
#include "api.h"
#include "buzzer.h"
#include "etat.h"
#include "expressions.h"
#include "reseau.h"

const int BROCHE_BOUTON = 27;  // entre D27 et GND, lu en INPUT_PULLUP (enfoncé = niveau bas)

const int BROCHE_BOUTON_CLAUDE = 26;  // entre D26 et GND, lu comme le Bouton musique

static bool dernierEtatWifi = true;  // forcé à l'opposé au premier tour pour dessiner

// Cadence du redessin périodique : PERIODE_ANIM_MS (etat.h).
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
  pinMode(BROCHE_BOUTON, INPUT_PULLUP);
  pinMode(BROCHE_BOUTON_CLAUDE, INPUT_PULLUP);
  etatInit(digitalRead(BROCHE_BOUTON) == LOW, digitalRead(BROCHE_BOUTON_CLAUDE) == LOW);
  buzzerInit();
  reseauInit();
  apiInit();
  dernierEtatWifi = !reseauConnecte();  // force un premier affichage
}

void loop() {
  reseauMiseAJour();
  bool aRedessiner = apiMiseAJour();
  aRedessiner |= etatMiseAJour(millis());
  aRedessiner |= etatBouton(digitalRead(BROCHE_BOUTON) == LOW, millis());
  aRedessiner |= etatBoutonClaude(digitalRead(BROCHE_BOUTON_CLAUDE) == LOW, millis());
  buzzerJouer(etatFrequence(millis()));  // Son courant, sans bloquer la boucle

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
