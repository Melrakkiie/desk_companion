// Tests du module État sur PC (sans Arduino).
// Lancer depuis la racine du dépôt :  make test
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../compagnon/etat.h"

static void test_repos_au_demarrage() {
  etatInit();
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatCourant().texte[0] == '\0');
}

static void test_claude_done_puis_retour_au_repos_apres_5s() {
  etatInit();
  assert(etatRecevoir(EV_CLAUDE_DONE, "mon-projet", 1000));
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(strcmp(etatCourant().texte, "mon-projet") == 0);

  assert(!etatMiseAJour(5999));  // 4,999 s : toujours content
  assert(etatCourant().expression == EXPR_CONTENT);

  assert(etatMiseAJour(6000));  // 5 s pile : retour au Repos
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatCourant().texte[0] == '\0');
  assert(!etatMiseAJour(7000));  // plus de changement
}

static void test_le_dernier_evenement_gagne() {
  etatInit();
  etatRecevoir(EV_CLAUDE_DONE, "projet-a", 0);
  etatRecevoir(EV_CLAUDE_DONE, "projet-b", 3000);
  assert(strcmp(etatCourant().texte, "projet-b") == 0);
  assert(!etatMiseAJour(5000));  // l'échéance de la 1re requête est oubliée
  assert(etatMiseAJour(8000));   // 3000 + 5000
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_texte_trop_long_tronque() {
  etatInit();
  char long_texte[200];
  memset(long_texte, 'x', sizeof(long_texte));
  long_texte[199] = '\0';
  etatRecevoir(EV_CLAUDE_DONE, long_texte, 0);
  assert(strlen(etatCourant().texte) == (size_t)TEXTE_MAX);
}

static void test_evenement_sans_texte() {
  etatInit();
  etatRecevoir(EV_CLAUDE_DONE, nullptr, 0);
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(etatCourant().texte[0] == '\0');
}

static void test_types_connus_et_inconnus() {
  TypeEvenement t;
  assert(etatTypeDepuisNom("claude_done", &t) && t == EV_CLAUDE_DONE);
  assert(etatTypeDepuisNom("claude_waiting", &t) && t == EV_CLAUDE_WAITING);
  assert(etatTypeDepuisNom("claude_working", &t) && t == EV_CLAUDE_WORKING);
  assert(etatTypeDepuisNom("music", &t) && t == EV_MUSIC);
  assert(etatTypeDepuisNom("git_push", &t) && t == EV_GIT_PUSH);
  assert(!etatTypeDepuisNom("inconnu", &t));
  assert(!etatTypeDepuisNom("", &t));
  assert(!etatTypeDepuisNom(nullptr, &t));
}

static void test_artiste_vide_par_defaut() {
  etatInit();
  assert(etatCourant().artiste[0] == '\0');
  etatRecevoir(EV_CLAUDE_DONE, "projet", 0, "quelqu'un");
  assert(etatCourant().artiste[0] == '\0');  // claude_done ignore l'artiste
}

static void test_repos_clignement_ferme_puis_rouvre() {
  // Sur 20 cycles : yeux ouverts au début de cycle, un seul clignement de la bonne durée par cycle
  unsigned long debuts[20];
  for (unsigned long c = 0; c < 20; c++) {
    unsigned long base = c * PERIODE_CLIGNEMENT_MS;
    assert(!etatYeuxFermes(base));
    unsigned long fermes = 0, premier = 0;
    for (unsigned long t = 0; t < PERIODE_CLIGNEMENT_MS; t++) {
      if (etatYeuxFermes(base + t)) {
        if (fermes == 0) premier = t;
        fermes++;
      }
    }
    assert(fermes == DUREE_CLIGNEMENT_MS);
    assert(premier >= 1000 && premier <= 3000);
    debuts[c] = premier;
  }
  bool irregulier = false;
  for (int c = 1; c < 20; c++) irregulier |= (debuts[c] != debuts[0]);
  assert(irregulier);
}

int main() {
  test_repos_au_demarrage();
  test_claude_done_puis_retour_au_repos_apres_5s();
  test_le_dernier_evenement_gagne();
  test_texte_trop_long_tronque();
  test_evenement_sans_texte();
  test_types_connus_et_inconnus();
  test_artiste_vide_par_defaut();
  test_repos_clignement_ferme_puis_rouvre();
  puts("Tous les tests passent.");
  return 0;
}
