// Tests du module État sur PC (sans Arduino).
// Lancer depuis la racine du dépôt :  make test
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../compagnon/etat.h"
#include "../compagnon/expressions.h"  // constantes de la Zone visage (en-tête sans Arduino)

static void test_repos_au_demarrage() {
  etatInit();
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatCourant().texte[0] == '\0');
}

static void test_claude_done_puis_retour_au_repos_apres_5s() {
  etatInit();
  assert(etatRecevoir(EV_CLAUDE_DONE, "mon-projet", 1000));
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(strcmp(etatCourant().texte, "Done!") == 0);  // comme dans le Rappel : le projet en ligne 2
  assert(strcmp(etatCourant().ligne2, "mon-projet") == 0);

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
  assert(strcmp(etatCourant().ligne2, "projet-b") == 0);
  assert(!etatMiseAJour(5000));  // l'échéance de la 1re requête est oubliée
  assert(etatMiseAJour(8000));   // 3000 + 5000
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_texte_trop_long_tronque() {
  etatInit();
  char long_texte[200];
  memset(long_texte, 'x', sizeof(long_texte));
  long_texte[199] = '\0';
  etatRecevoir(EV_GIT_PUSH, long_texte, 0);
  assert(strlen(etatCourant().texte) == (size_t)TEXTE_MAX);
}

static void test_evenement_sans_texte() {
  etatInit();
  etatRecevoir(EV_GIT_PUSH, nullptr, 0);
  assert(etatCourant().expression == EXPR_PUSH);
  assert(etatCourant().texte[0] == '\0');
  etatRecevoir(EV_CLAUDE_DONE, nullptr, 0);
  assert(strcmp(etatCourant().texte, "Done!") == 0 && etatCourant().ligne2[0] == '\0');
}

static void test_game_au_repos_affiche_jeu_puis_game_stop_ramene_au_repos() {
  etatInit();
  TypeEvenement t;
  assert(etatTypeDepuisNom("game", &t) && t == EV_GAME);
  assert(etatTypeDepuisNom("game_stop", &t) && t == EV_GAME_STOP);
  assert(etatRecevoir(EV_GAME, "Hollow Knight", 1000));
  assert(etatCourant().expression == EXPR_JEU);
  assert(strcmp(etatCourant().texte, "Hollow Knight") == 0);
  assert(etatCourant().ligne2[0] == '\0');
  assert(etatSonCourant() == SON_AUCUN);
  assert(!etatMiseAJour(20000));  // persistante
  assert(etatRecevoir(EV_GAME_STOP, nullptr, 21000));
  assert(etatCourant().expression == EXPR_REPOS);
  assert(!etatRecevoir(EV_GAME_STOP, nullptr, 22000));  // déjà au Repos
}

static void test_game_nom_different_sur_place_meme_nom_sans_effet() {
  etatInit();
  etatRecevoir(EV_GAME, "Jeu A", 1000);
  assert(!etatRecevoir(EV_GAME, "Jeu A", 11000));
  assert(etatCourant().debutMs == 1000);  // défilement non relancé
  assert(etatRecevoir(EV_GAME, "Jeu B", 12000));
  assert(etatCourant().expression == EXPR_JEU);
  assert(strcmp(etatCourant().texte, "Jeu B") == 0);
  assert(etatCourant().debutMs == 12000);  // défilement repris du début
}

static void test_game_nom_cp437_et_tronque() {
  etatInit();
  etatRecevoir(EV_GAME, "Pok\xC3\xA9mon", 0);
  assert(strcmp(etatCourant().texte, "Pok\x82mon") == 0);
  char long_texte[200];
  memset(long_texte, 'x', sizeof(long_texte));
  long_texte[199] = '\0';
  etatRecevoir(EV_GAME, long_texte, 1);
  assert(strlen(etatCourant().texte) == (size_t)TEXTE_MAX);
}

static void test_game_pendant_waiting_puis_working_mene_a_jeu() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "projet", 0);
  assert(!etatRecevoir(EV_GAME, "Jeu", 1000));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(etatSonCourant() == SON_CARILLON);  // Son non interrompu
  assert(etatRecevoir(EV_CLAUDE_WORKING, nullptr, 2000));
  assert(etatCourant().expression == EXPR_JEU);
}

static void test_game_et_game_stop_pendant_expression_temporaire() {
  etatInit();
  etatRecevoir(EV_GIT_PUSH, "p", 0);
  assert(!etatRecevoir(EV_GAME, "Jeu", 500));
  assert(etatCourant().expression == EXPR_PUSH);
  assert(etatSonCourant() == SON_FANFARE);
  assert(etatMiseAJour(DUREE_GIT_PUSH_MS));
  assert(etatCourant().expression == EXPR_JEU);
  assert(strcmp(etatCourant().texte, "Jeu") == 0);

  etatRecevoir(EV_CLAUDE_DONE, "p", 4000);
  assert(!etatRecevoir(EV_GAME_STOP, nullptr, 4500));
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(etatMiseAJour(4000 + DUREE_CLAUDE_DONE_MS));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_oubli_du_jeu_apres_30s_et_pas_avant() {
  etatInit();
  etatRecevoir(EV_GAME, "Jeu", 0);
  assert(!etatMiseAJour(OUBLI_JEU_MS - 1));
  assert(etatCourant().expression == EXPR_JEU);
  assert(etatRecevoir(EV_GAME, "Jeu", 10000) == false);  // un renvoi repousse l'oubli
  assert(!etatMiseAJour(10000 + OUBLI_JEU_MS - 1));
  assert(etatMiseAJour(10000 + OUBLI_JEU_MS));
  assert(etatCourant().expression == EXPR_REPOS);
  // oublié pendant une Expression temporaire : retour au Repos à son expiration
  etatRecevoir(EV_GAME, "Jeu", 100000);
  etatRecevoir(EV_MUSIC, "T", 100000 + OUBLI_JEU_MS - 2000, "A");
  assert(!etatMiseAJour(100000 + OUBLI_JEU_MS));
  etatMiseAJour(100000 + OUBLI_JEU_MS - 2000 + DUREE_MUSIC_MS);
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_rappel_pendant_jeu_revient_a_jeu_et_game_ne_le_coupe_pas() {
  etatInit();
  etatRecevoir(EV_GAME, "Jeu", 0);
  assert(etatBouton(true, 1000));
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(!etatRecevoir(EV_GAME, "Jeu", 1500));
  assert(!etatRecevoir(EV_GAME, "Autre", 1600));
  assert(!etatRecevoir(EV_GAME_STOP, nullptr, 1700));
  assert(etatCourant().expression == EXPR_SILENCE);  // Rappel intact
  assert(etatMiseAJour(1000 + DUREE_SILENCE_MS));
  assert(etatCourant().expression == EXPR_REPOS);    // game_stop a ramené Repos dessous
  etatBouton(false, 5000);
  etatInit();
  etatRecevoir(EV_GAME, "Jeu", 0);
  etatBouton(true, 1000);
  assert(etatMiseAJour(1000 + DUREE_SILENCE_MS));
  assert(etatCourant().expression == EXPR_JEU);
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
  assert(etatCourant().ligne2[0] == '\0');
  etatRecevoir(EV_CLAUDE_DONE, "projet", 0, "quelqu'un");
  assert(strcmp(etatCourant().ligne2, "projet") == 0);  // claude_done ignore l'artiste
}

static void test_push_temporaire_3s_puis_repos() {
  etatInit();
  assert(etatRecevoir(EV_GIT_PUSH, "desk_companion", 1000));
  assert(etatCourant().expression == EXPR_PUSH);
  assert(strcmp(etatCourant().texte, "desk_companion") == 0);
  assert(!etatMiseAJour(3999));
  assert(etatCourant().expression == EXPR_PUSH);
  assert(etatMiseAJour(4000));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_push_fleche_reste_dans_zone_visage() {
  for (unsigned long t = 0; t <= 4000; t += 7) {
    int haut = etatPushFlecheY(t);
    assert(haut >= ZONE_VISAGE_Y);
    assert(haut + PUSH_FLECHE_HAUTEUR - 1 <= ZONE_VISAGE_Y + ZONE_VISAGE_HAUTEUR - 1);
  }
  // elle monte : plus haut (y plus petit) à la fin d'un cycle qu'au début
  assert(etatPushFlecheY(PUSH_CYCLE_MS - 1) < etatPushFlecheY(0));
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
  assert(strcmp(etatCourant().ligne2, "Artiste") == 0);
  assert(!etatMiseAJour(100 + DUREE_MUSIC_MS - 1));
  assert(etatMiseAJour(100 + DUREE_MUSIC_MS));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_music_sans_artiste() {
  etatInit();
  assert(etatRecevoir(EV_MUSIC, "Titre", 0));
  assert(etatCourant().ligne2[0] == '\0');
  assert(etatCourant().dureeMs == DUREE_MUSIC_MS);
}

static void test_texte_coupe_au_premier_saut_de_ligne() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "autorisation ?\nmon_projet", 0);
  assert(strcmp(etatCourant().texte, "autorisation ?") == 0);
  assert(strcmp(etatCourant().ligne2, "mon_projet") == 0);
  etatRecevoir(EV_GIT_PUSH, "seul", 0);  // sans '\n' : une seule ligne
  assert(strcmp(etatCourant().texte, "seul") == 0 && etatCourant().ligne2[0] == '\0');
  etatRecevoir(EV_GIT_PUSH, "a\nb\nc", 0);  // au plus deux lignes : le 2e '\n' coupe la ligne 2
  assert(strcmp(etatCourant().texte, "a") == 0 && strcmp(etatCourant().ligne2, "b") == 0);
  etatRecevoir(EV_GIT_PUSH, "a\n\xC3\xA9t\xC3\xA9", 0);  // accents de la ligne 2 convertis en CP437
  assert(strlen(etatCourant().ligne2) == 3);
}

static void test_music_saut_de_ligne_l_emporte_sur_artiste() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre\nAutre", 0, "Artiste");
  assert(strcmp(etatCourant().ligne2, "Autre") == 0);
}

static void test_chaque_ligne_tronquee_a_100() {
  assert(TEXTE_MAX == 100);
  char texte[300];
  memset(texte, 'x', 150);
  texte[150] = '\n';
  memset(texte + 151, 'y', 150 - 1);
  texte[300 - 1] = '\0';
  etatInit();
  etatRecevoir(EV_GIT_PUSH, texte, 0);
  assert(strlen(etatCourant().texte) == 100 && strlen(etatCourant().ligne2) == 100);
}

static void test_defilement_par_ligne() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "à toi !\nun_nom_de_projet_vraiment_tres_long", 0);
  assert(etatPeriodeDefilement(etatLargeurTexte(etatCourant().texte)) == 0);  // ligne 1 fixe
  assert(etatPeriodeDefilement(etatLargeurTexte(etatCourant().ligne2)) > 0);  // ligne 2 défile
}

