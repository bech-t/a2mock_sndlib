# Écrire de la musique en texte

C'est la façon la plus économique de mettre de la musique sur une disquette
Apple II, et de loin. Un morceau entier tient dans quelques centaines
d'octets :

| morceau | fichier `.txt` | module `.A2M` | durée |
|---|---|---|---|
| Menuet en sol | 626 o | **235 o** | 11 s |
| Ode à la joie | 627 o | **250 o** | 16 s |
| Für Elise | 660 o | **268 o** | 13 s |
| Café arabe (3 voix) | 2 302 o | **902 o** | 56 s |
| Fée Dragée (3 voix) | 2 748 o | **966 o** | 39 s |

À titre de comparaison, la même Fée Dragée en réduction d'orchestre à six voix
pèse 4 857 octets, et un dump YM d'Atari 12 657 pour cinquante secondes.

Trois raisons à cet écart :

- **trois voix, une seule puce** — l'AY #2 reste libre pour les bruitages,
  ce qui est la configuration d'un jeu ;
- **profil T** — la partition déclare son enveloppe, le lecteur la fabrique ;
  rien à transmettre d'autre que les notes ;
- **ça s'édite** — une fausse note se corrige dans un fichier texte, pas dans
  du code.

```sh
python3 tools/a2mconv/a2mconv.py ma_partition.txt -o SORTIE.A2M
```

Ou simplement `make music` : toute partition déposée dans
`demo/scores/` est convertie, et le nom du module suit le nom du
fichier, en majuscules.

---

## 1. Structure d'un fichier

```
# Tout ce qui suit un « # » est un commentaire.
title: MENUET EN SOL
author: PETZOLD
tempo: 132
hz: 50
loop: yes

voice A: pluck
voice B: pluck
voice C: bass

A: d5/4 g4/8 a4/8 b4/8 c5/8
A: d5/4 g4/4 g4/4
B: r/2. b4/4 d4/4 d4/4
C: g2/2. g2/4 b2/4 g2/4
```

Un en-tête de réglages, puis trois voix. Rien d'autre.

**Attention au `#`** : il sert à la fois de dièse et de marque de commentaire.
Il n'ouvre un commentaire qu'en **début de ligne** ou **précédé d'une espace** —
`d#5/16` reste donc bien un ré dièse. (Ce n'était pas le cas au début : `d#5`
était tronqué en `d`, et le convertisseur refusait la partition.)

### Les clefs de l'en-tête

| clef | valeur | défaut | effet |
|---|---|---|---|
| `title` | ≤ 16 caractères | vide | affiché par le lecteur |
| `author` | ≤ 16 caractères | vide | affiché par le lecteur |
| `tempo` | noires par minute | 120 | vitesse |
| `hz` | 50 ou 60 | 50 | cadence du tick |
| `loop` | `yes` / `no` | yes | reboucler à la fin |
| `voice A:` `voice B:` `voice C:` | nom d'instrument | `pluck`, `pluck`, `bass` | le timbre de la voix |

**Les mots-clefs et les noms d'instruments sont en anglais**, la documentation
en français : ce sont des mots du *format*, au même titre que les noms de
notes. Les commentaires de vos partitions, eux, font ce qu'ils veulent.

Titre et auteur sont **mis en majuscules** : un Apple II+ n'a aucun glyphe
minuscule, un titre en bas de casse y serait illisible.

`titre` et `auteur` vivent dans l'en-tête du module, pas dans un fichier à
côté. Une disquette de musique sans crédits serait malpolie envers les
compositeurs dont on rejoue le travail.

---

## 2. Écrire les notes

Une voix s'écrit sur autant de lignes qu'on veut, toutes préfixées par sa
lettre. Les lignes se concatènent — découpez par mesure, c'est plus lisible :

```
A: d5/4 g4/8 a4/8 b4/8 c5/8
A: d5/4 g4/4 g4/4
```

### Anatomie d'une note

```
    f#5/8.
    │││ ││
    │││ │└─ pointée : durée × 1,5   (facultatif)
    │││ └── dénominateur de la durée
    ││└──── octave, 0 à 7
    │└───── altération : # dièse, b bémol   (facultatif)
    └────── nom de note : c d e f g a b
```

`db4` et `c#4` désignent la même hauteur — les enharmonies sont équivalentes,
écrivez celle qui rend la partition lisible.

### Silences

`r/4` ou `-/4`. Les deux écritures sont acceptées.

### Durées

