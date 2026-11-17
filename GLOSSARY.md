# Glossaire — Compagnon de bureau

**Compagnon** : le petit appareil posé sur le bureau (ESP32 + écran) qui réagit à ce qui se passe sur le PC.

**Événement** : un message envoyé au Compagnon pour lui signaler quelque chose. Il porte un _type_ (ce qui s'est passé) et un _texte_ facultatif (le détail à afficher).
_À éviter_ : notification, commande, message.

**Expression** : ce que le Compagnon montre à l'écran à un instant donné : un visage, éventuellement accompagné d'un texte ou d'une animation.
_À éviter_ : écran, mode, humeur.

**Bandeau** : la bande jaune en haut de l'écran, réservée au texte d'une Expression (nom du projet, titre du morceau…). Vide au Repos, sauf pour signaler une coupure du Wi-Fi.

**Zone visage** : la partie bleue de l'écran, sous le Bandeau, où se dessinent le visage et les animations.

**Son** : une courte mélodie jouée par le buzzer du Compagnon en réaction à un Événement. Deux Sons existent : la **fanfare** (victoire, quelques secondes) et le **carillon** (deux notes brèves).
_À éviter_ : bip, alarme, notification.

**Repos** : l'Expression par défaut, quand rien ne se passe et qu'il n'y a pas de Jeu courant : un visage neutre qui cligne des yeux de temps en temps.

**Jeu** : l'Expression par défaut tant qu'il y a un Jeu courant ; elle prend la place du Repos. Visage concentré qui cligne des yeux, une manette dans la Zone visage, le nom du jeu dans le Bandeau. Ni temporaire ni persistante : c'est le point de retour des autres Expressions.
_À éviter_ : gaming, mode jeu.

**Jeu courant** : le jeu lancé sur le PC par Steam ou Lutris, tant qu'il tourne. Le client Steam ou Lutris ouvert sans jeu n'en est pas un. Le PC rappelle l'état toutes les 10 s ; sans nouvelles pendant 30 s, il n'y a plus de Jeu courant.
_À éviter_ : partie, jeu en cours.

**Expression temporaire** : une Expression qui revient d'elle-même au Repos (ou à Jeu, s'il y a un Jeu courant) après une durée propre à son type.

**Expression persistante** : une Expression qui reste affichée jusqu'à ce qu'un autre Événement la remplace.

**Bouton** : le bouton physique du Compagnon. Un appui montre un Rappel. Un appui n'est pas un Événement : il ne vient pas du PC.
_À éviter_ : touche, interrupteur.

**Rappel** : ce que montre un appui sur le Bouton, par-dessus l'Expression courante sans la remplacer : l'Expression musique du Morceau courant, ou l'Expression silence s'il n'y en a pas. À la fin du Rappel, le Compagnon revient à l'Expression courante.
_À éviter_ : popup, superposition, aperçu.

**Morceau courant** : le dernier morceau annoncé par `music`, tant que la lecture n'a pas été mise en pause ou arrêtée (`music_stop`). Il n'y en a pas toujours. Un morceau mis en pause reste connu du Compagnon : s'il reprend, il redevient Morceau courant sans être réaffiché. Le PC rappelle l'état de lecture toutes les 10 s ; sans nouvelles pendant 30 s, il n'y a plus de Morceau courant.
_À éviter_ : musique en cours, chanson.

**Silence** : l'Expression qui dit qu'il n'y a pas de Morceau courant : Bandeau « pas de musique », note barrée, temporaire, sans Son.

## Types d'Événement

| Type | Ce qui s'est passé | Expression | Son |
|---|---|---|---|
| `claude_done` | Claude Code a fini de répondre | content, temporaire | — |
| `claude_waiting` | Claude Code attend une réponse ou une autorisation | interpelle, persistante | carillon |
| `claude_working` | On a répondu à Claude Code, il reprend son travail | met fin à `claude_waiting` (sans effet sinon) | — |
| `music` | Un morceau est en cours de lecture (le texte est le titre, accompagné d'un artiste) ; renvoyé toutes les 10 s pendant la lecture | musique, temporaire ; aucune si c'est le morceau déjà connu (reprise après pause) | — |
| `music_stop` | La lecture a été mise en pause ou arrêtée, ou le lecteur s'est fermé ; renvoyé toutes les 10 s tant que rien ne joue | aucune : il n'y a plus de Morceau courant | — |
| `git_push` | Un push git a été fait | animation de push, temporaire | fanfare |
| `game` | Un jeu tourne (le texte est son nom) ; renvoyé toutes les 10 s | Jeu devient le point de retour | — |
| `game_stop` | Aucun jeu ne tourne ; renvoyé toutes les 10 s | le Repos redevient le point de retour : il n'y a plus de Jeu courant | — |

## Relations

- Un Événement remplace toujours l'Expression courante : c'est le dernier reçu qui l'emporte, même s'il vient d'une autre session de Claude Code.
- Exception : `game` et `game_stop` ne changent que le point de retour. `game` n'affiche Jeu tout de suite que si l'Expression courante est le Repos, et `game_stop` ne ramène au Repos tout de suite que si elle est Jeu ; sinon le changement attend la fin de l'Expression courante.
- Un `game` portant un autre nom de jeu met à jour le Bandeau de Jeu sur place.
- Il n'y a jamais qu'un seul Jeu courant, même si plusieurs jeux tournent.
- Seul `claude_waiting` produit une Expression persistante.
- `claude_working` n'agit que si l'Expression courante est celle de `claude_waiting`.
- Pendant un Rappel, l'Expression courante continue sa vie : une Expression temporaire peut expirer en dessous, et le Rappel revient alors au Repos (ou à Jeu).
- Un Événement qui change l'Expression met fin au Rappel.
- Un Son est joué une fois, sans répétition. Seul un autre Son l'interrompt : un Événement sans Son change l'Expression mais laisse le Son finir.
