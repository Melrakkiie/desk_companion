#!/usr/bin/env bash
# Tests du hook git de push : dépôt jetable + remote nu local, hook déclaré dans un
# gitconfig global temporaire (GIT_CONFIG_GLOBAL), vrais git push / fetch,
# requêtes captées par un petit serveur HTTP local (python3).
set -u
cd "$(dirname "$0")/.."
HOOK=$PWD/hooks/compagnon-push.sh
TMP=$(mktemp -d)
trap 'kill $SRV 2>/dev/null; rm -rf "$TMP"' EXIT
echec=0

python3 -I - "$TMP/port" "$TMP/log" <<'PY' &
import sys, http.server
class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        corps = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode()
        with open(sys.argv[2], "a") as f:
            f.write(f"{self.path} {self.headers.get('Content-Type')} {corps}\n")
        self.send_response(200); self.end_headers()
    def log_message(self, *a): pass
s = http.server.HTTPServer(("127.0.0.1", 0), H)
open(sys.argv[1], "w").write(str(s.server_port))
s.serve_forever()
PY
SRV=$!
for _ in $(seq 50); do [ -s "$TMP/port" ] && break; sleep 0.1; done
export COMPAGNON_URL="http://127.0.0.1:$(cat "$TMP/port")"

# Config git isolée : jamais le vrai ~/.gitconfig.
export GIT_CONFIG_GLOBAL=$TMP/gitconfig GIT_CONFIG_NOSYSTEM=1
git config --global user.name test
git config --global user.email test@example.com
git config --global init.defaultBranch main
git config --global hook.compagnon-push.command "$HOOK"
git config --global hook.compagnon-push.event reference-transaction

git init -q --bare "$TMP/distant.git"
git init -q "$TMP/mon_projet"
cd "$TMP/mon_projet"
git remote add origin "$TMP/distant.git"
commit() { echo "$RANDOM" >> f; git add f; git commit -qm "$1"; }
commit un

# verifie <nom> <attendu : journal complet> <commande...>
verifie() {
  nom=$1; attendu=$2; shift 2
  : > "$TMP/log"
  "$@" >/dev/null 2>&1
  sleep 0.6  # laisse le temps à l'envoi détaché, et à un éventuel envoi en trop
  recu=$(cat "$TMP/log")
  if [ "$recu" = "$attendu" ]; then echo "ok   $nom"; else echo "ECHEC $nom : reçu '$recu', attendu '$attendu'"; echec=1; fi
}
evt() { printf '/event application/json {"type":"git_push","text":"%s\\nmon_projet"}' "$1"; }

verifie "push qui fait avancer une branche" "$(evt main)" git push origin main
verifie "push déjà à jour : rien" "" git push origin main

git checkout -q -b b2; commit deux
git checkout -q -b b3; commit trois
verifie "plusieurs branches : un seul Événement, la première" "$(evt b2)" git push origin b2 b3

git checkout -q b2; git commit -q --amend -m deux-bis
verifie "force-push : un Événement" "$(evt b2)" git push --force origin b2

verifie "suppression de branche distante : rien" "" git push origin --delete b3

git tag v1
verifie "tags seuls : rien" "" git push origin --tags

# push refusé (non fast-forward) : un autre clone avance main
git clone -q "$TMP/distant.git" "$TMP/autre"
( cd "$TMP/autre"; echo x >> f; git commit -qam autre; git push -q origin main ) >/dev/null 2>&1
git checkout -q main; commit local
verifie "push refusé (non fast-forward) : rien" "" git push origin main

# fetch qui ramène des commits : rien
verifie "fetch qui ramène des commits : rien" "" git fetch origin

# "push" n'est pas le sous-commande : `git -C push fetch` (dossier nommé push) ne doit rien envoyer
git clone -q "$TMP/distant.git" "$TMP/push"
( cd "$TMP/autre"; git checkout -q -b cote; echo y >> f; git commit -qam autre2; git push -q origin cote ) >/dev/null 2>&1
sleep 0.6  # l'envoi du push ci-dessus ne doit pas tomber dans le journal du test
verifie "fetch avec un argument « push » : rien" "" env -C "$TMP" git -C push fetch origin

# distant inexistant
git remote add fantome "$TMP/nulle_part.git"
verifie "distant inexistant : rien" "" git push fantome main

# Hook local du dépôt : s'exécute à côté du hook global
mkdir -p .git/hooks
printf '#!/bin/sh\n[ "$1" = prepared ] && echo local >> "%s/local"\nexit 0\n' "$TMP" > .git/hooks/reference-transaction
chmod +x .git/hooks/reference-transaction
git reset -q --hard origin/main; commit cinq
verifie "hook local : le global envoie toujours" "$(evt main)" git push origin main
if [ -s "$TMP/local" ]; then echo "ok   hook local exécuté à côté du global"; else echo "ECHEC hook local non exécuté"; echec=1; fi
rm .git/hooks/reference-transaction

# Compagnon injoignable : le push réussit quand même, vite.
commit quatre
debut=$(date +%s%N)
COMPAGNON_URL=http://10.255.255.1 git push origin main >/dev/null 2>&1
code=$?
ms=$(( ($(date +%s%N) - debut) / 1000000 ))
if [ "$code" = 0 ] && [ "$ms" -lt 1500 ] && [ "$(git rev-parse origin/main)" = "$(git rev-parse main)" ]; then echo "ok   cible injoignable (${ms} ms)"; else echo "ECHEC cible injoignable : code $code, ${ms} ms"; echec=1; fi

exit $echec
