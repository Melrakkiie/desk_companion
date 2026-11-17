#!/usr/bin/env bash
# Tests du script jeu : liste de processus factice en entrée (à la place de /proc), dossier Steam factice,
# requêtes captées par un petit serveur HTTP local (python3), corps JSON vérifiés.
set -u
cd "$(dirname "$0")/.."
SCRIPT=hooks/compagnon-jeu.sh
TMP=$(mktemp -d)
PIDS=""
trap 'for p in $PIDS; do kill -- -$p 2>/dev/null; kill $p 2>/dev/null; done; rm -rf "$TMP"' EXIT
echec=0
S=$'\x1f'  # séparateur d'arguments : un processus par ligne

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
# lance : le script dans son propre groupe de processus (setsid) ; pid dans $pid.
lance() { setsid bash "$SCRIPT" & pid=$!; PIDS="$PIDS $pid"; }
arrete() { kill -- -$pid 2>/dev/null; wait $pid 2>/dev/null; }
# attend FICHIER N : attend (5 s max) que FICHIER ait au moins N lignes.
attend() { for _ in $(seq 50); do [ "$(wc -l < "$1")" -ge "$2" ] && return; sleep 0.1; done; }
# nouveau_serveur N : serveur dans log$N, URL dans $URL.
nouveau_serveur() {
  : > "$TMP/log$1"; serveur "$TMP/log$1" "$TMP/port$1"
  for _ in $(seq 50); do [ -s "$TMP/port$1" ] && break; sleep 0.1; done
  URL="http://127.0.0.1:$(cat "$TMP/port$1")"
}
# proc ARG... : une ligne de processus, arguments séparés par \x1f.
proc() { local IFS=$S; printf '%s\n' "$*"; }
# verifie NOM ATTENDU : compare le journal reçu (dédoublonné) à ATTENDU.
verifie() {
  if [ "$(sort -u "$TMP/log$2")" = "$3" ]; then echo "ok   $1"
  else echo "ECHEC $1, reçu :"; cat "$TMP/log$2"; echec=1; fi
}

# Dossier Steam factice : deux bibliothèques ; un jeu avec guillemets et accents, un dans la 2e bibliothèque.
STEAM=$TMP/steam
mkdir -p "$STEAM/steamapps" "$TMP/lib2/steamapps"
printf '"libraryfolders"\n{\n\t"0"\n\t{\n\t\t"path"\t\t"%s"\n\t}\n\t"1"\n\t{\n\t\t"path"\t\t"%s"\n\t}\n}\n' "$STEAM" "$TMP/lib2" > "$STEAM/steamapps/libraryfolders.vdf"
printf '"AppState"\n{\n\t"appid"\t\t"111"\n\t"name"\t\t"Café \\"Noir\\" Été"\n}\n' > "$STEAM/steamapps/appmanifest_111.acf"
printf '"AppState"\n{\n\t"name"\t\t"Rivals of Aether II"\n}\n' > "$TMP/lib2/steamapps/appmanifest_222.acf"
printf '"AppState"\n{\n\t"name"\t\t"Proton Experimental"\n}\n' > "$STEAM/steamapps/appmanifest_1493710.acf"
printf '"AppState"\n{\n\t"name"\t\t"A\\\\B"\n}\n' > "$STEAM/steamapps/appmanifest_444.acf"
export COMPAGNON_JEU_STEAM_DIR=$STEAM COMPAGNON_JEU_RENVOI_S=30 COMPAGNON_JEU_SCAN_S=0.1

# Cas : source factice, première annonce attendue (ligne unique de journal).
cas() {  # cas NUM NOM ATTENDU : le premier Événement reçu avec la source de $TMP/procs
  nouveau_serveur "$1"
  COMPAGNON_URL=$URL COMPAGNON_JEU_SOURCE="cat $TMP/procs" lance
  attend "$TMP/log$1" 1; sleep 0.3
  arrete
  verifie "$2" "$1" "$3"
}

{
  proc /usr/bin/bash -c "echo reaper SteamLaunch AppId=999"   # simple mention : pas un jeu
  proc "$HOME/.local/share/Steam/ubuntu12_32/reaper" SteamLaunch AppId=111 -- /x/_v2-entry-point --verb=waitforexitandrun -- /x/proton waitforexitandrun /jeu.exe
} > "$TMP/procs"
cas 1 "jeu Steam : game avec le nom du manifest, guillemets et accents en JSON valide" \
  '/event application/json {"type":"game","text":"Café \"Noir\" Été"}'
jq -e . >/dev/null <<< "$(cut -d' ' -f3- "$TMP/log1" | head -n1)" || { echo "ECHEC JSON invalide"; echec=1; }

proc "/opt/steam/reaper" SteamLaunch AppId=222 -- /jeu > "$TMP/procs"
cas 2 "manifest dans la 2e bibliothèque" '/event application/json {"type":"game","text":"Rivals of Aether II"}'

