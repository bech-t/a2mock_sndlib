# L'oracle PT3

`PT3Play.cs` + `AYEmu.cs` sont le lecteur PT3 **original**, écrit par
[Sergey Bulba](mailto:svbulba@gmail.com) (l'auteur du format lui-même,
via `ayfly`), porté en C# par
[benbaker76](https://github.com/benbaker76/PT3Play) — **MIT**, vendorisés
ici tels quels (`LICENSE`), pas juste référencés : le port compile et
tourne sans modification sous .NET 8 / Linux (`dotnet build`), une fois
séparé du reste du dépôt PT3Play (rendu DirectX/SlimDX, Windows only, que
`Program.cs` ici n'appelle jamais).

C'est l'oracle au sens strict de `spec.md` §5.6 : la seule façon de savoir
si `tools/a2mconv/pt3.py` interprète un fichier PT3 **correctement**, pas
seulement sans planter. `test/pt3_test.py` et `test/pt3_corpus_test.py` ne
vérifient que la cohérence interne (round-trip, pas de crash) — c'est ici,
et seulement ici, qu'on compare **registre à registre, trame par trame**, à
un lecteur qui a fait ses preuves.

## Ce que ça a déjà trouvé (2026-09-10 → 09-11)

Six bugs réels dans `pt3.py`, aucun visible à la seule relecture. Une fois
les six corrigés, le flux de registres colle à l'oracle **exactement**,
registre par registre, sur les 800 premières trames des 4 morceaux de la
démo — aucun écart nulle part, en dehors de la route enveloppe (confirmée
cassée côté référence, cf. plus bas — pas un manque de notre côté).

1. **Mixer** — une voie inactive était forcée à "désactivée" ; la référence
   la laisse à "activée" (l'amplitude à 0 suffit à la taire). Écart dès la
   toute première trame comparée.
2. **`$B1` (sauter N lignes), off-by-one** — ajoutait N lignes vides au lieu
   de N-1. `Note_Skip_Counter` est décompté à *chaque* limite de ligne ; la
   ligne suivante n'arrive qu'après N limites franchies, pas N+1.
3. **`$B1`, persistant par voie** — la référence réécrit
   `Note_Skip_Counter = Number_Of_Notes_To_Skip` à la fin de *chaque* ligne
   qui s'exécute, que `$B1` y soit réapparu ou non. Une valeur fixée une
   fois continue donc à s'appliquer à tous les événements suivants, pas
   seulement à celui qui la porte.
4. **`$10-$1F` (enveloppe + échantillon), indice divisé par deux oublié** —
   `$F0-$FF` divisait déjà par 2 l'octet lu (comme la référence), mais
   `$10-$1F`, qui partage le même octet d'indice, ne le faisait pas :
   l'échantillon 6 (46 pas, boucle à 45 — un boîtier "boucle == longueur"
   qui a longtemps masqué le symptôme) était joué à la place du 3 (3 pas,
   boucle à 2) réellement visé. Trouvé en traçant `Sample_Length` d'une
   voie (`PT3DBG=`, ci-dessous) : une longueur qui ne correspond à AUCUN
   échantillon lu côté `pt3.py` trahit une indexation fausse, pas une
   dérive.
5. **Bruit (r6) jamais rééchelonné** — `ym.py` rééchelonne le sien
   (ZX → Apple) depuis le début ; `pt3.py` transportait la période source
   telle quelle. Trouvé sur `summer.pt3` : 44 % des trames comparées
   divergeaient sur ce seul registre, jusqu'à ce qu'on remarque que la
   valeur *brute* de l'oracle correspondait exactement à la nôtre — signe
   qu'un rééchelonnement manquait, pas qu'une valeur était mal calculée.
6. **Glissando (`$01`) jamais appliqué, en silence, depuis le début** — la
   table des noms d'effets porte `"glissando/tone"`, mais l'aiguillage qui
   applique chaque effet testait `"glissando"`. Aucune erreur, aucun
   plantage : l'effet le plus courant du corpus (chinesewatch.pt3 en
   comptait 91) était fidèlement COMPTÉ comme "appliqué" dans `--rapport`
   sans jamais l'être. Trouvé en traçant une glissade visible côté oracle
   (une période qui grimpe puis redescend sur plusieurs trames) contre une
   période plate côté `pt3.py`, sur la même voie, aux mêmes trames.

Et une confirmation plutôt qu'une découverte : la formule de la route
enveloppe dans `PT3Play.cs` (`AY_Sys_GetWord(Module, Env_Base_lo)`) relit
les octets du **fichier PT3 lui-même** comme une période dès que aucune
commande d'enveloppe n'est encore survenue (`Env_Base_lo` vaut alors 0) —
vérifié sur `oldlove.pt3`, trame 0 : l'oracle affiche `0x7250`, exactement
`"Pr"` (début de `"ProTracker 3."`) relu en petit-boutien. Bug confirmé du
portage (ou de sa source), pas une formule à reproduire — d'où le choix,
dans `pt3.py`, de ne pas implémenter cette route. Voir sa docstring et
`spec.md` §5.7 pour le détail.

## Installer .NET (sans root)

```sh
curl -sSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh
chmod +x /tmp/dotnet-install.sh
/tmp/dotnet-install.sh --channel 8.0 --install-dir "$HOME/.dotnet"
```

`run.sh` le trouve automatiquement (`$HOME/.dotnet` ou déjà dans `PATH`).

## Utiliser l'oracle

```sh
tools/pt3oracle/run.sh test/pt3_corpus/oldlove.pt3 800 /tmp/oracle.csv
python3 test/pt3_oracle_compare.py test/pt3_corpus/oldlove.pt3 /tmp/oracle.csv
```

`Program.cs` initialise l'oracle sur le fichier donné (`PT3_Init`), joue N
trames (`PT3_Play` — pas `MusicPlay`/`EmulateSample`, qu'on n'appelle
jamais : aucun rendu audio n'est nécessaire, seul l'état des 14 registres
après chaque trame compte), et écrit un CSV `trame,r0..r13`. Il ne gère
qu'UN AY (`Is2AY=False` toujours ici) — nos quatre morceaux de démo n'en
utilisent qu'un ; un module TurboSound demanderait d'étendre `Program.cs`.

**Pour localiser un écart** (pas juste le constater) : `PT3DBG=A` (ou `B`,
`C`) trace sur stderr l'état interne complet d'une voie, trame par trame
(note, position d'échantillon **et sa longueur/boucle déclarées**, position
d'ornement, accumulateur de ton, adresse dans le motif, compteur de saut) —
`PT3DBGN` borne le nombre de trames tracées (défaut 30) :

```sh
PT3DBG=A PT3DBGN=15 tools/pt3oracle/run.sh test/pt3_corpus/summer.pt3 15 /tmp/x.csv
```

C'est cette trace, comparée à un `print()` équivalent ajouté temporairement
dans `to_apple()` côté `pt3.py`, qui a permis de localiser le 4ᵉ bug
(ci-dessous) : une longueur d'échantillon qui ne correspond à RIEN de ce
que `pt3.py` avait lu est le signe le plus direct d'une erreur d'indexation,
pas d'une dérive progressive.

`test/pt3_oracle_compare.py` rejoue le même fichier avec `pt3.py`,
reconstruit l'état complet des registres à partir du flux de deltas (comme
`module_screen()` le ferait), rééchelonne les périodes de l'oracle
(ZX → Apple, la même fonction que `to_apple()`) et compare registre par
registre. Un registre qui diverge sur *une poignée* de trames trahit un
résidu isolé ; un registre qui diverge *systématiquement* trahit un vrai
bug d'interprétation — c'est cette distinction qui a permis de trouver les
trois listés ci-dessus.

## Ce que ce n'est PAS

Ni un lecteur audio, ni un outil de production — juste un instrument de
vérification, à charger une fois par investigation, pas à chaque
`make dsk`. Il n'existe aucun target Make pour lui : `dotnet` n'est pas une
dépendance du projet, seulement de cet outil de mise au point ponctuel.
