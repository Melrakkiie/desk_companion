#include "etat.h"
#include <string.h>

static EtatCourant etat;

// Lettres non ASCII disponibles en CP437 : {point de code Unicode, octet CP437}.
static const struct { unsigned int unicode; unsigned char cp437; } TABLE_CP437[] = {
  {0xC7, 0x80}, {0xFC, 0x81}, {0xE9, 0x82}, {0xE2, 0x83}, {0xE4, 0x84}, {0xE0, 0x85},
  {0xE5, 0x86}, {0xE7, 0x87}, {0xEA, 0x88}, {0xEB, 0x89}, {0xE8, 0x8A}, {0xEF, 0x8B},
  {0xEE, 0x8C}, {0xEC, 0x8D}, {0xC4, 0x8E}, {0xC5, 0x8F}, {0xC9, 0x90}, {0xE6, 0x91},
  {0xC6, 0x92}, {0xF4, 0x93}, {0xF6, 0x94}, {0xF2, 0x95}, {0xFB, 0x96}, {0xF9, 0x97},
  {0xFF, 0x98}, {0xD6, 0x99}, {0xDC, 0x9A}, {0xA2, 0x9B}, {0xA3, 0x9C}, {0xA5, 0x9D},
  {0xE1, 0xA0}, {0xED, 0xA1}, {0xF3, 0xA2}, {0xFA, 0xA3}, {0xF1, 0xA4}, {0xD1, 0xA5},
  {0xAA, 0xA6}, {0xBA, 0xA7}, {0xBF, 0xA8}, {0xA1, 0xAD}, {0xAB, 0xAE}, {0xBB, 0xAF},
  {0xDF, 0xE1}, {0xB0, 0xF8},
};

void etatVersCp437(const char* utf8, char* sortie, int taille) {
  int n = 0;
  const unsigned char* p = (const unsigned char*)(utf8 == nullptr ? "" : utf8);
  while (*p != '\0' && *p != '\n' && n < taille - 1) {
    unsigned int c = *p++;
    int suite = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (c >= 0xC0) c &= 0x3F >> suite;
    for (int i = 0; i < suite && (*p & 0xC0) == 0x80; i++) c = (c << 6) | (*p++ & 0x3F);
    unsigned char o = (c >= 0x20 && c <= 0x7E) ? (unsigned char)c : '?';
    for (const auto& e : TABLE_CP437) if (e.unicode == c) o = e.cp437;
    sortie[n++] = (char)o;
  }
  sortie[n] = '\0';
}

static Son sonCourant = SON_AUCUN;
static unsigned long sonDebutMs = 0;

static void demarrerSon(Son son, unsigned long maintenantMs) {
  sonCourant = son;
  sonDebutMs = maintenantMs;
}

static const Note* melodie(Son son, int* n) {
  if (son == SON_FANFARE) { *n = sizeof(MELODIE_FANFARE) / sizeof(Note); return MELODIE_FANFARE; }
  if (son == SON_CARILLON) { *n = sizeof(MELODIE_CARILLON) / sizeof(Note); return MELODIE_CARILLON; }
  *n = 0;
  return nullptr;
}

unsigned long etatDureeSon(Son son) {
  int n;
  const Note* notes = melodie(son, &n);
  unsigned long total = 0;
  for (int i = 0; i < n; i++) total += notes[i].dureeMs;
  return total;
}

unsigned int etatFrequenceSon(Son son, unsigned long tempsEcouleMs) {
  int n;
  const Note* notes = melodie(son, &n);
  for (int i = 0; i < n; i++) {
    if (tempsEcouleMs < notes[i].dureeMs) return notes[i].frequenceHz;
    tempsEcouleMs -= notes[i].dureeMs;
  }
  return 0;
}

Son etatSonCourant() {
  return sonCourant;
}

unsigned int etatFrequence(unsigned long maintenantMs) {
  return etatFrequenceSon(sonCourant, maintenantMs - sonDebutMs);
}

static void installer(Expression expression, const char* texte, bool temporaire,
                      unsigned long maintenantMs, unsigned long dureeMs,
                      const char* artiste = nullptr) {
  etat.expression = expression;
  etatVersCp437(texte, etat.texte, TEXTE_MAX + 1);
  // Ligne 2 : le texte après le premier '\n', sinon l'artiste
  const char* saut = texte == nullptr ? nullptr : strchr(texte, '\n');
  etatVersCp437(saut != nullptr ? saut + 1 : artiste, etat.ligne2, TEXTE_MAX + 1);
  etat.temporaire = temporaire;
  etat.debutMs = maintenantMs;
  etat.dureeMs = dureeMs;
}

// Retour au Repos : l'Expression change, le Son en cours n'est pas touché.
static void repos() {
  installer(EXPR_REPOS, nullptr, false, 0, 0);
}

void etatInit() {
  repos();
  sonCourant = SON_AUCUN;
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

unsigned long etatDureeMusique(const char* ligne1, const char* ligne2) {
  unsigned long p1 = etatPeriodeDefilement(etatLargeurTexte(ligne1));
  unsigned long p2 = etatPeriodeDefilement(etatLargeurTexte(ligne2));
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
      demarrerSon(SON_FANFARE, maintenantMs);
      return true;
    case EV_MUSIC:
      installer(EXPR_MUSIQUE, texte, true, maintenantMs, DUREE_MUSIC_MS, artiste);
      // la durée dépend des textes tronqués réellement conservés
      etat.dureeMs = etatDureeMusique(etat.texte, etat.ligne2);
      return true;
    // Expression persistante : reste jusqu'au prochain Événement.
    // Limite connue : avec plusieurs sessions Claude Code, le dernier Événement gagne
    // (un claude_working d'une session peut effacer le claude_waiting d'une autre).
    case EV_CLAUDE_WAITING:
      installer(EXPR_INTERPELLE, texte, false, maintenantMs, 0);
      demarrerSon(SON_CARILLON, maintenantMs);
      return true;
    // Ramène au Repos seulement si on attend ; sinon sans effet (l'API répond quand même 200).
    case EV_CLAUDE_WORKING:
      if (etat.expression != EXPR_INTERPELLE) return false;
      repos();
      return true;
  }
  return false;  // inatteignable : tous les types sont traités ci-dessus
}

bool etatMiseAJour(unsigned long maintenantMs) {
  if (etat.temporaire && maintenantMs - etat.debutMs >= etat.dureeMs) {
    repos();
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
  return CLIGNEMENT_DEBUT_MIN_MS + (h % CLIGNEMENT_DEBUT_PLAGE_MS);
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
