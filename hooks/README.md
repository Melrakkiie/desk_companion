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
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_waiting","text":"Hey?\nprojet"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_working","text":"projet"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"music","text":"Titre","artist":"Artiste"}' http://compagnon.local/event
curl -X POST -H 'Content-Type: application/json' -d '{"type":"git_push","text":"main"}' http://compagnon.local/event
```

`Stop` et `StopFailure` (tour fini sur une erreur d'API) envoient `claude_done`. Pour ces Événements de fin
et `claude_end`, le script attend la réponse (au plus 1 s) : envoyé détaché, le `claude_done` du `Stop` n'arrivait pas
(processus des hooks vraisemblablement tués à la fin du tour). Les autres partent détachés, sans attente.
Seule la notification
`permission_prompt` envoie `claude_waiting` (`idle_prompt`, « saisie inactive depuis ~60 s », est ignorée :
elle arrive aussi quand la Session n'attend rien). Limite connue : une interruption (Échap) ne déclenche aucun
hook, la Session reste « travaille » jusqu'à son prochain Événement.

Les Événements Claude portent le `session_id` du hook dans `session` (chaîne, facultatif : absent = Session anonyme) ; le Compagnon suit l'attente de chaque Session séparément.

Le hook `SessionEnd` envoie `claude_end` (avec `session`) : le Compagnon oublie la Session fermée ; si elle
attendait, l'interpellation passe à la Session suivante qui attend, ou disparaît. `claude_end` d'une Session
inconnue est sans effet. Pour une Session tuée sans `SessionEnd` (ou un `claude_end` perdu), le Compagnon oublie
une Session sans aucun Événement depuis 12 h, comme sur `claude_end` ; il ne suit de toute façon que 8 Sessions
et oublie la plus ancienne à la neuvième.

```
curl -X POST -H 'Content-Type: application/json' -d '{"type":"claude_end","text":"projet","session":"<session_id>"}' http://compagnon.local/event
```

Vérification de `SessionEnd` : documenté dans la référence des hooks de Claude Code (raisons `clear`, `resume`,
`logout`, `prompt_input_exit`, `other` ; budget partagé de 1,5 s, que le script respecte en
attendant la réponse au plus 1 s) et présent dans la version installée lors de l'écriture (2.1.329). Non vérifié en réel : le
déclenchement effectif sur une vraie Session fermée, ni le cas d'un processus tué (SIGKILL), où le hook ne
peut pas s'exécuter, d'où la limite de 8 Sessions. À contrôler après installation : fermer une Session qui attend
et voir l'interpellation disparaître.

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

## Jeu (Steam et Lutris)

`compagnon-jeu.sh` lit les processus (`/proc/*/cmdline`) et envoie `game` (nom du jeu) tant qu'un jeu lancé
par Steam, natif ou Proton, tourne, `game_stop` sinon (client Steam seul compris). Le jeu est le processus
`reaper SteamLaunch AppId=<id>` ; son nom vient du champ `name` de `appmanifest_<id>.acf` dans les
bibliothèques de `libraryfolders.vdf` (`Steam <id>` si le manifest est introuvable, par exemple disque
démonté). Les outils Steam (Proton, Steam Linux Runtime, Redistributables) ne comptent pas comme jeux.
Les jeux Lutris (tous runners) sont reconnus aussi : le processus a pour argument `lutris-wrapper`, suivi du
nom du jeu ; le client Lutris seul ne compte pas. Si un jeu Steam et un jeu Lutris tournent ensemble,
seul le jeu Steam est envoyé.
Envoi à chaque changement (lecture des processus toutes les 2 s : `COMPAGNON_JEU_SCAN_S`), puis renvoi de
l'état toutes les 10 s (`COMPAGNON_JEU_RENVOI_S`, à garder sous les 30 s au bout desquelles le Compagnon
oublie le jeu). Si le Compagnon est injoignable, le script continue.
Dépendances : `gawk`, `jq`, `curl`. Cible : `COMPAGNON_URL`. Racine de Steam : `COMPAGNON_JEU_STEAM_DIR`
(`~/.local/share/Steam` par défaut).

Lancement à l'ouverture de session (adapter le chemin dans `compagnon-jeu.service` si le dépôt est ailleurs) :

```
chmod +x hooks/compagnon-jeu.sh
mkdir -p ~/.config/systemd/user
cp hooks/compagnon-jeu.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now compagnon-jeu.service
```

Arrêt et retrait : `systemctl --user disable --now compagnon-jeu.service`.
