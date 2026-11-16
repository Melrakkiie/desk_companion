// Programme jetable : scanne le bus I2C et affiche les adresses qui répondent.
// Sert à vérifier le câblage de l'écran OLED (0x3C attendu, soit 0x78 en 8 bits).
#include <Wire.h>

const int PIN_SDA = 21;  // ligne de données I2C
const int PIN_SCL = 22;  // ligne d'horloge I2C

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  delay(1000);  // laisse le temps d'ouvrir le moniteur série
}

void loop() {
  Serial.println("Scan I2C en cours...");
  int trouves = 0;

  // Une adresse I2C valide va de 0x01 à 0x7E ; un appareil qui répond acquitte (erreur 0).
  for (uint8_t adresse = 1; adresse < 127; adresse++) {
    Wire.beginTransmission(adresse);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Appareil trouvé à l'adresse 0x%02X\n", adresse);
      trouves++;
    }
  }

  if (trouves == 0) {
    Serial.println("Aucun appareil I2C ne répond : vérifie le câblage (SDA=21, SCL=22, alimentation).");
  } else {
    Serial.printf("Scan terminé : %d appareil(s).\n", trouves);
  }

  Serial.println();
  delay(5000);  // nouveau scan toutes les 5 secondes
}
