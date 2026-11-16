// Module État : logique pure, sans aucune dépendance Arduino (testable sur PC avec g++).
// Reçoit les Événements et l'heure courante (en ms, passée en paramètre) ; décide de
// l'Expression courante, de son texte et de son échéance. Ne dessine rien.
#pragma once

// Durées des Expressions temporaires, regroupées ici (les tickets suivants y ajoutent les leurs).
const unsigned long DUREE_CLAUDE_DONE_MS = 5000;

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
