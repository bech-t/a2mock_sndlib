/* a2m.h -- lecteur de modules A2M, profil R (flux de registres).
 *
 * Un module R decrit l'etat des 14 registres de l'AY, trame par trame, en ne
 * codant QUE ce qui change :
 *
 *     trame := masque(2 octets) [rle(1 octet)] valeurs(0..14)
 *              bit i (0..13) = « registre i ecrit, sa valeur suit »
 *              bit 14        = un octet suit : nombre de trames identiques
 *     fin   := 0xFFFF
 *
 * Le lecteur ne fait donc que RECOPIER des octets au fil du masque, puis
 * appeler mb_push() avec ce meme masque. Tout le travail difficile -- lire le
 * PT3, interpreter les ornements, reechelonner les periodes de l'horloge du ZX
 * vers celle de l'Apple -- a ete fait a l'hote par le convertisseur.
 *
 * Le masque n'est pas un simple « ce qui a change » : le registre 13 (forme
 * d'enveloppe) est un DECLENCHEUR, y ecrire REARME l'enveloppe meme avec la
 * meme valeur. Le masque distingue « r13 vaut toujours 8 » de « on reecrit 8 ».
 */
#ifndef A2M_H
#define A2M_H

#include "a2mb.h"

#define A2M_STOPPED  0
#define A2M_PLAYING  1
#define A2M_PAUSED   2

/* --- Ou mettre le module -------------------------------------------------
 *
 * a2m_play() prend un POINTEUR : le module peut vivre n'importe ou. Les deux
 * constantes ci-dessous ne sont qu'une CONVENTION -- une adresse en dur et une
 * taille. Rien n'est declare, rien n'est reserve, et la bibliotheque ne s'en
 * sert jamais elle-meme. C'est a vous de garantir que la zone est libre.
 *
 * /!\ $2000-$3FFF EST LA PAGE HIRES 1. Un programme qui affiche du graphisme
 * ne peut PAS utiliser A2M_BUF tel quel : l'image et le module se recouvrent.
 * Dans ce cas, prenez A2M_BUF_LOW (6 Ko sous la page HIRES), qui suffit a
 * trois minutes de musique en profil T.
 *
 * Trois emplacements raisonnables, selon ce que fait votre programme :
 *
 *   A2M_BUF      $0800-$3FFF   14 Ko   ~7 min en profil T. Interdit si vous
 *                                      utilisez la page HIRES 1.
 *   A2M_BUF_LOW  $0800-$1FFF    6 Ko   ~3 min. Compatible HIRES.
 *   un tableau C  n'importe ou          lie DANS le binaire : pratique pour un
 *                                      jingle de quelques centaines d'octets,
 *                                      et rien a charger au demarrage.
 *
 * La page texte 1 ($0400-$07FF) est en dessous de tout ca ; la page texte 2
 * ($0800-$0BFF) n'est jamais activee par cette bibliotheque, mais elle l'est
 * par certains programmes -- verifiez.
 *
 * Debits mesures : ~32 o/s en profil T, ~250 o/s en profil R. */
#define A2M_BUF       ((u8 *)0x0800)
#define A2M_BUFSZ     14336

#define A2M_BUF_LOW   ((u8 *)0x0800)
#define A2M_BUFSZ_LOW  6144

/* Valide l'en-tete d'un module deja en memoire. `len` est le nombre d'octets
 * REELLEMENT charges a partir de `mod` -- PAS une taille lue dans le fichier
 * lui-meme, qu'on ne peut pas croire sur parole (secteur illisible, copie
 * interrompue : le disque peut rendre moins d'octets que prevu). 0 = refuse
 * (magie absente, version/profil inconnu, ou en-tete plus longue que `len`).
 * Ne joue rien. Passez le MEME `len` a a2m_play_t()/_r()/() ensuite : c'est
 * lui qui borne tout le flux, trame par trame -- cf. docs/integration.md. */
u8 __fastcall__ a2m_check(const u8 *mod, u16 len);

/* Titre / auteur, lus dans l'en-tete. Chaines de 16 caracteres NON terminees
 * par zero : a afficher avec une largeur fixe. Ils sont dans l'en-tete
 * expres -- une disquette de musique sans credits serait malpolie envers les
 * compositeurs dont on rejoue le travail. */
const char *__fastcall__ a2m_title(const u8 *mod);
const char *__fastcall__ a2m_author(const u8 *mod);

/* Frequence de tick voulue par le module, et sa valeur de latch T1 : un
 * morceau a 50 Hz et un morceau a 60 Hz ne se cadencent pas pareil. */
u8  __fastcall__ a2m_hz(const u8 *mod);
u16 __fastcall__ a2m_frames(const u8 *mod);   /* duree totale, en trames */
u8  __fastcall__ a2m_profile(const u8 *mod);  /* 'R' ou 'T' */
u16 __fastcall__ a2m_latch(const u8 *mod);

/* Arme la lecture. `len` est le nombre d'octets REELLEMENT charges a partir de
 * `mod` -- le MEME que celui passe a a2m_check(). C'est lui, et rien d'autre,
 * qui borne chaque lecture du flux pendant la partition entiere : un module
 * tronque (secteur illisible, copie interrompue) arrete proprement la lecture
 * au lieu de continuer dans la memoire qui suit le tampon -- potentiellement
 * votre PROGRAMME, charge juste au-dessus d'A2M_BUF (cf. plus haut). Se
 * trompe de `len` (par exemple `A2M_BUFSZ` au lieu des octets vraiment lus)
 * annule cette protection : le lecteur croirait le tampon plein alors qu'il
 * ne l'est pas.
 *
 * `loop` non nul reboucle a la fin (l'en-tete porte aussi son propre drapeau
 * de boucle ; celui-ci le force). Ne joue pas : c'est le tick qui fait
 * avancer. */
/* --- Demarrage : trois portes, payez ce que vous nommez ------------------
 *
 * a2m_play_t()  n'embarque que le moteur du profil T (notes + instruments).
 * a2m_play_r()  n'embarque que celui du profil R (flux de registres).
 * a2m_play()    lit le profil dans l'en-tete et appelle le bon -- donc elle
 *               NOMME les deux, et le lieur les embarque tous les deux.
 *
 * Une application qui ne diffuse que ses propres modules connait leur profil :
 * elle a tout interet a appeler directement le moteur voulu. */
void __fastcall__ a2m_play_t(const u8 *mod, u16 len, u8 loop);
void __fastcall__ a2m_play_r(const u8 *mod, u16 len, u8 loop);
void __fastcall__ a2m_play(const u8 *mod, u16 len, u8 loop);

void a2m_stop(void);
void __fastcall__ a2m_pause(u8 on);
u8   a2m_state(void);
u16  a2m_pos(void);          /* trame courante : barre de progression */

/* Attenuation globale, 0 = pleine puissance, 15 = silence. Appliquee AU
 * MOMENT de la poussee, par soustraction saturee sur r8/r9/r10 : le module
 * n'est pas modifie, et un fondu coute une soustraction par voie et par
 * trame. */
void __fastcall__ a2m_volume(u8 atten);

/* Fait avancer d'UNE trame. A brancher sur le tick :  mbt_hook(a2m_frame);
 * No-op si rien n'est en cours. */
void a2m_frame(void);

#endif /* A2M_H */
