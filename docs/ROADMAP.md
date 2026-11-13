# Feuille de route

## Fait (v0.1)

- Moteur C11 + SDL3 multiplateforme (callbacks SDL3, rendu vectoriel, texte, synthé).
- Gameplay repris du prototype HTML : boules qui se divisent, rayons, échelles,
  plateformes, 5 bonus, renforts, 4 difficultés, codes de reprise, record.
- 2 joueurs en local (clavier partagé, manettes), commandes tactiles.
- 10 niveaux de campagne en fichiers `.lvl`.
- Éditeur complet : 7 outils, sélection, poignées, grille, annuler/rétablir, trajectoires,
  propriétés du niveau, ouvrir / enregistrer / enregistrer sous, test immédiat.
- Menus : principal, niveaux (jouer / éditer / supprimer), options.
- Projets Web, Android et iOS, icônes, CI, tests automatiques.

## À faire avant la première sortie

1. **Vérifier les builds Web, Android et iOS** sur de vrais appareils (CI au premier push,
   puis tests manuels) : tactile, safe area (encoche), son, sauvegardes.
2. **Contenu** : passer à 25–30 niveaux de campagne avec une vraie courbe de difficulté
   (l'éditeur en mode `--levels-dir levels` est fait pour ça).
3. **Commandes tactiles** : tester sur téléphone et ajuster la taille et la position des
   boutons (`input.c`) ; tenir compte de `SDL_GetWindowSafeArea`.
4. **Éditeur sur tablette** : vérifier l'ergonomie au doigt (pas de clic droit ni de
   raccourcis : gomme et boutons du panneau).
5. **Partage de niveaux** sur mobile et Web : importer/exporter un `.lvl` (menu de partage
   iOS/Android, téléchargement/téléversement sur le Web).
6. Écran de fin de campagne / crédits.
7. Licence du code, politique de confidentialité, fiches des stores (voir `RELEASE.md`).

## Idées

- Nouveaux objets dans le format de niveau : plateformes de glace glissantes, blocs
  destructibles, téléporteurs, vent (les clés inconnues étant ignorées, les anciens jeux
  restent compatibles avec les fichiers qui n'utilisent pas ces nouveautés).
- Boules spéciales : qui ne se divisent pas, qui rebondissent plus vite, piquantes.
- Mode « contre la montre » et mode « survie » (renforts de plus en plus rapides).
- Vibrations (`SDL_RumbleGamepad`, retour haptique mobile).
- Musiques supplémentaires (une par lot de 5 niveaux), réglage du volume de la musique.
- Traduction anglaise (table de chaînes), choix de la langue.
- Succès locaux, statistiques.
- Android TV (bannière `android:banner`, navigation entièrement à la manette : déjà
  possible dans les menus).
