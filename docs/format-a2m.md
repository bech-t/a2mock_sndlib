# Le format A2M

Format de musique pour Mockingboard. Un module A2M se charge en mémoire et se
rejoue à cinquante trames par seconde, sans que le 6502 ait à comprendre quoi
que ce soit de la musique — tout le travail difficile est fait à l'hôte, par
un convertisseur.

**Version courante : `A2M\x03`.** Les versions 1 et 2 ne sont plus lues ; le
lecteur refuse tout module dont la magie ne correspond pas exactement, plutôt
que d'interpréter des octets qu'il ne comprend pas.

---

## Deux profils, un conteneur

| | **profil R** — flux de registres | **profil T** — événements |
|---|---|---|
| contenu | l'état des 14 registres, trame par trame | des notes et des instruments |
| convertit | **n'importe quoi** : YM, VTX, dump brut | ce dont on connaît les instruments |
| fidélité | **exacte par construction** | dépend des enveloppes déclarées |
| débit typique | 250-300 o/s | **13-40 o/s** |
| taille du lecteur | ~700 o | ~4 000 o |

Ils partagent l'en-tête, la base de temps et l'API. L'octet 4 dit lequel c'est ;
l'appelant n'a pas à le savoir.

**Quand utiliser lequel** : le profil R ne suppose rien, donc il ne peut pas se
tromper — c'est la seule porte pour une source dont on ignore les instruments
(un dump de registres n'en dit rien). Le profil T est dix fois plus compact,
mais il faut connaître les enveloppes ; depuis un MIDI ou une partition, on les
connaît, puisqu'on les fabrique.

---

## En-tête — 48 octets, communs aux deux profils

| offset | taille | champ | |
|---|---|---|---|
| 0 | 4 | magie | `41 32 4D 03` — `"A2M"` + version |
| 4 | 1 | profil | `'R'` (0x52) ou `'T'` (0x54) |
| 5 | 1 | drapeaux | b0 = reboucler, b1 = module à 2 AY |
| 6 | 2 | latch T1 | valeur à charger dans le timer, petit-boutien |
| 8 | 1 | Hz | 50 ou 60 — informatif, c'est le latch qui commande |
| 9 | 2 | trames | durée totale ; 0 si inconnue |
| 11 | 2 | boucle | **offset absolu** dans le fichier. Profil T seulement ; le profil R reboucle au début des données |
| 13 | 2 | données | offset absolu du corps — vaut 48 aujourd'hui |
| 15 | 1 | n_ay | 1 ou 2 |
| 16 | 16 | titre | ASCII **majuscules**, complété par des espaces, non terminé par zéro |
| 32 | 16 | auteur | idem |

Titre et auteur sont **dans l'en-tête**, pas dans un fichier à côté : le lecteur
les affiche sans ouvrir autre chose, et une disquette de musique sans crédits
serait malpolie envers les compositeurs dont on rejoue le travail.

Ils sont en majuscules parce qu'un Apple II+ n'a aucun glyphe minuscule.

> **Le champ `boucle` est un offset ABSOLU**, comme `données`. Il a compté
> depuis le début du *corps* dans une version antérieure : le lecteur
> rebouclait alors 48 octets trop tôt, en pleine chaîne d'auteur, dont il
> interprétait les caractères ASCII comme des événements. Des notes restaient,
> et le morceau partait en vrille dès la première reprise.

---

## Profil R — flux de registres

Une suite de trames. Chacune décrit **ce qui change**.

```
trame := ctrl(1)
         [ deltas d'amplitude (2) ]   si ctrl & 0x08
         [ masque AY#1 (1-2) ]        si ctrl & 0x01
         [ masque AY#2 (1-2) ]        si ctrl & 0x02
         [ répétition (1) ]           si ctrl & 0x04
         valeurs AY#1... valeurs AY#2...

ctrl = 0xFF  ->  fin du flux
```

### L'octet de contrôle

| bit | |
|---|---|
| 0 | un masque pour l'AY #1 suit |
| 1 | un masque pour l'AY #2 suit |
| 2 | un octet de répétition suit |
| 3 | un bloc de deltas d'amplitude (2 octets) suit |
| 4-7 | réservés, à zéro |

`ctrl = 0x00` seul signifie « rien ne change sur cette trame » et coûte **un
octet**. Avec le bit 2, « rien pendant N trames » en coûte deux.

Les bits 3 et 2 sont exclusifs : une trame porteuse de deltas n'est jamais
répétée.

### Les deltas d'amplitude

Deux octets, petit-boutien, **six codes de 2 bits** — un par voie, dans
l'ordre AY#1 voies A/B/C puis AY#2 voies A/B/C :

| code | |
|---|---|
| `00` | inchangé |
| `01` | −1 |
| `10` | −2 |
| `11` | +1 |

C'est l'optimisation qui porte l'essentiel du gain. Mesure sur la *Fée
Dragée* (2 500 trames, six voies) :

| ce qui change | part des trames |
|---|---|
| rien | 35,9 % |
| **seulement des amplitudes, de −2/−1/+1** | **51,3 %** |
| périodes ou mixer | 12,7 % |

Une trame sur deux ne transporte donc que « telle voie baisse d'un cran ».
Elle coûte **3 octets** au lieu de sept, et ne transporte **aucune valeur** :
c'est le lecteur qui applique le delta et sature entre 0 et 15.

### Les masques

Un ou deux octets, **bit 7 = continuation**. Les bits sont ordonnés du
registre le plus remué au plus rare, pour que la plupart des trames tiennent
sur un seul octet :

```
octet 1, bits 0-6 :  r8  r9  r10  r7  r0  r2  r4     bit 7 = un second octet suit
octet 2, bits 0-6 :  r1  r3  r5   r6  r11 r12 r13
```

Ce n'est pas arbitraire. Les octets **grossiers** de période (r1, r3, r5) ne
changent qu'**une seule fois** dans tout un morceau — la musique tient dans une
octave de périodes — tandis que les amplitudes changent presque à chaque trame.

Les valeurs suivent **dans l'ordre des bits du masque**, celles de l'AY #1
d'abord.

### Pourquoi un masque et pas un simple diff

Le registre **r13 (forme d'enveloppe) n'est pas un état mais un
déclencheur** : y écrire **réarme** l'enveloppe, même avec la même valeur. Un
encodage « écris ce qui a changé » perdrait tous les réarmements et
transformerait une basse percussive en note tenue.

Le masque distingue nativement « r13 vaut toujours 8 » de « on réécrit 8 ».
Le format YM d'Atari a dû résoudre le même problème, et l'a résolu autrement :
chez lui, `r13 = 0xFF` signifie « ne pas écrire ».

---

## Profil T — événements et instruments

```
corps := n_instr(1)  instruments(n × 8)  flux d'événements
```

### Table d'instruments — 8 octets chacun

| octet | champ | |
|---|---|---|
| 0 | crête | amplitude visée à l'attaque, 0-15 |
| 1 | attaque | vitesse de montée ; **0 = instantané** |
| 2 | déclin | vitesse de descente vers `tenue` |
| 3 | tenue | plancher tant que la note dure ; 0 = la note s'éteint seule |
| 4 | chute | vitesse de descente après le NOTE OFF |
| 5 | drapeaux | b0 = **bruit** au lieu du ton |
| 6 | arpège 1 | décalage en demi-tons |
| 7 | arpège 2 | décalage en demi-tons |

Les vitesses sont en **seizièmes de pas d'amplitude par trame** : 16 = un pas
par trame, 8 = un pas toutes les deux trames, 26 ≈ 1,6 pas par trame. Le
lecteur tient l'amplitude en 4.4 (0-240), ce qui réduit chaque trame à une
addition saturée — ni multiplication, ni table.

**Bruit** (drapeau b0) : la hauteur écrite ne choisit plus une note mais la
période du générateur de bruit — grave = sourd, aigu = claquant. Conversion :

```
période = 31 si note < 13 ; 1 si note > 43 ; sinon 44 − note
```

Ce générateur est **unique par puce** : deux voies de bruit sur le même AY se
partagent la dernière période écrite.

**Arpège** : si l'un des deux décalages est non nul, la voie alterne
`note`, `note+arp1`, `note+arp2` **à chaque trame**. À cinquante trames par
seconde, l'oreille entend un accord et non trois notes. Un accord sur une
seule voie, pour le prix d'une réécriture de période par trame.

### Flux d'événements

| octet | | |
|---|---|---|
| `0x00`-`0x05` | NOTE ON voie 0-5 | un octet suit : hauteur 0-95 |
| `0x08`-`0x0D` | NOTE OFF voie 0-5 | |
| `0x10`-`0x15` | INSTRUMENT voie 0-5 | un octet suit : numéro |
| `0x80`-`0xFD` | ATTENDRE (n − 0x7F) trames | 1 à 126 |
| `0xFE` | marque de rebouclage | indicatif — l'en-tête porte l'offset |
| `0xFF` | fin | |

Les voies 0-2 sont sur l'AY #1, les voies 3-5 sur l'AY #2. Comme chaque AY a
sa **propre sortie audio**, ce n'est pas un détail de câblage : c'est un
placement stéréo.

Une hauteur vaut `0` pour do0 et `95` pour si7 — même échelle que la table de
notes du lecteur, calculée pour l'horloge de la Mockingboard (1 020 500 Hz).

### Exemple — les premiers octets de `rythme.A2M`

```
en-tête       "A2M\x03"  T  drapeaux 01  latch 20408  50 Hz  400 trames
              boucle 79   données 48   1 AY   "DEMO RYTHME"  "A2MOCK"

offset 48     03                        trois instruments
offset 49     0F 00 0E 00 11 00 03 07   #0 crête 15, déclin 14, arpège 3/7  (mineur)
offset 57     0E 00 1A 00 1A 00 00 00   #1 crête 14, déclin 26             (basse)
offset 65     0F 00 1E 00 1E 01 00 00   #2 crête 15, déclin 30, BRUIT      (percussion)

offset 73     10 00  11 01  12 02       instruments des voies 0, 1, 2
offset 79     ← point de rebouclage
              00 2D  01 15  02 0C       ON v0 la3, ON v1 la1, ON v2 (bruit) do1
              8B                        attendre 12 trames
              09     02 30              OFF v1, ON v2 do4
```

---

## Ce que le lecteur vérifie, et ce qu'il ne vérifie pas

`a2m_check(mod, len)` valide la magie, le profil, et que le début du corps
tombe dans `len` — le nombre d'octets **réellement chargés**, pas une taille
lue dans le fichier lui-même (un secteur illisible ou une copie interrompue
rendent moins d'octets que prévu, et ce n'est pas au fichier de trancher s'il
ment). `a2m_play_t()`, `a2m_play_r()` et `a2m_play()` reprennent ce même
`len` : chaque lecture du flux, dans les deux moteurs, vérifie qu'elle reste
dans cette limite avant de consommer un octet de plus. Un module tronqué ou
corrompu arrête donc proprement la lecture (silence) au lieu de continuer
dans la mémoire qui suit le tampon — c'était le bug du point de rebouclage,
qui faisait interpréter la chaîne d'auteur comme des événements avant que
cette protection n'existe.

Ce que ça ne fait PAS : valider que le contenu du flux a un SENS au-delà de
ses bornes. Un octet corrompu mais dans les clous continue d'être joué tel
quel — une fausse note, un instrument inattendu, pas un plantage. Et la
protection entière dépend d'un `len` exact : lui passer la taille du tampon
(`A2M_BUFSZ`) au lieu des octets vraiment lus l'annule silencieusement,
puisque le lecteur croirait alors le tampon plein.

---

## Produire du A2M

| source | outil | profil |
|---|---|---|
| partition texte | `a2mconv.py` | T (R avec `--profile R`) |
| MIDI | `midi2a2m.py` | T ou R, 3 ou 6 voix |
| dump YM (Atari, ZX) | `ym2a2m.py` | R obligatoirement |
| par programme | `a2m.encode()` / `encode_t()` | les deux |

Voir [Écrire de la musique en texte](partitions.md) pour le format de
partition, qui est de loin le plus économique : quelques centaines d'octets
pour un morceau entier, et ça s'édite.
