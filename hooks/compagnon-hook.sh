#!/usr/bin/env bash
# Hook Claude Code -> Compagnon. Lit le JSON du hook sur l'entrée, envoie l'Événement
# correspondant en POST /event, sans jamais échouer ; attente de la réponse : au plus 3 s (Stop, StopFailure), 1 s
# (les autres, dont PostToolUse AskUserQuestion) ; seul le PostToolUse ordinaire est détaché, sans attente.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Le session_id du hook part dans le champ "session" : le Compagnon suit chaque Session
# séparément (champ omis s'il est absent : Session anonyme).

# Durée du hook, laissée ici (couche hook) plutôt que dans etat.h : voir le bloc de durées d'etat.h.
delai_curl_s=2  # envoi détaché : abandonne la requête après 2 s (le script, lui, rend la main tout de suite)
delai_fin_s=1   # envoi qui attend la réponse, au plus 1 s (aussi SessionEnd : budget 1,5 s)
delai_tour_s=3  # fin de tour (Stop, StopFailure) : le Compagnon répond lentement (0,4 s), 1 s ne suffisait pas

entree=$(cat)
champ() { printf '%s' "$entree" | jq -r "$1 // empty" 2>/dev/null; }

projet=$(basename "$(champ .cwd)")
hey="Hey?"$'\n'"$projet"  # texte de claude_waiting : libellé ligne 1, projet ligne 2

# Le texte final de Claude m'adresse-t-il une question ? (❓, ligne « needs input: »,
# ou dernier paragraphe hors blocs de code terminé par « ? »). Limite : pas d'analyse du sens.
finit_sur_question() {
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

# Durée d'attente de la réponse en s ; 0 : envoi détaché. Par défaut le script attend : un envoi détaché
# se perd (Claude Code tue vraisemblablement les processus du hook à sa sortie, le Compagnon est lent).
duree_attente_s=$delai_fin_s
# Du travail tourne-t-il encore en arrière-plan (sous-agent, commande de fond, Monitor) ? Tableau background_tasks
# non vide ; absent, null ou illisible : non. session_crons (tâches planifiées) ne compte pas.
fond_en_cours() { [ "$(champ '.background_tasks | if type == "array" then length else 0 end')" -gt 0 ] 2>/dev/null; }
case "$(champ .hook_event_name)" in
  Stop)
    duree_attente_s=$delai_tour_s
    if finit_sur_question; then type=claude_waiting; texte=$hey  # une question attend toujours, tâches de fond ou non
    elif fond_en_cours; then exit 0  # la Session travaille encore : pas de clochette avant la fin de tout
    else type=claude_done; texte=$projet; fi ;;
  StopFailure)  # tour fini sur une erreur d'API : aucune question
    fond_en_cours && exit 0
    duree_attente_s=$delai_tour_s; type=claude_done; texte=$projet ;;
  Notification)
    type=claude_waiting
    case "$(champ .notification_type)" in
      # libellé sur la ligne 1 du Bandeau, projet sur la ligne 2 (saut de ligne dans le texte)
      # elicitation_dialog : un serveur MCP demande une saisie
      permission_prompt | elicitation_dialog) texte=$hey ;;
      # idle_prompt (saisie inactive ~60 s après la fin) ne dit pas que la Session attend quelque chose :
      # ignoré comme les autres notifications.
      *) exit 0 ;;  # autres notifications : rien à signaler
    esac ;;
  PreToolUse)  # question à choix ouverte : la Session attend ; PostToolUse efface l'attente
    [ "$(champ .tool_name)" = AskUserQuestion ] || exit 0
    type=claude_waiting; texte=$hey ;;
  UserPromptSubmit) type=claude_working; texte=$projet ;;
  PostToolUse)  # dizaines par minute : détaché, sauf après une question à choix (efface le Hey?)
    type=claude_working; texte=$projet
    [ "$(champ .tool_name)" = AskUserQuestion ] || duree_attente_s=0 ;;
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

if [ "$duree_attente_s" != 0 ]; then envoyer "$duree_attente_s"; else ( envoyer "$delai_curl_s" & ) >/dev/null 2>&1; fi
exit 0
