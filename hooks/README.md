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
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_waiting","text":"autorisation ?\nprojet"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_waiting","text":"à toi !\nprojet"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_working","text":"projet"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"music","text":"Titre","artist":"Artiste"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"git_push","text":"main"}' http://compagnon.local/event
```

Le `text` s'affiche sur deux lignes du Bandeau au plus : coupé au premier `\n` (ligne 2 = le reste).
Chaque ligne est tronquée à 100 caractères et défile si elle dépasse l'écran.

## Push git (hook git global)

`compagnon-push.sh` envoie un Événement `git_push` (`branche` ligne 1, projet ligne 2) après tout push réussi
qui fait bouger une branche, quel que soit l'outil (terminal, IDE, Claude Code). Un seul Événement par push
(première branche). Rien pour : push déjà à jour, suppression de branche, tags seuls, push refusé, `fetch`/`pull`.
Dépendances : `git` récent (hooks par config, testé avec 2.56), `jq`, `curl`. Cible : `COMPAGNON_URL`.
Les hooks propres à chaque dépôt continuent de s'exécuter.

Installation (adapter le chemin) :

```
chmod +x hooks/compagnon-push.sh
git config --global hook.compagnon-push.command "$PWD/hooks/compagnon-push.sh"
git config --global hook.compagnon-push.event reference-transaction
```

**Attention** : le chemin doit rester valide. Si le script disparaît (dépôt déplacé, branche sans le script
extraite…), git échoue partout (`commit`, `init`, `push`…) avec « la mise à jour a été abandonnée par le
crochet reference-transaction ». Dans ce cas, retirer le hook (ci-dessous) ou corriger le chemin.

Retrait :

```
git config --global --unset hook.compagnon-push.command
git config --global --unset hook.compagnon-push.event
```

Limite : Linux uniquement (le script lit `/proc`), et seulement les clients qui utilisent le binaire `git`.

## Musique (lecteur multimédia)

`compagnon-musique.sh` suit `playerctl --follow metadata` et envoie un Événement `music` (titre, artiste)
par morceau : pas de renvoi pour le même morceau, ni après une pause. Si le Compagnon est injoignable, le
script continue et renvoie le morceau en cours toutes les 5 s jusqu'à ce que la cible réponde ; si `playerctl` s'arrête (aucun lecteur), il le relance toutes les 5 s.
Dépendances : `playerctl` (`sudo dnf install playerctl`), `jq`, `curl`. Cible : `COMPAGNON_URL`.

Lancement à l'ouverture de session (service utilisateur systemd ; adapter le chemin dans
`compagnon-musique.service` si le dépôt est ailleurs) :

```
chmod +x hooks/compagnon-musique.sh
mkdir -p ~/.config/systemd/user
cp hooks/compagnon-musique.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now compagnon-musique.service
```

Arrêt et retrait : `systemctl --user disable --now compagnon-musique.service`.
