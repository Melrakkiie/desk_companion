#include "expressions.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static const int PIN_SDA = 21;         // ligne de données I2C
static const int PIN_SCL = 22;         // ligne d'horloge I2C
static const uint8_t ADRESSE = 0x3C;   // adresse confirmée par le scan I2C

static Adafruit_SSD1306 ecran(128, 64, &Wire, -1);  // -1 : pas de broche de reset

bool expressionsInit() {
  Wire.begin(PIN_SDA, PIN_SCL);
  return ecran.begin(SSD1306_SWITCHCAPVCC, ADRESSE);
}

static const int CENTRE_Y = ZONE_VISAGE_Y + ZONE_VISAGE_HAUTEUR / 2;

// Un dessin par Expression : chaque ticket ne remplit que le sien.
// Tous dessinent dans la Zone visage (y >= 16) ; maintenantMs sert aux animations.

// Repos : deux yeux ronds et une bouche droite, centrés dans la zone (ticket #6 : clignement)
static void dessinerRepos(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  if (etatYeuxFermes(maintenantMs)) {
    // Clignement : yeux réduits à un trait
    ecran.fillRect(33, CENTRE_Y - 7, 15, 3, SSD1306_WHITE);
    ecran.fillRect(81, CENTRE_Y - 7, 15, 3, SSD1306_WHITE);
  } else {
    ecran.fillCircle(40, CENTRE_Y - 6, 7, SSD1306_WHITE);
    ecran.fillCircle(88, CENTRE_Y - 6, 7, SSD1306_WHITE);
  }
  ecran.drawFastHLine(52, CENTRE_Y + 14, 24, SSD1306_WHITE);
}

// claude_done : yeux en "^" (épais) et grand sourire
static void dessinerContent(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  (void)maintenantMs;
  for (int d = 0; d < 2; d++) {
    ecran.drawLine(32, CENTRE_Y - 2 + d, 40, CENTRE_Y - 10 + d, SSD1306_WHITE);
    ecran.drawLine(40, CENTRE_Y - 10 + d, 48, CENTRE_Y - 2 + d, SSD1306_WHITE);
    ecran.drawLine(80, CENTRE_Y - 2 + d, 88, CENTRE_Y - 10 + d, SSD1306_WHITE);
    ecran.drawLine(88, CENTRE_Y - 10 + d, 96, CENTRE_Y - 2 + d, SSD1306_WHITE);
  }
  // Sourire = demi-cercle du bas : on dessine le cercle puis on efface sa moitié haute
  ecran.drawCircle(64, CENTRE_Y + 4, 13, SSD1306_WHITE);
  ecran.fillRect(48, CENTRE_Y - 10, 33, 14, SSD1306_BLACK);
}

// claude_waiting : grands yeux avec pupilles, sourcils levés, bouche ouverte
static void dessinerInterpelle(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  (void)maintenantMs;
  ecran.drawCircle(40, CENTRE_Y - 4, 9, SSD1306_WHITE);
  ecran.drawCircle(88, CENTRE_Y - 4, 9, SSD1306_WHITE);
  ecran.fillCircle(40, CENTRE_Y - 4, 4, SSD1306_WHITE);
  ecran.fillCircle(88, CENTRE_Y - 4, 4, SSD1306_WHITE);
  ecran.drawLine(30, CENTRE_Y - 17, 50, CENTRE_Y - 20, SSD1306_WHITE);
  ecran.drawLine(78, CENTRE_Y - 20, 98, CENTRE_Y - 17, SSD1306_WHITE);
  ecran.drawCircle(64, CENTRE_Y + 15, 5, SSD1306_WHITE);
}

// Jeu : yeux du Repos mi-clos sous des sourcils froncés (même clignement que le Repos),
// bouche serrée, petite manette en bas à droite (à côté du visage, pour ne pas passer pour une bouche)
static void dessinerJeu(const EtatCourant& e, unsigned long maintenantMs) {
  (void)e;
  if (etatYeuxFermes(maintenantMs)) {
    ecran.fillRect(33, CENTRE_Y - 7, 15, 3, SSD1306_WHITE);
    ecran.fillRect(81, CENTRE_Y - 7, 15, 3, SSD1306_WHITE);
  } else {
    ecran.fillCircle(40, CENTRE_Y - 6, 7, SSD1306_WHITE);
    ecran.fillCircle(88, CENTRE_Y - 6, 7, SSD1306_WHITE);
    ecran.fillRect(32, CENTRE_Y - 13, 64, 5, SSD1306_BLACK);  // paupières : moitié haute masquée
  }
  for (int d = 0; d < 2; d++) {  // sourcils froncés, fixes pendant le clignement
    ecran.drawLine(30, CENTRE_Y - 17 + d, 48, CENTRE_Y - 13 + d, SSD1306_WHITE);
    ecran.drawLine(98, CENTRE_Y - 17 + d, 80, CENTRE_Y - 13 + d, SSD1306_WHITE);
  }
  ecran.drawFastHLine(58, CENTRE_Y + 14, 12, SSD1306_WHITE);
  // manette : corps arrondi, croix à gauche, deux boutons à droite
  ecran.fillRoundRect(101, CENTRE_Y + 6, 25, 13, 4, SSD1306_WHITE);
  ecran.fillRect(104, CENTRE_Y + 11, 7, 3, SSD1306_BLACK);
  ecran.fillRect(106, CENTRE_Y + 9, 3, 7, SSD1306_BLACK);
  ecran.fillCircle(117, CENTRE_Y + 14, 1, SSD1306_BLACK);
  ecran.fillCircle(121, CENTRE_Y + 11, 1, SSD1306_BLACK);
}

