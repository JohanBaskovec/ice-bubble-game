# Publier le jeu

Liste de contrôle pour une sortie sur le Web, le Google Play Store et l'App Store.
Les règles des stores évoluent : vérifier les pages officielles au moment de publier.

## 0. Avant tout

- [ ] **Originalité** : nom, graphismes, sons, musique et niveaux doivent être originaux
      (c'est le cas aujourd'hui). Ne rien reprendre d'un jeu existant : les stores rejettent
      les copies et les ayants droit peuvent demander un retrait.
- [ ] Vérifier que le nom « Boules de Givre » n'est pas déjà une marque ou une application
      existante (recherche INPI / EUIPO, Play Store, App Store).
- [ ] Choisir la **licence du code** (propriétaire, MIT, GPL…) et l'ajouter (`LICENSE`).
      SDL (zlib), stb (domaine public) et Fredoka (OFL) autorisent un usage commercial ;
      garder leurs mentions (écran Options, README).
- [ ] Numéro de version : `project(VERSION …)` dans `CMakeLists.txt` et
      `appVersionName`/`appVersionCode` dans `platforms/android/app/build.gradle`.
- [ ] **Politique de confidentialité** : obligatoire sur les deux stores. Le jeu ne collecte
      rien et n'a pas de réseau : une page courte qui le dit suffit (à héberger, par exemple
      sur le site web du jeu).
- [ ] Captures d'écran : utiliser `--screenshot` ou des captures réelles sur appareil
      (formats demandés par chaque store).

## 1. Web

- [ ] `cmake --preset web && cmake --build --preset web`
- [ ] Publier `index.html`, `index.js`, `index.wasm`, `favicon.png` sur un hébergement
      statique en HTTPS (GitHub Pages, Netlify, Cloudflare Pages, itch.io en « HTML5 »).
- [ ] Servir `.wasm` avec le type MIME `application/wasm` (fait automatiquement par ces
      hébergeurs).
- [ ] Tester sur Chrome, Firefox, Safari (macOS et iOS) et Chrome Android : clavier,
      tactile, manette, son après le premier clic, sauvegarde des niveaux après
      rechargement de la page.

## 2. Google Play (Android)

Compte Google Play Console : 25 $, à payer une seule fois. Depuis fin 2023, les **comptes
personnels récents** doivent faire un test fermé avec au moins **12 testeurs pendant 14
jours** avant de pouvoir publier en production.

- [ ] Créer une clé de signature (à conserver précieusement, avec une sauvegarde) :
      ```bash
      keytool -genkeypair -v -keystore boules-de-givre.jks -alias bdg -keyalg RSA -keysize 2048 -validity 10000
      ```
- [ ] Créer `platforms/android/keystore.properties` (non versionné) :
      ```properties
      storeFile=/chemin/vers/boules-de-givre.jks
      storePassword=...
      keyAlias=bdg
      keyPassword=...
      ```
- [ ] Activer la *signature d'application Play* (Play App Signing) dans la console.
- [ ] `cd platforms/android && ./gradlew bundleRelease` → `app/build/outputs/bundle/release/app-release.aab`
- [ ] Vérifier le **targetSdk** exigé par Google cette année (fixé à 36 dans
      `app/build.gradle`) et la prise en charge des pages mémoire de 16 Ko (NDK r28 :
      activée par défaut).
- [ ] Fiche : titre, description courte et longue, icône 512×512
      (`assets/icon/icon-512.png`), bannière 1024×500, au moins 2 captures en paysage.
- [ ] Questionnaire de classification du contenu (IARC) : jeu sans violence réaliste →
      PEGI 3 / Tout public attendu.
- [ ] Section « Sécurité des données » : aucune donnée collectée ni partagée.
- [ ] Public cible : si des enfants sont visés, respecter le programme « Familles ».
- [ ] Test interne → test fermé → production.

## 3. App Store (iOS)

Compte Apple Developer : 99 $/an. **Un Mac avec Xcode est obligatoire** pour compiler et
envoyer le jeu.

- [ ] Choisir l'identifiant de bundle définitif (`BDG_APP_ID`, par défaut
      `com.johanbaskovec.boulesdegivre`) et le créer dans « Certificates, Identifiers & Profiles ».
- [ ] `cmake --preset ios -DBDG_APPLE_TEAM_ID=<Team ID>` puis ouvrir le projet Xcode.
- [ ] Product → Archive → Distribute App → App Store Connect.
- [ ] App Store Connect : fiche, catégorie Jeux > Arcade, captures iPhone 6,9" et iPad 13"
      en paysage, politique de confidentialité, « Données non collectées ».
- [ ] Classification par âge : 4+.
- [ ] Export compliance : `ITSAppUsesNonExemptEncryption = false` est déjà dans
      `Info.plist.in` (pas de chiffrement).
- [ ] TestFlight pour tester sur de vrais appareils, puis soumission à la revue.
- [ ] Points de revue fréquents : pas de bouton « Quitter » (déjà masqué sur iOS), prise en
      charge des manettes déclarée (`GCSupportsControllerUserInteraction`), contenu
      suffisant au lancement (règle 4.2, minimum fonctionnel) : prévoir 20 à 30 niveaux.

## 4. Bureau (optionnel)

- itch.io ou Steam (100 $ par jeu sur Steam).
- Windows : signer l'exécutable pour éviter l'alerte SmartScreen.
- macOS : signature Developer ID et notarisation Apple.
- Linux : AppImage ou Flatpak.
