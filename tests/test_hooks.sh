#!/usr/bin/env bash
# Tests du script des hooks : JSON de hook en entrée, requêtes captées par un
# petit serveur HTTP local (python3), corps JSON vérifiés.
set -u
cd "$(dirname "$0")/.."
HOOK=hooks/compagnon-hook.sh
TMP=$(mktemp -d)
trap 'kill $SRV 2>/dev/null; rm -rf "$TMP"' EXIT
echec=0

# Serveur de test : écrit chaque requête (en-tête Content-Type + corps) sur une ligne du journal.
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

# verifie <nom> <json du hook> <ligne attendue dans le journal>
verifie() {
  : > "$TMP/log"
  printf '%s' "$2" | bash "$HOOK"
  for _ in $(seq 30); do [ -s "$TMP/log" ] && break; sleep 0.1; done
  recu=$(head -n1 "$TMP/log")
  if [ "$recu" = "$3" ]; then echo "ok   $1"; else echo "ECHEC $1 : reçu '$recu', attendu '$3'"; echec=1; fi
}

verifie "Stop -> claude_done" \
  '{"hook_event_name":"Stop","cwd":"/home/a/Projects/mon_projet"}' \
  '/event application/json {"type":"claude_done","text":"mon_projet"}'
verifie "Notification autorisation -> claude_waiting" \
  '{"hook_event_name":"Notification","notification_type":"permission_prompt","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_waiting","text":"autorisation ? mon_projet"}'
verifie "Notification attente -> claude_waiting" \
  '{"hook_event_name":"Notification","notification_type":"idle_prompt","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_waiting","text":"à toi ! mon_projet"}'
verifie "Notification d'un autre type -> rien envoyé" \
  '{"hook_event_name":"Notification","notification_type":"auth_success","cwd":"/x/mon_projet"}' \
  ''
verifie "UserPromptSubmit -> claude_working" \
  '{"hook_event_name":"UserPromptSubmit","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_working","text":"mon_projet"}'
verifie "PostToolUse -> claude_working" \
  '{"hook_event_name":"PostToolUse","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_working","text":"mon_projet"}'

# Cible injoignable : le script rend la main vite, code 0, sans rien écrire.
debut=$(date +%s%N)
printf '%s' '{"hook_event_name":"Stop","cwd":"/x/p"}' | COMPAGNON_URL=http://10.255.255.1 bash "$HOOK"
code=$?
ms=$(( ($(date +%s%N) - debut) / 1000000 ))
if [ "$code" = 0 ] && [ "$ms" -lt 500 ]; then echo "ok   cible injoignable (${ms} ms)"; else echo "ECHEC cible injoignable : code $code, ${ms} ms"; echec=1; fi

exit $echec
