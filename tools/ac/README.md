# AppleCommander

`ac.jar` **n'est pas versionné** — c'est un outil tiers, ce ne sont pas nos
droits à redistribuer. Le `Makefile` s'en sert pour écrire le programme et les
modules dans l'image ProDOS (`make dsk`).

Prenez le `.jar` de la dernière version sur
<https://github.com/AppleCommander/AppleCommander/releases>
et posez-le **ici**, sous le nom exact `ac.jar` :

```sh
mv ~/Téléchargements/AppleCommander-ac-*.jar tools/ac/ac.jar
java -jar tools/ac/ac.jar        # doit afficher l'aide
```

Il faut un Java installé (`default-jre` suffit). Si vous préférez le garder
ailleurs : `make dsk AC_JAR=/chemin/vers/ac.jar`.
