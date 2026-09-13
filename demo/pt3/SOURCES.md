# Provenance des musiques PT3

Quatre morceaux de [Shiru](https://shiru.untergrund.net/), compositeur ZX
Spectrum connu du homebrew et de la demoscene, qui publie explicitement ses
morceaux **originaux** (pas ses covers, pas ses collaborations) sous licence
libre, sur sa propre page :

> *« You can use my original music from this page under
> [CC-BY](https://creativecommons.org/licenses/by/3.0/) license terms. This
> does not apply to the cover versions or collaboration works. »*
> — [shiru.untergrund.net/music.shtml](https://shiru.untergrund.net/music.shtml)

Les quatre fichiers ci-dessous sont tous listés « original » sur sa page —
pas des covers, pas des collaborations (contrairement par exemple à
`megamix.pt3`, juste à côté, fait *avec* Alone Coder, et donc volontairement
absent d'ici).

## Quatre, pas huit

Le premier jet en gardait huit, chacun tronqué à quelques secondes pour
tenir sur la disquette. À l'écoute, deux d'entre eux (`mehalanholia.pt3`,
`hard.pt3`) sonnaient faux plus souvent que les six autres — et ce sont
précisément les deux dont `pt3.table_anchor()` signalait déjà un ancrage de
justesse peu fiable (`(ancrage incertain, +13c)`, visible via
`make pt3corpus`) : leur table de fréquence (famille ASM, `freq_table=2`) ne
s'aligne sur aucune grille 12-TET à mieux que 13 cents, contre les tables
« ST » (`freq_table=1`) des six autres, précises à moins d'un cent — cf.
`spec.md` §5.7 pour le détail de cette mesure. Écartés pour cette raison,
pas juste raccourcis : allonger un morceau qui sonne faux ne le rend pas
juste.

`kakvsegda.pt3` et `199Xnostalgy.pt3` sont écartés pour une raison
différente — libérer assez de marge disque pour que les quatre qui restent
jouent **entiers ou presque**, plutôt que huit extraits de dix secondes.
`oldlove.pt3` (le moins cher à l'octet du lot) joue maintenant en entier ;
les trois autres vont aussi loin que le budget le permet, toujours au-delà
de leur point de bouclage naturel.

Les quatre `.pt3` écartés restent de bons candidats si la marge disque
s'agrandit un jour (nouveau gabarit, compression) — rien dans leur contenu
ne les disqualifie, à part la justesse pour les deux premiers.

## Pourquoi ce fichier existe séparément de `test/pt3_corpus/SOURCES.md`

Les huit `.pt3` d'origine (les quatre d'ici, plus les quatre écartés) sont
*aussi* dans `test/pt3_corpus/`, avec son propre `SOURCES.md` — duplication
assumée, pas un oubli. `test/pt3_corpus/` sert à éprouver le
**convertisseur** (`tools/a2mconv/pt3.py`) sur un corpus large : il ne doit
pas bouger si la démo change sa sélection. `demo/pt3/` sert la **vitrine** :
même séparation déjà en place entre `demo/midi/` et tout ce qui teste
`midi2a2m.py`.

## Ce que le dépôt fait de ces fichiers

| | |
|---|---|
| les `.pt3` eux-mêmes | **jamais versionnés, jamais redistribués** (cf. `.gitignore`). Fichiers tiers ; les recopier ici n'apporterait rien et ce ne sont pas nos droits. |
| les `.A2M` qui en dérivent | **publiés** sur la disquette de démonstration, dans `PT3/`. |

`make music`/`make dsk` échouent bruyamment si un fichier manque — pas de
saut silencieux. Pour les récupérer :

```sh
cd demo/pt3
curl -L -o oldlove.pt3       https://shiru.untergrund.net/files/mus/ay/original/oldlove.pt3
curl -L -o moonlight.pt3     https://shiru.untergrund.net/files/mus/ay/original/moonlight.pt3
curl -L -o chinesewatch.pt3  https://shiru.untergrund.net/files/mus/ay/original/chinesewatch.pt3
curl -L -o summer.pt3        https://shiru.untergrund.net/files/mus/ay/original/summer.pt3
```

## Détail

| fichier | nom sur disque | titre affiché | durée jouée | bouclage naturel |
|---|---|---|---|---|
| `oldlove.pt3` | `PT3/OLDLOVE.A2M` | OLD LOVE | **entier** — 80,3 s | 4,1 s — 0 effet rencontré |
| `moonlight.pt3` | `PT3/MOONLIGHT.A2M` | MOONLIGHT | 26 s / 90,3 s | 21,4 s |
| `chinesewatch.pt3` | `PT3/CHINWATCH.A2M` | CHINESE WATCH | 22 s / 89,5 s | 13,2 s — le plus dense en effets du lot |
| `summer.pt3` | `PT3/SUMMER.A2M` | SUMMER | 22 s / 91,9 s | dès le début |

Le budget disque reste la contrainte : ProDOS ajoute un bloc d'INDEX par
fichier de plus de 512 o, en plus des blocs de données — à recompter avant
de gonfler une durée ci-dessus (`make dsk` échoue proprement si ça déborde,
mais autant vérifier avant). Marge actuelle : 6 144 o libres.

Convertis en profil R (`pt32a2m.py` ne produit que ça) : samples, arpège
d'ornement et tous les effets PT3 (glissando, portamento, vibrato, offsets,
vitesse, glissando d'enveloppe `$08`) sont interprétés. Vérifié registre à
registre contre le lecteur PT3 original (l'oracle, `tools/pt3oracle/`), pas
seulement à l'oreille — six bugs d'interprétation trouvés et corrigés les
2026-09-10/13. Reste une exception assumée : r11/r12 (période d'enveloppe)
— la formule de référence pour cette route est elle-même confirmée cassée
(`tools/pt3oracle/README.md`), donc notre implémentation, bien que lue
directement dans le lecteur original, ne peut pas être vérifiée contre
l'oracle sur ces deux registres précis et reste *plausible*, pas *prouvée*.
Voir `docs/format-a2m.md` et `spec.md` §5.7 pour l'état exact du
convertisseur.
