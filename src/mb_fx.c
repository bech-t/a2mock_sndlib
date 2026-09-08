/* mb_fx.c -- MOTEUR de bruitages sur l'AY #2 (cf. a2mb_fx.h).
 *
 * Ce fichier ne contient AUCUN son. Les sons sont des donnees, et les donnees
 * appartiennent a l'application : un jeu a son identite sonore, une demo une
 * autre. Voir demo/src/sons.c pour une table complete.
 *
 * Un effet est une SUITE DE PAS. Chaque pas dit quoi poser sur la voie et
 * combien de ticks le laisser sonner ; mb_fx_tick() passe au suivant quand le
 * compte est epuise. Aucune boucle d'attente nulle part -- c'est ce qui rend
 * l'ensemble utilisable depuis une interruption.
 *
 * Le format des pas est volontairement plat et minuscule : neuf effets tiennent
 * dans ~150 octets. Un encodage plus malin ne rapporterait rien de mesurable
 * et couterait un decodeur.
 */

#include "a2mb_fx.h"

/* --- Etat --------------------------------------------------------------- */

static const MbFxStep *cur;    /* pas courant, 0 = rien en cours */
static u8 left;              /* ticks restants sur ce pas */

/* Pose un pas sur la voie d'effets. */
static void fx_apply(const MbFxStep *s)
{
    u8 c = (u8)(MB_CH_FX % 3);          /* voie locale dans l'AY #2 */
    u16 mask;

    if (s->note == MB_FX_NOISE || s->note == MB_FX_KNOCK) {
        MB_REG(MB_FX_AY, AY_NOISE) = (u8)(s->arg & 0x1F);
        /* Mixer : bits a 1 = COUPE. On ouvre le BRUIT de la voie, on garde son
         * ton coupe -- sinon on entendrait les deux. */
        MB_REG(MB_FX_AY, AY_MIXER) = (u8)(0x3F & ~(8 << c));
        MB_REG(MB_FX_AY, AY_AMP_A + c) = s->amp;
        mask = (u16)((1 << AY_NOISE) | (1 << AY_MIXER) | (1 << (AY_AMP_A + c)));

        if (s->note == MB_FX_KNOCK) {
            MB_REG(MB_FX_AY, AY_ENV_LO)    = 0;
            MB_REG(MB_FX_AY, AY_ENV_HI)    = s->arg;
            MB_REG(MB_FX_AY, AY_ENV_SHAPE) = AY_ENV_DECAY;
            /* r13 DOIT etre dans le masque : c'est son ecriture qui rearme
             * l'enveloppe. Sans lui, le premier choc sonnerait et les suivants
             * seraient muets, l'enveloppe etant deja retombee a zero. */
            mask |= (u16)((1 << AY_ENV_LO) | (1 << AY_ENV_HI)
                        | (1 << AY_ENV_SHAPE));
        }
    } else {
        u16 p = mb_note_period(s->note);
        MB_REG(MB_FX_AY, AY_TONE_A_LO + c * 2) = (u8)(p & 0xFF);
        MB_REG(MB_FX_AY, AY_TONE_A_HI + c * 2) = (u8)((p >> 8) & 0x0F);
        MB_REG(MB_FX_AY, AY_MIXER)     = (u8)(0x3F & ~(1 << c));
        MB_REG(MB_FX_AY, AY_AMP_A + c) = s->amp;
        mask = (u16)((1 << (AY_TONE_A_LO + c * 2))
                   | (1 << (AY_TONE_A_HI + c * 2))
                   | (1 << AY_MIXER) | (1 << (AY_AMP_A + c)));
    }
    mb_push(MB_FX_AY, mask);
}

/* Coupe la voie d'effets : amplitude a zero, ton et bruit refermes. */
static void fx_silence(void)
{
    u8 c = (u8)(MB_CH_FX % 3);

    MB_REG(MB_FX_AY, AY_AMP_A + c) = 0;
    MB_REG(MB_FX_AY, AY_MIXER)     = 0x3F;
    mb_push(MB_FX_AY, (u16)((1 << (AY_AMP_A + c)) | (1 << AY_MIXER)));
}

void __fastcall__ mb_fx_play(const MbFxStep *fx)
{
    if (!mb_slot || fx == 0 || fx->note == MB_FX_END) {
        mb_fx_stop();
        return;
    }
    cur = fx;
    /* Le premier pas est pose TOUT DE SUITE, pas au prochain tick : sinon un
     * bruitage arme juste apres un tick attendrait jusqu'a 20 ms avant de
     * commencer, et le clic de validation ne collerait plus a la touche. */
    fx_apply(cur);
    left = (u8)(cur->ticks ? cur->ticks - 1 : 0);
}

void mb_fx_stop(void)
{
    if (cur) {
        cur = 0;
        if (mb_slot)
            fx_silence();
    }
    left = 0;
}

u8 mb_fx_active(void) { return (u8)(cur != 0); }

void mb_fx_tick(void)
{
    if (!cur)
        return;
    if (left) {
        --left;
        return;
    }
    ++cur;
    if (cur->note == MB_FX_END) {
        cur = 0;
        fx_silence();
        return;
    }
    fx_apply(cur);
    left = (u8)(cur->ticks ? cur->ticks - 1 : 0);
}