Le dénominateur est celui d'une **ronde**.

| écriture | valeur | à 120 bpm, 50 Hz |
|---|---|---|
| `/1` | ronde | 100 trames (2 s) |
| `/2` | blanche | 50 |
| `/4` | noire | 25 |
| `/4.` | noire pointée | 38 |
| `/8` | croche | 12 |
| `/16` | double croche | 6 |
| `/32` | triple croche | 3 |

**Le dénominateur est un nombre quelconque**, ce qui donne les triolets sans
notation particulière : `/6` vaut un tiers de blanche, donc un triolet de
noires ; `/12` un triolet de croches.

```
A: c4/6 d4/6 e4/6        # un triolet de noires
```

La durée en trames est **arrondie** : à 120 bpm, une croche fait 12,5 trames
et devient 12. L'écart se voit sur un morceau long — le tempo réel dérive un
peu. Choisissez un tempo qui tombe rond si ça compte : à 50 Hz, un tempo de
120 donne une ronde de 100 trames, un tempo de 150 en donne 80.

### Étendue

De `c0` à `b7`, soit huit octaves.

**Au-dessus de `c6`, l'AY sonne faux** — et ce n'est pas rattrapable. Sa
période de ton tient sur 12 bits ; plus la note est aiguë, plus le pas de
quantification est gros en proportion :

| octave | écart maximal | | octave | écart maximal |
|---|---|---|---|---|
| 0 à 3 | moins de 2,3 cents | | 5 | **+11,5 cents** |
| 4 | −4,8 cents | | 6 | **+17,9 cents** |
| | | | 7 | **−35,9 cents** |

L'oreille décroche vers 5 cents. Les octaves 0 à 4 sont donc justes, la 5 est
limite, et les octaves 6 et 7 sont **audiblement fausses** — 36 cents, c'est
plus d'un tiers de demi-ton. C'est le matériel, pas la table de notes.

---

## 3. Les instruments

L'AY n'a **qu'une** enveloppe matérielle par puce, partagée par ses trois
voies : elle ne peut donc pas donner à chaque voix son propre déclin. Les
enveloppes sont donc fabriquées **en logiciel**, par le lecteur, à partir d'un
modèle ADSR à pas entiers — les vitesses sont en seizièmes de pas d'amplitude
par trame.

### Tons simples

| nom | crête | attaque | déclin | tenue | chute | caractère |
|---|---|---|---|---|---|---|
| `pluck` | 15 | immédiate | 1,06 pas/trame | 0 | 1,06 | célesta, boîte à musique |
| `bass` | 14 | immédiate | 1,62 pas/trame | 0 | 1,62 | pizzicato, plus sec encore |
| `sustain` | 14 | ~5 trames | 0,12 pas/trame | 8 | 4 pas/trame | tenue, avec une fin nette |
| `soft` | 13 | 13 trames | aucun | 13 | 4 pas/trame | cordes, nappes |

`pluck` et `bass` **s'éteignent seuls** : la note meurt avant la fin de sa
durée écrite si celle-ci est longue. C'est ce qui donne le grain « boîte à
musique » plutôt qu'un orgue.

`sustain` et `soft` **tiennent** jusqu'au silence suivant. Écrivez donc vos
silences.

### Accords sur une seule voie — l'arpège

| nom | intervalles | donne |
|---|---|---|
| `major` | 0, 4, 7 | accord majeur |
| `minor` | 0, 3, 7 | accord mineur |
| `seventh` | 0, 4, 10 | septième |
| `fifth` | 0, 7, 12 | quinte + octave |
| `octave` | 0, 12, 0 | note doublée à l'octave |

La voie alterne les trois hauteurs **à chaque trame** — cinquante fois par
seconde. L'oreille n'entend pas trois notes successives mais un accord. C'est
l'astuce chiptune par excellence, et elle ne coûte qu'une réécriture de
période par trame.

```
voice A: minor
A: a3/4 a3/4 f3/4 g3/4      # quatre accords, sur UNE voie
```

Trois voix deviennent ainsi trois accords — ou un accord, une basse et une
batterie, ce qui est déjà un morceau.

### Percussions — le bruit

| nom | caractère |
|---|---|
| `drum` | sec : grosse caisse, caisse claire |
| `cymbal` | long : charleston, cymbale |
| `wind` | souffle tenu : vent, ressac |

Avec un instrument de bruit, **la hauteur écrite ne choisit plus une note mais
le grain** : grave = sourd, aigu = claquant.

