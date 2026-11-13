# Format des fichiers de niveau `.lvl` (version 1)

Un niveau est un fichier **texte UTF-8**, une instruction par ligne. Il se lit et se modifie
à la main, se compare facilement avec `git diff` et s'échange par simple copie.

## Exemple

```text
# Boules de Givre - fichier de niveau (voir docs/LEVEL_FORMAT.md)
format 1
name "Trois marches"
hue 268
reinforcements default

# joueurs : start <1|2> <x centre> <y pieds>
start 1 336 500
start 2 464 500

# platform <x> <y> <largeur>
platform 0 380 200
platform 300 300 200

# ladder <x gauche> <haut> <bas>
ladder 85 380 500
ladder 385 300 500

# ball <x> <y> <taille 1-4> <left|right>
ball 200 150 4 right
ball 600 150 2 left
```

## Règles de syntaxe

- Les mots sont séparés par des espaces ou des tabulations.
- `#` commence un commentaire (jusqu'à la fin de la ligne), sauf dans une chaîne.
- Chaînes entre guillemets doubles ; `\"` et `\\` pour un guillemet ou un antislash.
- Fins de ligne `\n` ou `\r\n` ; BOM UTF-8 accepté.
- Nombres décimaux avec un point (`12.5`). L'éditeur écrit des entiers quand c'est possible.
- **Les clés inconnues sont ignorées** : un fichier écrit par une version plus récente
  reste lisible tant que `format` n'augmente pas.

## Repère

Coordonnées en unités logiques du terrain : **800 × 540**, origine en haut à gauche, y vers
le bas.

| Repère | y |
|---|---|
| Plafond (bas du bandeau des scores) | 36 |
| Sol (haut de la neige) | 500 |

Épaisseur d'une plateforme : 14. Largeur d'une échelle : 30. Hauteur d'un joueur : 44.

## Instructions

| Instruction | Obligatoire | Description |
|---|---|---|
| `format 1` | **oui** | Version du format. Un fichier sans cette ligne est refusé. |
| `name "texte"` | non | Nom affiché (47 octets max). Défaut : « Sans titre ». |
| `hue <0-359>` | non | Teinte du ciel en degrés. Défaut : 215. |
| `reinforcements <v>` | non | `default` (selon la difficulté), `off` (aucun) ou un intervalle de 3 à 600 secondes. |
| `start <1\|2> <x> <y>` | non | Départ d'un joueur : `x` = centre, `y` = pieds (surface). Défaut : (336, 500) et (464, 500). |
| `platform <x> <y> <w>` | non | Plateforme : coin haut-gauche et largeur. 32 max. |
| `ladder <x> <haut> <bas>` | non | Échelle : bord gauche, y du sommet (là où on sort), y du bas. 32 max. |
| `ball <x> <y> <taille> <dir>` | en pratique | Boule : centre, taille 1 (petite) à 4 (géante), `left` ou `right`. 24 max. |

### Tailles des boules

| Taille | Rayon | Hauteur de rebond | Points | Se divise en |
|---|---|---|---|---|
| 1 | 9 | 95 | 200 | — |
| 2 | 15 | 165 | 150 | 2 × taille 1 |
| 3 | 23 | 235 | 100 | 2 × taille 2 |
| 4 | 33 | 300 | 50 | 2 × taille 3 |

## Bornes appliquées au chargement (`level_clamp`)

Les valeurs hors limites sont ramenées dans le terrain plutôt que refusées :
plateformes entre y=56 et y=466, largeur ≥ 16 ; échelles d'au moins 20 de haut ;
boules entièrement entre plafond et sol ; départs entre y=86 et le sol.

## Erreurs bloquantes

Ligne `format` absente ou version trop récente, guillemet non fermé, mauvais nombre de
valeurs, nombre invalide, taille de boule hors de 1 à 4, direction autre que
`left`/`right`, trop d'objets. Le message indique la ligne : `ligne 7 : direction de boule : 'left' ou 'right'`.

## Avertissements (`level_validate`)

Le niveau se charge, mais l'éditeur affiche un avertissement : aucune boule, boule qui
chevauche une plateforme, boule trop proche d'un départ de joueur, échelle trop courte.

## Emplacements

| Origine | Dossier | Modifiable |
|---|---|---|
| Campagne | `levels/` du dépôt, intégré au binaire, joué par ordre alphabétique | via `--levels-dir levels` (dev) |
| Niveaux perso, Linux | `~/.local/share/johanbaskovec/boules-de-givre/levels/` | oui |
| Niveaux perso, Windows | `%APPDATA%\johanbaskovec\boules-de-givre\levels\` | oui |
| Niveaux perso, macOS | `~/Library/Application Support/johanbaskovec/boules-de-givre/levels/` | oui |
| Android / iOS | dossier privé de l'application | oui (dans le jeu) |
| Web | IndexedDB du navigateur (IDBFS) | oui (dans le jeu) |

Nom de fichier : dérivé du nom du niveau (`"Le Belvédère !"` → `le-belvedere.lvl`).
