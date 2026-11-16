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

static void test_music_largeur_texte() {
  assert(etatLargeurTexte("") == 0);
  assert(etatLargeurTexte("abc") == 3 * LARGEUR_CHAR_PX);
}

static void test_music_ligne_courte_ne_defile_pas() {
  const char* t = "123456789012345678901";  // 21 car. = 126 px <= 128
  assert(etatPeriodeDefilement(etatLargeurTexte(t)) == 0);
  assert(etatPositionDefilement(etatLargeurTexte(t), 5000) == 0);
}

static void test_music_position_defilement() {
  int l = 40 * LARGEUR_CHAR_PX;  // 240 px, ligne qui défile
  assert(etatPositionDefilement(l, 0) == 0);
  assert(etatPositionDefilement(l, 1000) == -VITESSE_DEFILEMENT_PX_S);
  unsigned long p = etatPeriodeDefilement(l);
  assert(p == (unsigned long)(l + ECART_DEFILEMENT_PX) * 1000 / VITESSE_DEFILEMENT_PX_S);
  assert(etatPositionDefilement(l, p) == 0);  // boucle exactement
  assert(etatPositionDefilement(l, p + 1000) == -VITESSE_DEFILEMENT_PX_S);
  assert(etatPositionDefilement(l, p - 1) < -(l / 2));  // fin de cycle
}

static void test_music_duree_titre_court() {
  assert(etatDureeMusique("Titre", "Artiste") == DUREE_MUSIC_MS);
  assert(etatDureeMusique("Titre", nullptr) == DUREE_MUSIC_MS);
}

static void test_music_duree_titre_long_deux_defilements() {
  const char* t = "1234567890123456789012345678901234567890";  // 40 car.
  unsigned long p = etatPeriodeDefilement(etatLargeurTexte(t));
  assert(p * 2 > DUREE_MUSIC_MS);
  assert(etatDureeMusique(t, "Art") == 2 * p);
  assert(etatDureeMusique("Court", t) == 2 * p);  // la ligne la plus longue peut être l'artiste
}

static void test_music_evenement_installe_expression_temporaire() {
  etatInit();
  assert(etatRecevoir(EV_MUSIC, "Titre", 100, "Artiste"));
  assert(etatCourant().expression == EXPR_MUSIQUE);
  assert(strcmp(etatCourant().texte, "Titre") == 0);
  assert(strcmp(etatCourant().artiste, "Artiste") == 0);
  assert(!etatMiseAJour(100 + DUREE_MUSIC_MS - 1));
  assert(etatMiseAJour(100 + DUREE_MUSIC_MS));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_music_sans_artiste() {
  etatInit();
  assert(etatRecevoir(EV_MUSIC, "Titre", 0));
  assert(etatCourant().artiste[0] == '\0');
  assert(etatCourant().dureeMs == DUREE_MUSIC_MS);
}

static void test_waiting_persistante_ne_revient_jamais_seule() {
  etatInit();
  assert(etatRecevoir(EV_CLAUDE_WAITING, "autorisation ?", 100));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().texte, "autorisation ?") == 0);
  assert(!etatCourant().temporaire);
  assert(!etatMiseAJour(100 + 24UL * 3600 * 1000));
  assert(etatCourant().expression == EXPR_INTERPELLE);
}

static void test_waiting_working_efface() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "à toi !", 0);
  assert(etatRecevoir(EV_CLAUDE_WORKING, nullptr, 10));
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatCourant().texte[0] == '\0');
}

static void test_waiting_working_sans_effet_au_repos() {
  etatInit();
  assert(!etatRecevoir(EV_CLAUDE_WORKING, nullptr, 0));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_waiting_working_sans_effet_pendant_done() {
  etatInit();
  etatRecevoir(EV_CLAUDE_DONE, "projet", 0);
  assert(!etatRecevoir(EV_CLAUDE_WORKING, nullptr, 10));
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(strcmp(etatCourant().texte, "projet") == 0);
  assert(etatCourant().temporaire);
}

static void test_waiting_remplace_par_un_autre_evenement() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "à toi !", 0);
  assert(etatRecevoir(EV_CLAUDE_DONE, "fini", 10));
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(etatCourant().temporaire);
}

int main() {
  test_repos_au_demarrage();
  test_claude_done_puis_retour_au_repos_apres_5s();
  test_le_dernier_evenement_gagne();
  test_texte_trop_long_tronque();
  test_evenement_sans_texte();
  test_types_connus_et_inconnus();
  test_artiste_vide_par_defaut();
  test_music_largeur_texte();
  test_music_ligne_courte_ne_defile_pas();
  test_music_position_defilement();
  test_music_duree_titre_court();
  test_music_duree_titre_long_deux_defilements();
  test_music_evenement_installe_expression_temporaire();
  test_music_sans_artiste();
  test_waiting_persistante_ne_revient_jamais_seule();
  test_waiting_working_efface();
  test_waiting_working_sans_effet_au_repos();
  test_waiting_working_sans_effet_pendant_done();
  test_waiting_remplace_par_un_autre_evenement();
  puts("Tous les tests passent.");
  return 0;
}
