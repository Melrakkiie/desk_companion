# Hooks Claude Code

`compagnon-hook.sh` envoie l'Événement au Compagnon (`claude_done`, `claude_waiting`, `claude_working`).
Dépendances : `jq`, `curl`. Cible : `http://compagnon.local`, ou la variable `COMPAGNON_URL`.

## Installation

Ajouter les entrées de `settings-extrait.json` sous la clé `hooks` de `~/.claude/settings.json`, **à côté**
des hooks existants : si un événement (`Stop`, etc.) existe déjà, ajouter l'objet à son tableau
sans toucher aux autres. Vérifier le chemin du script, puis `chmod +x hooks/compagnon-hook.sh`.

## Tests

`make test`

## Commandes curl des 5 types

```
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_done","text":"projet"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_waiting","text":"autorisation ? projet"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_working","text":"projet"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"music","text":"Titre","artist":"Artiste"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"git_push","text":"main"}' http://compagnon.local/event
```
