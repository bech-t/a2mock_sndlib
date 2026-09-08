/* mb_card.c -- detection, initialisation et silence de la Mockingboard.
 * Le chemin chaud (poussee de registres) est dans mb_io.s ; ici, tout ce qui
 * ne tourne pas 50 fois par seconde.
 */

#include "a2mb.h"

/* Image RAM des 14 registres de chaque AY (pas de 16, cf. a2mb.h).
 * Definie ICI et pas en asm : mb_io.s l'importe. */
u8 mb_regs[32];
u8 mb_slot;

/* Fourni par mb_io.s : patche l'adresse de base dans le code. */
void __fastcall__ mb_io_setslot(u8 slot);

#ifdef __CC65__

/* --- Registres 6522 utiles a la detection -------------------------------- */
#define VIA_ORB   0x00
#define VIA_ORA   0x01
#define VIA_DDRB  0x02
#define VIA_DDRA  0x03
#define VIA_T1CL  0x04
#define VIA_T1CH  0x05
#define VIA_ACR   0x0B

#define AY_INACTIVE 0x04
#define AY_RESET    0x00

/* --- Detection ----------------------------------------------------------- */
/*
 * Un 6522 repond-il a cette base ? Deux epreuves, et il faut les DEUX.
 *
 * 1. ALLER-RETOUR sur DDRA. On ecrit $55 puis $AA et on relit. Un slot vide
 *    rend du bus flottant (des donnees video sur Apple II), qui ne rendra pas
 *    docilement les deux motifs.
 *
 * 2. DECOMPTE de T1. On arme le timer et on verifie qu'il DESCEND tout seul
 *    entre deux lectures. C'est ce qui distingue un 6522 d'une simple RAM, qui
 *    aurait passe l'epreuve 1 sans broncher.
 *
 * L'epreuve 1 seule accepterait de la RAM ; l'epreuve 2 seule pourrait etre
 * satisfaite par du bus flottant qui change de lui-meme. Ensemble, elles ne
 * laissent guere passer qu'un vrai VIA.
 */
static u8 via_present(u16 base)
{
    volatile u8 *v = (volatile u8 *)base;
    u8 a, b;

    v[VIA_DDRA] = 0x55;
    if (v[VIA_DDRA] != 0x55)
        return 0;
    v[VIA_DDRA] = 0xAA;
    if (v[VIA_DDRA] != 0xAA)
        return 0;

    v[VIA_ACR]  = 0x00;         /* T1 one-shot, pas de sortie sur PB7 */
    v[VIA_T1CL] = 0xFF;
    v[VIA_T1CH] = 0xFF;         /* ecrire T1CH arme et lance le decompte */
    a = v[VIA_T1CL];
    b = v[VIA_T1CL];
    /* Il DESCEND : b < a. Exiger la decroissance et pas seulement la
     * difference ecarte un bus flottant qui varierait au hasard. Les deux
     * lectures sont a quelques cycles l'une de l'autre, tres loin des 65536
     * qu'il faudrait pour reboucler. */
    return (u8)(b < a);
}

u8 __fastcall__ mb_probe_via(u8 slot, u8 which)
{
    if (slot < 1 || slot > 7)
        return 0;
    return via_present((u16)(0xC000 + ((u16)slot << 8) + (which ? 0x80 : 0)));
}

u8 __fastcall__ mb_probe(u8 slot)
{
    /* DEUX 6522, en $Cn00 et $Cn80 : signature de la Mockingboard. Une carte a
     * 6522 unique (Grappler, certaines cartes imprimante) ne passe pas. */
    return (u8)(mb_probe_via(slot, 0) && mb_probe_via(slot, 1));
}

u8 mb_scan(void)
{
    u8 s;
    /* Decroissant : on s'arrete au premier trouve, donc partir du haut limite
     * le nombre de slots reellement touches dans le cas usuel.
     *
     * Le slot 6 est SAUTE : la sonde y ecrirait dans les phases du moteur d'un
     * controleur Disk II et ferait bouger la tete de lecture -- sur la
     * disquette dont la machine vient de demarrer. Justification detaillee en
     * tete de a2mb.h. Une carte en slot 6 se prend a la main. */
    for (s = 7; s >= 1; --s) {
        if (s == MB_SCAN_SKIP_SLOT)
            continue;
        if (mb_probe(s))
            return s;
    }
    return 0;
}

