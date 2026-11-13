# Architecture

## Vue d'ensemble

```
            SDL3 (main callbacks)
                   │
              main.c ──► app.c ── scène courante ──┬─ scene_menu.c     (+ fond animé : game_demo_frame)
                   │       │                       ├─ scene_levels.c
                   │       │                       ├─ scene_options.c
                   │       │                       ├─ game.c / game_draw.c
                   │       │                       └─ scene_editor.c ──(test)──► game.c
                   │       │
                   │       ├─ input.c   clavier / manettes / tactile → PlayerInput
                   │       ├─ ui.c      boutons, champs texte, navigation
                   │       ├─ audio.c   synthé (thread audio SDL)
                   │       └─ settings.c, storage.c (fichiers, IDBFS)
                   │
              gfx.c (formes) · font.c (texte) · retro.c (pixel art) ── SDL_RenderGeometry
              level.c (format .lvl, C pur) ◄── storage.c, game.c, scene_editor.c, tests
              embedded_assets.c (généré : police + levels/*.lvl)
```

## Boucle principale

`main.c` utilise `SDL_MAIN_USE_CALLBACKS` : SDL appelle `SDL_AppIterate` à chaque image.
C'est obligatoire sur le Web (pas de boucle bloquante dans un navigateur) et recommandé sur
iOS et Android. `app_iterate` :

1. calcule `dt` (plafonné à 1/30 s) ;
2. sur le Web, charge les réglages quand IndexedDB est prêt ;
3. efface l'écran, `ui_begin_frame()`, appelle `<scène>_frame(dt)`, `ui_end_frame()` ;
4. présente l'image (VSync).

Les scènes font mise à jour et dessin dans la même fonction (style « mode immédiat ») :
les boutons sont dessinés et testés au même endroit.

## Rendu

- **Espace logique fixe** : 800×540 pour le jeu et les menus, 1100×640 pour l'éditeur.
  `SDL_SetRenderLogicalPresentation(..., LETTERBOX)` met à l'échelle et centre ; les
  événements sont convertis avec `SDL_ConvertEventToRenderCoordinates`. Comme tout est
  vectoriel, le rendu reste net quelle que soit la résolution (fenêtre HiDPI).
- `gfx.c` construit des triangles (`SDL_RenderGeometry`) : rectangles et rectangles
  arrondis en dégradé, cercles, anneaux, ellipses, secteurs, polygones convexes, traits
  épais, disque à dégradé radial (boules, bonshommes). L'état global contient un décalage
  (translation) et une opacité multiplicatrice, utilisés par l'éditeur et les effets de
  clignotement.
- `font.c` rastérise Fredoka avec stb_truetype en 3 tailles (18, 36, 72 px) dans un atlas
  2048×1024 et choisit la taille selon l'échelle d'affichage réelle. En style rétro
  (`font_set_pixel`), il utilise la police bitmap 8×8 `third_party/font8x8` (domaine
  public) en chasse proportionnelle, agrandie sans lissage ; la largeur est bornée à
  ~1,1 × celle de Fredoka pour ne pas casser les mises en page. L'éditeur garde Fredoka.

### Style rétro (`retro.c`)

- Palette fixe de 256 couleurs : 16 rampes (gris, rouge, orange… sable) × 16 nuances,
  `PAL(rampe, niveau)`. Ombres décalées vers le froid, lumières vers le chaud.
