# a2mock_sndlib — du son sur Mockingboard, pour de vrai

> **Une note d'honnêteté.** Ce projet est en partie écrit avec l'aide de l'IA.
> Si le sujet vous parle, n'hésitez pas à vous manifester : les vrais
> passionnés restent très largement les bienvenus.
>
> L'IA a permis de faire des outils de conversion en python et de verifier les timings,
> un retravaille pour factoriser et optimiser le code, tout en ajoutant des commentaires,
> pour le rendre plus lisible ... bref c'est criticable ou non, votre choix, c'est le miens en tout cas !

**a2mock_sndlib** est une bibliothèque son pour la carte **Mockingboard** de
l'Apple II — deux AY-3-8910 pilotés par deux 6522, six voies, deux sorties
audio. Elle vise un Apple II d'époque : **6502 strict**, 64 Ko, ProDOS.

## État

**Utilisable, et éprouvé sur du vrai matériel.** La bibliothèque et son lecteur
tournent, et tout ce qui est décrit ici a été vérifié sur une **Mockingboard
physique** dans un Apple //e — pas seulement sous émulateur. La disquette de
démonstration joue dix morceaux, dont six à six voies sur les deux puces.

Ce qu'il faut savoir avant de s'en servir :

- **L'API peut encore bouger.** Rien n'est figé tant qu'il n'y a pas de 1.0.
- Le lecteur **ne valide pas** le module au-delà de sa signature : un fichier
  tronqué le fait lire au hasard. Bornez vos entrées ou chargez des modules
  dont vous êtes sûr.
- Le chemin **Apple II+** (base de temps sans //e) est écrit mais n'a jamais
  été exercé sur machine réelle.
- Pas d'intégration continue : les 22 vérifications de `test/` se lancent à la
  main avec `make hosttest`.

Les retours de gens qui ont une carte sont exactement ce qui manque.

## Trois pièces

| | |
|---|---|
| **`liba2mb`** | La couche de base : détecter la carte, écrire dans les AY, une base de temps régulière (IRQ 6522, ou polling), des effets sonores. Un projet qui ne veut que des bruitages ne linke que ça. |
| **`liba2m`** | Le lecteur de musique, posé sur `liba2mb`. Deux profils : un **flux de registres** (convertit tout, exact par construction) et un profil **événements** (notes et instruments : quelques centaines d'octets le morceau entier). |
| **`a2mb-test.dsk`** | Une disquette ProDOS bootable qui joue, montre, et sert de banc d'essai pour la carte et la librairie. Elle est [dans le dépôt](build/a2mb-test.dsk). |


## Essayer sans rien compiler

La disquette de démonstration est dans le dépôt :
**[`build/a2mb-test.dsk`](build/a2mb-test.dsk)** — une image ProDOS 140 Ko
bootable, toujours celle produite par le dernier `make dsk`. Sur GitHub, ouvrez
le fichier et cliquez *Download*, ou :

```sh
git clone <ce-depot>          # elle est dedans, rien a compiler
```

Elle se lance telle quelle sous **MAME**, **AppleWin** ou **Virtual ][**, et
s'écrit sur une vraie disquette avec ADTPro pour un Apple II d'époque. Il faut
une Mockingboard : le programme demande le slot au démarrage, il n'y a **pas de
détection automatique** — sonder à l'aveugle un slot inconnu revient à écrire
dans les phases du moteur d'un contrôleur Disk II.

Le reste de `build/` n'est pas versionné, et `make clean` épargne l'image.

## Construire

Prérequis : [cc65](https://cc65.github.io/) (`cl65`), Python 3, Java (pour
AppleCommander, cf. [`tools/ac/`](tools/ac)).

Deux fichiers tiers ne sont **pas** dans le dépôt et ne sont nécessaires qu'à
`make dsk` : `tools/ac/ac.jar` et `tools/prodos/prodos.dsk`. Chacun a son
README qui dit où le prendre, et `make` vous y renvoie s'il manque. `make lib`
et `make hosttest`, eux, marchent sur un clone nu.

```sh
make lib        # la bibliothèque
make scores     # partitions demo/scores/*.txt -> .A2M (aucun telechargement)
make music      # idem + les six morceaux tires d'un MIDI
make dsk        # la disquette de démonstration (build/a2mb-test.dsk)
make hosttest   # tests hôte
```

## Documentation

- [Utiliser la bibliothèque](docs/integration.md) — les trois couches, la
  séquence minimale, et les trois règles qui ne se devinent pas. Un squelette
  de projet qui compile et boote vit dans
  [`examples/skeleton/`](examples/skeleton).
- [Le format A2M](docs/format-a2m.md) — la définition du format binaire :
  en-tête, profil R (flux de registres) et profil T (événements et
  instruments), avec un exemple décodé octet par octet.
- [Écrire de la musique en texte](docs/partitions.md) — le format de partition,
  la façon la plus économique de mettre de la musique sur une disquette :
  quelques centaines d'octets pour un morceau entier, et ça s'édite.

## Licence

MIT — voir [LICENSE](LICENSE).

La licence couvre le code, les outils et la documentation.

Les morceaux de la disquette de démonstration sont des **œuvres du domaine
public** (Bach, Grieg, Joplin, Offenbach, Saint-Saëns, Rimski-Korsakov,
Beethoven, Petzold). Six d'entre eux sont transcrits à partir de séquences MIDI
dont les auteurs sont crédités nommément dans
[`demo/midi/SOURCES.md`](demo/midi/SOURCES.md) ; les fichiers MIDI eux-mêmes ne
sont pas redistribués. Les quatre autres sont écrits directement en
[partition texte](docs/partitions.md) dans [`demo/scores/`](demo/scores).
