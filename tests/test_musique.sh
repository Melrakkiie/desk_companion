#!/usr/bin/env bash
# Tests du script musique : lignes de métadonnées d'exemple en entrée (à la place de playerctl),
# requêtes captées par un petit serveur HTTP local (python3), corps JSON vérifiés.
set -u
cd "$(dirname "$0")/.."
SCRIPT=hooks/compagnon-musique.sh
TMP=$(mktemp -d)
PIDS=""
trap 'for p in $PIDS; do kill -- -$p 2>/dev/null; kill $p 2>/dev/null; done; rm -rf "$TMP"' EXIT
echec=0
S=$'\x1f'  # séparateur de champs du format : statut, artiste, titre

# serveur LOG PORTFILE [PORT] : capte les POST dans LOG (port 0 = au choix, écrit dans PORTFILE).
serveur() {
  python3 -I - "$@" <<'PY' &
import sys, http.server
class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        corps = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode()
        with open(sys.argv[1], "a", encoding="utf-8") as f:
            f.write(f"{self.path} {self.headers.get('Content-Type')} {corps}\n")
        self.send_response(200); self.end_headers()
    def log_message(self, *a): pass
s = http.server.HTTPServer(("127.0.0.1", int(sys.argv[3]) if len(sys.argv) > 3 else 0), H)
open(sys.argv[2], "w").write(str(s.server_port))
s.serve_forever()
PY
  PIDS="$PIDS $!"
}

# lance : démarre le script dans son propre groupe de processus (setsid) pour pouvoir
# tout arrêter d'un coup, y compris la source `bash -c` ; pid dans $pid.
lance() { setsid bash "$SCRIPT" & pid=$!; PIDS="$PIDS $pid"; }
# arrete : tue tout le groupe puis attend sa fin.
arrete() { kill -- -$pid 2>/dev/null; wait $pid 2>/dev/null; }
# attend FICHIER N : attend (5 s max) que FICHIER ait au moins N lignes.
attend() { for _ in $(seq 50); do [ "$(wc -l < "$1")" -ge "$2" ] && return; sleep 0.1; done; }

: > "$TMP/log"
serveur "$TMP/log" "$TMP/port"
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

# Le script boucle sans fin (relance de la source) : on attend les Événements, puis on l'arrête.
# Période de renvoi longue : aucun renvoi périodique ici, seulement un Événement par ligne.
COMPAGNON_URL=$URL COMPAGNON_MUSIQUE_RENVOI_S=30 COMPAGNON_MUSIQUE_SOURCE="cat $TMP/lignes; sleep 30" lance
attend "$TMP/log" 6
sleep 0.5  # laisse passer un éventuel Événement en trop
arrete
M1='/event application/json {"type":"music","text":"Hyperballad","artist":"Björk"}'
STOP='/event application/json {"type":"music_stop"}'
# music, stop (pause), music (reprise), music (doublon : la mémoire du script n'existe plus), stop (vide), music
attendu=$(printf '%s\n' "$M1" "$STOP" "$M1" "$M1" "$STOP" '/event application/json {"type":"music","text":"À quoi je sers…","artist":"Mylène Farmer"}')
if [ "$(cat "$TMP/log")" = "$attendu" ]; then echo "ok   un Événement par ligne : music, music_stop à la pause, music à la reprise, accents"
else echo "ECHEC morceaux, reçu :"; cat "$TMP/log"; echec=1; fi

# Compagnon injoignable : le script continue de tourner (même quand la source se termine et est relancée).
COMPAGNON_URL=http://127.0.0.1:1 COMPAGNON_MUSIQUE_SOURCE="cat $TMP/lignes" COMPAGNON_MUSIQUE_RELANCE_S=0.2 lance
sleep 1.5
if kill -0 $pid 2>/dev/null; then echo "ok   cible injoignable : le script continue"; else echo "ECHEC cible injoignable : le script est mort"; echec=1; fi
arrete

