#include "etat.h"
#include <stdio.h>
#include <string.h>

static EtatCourant etat;  // l'Expression courante, que le Rappel recouvre sans la toucher

// Rappel (appui sur le Bouton) et Morceau connu : le dernier music reçu, en pause compris (copie de etat) ;
// seul un Morceau connu non en pause est le Morceau courant (voir GLOSSARY.md).
static EtatCourant rappel;
static bool rappelActif = false;
static EtatCourant morceau;
static bool morceauConnu = false;
static bool morceauEnPause = false;  // connu mais plus courant (music_stop)
static unsigned long nouvellesMusiqueMs = 0;  // dernier music ou music_stop reçu

// Sans nouvelles depuis OUBLI_MORCEAU_MS : le morceau n'est plus connu (le prochain music s'affiche).
static void oublierSiPerime(unsigned long maintenantMs) {
  if (morceauConnu && maintenantMs - nouvellesMusiqueMs >= OUBLI_MORCEAU_MS) morceauConnu = false;
}

// Jeu courant : le dernier game reçu, tant qu'il n'est ni arrêté (game_stop) ni oublié.
static char jeuNom[TEXTE_MAX + 1];  // déjà en CP437, une seule ligne
static bool jeuConnu = false;
static unsigned long nouvellesJeuMs = 0;  // dernier game reçu

// Sommeil : instant du dernier Événement (tout type) ; endormi = point de retour Endormi.
static unsigned long dernierEvenementMs = 0;
static bool endormi = false;
static bool wifiPrecedent = true;

// Table des Sessions Claude Code : identifiant, État Claude, texte brut du dernier Événement
// (« libellé\nprojet »), instant du dernier changement. La Session anonyme a l'identifiant "".
// Pleine, la plus ancienne (changementMs minimal) cède la place à une nouvelle Session.
struct Session {
  bool utilisee;
  char id[SESSION_ID_MAX + 1];
  EtatClaude etat;
  char texte[TEXTE_MAX * 2 + 2];  // les deux lignes brutes ; tronqué ensuite par installer
  unsigned long changementMs;
};
static Session sessions[SESSIONS_MAX];

// Session déjà connue, ou nullptr (ne crée rien).
static Session* sessionConnue(const char* id) {
  if (id == nullptr) id = "";
  for (Session& s : sessions)
    if (s.utilisee && strncmp(s.id, id, SESSION_ID_MAX) == 0) return &s;
  return nullptr;
}

static Session& sessionPour(const char* id, unsigned long maintenantMs) {
  if (id == nullptr) id = "";
  if (Session* connue = sessionConnue(id)) return *connue;
  Session* libre = nullptr;
  Session* ancienne = &sessions[0];
  for (Session& s : sessions) {
    if (!s.utilisee) { if (libre == nullptr) libre = &s; continue; }
    if (s.changementMs < ancienne->changementMs) ancienne = &s;
  }
  Session& s = libre != nullptr ? *libre : *ancienne;
  s.utilisee = true;
  strncpy(s.id, id, SESSION_ID_MAX);
  s.id[SESSION_ID_MAX] = '\0';
  s.etat = CLAUDE_FINI;
  s.texte[0] = '\0';
  s.changementMs = maintenantMs;
  return s;
}

// Met la Session dans cet État Claude, à cet instant, avec le texte brut de l'Événement (pour le Rappel Claude).
static void majSession(Session& s, EtatClaude etat, const char* texte, unsigned long maintenantMs) {
  s.etat = etat;
  s.changementMs = maintenantMs;
  strncpy(s.texte, texte == nullptr ? "" : texte, sizeof(s.texte) - 1);
  s.texte[sizeof(s.texte) - 1] = '\0';
}

// Session dans cet État Claude depuis le plus récent changement, ou nullptr. Égalité d'instant : la dernière de la table.
static const Session* laPlusRecente(EtatClaude etat) {
  const Session* res = nullptr;
  for (const Session& s : sessions)
    if (s.utilisee && s.etat == etat && (res == nullptr || s.changementMs >= res->changementMs)) res = &s;
  return res;
}

