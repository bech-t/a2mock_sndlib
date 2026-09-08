/* mb_time.c -- base de temps : programmation de T1, installation du tick.
 * Le handler lui-meme est dans mb_irq.s. Ici, tout ce qui n'est pas dans
 * l'interruption.
 */

#include "a2mb_time.h"
#include "mb_time_int.h"

/* Compteurs et pointeur de hook : DEFINIS ici, et non dans mb_irq.s.
 *
 * Ce n'est pas cosmetique. Tant qu'ils vivaient dans le handler, lire
 * mbt_ticks depuis une application en scrutation suffisait a faire embarquer
 * par le lieur tout mb_irq.o -- le handler d'interruption compris, dont elle
 * n'avait que faire. */
volatile u16 mbt_ticks;
volatile u16 mbt_lost;
#ifdef A2MB_DEBUG
volatile u16 mbt_maxdur;
volatile u16 mbt_period;
volatile u16 mbt_over;
#endif
void (*mbt_hookfn)(void);
u8 mbt_is_iie;


static u8 cur_mode = 0xFF;      /* 0xFF = arrete */

/* Comment rendre le vecteur d'interruption, si on en a pris un. Ce pointeur
 * est le seul lien entre mb_time.c et le code IRQ : tant que l'application
 * n'appelle pas mbt_start_irq(), rien ne le renseigne et le lieur laisse
 * mb_irq.o et mb_prodos.o de cote. */
static void (*irq_release)(void);

/* Horloge du 6522 en slot : 1 020 500 Hz. Ce n'est PAS 1 023 000 -- l'Apple II
 * etire un cycle sur 65 pour rester en phase avec le balayage video, et la
 * moyenne qui en resulte est 1,0205 MHz. Et T1 en free-run compte N + 2 cycles
 * par periode, d'ou le -2.
 *
 * L'erreur des deux details reunis vaut 0,25 % : inaudible sur un bruitage,
 * une demi-seconde de decalage sur un morceau de trois minutes. */
#define VIA_CLOCK 1020500UL

u16 __fastcall__ mbt_latch(u8 hz)
{
    if (hz < 10)                /* garde-fou : en dessous, N deborde 16 bits */
        hz = 50;
    return (u16)((VIA_CLOCK / hz) - 2);
}

u8 mbt_mode(void) { return cur_mode; }

void __fastcall__ mbt_hook(void (*cb)(void)) { mbt_hookfn = cb; }

#ifdef __CC65__

#define VIA_T1CL  0x04
#define VIA_T1CH  0x05
#define VIA_T1LL  0x06
#define VIA_T1LH  0x07
#define VIA_ACR   0x0B
#define VIA_IER   0x0E
#define VIA_IFR   0x0D

#define ACR_T1_FREERUN 0x40     /* T1 recharge et repart tout seul */
#define IFR_T1         0x40
#define IER_SET        0x80     /* bit 7 a 1 : les bits poses ARMENT */
#define IER_T1         0x40

/* Base de la VIA #1 (celle qui porte la base de temps). L'AY #2 n'a pas de
 * timer arme : un seul suffit, et deux se dephaseraient. */
static volatile u8 *via1(void)
{
    return (volatile u8 *)(0xC000 + ((u16)mb_slot << 8));
}

/* Machine ID en $FBB3 : $06 = //e ou ulterieur, donc bascules main/aux
 * presentes. Sur un II+ ($EA) elles n'existent pas, et le handler doit sauter
 * tout le passage -- ecrire en $C002-$C009 sur un II+ ne ferait rien de bon.
 * Lu UNE fois, au demarrage : c'est une propriete de la machine. */
static u8 detect_iie(void)
{
    return (u8)(*(volatile u8 *)0xFBB3 == 0x06);
}

/* --- Demarrage / arret --------------------------------------------------- */

/* --- Armement du timer, commun aux deux modes ---------------------------- */

volatile u8 *mbt_via1(void)
{
    return mb_slot ? (volatile u8 *)(0xC000 + ((u16)mb_slot << 8)) : 0;
}

