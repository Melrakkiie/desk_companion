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
  (void)maintenantMs;
  ecran.fillCircle(40, CENTRE_Y - 6, 7, SSD1306_WHITE);
  ecran.fillCircle(88, CENTRE_Y - 6, 7, SSD1306_WHITE);
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

// Ticket #7 : claude_waiting (en attendant : visage de Repos)
static void dessinerInterpelle(const EtatCourant& e, unsigned long maintenantMs) {
  dessinerRepos(e, maintenantMs);
}

// Ticket #8 : music (en attendant : visage de Repos)
static void dessinerMusique(const EtatCourant& e, unsigned long maintenantMs) {
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

  // Bandeau : texte en haut, centré verticalement dans les 16 lignes
  // (tickets #8 : la musique dessine son propre Bandeau à 2 lignes)
  if (texteBandeau != nullptr && texteBandeau[0] != '\0') {
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
