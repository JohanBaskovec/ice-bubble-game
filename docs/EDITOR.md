# Éditeur de niveaux

Accessible depuis le menu principal (« Éditeur »), depuis « Mes niveaux » (« Éditer »,
« Nouveau niveau ») ou directement avec `boules-de-givre --editor`.

```
┌ Nouveau · Ouvrir… · Enregistrer · Enregistrer sous… · Annuler · Rétablir · Tester · 1/2 joueurs · Menu ┐
│                                                         │ Outils                       │
│        Terrain 800×540 (identique au jeu)               │ Options boule / sélection    │
│                                                         │ Niveau : nom, teinte,        │
│                                                         │ renforts, grille, trajets    │
└ Barre d'état : aide de l'outil ou message · fichier · compteurs · position du curseur ┘
```

## Outils

| Outil | Touche | Utilisation |
|---|---|---|
| Sélection | V | Clic : sélectionner. Glisser : déplacer. Poignées : élargir une plateforme (bords) ou allonger une échelle ou un laser (haut/bas). |
| Plateforme | P | Glisser horizontalement pour tracer. Clic simple : plateforme de 120. |
| Échelle | L | Clic sur une plateforme : échelle de son sommet jusqu'à la surface du dessous. Clic dans le vide : échelle depuis ce point. |
| Boule | B | Clic : poser une boule avec la taille (1 à 4) et la direction du panneau. Clic sur une boule existante : la déplacer. |
| Départ J1 / J2 | 1 / 2 | Clic : position de départ, posée automatiquement sur la surface en dessous. |
| Laser | R | Clic : laser vertical, du plafond (ou du dessous de la plateforme au-dessus) jusqu'à la surface en dessous. Les boules rebondissent dessus, les joueurs le traversent. |
| Gomme | E | Clic : effacer l'objet sous le curseur. |

**Clic droit** : efface l'objet sous le curseur, quel que soit l'outil.

## Raccourcis

| Raccourci | Action |
|---|---|
| Ctrl+Z / Ctrl+Y (ou Ctrl+Maj+Z) | Annuler / rétablir (64 étapes) |
| Ctrl+S / Ctrl+Maj+S | Enregistrer / enregistrer sous |
| Ctrl+O / Ctrl+N | Ouvrir / nouveau |
| Ctrl+D | Dupliquer la sélection |
| Suppr / Retour arrière | Supprimer la sélection |
| Flèches (Maj : par 10) | Déplacer la sélection au pixel près |
| X | Inverser la direction de la boule sélectionnée |
| G | Grille magnétique (pas de 10) |
| Maj pendant un glisser | Placement libre, sans grille |
| T ou F5 | Tester le niveau |
| Échap | Désélectionner, puis revenir au menu |

Sur Mac, Cmd remplace Ctrl.

## Panneau « Niveau »

- **Nom** : affiché en jeu et utilisé pour nommer le fichier.
- **Décor** (style rétro) : Auto, Lac, Crépuscule ou Nuit. « Auto » prend le décor de la
  tranche de 10 niveaux en campagne.
- **Teinte** (style moderne) : couleur du ciel (par pas de 15°). Le panneau montre l'un
  ou l'autre selon le style choisi dans les options.
- **Renforts** : boules qui tombent du plafond régulièrement. « Selon difficulté » (36 s en
  Débutant, 16 s en Expert), « Aucun », ou un intervalle fixe de 5 à 120 s.
- **Grille** et **Trajectoires** : la seconde option affiche le trajet de chaque boule
  pendant 4 secondes, calculé avec la vraie physique du jeu. Pratique pour éviter qu'une
  boule reste coincée ou tombe directement sur un départ.

## Fichiers

- **Enregistrer** écrit dans le fichier courant. Pour un nouveau niveau, ou un niveau de la
  campagne ouvert en copie, la fenêtre « Enregistrer sous » s'ouvre.
- **Enregistrer sous** propose un nom ; le fichier est nommé d'après lui
  (`mon-niveau.lvl`), avec un avertissement si ce fichier existe déjà.
- Une étoile `*` dans la barre d'état signale des modifications non enregistrées ;
  l'éditeur demande confirmation avant de les perdre.
- **Mode développeur** : avec `--levels-dir levels`, « Enregistrer sous » permet de choisir
  la destination *campagne (dépôt)*, ce qui écrit directement dans `levels/`. Il faut ensuite
  recompiler pour intégrer les niveaux au binaire.

## Tester

« Tester » lance le niveau tel qu'il est (même non enregistré) en difficulté Normal, avec
1 ou 2 joueurs selon le bouton voisin. Pause → « Retour à l'éditeur », ou fin du niveau →
« Retour à l'éditeur ». Le niveau en cours d'édition est conservé.

## Conseils de conception

- Laisser au moins 60 unités entre une boule et le départ d'un joueur (l'éditeur avertit).
- Une plateforme bloque les rayons : un joueur sous une plateforme ne peut tirer que
  jusqu'à elle.
- Les grosses boules rebondissent haut (300) : une plateforme basse au-dessus du sol peut
  créer des zones où elles restent piégées. Vérifier avec les trajectoires.
- Un niveau sans renfort (`Aucun`) se termine forcément ; avec renforts, la pression monte.