static void test_music_duree_avec_deux_lignes_dans_le_texte() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre\nUn titre vraiment très très long pour défiler jusqu'au bout", 0);
  unsigned long p = etatPeriodeDefilement(etatLargeurTexte(etatCourant().ligne2));
  assert(etatCourant().dureeMs == 2 * p && p * 2 > DUREE_MUSIC_MS);
}

// Signe de vie du PC (music_stop) : empêche le Sommeil sans toucher aux Sessions.
static void signe_de_vie(unsigned long ms) { etatRecevoir(EV_MUSIC_STOP, nullptr, ms); }

// Une Expression temporaire (git_push) prend l'affichage : l'Expression courante n'est plus interpelle,
// donc le Bouton Claude montre le Rappel au lieu de chasser la Session.
static void quitter_interpelle(unsigned long ms) { etatRecevoir(EV_GIT_PUSH, "main\nprojet", ms); }

static void test_waiting_persistante_ne_revient_jamais_seule() {
  etatInit();
  assert(etatRecevoir(EV_CLAUDE_WAITING, "autorisation ?", 100));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().texte, "autorisation ?") == 0);
  assert(!etatCourant().temporaire);
  signe_de_vie(100 + OUBLI_SESSION_MS - 2);
  assert(!etatMiseAJour(100 + OUBLI_SESSION_MS - 1));  // seul l'oubli des Sessions muettes (12 h) y met fin
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
  assert(strcmp(etatCourant().ligne2, "projet") == 0);
  assert(etatCourant().temporaire);
}

static void test_waiting_remplace_par_un_autre_evenement() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "à toi !", 0);
  assert(etatRecevoir(EV_CLAUDE_DONE, "fini", 10));
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(etatCourant().temporaire);
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

static void test_cp437_accents_francais() {
  char s[16];
  etatVersCp437("àâäçéèêëîïôöùûü", s, sizeof(s));
  const unsigned char attendu[] = {0x85, 0x83, 0x84, 0x87, 0x82, 0x8A, 0x88, 0x89,
                                   0x8C, 0x8B, 0x93, 0x94, 0x97, 0x96, 0x81, 0};
  assert(memcmp(s, attendu, sizeof(attendu)) == 0);
  etatVersCp437("ÉÇÜÖÄ", s, sizeof(s));
  assert((unsigned char)s[0] == 0x90 && (unsigned char)s[1] == 0x80 && strlen(s) == 5);
  etatVersCp437("À€😀a", s, sizeof(s));  // non disponibles en CP437 -> '?'
  assert(strcmp(s, "???a") == 0);
}

static void test_cp437_largeur_en_glyphes_et_troncature() {
  etatRecevoir(EV_GIT_PUSH, "été", 0);
  assert(strlen(etatCourant().texte) == 3 && etatLargeurTexte(etatCourant().texte) == 3 * LARGEUR_CHAR_PX);
  char longTexte[300] = "";
  for (int i = 0; i < 120; i++) strcat(longTexte, "é");
  etatRecevoir(EV_GIT_PUSH, longTexte, 0);
  assert((int)strlen(etatCourant().texte) == TEXTE_MAX);
}

// ---- Sons (ticket #15) ----
static void test_son_fanfare_au_push_et_carillon_au_waiting() {
  etatInit();
  assert(etatSonCourant() == SON_AUCUN);
  assert(etatFrequence(0) == 0);
  etatRecevoir(EV_GIT_PUSH, "main\nprojet", 100);
  assert(etatSonCourant() == SON_FANFARE);
  assert(etatFrequence(100) == 523);
  etatRecevoir(EV_CLAUDE_WAITING, nullptr, 5000);
  assert(etatSonCourant() == SON_CARILLON);
  assert(etatFrequence(5000) == 988);
}

static void test_son_evenements_silencieux_ne_touchent_pas_au_son() {
  etatInit();
  etatRecevoir(EV_GIT_PUSH, nullptr, 0);
  etatRecevoir(EV_MUSIC, "titre", 200, "artiste");
  etatRecevoir(EV_CLAUDE_WORKING, nullptr, 300);  // sans effet
  assert(etatSonCourant() == SON_FANFARE);
  assert(etatFrequence(900) == 415);  // 900 ms après le début : la fanfare n'a pas été relancée (sinon 523)
  etatRecevoir(EV_CLAUDE_WAITING, nullptr, 400);
  etatRecevoir(EV_CLAUDE_WORKING, nullptr, 500);  // avec effet : efface l'Expression
  assert(etatSonCourant() == SON_CARILLON);
  assert(etatFrequence(500) == 988);  // 100 ms après le début du carillon
}

static void test_son_nouveau_son_remplace_et_repart_du_debut() {
  etatInit();
  etatRecevoir(EV_GIT_PUSH, nullptr, 1000);
  assert(etatFrequence(1900) == 415);  // 900 ms : 5e note
  etatRecevoir(EV_CLAUDE_WAITING, nullptr, 1900);
  assert(etatFrequence(1900) == 988);
  etatRecevoir(EV_GIT_PUSH, nullptr, 2000);
  assert(etatFrequence(2000) == 523);
}

static void test_son_frequence_fanfare_aux_bornes() {
  etatInit();
  etatRecevoir(EV_GIT_PUSH, nullptr, 10000);
  assert(etatFrequence(10000) == 523);
  assert(etatFrequence(10000 + 110) == 523);
  assert(etatFrequence(10000 + 111) == 0);     // silence entre deux notes identiques
  assert(etatFrequence(10000 + 141) == 523);   // 2e note, détachée
  assert(etatFrequence(10000 + 393) == 0);     // silence avant la 4e note
  assert(etatFrequence(10000 + 423) == 523);
  assert(etatFrequence(10000 + 847) == 523);   // fin de la 4e note tenue
  assert(etatFrequence(10000 + 848) == 415);   // frontière
  assert(etatFrequence(10000 + 1273) == 466);
  assert(etatFrequence(10000 + 1839) == 0);    // silence
  assert(etatFrequence(10000 + 1979) == 0);
  assert(etatFrequence(10000 + 1980) == 466);
  assert(etatFrequence(10000 + 2970) == 523);  // note finale tenue
  assert(etatFrequence(10000 + 2971) == 0);    // terminé
  assert(etatFrequence(10000 + 99999) == 0);
}

static void test_son_frequence_carillon_aux_bornes() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, nullptr, 0);
  assert(etatFrequence(0) == 988);
  assert(etatFrequence(149) == 988);
  assert(etatFrequence(150) == 0);  // silence entre les deux notes
  assert(etatFrequence(179) == 0);
  assert(etatFrequence(180) == 784);
  assert(etatFrequence(429) == 784);
  assert(etatFrequence(430) == 0);  // terminé
}

static void test_son_clochette_au_done_aux_bornes() {
  etatInit();
  etatRecevoir(EV_CLAUDE_DONE, "p", 0);
  assert(etatSonCourant() == SON_CLOCHETTE);
  assert(etatFrequence(0) == 1319);
  assert(etatFrequence(99) == 1319);
  assert(etatFrequence(100) == 1568);
  assert(etatFrequence(199) == 1568);
  assert(etatFrequence(200) == 2093);
  assert(etatFrequence(349) == 2093);
  assert(etatFrequence(350) == 0);  // terminé
  assert(etatDureeSon(SON_CLOCHETTE) < 500);
}

