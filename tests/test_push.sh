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

exit $echec
