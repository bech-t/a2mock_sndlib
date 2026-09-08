# Utiliser a2mock_sndlib dans votre projet

Un squelette prêt à copier vit dans [`examples/skeleton/`](../examples/skeleton).
Il compile, boote et joue :

```sh
cd examples/skeleton
make dsk
```

Ce document explique ce qu'il fait, et les quatre règles qui ne se devinent pas.

---

## Trois couches, prenez ce dont vous avez besoin

| en-tête | ce que ça donne | linké |
|---|---|---|
| `a2mb.h` | la carte : détection, registres, notes, silence | toujours |
| `a2mb_time.h` | un tick régulier à 50 ou 60 Hz, IRQ ou scrutation | si vous voulez que quelque chose avance seul |
| `a2mb_fx.h` | le **moteur** de bruitages (pas les sons) | si vous en voulez |
| `a2m.h` | le lecteur de modules A2M | si vous voulez de la musique |

### Payez ce que vous nommez

Le lieur cc65 prend les modules **un par un** dans la bibliothèque. Ce n'est
pas une promesse, c'est mesurable au `ld65 -m` :

| ce que le programme appelle | taille | modules liés |
|---|---|---|
| `mb_init` + `mb_reg` | **1 600 o** | `mb_card` `mb_io` |
| + `mbt_start_poll` + bruitages | **3 419 o** | + `mb_fx` `mb_notes` `mb_time` |
| + `mbt_start_irq` | 3 948 o | + `mb_irq` `mb_prodos` `mb_time_irq` |
| + `a2m_play_r` (profil R seul) | **5 062 o** | + `a2m_core` `a2m_r` |
| + `a2m_play_t` (profil T seul) | **6 273 o** | + `a2m_core` `a2m_t` |
| + `a2m_play` (les deux) | 8 418 o | + `a2m_any` `a2m_r` `a2m_t` |

Taille de chaque morceau du lecteur :

| | |
|---|---|
| `a2m_core` — API, état, aiguillage | 631 o |
| `a2m_r` — décodeur de registres | 1 197 o |
| `a2m_t` — moteur à instruments | 2 263 o |
| `a2m_any` — `a2m_play()` | 53 o |

Aucun `#define` n'est nécessaire pour ça, et il n'y en a pas : c'est
l'édition de liens qui trie.

Deux conséquences pratiques :

- le lecteur A2M pèse 4 Ko et **n'est pas lié** si vous ne l'appelez pas ;
- `mbt_start(latch, mode)` référence les **deux** chemins, donc l'appeler
  embarque le handler d'interruption et l'appel ProDOS même si vous demandez
  `MBT_POLL`. Une application qui se contente de la scrutation appelle
  `mbt_start_poll()` directement et économise ~530 octets ;
- de même, `a2m_play()` lit le profil dans l'en-tête et appelle le bon
  moteur — donc elle les **nomme tous les deux**. Si vous ne diffusez que vos
  propres modules, vous connaissez leur profil : `a2m_play_t()` économise
  **2 145 octets**, `a2m_play_r()` en économise **3 356**.

C'est la même règle partout : **on paie ce qu'on nomme**.

### L'instrumentation coûte, elle aussi

Le chronomètre du handler (`mbt_maxdur`, `mbt_over`, `mbt_period`) n'existe que
si la bibliothèque est bâtie avec **`A2MB_DEBUG`** :

```sh
make lib                  # avec    (défaut de ce dépôt : banc de validation)
make lib A2MB_DEBUG=0     # sans    -- 132 octets et ~97 cycles par tick
```

`mbt_ticks` et `mbt_lost` restent **toujours** disponibles : ils ne coûtent
qu'une incrémentation et un test, et `lost` est le seul indicateur qui dise en
exploitation si la machine tient la cadence.

---

## La séquence minimale

