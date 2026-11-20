#!/usr/bin/env bash
# Hook Claude Code -> Compagnon. Lit le JSON du hook sur l'entrée, envoie l'Événement
# correspondant en POST /event, sans jamais échouer ; au plus 1 s d'attente (fin de tour), sinon aucune.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Le session_id du hook part dans le champ "session" : le Compagnon suit chaque Session
# séparément (champ omis s'il est absent : Session anonyme).

# Durée du hook, laissée ici (couche hook) plutôt que dans etat.h : voir le bloc de durées d'etat.h.
delai_curl_s=2  # envoi détaché : abandonne la requête après 2 s (le script, lui, rend la main tout de suite)
delai_fin_s=1   # fin de tour ou de Session : le script attend la réponse, au plus 1 s

entree=$(cat)
champ() { printf '%s' "$entree" | jq -r "$1 // empty" 2>/dev/null; }

projet=$(basename "$(champ .cwd)")

# Le texte final de Claude m'adresse-t-il une question ? (❓, ligne « needs input: »,
# ou dernier paragraphe hors blocs de code terminé par « ? »). Limite : pas d'analyse du sens.
attend() {
  local msg; msg=$(champ .last_assistant_message)
  [[ $msg == *❓* ]] && return 0
  printf '%s\n' "$msg" | grep -q '^needs input:' && return 0
  printf '%s\n' "$msg" | awk '
    /^```/ { c = !c; if (p != "") d = p; p = ""; next }
    c { next }
    /[^[:space:]]/ { p = p $0; next }
    { if (p != "") d = p; p = "" }
    END { if (p != "") d = p; exit !(d ~ /\?[[:space:]]*$/) }'
}

attendre=0  # 1 : envoi qui attend la réponse (fin de tour)
case "$(champ .hook_event_name)" in
  Stop)
    attendre=1
    if attend; then type=claude_waiting; texte="Hey?"$'\n'"$projet"; else type=claude_done; texte=$projet; fi ;;
  StopFailure) type=claude_done; texte=$projet ;;  # tour fini sur une erreur d'API : aucune question
  Notification)
    type=claude_waiting
    case "$(champ .notification_type)" in
      # libellé sur la ligne 1 du Bandeau, projet sur la ligne 2 (saut de ligne dans le texte)
      # elicitation_dialog : un serveur MCP demande une saisie
      permission_prompt | elicitation_dialog) texte="Hey?"$'\n'"$projet" ;;
      # idle_prompt (saisie inactive ~60 s après la fin) ne dit pas que la Session attend quelque chose :
      # ignoré comme les autres notifications.
      *) exit 0 ;;  # autres notifications : rien à signaler
    esac ;;
  PreToolUse)  # question à choix ouverte : la Session attend ; PostToolUse efface l'attente
    [ "$(champ .tool_name)" = AskUserQuestion ] || exit 0
    type=claude_waiting; texte="Hey?"$'\n'"$projet" ;;
  UserPromptSubmit | PostToolUse) type=claude_working; texte=$projet ;;
  SessionEnd) type=claude_end; texte=$projet ;;  # la Session fermée n'est plus suivie
  *) exit 0 ;;
esac

session=$(champ .session_id)
corps=$(jq -nc --arg type "$type" --arg text "$texte" --arg session "$session" \
  '{type: $type, text: $text} + (if $session == "" then {} else {session: $session} end)' 2>/dev/null) || exit 0

envoyer() {
  curl -s -o /dev/null --max-time "$1" -X POST -H 'Content-Type: application/json' \
    -d "$corps" "${COMPAGNON_URL:-http://compagnon.local}/event" >/dev/null 2>&1
}

case "$type" in
  # Fin de tour ou de Session : envoyé détaché, le claude_done du Stop n'arrivait pas (processus
  # des hooks vraisemblablement tués à la fin du tour). On attend la réponse, au plus 1 s
  # (budget SessionEnd : 1,5 s).
  claude_done | claude_end) envoyer "$delai_fin_s" ;;
  # Le reste (dizaines de PostToolUse par minute) part détaché pour rendre la main tout de suite,
  # sauf le claude_waiting d'une fin de tour (Stop), attendu comme claude_done.
  *) if [ "$attendre" = 1 ]; then envoyer "$delai_fin_s"; else ( envoyer "$delai_curl_s" & ) >/dev/null 2>&1; fi ;;
esac
exit 0