static const Session* attenteLaPlusRecente() {
  return laPlusRecente(CLAUDE_ATTEND);
}

// Anti-rebond d'un Bouton : niveau accepté et instant du dernier changement accepté.
struct Bouton {
  bool enfonce;
  bool changement;  // un changement accepté a déjà eu lieu
  unsigned long changementMs;
};
static Bouton boutonMusique, boutonClaude;

// Vrai sur un front d'appui accepté (anti-rebond passé) ; un relâchement ou un niveau inchangé renvoie faux.
static bool frontAppui(Bouton& b, bool enfonce, unsigned long maintenantMs) {
  if (enfonce == b.enfonce) return false;
  if (b.changement && maintenantMs - b.changementMs < ANTI_REBOND_MS) return false;
  b.enfonce = enfonce;
  b.changement = true;
  b.changementMs = maintenantMs;
  return enfonce;
}

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
  if (son == SON_CLOCHETTE) { *n = sizeof(MELODIE_CLOCHETTE) / sizeof(Note); return MELODIE_CLOCHETTE; }
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

// Les deux lignes du Bandeau en CP437 : ligne 1 = texte jusqu'au premier '\n' ; ligne 2 = le texte
// après ce '\n', sinon l'artiste. texte et artiste peuvent être nullptr.
static void lignesBandeau(const char* texte, const char* artiste, char* l1, char* l2) {
  etatVersCp437(texte, l1, TEXTE_MAX + 1);
  const char* saut = texte == nullptr ? nullptr : strchr(texte, '\n');
  etatVersCp437(saut != nullptr ? saut + 1 : artiste, l2, TEXTE_MAX + 1);
}

static const char* const LIBELLE_FINI = "Done!";  // ligne 1 du Bandeau d'une Session qui a fini

static void installer(Expression expression, const char* texte, bool temporaire,
                      unsigned long maintenantMs, unsigned long dureeMs,
                      const char* artiste = nullptr) {
  etat.expression = expression;
  lignesBandeau(texte, artiste, etat.texte, etat.ligne2);
  etat.temporaire = temporaire;
  etat.debutMs = maintenantMs;
  etat.dureeMs = dureeMs;
}

// Retour au Repos : l'Expression change, le Son en cours n'est pas touché.
static void repos() {
  installer(EXPR_REPOS, nullptr, false, 0, 0);
}

// Jeu : ni temporaire ni persistante, c'est le point de retour ; nom du jeu seul dans le Bandeau (jeuNom est déjà en CP437).
static void jeu(unsigned long maintenantMs) {
  installer(EXPR_JEU, nullptr, false, maintenantMs, 0);
  strcpy(etat.texte, jeuNom);
}

// Point de retour : interpelle (Session qui attend la plus récente) > Jeu > Repos.
static void retour(unsigned long maintenantMs) {
  const Session* attente = attenteLaPlusRecente();
  if (attente != nullptr) installer(EXPR_INTERPELLE, attente->texte, false, maintenantMs, 0);
  else if (jeuConnu) jeu(maintenantMs);
  else if (endormi) installer(EXPR_ENDORMI, nullptr, false, maintenantMs, 0);
  else repos();
}

// Oublie la Session (claude_end ou oubli après OUBLI_SESSION_MS) ; si c'était la Session affichée,
// passage au point de retour, sans carillon. Vrai si l'Expression a changé.
static bool oublierSession(Session& s, unsigned long maintenantMs) {
  bool affichee = s.etat == CLAUDE_ATTEND && attenteLaPlusRecente() == &s;
  s.utilisee = false;
  if (!affichee || etat.expression != EXPR_INTERPELLE) return false;
  retour(maintenantMs);
  return true;
}

// Oublie le Jeu courant (game_stop ou oubli après OUBLI_JEU_MS) ; vrai si l'Expression Jeu cède la place au Repos.
static bool oublierJeu() {
  jeuConnu = false;
  if (etat.expression != EXPR_JEU) return false;
  repos();
  return true;
}