```c
#include "a2mb.h"
#include "a2mb_time.h"
#include "a2m.h"

static void tick(void) { a2m_frame(); }   /* ce que le tick appelle */

...
    if (!mb_init(4))                      /* slot DONNÉ, jamais deviné */
        return 1;

    fread(A2M_BUF, 1, A2M_BUFSZ, f);      /* charger le module */
    if (!a2m_check(A2M_BUF))
        return 1;

    mbt_hook(tick);
    mbt_start(a2m_latch(A2M_BUF), MBT_IRQ);   /* cadence du MODULE */
    a2m_play_t(A2M_BUF, 1);                   /* 1 = en boucle */

    /* ... votre programme. La musique avance toute seule. ... */

    a2m_stop();
    mbt_stop();                           /* IMPÉRATIF, voir plus bas */
    mb_shutdown();
```

Cinq lignes utiles. Tout le reste est à vous.

---

## Règle 1 — les sons sont à vous

La bibliothèque fournit un **séquenceur**, pas un catalogue. Un jeu a son
identité sonore ; la figer dans la bibliothèque l'imposerait à tous, et ferait
payer neuf effets à qui n'en veut qu'un.

Un effet est une suite de pas, en données pures :

```c
static const MbFxStep son_choc[] = {
    { MB_FX_KNOCK, AY_AMP_ENV, 1, 3 },   /* bruit + enveloppe percussive */
    { MB_FX_END,   0, 0, 0 }
};

mb_fx_play(son_choc);      /* arme, et rend la main TOUT DE SUITE */
```

| champ | |
|---|---|
| `note` | index de note (0 = do0), ou `MB_FX_NOISE`, `MB_FX_KNOCK`, `MB_FX_END` |
| `amp` | 0-15, ou `AY_AMP_ENV` pour suivre l'enveloppe matérielle |
| `arg` | bruit : période 0-31 · choc : vitesse d'extinction |
| `ticks` | durée du pas — 20 ms à 50 Hz |

`mb_fx_play()` **arme** et rend la main ; c'est le tick qui fait avancer. Sous
IRQ, une fonction bloquante n'aurait aucun sens : appelée depuis le programme
elle le gèlerait, appelée depuis l'interruption elle gèlerait la machine.

La table doit rester valide tant que l'effet joue — en pratique un
`static const`.

Exemples complets : [`demo/src/sons.c`](../demo/src/sons.c) et
[`examples/skeleton/src/sons.c`](../examples/skeleton/src/sons.c).

## Règle 2 — chacun ne réinitialise que ce qu'il possède

## Règle 3 — le cœur d'écriture n'est pas réentrant

`mb_push()` est du **code auto-modifiant** : l'adresse de base du slot est
écrite dans les instructions elles-mêmes. C'est ce qui le rend rapide sans
consommer un octet de page zéro — la cible `apple2` de cc65 n'en expose que
26, tous pris par le runtime C.

Le prix : si le tick tourne sous IRQ et pousse des registres, un appel
concurrent depuis votre programme tombe au milieu.

| | |
|---|---|
| **tick arrêté** | appelez librement |
| **tick en marche** | tout passe par le hook, ou encadrez de `mb_lock()` / `mb_unlock()` |

`mb_lock()` est un `SEI`, `mb_unlock()` un `CLI`. Gardez la section **courte** :
une IRQ manquée est un tick de musique perdu.

C'est aussi pourquoi les bruitages sont **armés** et non joués :
`mb_fx_play()` rend la main immédiatement, le tick fait avancer l'effet. Sous
IRQ, une fonction bloquante n'aurait aucun sens.

---

## Règle 4 — arrêter le tick avant de partir

```c
mbt_stop();
```

**Impératif.** Un handler laissé en place sur un timer qui bat encore plante la
machine dès que le code a disparu — et sous ProDOS, il disparaît au prochain
programme chargé.

Même chose avant de sonder un slot : `mb_probe()` et `mb_init()`
**reprogramment le timer T1** de la VIA #1 — c'est ainsi qu'ils reconnaissent
un 6522. Si un tick tourne, il meurt. Toujours `mbt_stop()` d'abord.

---

## Où mettre le module

