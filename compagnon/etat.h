// Module État : logique pure, sans aucune dépendance Arduino (testable sur PC avec g++).
// Reçoit les Événements et l'heure courante (en ms, passée en paramètre) ; décide de
// l'Expression courante, de son texte et de son échéance. Ne dessine rien.
#pragma once

// ---- Toutes les durées et cadences, regroupées ici ----
// (hors module : DELAI_NOUVELLE_TENTATIVE_MS dans reseau.cpp et le délai curl du hook,
// qui relèvent de leur couche.)
const unsigned long DUREE_CLAUDE_DONE_MS = 5000;
const unsigned long DUREE_GIT_PUSH_MS = 3000;
const unsigned long DUREE_MUSIC_MS = 10000;  // minimum ; plus si deux défilements durent plus longtemps
const unsigned long DUREE_SILENCE_MS = 3000;  // Rappel silence (aucun Morceau courant)
// Le PC renvoie l'état de lecture (music ou music_stop) toutes les 10 s (hook, hors module) ;
// sans aucune nouvelle pendant 30 s, le Compagnon oublie le Morceau courant (3 renvois manqués).
const unsigned long OUBLI_MORCEAU_MS = 30000;
// Même principe pour le Jeu courant : le PC renvoie game ou game_stop toutes les 10 s (hors module).
const unsigned long OUBLI_JEU_MS = 30000;
const unsigned long ANTI_REBOND_MS = 50;  // Bouton : changements de niveau ignorés après un changement accepté
const unsigned long PERIODE_ANIM_MS = 33;    // redessin périodique (~30 images/s)
// Clignement du Repos : un par cycle, à un instant irrégulier du cycle.
const unsigned long PERIODE_CLIGNEMENT_MS = 4000;
const unsigned long DUREE_CLIGNEMENT_MS = 150;
const unsigned long CLIGNEMENT_DEBUT_MIN_MS = 1000;    // début du clignement : entre 1000 ms
const unsigned long CLIGNEMENT_DEBUT_PLAGE_MS = 2001;  // et 3000 ms (2001 valeurs possibles)
const unsigned long PUSH_CYCLE_MS = 1000;         // git_push : une montée de la flèche
const unsigned long NOTE_REBOND_PERIODE_MS = 250; // music : les notes changent de position

// Mélodies des Sons : {fréquence Hz, durée ms}, fréquence 0 = silence.
// Faciles à remplacer : coller ici ses propres notes (le reste suit tout seul).
struct Note {
  unsigned int frequenceHz;
  unsigned long dureeMs;
};

// Fanfare (git_push) : intro de la fanfare de victoire trouvée en ligne, ~3 s.
// Une octave au-dessus du sketch d'origine (262 → 523 Hz, etc.), plus audible sur le buzzer.
// Durées converties du sketch Arduino : 0,85 × 1000 / valeur (6 → 141, 2 → 425, 1 → 850 ms).
// Notes identiques consécutives séparées par 30 ms de silence (pris sur la note), sinon le buzzer les fond en une.
const Note MELODIE_FANFARE[] = {
  {523, 111}, {0, 30}, {523, 111}, {0, 30}, {523, 111}, {0, 30}, {523, 425},
  {415, 425}, {466, 425}, {523, 141}, {0, 141}, {466, 141}, {523, 850},
};
// Carillon (claude_waiting) : ding-dong descendant, < 0,5 s.
const Note MELODIE_CARILLON[] = {
  {988, 150}, {0, 30}, {784, 250},
};
// Clochette (claude_done) : trois notes montantes Mi-Sol-Do aigu, < 0,5 s (le carillon descend).
const Note MELODIE_CLOCHETTE[] = {
  {1319, 100}, {1568, 100}, {2093, 150},
};

// Géométrie de l'écran mesurée sur le matériel (ticket #3) : la bande jaune du haut est le Bandeau.
const int BANDEAU_HAUTEUR = 16;     // Bandeau = lignes 0 à 15
const int ZONE_VISAGE_Y = 16;       // Zone visage = lignes 16 à 63
const int ZONE_VISAGE_HAUTEUR = 48;

// Défilement des lignes du Bandeau (toutes les Expressions) : police par défaut, 6 px par caractère à la taille 1.
const int LARGEUR_ECRAN_PX = 128;
const int LARGEUR_CHAR_PX = 6;
const int VITESSE_DEFILEMENT_PX_S = 30;
const int ECART_DEFILEMENT_PX = 24;  // blanc entre la fin du texte et sa répétition

const int TEXTE_MAX = 100;  // longueur maximale de chaque ligne conservée (le reste est tronqué)

enum TypeEvenement {
  EV_CLAUDE_DONE,
  EV_CLAUDE_WAITING,
  EV_CLAUDE_WORKING,
  EV_MUSIC,
  EV_GIT_PUSH,
  EV_MUSIC_STOP,
  EV_GAME,       // texte = nom du jeu
  EV_GAME_STOP
};

enum Expression {
  EXPR_REPOS,
  EXPR_CONTENT,     // claude_done
  EXPR_INTERPELLE,  // claude_waiting (ticket #7)
  EXPR_MUSIQUE,     // music (ticket #8)
  EXPR_PUSH,        // git_push (ticket #9)
  EXPR_JEU,         // Jeu courant : point de retour à la place du Repos
  EXPR_SILENCE      // Rappel sans Morceau courant : jamais issue d'un Événement
};