proc "/opt/steam/reaper" SteamLaunch AppId=333 -- /jeu > "$TMP/procs"
cas 3 "manifest introuvable : Steam <id>" '/event application/json {"type":"game","text":"Steam 333"}'

proc "/opt/steam/reaper" SteamLaunch AppId=444 -- /jeu > "$TMP/procs"
cas 12 "manifest avec antislash : \\\\ devient \\" '/event application/json {"type":"game","text":"A\\B"}'

{ proc /usr/bin/steam; proc "/opt/steam/reaper" SteamLaunch AppId=1493710 -- /proton; proc /usr/bin/bash; } > "$TMP/procs"
cas 4 "client Steam et outils seulement : game_stop" '/event application/json {"type":"game_stop"}'

# Lutris : argv[0] est python3, le nom du jeu suit l'argument `lutris-wrapper`.
{ proc /usr/bin/python3 /app/bin/lutris; proc /usr/bin/bash -c "echo lutris-wrapper Fake 0 0"
  proc /usr/bin/python3 /app/share/lutris/bin/lutris-wrapper "Guild Wars 2" 0 0 gamemoderun /x/umu-run /x/Gw2-64.exe; } > "$TMP/procs"
cas 9 "jeu Lutris : game avec son nom (espaces conservés)" '/event application/json {"type":"game","text":"Guild Wars 2"}'

{ proc bwrap --args 80 -- lutris; proc /usr/bin/python3 /app/bin/lutris; } > "$TMP/procs"
cas 10 "client Lutris seul : game_stop" '/event application/json {"type":"game_stop"}'

# Steam prioritaire, même si le processus Lutris est listé avant.
{ proc /usr/bin/python3 /app/share/lutris/bin/lutris-wrapper "Guild Wars 2" 0 0 /x/Gw2-64.exe
  proc "/opt/steam/reaper" SteamLaunch AppId=222 -- /jeu; } > "$TMP/procs"
cas 11 "Steam et Lutris ensemble : un seul game, celui de Steam" '/event application/json {"type":"game","text":"Rivals of Aether II"}'

# Changement : un jeu qui se lance puis se ferme est annoncé tout de suite (renvoi à 30 s).
: > "$TMP/procs"
nouveau_serveur 5
COMPAGNON_URL=$URL COMPAGNON_JEU_SOURCE="cat $TMP/procs" lance
attend "$TMP/log5" 1
proc "/opt/steam/reaper" SteamLaunch AppId=222 -- /jeu > "$TMP/procs"
attend "$TMP/log5" 2
: > "$TMP/procs"
attend "$TMP/log5" 3
arrete
if [ "$(cat "$TMP/log5")" = "$(printf '%s\n' '/event application/json {"type":"game_stop"}' '/event application/json {"type":"game","text":"Rivals of Aether II"}' '/event application/json {"type":"game_stop"}')" ]; then echo "ok   un Événement par changement : game_stop, game, game_stop"
else echo "ECHEC changements, reçu :"; cat "$TMP/log5"; echec=1; fi

# Renvoi périodique de l'état courant (période remplaçable).
proc "/opt/steam/reaper" SteamLaunch AppId=222 -- /jeu > "$TMP/procs"
nouveau_serveur 6
COMPAGNON_URL=$URL COMPAGNON_JEU_RENVOI_S=0.3 COMPAGNON_JEU_SOURCE="cat $TMP/procs" lance
attend "$TMP/log6" 4
arrete
n=$(wc -l < "$TMP/log6")
if [ "$n" -ge 4 ]; then verifie "game renvoyé périodiquement" 6 '/event application/json {"type":"game","text":"Rivals of Aether II"}'
else echo "ECHEC renvoi ($n envois)"; echec=1; fi

# Compagnon injoignable : le script continue, puis l'état est rattrapé quand la cible revient.
PORT=$(python3 -I -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
COMPAGNON_URL=http://127.0.0.1:$PORT COMPAGNON_JEU_RENVOI_S=0.4 COMPAGNON_JEU_SOURCE="cat $TMP/procs" lance
sleep 0.8
if kill -0 $pid 2>/dev/null; then echo "ok   cible injoignable : le script continue"; else echo "ECHEC script mort"; echec=1; fi
: > "$TMP/log7"
serveur "$TMP/log7" "$TMP/port7" "$PORT"
attend "$TMP/log7" 1
arrete
verifie "cible rallumée : l'état est rattrapé au renvoi suivant" 7 '/event application/json {"type":"game","text":"Rivals of Aether II"}'

# Source réelle : /proc de cette machine, sans jeu Steam lancé par ce test → pas d'échec de lecture.
nouveau_serveur 8
COMPAGNON_URL=$URL COMPAGNON_JEU_SOURCE= lance
attend "$TMP/log8" 1
arrete
if grep -q '"type":"game' "$TMP/log8"; then echo "ok   lecture réelle de /proc"; else echo "ECHEC lecture réelle de /proc"; echec=1; fi

exit $echec
