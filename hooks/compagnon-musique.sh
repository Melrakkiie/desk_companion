#!/usr/bin/env bash
# Lecteur multimédia -> Compagnon. Suit `playerctl --follow metadata` (MPRIS) et envoie un
# Événement `music` (titre + artiste) en POST /event à chaque changement de morceau ou reprise,
# et `music_stop` à la pause, à l'arrêt, sans titre ou quand le lecteur se ferme, en arrière-plan, sans jamais s'arrêter ni bloquer, même si le Compagnon est injoignable.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Variables de test : COMPAGNON_MUSIQUE_SOURCE (commande qui remplace playerctl),
# COMPAGNON_MUSIQUE_RELANCE_S (attente avant de relancer la source),
# COMPAGNON_MUSIQUE_REESSAI_S (attente avant de renvoyer un morceau que la cible n'a pas reçu).

delai_curl_s=2  # abandonne la requête après 2 s, comme compagnon-hook.sh
relance_s=${COMPAGNON_MUSIQUE_RELANCE_S:-5}
reessai_s=${COMPAGNON_MUSIQUE_REESSAI_S:-5}  # délai entre deux essais tant que le Compagnon est injoignable

# Séparateur de champs : \x1f n'est pas un blanc, donc `read` garde un artiste vide
# (une tabulation serait avalée) et les accents passent tels quels.
sep=$'\x1f'
source_cmd=${COMPAGNON_MUSIQUE_SOURCE:-"playerctl --follow metadata --format '{{status}}${sep}{{artist}}${sep}{{title}}'"}

dernier=""      # état que la cible a reçu : « artiste<sep>titre » du morceau, ou « stop » (survit aux relances)
en_attente=""   # état pas encore reçu par la cible (corps dans $corps)

# prepare ETAT CORPS : ce qu'il faut annoncer ; rien si la cible l'a déjà.
prepare() {
  en_attente=""
  [ "$1" = "$dernier" ] && return
  corps=$2
  en_attente=$1
}

# Synchrone, borné par --max-time ; l'état n'est « envoyé » qu'une fois curl réussi (-f).
envoyer() {
  [ -n "$en_attente" ] || return 0
  curl -sf -o /dev/null --max-time "$delai_curl_s" -X POST -H 'Content-Type: application/json' \
    -d "$corps" "${COMPAGNON_URL:-http://compagnon.local}/event" >/dev/null 2>&1 \
    && { dernier=$en_attente; en_attente=""; }
  return 0
}

while true; do
  # `playerctl --follow` se termine quand le dernier lecteur se ferme (ou s'il n'y en a pas) :
  # on attend puis on relance, au lieu de mourir.
  while true; do
    IFS=$sep read -r -t "$reessai_s" statut artiste titre
    rc=$?
    if [ $rc -eq 0 ]; then
      # Lecture : music (reprise d'un morceau après pause : renvoyé) ; sinon music_stop.
      if [ "$statut" = Playing ] && [ -n "$titre" ]; then
        c=$(jq -nc --arg text "$titre" --arg artist "$artiste" \
          '{type: "music", text: $text, artist: $artist}' 2>/dev/null) || continue
        prepare "$artiste$sep$titre" "$c"
      else
        prepare stop '{"type":"music_stop"}'
      fi
    elif [ $rc -le 128 ]; then
      prepare stop '{"type":"music_stop"}'  # fin de la source : plus de lecteur
      envoyer
      break
    fi
    # Délai dépassé (rc > 128) ou nouvel état : (ré)essai de l'état en attente.
    envoyer
  done < <(bash -c "$source_cmd" 2>/dev/null)
  sleep "$relance_s"
done
