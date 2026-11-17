#!/usr/bin/env bash
# Jeu -> Compagnon. Lit les processus (/proc/*/cmdline) et envoie en POST /event `game` (nom du jeu)
# tant qu'un jeu lancé par Steam (natif ou Proton) ou Lutris tourne, `game_stop` sinon (client seul compris).
# Envoi à chaque changement, puis renvoi de l'état courant toutes les 10 s : le Compagnon rattrape ainsi
# l'état réel après un redémarrage, un envoi perdu ou une cible injoignable.
# Tourne en arrière-plan, sans jamais s'arrêter ni bloquer, même si le Compagnon est injoignable.
#
# jeu Steam : processus `reaper SteamLaunch AppId=<id>` (vérifié, natif et Proton) ; nom lu dans le champ
# `name` de appmanifest_<id>.acf des bibliothèques de libraryfolders.vdf, sinon `Steam <id>`.
# jeu Lutris : argument `lutris-wrapper`, suivi du nom du jeu. Steam prime si les deux tournent.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Période de renvoi (10 s par défaut) : COMPAGNON_JEU_RENVOI_S (secondes, décimales permises) ;
# période de lecture des processus (2 s) : COMPAGNON_JEU_SCAN_S.
# Variables de test : COMPAGNON_JEU_SOURCE (commande qui remplace la lecture de /proc),
# COMPAGNON_JEU_STEAM_DIR (racine de Steam, ~/.local/share/Steam par défaut).
#
# Format de la source : un processus par ligne, arguments séparés par \x1f (les frontières d'arguments
# comptent : un nom de jeu peut contenir des espaces, ce que `ps` ne permet pas de distinguer).

delai_curl_s=2  # abandonne la requête après 2 s, comme compagnon-hook.sh
scan_s=${COMPAGNON_JEU_SCAN_S:-2}
renvoi_s=${COMPAGNON_JEU_RENVOI_S:-10}  # garder < 30 s (oubli côté Compagnon)
steam_dir=${COMPAGNON_JEU_STEAM_DIR:-$HOME/.local/share/Steam}
sep=$'\x1f'
# Par défaut : tous les /proc/<pid>/cmdline (arguments séparés par NUL) ; les processus disparus en cours de lecture sont ignorés.
source_defaut=$'gawk \'BEGIN{RS="\\0"} {printf "%s%s", (FNR>1 ? "\\037" : ""), $0} ENDFILE{print ""}\' /proc/[0-9]*/cmdline'
source_cmd=${COMPAGNON_JEU_SOURCE:-$source_defaut}

# Outils Steam qui ne sont pas des jeux : Linux Runtime scout/sniper, Proton Experimental, Redistributables.
outils_steam=" 1070560 1493710 1628350 228980 "

# Source par défaut sans gawk : aucun jeu ne serait jamais détecté ; on le dit dans le journal (stderr), on continue.
[ -z "$COMPAGNON_JEU_SOURCE" ] && ! command -v gawk >/dev/null && echo "compagnon-jeu : gawk introuvable, aucun jeu ne sera détecté (installer gawk)" >&2

renvoi_us=$(awk -v s="$renvoi_s" 'BEGIN{printf "%d", s*1000000}')
corps=""
dernier_envoi_us=0

# Nom du manifest de l'AppId $1, sinon "Steam <id>".
nom_steam() {
  local biblio nom
  while IFS= read -r biblio; do
    [ -f "$biblio/steamapps/appmanifest_$1.acf" ] || continue
    nom=$(sed -n 's/^[[:space:]]*"name"[[:space:]]*"\(.*\)"[[:space:]]*$/\1/p' "$biblio/steamapps/appmanifest_$1.acf" | head -n 1)
    [ -n "$nom" ] && { sed 's/\\\(["\\]\)/\1/g' <<< "$nom"; return; }
  done < <(printf '%s\n' "$steam_dir"; sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"[[:space:]]*$/\1/p' "$steam_dir/steamapps/libraryfolders.vdf" 2>/dev/null)
  echo "Steam $1"
}

# Un matcher par type de jeu : reçoit les arguments d'un processus, affiche le nom du jeu et réussit
# s'il en est un. Pour un autre lanceur, ajouter une fonction et la lister dans `matchers`.
jeu_steam() {
  [ "${1##*/}" = reaper ] && [ "$2" = SteamLaunch ] && [[ $3 =~ ^AppId=([0-9]+)$ ]] || return 1
  [[ $outils_steam == *" ${BASH_REMATCH[1]} "* ]] && return 1
  nom_steam "${BASH_REMATCH[1]}"
}

# jeu Lutris (vérifié, tous runners) : argv[0] est python3 ; l'argument de basename `lutris-wrapper` est
# suivi du nom du jeu (un seul argument). Le client Lutris seul n'a pas ce wrapper.
jeu_lutris() {
  local i
  for ((i = 1; i < $#; i++)); do
    [ "${!i##*/}" = lutris-wrapper ] && { printf '%s\n' "${@:i+1:1}"; return 0; }
  done
  return 1
}
matchers=(jeu_steam jeu_lutris)  # par ordre de priorité : Steam l'emporte sur Lutris

# Nom du premier jeu trouvé (matchers dans l'ordre, puis processus) ; échoue s'il n'y en a pas.
trouver_jeu() {
  local ligne args m nom lignes=()
  mapfile -t lignes < <(bash -c "$source_cmd" 2>/dev/null)
  for m in "${matchers[@]}"; do
    for ligne in "${lignes[@]}"; do
      IFS=$sep read -r -a args <<< "$ligne"
      nom=$("$m" "${args[@]}") && { printf '%s\n' "$nom"; return 0; }
    done
  done
  return 1
}

# Synchrone, borné par --max-time ; un échec est sans conséquence : le renvoi suivant rattrape.
envoyer() {
  dernier_envoi_us=${EPOCHREALTIME/[.,]/}  # microsecondes, quel que soit le séparateur décimal local
  curl -sf -o /dev/null --max-time "$delai_curl_s" -X POST -H 'Content-Type: application/json' \
    -d "$corps" "${COMPAGNON_URL:-http://compagnon.local}/event" >/dev/null 2>&1
  return 0
}

while true; do
  if nom=$(trouver_jeu); then
    nouveau=$(jq -nc --arg text "$nom" '{type: "game", text: $text}' 2>/dev/null) || nouveau=$corps
  else
    nouveau='{"type":"game_stop"}'
  fi
  if [ "$nouveau" != "$corps" ] || [ $((${EPOCHREALTIME/[.,]/} - dernier_envoi_us)) -ge "$renvoi_us" ]; then
    corps=$nouveau
    envoyer
  fi
  sleep "$scan_s"
done