static void test_son_clochette_interrompt_et_est_interrompue_seulement_par_un_son() {
  etatInit();
  etatRecevoir(EV_GIT_PUSH, nullptr, 0);
  etatRecevoir(EV_CLAUDE_DONE, "p", 100);  // interrompt la fanfare
  assert(etatSonCourant() == SON_CLOCHETTE);
  assert(etatFrequence(100) == 1319);
  etatRecevoir(EV_CLAUDE_WAITING, nullptr, 200);  // interrompt la clochette
  assert(etatSonCourant() == SON_CARILLON);
  etatRecevoir(EV_CLAUDE_DONE, "p", 300);  // interrompt le carillon
  assert(etatSonCourant() == SON_CLOCHETTE);
  etatRecevoir(EV_MUSIC, "titre", 320, "artiste");  // sans Son : ne l'interrompt pas
  assert(etatSonCourant() == SON_CLOCHETTE);
  assert(etatFrequence(420) == 1568);
}

static void test_son_durees_totales() {
  assert(etatDureeSon(SON_AUCUN) == 0);
  assert(etatDureeSon(SON_FANFARE) >= 2900 && etatDureeSon(SON_FANFARE) <= 3100);
  assert(etatDureeSon(SON_CARILLON) > 0 && etatDureeSon(SON_CARILLON) < 500);
}

// ---- music_stop (ticket #22) ----

static void test_music_stop_nom_et_pause_sans_changement() {
  etatInit();
  TypeEvenement t;
  assert(etatTypeDepuisNom("music_stop", &t) && t == EV_MUSIC_STOP);
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  assert(!etatRecevoir(EV_MUSIC_STOP, nullptr, 100));  // aucun changement d'Expression
  assert(etatCourant().expression == EXPR_MUSIQUE);
  assert(etatMiseAJour(DUREE_MUSIC_MS));  // l'Expression music expire normalement
  assert(etatCourant().expression == EXPR_REPOS);
  // en pause : plus de Morceau courant, le Bouton répond silence
  assert(etatBouton(true, 20000));
  assert(etatCourant().expression == EXPR_SILENCE);
}

static void test_music_identique_apres_pause_redevient_courant() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  etatRecevoir(EV_MUSIC_STOP, nullptr, 100);
  etatMiseAJour(DUREE_MUSIC_MS);  // retour au Repos
  assert(!etatRecevoir(EV_MUSIC, "Titre", 20000, "Artiste"));  // reprise : pas de changement
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatBouton(true, 21000));
  assert(etatCourant().expression == EXPR_MUSIQUE);
  assert(strcmp(etatCourant().texte, "Titre") == 0);
}

static void test_music_different_apres_pause_s_affiche() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  etatRecevoir(EV_MUSIC_STOP, nullptr, 100);
  assert(etatRecevoir(EV_MUSIC, "Titre", 200, "Autre"));
  assert(etatRecevoir(EV_MUSIC, "Autre titre", 300, "Artiste"));
}

static void test_music_stop_sans_morceau_et_pendant_rappel() {
  etatInit();
  assert(!etatRecevoir(EV_MUSIC_STOP, nullptr, 0));  // rien de connu : sans effet
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  etatBouton(true, 1000);
  assert(!etatRecevoir(EV_MUSIC_STOP, nullptr, 1500));  // ne met pas fin au Rappel
  assert(etatCourant().debutMs == 1000);
  assert(!etatMiseAJour(1000 + 9999));
  assert(etatMiseAJour(1000 + DUREE_MUSIC_MS));
}

// ---- Oubli du Morceau courant (ticket #23) ----

static void test_oubli_apres_30s_sans_nouvelles() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  etatMiseAJour(DUREE_MUSIC_MS);  // retour au Repos
  assert(!etatBouton(false, OUBLI_MORCEAU_MS - 1));
  assert(etatBouton(true, OUBLI_MORCEAU_MS - 1));  // 1 ms avant l'oubli : encore le morceau
  assert(etatCourant().expression == EXPR_MUSIQUE);
  etatBouton(false, OUBLI_MORCEAU_MS + 100);
  etatMiseAJour(OUBLI_MORCEAU_MS + 10000);  // fin du Rappel
  assert(etatBouton(true, OUBLI_MORCEAU_MS + 20000));  // plus de nouvelles depuis longtemps
  assert(etatCourant().expression == EXPR_SILENCE);
}

static void test_renvois_music_gardent_le_morceau_sans_reaffichage() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  for (unsigned long t = 10000; t <= 100000; t += 10000)
    assert(!etatRecevoir(EV_MUSIC, "Titre", t, "Artiste"));  // renvoi invisible
  assert(etatBouton(true, 105000));
  assert(etatCourant().expression == EXPR_MUSIQUE);  // toujours courant
}

static void test_renvois_music_stop_ne_ressuscitent_pas_et_music_apres_oubli_s_affiche() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  etatMiseAJour(DUREE_MUSIC_MS);
  etatMiseAJour(OUBLI_MORCEAU_MS + 1);  // oubli
  assert(etatRecevoir(EV_MUSIC, "Titre", OUBLI_MORCEAU_MS + 2, "Artiste"));  // nouveau morceau
  assert(etatCourant().expression == EXPR_MUSIQUE);
}

static void test_music_stop_compte_comme_des_nouvelles() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  etatRecevoir(EV_MUSIC_STOP, nullptr, OUBLI_MORCEAU_MS - 1);
  etatMiseAJour(OUBLI_MORCEAU_MS + 10000);  // 10 s après le music_stop : pas encore oublié
  // reprise du même morceau : pas de réaffichage
  assert(!etatRecevoir(EV_MUSIC, "Titre", OUBLI_MORCEAU_MS + 10000, "Artiste"));
}

// ---- Bouton et Rappel (ticket #21) ----

static void test_rappel_musique_puis_retour() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre", 0, "Artiste");
  etatMiseAJour(10000);  // l'Expression music expire : Repos
  assert(etatCourant().expression == EXPR_REPOS);

  assert(etatBouton(true, 20000));  // enfoncement : Rappel
  assert(etatCourant().expression == EXPR_MUSIQUE);
  assert(strcmp(etatCourant().texte, "Titre") == 0);
  assert(strcmp(etatCourant().ligne2, "Artiste") == 0);
  assert(etatCourant().debutMs == 20000);
  assert(etatSonCourant() == SON_AUCUN);

  assert(!etatMiseAJour(29999));
  assert(etatMiseAJour(30000));  // fin du Rappel : retour à l'Expression d'avant
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_rappel_silence_sans_music() {
  etatInit();
  assert(etatBouton(true, 1000));
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(strcmp(etatCourant().texte, "Silence radio") == 0);
  assert(etatCourant().ligne2[0] == '\0');
  assert(etatSonCourant() == SON_AUCUN);
  assert(!etatMiseAJour(3999));
  assert(etatMiseAJour(4000));  // 3 s
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_bouton_rebonds_et_maintien_un_seul_rappel() {
  etatInit();
  assert(etatBouton(true, 1000));
  assert(!etatBouton(false, 1010));  // rebond
  assert(!etatBouton(true, 1020));   // rebond
  assert(!etatBouton(true, 1500));   // maintenu
  assert(etatCourant().debutMs == 1000);
  assert(!etatBouton(false, 2000));  // relâché
  assert(etatBouton(true, 2100));    // nouvel appui
}

static void test_bouton_tenu_au_demarrage_sans_rappel() {
  etatInit(true);
  assert(!etatBouton(true, 100));    // toujours tenu : pas de Rappel
  assert(etatCourant().expression == EXPR_REPOS);
  assert(!etatBouton(false, 500));   // relâché
  assert(etatBouton(true, 1000));    // vrai front d'appui : Rappel
  assert(etatCourant().expression == EXPR_SILENCE);
}

// ---- Potentiomètre et Volume (ticket #82) ----

static void test_volume_premiere_lecture_fixe_le_volume_proportionnel() {
  etatInit();
  assert(!etatPotentiometre(2048, 100));  // rien de nouveau à l'écran
  assert(etatVolume() >= 49 && etatVolume() <= 51);
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_volume_zones_mortes_butees_exactes_malgre_le_bruit() {
  etatInit();
  etatPotentiometre(5, 100);
  assert(etatVolume() == 0);
  etatPotentiometre(30, 110);
  assert(etatVolume() == 0);
  etatPotentiometre(4090, 120);
  assert(etatVolume() == 100);
  etatPotentiometre(4060, 130);
  assert(etatVolume() == 100);
  etatPotentiometre(2000, 140);
  etatPotentiometre(3, 150);  // retour en butée basse depuis le milieu
  assert(etatVolume() == 0);
}

static void test_volume_bruit_ignore_variation_nette_prise_en_compte() {
  etatInit();
  etatPotentiometre(2048, 100);
  int v = etatVolume();
  etatPotentiometre(2052, 110);
  etatPotentiometre(2043, 120);
  etatPotentiometre(2055, 130);
  assert(etatVolume() == v);
  etatPotentiometre(3000, 140);
  assert(etatVolume() > v + 15);
  etatPotentiometre(1000, 150);
  assert(etatVolume() < v - 15);
}

static void test_volume_butee_de_pres_sans_sauter_la_zone_morte() {
  etatInit();
  etatPotentiometre(100, 100);  // vol faible mais > 0
  assert(etatVolume() > 0);
  etatPotentiometre(20, 110);   // écart < seuil mais en zone morte : 0 exact
  assert(etatVolume() == 0);
}

static void test_bouton_nouvel_appui_relance_le_rappel() {
  etatInit();
  etatBouton(true, 1000);
  etatBouton(false, 1100);
  assert(etatBouton(true, 2000));
  assert(etatCourant().debutMs == 2000);
  assert(!etatMiseAJour(4999));  // 3 s depuis le second appui
  assert(etatMiseAJour(5000));
}

static void test_rappel_pendant_waiting_retour_a_l_interpellation() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "projet", 0);
  etatBouton(true, 1000);
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(etatMiseAJour(4000));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().texte, "projet") == 0);
}

