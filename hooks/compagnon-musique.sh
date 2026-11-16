#!/usr/bin/env bash
# Lecteur multimédia -> Compagnon. Suit `playerctl --follow metadata` (MPRIS) et envoie un
# Événement `music` (titre + artiste) en POST /event à chaque changement de morceau,
# en arrière-plan, sans jamais s'arrêter ni bloquer, même si le Compagnon est injoignable.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Variables de test : COMPAGNON_MUSIQUE_SOURCE (commande qui remplace playerctl),
# COMPAGNON_MUSIQUE_RELANCE_S (attente avant de relancer la source).

delai_curl_s=2  # abandonne la requête après 2 s, comme compagnon-hook.sh
relance_s=${COMPAGNON_MUSIQUE_RELANCE_S:-5}

# Séparateur de champs : \x1f n'est pas un blanc, donc `read` garde un artiste vide
# (une tabulation serait avalée) et les accents passent tels quels.
sep=$'\x1f'
source_cmd=${COMPAGNON_MUSIQUE_SOURCE:-"playerctl --follow metadata --format '{{status}}${sep}{{artist}}${sep}{{title}}'"}

dernier=""  # « artiste<sep>titre » du dernier morceau envoyé (hors boucle de lecture : survit aux relances)

while true; do
  # `playerctl --follow` se termine quand le dernier lecteur se ferme (ou s'il n'y en a pas) :
  # on attend puis on relance, au lieu de mourir.
  while IFS=$sep read -r statut artiste titre; do
    # Pause, arrêt ou métadonnées vides : rien à envoyer. La reprise du même morceau ne renvoie rien.
    [ "$statut" = Playing ] && [ -n "$titre" ] || continue
    [ "$artiste$sep$titre" = "$dernier" ] && continue
    dernier="$artiste$sep$titre"
    corps=$(jq -nc --arg text "$titre" --arg artist "$artiste" \
      '{type: "music", text: $text, artist: $artist}' 2>/dev/null) || continue
    # Tout détaché (sous-shell + &) : la boucle n'attend jamais la cible.
    ( curl -s -o /dev/null --max-time "$delai_curl_s" -X POST -H 'Content-Type: application/json' \
        -d "$corps" "${COMPAGNON_URL:-http://compagnon.local}/event" >/dev/null 2>&1 & ) >/dev/null 2>&1
  done < <(bash -c "$source_cmd" 2>/dev/null)
  sleep "$relance_s"
done