void etatInit(bool boutonMusiqueEnfonceAuDemarrage, bool boutonClaudeEnfonceAuDemarrage) {
  repos();
  sonCourant = SON_AUCUN;
  rappelActif = false;
  morceauConnu = false;
  morceauEnPause = false;
  jeuConnu = false;
  endormi = false;
  wifiPrecedent = true;
  dernierEvenementMs = 0;
  memset(sessions, 0, sizeof(sessions));
  // tenu au démarrage : pas de front d'appui
  boutonMusique = {boutonMusiqueEnfonceAuDemarrage, false, 0};
  boutonClaude = {boutonClaudeEnfonceAuDemarrage, false, 0};
}

bool etatTypeDepuisNom(const char* nom, TypeEvenement* type) {
  static const char* const NOMS[] = {"claude_done", "claude_waiting", "claude_working", "music", "git_push", "music_stop", "game", "game_stop", "claude_end"};
  if (nom == nullptr) return false;
  for (unsigned i = 0; i < sizeof(NOMS) / sizeof(*NOMS); i++) {
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

static bool recevoir(TypeEvenement type, const char* texte, unsigned long maintenantMs,
                     const char* artiste, const char* session) {
  switch (type) {
    case EV_CLAUDE_DONE: {
      Session& s = sessionPour(session, maintenantMs);
      majSession(s, CLAUDE_FINI, texte, maintenantMs);
      demarrerSon(SON_CLOCHETTE, maintenantMs);
      installer(EXPR_CONTENT, LIBELLE_FINI, true, maintenantMs, DUREE_CLAUDE_DONE_MS, texte);  // projet en ligne 2
      return true;
    }
    case EV_GIT_PUSH:
      installer(EXPR_PUSH, texte, true, maintenantMs, DUREE_GIT_PUSH_MS);
      demarrerSon(SON_FANFARE, maintenantMs);
      return true;
    case EV_MUSIC: {
      // Même morceau que le connu (reprise après pause) : redevient courant, sans réaffichage.
      oublierSiPerime(maintenantMs);
      nouvellesMusiqueMs = maintenantMs;
      char l1[TEXTE_MAX + 1], l2[TEXTE_MAX + 1];
      lignesBandeau(texte, artiste, l1, l2);
      if (morceauConnu && strcmp(l1, morceau.texte) == 0 && strcmp(l2, morceau.ligne2) == 0) {
        morceauEnPause = false;
        return false;
      }
      installer(EXPR_MUSIQUE, texte, true, maintenantMs, DUREE_MUSIC_MS, artiste);
      // la durée dépend des textes tronqués réellement conservés
      etat.dureeMs = etatDureeMusique(etat.texte, etat.ligne2);
      morceau = etat;
      morceauConnu = true;
      morceauEnPause = false;
      return true;
    }
    // Pause ou arrêt : le morceau reste connu mais n'est plus courant ; l'Expression ne change pas.
    case EV_MUSIC_STOP:
      oublierSiPerime(maintenantMs);
      nouvellesMusiqueMs = maintenantMs;
      morceauEnPause = true;
      return false;
    // Persistante : la Session attend jusqu'à son propre claude_working ou claude_done.
    case EV_CLAUDE_WAITING: {
      Session& s = sessionPour(session, maintenantMs);
      majSession(s, CLAUDE_ATTEND, texte, maintenantMs);
      installer(EXPR_INTERPELLE, s.texte, false, maintenantMs, 0);
      demarrerSon(SON_CARILLON, maintenantMs);
      return true;
    }
    // Change l'affichage seulement si c'est la Session affichée qui reprend ; passe alors au point
    // de retour (autre Session qui attend, Jeu, Repos), sans carillon. Sinon sans effet (l'API répond 200).
    case EV_CLAUDE_WORKING: {
      Session& s = sessionPour(session, maintenantMs);
      bool affichee = s.etat == CLAUDE_ATTEND && attenteLaPlusRecente() == &s;
      majSession(s, CLAUDE_TRAVAILLE, texte, maintenantMs);  // texte gardé pour le Rappel Claude, pas d'affichage direct
      if (!affichee || etat.expression != EXPR_INTERPELLE) return false;
      retour(maintenantMs);
      return true;
    }
    // Ne changent que le point de retour ; Jeu ne s'affiche tout de suite qu'au Repos (ou sur place).
    case EV_GAME: {
      char nom[TEXTE_MAX + 1];
      etatVersCp437(texte, nom, TEXTE_MAX + 1);
      bool memeNom = jeuConnu && strcmp(nom, jeuNom) == 0;
      strcpy(jeuNom, nom);
      jeuConnu = true;
      nouvellesJeuMs = maintenantMs;
      if (etat.expression == EXPR_REPOS || (etat.expression == EXPR_JEU && !memeNom)) {
        jeu(maintenantMs);
        return true;
      }
      return false;
    }
    case EV_GAME_STOP:
      return oublierJeu();
    // Oublie la Session (inconnue : sans effet, sans lui prendre de place) ; change l'affichage
    // seulement si c'était la Session affichée : passage au point de retour, sans carillon.
    case EV_CLAUDE_END: {
      Session* s = sessionConnue(session);
      return s != nullptr && oublierSession(*s, maintenantMs);
    }
  }
  return false;  // inatteignable : tous les types sont traités ci-dessus
}

// Un Événement qui change l'Expression met fin au Rappel ; sans effet (false), il ne l'interrompt pas.
bool etatRecevoir(TypeEvenement type, const char* texte, unsigned long maintenantMs,
                  const char* artiste, const char* session) {
  // N'importe quel Événement remet le compte à zéro et réveille (sans Son) avant de s'appliquer.
  dernierEvenementMs = maintenantMs;
  bool reveil = endormi;
  endormi = false;
  if (etat.expression == EXPR_ENDORMI) repos();
  bool change = recevoir(type, texte, maintenantMs, artiste, session) || (reveil && !rappelActif);
  if (type == EV_GAME || type == EV_GAME_STOP) return change && !rappelActif;  // le Rappel continue
  if (change) rappelActif = false;
  return change;
}

bool etatMiseAJour(unsigned long maintenantMs, bool wifiConnecte) {
  oublierSiPerime(maintenantMs);
  bool visible = false;
  // Sans Wi-Fi le Compagnon n'entend rien : pas de Sommeil, et le compte repart au retour du Wi-Fi.
  if (!wifiConnecte || !wifiPrecedent) dernierEvenementMs = maintenantMs;
  wifiPrecedent = wifiConnecte;
  if (jeuConnu && maintenantMs - nouvellesJeuMs >= OUBLI_JEU_MS) {  // même effet qu'un game_stop
    if (oublierJeu()) visible = !rappelActif;
  }
  for (Session& s : sessions)  // même effet qu'un claude_end (Session tuée sans SessionEnd)
    if (s.utilisee && maintenantMs - s.changementMs >= OUBLI_SESSION_MS && oublierSession(s, maintenantMs))
      visible = !rappelActif;
  if (!endormi && maintenantMs - dernierEvenementMs >= DELAI_SOMMEIL_MS) {
    endormi = true;  // avant l'oubli : le retour() d'une Session affichée donne alors Endormi
    Expression avant = etat.expression;
    for (Session& s : sessions)
      if (s.utilisee) oublierSession(s, maintenantMs);
    if (!etat.temporaire && etat.expression != EXPR_ENDORMI) retour(maintenantMs);  // une Expression temporaire finit sa durée
    if (etat.expression != avant) visible = !rappelActif;
  }
  if (rappelActif && maintenantMs - rappel.debutMs >= rappel.dureeMs) {
    rappelActif = false;
    visible = true;
  }
  if (etat.temporaire && maintenantMs - etat.debutMs >= etat.dureeMs) {
    retour(maintenantMs);
    if (!rappelActif) visible = true;  // sous un Rappel, le changement n'est pas encore visible
  }
  return visible;
}

bool etatBouton(bool enfonce, unsigned long maintenantMs) {
  if (!frontAppui(boutonMusique, enfonce, maintenantMs)) return false;

  oublierSiPerime(maintenantMs);
  if (morceauConnu && !morceauEnPause) {
    rappel = morceau;
  } else {
    rappel.expression = EXPR_SILENCE;
    etatVersCp437("pas de musique", rappel.texte, TEXTE_MAX + 1);
    rappel.ligne2[0] = '\0';
    rappel.dureeMs = DUREE_SILENCE_MS;
  }
  rappel.temporaire = true;
  rappel.debutMs = maintenantMs;
  rappelActif = true;
  return true;
}

// Rappel Claude : l'État Claude le plus urgent. Priorité : attend > travaille > a fini > Sans Claude.
bool etatBoutonClaude(bool enfonce, unsigned long maintenantMs) {
  if (!frontAppui(boutonClaude, enfonce, maintenantMs)) return false;

  const Session* s = attenteLaPlusRecente();
  rappel.expression = EXPR_INTERPELLE;
  if (s == nullptr) { s = laPlusRecente(CLAUDE_TRAVAILLE); rappel.expression = EXPR_TRAVAILLE; }
  if (s == nullptr) { s = laPlusRecente(CLAUDE_FINI); rappel.expression = EXPR_CONTENT; }
  int utilisees = 0;
  for (const Session& t : sessions) if (t.utilisee) utilisees++;
  if (s == nullptr) {
    rappel.expression = EXPR_SANS_CLAUDE;
    etatVersCp437("pas de Claude", rappel.texte, TEXTE_MAX + 1);
    rappel.ligne2[0] = '\0';
  } else {
    if (rappel.expression != EXPR_INTERPELLE) {  // le texte d'une Session qui travaille ou a fini est son projet : ligne 2
      etatVersCp437(rappel.expression == EXPR_TRAVAILLE ? "Working" : LIBELLE_FINI, rappel.texte, TEXTE_MAX + 1);
      etatVersCp437(s->texte, rappel.ligne2, TEXTE_MAX + 1);
    } else {
      lignesBandeau(s->texte, nullptr, rappel.texte, rappel.ligne2);
    }
    static_assert(SESSIONS_MAX <= 9, "« N sessions » doit tenir sur un chiffre (10 glyphes)");
    if (utilisees > 1 && utilisees <= 9) {  // borne redondante avec le static_assert, pour gcc
      // « │ N sessions » au bord droit de la ligne 1 (bourrage d'espaces) ; à l'étroit, sans l'espace après « │ » ;
      // trop long : « │ N sessions » à la suite, et la ligne défile. « │ » = 0xB3 en CP437 (le « | » de la police est brisé).
      const int GLYPHES = LARGEUR_ECRAN_PX / LARGEUR_CHAR_PX;  // ligne pleine sans défilement
      int n = (int)strlen(rappel.texte);
      if (n > TEXTE_MAX - 13) n = TEXTE_MAX - 13;  // 13 = strlen(" │ N sessions")
      int debut = n;
      const char* format = " \xB3 %d sessions";
      if (n + 12 <= GLYPHES) { debut = GLYPHES - 12; format = "\xB3 %d sessions"; }
      else if (n + 11 <= GLYPHES) format = "\xB3%d sessions";
      memset(rappel.texte + n, ' ', debut - n);
      snprintf(rappel.texte + debut, 14, format, utilisees);
    }
  }
  rappel.dureeMs = DUREE_RAPPEL_CLAUDE_MS;
  rappel.temporaire = true;
  rappel.debutMs = maintenantMs;
  rappelActif = true;
  return true;
}

const EtatCourant& etatCourant() {
  return rappelActif ? rappel : etat;
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