static void test_rappel_pendant_expression_qui_expire_retour_au_repos() {
  etatInit();
  etatRecevoir(EV_CLAUDE_DONE, "p", 0);
  etatBouton(true, 4000);                // Rappel de 4000 à 7000
  assert(!etatMiseAJour(5000));          // done expire dessous : rien de visible
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(etatMiseAJour(7000));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_rappel_evenement_qui_change_y_met_fin_working_sans_effet_non() {
  etatInit();
  etatBouton(true, 1000);
  assert(!etatRecevoir(EV_CLAUDE_WORKING, nullptr, 1500));  // sans effet
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(etatRecevoir(EV_GIT_PUSH, "main", 2000));
  assert(etatCourant().expression == EXPR_PUSH);
  assert(etatSonCourant() == SON_FANFARE);
}

// Sessions : l'identifiant (5e argument) sépare les attentes ; nullptr = Session anonyme.
static void test_sessions_working_d_une_autre_ne_coupe_pas_l_attente() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  assert(!etatRecevoir(EV_CLAUDE_WORKING, "B", 100, nullptr, "b"));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().texte, "A") == 0);
  assert(!etatMiseAJour(60000));  // pas de délai d'attente
  assert(etatCourant().expression == EXPR_INTERPELLE);
}

static void test_sessions_done_d_une_autre_content_puis_retour_a_l_attente() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  assert(etatRecevoir(EV_CLAUDE_DONE, "B", 1000, nullptr, "b"));
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(strcmp(etatCourant().ligne2, "B") == 0);
  assert(etatMiseAJour(1000 + DUREE_CLAUDE_DONE_MS));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().texte, "A") == 0);
}

static void test_sessions_expression_temporaire_retour_a_l_attente() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  etatRecevoir(EV_GIT_PUSH, "main", 1000);
  assert(etatCourant().expression == EXPR_PUSH);
  assert(etatMiseAJour(1000 + DUREE_GIT_PUSH_MS));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  etatRecevoir(EV_MUSIC, "Titre", 5000, "Artiste");
  assert(etatMiseAJour(5000 + etatCourant().dureeMs));
  assert(etatCourant().expression == EXPR_INTERPELLE);
}

static void test_sessions_interpelle_l_emporte_sur_le_jeu() {
  etatInit();
  etatRecevoir(EV_GAME, "Jeu", 0);
  assert(etatRecevoir(EV_CLAUDE_WAITING, "A", 1000, nullptr, "a"));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(!etatRecevoir(EV_GAME, "Autre", 2000));
  assert(!etatRecevoir(EV_GAME_STOP, nullptr, 2500));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  etatRecevoir(EV_GAME, "Jeu", 3000);
  assert(etatRecevoir(EV_CLAUDE_WORKING, "A", 4000, nullptr, "a"));
  assert(etatCourant().expression == EXPR_JEU);  // Jeu connu à nouveau
  etatRecevoir(EV_CLAUDE_WAITING, "A", 5000, nullptr, "a");
  etatRecevoir(EV_GAME_STOP, nullptr, 5500);
  assert(etatRecevoir(EV_CLAUDE_WORKING, "A", 6000, nullptr, "a"));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_sessions_la_plus_recente_puis_la_precedente_sans_carillon() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WAITING, "B", 1000, nullptr, "b");
  assert(strcmp(etatCourant().texte, "B") == 0);
  unsigned long fin = 1000 + etatDureeSon(SON_CARILLON);
  assert(etatFrequence(fin) == 0);
  assert(etatRecevoir(EV_CLAUDE_WORKING, "B", fin, nullptr, "b"));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().texte, "A") == 0);
  assert(etatFrequence(fin) == 0);  // pas de nouveau carillon
  assert(etatSonCourant() == SON_CARILLON);
  assert(etatRecevoir(EV_CLAUDE_DONE, "A", fin + 1, nullptr, "a"));
  assert(etatMiseAJour(fin + 1 + DUREE_CLAUDE_DONE_MS));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_sessions_seuls_working_et_done_de_la_meme_session_terminent_l_attente() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  assert(!etatRecevoir(EV_CLAUDE_WORKING, "x", 10, nullptr, "inconnue"));
  assert(!etatRecevoir(EV_CLAUDE_WORKING, "x", 20));  // anonyme != "a"
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(etatRecevoir(EV_CLAUDE_WORKING, "x", 30, nullptr, "a"));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_sessions_anonyme_comme_avant_et_distincte_des_nommees() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "anon", 0);
  etatRecevoir(EV_CLAUDE_WAITING, "A", 100, nullptr, "a");
  assert(strcmp(etatCourant().texte, "A") == 0);
  assert(etatRecevoir(EV_CLAUDE_WORKING, nullptr, 200, nullptr, "a"));
  assert(strcmp(etatCourant().texte, "anon") == 0);
  assert(etatRecevoir(EV_CLAUDE_WORKING, nullptr, 300));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_sessions_rappel_pendant_attente_de_deux_sessions() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WAITING, "B", 100, nullptr, "b");
  etatBouton(true, 1000);
  assert(!etatRecevoir(EV_CLAUDE_WORKING, nullptr, 1500, nullptr, "inconnue"));  // sans effet : le Rappel continue
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(etatMiseAJour(4000));
  assert(strcmp(etatCourant().texte, "B") == 0);
  etatBouton(false, 4100);
  etatBouton(true, 5000);
  assert(etatRecevoir(EV_CLAUDE_WORKING, nullptr, 5500, nullptr, "b"));  // change l'Expression : fin du Rappel
  assert(strcmp(etatCourant().texte, "A") == 0);
}

static void test_sessions_au_dela_de_huit_la_plus_ancienne_est_evincee() {
  etatInit();
  char id[8];
  for (int i = 0; i < 9; i++) {
    snprintf(id, sizeof id, "s%d", i);
    etatRecevoir(EV_CLAUDE_WAITING, id, 100 + i, nullptr, id);
  }
  assert(strcmp(etatCourant().texte, "s8") == 0);
  for (int i = 8; i >= 1; i--) {
    snprintf(id, sizeof id, "s%d", i);
    assert(strcmp(etatCourant().texte, id) == 0);
    etatRecevoir(EV_CLAUDE_WORKING, nullptr, 1000 + i, nullptr, id);
  }
  assert(etatCourant().expression == EXPR_REPOS);  // s0 évincée : plus rien n'attend
}

static void test_end_de_la_session_affichee_passe_a_la_suivante_sans_carillon() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WAITING, "B", 1000, nullptr, "b");
  unsigned long fin = 1000 + etatDureeSon(SON_CARILLON);
  assert(etatRecevoir(EV_CLAUDE_END, nullptr, fin, nullptr, "b"));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().texte, "A") == 0);
  assert(etatFrequence(fin) == 0);  // pas de nouveau carillon
  assert(etatRecevoir(EV_CLAUDE_END, nullptr, fin + 1, nullptr, "a"));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_end_d_une_session_non_affichee_ne_change_rien_et_l_oublie() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WAITING, "B", 1000, nullptr, "b");
  assert(!etatRecevoir(EV_CLAUDE_END, nullptr, 2000, nullptr, "a"));
  assert(strcmp(etatCourant().texte, "B") == 0);
  assert(etatRecevoir(EV_CLAUDE_END, nullptr, 3000, nullptr, "b"));
  assert(etatCourant().expression == EXPR_REPOS);  // A oubliée : plus rien n'attend
}

