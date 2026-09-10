# Corpus de test PT3 -- provenance

spec.md §5.7 posait le problème : la plupart des `.pt3` en circulation
n'ont pas de licence exploitable, contrairement au MIDI où l'on s'appuie
sur des compositions du domaine public. Solution trouvée le 2026-09-10 :
[Shiru](https://shiru.untergrund.net/), compositeur ZX Spectrum connu du
homebrew et de la demoscene, publie explicitement ses morceaux **originaux**
(pas ses covers ni ses collaborations) sous licence libre, sur sa propre
page :

> *"You can use my original music from this page under
> [CC-BY](https://creativecommons.org/licenses/by/3.0/) license terms.
> This does not apply to the cover versions or collaboration works."*
> — [shiru.untergrund.net/music.shtml](https://shiru.untergrund.net/music.shtml)

Ce que le dépôt fait de ces fichiers, comme pour `demo/midi/` : les `.pt3`
**ne sont jamais versionnés** (cf. `.gitignore`) -- ce sont des fichiers
tiers, à télécharger soi-même. Ils servent ici de **corpus de test pour le
convertisseur** (`tools/a2mconv/pt3.py`), pas de matériau de démo.

```sh
cd test/pt3_corpus
curl -L -O https://shiru.untergrund.net/files/mus/ay/original/mehalanholia.pt3
curl -L -O https://shiru.untergrund.net/files/mus/ay/original/oldlove.pt3
curl -L -O https://shiru.untergrund.net/files/mus/ay/original/moonlight.pt3
curl -L -O https://shiru.untergrund.net/files/mus/ay/original/199Xnostalgy.pt3
curl -L -O https://shiru.untergrund.net/files/mus/ay/original/hard.pt3
curl -L -O https://shiru.untergrund.net/files/mus/ay/original/kakvsegda.pt3
curl -L -O https://shiru.untergrund.net/files/mus/ay/original/chinesewatch.pt3
curl -L -O https://shiru.untergrund.net/files/mus/ay/original/summer.pt3
```

`make pt3corpus` (ou `python3 test/pt3_corpus_test.py`) lit tout ce qui est
présent dans ce dossier et rapporte, par fichier : succès/échec du parseur,
nombre d'effets rencontrés (et donc ignorés, §5.7 étape 3), pire écart de
justesse. Il **ne fait rien** (juste un avertissement) si le dossier est
vide -- contrairement à `make pt3test`, qui reste la seule vérification
obligatoire puisqu'elle ne dépend pas du réseau.

## Détail

| fichier | titre | remarque |
|---|---|---|
| `mehalanholia.pt3` | Mehalonholia | "my first PT3 track" -- le plus simple, bon premier test |
| `oldlove.pt3` | Old Love | 0 effet dans la version testée -- bon cas de base |
| `moonlight.pt3` | Moonlight | |
| `199Xnostalgy.pt3` | 199x Nostalgy | dense en effets (49 rencontrés) |
| `hard.pt3` | *(titre en russe/cyrillique)* | dense en effets (43) |
| `kakvsegda.pt3` | Kak Vsegda... | |
| `chinesewatch.pt3` | Chinese Watch | le plus dense en effets testé (91) |
| `summer.pt3` | Summer | |

Toutes composées par Shiru, `original` (ni cover ni collaboration) --
c'est cette catégorie, précisément, que le CC-BY couvre sur sa page.
`megamix.pt3`, listé juste à côté sur son site, est explicitement une
collaboration ("made with Alone Coder") et donc **hors** de cette licence :
volontairement absent d'ici.

**Ce que ce corpus a déjà permis de trouver** (spec.md §5.7) : la première
version du parseur, écrite sur la seule foi de `README_pt3.txt`, échouait
sur les 7 des 8 fichiers ci-dessus (`oldlove.pt3` passait par chance). La
grammaire réelle d'une ligne PT3 -- où les paramètres d'un effet se lisent
APRÈS le déclencheur de note, pas juste après son opcode -- a été confirmée
en comparant au portage C# de `PT3Play.cs` (Sergey Bulba, l'auteur du
format), pas seulement à la documentation en prose. Les 8 passent
maintenant, aller-retour identique compris.
