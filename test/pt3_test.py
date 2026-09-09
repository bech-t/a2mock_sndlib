#!/usr/bin/env python3
"""pt3_test.py -- verifie tools/a2mconv/pt3.py contre les fixtures maison de
pt3_fixtures.py (spec.md §5.7, point 1).

IMPORTANT : ceci est un test d'AUTO-COHERENCE. Les fixtures sont ecrites a
la main a partir du meme document de reference que le parseur ; un test qui
passe ici prouve que le code fait ce qu'on a compris du format, pas qu'il
lit correctement un vrai fichier PT3. Aucun corpus reel n'etait disponible
sous licence claire au moment d'ecrire ceci (spec.md §5.7).

    python3 test/pt3_test.py
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "a2mconv"))
import pt3
import pt3_fixtures as fx

FAILURES = []


def check(name, cond, detail=""):
    if not cond:
        FAILURES.append(name)
    print("  [%s] %s%s" % ("OK" if cond else "FAIL", name,
                            ("  (%s)" % detail) if detail and not cond else ""))
    return cond


def _replay(frames):
    """Reconstruit l'etat COMPLET des 14 registres a chaque trame, a partir
    du flux de deltas -- comme le ferait le lecteur 6502."""
    st = [0] * 14
    out = []
    for fr in frames:
        for r, v in fr.items():
            st[r] = v
        out.append(list(st))
    return out


def test_scale():
    print("scale() -- une voie, huit notes, volume decroissant")
    mod = pt3.parse(fx.scale())
    check("nom", mod["name"] == "GAMME")
    check("auteur", mod["author"] == "TEST")
    check("ordre", mod["order"] == [0])

    frames, loop_frame = pt3.to_apple(mod)
    check("24 trames (8 lignes x vitesse 3)", len(frames) == 24, "obtenu %d" % len(frames))
    check("pas de bouclage explicite -> loop_frame = 0", loop_frame == 0)

    states = _replay(frames)
    exp0 = pt3._period_apple(24)
    per0 = (states[0][1] << 8) | states[0][0]
    check("periode ligne 0 (note 24)", per0 == exp0, "attendu %d obtenu %d" % (exp0, per0))
    check("volume voie A = 15 en ligne 0", states[0][8] == 15)
    check("voies B/C silencieuses", states[0][9] == 0 and states[0][10] == 0)
    check("mixer : ton A actif (bit0 a 0)", states[0][7] & 1 == 0)

    exp1 = pt3._period_apple(25)
    per1 = (states[3][1] << 8) | states[3][0]
    check("periode ligne 1 (note 25, trame 3)", per1 == exp1)
    check("volume decroit a 14 en ligne 1", states[3][8] == 14)


def test_arpeggio():
    print("arpeggio() -- ornement 0/4/7, une note tenue huit lignes")
    mod = pt3.parse(fx.arpeggio())
    frames, _ = pt3.to_apple(mod)
    check("24 trames", len(frames) == 24)

    states = _replay(frames)
    got = [(s[1] << 8) | s[0] for s in states]
    expected_notes = ([36, 40, 43] * 8)[:len(frames)]
    expected = [pt3._period_apple(n) for n in expected_notes]
    check("arpege 0/+4/+7 rejoue TRAME PAR TRAME (pas ligne par ligne)",
          got == expected,
          "premier ecart a l'indice %d" % next(
              (i for i, (a, b) in enumerate(zip(got, expected)) if a != b), -1))


def test_skip():
    print("skip_lines() -- note + $B1 (sauter 3 lignes)")
    mod = pt3.parse(fx.skip_lines())
    frames, _ = pt3.to_apple(mod)
    check("15 trames (5 lignes x vitesse 3)", len(frames) == 15, "obtenu %d" % len(frames))

    states = _replay(frames)
    exp_per = pt3._period_apple(40)
    held = all((s[1] << 8) | s[0] == exp_per and s[8] == 10 for s in states)
    check("note et volume tenus sur toute la duree du saut", held)


def test_envelope():
    print("envelope_trigger() -- enveloppe declenchee sur une seule ligne")
    mod = pt3.parse(fx.envelope_trigger())
    frames, _ = pt3.to_apple(mod)

    f0 = frames[0]
    check("r11/r12/r13 presents a la trame de declenchement",
          all(k in f0 for k in (11, 12, 13)))
    exp_per = max(1, min(0xFFFF, round(0x1234 * (pt3.APPLE_CLOCK / pt3.ZX_CLOCK))))
    got_per = (f0.get(12, 0) << 8) | f0.get(11, 0)
    check("periode d'enveloppe reechelonnee (ZX -> Apple, pas transportee brute)",
          got_per == exp_per, "attendu %d obtenu %d" % (exp_per, got_per))
    check("forme d'enveloppe = 8 (round-trip de l'etype passe a la fixture)",
          f0.get(13) == 8, "obtenu %r" % f0.get(13))

    f1 = frames[1]
    check("PAS de re-declenchement a la trame suivante (r13 absent, sinon "
          "l'enveloppe se rearmerait a chaque trame)", 13 not in f1)


def test_effect_skipped():
    print("effect_skipped() -- effet consomme, jamais applique en v1")
    mod = pt3.parse(fx.effect_skipped())
    check("un effet compte et signale", mod["n_effects_ignored"] == 1,
          "obtenu %d" % mod["n_effects_ignored"])

    frames, _ = pt3.to_apple(mod)
    check("6 trames (2 lignes x vitesse 3)", len(frames) == 6)
    states = _replay(frames)
    exp0, exp1 = pt3._period_apple(24), pt3._period_apple(28)
    per0 = (states[0][1] << 8) | states[0][0]
    per1 = (states[3][1] << 8) | states[3][0]
    check("note de la ligne 0 correcte malgre l'octet d'effet", per0 == exp0)
    check("ligne 1 NON desynchronisee par l'effet ignore", per1 == exp1)


def test_loop():
    print("two_patterns_loop() -- deux motifs, bouclage sur le second")
    mod = pt3.parse(fx.two_patterns_loop())
    check("ordre = [0, 1]", mod["order"] == [0, 1])
    check("loop_order = 1 (LPosPtr)", mod["loop_order"] == 1)

    frames, loop_frame = pt3.to_apple(mod)
    speed = mod["speed"]
    check("8 lignes au total (4 + 4) x vitesse", len(frames) == 8 * speed,
          "obtenu %d" % len(frames))
    check("loop_frame pointe sur le debut du motif 1",
          loop_frame == 4 * speed, "attendu %d obtenu %d" % (4 * speed, loop_frame))


def main():
    for fn in (test_scale, test_arpeggio, test_skip, test_envelope,
               test_effect_skipped, test_loop):
        fn()
        print()

    if FAILURES:
        print("%d echec(s) : %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("tous les tests d'auto-coherence PT3 passent (ce qui ne veut PAS "
          "dire : lit un vrai fichier PT3 -- voir spec.md §5.7).")


if __name__ == "__main__":
    main()