static void test_session_muette_12h_est_oubliee() {
  etatInit();
  etatRecevoir(EV_CLAUDE_DONE, "fantome", 0, nullptr, "f");
  etatRecevoir(EV_CLAUDE_WAITING, "A", 1000, nullptr, "a");
  signe_de_vie(OUBLI_SESSION_MS - 2);
  assert(!etatMiseAJour(OUBLI_SESSION_MS - 1));  // f encore connue
  signe_de_vie(OUBLI_SESSION_MS - 1);
  assert(!etatMiseAJour(OUBLI_SESSION_MS));      // f oubliée, l'affichage (A) ne change pas
  quitter_interpelle(OUBLI_SESSION_MS);  // sinon le Bouton Claude chasserait A au lieu de montrer le Rappel
  etatBoutonClaude(true, OUBLI_SESSION_MS + 1);
  assert(strcmp(etatCourant().texte, "A") == 0);  // plus de « +1 »
  etatBoutonClaude(false, OUBLI_SESSION_MS + 100);
  signe_de_vie(OUBLI_SESSION_MS + 500);
  assert(!etatMiseAJour(OUBLI_SESSION_MS + 1000));  // A oubliée sous le Rappel : pas encore visible
  signe_de_vie(OUBLI_SESSION_MS + 2000);
  assert(etatMiseAJour(OUBLI_SESSION_MS + 1 + DUREE_RAPPEL_CLAUDE_MS));  // fin du Rappel : le Repos
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_session_affichee_muette_12h_retour_au_repos() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  signe_de_vie(OUBLI_SESSION_MS - 1);
  assert(etatMiseAJour(OUBLI_SESSION_MS));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_end_d_une_session_inconnue_est_sans_effet() {
  etatInit();
  assert(!etatRecevoir(EV_CLAUDE_END, nullptr, 0, nullptr, "inconnue"));
  assert(etatCourant().expression == EXPR_REPOS);
  etatRecevoir(EV_CLAUDE_WAITING, "A", 100, nullptr, "a");
  assert(!etatRecevoir(EV_CLAUDE_END, nullptr, 200, nullptr, "inconnue"));
  assert(!etatRecevoir(EV_CLAUDE_END, nullptr, 300));  // anonyme inconnue
  assert(strcmp(etatCourant().texte, "A") == 0);
  assert(etatCourant().expression == EXPR_INTERPELLE);
}

static void test_end_ne_consomme_pas_de_place_dans_la_table() {
  etatInit();
  char id[8];
  for (int i = 0; i < 8; i++) {
    snprintf(id, sizeof id, "s%d", i);
    etatRecevoir(EV_CLAUDE_WAITING, id, 100 + i, nullptr, id);
  }
  for (int i = 0; i < 20; i++) etatRecevoir(EV_CLAUDE_END, nullptr, 500 + i, nullptr, "inconnue");
  etatRecevoir(EV_CLAUDE_WORKING, nullptr, 900, nullptr, "s7");
  assert(strcmp(etatCourant().texte, "s6") == 0);
  for (int i = 6; i >= 0; i--) {  // s0 est toujours là
    snprintf(id, sizeof id, "s%d", i);
    assert(strcmp(etatCourant().texte, id) == 0);
    etatRecevoir(EV_CLAUDE_WORKING, nullptr, 1000 + i, nullptr, id);
  }
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_end_nomme_reconnu_par_l_api() {
  TypeEvenement t;
  assert(etatTypeDepuisNom("claude_end", &t) && t == EV_CLAUDE_END);
}

// s0 attend, s1..s7 travaillent : la table est pleine.
static void huit_sessions_dont_s0_attend() {
  etatInit();
  char id[8];
  for (int i = 0; i < 8; i++) {
    snprintf(id, sizeof id, "s%d", i);
    etatRecevoir(i == 0 ? EV_CLAUDE_WAITING : EV_CLAUDE_WORKING, i == 0 ? "A" : "x", 100 + i, nullptr, id);
  }
}

static void test_eviction_d_une_neuvieme_session_garde_celle_qui_attend() {
  huit_sessions_dont_s0_attend();
  etatRecevoir(EV_CLAUDE_WAITING, "B", 500, nullptr, "s8");  // neuvième : chasse s1, pas s0
  assert(etatRecevoir(EV_CLAUDE_WORKING, "x", 600, nullptr, "s8"));
  assert(etatCourant().expression == EXPR_INTERPELLE);  // s0 attend toujours
  assert(strcmp(etatCourant().texte, "A") == 0);
}

static void test_eviction_ne_fige_pas_l_interpellation_affichee() {
  huit_sessions_dont_s0_attend();
  assert(!etatRecevoir(EV_CLAUDE_WORKING, "x", 500, nullptr, "s8"));  // neuvième : chasse s1
  assert(strcmp(etatCourant().texte, "A") == 0);
  assert(etatRecevoir(EV_CLAUDE_WORKING, "x", 600, nullptr, "s0"));  // réponse à s0 : fin de l'attente
  assert(etatCourant().expression == EXPR_REPOS);
}

// millis() repasse à zéro : « a » attend juste avant, « b » juste après ; « b » est la plus récente.
static void test_sessions_ordonnees_par_age_a_travers_le_retour_a_zero() {
  etatInit();
  const unsigned long avant = (unsigned long)-1 - 10;
  etatRecevoir(EV_CLAUDE_WAITING, "A", avant, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WAITING, "B", 5, nullptr, "b");
  assert(strcmp(etatCourant().texte, "B") == 0);
  assert(etatBoutonClaude(true, 10));  // chasse la Session affichée, b
  assert(strcmp(etatCourant().texte, "A") == 0);
}

static void test_attente_ne_disparait_pas_avec_le_temps_meme_a_plusieurs() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 0, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WAITING, "B", 10, nullptr, "b");
  signe_de_vie(OUBLI_SESSION_MS - 2);
  etatMiseAJour(OUBLI_SESSION_MS - 1);  // juste avant l'oubli des Sessions muettes
  assert(strcmp(etatCourant().texte, "B") == 0);
  etatRecevoir(EV_CLAUDE_END, nullptr, OUBLI_SESSION_MS - 1, nullptr, "b");
  assert(strcmp(etatCourant().texte, "A") == 0);
}

static void test_bouton_claude_sans_session_sans_claude_3s_puis_retour() {
  etatInit();
  assert(etatBoutonClaude(true, 1000));
  assert(etatCourant().expression == EXPR_SANS_CLAUDE);
  assert(strcmp(etatCourant().texte, "pas de Claude") == 0);
  assert(etatCourant().ligne2[0] == '\0');
  assert(etatSonCourant() == SON_AUCUN);
  assert(!etatMiseAJour(3999));
  assert(etatMiseAJour(4000));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_bouton_claude_attend_interpelle_avec_libelle_et_projet() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "autorisation ?\nprojet-a", 500, nullptr, "a");
  quitter_interpelle(600);
  assert(etatBoutonClaude(true, 1000));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().texte, "autorisation ?") == 0);
  assert(strcmp(etatCourant().ligne2, "projet-a") == 0);
  assert(etatCourant().dureeMs == 3000);
  assert(etatMiseAJour(4000));
  assert(etatCourant().expression == EXPR_INTERPELLE);  // l'Expression courante, en dessous
}

static void test_bouton_claude_attend_la_plus_recente_avant_fini() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "libelle A\nprojet-a", 100, nullptr, "a");
  etatRecevoir(EV_CLAUDE_DONE, "projet-b", 200, nullptr, "b");
  etatRecevoir(EV_CLAUDE_WAITING, "libelle C\nprojet-c", 300, nullptr, "c");
  quitter_interpelle(350);
  etatBoutonClaude(true, 1000);
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().ligne2, "projet-c") == 0);
}

static void test_bouton_claude_travaille_avant_fini() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WORKING, "projet-a", 100, nullptr, "a");
  etatRecevoir(EV_CLAUDE_DONE, "projet-b", 200, nullptr, "b");
  etatMiseAJour(10000);
  etatBoutonClaude(true, 11000);
  assert(etatCourant().expression == EXPR_TRAVAILLE);
  assert(strcmp(etatCourant().ligne2, "projet-a") == 0);
  assert(etatCourant().dureeMs == 3000);
}

static void test_sommeil_a_2_min_pile_pas_avant() {
  etatInit();
  assert(!etatMiseAJour(DELAI_SOMMEIL_MS - 1));
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatMiseAJour(DELAI_SOMMEIL_MS));
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(etatCourant().texte[0] == '\0' && etatCourant().ligne2[0] == '\0');
  assert(!etatCourant().temporaire);
  assert(etatSonCourant() == SON_AUCUN);
  assert(!etatMiseAJour(DELAI_SOMMEIL_MS + 1000));
}

static void test_pas_de_sommeil_sans_wifi() {
  etatInit();
  etatMiseAJour(DELAI_SOMMEIL_MS * 3, false);
  assert(etatCourant().expression == EXPR_REPOS);
  etatMiseAJour(DELAI_SOMMEIL_MS * 10, false);
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_wifi_perdu_pendant_le_sommeil_reveille_sur_repos() {
  etatInit();
  etatMiseAJour(DELAI_SOMMEIL_MS);
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(etatMiseAJour(DELAI_SOMMEIL_MS + 1000, false));
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatSonCourant() == SON_AUCUN);
  assert(!etatMiseAJour(DELAI_SOMMEIL_MS + 2000, false));  // reste au Repos, pas de Sommeil sans Wi-Fi
}

static void test_sommeil_2_min_apres_le_retour_du_wifi() {
  etatInit();
  etatMiseAJour(DELAI_SOMMEIL_MS * 3, false);
  etatMiseAJour(DELAI_SOMMEIL_MS * 3 + 1000);  // Wi-Fi de retour
  assert(!etatMiseAJour(DELAI_SOMMEIL_MS * 4 + 999));
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatMiseAJour(DELAI_SOMMEIL_MS * 4 + 1000));
  assert(etatCourant().expression == EXPR_ENDORMI);
}

static void test_evenements_reguliers_empechent_le_sommeil() {
  etatInit();
  for (unsigned long t = 10000; t <= 5 * DELAI_SOMMEIL_MS; t += 10000) {
    etatRecevoir(EV_MUSIC_STOP, nullptr, t);
    etatRecevoir(EV_GAME_STOP, nullptr, t + 1);
    etatMiseAJour(t + 2);
    assert(etatCourant().expression == EXPR_REPOS);
  }
}

static void test_sommeil_oublie_les_sessions() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "A", 1000, nullptr, "a");
  assert(etatMiseAJour(1000 + DELAI_SOMMEIL_MS));
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(etatSonCourant() == SON_CARILLON);  // le carillon d'avant, aucun nouveau Son
  etatBoutonClaude(true, 1000 + DELAI_SOMMEIL_MS + 100);
  assert(etatCourant().expression == EXPR_SANS_CLAUDE);
  etatBoutonClaude(false, 1000 + DELAI_SOMMEIL_MS + 200);
  assert(etatMiseAJour(1000 + DELAI_SOMMEIL_MS + 100 + DUREE_RAPPEL_CLAUDE_MS));
  assert(etatCourant().expression == EXPR_ENDORMI);  // Endormi, plus l'attente de A
}