- Tout le pixel art est **généré au démarrage** dans des canevas indexés (`Uint16`,
  `0xFFFF` = transparent) avec tramage ordonné de Bayer 4×4, puis converti en textures
  `SDL_SCALEMODE_NEAREST` : boules (normales et gelées), bonshommes (debout + 2 images
  d'escalade, 2 palettes), 8 fruits (dessins ASCII 12×12), carreaux de quadrillage, échelle
  et 3 décors 400×270 (ciel, nuages, montagnes par déplacement du point milieu, collines,
  sapins, lac avec reflet, buissons, roseaux, cadre en quadrillage).
- `draw_game` dessine la partie dans une cible 400×270 (`retro_target`, échelle de rendu
  0,5 : le code continue de travailler en unités 800×540) puis l'agrandit ×2. Les formes
  vectorielles (rayons, particules, cœurs) sont donc pixelisées elles aussi, et la police
  pixel y utilise des pixels de 2 unités (`font_set_pixel_target`).
- Les fonctions `draw_platform`, `draw_ball`… choisissent le style selon
  `g_settings.retro` ; l'éditeur les réutilise et affiche donc le style courant.
- `ui.c` dessine des boutons et panneaux biseautés quand la police pixel est active.

## Simulation (`game.c`)

État unique `g_game` (structure `Game`, tableaux de taille fixe). États :
`READY → PLAY → CLEAR → (niveau suivant | DONE)`, `PAUSE`, `OVER`.
Modes : campagne, niveau isolé, test depuis l'éditeur, démo (fond des menus).

`game_ball_step()` (physique d'une boule) est publique : l'éditeur s'en sert pour
dessiner les trajectoires prévues.

## Entrées

- `input_player(i, n)` fusionne clavier (codes physiques, donc ZQSD et WASD sont la même
  touche), manettes (n-ième manette = n-ième joueur en mode 2 joueurs) et zones tactiles
  (multi-doigts, croix 8 directions + bouton de tir).
- Le tactile s'affiche automatiquement dès qu'un doigt touche l'écran (et toujours sur
  Android/iOS), réglable dans Options.
- L'UI reçoit les événements souris (SDL en génère aussi à partir du tactile) et la
  navigation clavier/manette (flèches/croix + Entrée/A, Échap/B pour revenir).

## Fichiers et persistance (`storage.c`)

- Dossier de préférences : `SDL_GetPrefPath("johanbaskovec", "boules-de-givre")`.
- `settings.ini` (clé = valeur) et `levels/*.lvl`.
- Écriture atomique : fichier `.tmp`, puis renommage.
- Web : `/libsdl` monté en IDBFS ; `FS.syncfs` charge au démarrage puis enregistre après
  chaque écriture. `storage_ready()` attend la fin du chargement.
- Campagne : intégrée (`g_campaign_levels`, générée depuis `levels/`), ou lue/écrite dans
  le dossier `--levels-dir`.

## Audio (`audio.c`)

Flux SDL mono float 44,1 kHz avec callback. 64 voix (sinus, triangle, carré, dent de
scie) avec glissando exponentiel et enveloppe attaque/décroissance. Séquenceur de musique
de 64 pas joué dans le callback. Le thread principal ajoute des voix sous
`SDL_LockAudioStream`.

## Cycle de vie mobile

`SDL_EVENT_WILL_ENTER_BACKGROUND` / `DID_ENTER_BACKGROUND` : pause de la partie,
sauvegarde des réglages, coupure du son. `DID_ENTER_FOREGROUND` : reprise du son.
La perte de focus met aussi le jeu en pause. Le bouton Retour d'Android agit comme Échap.

## Build

- `CMakeLists.txt` : SDL3 (système, sous-module ou FetchContent), bibliothèque
  `bdg_core` (format de niveau, sans SDL), exécutable (ou `libmain.so` sur Android),
  options par plateforme (Emscripten, iOS, macOS, Windows), tests.
- `cmake/EmbedAssets.cmake` : génère `build/generated/embedded_assets.c` (police +
  niveaux) ; relancé automatiquement quand un `.lvl` change. Un nouveau fichier `.lvl`
  est détecté à la reconfiguration (`CONFIGURE_DEPENDS`).

## Tests

- `test_level` : lecture, écriture, aller-retour, erreurs, bornes, noms de fichiers, et
  validation de chaque niveau de `levels/`.
- `editor_smoke` (Linux) : lance le jeu hors écran, rejoue `tests/editor_smoke.script`
  (tracer une plateforme, une échelle, une boule, déplacer un départ, élargir une
  plateforme, enregistrer sous, annuler/rétablir, tester, pause, retour à l'éditeur) et
  vérifie le fichier `.lvl` produit.
