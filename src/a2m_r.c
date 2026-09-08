/* a2m_r.c -- moteur du profil R : un flux de registres, trame par trame.
 *
 * Module SEPARE du profil T : une application qui ne joue que des modules T
 * n'embarque pas ce decodeur, et inversement. Appelez a2m_play_r() plutot que
 * a2m_play() pour en profiter.
 *
 * Le profil R ne suppose rien de la musique -- il ne voit que des registres --
 * ce qui en fait la seule porte pour une source dont on ignore les
 * instruments : un dump YM d'Atari, une capture d'emulateur.
 */

#include "a2m_int.h"

/* --- Encodage v2 (cf. docs/format-a2m.md) ------------------------------- */
#define CTRL_MASK1 0x01
#define CTRL_MASK2 0x02
#define CTRL_RLE   0x04
#define CTRL_AMP   0x08
#define CTRL_END   0xFF

/* Bits de masque ordonnes du registre le plus remue au plus rare. */
static const u8 mask_order[14] = { 8, 9, 10, 7, 0, 2, 4,  1, 3, 5, 6, 11, 12, 13 };

static u8  rle;            /* trames identiques restant a tenir */
static u8  amp_raw[2][3];  /* amplitude REELLE, avant attenuation */

/* Ecrit une amplitude : valeur vraie gardee a part, valeur attenuee poussee. */
static void set_amp(u8 ay, u8 ch, u8 v)
{
    amp_raw[ay][ch] = v;
    if (a2m_atten && !(v & AY_AMP_ENV))
        v = (u8)(v > a2m_atten ? v - a2m_atten : 0);
    MB_REG(ay, AY_AMP_A + ch) = v;
}

static void r_frame(void)
{
    u16 push0 = 0, push1 = 0;
    u16 packed;
    u8  ctrl, bits0 = 0, bits1 = 0, hi0 = 0, hi1 = 0;
    u8  b, i, r, k, c, ay, ch, v;

    if (a2m_st != A2M_PLAYING)
        return;

    if (rle) {                             /* « rien pendant N trames » */
        --rle;
        ++a2m_no;
        return;
    }

    ctrl = *a2m_p++;
    if (ctrl == CTRL_END) {
        if (!a2m_loop) { a2m_stop(); return; }
        a2m_p = a2m_data0;
        a2m_no = 0;
        ctrl = *a2m_p++;
        if (ctrl == CTRL_END) { a2m_stop(); return; }
    }

    /* --- deltas d'amplitude : le cas le plus frequent, et le moins cher ---
     * Une trame sur deux ne dit que « telle voie baisse d'un cran ». Deux
     * octets pour les six voies, aucune valeur transportee. */
    if (ctrl & CTRL_AMP) {
        packed = a2m_rd16(a2m_p);
        a2m_p += 2;
        for (k = 0; k < 6; ++k) {
            c = (u8)((packed >> (k * 2)) & 3);
            if (c) {
                ay = (u8)(k / 3);
                ch = (u8)(k % 3);
                v  = amp_raw[ay][ch];
                if (c == 3)      v = (u8)(v < 15 ? v + 1 : 15);
                else if (c == 1) v = (u8)(v ? v - 1 : 0);
                else             v = (u8)(v > 1 ? v - 2 : 0);
                set_amp(ay, ch, v);
                if (ay) push1 |= (u16)(1 << (AY_AMP_A + ch));
                else    push0 |= (u16)(1 << (AY_AMP_A + ch));
            }
        }
    }

    /* --- masques, avec bit de continuation ------------------------------ */
    if (ctrl & CTRL_MASK1) {
        b = *a2m_p++;
        bits0 = (u8)(b & 0x7F);
        if (b & 0x80) { hi0 = *a2m_p++; }
    }
    if (ctrl & CTRL_MASK2) {
        b = *a2m_p++;
        bits1 = (u8)(b & 0x7F);
        if (b & 0x80) { hi1 = *a2m_p++; }
    }

    if (ctrl & CTRL_RLE)
        rle = *a2m_p++;

    /* --- valeurs : AY #1 puis AY #2, dans l'ordre des bits de masque ----- */
    for (i = 0; i < 14; ++i) {
        if (i < 7 ? (bits0 & (1 << i)) : (hi0 & (1 << (i - 7)))) {
            r = mask_order[i];
            v = *a2m_p++;
            if (r >= AY_AMP_A && r <= AY_AMP_C) set_amp(0, (u8)(r - AY_AMP_A), v);
            else                                MB_REG(0, r) = v;
            push0 |= (u16)(1 << r);
        }
    }
    for (i = 0; i < 14; ++i) {
        if (i < 7 ? (bits1 & (1 << i)) : (hi1 & (1 << (i - 7)))) {
            r = mask_order[i];
            v = *a2m_p++;
            if (r >= AY_AMP_A && r <= AY_AMP_C) set_amp(1, (u8)(r - AY_AMP_A), v);
            else                                MB_REG(1, r) = v;
            push1 |= (u16)(1 << r);
        }
    }

    if (push0) mb_push(0, push0);
    if (push1 && a2m_nay == 2) mb_push(1, push1);

    ++a2m_no;
}


void __fastcall__ a2m_play_r(const u8 *mod, u8 loop)
{
    if (!a2m_begin(mod, loop))
        return;
    rle = 0;
    amp_raw[0][0] = amp_raw[0][1] = amp_raw[0][2] = 0;
    amp_raw[1][0] = amp_raw[1][1] = amp_raw[1][2] = 0;
    a2m_engine = r_frame;
    a2m_st = A2M_PLAYING;
}
