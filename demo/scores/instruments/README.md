# Un morceau par instrument

Quinze partitions texte, une par instrument du catalogue
(`tools/a2mconv/a2m.py:INSTRUMENTS`), pour les comparer à l'oreille et servir
de point de départ si vous voulez en écrire un nouveau.

| fichier | ce qu'il isole |
|---|---|
| `pluck.txt` `bass.txt` `sustain.txt` `soft.txt` `organ.txt` `piano.txt` `brass.txt` | les 7 timbres — **même mélodie** dans les sept, seul l'instrument change |
| `major.txt` `minor.txt` `seventh.txt` `fifth.txt` `octave.txt` | les 5 accords sur une voix — **même progression** de racines |
| `drum.txt` `cymbal.txt` `wind.txt` | les 3 percussions — même accompagnement, seule la voix C change |

Dans chaque groupe, tout est identique sauf l'instrument testé : enchaînez
les fichiers d'un même groupe pour n'entendre changer qu'une seule variable.

## Les écouter

`make dsk` les convertit et les dépose dans `TIMBRES/`, un sous-dossier
ProDOS de la disquette de démonstration — **menu 6** (`t_instruments()` dans
`demo/src/tests.c`), à côté du 5 (MUSIQUE). Même écran, même clavier
(`1-9,A-F` pour choisir, `P` pause, `X` bruitage par-dessus, `Q` sortir).

Ils sont **séparés** de `demo/scores/` (le `Makefile` ne descend pas dans les
sous-dossiers pour `make music`) : ce sont des outils de comparaison, pas des
morceaux choisis pour la vitrine — la playlist MUSIQUE n'en est pas changée.

Pour tester une modification sans reconstruire toute la disquette, le
squelette d'exemple est plus rapide :

```sh
cp demo/scores/instruments/organ.txt examples/skeleton/music/theme.txt
cd examples/skeleton && make dsk
```

`build/THEME.dsk` boote directement sur ce morceau.