struct EtatCourant {
  Expression expression;
  char texte[TEXTE_MAX + 1];   // ligne 1 du Bandeau ("" = vide) : le texte jusqu'au premier '\n'
  char ligne2[TEXTE_MAX + 1];  // ligne 2 : le texte après ce '\n', sinon l'artiste (music) ; "" si absente
  bool temporaire;            // faux : persistante (ou Repos)
  unsigned long debutMs;      // instant où l'Expression a été installée
  unsigned long dureeMs;      // si temporaire : retour au Repos à debutMs + dureeMs
};

// Convertit un texte UTF-8 en CP437 (police de l'écran) : 1 octet = 1 glyphe affiché, donc
// largeurs et défilement comptent des glyphes. Caractère absent de CP437 -> '?'.
// Écrit au plus taille-1 glyphes puis '\0'. S'arrête au premier '\n'. utf8 peut être nullptr (sortie vide).
void etatVersCp437(const char* utf8, char* sortie, int taille);

// Traduit le nom d'un type ("claude_done"...) ; faux si le type est inconnu.
bool etatTypeDepuisNom(const char* nom, TypeEvenement* type);

enum Son { SON_AUCUN, SON_FANFARE, SON_CARILLON, SON_CLOCHETTE };

// Durée totale d'un Son (ms) ; 0 pour SON_AUCUN.
unsigned long etatDureeSon(Son son);

// Fréquence (Hz) à jouer pour ce Son après tempsEcouleMs ; 0 = silence ou Son terminé.
unsigned int etatFrequenceSon(Son son, unsigned long tempsEcouleMs);

// Son en cours (ou terminé : seule la fréquence 0 le dit) ; démarré par claude_done, claude_waiting et git_push,
// jamais interrompu par les autres Événements.
Son etatSonCourant();

// Fréquence à jouer maintenant pour le Son courant (0 = silence).
unsigned int etatFrequence(unsigned long maintenantMs);

// Remet l'État au Repos et le Son à SON_AUCUN. boutonEnfonceAuDemarrage : niveau du Bouton à cet
// instant ; un Bouton déjà tenu ne montre pas de Rappel (il faut le relâcher puis le renfoncer).
void etatInit(bool boutonEnfonceAuDemarrage = false);

// État Claude d'une Session (voir GLOSSARY.md).
enum EtatClaude { CLAUDE_ATTEND, CLAUDE_TRAVAILLE, CLAUDE_FINI };

const int SESSIONS_MAX = 8;       // taille de la table des Sessions ; la plus ancienne cède la place
const int SESSION_ID_MAX = 40;    // longueur conservée de l'identifiant (le reste est tronqué)

// Applique un Événement. Renvoie vrai si l'Expression a changé.
// texte, artiste et session peuvent être nullptr ; session = identifiant de la Session Claude Code
// (claude_*), nullptr = Session anonyme unique. Seuls les Événements de la même Session changent son État Claude.
bool etatRecevoir(TypeEvenement type, const char* texte, unsigned long maintenantMs,
                  const char* artiste = nullptr, const char* session = nullptr);

// À appeler à chaque tour de loop() : renvoie vrai si l'Expression a changé (retour au Repos).
bool etatMiseAJour(unsigned long maintenantMs);

// Bouton : à appeler à chaque tour de loop() avec le niveau brut (vrai = enfoncé) et l'heure.
// Anti-rebond et front d'appui sont ici. L'enfoncement montre un Rappel (musique du Morceau
// courant, sinon silence) ; un bouton maintenu ou qui rebondit n'en montre qu'un seul.
// Renvoie vrai si l'affichage change.
bool etatBouton(bool enfonce, unsigned long maintenantMs);

// Expression à afficher : le Rappel s'il y en a un, sinon l'Expression courante.
const EtatCourant& etatCourant();

// Clignement du Repos (ticket #6) : sans état, dépend seulement de l'heure.
// Un clignement par cycle de PERIODE_CLIGNEMENT_MS, à un instant pseudo-aléatoire du cycle
// (rendu irrégulier mais déterministe, donc testable).

// Vrai si les yeux du Repos sont fermés à cet instant.
bool etatYeuxFermes(unsigned long maintenantMs);

// git_push (ticket #9) : la flèche monte du bas au sommet de la Zone visage, en boucle.
const int PUSH_FLECHE_HAUTEUR = 20;
// Ordonnée du sommet de la flèche après tempsEcouleMs d'animation (toujours dans la Zone visage).
int etatPushFlecheY(unsigned long tempsEcouleMs);

// Largeur en pixels d'une ligne de texte (police fixe, sans GFX).
int etatLargeurTexte(const char* texte);

// Durée d'un défilement complet (ms) d'une ligne de cette largeur ; 0 si elle tient à l'écran.
unsigned long etatPeriodeDefilement(int largeurTextePx);

// Décalage horizontal (<= 0) de la ligne à l'instant tempsMs depuis le début ; 0 si elle tient.
// La ligne se répète à +largeur+ECART_DEFILEMENT_PX pour boucler sans saut.
int etatPositionDefilement(int largeurTextePx, unsigned long tempsMs);

// Durée de l'Expression music : max(10 s, deux défilements de la ligne la plus longue).
// ligne2 peut être nullptr.
unsigned long etatDureeMusique(const char* ligne1, const char* ligne2);
