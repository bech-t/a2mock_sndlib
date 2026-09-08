# Squelette de projet a2mock_sndlib

Un programme minimal qui prend la carte, joue un module A2M sous IRQ, et
déclenche un bruitage par-dessus sans couper la musique.

```sh
make dsk        # -> build/THEME.dsk, bootable
```

Copiez ce dossier ailleurs et ajustez `A2MB` dans le `Makefile`.

```
src/main.c       le programme, commenté
src/sons.c       VOS bruitages — la lib fournit le moteur, pas le catalogue
music/theme.txt  une partition d'exemple
Makefile         binaire + conversion des partitions + disquette ProDOS
```

Ce que le squelette montre :

- prendre la carte dans un slot **donné**, jamais deviné ;
- charger un module depuis la disquette et le valider ;
- démarrer le tick **à la cadence que le module demande**, avec repli
  scrutation si l'IRQ est refusée ;
- faire avancer musique **et** bruitages depuis le même hook ;
- rendre la machine propre en sortant — un handler laissé en place plante la
  machine au programme suivant.

Les règles qui ne se devinent pas — sons fournis par l'application,
propriété des puces, non-réentrance, discipline d'arrêt — sont dans
[docs/integration.md](../../docs/integration.md).
