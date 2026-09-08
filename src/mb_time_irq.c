/* mb_time_irq.c -- installation du tick sous INTERRUPTION.
 *
 * Module separe, et c'est tout son interet : tant qu'une application n'appelle
 * pas mbt_start_irq(), le lieur ne prend ni ce fichier, ni le handler
 * (mb_irq.s), ni l'appel ProDOS (mb_prodos.s). Un programme qui se contente
 * de la scrutation economise ainsi ~450 octets.
 *
 * On paie ce qu'on nomme.
 */

#include "a2mb_time.h"
#include "mb_time_int.h"

#ifdef __CC65__

#define VIA_IER   0x0E
#define IER_SET   0x80          /* bit 7 a 1 : les bits poses ARMENT */
#define IER_T1    0x40

/* Fournis par mb_prodos.s : la sequence JSR $BF00 suivie de donnees ne
 * s'ecrit pas dans le __asm__ en ligne de cc65. */
u8   __fastcall__ pd_alloc_irq(void *handler);
void __fastcall__ pd_dealloc_irq(u8 num);

/* Fournis par mb_irq.s */
void mbt_isr(void);
void __fastcall__ mbt_irq_setslot(u8 slot);

static u8 int_num;              /* numero rendu par ALLOC_INTERRUPT, 0 = aucun */

/* ProDOS 8 est-il la ? Sa page globale commence par un JMP ($4C) en $BF00. */
static u8 prodos_present(void)
{
    return (u8)(*(volatile u8 *)0xBF00 == 0x4C);
}

/* Rendue a mbt_stop() par le pointeur de mbt_set_mode : c'est ce detour qui
 * evite a mb_time.c de referencer DEALLOC_INTERRUPT. */
static void release(void)
{
    if (int_num) {
        pd_dealloc_irq(int_num);
        int_num = 0;
    }
}

u8 __fastcall__ mbt_start_irq(u16 latch)
{
    volatile u8 *v;

    mbt_stop();
    if (!mbt_arm(latch))
        return 0;

    mbt_irq_setslot(mb_slot);
    int_num = prodos_present() ? pd_alloc_irq((void *)mbt_isr) : 0;
    if (!int_num) {
        /* Refus franc : pas de vecteur, donc pas d'IRQ. L'appelant se rabat
         * sur la scrutation en connaissance de cause, plutot que de croire a
         * un tick qui n'arrivera jamais. */
        v = mbt_via1();
        if (v) v[VIA_IER] = 0x7F;
        return 0;
    }

    v = mbt_via1();
    v[VIA_IER] = IER_SET | IER_T1;       /* maintenant, on arme la VIA */

    /* ... et on autorise le 6502 a ecouter. Sans ce CLI, tout est pourtant
     * juste -- ProDOS a accepte le handler, T1 tourne, l'IER est arme -- et il
     * ne se passe RIEN : le drapeau I du 6502 est pose depuis le RESET, et
     * rien dans la chaine ProDOS -> cc65 ne le leve pour nous. Symptome
     * exact : mbt_mode() dit IRQ, mbt_ticks reste a zero. */
    mb_unlock();                         /* CLI */

    mbt_set_mode(MBT_IRQ, release);
    return 1;
}

#else   /* hote */

u8 __fastcall__ mbt_start_irq(u16 latch)
{
    (void)latch;
    return 0;                            /* pas d'interruptions sur PC */
}

#endif
