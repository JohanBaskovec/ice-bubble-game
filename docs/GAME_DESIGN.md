# Conception du jeu

## Pitch

Les boules de Noël se sont rebellées. Un bonhomme de neige (ou deux, en coopération locale)
les fait éclater avec un rayon de givre. Chaque boule touchée se divise en deux plus
petites, jusqu'à disparaître. Le niveau est gagné quand il n'en reste plus.

Le jeu se joue en mode paysage, au clavier, à la manette ou au tactile, en solo ou à deux
sur le même appareil. **Pas de jeu en ligne.**

## Règles

- Le joueur marche à gauche et à droite, grimpe aux échelles (haut/bas devant une échelle,
  sortie par gauche/droite en haut ou en bas) et tire **verticalement**.
- **Rayon de givre** (tir normal) : monte depuis les pieds jusqu'au plafond ou à la première
  plateforme au-dessus. Un seul rayon à la fois par joueur (deux avec le bonus ×2).
- Toucher une boule : elle éclate. Taille 2 à 4 : deux boules plus petites partent vers la
  gauche et la droite.
- Être touché par une boule : perte d'une vie (sauf bouclier), retour au départ, 2,5 s
  d'invincibilité. Plus de vies pour aucun joueur : fin de partie.
- **Renforts** : une jauge en bas de l'écran ; quand elle est vide, une nouvelle boule
  tombe du plafond (un triangle rouge clignote à l'endroit prévu 2,5 s avant). Leur taille
  grossit avec le temps passé dans le niveau.
- La vitesse des boules augmente doucement avec le temps (jusqu'à +50 % après 55 s).

## Boules

| Taille | Rayon | Rebond | Points | Couleur |
|---|---|---|---|---|
| 1 | 9 | 95 | 200 | rose |
| 2 | 15 | 165 | 150 | jaune |
| 3 | 23 | 235 | 100 | vert |
| 4 | 33 | 300 | 50 | bleu |

Les petites boules rapportent plus parce qu'elles sont plus dures à toucher. Physique :
gravité 520, vitesse horizontale constante 100, rebond au sol toujours à la même hauteur
selon la taille ; rebonds sur les murs, le plafond et les plateformes (dessus, dessous,
côtés, coins).

## Bonus

16 % de chances qu'une boule éclatée lâche un bonus (il disparaît après 9 s).

| Bonus | Probabilité | Effet |
|---|---|---|
| R : tir rapide | 3/11 | 8 s de petits projectiles à cadence élevée |
| ×2 : double rayon | 3/11 | 12 s avec deux rayons simultanés |
| Gel | 2/11 | Toutes les boules figées pendant 4,5 s |
| Bouclier | 2/11 | Absorbe le prochain coup |
| Vie | 1/11 | +1 vie |

## Score

- Points de la boule éclatée, attribués au joueur qui a tiré.
- **Bonus de temps** à la fin d'un niveau : `(60 − secondes) × 20` si positif, pour chaque
  joueur encore en vie.
- Record (meilleur score de campagne) sauvegardé dans les réglages.

## Difficultés

| | Vitesse des boules | Vies | Renforts toutes les |
|---|---|---|---|
| Débutant | ×0,72 | 5 | 36 s |
| Normal | ×0,9 | 3 | 28 s |
| Difficile | ×1,05 | 3 | 22 s |
| Expert | ×1,22 | 2 | 16 s |

## Campagne

Les niveaux de `levels/` dans l'ordre. Après le dernier, la campagne recommence avec des
boules supplémentaires, une vitesse +10 % par tour et une teinte décalée. Tous les 5
niveaux, un **code** permet de reprendre plus tard au niveau suivant :

| Niveau de reprise | Code |
|---|---|
| 6 | SAPIN |
| 11 | FLOCON |
| 16 | IGLOO |
| 21 | GIVRE |
| 26 | BANQUISE |
| 31 | AURORE |
| 36 | BLIZZARD |
| 41 | ETOILE |
| 46 | LUGE |
| 51 | TRAINEAU |
| au-delà | NEIGE<n> (ex. NEIGE56) |

## Modes

- **Campagne** (bouton Jouer) : enchaînement des niveaux, record.
- **Mes niveaux** : jouer un niveau perso ou un niveau isolé de la campagne.
- **Test** depuis l'éditeur.

## Direction artistique

Nuit d'hiver : ciel en dégradé dont la teinte varie par niveau, étoiles scintillantes,
aurores boréales animées, lune, montagnes en deux plans, sol enneigé, flocons qui tombent.
Plateformes de glace enneigées avec stalactites. Bonshommes de neige bleu (J1) et rouge
(J2) avec bonnet, écharpe et canon à givre. Boules de Noël brillantes avec attache dorée.
Tout est vectoriel et dessiné en code. Police Fredoka (arrondie).

Musique : petite boucle de 64 pas à 138 BPM (mélodie triangle, basse carrée, charleston)
synthétisée en temps réel ; effets sonores synthétisés eux aussi.
