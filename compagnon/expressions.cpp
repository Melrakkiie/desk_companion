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

void expressionsAfficher(Expression expression, const char* texteBandeau) {
  ecran.clearDisplay();
  ecran.setTextColor(SSD1306_WHITE);
  ecran.setTextSize(1);

  // Bandeau : texte en haut, centré verticalement dans les 16 lignes
  if (texteBandeau != nullptr && texteBandeau[0] != '\0') {
    ecran.setCursor(2, (BANDEAU_HAUTEUR - 8) / 2);
    ecran.print(texteBandeau);
  }

  const int centreY = ZONE_VISAGE_Y + ZONE_VISAGE_HAUTEUR / 2;
  if (expression == EXPR_CONTENT) {
    // Zone visage : yeux en "^" (épais) et grand sourire
    for (int d = 0; d < 2; d++) {
      ecran.drawLine(32, centreY - 2 + d, 40, centreY - 10 + d, SSD1306_WHITE);
      ecran.drawLine(40, centreY - 10 + d, 48, centreY - 2 + d, SSD1306_WHITE);
      ecran.drawLine(80, centreY - 2 + d, 88, centreY - 10 + d, SSD1306_WHITE);
      ecran.drawLine(88, centreY - 10 + d, 96, centreY - 2 + d, SSD1306_WHITE);
    }
    // Sourire = demi-cercle du bas : on dessine le cercle puis on efface sa moitié haute
    ecran.drawCircle(64, centreY + 4, 13, SSD1306_WHITE);
    ecran.fillRect(48, centreY - 10, 33, 14, SSD1306_BLACK);
  } else {
    // Repos : deux yeux ronds et une bouche droite, centrés dans la zone
    ecran.fillCircle(40, centreY - 6, 7, SSD1306_WHITE);
    ecran.fillCircle(88, centreY - 6, 7, SSD1306_WHITE);
    ecran.drawFastHLine(52, centreY + 14, 24, SSD1306_WHITE);
  }

  ecran.display();
}