static void test_sommeil_pendant_un_rappel_retour_a_endormi() {
  etatInit();
  etatBouton(true, DELAI_SOMMEIL_MS - 100);  // Rappel silence en cours quand le Sommeil arrive
  assert(!etatMiseAJour(DELAI_SOMMEIL_MS));  // Rappel visible : rien ne change à l'écran
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(etatMiseAJour(DELAI_SOMMEIL_MS - 100 + DUREE_SILENCE_MS));
  assert(etatCourant().expression == EXPR_ENDORMI);
}

static void test_rappel_musique_pendant_le_sommeil_gros_dodo_temps() {
  etatInit();
  etatMiseAJour(DELAI_SOMMEIL_MS);
  assert(etatCourant().expression == EXPR_ENDORMI);
  unsigned long t = DELAI_SOMMEIL_MS + 1000;
  assert(etatBouton(true, t));
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(strcmp(etatCourant().texte, "Gros dodo temps") == 0);
  assert(etatCourant().ligne2[0] == '\0');
  assert(etatCourant().temporaire);
  assert(etatSonCourant() == SON_AUCUN);
  assert(!etatMiseAJour(t + DUREE_SILENCE_MS - 1));
  assert(strcmp(etatCourant().texte, "Gros dodo temps") == 0);
  assert(etatMiseAJour(t + DUREE_SILENCE_MS));
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(etatCourant().texte[0] == '\0');
  assert(etatSonCourant() == SON_AUCUN);
  // l'appui n'a pas réveillé : le Sommeil continue
  assert(!etatMiseAJour(t + DUREE_SILENCE_MS + 1000));
  assert(etatCourant().expression == EXPR_ENDORMI);
}

static void test_evenements_reveillent_et_s_appliquent() {
  etatInit();
  etatMiseAJour(DELAI_SOMMEIL_MS);
  assert(etatRecevoir(EV_MUSIC_STOP, nullptr, DELAI_SOMMEIL_MS + 1));
  assert(etatCourant().expression == EXPR_REPOS);

  etatMiseAJour(DELAI_SOMMEIL_MS * 2 + 1);
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(etatRecevoir(EV_MUSIC, "titre", DELAI_SOMMEIL_MS * 2 + 2, "art"));
  assert(etatCourant().expression == EXPR_MUSIQUE);

  etatMiseAJour(DELAI_SOMMEIL_MS * 4);
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(etatRecevoir(EV_GAME, "Jeu", DELAI_SOMMEIL_MS * 4 + 1));
  assert(etatCourant().expression == EXPR_JEU);

  etatMiseAJour(DELAI_SOMMEIL_MS * 5 + 1);  // Jeu oublié puis Sommeil
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(etatRecevoir(EV_GAME, "Jeu", DELAI_SOMMEIL_MS * 5 + 1));
  assert(etatRecevoir(EV_GAME_STOP, nullptr, DELAI_SOMMEIL_MS * 5 + 2));
  assert(etatCourant().expression == EXPR_REPOS);

  etatMiseAJour(DELAI_SOMMEIL_MS * 8);
  assert(etatCourant().expression == EXPR_ENDORMI);
  assert(etatRecevoir(EV_CLAUDE_WAITING, "A", DELAI_SOMMEIL_MS * 8 + 1, nullptr, "a"));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(etatSonCourant() == SON_CARILLON);
}

static void test_reveil_sans_son_et_bouton_ne_reveille_pas() {
  etatInit();
  etatMiseAJour(DELAI_SOMMEIL_MS);
  assert(etatSonCourant() == SON_AUCUN);
  assert(etatBouton(true, DELAI_SOMMEIL_MS + 100));
  etatBouton(false, DELAI_SOMMEIL_MS + 200);
  assert(etatMiseAJour(DELAI_SOMMEIL_MS + 100 + DUREE_SILENCE_MS));
  assert(etatCourant().expression == EXPR_ENDORMI);  // le Bouton n'a pas réveillé
  assert(etatRecevoir(EV_MUSIC_STOP, nullptr, DELAI_SOMMEIL_MS + 5000));
  assert(etatSonCourant() == SON_AUCUN);
  assert(!etatMiseAJour(DELAI_SOMMEIL_MS + 5000 + DELAI_SOMMEIL_MS - 1));  // compte remis à zéro au réveil
  assert(etatMiseAJour(DELAI_SOMMEIL_MS + 5000 + DELAI_SOMMEIL_MS));
}

static void test_bouton_claude_attend_avant_travaille() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "libelle\nprojet-a", 100, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WORKING, "projet-b", 200, nullptr, "b");
  quitter_interpelle(900);
  etatBoutonClaude(true, 1000);
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().ligne2, "projet-a") == 0);
}

static void test_bouton_claude_travaille_la_plus_recente() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WORKING, "projet-a", 100, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WORKING, "projet-b", 200, nullptr, "b");
  etatBoutonClaude(true, 1000);
  assert(etatCourant().expression == EXPR_TRAVAILLE);
  assert(strcmp(etatCourant().ligne2, "projet-b") == 0);
}

