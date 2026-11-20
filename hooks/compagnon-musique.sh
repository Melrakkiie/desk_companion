#!/usr/bin/env bash
# Lecteur multimédia -> Compagnon. Suit `playerctl --follow metadata` (MPRIS) et envoie en POST /event
# l'état de lecture : `video` (titre + chaîne) si l'adresse (xesam:url) est une vidéo, `music` (titre + artiste)
# si un morceau joue, `music_stop` sinon (pause, arrêt, sans titre,
# lecteur fermé). Envoi à chaque changement, puis renvoi de l'état courant toutes les 10 s :
# le Compagnon rattrape ainsi l'état réel après un redémarrage, un envoi perdu ou une cible injoignable,
# et reconnaît lui-même un morceau déjà connu (le script ne garde aucune mémoire de ce qu'il a envoyé).
# Tourne en arrière-plan, sans jamais s'arrêter ni bloquer, même si le Compagnon est injoignable.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Période de renvoi (10 s par défaut) : COMPAGNON_MUSIQUE_RENVOI_S (secondes, décimales permises).
# Variables de test : COMPAGNON_MUSIQUE_SOURCE (commande qui remplace playerctl),
# COMPAGNON_MUSIQUE_RELANCE_S (attente avant de relancer la source).

delai_curl_s=2  # abandonne la requête après 2 s, comme compagnon-hook.sh
relance_s=${COMPAGNON_MUSIQUE_RELANCE_S:-5}
renvoi_s=${COMPAGNON_MUSIQUE_RENVOI_S:-10}  # période de renvoi ; garder < 30 s (oubli côté Compagnon)

# Séparateur de champs : \x1f n'est pas un blanc, donc `read` garde un artiste vide
# (une tabulation serait avalée) et les accents passent tels quels.
sep=$'\x1f'
source_cmd=${COMPAGNON_MUSIQUE_SOURCE:-"playerctl --follow metadata --format '{{status}}${sep}{{artist}}${sep}{{title}}${sep}{{xesam:url}}'"}

# genre_de ADRESSE : affiche `video` ou `music`. Règle en dur, à allonger quand un cas se présente :
# sites vidéo (youtube.com et sous-domaines sauf music.youtube.com) et fichiers vidéo locaux ; le reste,
# adresse vide comprise, est un morceau.
genre_de() {
  local u=${1,,} hote
  case $u in
    file://*) case $u in *.mkv|*.mp4|*.webm|*.avi|*.mov|*.m4v) echo video; return;; esac ;;
    *://*) hote=${u#*://}; hote=${hote%%[/?#]*}; hote=${hote##*@}; hote=${hote%%:*}
      case $hote in
        music.youtube.com) ;;
        youtube.com|*.youtube.com|youtu.be|twitch.tv|*.twitch.tv|netflix.com|*.netflix.com|\
        primevideo.com|*.primevideo.com|disneyplus.com|*.disneyplus.com|vimeo.com|*.vimeo.com|\
        dailymotion.com|*.dailymotion.com|crunchyroll.com|*.crunchyroll.com) echo video; return;;
      esac ;;
  esac
  echo music
}

corps_stop='{"type":"music_stop"}'
corps=$corps_stop  # état courant à annoncer (survit aux relances de la source)
renvoi_us=$(awk -v s="$renvoi_s" 'BEGIN{printf "%d", s*1000000}')
dernier_envoi_us=0

# Synchrone, borné par --max-time ; un échec est sans conséquence : le renvoi suivant rattrape.
envoyer() {
  dernier_envoi_us=${EPOCHREALTIME/[.,]/}  # microsecondes, quel que soit le séparateur décimal local
  curl -sf -o /dev/null --max-time "$delai_curl_s" -X POST -H 'Content-Type: application/json' \
    -d "$corps" "${COMPAGNON_URL:-http://compagnon.local}/event" >/dev/null 2>&1
  return 0
}

while true; do
  # `playerctl --follow` se termine quand le dernier lecteur se ferme (ou s'il n'y en a pas) :
  # on attend puis on relance, au lieu de mourir.
  while true; do
    IFS=$sep read -r -t "$renvoi_s" statut artiste titre adresse
    rc=$?
    if [ $rc -eq 0 ]; then
      # Lecture : music (reprise d'un morceau après pause : renvoyé) ; sinon music_stop.
      if [ "$statut" = Playing ] && [ -n "$titre" ]; then
        genre=$(genre_de "$adresse")
        corps_music=$(jq -nc --arg type "$genre" --arg text "$titre" --arg artist "$artiste" \
          '{type: $type, text: $text, artist: $artist}' 2>/dev/null) || continue
        corps=$corps_music
      else
        corps=$corps_stop
      fi
    elif [ $rc -le 128 ]; then
      # Fin de la source : plus de lecteur. music_stop tout de suite si l'état change, sinon
      # seulement à la cadence de renvoi (la source est relancée plus souvent, toutes les relance_s).
      if [ "$corps" != "$corps_stop" ] || [ $((${EPOCHREALTIME/[.,]/} - dernier_envoi_us)) -ge "$renvoi_us" ]; then
        corps=$corps_stop
        envoyer
      fi
      break
    fi
    # Nouvel état ou délai dépassé (rc > 128) : (ré)envoi de l'état courant.
    envoyer
  done < <(bash -c "$source_cmd" 2>/dev/null)
  sleep "$relance_s"
done
