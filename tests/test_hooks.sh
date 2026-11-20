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
  '{"hook_event_name":"Stop","session_id":"s1","cwd":"/home/a/Projects/mon_projet"}' \
  '/event application/json {"type":"claude_done","text":"mon_projet","session":"s1"}'
verifie "StopFailure (erreur d'API) -> claude_done" \
  '{"hook_event_name":"StopFailure","session_id":"s1","cwd":"/home/a/Projects/mon_projet"}' \
  '/event application/json {"type":"claude_done","text":"mon_projet","session":"s1"}'
verifie "Notification autorisation -> claude_waiting" \
  '{"hook_event_name":"Notification","notification_type":"permission_prompt","session_id":"s1","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_waiting","text":"Hey?\nmon_projet","session":"s1"}'
verifie "Notification idle_prompt (Session qui a fini) -> rien envoyé" \
  '{"hook_event_name":"Notification","notification_type":"idle_prompt","session_id":"s1","cwd":"/x/mon_projet"}' \
  ''
verifie "Notification d'un autre type -> rien envoyé" \
  '{"hook_event_name":"Notification","notification_type":"auth_success","session_id":"s1","cwd":"/x/mon_projet"}' \
  ''

# Fin de tour sur une question : « attend » (last_assistant_message).
ATTEND='/event application/json {"type":"claude_waiting","text":"Hey?\nmon_projet","session":"s1"}'
FINI='/event application/json {"type":"claude_done","text":"mon_projet","session":"s1"}'
stop() { printf '{"hook_event_name":"%s","session_id":"s1","cwd":"/x/mon_projet","last_assistant_message":"%s"}' "$1" "$2"; }
verifie "Stop avec ❓ -> claude_waiting" "$(stop Stop 'Round 1\n❓ Quel nom ?\n➡️ je propose X')" "$ATTEND"
verifie "Stop avec ligne needs input: -> claude_waiting" "$(stop Stop 'Job bloqué\nneeds input: choisis une option\nfin.')" "$ATTEND"
verifie "Stop dont le dernier paragraphe finit par ? -> claude_waiting" "$(stop Stop 'Fait.\n\nTu veux que je pousse ?  \n\n')" "$ATTEND"
verifie "Stop avec ? seulement dans un bloc de code -> claude_done" "$(stop Stop 'Voici :\n\n```\ncurl http://x?a=1\n```')" "$FINI"
verifie "Stop avec ? dans un paragraphe non final -> claude_done" "$(stop Stop 'Tu veux ça ?\n\nJ ai fini.')" "$FINI"
verifie "Stop avec texte vide -> claude_done" "$(stop Stop '')" "$FINI"
verifie "StopFailure avec ? -> claude_done" "$(stop StopFailure 'Erreur ?')" "$FINI"
verifie "UserPromptSubmit -> claude_working" \
  '{"hook_event_name":"UserPromptSubmit","session_id":"s1","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_working","text":"mon_projet","session":"s1"}'
verifie "PostToolUse -> claude_working" \
  '{"hook_event_name":"PostToolUse","session_id":"s1","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_working","text":"mon_projet","session":"s1"}'
verifie "SessionEnd -> claude_end" \
  '{"hook_event_name":"SessionEnd","session_id":"s1","reason":"other","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_end","text":"mon_projet","session":"s1"}'
verifie "Sans session_id -> pas de champ session" \
  '{"hook_event_name":"Stop","cwd":"/x/mon_projet"}' \
  '/event application/json {"type":"claude_done","text":"mon_projet"}'

# Fin de tour : la requête est déjà reçue quand le hook rend la main (Claude Code peut tuer
# les processus des hooks à la fin du tour, un envoi détaché n'arriverait pas).
for evt in Stop StopFailure SessionEnd; do
  : > "$TMP/log"
  printf '%s' "{\"hook_event_name\":\"$evt\",\"session_id\":\"s1\",\"cwd\":\"/x/p\"}" | bash "$HOOK"
  if [ -s "$TMP/log" ]; then echo "ok   $evt envoyé avant de rendre la main"; else echo "ECHEC $evt : rien reçu au retour du hook"; echec=1; fi
done
: > "$TMP/log"
stop Stop 'Ok ?' | bash "$HOOK"
if [ -s "$TMP/log" ]; then echo "ok   Stop claude_waiting envoyé avant de rendre la main"; else echo "ECHEC Stop claude_waiting : rien reçu au retour du hook"; echec=1; fi

# Cible injoignable : le script rend la main vite, code 0, sans rien écrire.
# temps_max_ms <nom> <événement> <limite en ms>
temps_max_ms() {
  debut=$(date +%s%N)
  printf '%s' "{\"hook_event_name\":\"$2\",\"cwd\":\"/x/p\"}" | COMPAGNON_URL=http://10.255.255.1 bash "$HOOK"
  code=$?
  ms=$(( ($(date +%s%N) - debut) / 1000000 ))
  if [ "$code" = 0 ] && [ "$ms" -lt "$3" ]; then echo "ok   $1 (${ms} ms)"; else echo "ECHEC $1 : code $code, ${ms} ms"; echec=1; fi
}
temps_max_ms "cible injoignable, PostToolUse détaché" PostToolUse 500
temps_max_ms "cible injoignable, Stop borné à 1 s" Stop 1500

exit $echec
