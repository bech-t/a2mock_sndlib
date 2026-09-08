# Provenance des sources musicales

Les **œuvres** sont dans le domaine public — tous ces compositeurs sont morts
avant 1930. Le **séquençage** d'un MIDI, lui, est le travail de son auteur, et
il est crédité nommément dans le tableau ci-dessous.

Ce que le dépôt fait de ces fichiers :

| | |
|---|---|
| les `.mid` et `.ym` eux-mêmes | **jamais versionnés, jamais redistribués** (cf. `.gitignore`). Ce sont des fichiers tiers ; les recopier ici n'apporterait rien et ce ne sont pas nos droits. |
| les `.A2M` qui en dérivent | **publiés** sur la disquette de démonstration. |

La disquette, elle, est versionnée dans le dépôt (`build/a2mb-test.dsk`) :
c'est ce qui permet de l'écouter sans rien installer.

Ce choix mérite d'être explicité, parce qu'il n'est pas neutre. Un module A2M
n'est pas une copie du MIDI : la réduction à six voies jette tout sauf la
hauteur et la durée — plus de vélocité, plus de pédale, plus d'instrumentation,
un orchestre entier ramené à trois oscillateurs carrés par puce, et le choix
des voix conservées est écrit à la main dans le `Makefile`. Ce qui subsiste de
la séquence d'origine, c'est essentiellement la partition, qui est du domaine
public. Le reste est une transcription.

Cela dit, ces modules **n'existeraient pas** sans le travail des personnes
citées plus bas : le tableau est là pour ça. Si l'une d'elles préfère que sa
transcription n'alimente pas la démo, qu'elle ouvre une issue — le morceau
saute, sans discussion.

`make music` échoue si un fichier manque. Pour les récupérer :

```sh
cd demo/midi
curl -L -o sugarplum.mid    https://www.mfiles.co.uk/downloads/sugar-plum-fairy.mid
curl -L -o bachfugue.mid    https://www.mfiles.co.uk/downloads/bach-fugue-g-minor-organ.mid
curl -L -o entertainer.mid  https://www.mfiles.co.uk/downloads/the-entertainer.mid
curl -L -o arabian.mid      https://www.classicalmidi.co.uk/music2/nutcrkr5a.mid
curl -L -o mountainking.mid https://www.classicalmidi.co.uk/music2/2320tmmntkng.mid
curl -L -o fossiles.mid     https://www.classicalmidi.co.uk/12foss.mid
curl -L -o bumblebee.mid    https://www.classicalmidi.co.uk/music1/1669bee3.mid
curl -L -o cancan.mid       "https://www.classicalmidi.co.uk/music3/celrbratedgalopstevenritchie.mid"
curl -L -o arabesque1.mid   https://www.mutopiaproject.org/ftp/DebussyC/L66/debussy_Arabesque_1/debussy_Arabesque_1.mid
cd ../ym
curl -L -o androids.ym      https://raw.githubusercontent.com/simondotm/ym2149f/master/example/Androids.ym
```

## Détail

| fichier | œuvre | séquencé par |
|---|---|---|
| `bachfugue.mid` | J.-S. Bach, Petite fugue en sol mineur BWV 578 | mfiles.co.uk |
| `mountainking.mid` | Grieg, *Dans l'antre du roi de la montagne* (Peer Gynt, 1875) | Tony Matthews |
| `entertainer.mid` | Scott Joplin, *The Entertainer* (1902) | mfiles.co.uk |
| `cancan.mid` | Offenbach, *Galop infernal* (Orphée aux enfers, 1858) | Steven Ritchie |
| `fossiles.mid` | Saint-Saëns, *Fossiles* (Le Carnaval des animaux, 1886) | classicalmidi.co.uk |
| `bumblebee.mid` | Rimski-Korsakov, *Le Vol du bourdon* (1900) | Carl Bertram |
| `sugarplum.mid` | Tchaïkovski, *Danse de la Fée Dragée* (1892) | mfiles.co.uk |
| `arabian.mid` | Tchaïkovski, *Café / Danse arabe* (1892) | Scott P. Anderson |
| `arabesque1.mid` | Debussy, *Première Arabesque*, L. 66 (1888-91) | Mutopia Project — typographié depuis la partition, **domaine public déclaré** (contrairement aux autres lignes : ici même le séquençage l'est) |

## Le YM

`../ym/androids.ym` — *Androids*, par TAO of ACF (Atari ST). Format YM6!
non compressé, 8 833 trames, 50 Hz, horloge 2 MHz.

**Il n'est plus sur la disquette** : converti en profil R il pèse 12,6 Ko pour
cinquante secondes, et le morceau — un buzzer d'enveloppe du début à la fin —
met mal la carte en valeur. Il reste ici parce qu'il est le seul cas d'essai du
convertisseur `ym2a2m.py`, qui sans lui n'aurait plus aucune vérification.
