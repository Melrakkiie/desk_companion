#!/usr/bin/env bash
# Tests du script musique : lignes de métadonnées d'exemple en entrée (à la place de playerctl),
# requêtes captées par un petit serveur HTTP local (python3), corps JSON vérifiés.
set -u
cd "$(dirname "$0")/.."
SCRIPT=hooks/compagnon-musique.sh
TMP=$(mktemp -d)
trap 'kill $SRV 2>/dev/null; rm -rf "$TMP"' EXIT
echec=0
S=$'\x1f'  # séparateur de champs du format : statut, artiste, titre

python3 -I - "$TMP/port" "$TMP/log" <<'PY' &
import sys, http.server
class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        corps = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode()
        with open(sys.argv[2], "a", encoding="utf-8") as f:
            f.write(f"{self.path} {self.headers.get('Content-Type')} {corps}\n")
        self.send_response(200); self.end_headers()
    def log_message(self, *a): pass
s = http.server.HTTPServer(("127.0.0.1", 0), H)
open(sys.argv[1], "w").write(str(s.server_port))
s.serve_forever()
PY
SRV=$!
for _ in $(seq 50); do [ -s "$TMP/port" ] && break; sleep 0.1; done
URL="http://127.0.0.1:$(cat "$TMP/port")"

# Lignes jouées : deux morceaux, avec une pause/reprise et un doublon de métadonnées au milieu.
{
  printf 'Playing%sBjörk%sHyperballad\n' "$S" "$S"
  printf 'Paused%sBjörk%sHyperballad\n' "$S" "$S"
  printf 'Playing%sBjörk%sHyperballad\n' "$S" "$S"
  printf 'Playing%sBjörk%sHyperballad\n' "$S" "$S"
  printf 'Paused%s%s\n' "$S" "$S"
  printf 'Playing%sMylène Farmer%sÀ quoi je sers…\n' "$S" "$S"
} > "$TMP/lignes"

: > "$TMP/log"
# Le script boucle sans fin (relance de la source) : on le laisse tourner un peu puis on l'arrête.
COMPAGNON_URL=$URL COMPAGNON_MUSIQUE_SOURCE="cat $TMP/lignes; sleep 3" \
  timeout 1.5 bash "$SCRIPT"
code=$?
attendu=$'/event application/json {"type":"music","text":"Hyperballad","artist":"Björk"}\n/event application/json {"type":"music","text":"À quoi je sers…","artist":"Mylène Farmer"}'
if [ "$code" = 124 ] && [ "$(cat "$TMP/log")" = "$attendu" ]; then echo "ok   un Événement par morceau, accents, sans doublon ni renvoi après pause"
else echo "ECHEC morceaux : code $code, reçu :"; cat "$TMP/log"; echec=1; fi

# Compagnon injoignable : le script continue de tourner (arrêté ici par le timeout, code 124).
COMPAGNON_URL=http://127.0.0.1:1 COMPAGNON_MUSIQUE_SOURCE="cat $TMP/lignes" COMPAGNON_MUSIQUE_RELANCE_S=0.2 \
  timeout 1.5 bash "$SCRIPT"
code=$?
if [ "$code" = 124 ]; then echo "ok   cible injoignable : le script continue"; else echo "ECHEC cible injoignable : code $code"; echec=1; fi

exit $echec
