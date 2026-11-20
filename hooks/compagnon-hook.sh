#!/usr/bin/env bash
# Hook Claude Code -> Compagnon. Lit le JSON du hook sur l'entrée, envoie l'Événement
# correspondant en POST /event, en arrière-plan, sans jamais échouer ni bloquer.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Le session_id du hook part dans le champ "session" : le Compagnon suit chaque Session
# séparément (champ omis s'il est absent : Session anonyme).

# Durée du hook, laissée ici (couche hook) plutôt que dans etat.h : voir le bloc de durées d'etat.h.
delai_curl_s=2  # abandonne la requête après 2 s (le script, lui, rend la main tout de suite)

entree=$(cat)
champ() { printf '%s' "$entree" | jq -r "$1 // empty" 2>/dev/null; }

projet=$(basename "$(champ .cwd)")
case "$(champ .hook_event_name)" in
  Stop) type=claude_done; texte=$projet ;;
  Notification)
    type=claude_waiting
    case "$(champ .notification_type)" in
      # libellé sur la ligne 1 du Bandeau, projet sur la ligne 2 (saut de ligne dans le texte)
      permission_prompt) texte="autorisation ?"$'\n'"$projet" ;;
      idle_prompt) texte="à toi !"$'\n'"$projet" ;;
      *) exit 0 ;;  # autres notifications : rien à signaler
    esac ;;
  UserPromptSubmit | PostToolUse) type=claude_working; texte=$projet ;;
  *) exit 0 ;;
esac

session=$(champ .session_id)
corps=$(jq -nc --arg type "$type" --arg text "$texte" --arg session "$session" \
  '{type: $type, text: $text} + (if $session == "" then {} else {session: $session} end)' 2>/dev/null) || exit 0

# Tout détaché (sous-shell + &) pour rendre la main tout de suite, même si la cible est injoignable.
( curl -s -o /dev/null --max-time "$delai_curl_s" -X POST -H 'Content-Type: application/json' \
    -d "$corps" "${COMPAGNON_URL:-http://compagnon.local}/event" >/dev/null 2>&1 & ) >/dev/null 2>&1
exit 0
