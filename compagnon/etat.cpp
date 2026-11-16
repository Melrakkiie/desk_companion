#include "etat.h"
#include "expressions.h"  // constantes de la Zone visage

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

int etatLargeurTexte(const char* texte) {
  return texte == nullptr ? 0 : (int)strlen(texte) * LARGEUR_CHAR_PX;
}

unsigned long etatPeriodeDefilement(int largeurTextePx) {
  if (largeurTextePx <= LARGEUR_ECRAN_PX) return 0;
  return (unsigned long)(largeurTextePx + ECART_DEFILEMENT_PX) * 1000 / VITESSE_DEFILEMENT_PX_S;
}

int etatPositionDefilement(int largeurTextePx, unsigned long tempsMs) {
  if (largeurTextePx <= LARGEUR_ECRAN_PX) return 0;
  unsigned long cycle = largeurTextePx + ECART_DEFILEMENT_PX;
  return -(int)(tempsMs * VITESSE_DEFILEMENT_PX_S / 1000 % cycle);
}

unsigned long etatDureeMusique(const char* titre, const char* artiste) {
  unsigned long p1 = etatPeriodeDefilement(etatLargeurTexte(titre));
  unsigned long p2 = etatPeriodeDefilement(etatLargeurTexte(artiste));
  unsigned long deux = 2 * (p1 > p2 ? p1 : p2);
  return deux > DUREE_MUSIC_MS ? deux : DUREE_MUSIC_MS;
}

bool etatRecevoir(TypeEvenement type, const char* texte, unsigned long maintenantMs,
                  const char* artiste) {
  switch (type) {
    case EV_CLAUDE_DONE:
      installer(EXPR_CONTENT, texte, true, maintenantMs, DUREE_CLAUDE_DONE_MS);
      return true;
    case EV_GIT_PUSH:
      installer(EXPR_PUSH, texte, true, maintenantMs, DUREE_GIT_PUSH_MS);
      return true;
    case EV_MUSIC:
      installer(EXPR_MUSIQUE, texte, true, maintenantMs, DUREE_MUSIC_MS, artiste);
      // la durée dépend des textes tronqués réellement conservés
      etat.dureeMs = etatDureeMusique(etat.texte, etat.artiste);
      return true;
    // Un case par ticket, à remplir (en attendant : accepté sans effet).
    // Expression persistante : reste jusqu'au prochain Événement.
    // Limite connue : avec plusieurs sessions Claude Code, le dernier Événement gagne
    // (un claude_working d'une session peut effacer le claude_waiting d'une autre).
    case EV_CLAUDE_WAITING:
      installer(EXPR_INTERPELLE, texte, false, maintenantMs, 0);
      return true;
    // Ramène au Repos seulement si on attend ; sinon sans effet (l'API répond quand même 200).
    case EV_CLAUDE_WORKING:
      if (etat.expression != EXPR_INTERPELLE) return false;
      etatInit();
      return true;
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

// Début du clignement dans le cycle : entre 1000 et 3000 ms (hachage du numéro de cycle).
static unsigned long debutClignementDansCycle(unsigned long cycle) {
  unsigned long h = cycle * 2654435761UL;
  h ^= h >> 15;
  return 1000 + (h % 2001);
}

bool etatYeuxFermes(unsigned long maintenantMs) {
  unsigned long cycle = maintenantMs / PERIODE_CLIGNEMENT_MS;
  unsigned long dansCycle = maintenantMs % PERIODE_CLIGNEMENT_MS;
  unsigned long debut = debutClignementDansCycle(cycle);
  return dansCycle >= debut && dansCycle < debut + DUREE_CLIGNEMENT_MS;
}

int etatPushFlecheY(unsigned long tempsEcouleMs) {
  int course = ZONE_VISAGE_HAUTEUR - PUSH_FLECHE_HAUTEUR;  // du bas (course) au haut (0)
  int fait = (int)(tempsEcouleMs % PUSH_CYCLE_MS * course / PUSH_CYCLE_MS);
  return ZONE_VISAGE_Y + course - fait;
}
