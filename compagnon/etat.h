// Module État : logique pure, sans aucune dépendance Arduino (testable sur PC avec g++).
// Reçoit les Événements et l'heure courante (en ms, passée en paramètre) ; décide de
// l'Expression courante, de son texte et de son échéance. Ne dessine rien.
#pragma once

// Durées des Expressions temporaires, regroupées ici (les tickets suivants y ajoutent les leurs).
const unsigned long DUREE_CLAUDE_DONE_MS = 5000;
const unsigned long DUREE_GIT_PUSH_MS = 3000;
const unsigned long DUREE_MUSIC_MS = 10000;  // minimum ; plus si deux défilements durent plus longtemps

// Défilement du texte du Bandeau (music) : police par défaut, 6 px par caractère à la taille 1.
const int LARGEUR_ECRAN_PX = 128;
const int LARGEUR_CHAR_PX = 6;
const int VITESSE_DEFILEMENT_PX_S = 30;
const int ECART_DEFILEMENT_PX = 24;  // blanc entre la fin du texte et sa répétition

const int TEXTE_MAX = 40;  // longueur maximale du texte conservé (le reste est tronqué)

enum TypeEvenement {
  EV_CLAUDE_DONE,
  EV_CLAUDE_WAITING,
  EV_CLAUDE_WORKING,
  EV_MUSIC,
  EV_GIT_PUSH
};

enum Expression {
  EXPR_REPOS,
  EXPR_CONTENT,     // claude_done
  EXPR_INTERPELLE,  // claude_waiting (ticket #7)
  EXPR_MUSIQUE,     // music (ticket #8)
  EXPR_PUSH         // git_push (ticket #9)
};

struct EtatCourant {
  Expression expression;
  char texte[TEXTE_MAX + 1];    // texte du Bandeau ("" = vide)
  char artiste[TEXTE_MAX + 1];  // artiste (music, ticket #8) ; "" si absent
  bool temporaire;            // faux : persistante (ou Repos)
  unsigned long debutMs;      // instant où l'Expression a été installée
  unsigned long dureeMs;      // si temporaire : retour au Repos à debutMs + dureeMs
};

// Traduit le nom d'un type ("claude_done"...) ; faux si le type est inconnu.
bool etatTypeDepuisNom(const char* nom, TypeEvenement* type);

// Remet l'État au Repos.
void etatInit();

// Applique un Événement (le dernier gagne). Renvoie vrai si l'Expression a changé.
// texte et artiste peuvent être nullptr.
bool etatRecevoir(TypeEvenement type, const char* texte, unsigned long maintenantMs,
                  const char* artiste = nullptr);

// À appeler à chaque tour de loop() : renvoie vrai si l'Expression a changé (retour au Repos).
bool etatMiseAJour(unsigned long maintenantMs);

const EtatCourant& etatCourant();

// Clignement du Repos (ticket #6) : sans état, dépend seulement de l'heure.
// Un clignement par cycle de PERIODE_CLIGNEMENT_MS, à un instant pseudo-aléatoire du cycle
// (rendu irrégulier mais déterministe, donc testable).
const unsigned long PERIODE_CLIGNEMENT_MS = 4000;
const unsigned long DUREE_CLIGNEMENT_MS = 150;

// Vrai si les yeux du Repos sont fermés à cet instant.
bool etatYeuxFermes(unsigned long maintenantMs);

// git_push (ticket #9) : la flèche monte du bas au sommet de la Zone visage, en boucle.
const int PUSH_FLECHE_HAUTEUR = 20;
const unsigned long PUSH_CYCLE_MS = 1000;
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
// artiste peut être nullptr.
unsigned long etatDureeMusique(const char* titre, const char* artiste);
