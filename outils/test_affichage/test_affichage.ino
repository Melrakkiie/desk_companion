// Programme jetable : teste l'écran OLED SSD1306 128x64 bicolore (jaune en haut, bleu en bas).
// Sert à mesurer où se trouve réellement la frontière entre le Bandeau (jaune)
// et la Zone visage (bleue). On la suppose entre y=15 et y=16.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int PIN_SDA = 21;         // ligne de données I2C
const int PIN_SCL = 22;         // ligne d'horloge I2C
const uint8_t ADRESSE = 0x3C;   // adresse confirmée par le scan I2C

Adafruit_SSD1306 ecran(128, 64, &Wire, -1);  // -1 : pas de broche de reset

void setup() {
  Wire.begin(PIN_SDA, PIN_SCL);
  ecran.begin(SSD1306_SWITCHCAPVCC, ADRESSE);
  ecran.clearDisplay();
  ecran.setTextColor(SSD1306_WHITE);
  ecran.setTextSize(1);

  // Cadre autour de tout l'écran
  ecran.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  // Textes : un dans le Bandeau supposé, un dans la Zone visage supposée
  ecran.setCursor(4, 3);
  ecran.print("Bandeau");
  ecran.setCursor(4, 44);
  ecran.print("Zone visage");

  // Lignes de repère : 6 petits traits côte à côte, chacun sur une ligne y différente
  // (13 à 18), avec son numéro écrit juste dessous. Le trait au-dessus du "15"
  // est à y=15, celui au-dessus du "16" est à y=16.
  for (int i = 0; i < 6; i++) {
    int y = 13 + i;
    int x = 4 + i * 20;
    ecran.drawFastHLine(x, y, 16, SSD1306_WHITE);
    ecran.setCursor(x, 24);
    ecran.print(y);
  }

  ecran.display();
}

void loop() {}
