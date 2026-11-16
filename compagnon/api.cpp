#include "api.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include "etat.h"

static const size_t CORPS_MAX = 512;  // octets

static WebServer serveur(80);
static bool expressionChangee = false;

// Répond et journalise (une ligne par requête, acceptée ou refusée, avec la raison).
static void repondre(int code, const char* message) {
  Serial.printf("API : %d %s\n", code, message);
  char corps[96];
  snprintf(corps, sizeof(corps), "{\"%s\":\"%s\"}", code < 300 ? "ok" : "erreur", message);
  serveur.send(code, "application/json", corps);
}

static void traiterEvenement() {
  // shortcut: WebServer lit tout le corps en mémoire avant ce handler ; un corps énorme est
  // refusé ici mais déjà reçu. Acceptable sur réseau domestique.
  if (serveur.clientContentLength() > (int)CORPS_MAX) {
    repondre(413, "corps trop gros (512 octets max)");
    return;
  }
  const String corps = serveur.arg("plain");
  if (corps.length() == 0) {
    repondre(400, "corps vide (envoyer du JSON avec Content-Type: application/json)");
    return;
  }
  if (corps.length() > CORPS_MAX) {
    repondre(413, "corps trop gros (512 octets max)");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, corps)) {
    repondre(400, "JSON invalide");
    return;
  }
  if (!doc["type"].is<const char*>()) {
    repondre(400, "type absent");
    return;
  }
  TypeEvenement type;
  if (!etatTypeDepuisNom(doc["type"], &type)) {
    repondre(400, "type inconnu");
    return;
  }
  if (!doc["text"].isNull() && !doc["text"].is<const char*>()) {
    repondre(400, "text doit être une chaîne");
    return;
  }
  if (!doc["artist"].isNull() && !doc["artist"].is<const char*>()) {
    repondre(400, "artist doit être une chaîne");
    return;
  }

  // Le texte et l'artiste trop longs sont tronqués par le module État.
  if (etatRecevoir(type, doc["text"] | (const char*)nullptr, millis(),
                   doc["artist"] | (const char*)nullptr)) {
    expressionChangee = true;
  }
  repondre(200, "événement accepté");
}

void apiInit() {
  serveur.on("/event", HTTP_POST, traiterEvenement);
  serveur.onNotFound([]() { repondre(404, "route inconnue (POST /event uniquement)"); });
  serveur.begin();
  Serial.println("API : écoute sur le port 80");
}

bool apiMiseAJour() {
  serveur.handleClient();
  bool changee = expressionChangee;
  expressionChangee = false;
  return changee;
}