# Cible injoignable au début du morceau, puis joignable : l'Événement arrive une seule fois.
PORT=$(python3 -I -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
printf 'Playing%sBjörk%sHyperballad\n' "$S" "$S" > "$TMP/une"
COMPAGNON_URL=http://127.0.0.1:$PORT COMPAGNON_MUSIQUE_SOURCE="cat $TMP/une; sleep 30" COMPAGNON_MUSIQUE_RENVOI_S=0.5 lance
sleep 0.8  # le morceau a démarré cible éteinte
: > "$TMP/log2"
serveur "$TMP/log2" "$TMP/port2" "$PORT"
attend "$TMP/log2" 1
arrete
if [ "$(head -n 1 "$TMP/log2")" = "$M1" ]; then echo "ok   cible rallumée : l'état réel est rattrapé au renvoi suivant"
else echo "ECHEC rattrapage, reçu :"; cat "$TMP/log2"; echec=1; fi

# Renvoi périodique : pendant la lecture music, sans lecture music_stop (y compris source finie).
: > "$TMP/log4"
serveur "$TMP/log4" "$TMP/port4"
for _ in $(seq 50); do [ -s "$TMP/port4" ] && break; sleep 0.1; done
COMPAGNON_URL=http://127.0.0.1:$(cat "$TMP/port4") COMPAGNON_MUSIQUE_RENVOI_S=0.3 COMPAGNON_MUSIQUE_SOURCE="cat $TMP/une; sleep 30" lance
attend "$TMP/log4" 4
arrete
if [ "$(sort -u "$TMP/log4")" = "$M1" ] && [ "$(wc -l < "$TMP/log4")" -ge 4 ]; then echo "ok   lecture : music renvoyé périodiquement"
else echo "ECHEC renvoi music, reçu :"; cat "$TMP/log4"; echec=1; fi

: > "$TMP/log5"
serveur "$TMP/log5" "$TMP/port5"
for _ in $(seq 50); do [ -s "$TMP/port5" ] && break; sleep 0.1; done
COMPAGNON_URL=http://127.0.0.1:$(cat "$TMP/port5") COMPAGNON_MUSIQUE_RENVOI_S=0.3 COMPAGNON_MUSIQUE_SOURCE="sleep 30" lance
attend "$TMP/log5" 3
arrete
if [ "$(sort -u "$TMP/log5")" = "$STOP" ] && [ "$(wc -l < "$TMP/log5")" -ge 3 ]; then echo "ok   sans lecture : music_stop renvoyé périodiquement"
else echo "ECHEC renvoi music_stop, reçu :"; cat "$TMP/log5"; echec=1; fi

# Sans lecteur (la source se termine aussitôt, relancée toutes les 0,1 s) : music_stop à la cadence
# de renvoi (1 s), pas à celle de la relance. En 2,5 s : 3 envois attendus (t=0, 1, 2), jamais ~25.
: > "$TMP/log6"
serveur "$TMP/log6" "$TMP/port6"
for _ in $(seq 50); do [ -s "$TMP/port6" ] && break; sleep 0.1; done
COMPAGNON_URL=http://127.0.0.1:$(cat "$TMP/port6") COMPAGNON_MUSIQUE_RENVOI_S=1 COMPAGNON_MUSIQUE_RELANCE_S=0.1 COMPAGNON_MUSIQUE_SOURCE="true" lance
sleep 2.5
arrete
n=$(wc -l < "$TMP/log6")
if [ "$(sort -u "$TMP/log6")" = "$STOP" ] && [ "$n" -ge 2 ] && [ "$n" -le 4 ]; then echo "ok   sans lecteur : music_stop à la cadence de renvoi, pas de relance"
else echo "ECHEC cadence sans lecteur ($n envois), reçu :"; cat "$TMP/log6"; echec=1; fi

# Fin de la source (lecteur fermé) pendant la lecture : music_stop.
: > "$TMP/log3"
serveur "$TMP/log3" "$TMP/port3"
for _ in $(seq 50); do [ -s "$TMP/port3" ] && break; sleep 0.1; done
COMPAGNON_URL=http://127.0.0.1:$(cat "$TMP/port3") COMPAGNON_MUSIQUE_SOURCE="cat $TMP/une" COMPAGNON_MUSIQUE_RELANCE_S=30 lance
attend "$TMP/log3" 2
sleep 0.3
arrete
if [ "$(cat "$TMP/log3")" = "$(printf '%s\n' "$M1" "$STOP")" ]; then echo "ok   fin de la source : music_stop"
else echo "ECHEC fin de source, reçu :"; cat "$TMP/log3"; echec=1; fi

exit $echec
