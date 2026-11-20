# Glossaire — Compagnon de bureau

**Compagnon** : le petit appareil posé sur le bureau (ESP32 + écran) qui réagit à ce qui se passe sur le PC.

**PC** : un ordinateur qui envoie des Événements au Compagnon. Il peut y en avoir plusieurs ; le Compagnon ne sait pas lequel envoie quoi.
_À éviter_ : poste, machine.

**Sommeil** : l'état du Compagnon quand aucun PC ne donne plus de nouvelles (tous éteints). Il s'endort après 2 min sans aucun Événement, quel qu'en soit le type, et seulement tant que son Wi-Fi est connecté : un Compagnon sans Wi-Fi n'entend plus rien mais ne dort pas, et les 2 min repartent de zéro au retour du Wi-Fi. Ce sont les rappels de lecture et de jeu, envoyés toutes les 10 s, qui prouvent qu'un PC est allumé ; un PC qui n'envoie que des Événements Claude peut endormir le Compagnon à tort pendant une longue tâche. Ni l'endormissement ni le réveil ne jouent de Son. En s'endormant, il oublie toutes les Sessions. N'importe quel Événement le réveille ; un appui sur un Bouton, non.
_À éviter_ : veille, hors ligne, PC éteint.

**Événement** : un message envoyé au Compagnon pour lui signaler quelque chose. Il porte un _type_ (ce qui s'est passé) et un _texte_ facultatif (le détail à afficher). Les Événements Claude portent aussi leur Session.
_À éviter_ : notification, commande, message.

**Expression** : ce que le Compagnon montre à l'écran à un instant donné : un visage, éventuellement accompagné d'un texte ou d'une animation.
_À éviter_ : écran, mode, humeur.

**Bandeau** : la bande jaune en haut de l'écran, réservée au texte d'une Expression (nom du projet, titre du morceau…). Vide au Repos, sauf pour signaler une coupure du Wi-Fi.

**Zone visage** : la partie bleue de l'écran, sous le Bandeau, où se dessinent le visage et les animations.

**Son** : une courte mélodie jouée par le buzzer du Compagnon en réaction à un Événement. Trois Sons existent : la **fanfare** (victoire, quelques secondes), le **carillon** (deux notes descendantes, « on t'attend ») et la **clochette** (trois notes montantes brèves, « c'est fini »).
_À éviter_ : bip, alarme, notification.

**Repos** : l'Expression par défaut, quand rien ne se passe, qu'aucune Session n'attend et qu'il n'y a pas de Jeu courant : la Boule qui sourit et cligne des yeux de temps en temps.

**Endormi** : le point de retour pendant le Sommeil ; il prend la place du Repos. Boule aux yeux fermés (deux traits), bouche en petit « o », des « z » qui montent lentement dans la Zone visage ; Bandeau vide. Le Rappel musique y montre Endormi avec « Gros dodo temps » dans le Bandeau, à la place de Silence.

**Boule** : la petite mascotte toute ronde du Compagnon (corps rond, antenne à pompon, yeux et bouche en noir sur le corps). Toutes les Expressions qui ne concernent pas Claude (Repos, Endormi, Jeu, musique, Silence, push) en sont une variation ; les Expressions Claude montrent la Mascotte.
_À éviter_ : visage, mascotte (réservé à celle de Claude).

**Jeu** : le point de retour tant qu'il y a un Jeu courant et qu'aucune Session n'attend ; elle prend la place du Repos. Boule concentrée qui cligne des yeux, une manette dans la Zone visage, le nom du jeu dans le Bandeau.
_À éviter_ : gaming, mode jeu.

**Jeu courant** : le jeu lancé sur le PC par Steam ou Lutris, tant qu'il tourne. Le client Steam ou Lutris ouvert sans jeu n'en est pas un. Le PC rappelle l'état toutes les 10 s ; sans nouvelles pendant 30 s, il n'y a plus de Jeu courant.
_À éviter_ : partie, jeu en cours.

**Point de retour** : l'Expression vers laquelle le Compagnon revient quand rien d'autre n'est à montrer, par ordre de priorité : interpelle si au moins une Session attend (celle qui attend depuis le plus récemment), sinon Jeu s'il y a un Jeu courant, sinon Endormi pendant le Sommeil, sinon le Repos. Ce n'est ni une Expression temporaire ni une Expression persistante.

**Expression temporaire** : une Expression qui revient d'elle-même au point de retour après une durée propre à son type.

**Bouton** : un des deux boutons physiques du Compagnon, le **Bouton musique** ou le **Bouton Claude**. Un appui montre un Rappel, sauf l'appui sur le Bouton Claude pendant interpelle : il chasse la Session affichée (elle passe à « a fini », avec son projet seul, et le Compagnon revient au point de retour, sans Rappel ni Son). Un appui n'est pas un Événement : il ne vient pas du PC.
_À éviter_ : touche, interrupteur.

**Rappel** : ce que montre un appui sur un Bouton, par-dessus l'Expression courante sans la remplacer, pendant une durée propre à son type et sans Son. Le **Rappel musique** (Bouton musique) montre l'Expression musique du Morceau courant, ou Silence s'il n'y en a pas (Endormi pendant le Sommeil). Le **Rappel Claude** (Bouton Claude) montre l'État Claude le plus urgent. À la fin du Rappel, le Compagnon revient à l'Expression courante. Un nouvel appui, sur l'un ou l'autre Bouton, remplace le Rappel en cours.
_À éviter_ : popup, superposition, aperçu.

**Morceau courant** : le dernier morceau annoncé par `music`, tant que la lecture n'a pas été mise en pause ou arrêtée (`music_stop`). Il n'y en a pas toujours. Un morceau mis en pause reste connu du Compagnon : s'il reprend, il redevient Morceau courant sans être réaffiché. Le PC rappelle l'état de lecture toutes les 10 s ; sans nouvelles pendant 30 s, il n'y a plus de Morceau courant.
_À éviter_ : musique en cours, chanson.

**Silence** : l'Expression qui dit qu'il n'y a pas de Morceau courant : Bandeau « pas de musique », note barrée, temporaire, sans Son.

**Session** : une conversation Claude Code ouverte sur le PC, du premier Événement Claude reçu jusqu'à `claude_end`, ou jusqu'à 12 h sans aucun Événement de sa part (Session fermée sans `claude_end`). Plusieurs Sessions peuvent tourner en parallèle ; le Compagnon en connaît au plus 8 et oublie la plus ancienne quand il en apprend une neuvième.
_À éviter_ : conversation, instance, onglet.

**État Claude** : ce qu'une Session est en train de faire, d'après son dernier Événement Claude : **attend** (`claude_waiting`), **travaille** (`claude_working`) ou **a fini** (`claude_done`). Une Session attend quand elle est bloquée sur toi : elle demande une autorisation, te pose une question à choix, ou termine sa réponse sur une question qui t'est adressée ; une réponse qui se termine sans rien te demander, c'est « a fini ». Il n'expire pas avec le temps (seule la Session entière est oubliée après 12 h sans Événement).

**Mascotte** : le petit personnage pixel-art de Claude Code (corps carré, deux yeux, deux bras, quatre pattes). Toutes les Expressions Claude (content, interpelle, travaille, Sans Claude) en sont une variation, à la place du visage.
_À éviter_ : avatar, logo, sprite.

**Travaille** : l'Expression d'une Session qui travaille : Mascotte aux yeux plissés qui trottine, points qui tournent à côté, projet dans le Bandeau. Elle n'apparaît que dans le Rappel Claude.

**Sans Claude** : l'Expression du Rappel Claude quand aucune Session n'est connue : Bandeau « pas de Claude », contour vide de la Mascotte, temporaire, sans Son.

## Types d'Événement

| Type | Ce qui s'est passé | Expression | Son |
|---|---|---|---|
| `claude_done` | Une Session a fini de répondre | content (« Done! », projet en ligne 2), temporaire | clochette |
| `claude_waiting` | Une Session attend une réponse ou une autorisation | interpelle, qui reste le point de retour tant que la Session attend | carillon |
| `claude_working` | On a répondu à une Session, elle reprend son travail | aucune ; si cette Session attendait, interpelle passe à la Session suivante qui attend, sinon s'en va | — |
| `claude_end` | Une Session s'est fermée | aucune : la Session est oubliée (comme `claude_working` si elle attendait) | — |
| `music` | Un morceau est en cours de lecture (le texte est le titre, accompagné d'un artiste) ; renvoyé toutes les 10 s pendant la lecture | musique, temporaire ; aucune si c'est le morceau déjà connu (reprise après pause) | — |
| `music_stop` | La lecture a été mise en pause ou arrêtée, ou le lecteur s'est fermé ; renvoyé toutes les 10 s tant que rien ne joue | aucune : il n'y a plus de Morceau courant | — |
| `git_push` | Un push git a été fait | animation de push, temporaire | fanfare |
| `game` | Un jeu tourne (le texte est son nom) ; renvoyé toutes les 10 s | change le point de retour | — |
| `game_stop` | Aucun jeu ne tourne ; renvoyé toutes les 10 s | change le point de retour : il n'y a plus de Jeu courant | — |

## Relations

- Un Événement qui produit une Expression temporaire remplace l'Expression courante : c'est le dernier reçu qui l'emporte. À sa fin, le Compagnon revient au point de retour.
- Seuls les Événements de la même Session changent son État Claude : le `claude_done` ou le `claude_working` d'une Session ne met jamais fin à l'attente d'une autre.
- Quand interpelle passe d'une Session à une autre qui attendait déjà, le carillon n'est pas rejoué.
- `game`, `game_stop`, `claude_working` et `claude_end` ne changent que le point de retour : le changement ne se voit tout de suite que si l'Expression courante est le point de retour ; sinon il attend la fin de l'Expression courante.
- Un `game` portant un autre nom de jeu met à jour le Bandeau de Jeu sur place.
- Il n'y a jamais qu'un seul Jeu courant, même si plusieurs jeux tournent.
- Le Rappel Claude montre, par ordre de priorité, la Session qui attend la plus récente (interpelle), sinon celle qui travaille la plus récente (travaille), sinon celle qui a fini la plus récente (content), sinon Sans Claude. La ligne 1 du Bandeau dit l'état (« Working », « Done! » ou le libellé d'attente, « Hey? ») ; le projet est en ligne 2. Quand plusieurs Sessions sont connues, « │ N sessions » au bord droit de la ligne 1 en donne le nombre total.
- La décision se prend sur l'Expression courante : si elle est interpelle (même sous un Rappel, qui prend fin), l'appui sur le Bouton Claude ne montre pas de Rappel, il chasse la Session affichée. Elle reste suivie et compte dans « N sessions » ; si l'Expression courante n'est pas interpelle (content d'une autre Session par-dessus l'attente, par exemple), l'appui montre le Rappel.
- Pendant un Rappel, l'Expression courante continue sa vie : une Expression temporaire peut expirer en dessous, et le Rappel revient alors au point de retour.
- Un Événement qui change l'Expression met fin au Rappel.
- Un Son est joué une fois, sans répétition. Seul un autre Son l'interrompt : un Événement sans Son change l'Expression mais laisse le Son finir.
