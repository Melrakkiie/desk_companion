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

// music : titre (ligne 1) et artiste (ligne 2) dans le Bandeau, petite police ;
// visage de Repos entouré de notes qui rebondissent
static void dessinerMusique(const EtatCourant& e, unsigned long maintenantMs) {
  unsigned long t = maintenantMs - e.debutMs;
  ligneDefilante(e.texte, 0, t);
  ligneDefilante(e.artiste, 8, t);
  dessinerRepos(e, maintenantMs);
  int rebond = (t / NOTE_REBOND_PERIODE_MS) % 2 ? 3 : 0;
  dessinerNote(14, CENTRE_Y + 4 - rebond);
  dessinerNote(112, CENTRE_Y - 6 + rebond);
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

  // Bandeau : texte en haut, centré verticalement dans les 16 lignes
  // (la musique dessine son propre Bandeau à 2 lignes)
  if (e.expression != EXPR_MUSIQUE && texteBandeau != nullptr && texteBandeau[0] != '\0') {
    ecran.setCursor(2, (BANDEAU_HAUTEUR - 8) / 2);
    ecran.print(texteBandeau);
  }

  switch (e.expression) {
    case EXPR_CONTENT:    dessinerContent(e, maintenantMs); break;
    case EXPR_INTERPELLE: dessinerInterpelle(e, maintenantMs); break;
    case EXPR_MUSIQUE:    dessinerMusique(e, maintenantMs); break;
    case EXPR_PUSH:       dessinerPush(e, maintenantMs); break;
    case EXPR_REPOS:
    default:              dessinerRepos(e, maintenantMs); break;
  }

  ecran.display();
}