// Une ligne du Bandeau : fixe si elle tient, sinon défile en boucle (2 copies côte à côte)
static void ligneDefilante(const char* texte, int y, unsigned long tempsMs) {
  int largeur = etatLargeurTexte(texte);
  int x = etatPositionDefilement(largeur, tempsMs);
  ecran.setCursor(x, y);
  ecran.print(texte);
  if (largeur > LARGEUR_ECRAN_PX) {
    ecran.setCursor(x + largeur + ECART_DEFILEMENT_PX, y);
    ecran.print(texte);
  }
}

// Une croche : tête pleine, hampe, drapeau
static void dessinerNote(int x, int y) {
  ecran.fillCircle(x, y, 3, SSD1306_WHITE);
  ecran.drawFastVLine(x + 3, y - 12, 12, SSD1306_WHITE);
  ecran.drawLine(x + 3, y - 12, x + 8, y - 8, SSD1306_WHITE);
}

// music : visage de Repos entouré de notes qui rebondissent (titre et artiste : voir le Bandeau)
static void dessinerMusique(const EtatCourant& e, unsigned long maintenantMs) {
  unsigned long t = maintenantMs - e.debutMs;
  dessinerRepos(e, maintenantMs);
  int rebond = (t / NOTE_REBOND_PERIODE_MS) % 2 ? 3 : 0;
  dessinerNote(14, CENTRE_Y + 4 - rebond);
  dessinerNote(112, CENTRE_Y - 6 + rebond);
}

// silence (Rappel sans Morceau courant) : visage de Repos avec une note barrée
static void dessinerSilence(const EtatCourant& e, unsigned long maintenantMs) {
  dessinerRepos(e, maintenantMs);
  dessinerNote(64, CENTRE_Y + 14);
  ecran.drawLine(50, CENTRE_Y + 18, 78, CENTRE_Y - 8, SSD1306_WHITE);
}

// Sans Claude (Rappel Claude sans Session connue) : visage neutre, le Bandeau dit « pas de Claude »
static void dessinerSansClaude(const EtatCourant& e, unsigned long maintenantMs) {
  dessinerRepos(e, maintenantMs);
}

// git_push : yeux qui regardent vers le haut (pupilles en haut) et flèche qui monte entre eux
static void dessinerPush(const EtatCourant& e, unsigned long maintenantMs) {
  const int yeuxY = CENTRE_Y - 6;
  for (int x = 40; x <= 88; x += 48) {
    ecran.drawCircle(x, yeuxY, 9, SSD1306_WHITE);
    ecran.fillCircle(x, yeuxY - 4, 4, SSD1306_WHITE);
  }
  int haut = etatPushFlecheY(maintenantMs - e.debutMs);  // toujours dans la Zone visage
  ecran.fillTriangle(64, haut, 56, haut + 9, 72, haut + 9, SSD1306_WHITE);
  ecran.fillRect(61, haut + 9, 7, PUSH_FLECHE_HAUTEUR - 9, SSD1306_WHITE);
}

void expressionsAfficher(const EtatCourant& e, const char* texteBandeau, unsigned long maintenantMs) {
  ecran.clearDisplay();
  ecran.setTextColor(SSD1306_WHITE);
  ecran.setTextSize(1);
  ecran.cp437(true);         // les textes de l'État sont déjà en CP437 (voir etatVersCp437)
  ecran.setTextWrap(false); // un texte trop long est coupé au bord, jamais replié sous le Bandeau

  // Bandeau, pour toutes les Expressions : une ligne centrée verticalement, ou deux lignes
  // (y = 0 et 8, police 8 px : rien ne dépasse y = 16). Chaque ligne trop large défile.
  unsigned long t = maintenantMs - e.debutMs;
  bool deuxLignes = e.ligne2[0] != '\0';
  if (texteBandeau != nullptr) ligneDefilante(texteBandeau, deuxLignes ? 0 : (BANDEAU_HAUTEUR - 8) / 2, t);
  if (deuxLignes) ligneDefilante(e.ligne2, 8, t);

  switch (e.expression) {
    case EXPR_CONTENT:    dessinerContent(e, maintenantMs); break;
    case EXPR_INTERPELLE: dessinerInterpelle(e, maintenantMs); break;
    case EXPR_MUSIQUE:    dessinerMusique(e, maintenantMs); break;
    case EXPR_PUSH:       dessinerPush(e, maintenantMs); break;
    case EXPR_JEU:        dessinerJeu(e, maintenantMs); break;
    case EXPR_SILENCE:    dessinerSilence(e, maintenantMs); break;
    case EXPR_SANS_CLAUDE: dessinerSansClaude(e, maintenantMs); break;
    case EXPR_REPOS:
    default:             dessinerRepos(e, maintenantMs); break;
  }

  ecran.display();
}
