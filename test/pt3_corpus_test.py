#!/usr/bin/env python3
"""pt3_corpus_test.py -- fait tourner tools/a2mconv/pt3.py sur de VRAIS
fichiers .pt3 (cf. test/pt3_corpus/SOURCES.md) : contrairement a
pt3_test.py (fixtures maison, auto-coherentes), ceci est la verification
contre le monde exterieur que spec.md §5.7 demandait.

Best-effort : n'echoue pas si le dossier est vide (le corpus n'est pas
versionne, cf. .gitignore), se contente d'un avertissement. Reseau requis
une seule fois, pour recuperer les fichiers (voir SOURCES.md) -- ce script
ne telecharge rien lui-meme.

    python3 test/pt3_corpus_test.py
"""

import glob
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "a2mconv"))
import pt3

CORPUS_DIR = os.path.join(os.path.dirname(__file__), "pt3_corpus")


def main():
    files = sorted(glob.glob(os.path.join(CORPUS_DIR, "*.pt3")))
    if not files:
        print("corpus vide (%s) -- voir test/pt3_corpus/SOURCES.md pour le"
              " remplir. Rien a verifier, ce n'est pas un echec." % CORPUS_DIR)
        return

    n_ok = 0
    for path in files:
        name = os.path.basename(path)
        try:
            mod = pt3.read(path)
            frames, loop_frame = pt3.to_apple(mod)

            notes = set()
            for pat in mod["patterns"].values():
                for c in "ABC":
                    for ev in pat[c]:
                        if "note" in ev:
                            notes.add(ev["note"])
            note_table = pt3._note_table(mod["freq_table"], mod["version"])
            anchor, confiance = pt3.table_anchor(note_table)
            pire = max((pt3.cents_error(i, note_table[i], anchor) for i in notes),
                       key=abs, default=0.0)
            flag = "" if confiance < 5 else " (ancrage incertain, %+.0fc)" % confiance

            print("%-18s OK   %5d trames  %3d motifs  %3d effets  "
                  "%d non appliques  pire justesse %+.1f c%s" % (
                      name, len(frames), len(mod["order"]),
                      mod["n_effects"], mod["n_effects_unapplied"], pire, flag))
            n_ok += 1
        except Exception as e:
            print("%-18s ECHEC  %r" % (name, e))

    print("\n%d/%d fichiers du corpus lus sans erreur." % (n_ok, len(files)))
    if n_ok < len(files):
        sys.exit(1)


if __name__ == "__main__":
    main()
