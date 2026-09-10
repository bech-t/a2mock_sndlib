# Provenance des musiques PT3

Huit morceaux de [Shiru](https://shiru.untergrund.net/), compositeur ZX
Spectrum connu du homebrew et de la demoscene, qui publie explicitement ses
morceaux **originaux** (pas ses covers, pas ses collaborations) sous licence
libre, sur sa propre page :

> *« You can use my original music from this page under
> [CC-BY](https://creativecommons.org/licenses/by/3.0/) license terms. This
> does not apply to the cover versions or collaboration works. »*
> — [shiru.untergrund.net/music.shtml](https://shiru.untergrund.net/music.shtml)

Les huit fichiers ci-dessous sont tous listés « original » sur sa page — pas
des covers, pas des collaborations (contrairement par exemple à `megamix.pt3`,
juste à côté, fait *avec* Alone Coder, et donc volontairement absent d'ici).

## Pourquoi ce fichier existe séparément de `test/pt3_corpus/SOURCES.md`

Les mêmes huit `.pt3` sont *aussi* dans `test/pt3_corpus/`, avec son propre
`SOURCES.md` presque identique — duplication assumée, pas un oubli.
`test/pt3_corpus/` sert à éprouver le **convertisseur**
(`tools/a2mconv/pt3.py`) : il ne doit pas bouger si la démo change un jour sa
sélection de morceaux ou leur troncature. `demo/pt3/` sert la **vitrine** :
c'est la même séparation déjà en place entre `demo/midi/` et tout ce qui
teste `midi2a2m.py`.

## Ce que le dépôt fait de ces fichiers

| | |
|---|---|
| les `.pt3` eux-mêmes | **jamais versionnés, jamais redistribués** (cf. `.gitignore`). Fichiers tiers ; les recopier ici n'apporterait rien et ce ne sont pas nos droits. |
| les `.A2M` qui en dérivent | **publiés** sur la disquette de démonstration, dans `PT3/`. |

`make music`/`make dsk` échouent bruyamment si un fichier manque — pas de
saut silencieux. Pour les récupérer :

```sh
cd demo/pt3
curl -L -o mehalanholia.pt3  https://shiru.untergrund.net/files/mus/ay/original/mehalanholia.pt3
curl -L -o 199Xnostalgy.pt3  https://shiru.untergrund.net/files/mus/ay/original/199Xnostalgy.pt3
curl -L -o moonlight.pt3     https://shiru.untergrund.net/files/mus/ay/original/moonlight.pt3
curl -L -o oldlove.pt3       https://shiru.untergrund.net/files/mus/ay/original/oldlove.pt3
curl -L -o summer.pt3        https://shiru.untergrund.net/files/mus/ay/original/summer.pt3
curl -L -o chinesewatch.pt3  https://shiru.untergrund.net/files/mus/ay/original/chinesewatch.pt3
curl -L -o hard.pt3          https://shiru.untergrund.net/files/mus/ay/original/hard.pt3
curl -L -o kakvsegda.pt3     https://shiru.untergrund.net/files/mus/ay/original/kakvsegda.pt3
```

## Détail

Les durées sont volontairement **tronquées** (le format budget disque, pas
le morceau) : chaque troncature respecte le point de bouclage naturel
mesuré par `pt32a2m.py --report` (couper avant produirait une boucle d'une
seule trame — pas fausse, juste pas musicale). `hard.pt3` porte en interne
un titre-blague en translittération, pas un titre présentable : affiché
comme `HARD` (son nom sur disque) plutôt que traduit.

| fichier | nom sur disque | titre affiché | durée jouée | bouclage naturel |
|---|---|---|---|---|
| `mehalanholia.pt3` | `PT3/MEHALAN.A2M` | MEHALONHOLIA | 9 s | dès le début |
| `oldlove.pt3` | `PT3/OLDLOVE.A2M` | OLD LOVE | 30 s | 4,1 s — 0 effet rencontré, bon cas de base |
| `moonlight.pt3` | `PT3/MOONLIGHT.A2M` | MOONLIGHT | 23 s | 21,4 s |
| `199Xnostalgy.pt3` | `PT3/NOSTALGY.A2M` | 199X NOSTALGY | 12 s | dès le début |
| `hard.pt3` | `PT3/HARD.A2M` | HARD | 20 s | 17,9 s |
| `kakvsegda.pt3` | `PT3/KAKVSEGDA.A2M` | KAK VSEGDA... | 14 s | 11,9 s |
| `chinesewatch.pt3` | `PT3/CHINWATCH.A2M` | CHINESE WATCH | 15 s | 13,2 s — le plus dense en effets du lot |
| `summer.pt3` | `PT3/SUMMER.A2M` | SUMMER | 10 s | dès le début |

Le budget disque est serré (**4,6 Ko / 9 blocs libres** une fois tout
copié, cf. `make dsk`) : ProDOS ajoute un bloc d'INDEX par fichier de plus
de 512 o, en plus des blocs de données — à compter avant de gonfler une
durée ci-dessus.

Convertis en profil R (`pt32a2m.py` ne produit que ça) : samples, arpège
d'ornement et la plupart des effets PT3 sont interprétés — seul le
glissando d'enveloppe (`$08`) est encore compté sans être appliqué. Voir
`docs/format-a2m.md` et `spec.md` §5.7 pour l'état exact du convertisseur.
