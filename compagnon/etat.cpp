#include "etat.h"

#include <string.h>

static EtatCourant etat;

static void installer(Expression expression, const char* texte, bool temporaire,
                      unsigned long maintenantMs, unsigned long dureeMs) {
  etat.expression = expression;
  etat.texte[0] = '\0';
  if (texte != nullptr) {
    strncpy(etat.texte, texte, TEXTE_MAX);
    etat.texte[TEXTE_MAX] = '\0';
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

bool etatRecevoir(TypeEvenement type, const char* texte, unsigned long maintenantMs) {
  // Tickets #6 à #9 : ajouter ici un case par type (waiting, working, music, git_push).
  switch (type) {
    case EV_CLAUDE_DONE:
      installer(EXPR_CONTENT, texte, true, maintenantMs, DUREE_CLAUDE_DONE_MS);
      return true;
    default:
      return false;  // accepté par l'API, pas encore dessiné
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
