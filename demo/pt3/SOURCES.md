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