static void test_claude_working_hors_rappel_ne_change_rien() {
  etatInit();
  assert(!etatRecevoir(EV_CLAUDE_WORKING, "projet-a", 100, nullptr, "a"));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_bouton_claude_fini_content_de_la_plus_recente() {
  etatInit();
  etatRecevoir(EV_CLAUDE_DONE, "projet-a", 100, nullptr, "a");
  etatRecevoir(EV_CLAUDE_DONE, "projet-b", 200, nullptr, "b");
  etatMiseAJour(10000);
  etatBoutonClaude(true, 11000);
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(strncmp(etatCourant().texte, "Done!", 5) == 0);  // comme « Working » : le projet en ligne 2
  assert(strcmp(etatCourant().ligne2, "projet-b") == 0);
  assert(etatSonCourant() == SON_CLOCHETTE);  // le Son de claude_done, pas du Rappel
  assert(etatFrequence(11000 + etatDureeSon(SON_CLOCHETTE)) == 0);
}

static void test_bouton_claude_compteur_autres_sessions() {
  etatInit();
  etatRecevoir(EV_CLAUDE_DONE, "projet-a", 100, nullptr, "a");
  etatBoutonClaude(true, 1000);
  assert(strcmp(etatCourant().texte, "Done!") == 0);  // une seule : pas de compteur
  etatBoutonClaude(false, 1100);
  etatRecevoir(EV_CLAUDE_DONE, "projet-b", 1200, nullptr, "b");
  etatBoutonClaude(true, 2000);
  assert(strcmp(etatCourant().texte, "Done!    \xB3 2 sessions") == 0);  // 21 glyphes : compteur au bord droit
  assert(strcmp(etatCourant().ligne2, "projet-b") == 0);
  etatBoutonClaude(false, 2100);
  etatRecevoir(EV_CLAUDE_WAITING, "libelle\nprojet-c", 2200, nullptr, "c");
  quitter_interpelle(2900);
  etatBoutonClaude(true, 3000);
  assert(strcmp(etatCourant().texte, "libelle  \xB3 3 sessions") == 0);
  assert(strcmp(etatCourant().ligne2, "projet-c") == 0);
  etatBoutonClaude(false, 3100);
  etatRecevoir(EV_CLAUDE_END, nullptr, 3200, nullptr, "c");
  etatRecevoir(EV_CLAUDE_WORKING, "projet-b", 3300, nullptr, "b");
  etatBoutonClaude(true, 4000);
  assert(strcmp(etatCourant().texte, "Working  \xB3 2 sessions") == 0);  // « Working » reste à gauche
  assert(etatLargeurTexte(etatCourant().texte) <= LARGEUR_ECRAN_PX);
}

static void test_bouton_claude_compteur_ne_deborde_pas_la_ligne() {
  etatInit();
  char long_[TEXTE_MAX + 20];
  memset(long_, 'x', sizeof(long_));
  long_[sizeof(long_) - 1] = '\0';
  etatRecevoir(EV_CLAUDE_DONE, "b", 100, nullptr, "b");
  etatRecevoir(EV_CLAUDE_WAITING, "permission\nprojet", 200, nullptr, "c");
  quitter_interpelle(900);
  etatBoutonClaude(true, 1000);
  assert(strcmp(etatCourant().texte, "permission\xB3" "2 sessions") == 0);  // à l'étroit : sans espace après « │ »
  etatBoutonClaude(false, 1100);
  etatRecevoir(EV_CLAUDE_WAITING, "autorisation ?\nprojet", 1200, nullptr, "c");
  quitter_interpelle(1900);
  etatBoutonClaude(true, 2000);
  assert(strcmp(etatCourant().texte, "autorisation ? \xB3 2 sessions") == 0);  // trop long : à la suite, défile
  etatBoutonClaude(false, 2100);
  etatRecevoir(EV_CLAUDE_WAITING, long_, 2200, nullptr, "c");
  quitter_interpelle(2900);
  etatBoutonClaude(true, 3000);
  assert(strlen(etatCourant().texte) <= TEXTE_MAX);
  assert(strcmp(etatCourant().texte + strlen(etatCourant().texte) - 13, " \xB3 2 sessions") == 0);
}

static void test_bouton_claude_remplace_rappel_musique_et_inversement() {
  etatInit();
  etatBouton(true, 1000);
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(etatBoutonClaude(true, 1500));
  assert(etatCourant().expression == EXPR_SANS_CLAUDE);
  assert(etatCourant().debutMs == 1500);
  assert(etatBoutonClaude(false, 1600) == false);
  assert(etatBouton(false, 1600) == false);
  assert(etatBouton(true, 2000));
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(etatCourant().debutMs == 2000);
  assert(etatMiseAJour(5000));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_boutons_anti_rebond_independants() {
  etatInit();
  assert(etatBouton(true, 1000));
  assert(etatBoutonClaude(true, 1010));   // le rebond de l'autre Bouton ne le concerne pas
  assert(!etatBoutonClaude(false, 1020)); // rebond de D26 ignoré
  assert(!etatBoutonClaude(true, 1030));
  assert(etatCourant().debutMs == 1010);
  assert(!etatBoutonClaude(false, 1500));
  assert(etatBoutonClaude(true, 1600));   // vrai nouvel appui
}

static void test_bouton_claude_tenu_au_demarrage_sans_rappel() {
  etatInit(false, true);
  assert(!etatBoutonClaude(true, 100));
  assert(etatCourant().expression == EXPR_REPOS);
  assert(!etatBoutonClaude(false, 500));
  assert(etatBoutonClaude(true, 1000));
  assert(etatCourant().expression == EXPR_SANS_CLAUDE);
  etatInit(true, false);  // l'autre Bouton tenu n'empêche pas D26
  assert(etatBoutonClaude(true, 100));
}

static void test_rappel_claude_evenement_qui_change_y_met_fin() {
  etatInit();
  etatBoutonClaude(true, 1000);
  assert(!etatRecevoir(EV_CLAUDE_WORKING, "", 1500, nullptr, "a"));  // sans effet : le Rappel continue
  assert(etatCourant().expression == EXPR_SANS_CLAUDE);
  assert(etatRecevoir(EV_GIT_PUSH, "p", 2000));
  assert(etatCourant().expression == EXPR_PUSH);
}

static void test_bouton_claude_pendant_interpelle_chasse_la_session_vers_repos_sans_son_ni_rappel() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "Hey?\nprojet-a", 100, nullptr, "a");
  assert(etatBoutonClaude(true, 100000));
  assert(etatCourant().expression == EXPR_REPOS);
  assert(etatFrequence(100000) == 0);  // le carillon est fini et rien de neuf ne sonne
  assert(!etatMiseAJour(100100));  // pas de Rappel à expirer
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_bouton_claude_chasse_vers_jeu_quand_un_jeu_tourne() {
  etatInit();
  etatRecevoir(EV_GAME, "Jeu", 50);
  etatRecevoir(EV_CLAUDE_WAITING, "Hey?\nprojet-a", 100, nullptr, "a");
  assert(etatBoutonClaude(true, 1000));
  assert(etatCourant().expression == EXPR_JEU);
}

static void test_bouton_claude_deux_sessions_attendent_chasse_une_a_la_fois_sans_carillon() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "Hey?\nprojet-a", 100, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WAITING, "Hey?\nprojet-b", 200, nullptr, "b");
  assert(etatBoutonClaude(true, 100000));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(strcmp(etatCourant().ligne2, "projet-a") == 0);
  assert(etatFrequence(100000) == 0);  // pas de carillon
  assert(!etatBoutonClaude(true, 100100));  // maintenu : une seule Session chassée
  etatBoutonClaude(false, 100200);
  assert(etatBoutonClaude(true, 101000));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_bouton_claude_session_chassee_reste_suivie_en_fini() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "Hey?\nprojet-a", 100, nullptr, "a");
  etatRecevoir(EV_CLAUDE_WORKING, "projet-b", 150, nullptr, "b");
  etatBoutonClaude(true, 1000);
  etatBoutonClaude(false, 1100);
  assert(etatBoutonClaude(true, 2000));  // plus interpelle : Rappel Claude, B travaille
  assert(etatCourant().expression == EXPR_TRAVAILLE);
  etatBoutonClaude(false, 2100);
  etatRecevoir(EV_CLAUDE_END, nullptr, 2200, nullptr, "b");
  assert(etatBoutonClaude(true, 3000));
  assert(etatCourant().expression == EXPR_CONTENT);
  assert(strcmp(etatCourant().texte, "Done!") == 0);
  assert(strcmp(etatCourant().ligne2, "projet-a") == 0);
}

static void test_bouton_claude_chasse_met_fin_au_rappel_musique_au_dessus_d_interpelle() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "Hey?\nprojet-a", 100, nullptr, "a");
  assert(etatBouton(true, 1000));
  assert(etatCourant().expression == EXPR_SILENCE);
  assert(etatBoutonClaude(true, 1500));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_bouton_claude_session_chassee_qui_attend_a_nouveau_interpelle_avec_carillon() {
  etatInit();
  etatRecevoir(EV_CLAUDE_WAITING, "Hey?\nprojet-a", 100, nullptr, "a");
  etatBoutonClaude(true, 100000);
  assert(etatRecevoir(EV_CLAUDE_WAITING, "Hey?\nprojet-a", 105000, nullptr, "a"));
  assert(etatCourant().expression == EXPR_INTERPELLE);
  assert(etatFrequence(105000) != 0);
}

// --- Vidéo courante (#70) ---

static void test_video_reconnue_et_installe_expression_video() {
  TypeEvenement type;
  assert(etatTypeDepuisNom("video", &type) && type == EV_VIDEO);
  etatInit();
  assert(etatRecevoir(EV_VIDEO, "Titre", 1000, "Chaine"));
  assert(etatCourant().expression == EXPR_VIDEO);
  assert(strcmp(etatCourant().texte, "Titre") == 0);
  assert(strcmp(etatCourant().ligne2, "Chaine") == 0);
  assert(etatCourant().temporaire);
  assert(etatSonCourant() == SON_AUCUN);
  assert(etatCourant().dureeMs == DUREE_MUSIC_MS);
  assert(!etatMiseAJour(1000 + DUREE_MUSIC_MS - 1));
  assert(etatMiseAJour(1000 + DUREE_MUSIC_MS));
  assert(etatCourant().expression == EXPR_REPOS);
}

static void test_video_duree_identique_a_musique_pour_textes_longs() {
  const char* longTitre = "Un titre vraiment tres tres long qui defile sur le Bandeau de la Boule";
  etatInit();
  etatRecevoir(EV_MUSIC, longTitre, 0, "Chaine");
  unsigned long dMusique = etatCourant().dureeMs;
  etatInit();
  etatRecevoir(EV_VIDEO, longTitre, 0, "Chaine");
  assert(etatCourant().dureeMs == dMusique);
  assert(dMusique > DUREE_MUSIC_MS);
}

static void test_meme_video_apres_music_stop_pas_de_reaffichage() {
  etatInit();
  etatRecevoir(EV_VIDEO, "Titre", 0, "Chaine");
  etatMiseAJour(DUREE_MUSIC_MS);
  etatRecevoir(EV_MUSIC_STOP, nullptr, 11000);
  assert(!etatRecevoir(EV_VIDEO, "Titre", 12000, "Chaine"));
  assert(etatBouton(true, 13000));
  assert(etatCourant().expression == EXPR_VIDEO);
}

static void test_music_puis_video_et_inverse_remplacent_et_rappel_montre_la_derniere() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Morceau", 0, "Artiste");
  assert(etatRecevoir(EV_VIDEO, "Film", 1000, "Chaine"));
  assert(etatCourant().expression == EXPR_VIDEO);
  etatMiseAJour(1000 + DUREE_MUSIC_MS);
  assert(etatBouton(true, 12000));
  assert(etatCourant().expression == EXPR_VIDEO);
  assert(strcmp(etatCourant().texte, "Film") == 0);
  etatBouton(false, 12100);
  etatMiseAJour(12000 + DUREE_SILENCE_MS + 30000);
  etatRecevoir(EV_VIDEO, "Film", 50000, "Chaine");
  assert(etatRecevoir(EV_MUSIC, "Morceau", 51000, "Artiste"));
  assert(etatCourant().expression == EXPR_MUSIQUE);
  etatMiseAJour(51000 + DUREE_MUSIC_MS);
  assert(etatBouton(true, 62000));
  assert(etatCourant().expression == EXPR_MUSIQUE);
  assert(strcmp(etatCourant().texte, "Morceau") == 0);
}

static void test_meme_titre_et_artiste_de_music_a_video_reaffiche_et_inversement() {
  etatInit();
  etatRecevoir(EV_MUSIC, "Titre", 0, "Quelqu un");
  assert(etatRecevoir(EV_VIDEO, "Titre", 1000, "Quelqu un"));
  assert(etatCourant().expression == EXPR_VIDEO);
  assert(etatRecevoir(EV_MUSIC, "Titre", 2000, "Quelqu un"));
  assert(etatCourant().expression == EXPR_MUSIQUE);
}

