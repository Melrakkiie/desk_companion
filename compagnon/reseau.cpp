#include "reseau.h"

#include <WiFi.h>
#include <ESPmDNS.h>

#include "secrets.h"

// Durée réseau, laissée ici (couche Réseau) plutôt que dans etat.h : voir le bloc de durées d'etat.h.
static const unsigned long DELAI_NOUVELLE_TENTATIVE_MS = 10000;  // 10 s entre deux tentatives
static const char* NOM_MDNS = "compagnon";                       // => compagnon.local

static bool etaitConnecte = false;
static unsigned long derniereTentativeMs = 0;

static void lancerTentative() {
  Serial.println("Wi-Fi : tentative de connexion...");
  WiFi.disconnect();  // repart d'un état propre avant de réessayer
  WiFi.begin(WIFI_SSID, WIFI_MOT_DE_PASSE);
  derniereTentativeMs = millis();
}

void reseauInit() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // c'est ce module qui gère les nouvelles tentatives
  lancerTentative();
}

void reseauMiseAJour() {
  bool connecte = (WiFi.status() == WL_CONNECTED);

  if (connecte && !etaitConnecte) {
    Serial.print("Wi-Fi : connecté, adresse IP ");
    Serial.println(WiFi.localIP());
    // mDNS doit être republié après chaque reconnexion
    MDNS.end();
    if (MDNS.begin(NOM_MDNS)) {
      Serial.println("mDNS : compagnon.local publié");
    } else {
      Serial.println("mDNS : échec de la publication");
    }
  } else if (!connecte && etaitConnecte) {
    Serial.println("Wi-Fi : connexion perdue");
    MDNS.end();
    derniereTentativeMs = millis();  // première nouvelle tentative dans 10 s
  } else if (!connecte && millis() - derniereTentativeMs >= DELAI_NOUVELLE_TENTATIVE_MS) {
    lancerTentative();
  }

  etaitConnecte = connecte;
}

bool reseauConnecte() {
  return etaitConnecte;
}