u8 mb_ident(void)
{
    if (!mb_slot)
        return MB_CARD_UNKNOWN;
    /* On sait qu'il y a deux 6522 (mb_init l'a verifie). Distinguer une Phasor
     * d'une Mockingboard demande de sonder ses AY supplementaires et son mode
     * natif -- pas ecrit tant qu'on ne peut pas le VERIFIER sur emulateur ou
     * sur carte. Une detection inventee qui se trompe est pire que pas de
     * detection : la demo afficherait un nom faux avec aplomb. */
    return MB_CARD_MOCKINGBOARD;
}

/* --- Initialisation ------------------------------------------------------ */

static void ay_reset(u8 ay)
{
    volatile u8 *v = (volatile u8 *)(0xC000 + ((u16)mb_slot << 8)
                                            + (ay ? 0x80 : 0x00));
    u8 i;

    v[VIA_DDRA] = 0xFF;                 /* les deux ports en SORTIE */
    v[VIA_DDRB] = 0xFF;
    v[VIA_ORB]  = AY_INACTIVE;
    v[VIA_ORB]  = AY_RESET;             /* impulsion de RESET sur l'AY */
    v[VIA_ORB]  = AY_INACTIVE;

    /* Image RAM remise a l'etat que l'AY vient de prendre : tout coupe.
     * Sans ca, le premier mb_push croirait le mixer deja ouvert. */
    for (i = 0; i < 14; ++i)
        MB_REG(ay, i) = 0;
    MB_REG(ay, AY_MIXER) = 0x3F;        /* bits a 1 = COUPE : tout est coupe */
    mb_push(ay, MB_MASK_ALL);
}

u8 __fastcall__ mb_init(u8 slot)
{
    if (!mb_probe(slot)) {
        mb_slot = 0;
        return 0;
    }
    mb_slot = slot;
    mb_io_setslot(slot);        /* patche le coeur asm AVANT tout mb_push */
    ay_reset(0);
    ay_reset(1);
    return 1;
}

void __fastcall__ mb_reset_ay(u8 ay)
{
    if (mb_slot)
        ay_reset((u8)(ay & 1));
}

void mb_reset(void)
{
    if (!mb_slot)
        return;
    ay_reset(0);
    ay_reset(1);
}

void mb_silence(void)
{
    u8 ay;

    if (!mb_slot)
        return;
    for (ay = 0; ay < 2; ++ay) {
        MB_REG(ay, AY_AMP_A) = 0;
        MB_REG(ay, AY_AMP_B) = 0;
        MB_REG(ay, AY_AMP_C) = 0;
        MB_REG(ay, AY_MIXER) = 0x3F;
        /* r13 n'est PAS reecrit : le rearmer relancerait une enveloppe, ce qui
         * dans "silence" serait exactement le contraire du but. */
        mb_push(ay, (1 << AY_AMP_A) | (1 << AY_AMP_B) | (1 << AY_AMP_C)
                  | (1 << AY_MIXER));
    }
}

void mb_shutdown(void)
{
    mb_silence();
    mb_slot = 0;
}

#else   /* --- hote : pas de materiel, la lib se compile et se teste ------- */

u8 __fastcall__ mb_probe(u8 slot)            { (void)slot; return 0; }
u8 __fastcall__ mb_probe_via(u8 s, u8 w)     { (void)s; (void)w; return 0; }
u8 mb_scan(void)                             { return 0; }
u8 mb_ident(void)                            { return MB_CARD_UNKNOWN; }
u8 __fastcall__ mb_init(u8 slot)             { (void)slot; return 0; }
void mb_silence(void)                        { }
void __fastcall__ mb_reset_ay(u8 ay)         { (void)ay; }
void mb_reset(void)                          { }
void mb_shutdown(void)                       { mb_slot = 0; }
void __fastcall__ mb_push(u8 ay, u16 mask)   { (void)ay; (void)mask; }
void __fastcall__ mb_reg(u8 ay, u8 r, u8 v)  { MB_REG(ay, r) = v; }
void mb_lock(void)                           { }
void mb_unlock(void)                         { }
void __fastcall__ mb_io_setslot(u8 slot)     { (void)slot; }

#endif
