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

**Repos** : l'Expression par défaut, quand rien ne se passe : un visage neutre qui cligne des yeux de temps en temps.

**Expression temporaire** : une Expression qui revient d'elle-même au Repos après une durée propre à son type.

**Expression persistante** : une Expression qui reste affichée jusqu'à ce qu'un autre Événement la remplace.

## Types d'Événement

| Type | Ce qui s'est passé | Expression | Son |
|---|---|---|---|
| `claude_done` | Claude Code a fini de répondre | content, temporaire | — |
| `claude_waiting` | Claude Code attend une réponse ou une autorisation | interpelle, persistante | carillon |
| `claude_working` | On a répondu à Claude Code, il reprend son travail | met fin à `claude_waiting` (sans effet sinon) | — |
| `music` | Un morceau est en cours de lecture (le texte est le titre, accompagné d'un artiste) | musique, temporaire | — |
| `git_push` | Un push git a été fait | animation de push, temporaire | fanfare |

## Relations

- Un Événement remplace toujours l'Expression courante : c'est le dernier reçu qui l'emporte, même s'il vient d'une autre session de Claude Code.
- Seul `claude_waiting` produit une Expression persistante.
- `claude_working` n'agit que si l'Expression courante est celle de `claude_waiting`.
- Un Son est joué une fois, sans répétition. Seul un autre Son l'interrompt : un Événement sans Son change l'Expression mais laisse le Son finir.
