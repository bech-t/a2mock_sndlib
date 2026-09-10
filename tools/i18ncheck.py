#!/usr/bin/env python3
"""i18ncheck.py -- verifie que chaque paire T(fr, en) porte les memes
%-specificateurs, dans le meme ordre. Scanner par regex, pas un vrai
parseur C -- il ne verifie NI la qualite de traduction NI la largeur des
mots a colonne fixe (cf. demo/src/lang.h) : un filet mecanique en plus de
la relecture humaine, pas a sa place.

    python3 tools/i18ncheck.py
"""

import re
import sys

FILES = ["demo/src/main.c", "demo/src/tests.c"]

# Repere un appel T( "..." , "..." ) -- chaines C simples, sans concatenation
# adjacente ("..." "..." sur deux lignes) : celles-ci sont volontairement
# ecrites en une seule chaine dans ce fichier pour rester detectables ici.
_STRING = r'"((?:[^"\\]|\\.)*)"'
_CALL = re.compile(r'\bT\(\s*' + _STRING + r'\s*,\s*' + _STRING + r'\s*\)', re.S)

# Specificateur %..X : on ne garde que la lettre de conversion, flags/largeur/
# precision ignores -- seuls le NOMBRE, l'ORDRE et le TYPE de conversion
# doivent concorder entre fr et en.
_SPEC = re.compile(r'%[-+ 0#]*[0-9]*(?:\.[0-9]+)?([a-zA-Z%])')


def specs(s):
    return [m.group(1) for m in _SPEC.finditer(s) if m.group(1) != '%']


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def main():
    errors = 0
    checked = 0
    for path in FILES:
        text = open(path, encoding="utf-8").read()
        for m in _CALL.finditer(text):
            checked += 1
            fr, en = m.group(1), m.group(2)
            sfr, sen = specs(fr), specs(en)
            if sfr != sen:
                errors += 1
                print("%s:%d: %%-specificateurs differents" % (path, line_of(text, m.start())))
                print("  fr %r -> %r" % (fr, sfr))
                print("  en %r -> %r" % (en, sen))

    print("%d paire(s) T() verifiee(s), %d erreur(s)" % (checked, errors))
    if errors:
        sys.exit(1)


if __name__ == "__main__":
    main()