`a2m_play()` prend un **pointeur**. `A2M_BUF` n'est qu'une convention — une
adresse en dur et une constante de taille. **Rien n'est déclaré, rien n'est
réservé**, et la bibliothèque ne s'en sert jamais elle-même :

```c
#define A2M_BUF   ((u8 *)0x0800)
#define A2M_BUFSZ 14336
```

C'est à vous de garantir que la zone est libre.

```
$0400 ─────────────────────────  page texte 1
$0800 ─────────────────────────  A2M_BUF_LOW  ┐  6 Ko
$1FFF ─────────────────────────               │
$2000 ─────────────────────────  page HIRES 1 │  A2M_BUF, 14 Ko
$3FFF ─────────────────────────               ┘
$4000 ─────────────────────────  votre programme (--start-addr 0x4000)
$9600 ─────────────────────────  __HIMEM__ par défaut
```

> **`$2000-$3FFF` est la page HIRES 1.** Un programme qui affiche du graphisme
> ne peut **pas** utiliser `A2M_BUF` tel quel : l'image et le module se
> recouvrent. Prenez `A2M_BUF_LOW` (6 Ko), qui tient trois minutes de musique
> en profil T.

| | taille | durée (profil T) | |
|---|---|---|---|
| `A2M_BUF` | 14 Ko | ~7 min | interdit avec la page HIRES 1 |
| `A2M_BUF_LOW` | 6 Ko | ~3 min | compatible HIRES |
| un tableau C | ce que vous voulez | — | **lié dans le binaire** |

Le troisième cas mérite d'être connu : rien n'oblige à charger un fichier.

```c
static const u8 jingle[] = {
    0x41,0x32,0x4D,0x03, 'T', 0x01, /* ... */
};
a2m_play_t(jingle, 0);
```

Un jingle de quelques centaines d'octets se lie directement — pas de fichier
sur la disquette, pas de `fopen`, rien à charger au démarrage. Le convertisseur
peut produire ce tableau : `xxd -i SORTIE.A2M`.

Débits mesurés : **~32 o/s** en profil T, **~250 o/s** en profil R.

## IRQ ou scrutation

```c
if (!mbt_start(latch, MBT_IRQ))
    mbt_start(latch, MBT_POLL);      /* repli */
```

`mbt_start(MBT_IRQ)` échoue franchement s'il n'obtient pas de vecteur — pas de
ProDOS, table d'interruptions pleine. Un refus vaut mieux qu'un tick qui
n'arriverait jamais.

En repli, **c'est votre boucle qui fait avancer le temps** :

```c
if (mbt_mode() == MBT_POLL)
    mbt_poll();
```

Le même code marche dans les deux modes : sous IRQ, cette ligne ne fait rien.
C'est tout l'intérêt d'avoir un hook unique.

---

## Ce qu'on peut surveiller

| | |
|---|---|
| `mbt_ticks` | ticks écoulés — doit monter d'environ 50 par seconde |
| `mbt_lost` | ticks manqués. **0 au repos.** S'il monte pendant un chargement disque, c'est ProDOS qui masque les IRQ — normal. S'il monte en jouant, votre hook est trop long. |
| `mbt_maxdur` | durée du handler la plus longue, **en cycles** |
| `mbt_period` | le budget : 20 408 cycles à 50 Hz |

`mbt_maxdur` est une mesure, pas une estimation : le handler se chronomètre en
lisant un compteur libre. Comparez-le à `mbt_period` — c'est le seul moyen
honnête de savoir si votre hook tient dans son budget.

Repères mesurés : handler nu **1 076 cycles**, lecteur A2M profil T à six voix
**15 400**. Il vous reste donc les trois quarts d'une période à 50 Hz si vous
jouez de la musique, et la quasi-totalité si vous ne faites que des bruitages.

---

## Fabriquer la musique

```sh
python3 tools/a2mconv/a2mconv.py ma_partition.txt -o THEME.A2M
```

Voir [Écrire de la musique en texte](partitions.md) — quelques centaines
d'octets pour un morceau entier, et ça s'édite. Les convertisseurs MIDI et YM
sont décrits dans [Le format A2M](format-a2m.md).