```
voice C: drum
C: c1/8 c4/8 a3/8 c4/8      # grosse, charleston, caisse, charleston
```

Repères : `c1` très sourd, `c2` grosse caisse, `a3` caisse claire, `c4` et
au-dessus, charleston.

**Une seule voie de bruit par puce.** Le générateur de bruit de l'AY est
unique et partagé par les trois voies : deux voix de bruit sur le même AY se
disputent la dernière période écrite. Ce n'est pas interdit, mais le résultat
n'est pas celui qu'on croit.

### Nom inconnu

Un nom d'instrument non reconnu déclenche un **avertissement** et un repli sur
`pluck` :

```
  /!\ voix B : enveloppe 'plukc' inconnue, repli sur 'pluck'.
      connues : pluck, bass, sustain, soft, major, minor, seventh, ...
```

(Le repli était silencieux au début — une faute de frappe changeait le son
sans rien dire.)

### Ce qui n'est pas exposé

L'**enveloppe matérielle** de l'AY (ses huit formes, le fameux « buzz bass »)
et les ornements (vibrato, glissando) ne sont pas dans le format. La première
parce qu'elle est partagée par les trois voies d'une puce, ce qui en fait une
ressource à arbitrer plutôt qu'un timbre à choisir. Les seconds parce qu'ils
demandent des paramètres par note, pas par instrument.

Les **digidrums** — les échantillons joués par le registre de volume, comme
sur Atari — resteront hors d'atteinte : ils demandent des écritures
d'amplitude à plusieurs kilohertz quand notre tick en fait 50.

## 4. Deux choses qui surprennent

### Les voix n'ont pas à faire la même longueur

Chacune est indépendante. Une voix plus courte **se tait** à sa fin — un
note-off est ajouté automatiquement. Sans lui, sa dernière note continuerait de
sonner jusqu'au rebouclage : invisible avec `pluck` (elle a déjà décliné),
mais une voix `sustain` tiendrait indéfiniment. Le piège ne se serait révélé
qu'en changeant d'enveloppe.

Cela dit, la durée du morceau est celle de la **voix la plus longue**. Pour un
rebouclage propre, faites finir les trois voix ensemble — au besoin en
complétant par des silences.

### Une note répétée doit être réécrite

```
A: c4/8 c4/8 c4/8        # trois attaques distinctes
A: c4/4.                 # UNE note, trois fois plus longue
```

Le lecteur déclenche une nouvelle attaque à chaque note écrite, même identique
à la précédente. Il n'y a pas de liaison : pour tenir une note, allongez sa
durée.

---

## 5. Vérifier son travail

```sh
python3 tools/a2mconv/a2mconv.py ma_partition.txt -o /tmp/T.A2M
```

La sortie donne la taille, le nombre de trames, la durée, le débit et le
nombre de notes. `--profile R` force l'ancien encodage (flux de registres,
quatre fois plus gros) ; `--report` y ajoute une vérification aller-retour de
l'encodage et l'erreur de justesse la plus grande du morceau.

Pour écouter sans matériel :

```sh
make dsk
tools/emu/mame.sh build/a2mb-test.dsk        # ou applewin.sh
```

---

## 6. Partir d'un MIDI

Écrire à la main depuis une partition papier est fastidieux. `midi2score.py`
extrait une première version depuis un MIDI — les bonnes notes, à corriger
ensuite :

```sh
python3 tools/a2mconv/midi2score.py source.mid -o demo/scores/ma_piece.txt \
    --title "MA PIECE" --author COMPOSITEUR --tempo 104 --bars 32 \
    --voice "8:0" --voice "8:1" --voice "12+13:0:bass"
```

Chaque `--voice` vaut `pistes:rang[:enveloppe]` ; `rang` sert quand une piste
joue des accords — 0 prend la note la plus haute, 1 la suivante. On additionne
des pistes avec `+`.

C'est une **simplification**, et il faut le savoir : trois voix au lieu de tout
l'orchestre, un seul tempo au lieu de la carte des tempos du MIDI, et les
durées ramenées à une grille. Quand une durée ne tombe pas juste, l'outil émet
la plus longue valeur représentable puis un silence : l'**alignement** est
préservé, la note raccourcie. Sur un instrument pincé la différence ne
s'entend pas — elle avait déjà fini de décliner.

Une fois engendrée, la partition est à vous. `make` ne la régénère pas : ce
serait effacer vos retouches.
