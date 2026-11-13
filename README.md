# Boules de Givre

Jeu d'arcade hivernal en **C + SDL3** : les boules de Noël se sont rebellées. Tire des
rayons de givre, divise-les jusqu'à les faire éclater, grimpe aux échelles, ramasse les
bonus… seul ou à deux sur le même appareil. Avec un **éditeur de niveaux complet** :
chaque niveau est un simple fichier texte `.lvl`.

Plateformes visées : **Windows, macOS, Linux, Web, Android (Play Store), iOS (App Store)**,
avec une seule base de code.

## Démarrage rapide (bureau)

```bash
git clone --recurse-submodules <dépôt>
cd ice-bubble-game
cmake --preset desktop-debug
cmake --build --preset desktop-debug -j
./build/boules-de-givre
```

Prérequis : CMake ≥ 3.22, un compilateur C11. Sous Linux, les en-têtes de développement
X11/Wayland et PulseAudio/PipeWire permettent à SDL d'activer ces pilotes
(`sudo apt install libx11-dev libxext-dev libwayland-dev libxkbcommon-dev libpulse-dev`).

## Commandes

| | Joueur 1 | Joueur 2 |
|---|---|---|
| Clavier | Flèches + Espace | ZQSD (WASD en QWERTY) + F |
| Manette | 1re manette | 2e manette |
| Tactile | Croix à gauche, flocon à droite | — |

P / Échap : pause · M : musique · F11 : plein écran.

## Documentation

- [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md) : règles, boules, bonus, score, difficultés
- [docs/EDITOR.md](docs/EDITOR.md) : guide de l'éditeur de niveaux et raccourcis
- [docs/LEVEL_FORMAT.md](docs/LEVEL_FORMAT.md) : spécification des fichiers `.lvl`
- [docs/BUILD.md](docs/BUILD.md) : compiler pour bureau, Web, Android, iOS
- [docs/RELEASE.md](docs/RELEASE.md) : publier sur le Web, le Play Store et l'App Store
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) : organisation du code
- [docs/ROADMAP.md](docs/ROADMAP.md) : prochaines étapes
- [CLAUDE.md](CLAUDE.md) : guide pour les sessions Claude Code

## Licences

- Code du jeu : à définir par l'auteur (voir `docs/RELEASE.md`).
- [SDL3](https://libsdl.org) : licence zlib.
- [stb_truetype](https://github.com/nothings/stb) : domaine public / MIT.
- Police [Fredoka](https://github.com/hafontia/Fredoka-One) : SIL Open Font License 1.1
  (`assets/fonts/OFL-Fredoka.txt`). Instance statique SemiBold générée depuis la police
  variable.
