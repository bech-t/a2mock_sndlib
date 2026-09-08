/* a2mb_time.h -- base de temps : un tick regulier a 50 ou 60 Hz.
 *
 * Source : le timer T1 de la VIA #1 en free-run. Deux facons de le consommer,
 * MEME hook dans les deux cas -- c'est le point de toute cette couche.
 *
 * ---------------------------------------------------------------------------
 * POURQUOI DEUX MODES
 *
 * MBT_POLL : on SCRUTE le drapeau T1, aucune interruption, aucun vecteur
 *   touche. Zero risque de conflit avec ProDOS ou avec la commutation main/aux
 *   du 80 colonnes. Mais le tick n'avance que quand l'appelant appelle : une
 *   musique se fige des que le programme fait autre chose qu'attendre. Cela
 *   convient a un livre-jeu (la machine attend le joueur 95 % du temps) ; pas
 *   a un lecteur de modules.
 *
 * MBT_IRQ : le tick arrive tout seul, quoi que fasse le programme. C'est ce
 *   qu'il faut pour de la musique. Le prix est reel -- vecteur d'interruption,
 *   etat de commutation memoire a preserver, ProDOS a prevenir -- et il est
 *   paye dans mb_irq.s.
 *
 * Un programme ecrit pour l'un passe a l'autre en changeant UN argument.
 * ---------------------------------------------------------------------------
 */
#ifndef A2MB_TIME_H
#define A2MB_TIME_H

#include "a2mb.h"

#define MBT_POLL  0
#define MBT_IRQ   1

/* Frequences usuelles. Un module porte la sienne dans son en-tete : un morceau
 * Atari a 50 Hz et un morceau NTSC a 60 Hz ne demandent pas le meme latch. */
#define MBT_HZ_50  50
#define MBT_HZ_60  60

/* Valeur a charger dans T1 pour `hz` ticks par seconde.
 *
 * Horloge du 6522 en slot : 1 020 500 Hz (moyenne Apple II, cycle long
 * compris) -- et non 1 023 000. T1 en free-run se recharge seul, mais
 * l'intervalle reel vaut N + 2 cycles : d'ou le -2.
 *
 *     N = 1020500 / hz - 2      50 Hz -> 20408      60 Hz -> 17006
 *
 * Ces deux details valent 0,25 % de derive, inaudible sur un bruitage mais
 * une demi-seconde d'ecart sur un morceau de trois minutes. */
u16 __fastcall__ mbt_latch(u8 hz);

/* --- Demarrage : trois portes, payez ce que vous nommez ------------------
 *
 * mbt_start_poll()  n'embarque QUE la scrutation.
 * mbt_start_irq()   embarque le handler et l'appel ProDOS (~450 octets).
 * mbt_start()       est la commodite : elle reference les deux, donc l'appeler
 *                   embarque tout. Une application qui ne veut que la
 *                   scrutation gagne a appeler mbt_start_poll() directement.
 *
 * Le lieur cc65 prend les modules un par un : ce n'est pas une optimisation
 * theorique, c'est mesurable au `ld65 -m`. */
u8 __fastcall__ mbt_start_poll(u16 latch);
u8 __fastcall__ mbt_start_irq(u16 latch);

/* Demarre la base de temps. `latch` vient de mbt_latch().
 * Renvoie 1 si le mode demande a ete obtenu, 0 sinon -- et c'est un vrai
 * refus, pas une politesse : MBT_IRQ echoue si aucun vecteur n'est
 * installable. L'appelant doit alors se rabattre sur MBT_POLL, ou renoncer.
 * Exige une carte prise (mb_init). */
u8 __fastcall__ mbt_start(u16 latch, u8 mode);

/* Arrete le tick, desarme T1, rend le vecteur d'interruption. Idempotent.
 * A appeler AVANT de quitter le programme : un handler laisse en place sur un
 * timer qui continue de battre plante la machine des que le code a disparu. */
void mbt_stop(void);

/* Ce que le tick appelle, une fois par periode. 0 pour ne rien appeler.
 * Sous IRQ, cette fonction s'execute DANS l'interruption : courte, et sans
 * appel a ProDOS ni a la conio. */
void __fastcall__ mbt_hook(void (*cb)(void));

/* Mode courant (MBT_POLL / MBT_IRQ), ou 0xFF si arrete. */
u8 mbt_mode(void);

/* --- Mode polling uniquement --------------------------------------------- */

/* Le tick est-il echu ? Renvoie 1 et appelle le hook, ou 0. Non bloquant :
 * a appeler aussi souvent que possible depuis la boucle principale.
 * No-op en mode IRQ (le hook y est deja appele tout seul). */
u8 mbt_poll(void);

/* Compteur de ticks depuis mbt_start. Utile pour mesurer une duree sans
 * boucle calibree -- et, en mode IRQ, pour verifier depuis le programme
 * principal que le tick bat vraiment. */
extern volatile u16 mbt_ticks;

/* Ticks perdus : le handler a trouve le drapeau T1 deja re-arme, donc au moins
 * une periode entiere s'est ecoulee sans etre servie. C'est la MESURE du
 * hoquet disque (ProDOS masque les IRQ pendant un acces Disk II) : si ce
 * compteur grimpe pendant un chargement, ce n'est pas un bug, c'est le
 * materiel. S'il grimpe au repos, c'est un bug. */
extern volatile u16 mbt_lost;

/* --- Instrumentation, seulement si la lib est batie avec A2MB_DEBUG ------
 *
 * Le chronometre coute quatre lectures de VIA et une soustraction 16 bits a
 * chaque tick. C'est un outil de mise au point : un jeu fini n'en a pas
 * l'usage, et le handler tourne deja a 75 % de son budget.
 *
 * mbt_ticks et mbt_lost, eux, restent TOUJOURS disponibles -- ils ne coutent
 * qu'une incrementation et un test, et `lost` est le seul indicateur qui dise
 * en exploitation si la machine tient la cadence. */
#ifdef A2MB_DEBUG

/* Duree du handler la PLUS LONGUE observee, EN CYCLES -- mesuree en lisant le
 * compteur T1 a l'entree et a la sortie, pas estimee. A comparer a la periode
 * du tick (20408 cycles a 50 Hz) : au-dela, un tick est forcement perdu.
 * 0xFFFF signifie que le compteur a reboucle pendant le handler. */
extern volatile u16 mbt_maxdur;
extern volatile u16 mbt_over;    /* handlers ayant ATTEINT ou depasse la periode */
extern volatile u16 mbt_period;  /* periode du tick, en cycles (le budget) */

#endif /* A2MB_DEBUG */

#endif /* A2MB_TIME_H */
