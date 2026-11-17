#!/usr/bin/env bash
# Hook git global (événement reference-transaction) -> Compagnon : envoie git_push après un push réussi.
# Lit /proc : Linux uniquement. Détecte les ref de suivi qui bougent, donc seulement les clients
# qui utilisent le binaire `git`. Voir hooks/README.md pour l'installation.
#
# Cible : http://compagnon.local, remplaçable par la variable COMPAGNON_URL (tests).
# Sort toujours en succès : un échec à l'étape `prepared` annulerait la mise à jour des refs.

delai_curl_s=2  # abandonne la requête après 2 s (le script, lui, rend la main tout de suite)

[ "$1" = prepared ] || exit 0
# Le processus parent doit être un `git push` (écarte fetch/pull, qui mettent aussi à jour ces refs).
# shortcut: tout argument égal à "push" suffit, e.g. `git commit -m push` passerait, mais ne touche pas les refs distantes.
tr '\0' '\n' < "/proc/$PPID/cmdline" 2>/dev/null | grep -qx push || exit 0

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
marque=${TMPDIR:-/tmp}/compagnon-push-$PPID-$(cut -d' ' -f22 "/proc/$PPID/stat" 2>/dev/null)
find "${TMPDIR:-/tmp}" -maxdepth 1 -name 'compagnon-push-*' -mmin +5 -exec rmdir {} + 2>/dev/null
mkdir "$marque" 2>/dev/null || exit 0

projet=$(basename "$(git rev-parse --show-toplevel 2>/dev/null || pwd)")
corps=$(jq -nc --arg text "$branche"$'\n'"$projet" '{type: "git_push", text: $text}' 2>/dev/null) || exit 0

( curl -s -o /dev/null --max-time "$delai_curl_s" -X POST -H 'Content-Type: application/json' \
    -d "$corps" "${COMPAGNON_URL:-http://compagnon.local}/event" >/dev/null 2>&1 & ) >/dev/null 2>&1
exit 0
