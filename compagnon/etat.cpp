#include "etat.h"

#include <string.h>

static EtatCourant etat;

static void installer(Expression expression, const char* texte, bool temporaire,
                      unsigned long maintenantMs, unsigned long dureeMs,
                      const char* artiste = nullptr) {
  etat.expression = expression;
  etat.texte[0] = '\0';
  if (texte != nullptr) {
    strncpy(etat.texte, texte, TEXTE_MAX);
    etat.texte[TEXTE_MAX] = '\0';
  }
  etat.artiste[0] = '\0';
  if (artiste != nullptr) {
    strncpy(etat.artiste, artiste, TEXTE_MAX);
    etat.artiste[TEXTE_MAX] = '\0';
  }
  etat.temporaire = temporaire;
  etat.debutMs = maintenantMs;
  etat.dureeMs = dureeMs;
}

void etatInit() {
  installer(EXPR_REPOS, nullptr, false, 0, 0);
}

bool etatTypeDepuisNom(const char* nom, TypeEvenement* type) {
  static const char* const NOMS[] = {"claude_done", "claude_waiting", "claude_working", "music", "git_push"};
  if (nom == nullptr) return false;
  for (int i = 0; i < 5; i++) {
    if (strcmp(nom, NOMS[i]) == 0) {
      *type = (TypeEvenement)i;  // l'ordre de NOMS suit celui de l'enum TypeEvenement
      return true;
    }
  }
  return false;
}

bool etatRecevoir(TypeEvenement type, const char* texte, unsigned long maintenantMs,
                  const char* artiste) {
  (void)artiste;  // ticket #8 : le passer à installer(...)
  switch (type) {
    case EV_CLAUDE_DONE:
      installer(EXPR_CONTENT, texte, true, maintenantMs, DUREE_CLAUDE_DONE_MS);
      return true;
    // Un case par ticket, à remplir (en attendant : accepté sans effet).
    case EV_CLAUDE_WAITING:  // ticket #7
    case EV_CLAUDE_WORKING:  // ticket #7
    case EV_MUSIC:           // ticket #8
    case EV_GIT_PUSH:        // ticket #9
    default:
      return false;
  }
}

bool etatMiseAJour(unsigned long maintenantMs) {
  if (etat.temporaire && maintenantMs - etat.debutMs >= etat.dureeMs) {
    etatInit();
    return true;
  }
  return false;
}

const EtatCourant& etatCourant() {
  return etat;
}