u8 __fastcall__ mbt_arm(u16 latch)
{
    volatile u8 *v, *v2;

    if (!mb_slot)
        return 0;
    mbt_is_iie = detect_iie();
    mbt_ticks  = 0;
    mbt_lost   = 0;
#ifdef A2MB_DEBUG
    mbt_maxdur = 0;
    mbt_over   = 0;
    mbt_period = latch;
#endif
    v = via1();

    /* T1 en free-run. IRQ DESARMEE : c'est a l'appelant de l'armer une fois
     * qu'il a obtenu un vecteur, sinon une interruption tomberait sans
     * handler -- c'est-a-dire dans le decor. */
    v[VIA_IER]  = 0x7F;
    v[VIA_ACR]  = ACR_T1_FREERUN;
    v[VIA_T1LL] = (u8)(latch & 0xFF);
    v[VIA_T1LH] = (u8)(latch >> 8);
    v[VIA_T1CH] = (u8)(latch >> 8);      /* arme et lance */

    /* Horloge de MESURE, sur la VIA #2 : free-run a periode maximale. Le
     * handler s'en sert pour se chronometrer sans toucher au drapeau de la
     * VIA #1, dont la lecture acquitterait un tick. */
    v2 = (volatile u8 *)(0xC000 + ((u16)mb_slot << 8) + 0x80);
    v2[VIA_IER]  = 0x7F;
    v2[VIA_ACR]  = ACR_T1_FREERUN;
    v2[VIA_T1LL] = 0xFF;
    v2[VIA_T1LH] = 0xFF;
    v2[VIA_T1CH] = 0xFF;
    return 1;
}

void mbt_set_mode(u8 mode, void (*release)(void))
{
    cur_mode    = mode;
    irq_release = release;
}

/* --- Scrutation : ne reference RIEN de l'IRQ ---------------------------- */

u8 __fastcall__ mbt_start_poll(u16 latch)
{
    mbt_stop();
    if (!mbt_arm(latch))
        return 0;
    mbt_set_mode(MBT_POLL, 0);           /* T1 tourne, personne ne l'ecoute */
    return 1;
}

void mbt_stop(void)
{
    volatile u8 *v;

    if (cur_mode == 0xFF)
        return;
    if (mb_slot) {
        v = via1();
        v[VIA_IER] = 0x7F;               /* desarmer AVANT de rendre le vecteur */
        v[VIA_ACR] = 0x00;               /* T1 en one-shot : il s'arretera */
        (void)v[VIA_T1CL];               /* acquitte un drapeau en attente */
    }
    if (irq_release) {
        irq_release();
        irq_release = 0;
    }
    cur_mode = 0xFF;
}

u8 mbt_poll(void)
{
    volatile u8 *v;

    if (cur_mode != MBT_POLL || !mb_slot)
        return 0;
    v = via1();
    if ((v[VIA_IFR] & IFR_T1) == 0)
        return 0;
    (void)v[VIA_T1CL];                   /* lire T1CL acquitte le drapeau */
    ++mbt_ticks;
    if (mbt_hookfn)
        mbt_hookfn();
    return 1;
}

#else   /* --- hote ------------------------------------------------------- */

/* Sur Apple II ces symboles vivent dans mb_irq.s, qui n'est pas assemble ici.
 * Sans eux le build hote ne se lierait pas -- et c'est justement le build qui
 * permet de tester la logique portable sans emulateur. */
u8 __fastcall__ mbt_start_poll(u16 latch)  { (void)latch; cur_mode = MBT_POLL; return 1; }
u8 __fastcall__ mbt_arm(u16 latch)         { (void)latch; return 1; }
void mbt_set_mode(u8 m, void (*r)(void))   { cur_mode = m; (void)r; }
volatile u8 *mbt_via1(void)                { return 0; }
void mbt_stop(void)                      { cur_mode = 0xFF; }
u8   mbt_poll(void)
{
    if (cur_mode != MBT_POLL) return 0;
    ++mbt_ticks;
    if (mbt_hookfn) mbt_hookfn();
    return 1;
}


#endif
