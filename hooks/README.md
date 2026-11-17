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

Installation, depuis la racine du dépôt :

```
mkdir -p ~/.local/bin
install -m 755 hooks/compagnon-push.sh ~/.local/bin/
git config --global hook.compagnon-push.command "$HOME/.local/bin/compagnon-push.sh"
git config --global hook.compagnon-push.event reference-transaction
```

Le hook pointe vers une copie hors du dépôt : ne pas le faire pointer vers `hooks/compagnon-push.sh`. Si le
script disparaît (branche ou commit sans le script extrait, dépôt déplacé…), git échoue partout (`commit`,
`checkout`, `pull`…) avec « la mise à jour a été abandonnée par le crochet reference-transaction ». Dans ce
cas, retirer le hook (ci-dessous) ou corriger le chemin. Après une modification du script, relancer la
commande `install`.

Retrait :

```
git config --global --unset hook.compagnon-push.command
git config --global --unset hook.compagnon-push.event
rm ~/.local/bin/compagnon-push.sh
```

Limite : Linux uniquement (le script lit `/proc`), et seulement les clients qui utilisent le binaire `git`.

## Musique (lecteur multimédia)

`compagnon-musique.sh` suit `playerctl --follow metadata` et envoie l'état de lecture à chaque changement :
`music` (titre, artiste) si un morceau joue, `music_stop` sinon (pause, arrêt, lecteur fermé). Il renvoie
aussi cet état toutes les 10 s (période réglable : `COMPAGNON_MUSIQUE_RENVOI_S`, en secondes ; la garder
sous les 30 s au bout desquelles le Compagnon oublie le morceau) : c'est le Compagnon qui reconnaît un morceau
déjà connu, donc ces renvois ne changent rien à l'écran. Si le Compagnon est injoignable, le script continue et
l'état réel est rattrapé au renvoi suivant ; si `playerctl` s'arrête (aucun lecteur), il le relance toutes les 5 s.
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
