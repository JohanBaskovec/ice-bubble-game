# Compiler pour chaque plateforme

Une seule base de code C11 + SDL3. SDL est un sous-module Git (`third_party/SDL`, tag
`release-3.4.18`) compilé avec le jeu : pas besoin de l'installer.

```bash
git submodule update --init --depth 1
```

Sans le sous-module, CMake télécharge SDL avec FetchContent. Pour utiliser un SDL3 déjà
installé : `-DBDG_USE_SYSTEM_SDL=ON`.

| Plateforme | Machine de build | Outils | Statut |
|---|---|---|---|
| Linux | Linux | CMake, gcc/clang | ✅ vérifié (build, tests, captures) |
| Windows | Windows | CMake, Visual Studio 2022 ou MinGW | à vérifier |
| macOS | Mac | CMake, Xcode | à vérifier |
| Web | Linux, Mac ou Windows | Emscripten (emsdk) | CI configurée, à vérifier |
| Android | Linux, Mac ou Windows | Android Studio, SDK 36, NDK 28 | CI configurée, à vérifier |
| iOS | **Mac obligatoire** | Xcode 16+, compte Apple Developer pour l'appareil | CI configurée, à vérifier |

## Bureau

```bash
cmake --preset desktop-debug          # ou desktop-release
cmake --build --preset desktop-debug -j
ctest --preset desktop-debug
./build/boules-de-givre
```

Linux : pour que SDL active X11, Wayland et l'audio, installer les en-têtes avant de
configurer (sinon la fenêtre ne pourra pas s'ouvrir) :

```bash
sudo apt install build-essential cmake libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxi-dev libxss-dev libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev \
  libpulse-dev libpipewire-0.3-dev libasound2-dev libudev-dev libdbus-1-dev
```

Windows : `cmake --preset desktop-release` puis ouvrir la solution ou
`cmake --build build-release --config Release`. L'exécutable est autonome (SDL lié
statiquement).

macOS : idem ; produit un bundle `.app`.

### Options de ligne de commande

| Option | Effet |
|---|---|
| `--levels-dir <dossier>` | La campagne est lue dans ce dossier et l'éditeur peut y écrire (mode dev) |
| `--editor` | Ouvre l'éditeur au lancement |
| `--play <n>` | Lance la campagne au niveau n |
| `--fullscreen` | Plein écran |
| `--screenshot <f.bmp> [--frames n]` | Capture après n images puis quitte |
| `--script <fichier>` | Rejoue un scénario d'entrées (tests) |

Rendu sans fenêtre (CI, captures) : `SDL_VIDEO_DRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIO_DRIVER=dummy`.

## Web (Emscripten)

```bash
# une fois : installer emsdk
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
source ~/emsdk/emsdk_env.sh

cmake --preset web        # utilise $EMSDK
cmake --build --preset web -j
# ou : emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web

cd build-web && python3 -m http.server 8000   # puis http://localhost:8000
```

Fichiers à publier : `index.html`, `index.js`, `index.wasm`, `favicon.png`. Les ouvrir
directement en `file://` ne fonctionne pas : il faut un serveur HTTP.

Détails : page `platforms/web/shell.html`, canvas qui remplit la page
(`SDL_WINDOW_FILL_DOCUMENT`), sauvegardes dans IndexedDB (IDBFS, monté sur `/libsdl`).
Le son démarre après la première interaction (règle des navigateurs).

## Android

Projet Gradle dans `platforms/android`. Il compile le `CMakeLists.txt` racine
(bibliothèques `libSDL3.so` + `libmain.so`) et reprend les classes Java de SDL depuis le
sous-module.

1. Installer Android Studio, puis dans le SDK Manager : SDK Platform 36, NDK
   `28.2.13676358`, CMake 3.22.1.
2. Ouvrir `platforms/android` dans Android Studio (le wrapper Gradle est fourni), ou en
   ligne de commande :

```bash
cd platforms/android
echo "sdk.dir=$HOME/Android/Sdk" > local.properties
./gradlew assembleDebug                 # APK de test
./gradlew installDebug                  # installer sur un appareil branché (débogage USB)
./gradlew bundleRelease                 # AAB pour le Play Store (signature : docs/RELEASE.md)
```

ABI compilées : `arm64-v8a`, `armeabi-v7a`, `x86_64` (émulateur). Orientation paysage
imposée, manettes acceptées.

## iOS

Sur un Mac avec Xcode :

```bash
cmake --preset ios -DBDG_APPLE_TEAM_ID=ABCDE12345   # votre Team ID
open build-ios/BoulesDeGivre.xcodeproj
```

Dans Xcode : schéma `boules-de-givre`, choisir un simulateur ou un iPhone branché, Run.
Pour un appareil réel, un compte Apple Developer (gratuit pour tester sur son propre
iPhone, 99 $/an pour publier) et la signature automatique suffisent.

Fichiers : `platforms/ios/Info.plist.in` (paysage, plein écran, catégorie arcade),
`LaunchScreen.storyboard`, `Assets.xcassets` (icône 1024×1024 générée par
`tools/gen_icons.py`).

## Icônes

```bash
python3 tools/gen_icons.py    # nécessite Pillow
```

Régénère les icônes Android (mipmap), iOS (1024), Web (favicon) et l'icône 512×512 de la
fiche Play Store (`assets/icon/icon-512.png`).

## Intégration continue

`.github/workflows/ci.yml` compile Linux (avec tests), Web, Android (APK debug) et iOS
(simulateur, sans signature) à chaque push, et publie les builds Web et Android en
artefacts.
