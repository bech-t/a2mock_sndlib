# Gabarit ProDOS

`prodos.dsk` **n'est pas versionné** : c'est le système d'Apple, pas notre
code. Le `Makefile` en fait une copie, la reformate au nom `/A2MB/` puis y
écrit la démo — c'est ce qui rend la disquette **amorçable**.

Il faut une image **140 Ko amorçable** au format `.dsk` (ordre DOS). ProDOS 2.4
se télécharge sur <https://prodos8.com/>. Posez l'image **ici**, sous le nom
exact `prodos.dsk` :

```sh
mv ~/Téléchargements/prodos-2.4.3.dsk tools/prodos/prodos.dsk
ls -l tools/prodos/prodos.dsk    # doit faire 143 360 octets
```

Si vous préférez la garder ailleurs :
`make dsk PRODOS_TPL=/chemin/vers/prodos.dsk`.

## `loader.system`

Ce petit binaire de 459 octets **est** versionné, lui : c'est le lanceur
`.SYSTEM` que ProDOS exécute au démarrage. Il affiche `Loading ...`, charge
`MBDEMO` et lui saute dedans ; en cas de pépin il dit `File Not Found` ou
`Error $xx` et attend une touche.

ProDOS ne démarre que le **premier** fichier `.SYSTEM` de la disquette : sans
ce lanceur, l'image serait amorçable mais ne mènerait nulle part.
