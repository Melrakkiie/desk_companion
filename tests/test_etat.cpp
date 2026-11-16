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

int main() {
  test_repos_au_demarrage();
  test_claude_done_puis_retour_au_repos_apres_5s();
  puts("Tous les tests passent.");
  return 0;
}
