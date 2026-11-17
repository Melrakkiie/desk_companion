#!/usr/bin/env bash
# Hook git global (événement reference-transaction) -> Compagnon : envoie git_push après un push réussi.
# Lit /proc : Linux uniquement. Détecte les ref de suivi qui bougent, donc seulement les clients
# qui utilisent le binaire `git`. Voir hooks/README.md pour l'installation.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Sort toujours en succès : un échec à l'étape `prepared` annulerait la mise à jour des refs.

# Durée du hook, laissée ici (couche hook) plutôt que dans etat.h : voir le bloc de durées d'etat.h.
delai_curl_s=2  # abandonne la requête après 2 s (le script, lui, rend la main tout de suite)

[ "$1" = prepared ] || exit 0
# Le processus parent doit être un `git push` (écarte fetch/pull, qui mettent aussi à jour ces refs).
mapfile -d '' args < "/proc/$PPID/cmdline" 2>/dev/null || exit 0
sous_commande=
for ((i = 1; i < ${#args[@]}; i++)); do   # args[0] = git ; on saute les options globales
  case ${args[i]} in
    -c | -C | --git-dir | --work-tree | --namespace) ((i++)) ;;  # la valeur est l'argument suivant
    -*) ;;
    *) sous_commande=${args[i]}; break ;;
  esac
done
[ "$sous_commande" = push ] || exit 0

nul=0000000000000000000000000000000000000000
branche=
while read -r _ nouveau ref; do
  case $ref in refs/remotes/*/*) ;; *) continue ;; esac
  [ "$nouveau" = "$nul" ] && continue                          # suppression de branche distante
  [ "$nouveau" = "$(git rev-parse -q --verify "$ref" 2>/dev/null)" ] && continue  # push déjà à jour
  branche=${ref#refs/remotes/*/}
  break
done
[ -n "$branche" ] || exit 0

# Un push de plusieurs branches ouvre une transaction par ref : un seul Événement par processus push
# (pid + instant de démarrage), marqué par un dossier créé de façon atomique.
dossier_marques=${XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}}  # dossier propre à l'utilisateur, pas /tmp partagé
marque=$dossier_marques/compagnon-push-$PPID-$(cut -d' ' -f22 "/proc/$PPID/stat" 2>/dev/null)
find "$dossier_marques" -maxdepth 1 -name 'compagnon-push-*' -mmin +5 -exec rmdir {} + 2>/dev/null
mkdir "$marque" 2>/dev/null || exit 0

projet=$(basename "$(git rev-parse --show-toplevel 2>/dev/null || pwd)")
corps=$(jq -nc --arg text "$branche"$'\n'"$projet" '{type: "git_push", text: $text}' 2>/dev/null) || exit 0

( curl -s -o /dev/null --max-time "$delai_curl_s" -X POST -H 'Content-Type: application/json' \
    -d "$corps" "${COMPAGNON_URL:-http://compagnon.local}/event" >/dev/null 2>&1 & ) >/dev/null 2>&1
exit 0