static void test_video_puis_music_stop_rappel_silence() {
  etatInit();
  etatRecevoir(EV_VIDEO, "Titre", 0, "Chaine");
  etatMiseAJour(DUREE_MUSIC_MS);
  etatRecevoir(EV_MUSIC_STOP, nullptr, 11000);
  assert(etatBouton(true, 12000));
  assert(etatCourant().expression == EXPR_SILENCE);
}

static void test_video_oubliee_apres_30s_sans_nouvelles() {
  etatInit();
  etatRecevoir(EV_VIDEO, "Titre", 0, "Chaine");
  etatMiseAJour(DUREE_MUSIC_MS);
  assert(etatBouton(true, OUBLI_MORCEAU_MS + 1000));
  assert(etatCourant().expression == EXPR_SILENCE);
  // la même vidéo revenue ensuite s'affiche de nouveau
  etatBouton(false, OUBLI_MORCEAU_MS + 1100);
  etatMiseAJour(OUBLI_MORCEAU_MS + 20000);
  assert(etatRecevoir(EV_VIDEO, "Titre", OUBLI_MORCEAU_MS + 21000, "Chaine"));
}

static void test_video_compte_comme_nouvelle_du_pc_pour_le_sommeil() {
  etatInit();
  etatRecevoir(EV_VIDEO, "Titre", DELAI_SOMMEIL_MS - 1000, "Chaine");
  etatMiseAJour(DELAI_SOMMEIL_MS + 5000);
  assert(etatCourant().expression != EXPR_ENDORMI);
}

int main() {
  test_video_reconnue_et_installe_expression_video();
  test_video_duree_identique_a_musique_pour_textes_longs();
  test_meme_video_apres_music_stop_pas_de_reaffichage();
  test_music_puis_video_et_inverse_remplacent_et_rappel_montre_la_derniere();
  test_meme_titre_et_artiste_de_music_a_video_reaffiche_et_inversement();
  test_video_puis_music_stop_rappel_silence();
  test_video_oubliee_apres_30s_sans_nouvelles();
  test_video_compte_comme_nouvelle_du_pc_pour_le_sommeil();
  test_bouton_claude_deux_sessions_attendent_chasse_une_a_la_fois_sans_carillon();
  test_bouton_claude_session_chassee_reste_suivie_en_fini();
  test_bouton_claude_chasse_met_fin_au_rappel_musique_au_dessus_d_interpelle();
  test_bouton_claude_session_chassee_qui_attend_a_nouveau_interpelle_avec_carillon();
  test_bouton_claude_pendant_interpelle_chasse_la_session_vers_repos_sans_son_ni_rappel();
  test_bouton_claude_chasse_vers_jeu_quand_un_jeu_tourne();
  test_end_de_la_session_affichee_passe_a_la_suivante_sans_carillon();
  test_end_d_une_session_non_affichee_ne_change_rien_et_l_oublie();
  test_end_d_une_session_inconnue_est_sans_effet();
  test_session_muette_12h_est_oubliee();
  test_session_affichee_muette_12h_retour_au_repos();
  test_end_ne_consomme_pas_de_place_dans_la_table();
  test_end_nomme_reconnu_par_l_api();
  test_eviction_d_une_neuvieme_session_garde_celle_qui_attend();
  test_eviction_ne_fige_pas_l_interpellation_affichee();
  test_sessions_ordonnees_par_age_a_travers_le_retour_a_zero();
  test_attente_ne_disparait_pas_avec_le_temps_meme_a_plusieurs();
  test_sessions_working_d_une_autre_ne_coupe_pas_l_attente();
  test_sessions_done_d_une_autre_content_puis_retour_a_l_attente();
  test_sessions_expression_temporaire_retour_a_l_attente();
  test_sessions_interpelle_l_emporte_sur_le_jeu();
  test_sessions_la_plus_recente_puis_la_precedente_sans_carillon();
  test_sessions_seuls_working_et_done_de_la_meme_session_terminent_l_attente();
  test_sessions_anonyme_comme_avant_et_distincte_des_nommees();
  test_sessions_rappel_pendant_attente_de_deux_sessions();
  test_sessions_au_dela_de_huit_la_plus_ancienne_est_evincee();
  test_son_fanfare_au_push_et_carillon_au_waiting();
  test_son_evenements_silencieux_ne_touchent_pas_au_son();
  test_son_nouveau_son_remplace_et_repart_du_debut();
  test_son_frequence_fanfare_aux_bornes();
  test_son_frequence_carillon_aux_bornes();
  test_son_clochette_au_done_aux_bornes();
  test_son_clochette_interrompt_et_est_interrompue_seulement_par_un_son();
  test_son_durees_totales();
  test_cp437_accents_francais();
  test_cp437_largeur_en_glyphes_et_troncature();
  test_repos_au_demarrage();
  test_claude_done_puis_retour_au_repos_apres_5s();
  test_le_dernier_evenement_gagne();
  test_texte_trop_long_tronque();
  test_evenement_sans_texte();
  test_types_connus_et_inconnus();
  test_game_au_repos_affiche_jeu_puis_game_stop_ramene_au_repos();
  test_game_nom_different_sur_place_meme_nom_sans_effet();
  test_game_nom_cp437_et_tronque();
  test_game_pendant_waiting_puis_working_mene_a_jeu();
  test_game_et_game_stop_pendant_expression_temporaire();
  test_oubli_du_jeu_apres_30s_et_pas_avant();
  test_rappel_pendant_jeu_revient_a_jeu_et_game_ne_le_coupe_pas();
  test_artiste_vide_par_defaut();
  test_music_largeur_texte();
  test_music_ligne_courte_ne_defile_pas();
  test_music_position_defilement();
  test_music_duree_titre_court();
  test_music_duree_titre_long_deux_defilements();
  test_music_evenement_installe_expression_temporaire();
  test_music_sans_artiste();
  test_texte_coupe_au_premier_saut_de_ligne();
  test_music_saut_de_ligne_l_emporte_sur_artiste();
  test_chaque_ligne_tronquee_a_100();
  test_defilement_par_ligne();
  test_music_duree_avec_deux_lignes_dans_le_texte();
  test_waiting_persistante_ne_revient_jamais_seule();
  test_waiting_working_efface();
  test_waiting_working_sans_effet_au_repos();
  test_waiting_working_sans_effet_pendant_done();
  test_waiting_remplace_par_un_autre_evenement();
  test_repos_clignement_ferme_puis_rouvre();
  test_push_temporaire_3s_puis_repos();
  test_push_fleche_reste_dans_zone_visage();
  test_music_stop_nom_et_pause_sans_changement();
  test_music_identique_apres_pause_redevient_courant();
  test_music_different_apres_pause_s_affiche();
  test_music_stop_sans_morceau_et_pendant_rappel();
  test_oubli_apres_30s_sans_nouvelles();
  test_renvois_music_gardent_le_morceau_sans_reaffichage();
  test_renvois_music_stop_ne_ressuscitent_pas_et_music_apres_oubli_s_affiche();
  test_music_stop_compte_comme_des_nouvelles();
  test_rappel_musique_puis_retour();
  test_rappel_silence_sans_music();
  test_rappel_musique_pendant_le_sommeil_gros_dodo_temps();
  test_bouton_rebonds_et_maintien_un_seul_rappel();
  test_bouton_tenu_au_demarrage_sans_rappel();
  test_bouton_nouvel_appui_relance_le_rappel();
  test_rappel_pendant_waiting_retour_a_l_interpellation();
  test_rappel_pendant_expression_qui_expire_retour_au_repos();
  test_rappel_evenement_qui_change_y_met_fin_working_sans_effet_non();
  test_bouton_claude_sans_session_sans_claude_3s_puis_retour();
  test_bouton_claude_attend_interpelle_avec_libelle_et_projet();
  test_bouton_claude_attend_la_plus_recente_avant_fini();
  test_bouton_claude_fini_content_de_la_plus_recente();
  test_bouton_claude_compteur_autres_sessions();
  test_bouton_claude_compteur_ne_deborde_pas_la_ligne();
  test_bouton_claude_remplace_rappel_musique_et_inversement();
  test_boutons_anti_rebond_independants();
  test_bouton_claude_tenu_au_demarrage_sans_rappel();
  test_rappel_claude_evenement_qui_change_y_met_fin();
  test_bouton_claude_travaille_avant_fini();
  test_bouton_claude_attend_avant_travaille();
  test_bouton_claude_travaille_la_plus_recente();
  test_claude_working_hors_rappel_ne_change_rien();
  test_sommeil_a_2_min_pile_pas_avant();
  test_evenements_reguliers_empechent_le_sommeil();
  test_sommeil_oublie_les_sessions();
  test_sommeil_pendant_un_rappel_retour_a_endormi();
  test_evenements_reveillent_et_s_appliquent();
  test_reveil_sans_son_et_bouton_ne_reveille_pas();
  test_pas_de_sommeil_sans_wifi();
  test_wifi_perdu_pendant_le_sommeil_reveille_sur_repos();
  test_sommeil_2_min_apres_le_retour_du_wifi();
  test_volume_premiere_lecture_fixe_le_volume_proportionnel();
  test_volume_zones_mortes_butees_exactes_malgre_le_bruit();
  test_volume_bruit_ignore_variation_nette_prise_en_compte();
  test_volume_butee_de_pres_sans_sauter_la_zone_morte();
  puts("Tous les tests passent.");
  return 0;
}
