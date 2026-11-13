# CLAUDE.md

Guide pour Claude (et tout contributeur) travaillant sur **Boules de Givre**, jeu d'arcade
en **C11 + SDL3 + CMake** destiné au bureau, au Web (Emscripten), à Android et à iOS.

## Commandes essentielles

```bash
git submodule update --init --depth 1        # SDL3 (third_party/SDL, tag release-3.4.18)
cmake --preset desktop-debug                 # configure dans build/
cmake --build --preset desktop-debug -j      # compile build/boules-de-givre
ctest --preset desktop-debug                 # tests : format de niveau + scénario éditeur
./build/boules-de-givre                      # lancer le jeu
./build/boules-de-givre --levels-dir levels  # mode dev : l'éditeur modifie la campagne du dépôt
./build/boules-de-givre --editor             # ouvrir directement l'éditeur
```

Vérifier un rendu sans fenêtre (utile pour Claude, pas d'écran nécessaire) :

```bash
SDL_VIDEO_DRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy \
  ./build/boules-de-givre --play 3 --screenshot /tmp/shot.bmp --frames 120
```

`--script fichier` rejoue des clics et des touches (format décrit dans `src/app.c` et
`tests/editor_smoke.script`) ; la commande `shot` du script fait une capture. Convertir
les BMP en PNG avec `convert` (ImageMagick) avant de les lire.

Builds des autres plateformes : voir `docs/BUILD.md` (Web : `emcmake`, Android : Gradle
dans `platforms/android`, iOS : `cmake --preset ios` sur macOS).

## Règles du projet

- **Création originale uniquement.** Le jeu s'inspire d'un genre d'arcade (tirer sur des
  boules qui se divisent) mais tout est original : nom, personnages (bonshommes de neige),
  boules de Noël, décors, musique, sons, niveaux. Ne jamais reprendre le nom, les graphismes,
  les sons, les niveaux ou l'apparence d'un jeu existant (Freeze Ball, Pang, etc.).
- **Pas de multijoueur en ligne.** Seulement le jeu local (2 joueurs sur un même appareil :
  clavier partagé et/ou manettes). Pas de code réseau.
- **Chaque niveau est un fichier `.lvl`** (texte, format documenté dans
  `docs/LEVEL_FORMAT.md`). La campagne est dans `levels/` et intégrée au binaire au moment
  de la compilation. Les niveaux des joueurs vont dans le dossier de préférences SDL.
- **Zéro ressource binaire chargée à l'exécution.** Graphismes vectoriels dessinés en code
  (`gfx.c`), sons synthétisés (`audio.c`), police et niveaux intégrés au binaire par
  `cmake/EmbedAssets.cmake`. Résultat : aucun problème de chemins entre plateformes.
- **Interface en français**, chaînes en UTF-8 avec accents. La police (`font.c`) ne contient
  que certaines plages Unicode (ASCII, Latin-1, quelques signes) : pour un nouveau symbole,
  l'ajouter à `RANGES` ou le dessiner (`gfx_heart`, `gfx_snowflake`, `gfx_arrow`…).
- Pas de bouton « Quitter » sur iOS ni sur le Web (règle Apple / inutile dans un navigateur).

## Architecture (détails : `docs/ARCHITECTURE.md`)

| Fichier | Rôle |
|---|---|
| `src/main.c` | Callbacks SDL3 (`SDL_AppInit/Event/Iterate/Quit`) : une seule boucle pour toutes les plateformes |
| `src/app.c` | Fenêtre, scènes, cycle de vie mobile, options de ligne de commande, lecteur de scripts |
| `src/level.c/.h` | Structure `Level`, lecture/écriture `.lvl`, validation. **C pur, sans SDL** (testable) |
| `src/storage.c` | Fichiers : campagne intégrée ou `--levels-dir`, niveaux perso, réglages, IDBFS sur le Web |
| `src/game.c` | Simulation : joueurs, boules, rayons, bonus, états de partie, overlays pause/fin |
| `src/game_draw.c` | Rendu du jeu (décor, bonshommes, boules, HUD), réutilisé par l'éditeur |
| `src/scene_editor.c` | Éditeur de niveaux (outils, sélection, annuler/rétablir, fichiers, test) |
| `src/scene_menu.c`, `scene_levels.c`, `scene_options.c` | Menus |
| `src/ui.c` | Mini-UI en mode immédiat (boutons, champs texte, navigation clavier/manette) |
| `src/gfx.c`, `src/font.c` | Primitives vectorielles sur `SDL_RenderGeometry`, texte via stb_truetype |
| `src/input.c` | Clavier, manettes, commandes tactiles → `PlayerInput` par joueur |
| `src/audio.c` | Synthé : musique séquencée + effets, dans le callback audio SDL |

Coordonnées : le terrain fait **800×540** unités logiques (`config.h`), plafond à y=36,
sol à y=500. SDL met à l'échelle avec `SDL_SetRenderLogicalPresentation` (letterbox).
L'éditeur utilise un espace logique plus grand (1100×640) avec le terrain décalé en (16, 56).

## Conventions de code

- C11 strict (`CMAKE_C_EXTENSIONS OFF`) : pas de `typeof` ni d'extensions GNU.
  Compilation sans aucun warning avec `-Wall -Wextra -Wshadow`, à garder ainsi.
- Préfixes : `gfx_`, `font_`, `ui_`, `level_`, `storage_`, `game_`, `audio_`, `input_`.
  Fonctions de scène : `<scene>_enter()` / `<scene>_frame(dt)`.
- Utiliser les fonctions SDL (`SDL_snprintf`, `SDL_strlcpy`, `SDL_LoadFile`…) partout sauf
  dans `level.c` (C standard pour les tests).
- État global par module (`static`), pas d'allocation dynamique dans la boucle de jeu
  (tableaux de taille fixe, voir `MAX_*` dans `game.h`).
- `ui_back_pressed()` **consomme** l'appui sur Échap/Retour : un seul écran doit réagir.
- Commentaires en français, sans accents dans le code (ASCII) ; accents dans les chaînes
  affichées.

## Ajouter / modifier

- **Un niveau de campagne** : `./build/boules-de-givre --levels-dir levels`, éditeur,
  « Enregistrer sous » → destination campagne. Ou écrire le `.lvl` à la main. Les fichiers
  sont joués dans l'ordre alphabétique (`001-…`, `002-…`). `ctest` valide tous les niveaux.
- **Un champ dans le format de niveau** : `level.h` (struct), `level_parse`,
  `level_serialize`, `level_clamp`, test dans `tests/test_level.c`, puis
  `docs/LEVEL_FORMAT.md`. Incrémenter `LEVEL_FORMAT_VERSION` seulement si un ancien jeu ne
  peut pas lire le nouveau fichier ; les clés inconnues sont ignorées.
- **Un bonus** : `PupKind` dans `game.h`, poids dans `drop_pup`, effet dans `apply_pup`,
  couleur/icône dans `game_draw.c` (`PUP_COLORS`, `pup_icon`).
- **Un son** : entrée dans `Sfx` (`audio.h`) et recette dans `audio_play`.

## Vérifications avant de livrer

1. `cmake --build --preset desktop-debug` sans warning.
2. `ctest --preset desktop-debug` vert.
3. Pour un changement visuel : capture hors écran (voir plus haut) et regarder le résultat.
4. Mettre à jour la doc concernée (`docs/`).

Ce qui n'a pas pu être vérifié localement à la création du projet : les builds Web,
Android et iOS (pas d'Emscripten, de SDK Android ni de Mac sur la machine de dev). La CI
(`.github/workflows/ci.yml`) les compile ; à surveiller lors du premier push.
