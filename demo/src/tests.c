/* tests.c -- les quatre epreuves. Voir tests.h.
 *
 * Ordre deliberé : chacune ne suppose vraie que ce que la precedente a
 * demontre. Si la 2 echoue, inutile de lire la 3.
 *   1. les slots repondent-ils ?          (aucun materiel pilote)
 *   2. sort-il un son ?                   (registres seuls, pas de temps)
 *   3. le temps s'ecoule-t-il tout seul ? (IRQ, sans son)
 *   4. les deux ensemble ?                (effets cadences par le tick)
 */

#include <conio.h>
#include "tests.h"
#include "a2mb_time.h"
#include "a2mb_fx.h"
#include "sons.h"
#include "a2m.h"
#ifdef A2MB_DEBUG
extern u16 a2m_t_ev, a2m_t_env;
#endif
#include <stdio.h>
#include <string.h>

/* Attente grossiere, utilisee AVANT que la base de temps existe (epreuve 2).
 * ~1 ms par unite a 1,02 MHz. Une fois le tick disponible, on ne s'en sert
 * plus : c'est justement ce que l'epreuve 3 doit rendre inutile. */
static void wait_ms(u16 ms)
{
    volatile u16 i;
    while (ms--)
        for (i = 0; i < 90; ++i)
            ;
}

/* Plus de neuf entrees a choisir : les chiffres ne suffisent plus.
 * 1-9, puis A et suivantes. Sert au menu des sons comme a celui des morceaux. */
static char touche(u8 i)
{
    return (char)(i < 9 ? '1' + i : 'A' + (i - 9));
}

static void pause(void)
{
    cprintf("\r\n-- une touche --");
    cgetc();
}

/* --- 1. sonde detaillee du slot choisi ----------------------------------- */
/*
 * On ne sonde QUE le slot demande. Pas de balayage : ni ici, ni ailleurs.
 *
 * L'interet par rapport a un simple oui/non : la sonde separe les DEUX 6522.
 * « rien du tout » et « le premier repond, pas le second » sont deux pannes
 * completement differentes -- la seconde veut dire qu'il y a bien une carte,
 * mais que ce n'est pas une Mockingboard (carte a 6522 unique), ou que son
 * second VIA ne repond pas.
 */
void __fastcall__ t_probe_slot(u8 slot)
{
    u8 a, b, k;

    clrscr();
    cprintf("1. SONDE DU SLOT %u\r\n\n", slot);

    if (slot == 6) {
        cprintf("ATTENTION : le slot 6 porte presque\r\n");
        cprintf("toujours le controleur de disquette.\r\n");
        cprintf("la sonde y ecrit dans les phases du\r\n");
        cprintf("moteur pas-a-pas : la tete va bouger,\r\n");
        cprintf("sur la disquette qui vous fait tourner.\r\n\n");
        cprintf("continuer ? (O/N) ");
        k = cgetc();                    /* UNE seule lecture : deux cgetc()
                                         * demanderaient deux touches. */
        if (k != 'o' && k != 'O')
            return;
        cprintf("\r\n\n");
    }

    a = mb_probe_via(slot, 0);
    b = mb_probe_via(slot, 1);

    cprintf("  6522 en $C%u00 : %s\r\n", slot, a ? "OUI" : "non");
    cprintf("  6522 en $C%u80 : %s\r\n\n", slot, b ? "OUI" : "non");

    if (a && b)
        cprintf("-> Mockingboard.\r\n");
    else if (a || b)
        cprintf("-> une carte repond, mais un seul 6522 :\r\n"
                "   ce n'est pas une Mockingboard.\r\n");
    else
        cprintf("-> rien ne repond dans ce slot.\r\n");

    cprintf("\r\nla sonde exige DEUX epreuves : un\r\n");
    cprintf("aller-retour sur DDRA ($55 puis $AA),\r\n");
    cprintf("et un timer T1 qui DESCEND tout seul.\r\n");
    cprintf("la premiere ecarte le bus flottant, la\r\n");
    cprintf("seconde ecarte de la simple RAM.\r\n");
    pause();
}

/* --- 2. les six voies ---------------------------------------------------- */

/* Une note sur une voie 0..5 (0-2 = AY #1, 3-5 = AY #2). */
static void note(u8 ch, u8 idx, u8 amp, u16 ms)
{
    u8  ay = (u8)(ch >= 3);
    u8  c  = (u8)(ch % 3);
    u16 p  = mb_note_period(idx);

    MB_REG(ay, AY_TONE_A_LO + c * 2) = (u8)(p & 0xFF);
    MB_REG(ay, AY_TONE_A_HI + c * 2) = (u8)((p >> 8) & 0x0F);
    MB_REG(ay, AY_AMP_A + c)         = amp;
    /* Mixer : bits a 1 = COUPE. On ouvre le TON de cette voie seulement. */
    MB_REG(ay, AY_MIXER) = (u8)(0x3F & ~(1 << c));
    mb_push(ay, (u16)((1 << (AY_TONE_A_LO + c * 2))
                    | (1 << (AY_TONE_A_HI + c * 2))
                    | (1 << (AY_AMP_A + c)) | (1 << AY_MIXER)));
    wait_ms(ms);
    MB_REG(ay, AY_AMP_A + c) = 0;
    mb_push(ay, (u16)(1 << (AY_AMP_A + c)));
}

void t_voices(void)
{
    u8 ch;

    clrscr();
    cprintf("2. LES SIX VOIES\r\n\n");
    cprintf("do-mi-sol montant, AY #1 (voies 0-2)\r\n");
    cprintf("puis AY #2 (voies 3-5).\r\n\n");
    cprintf("si un seul AY repond, ce sont deux VIA\r\n");
    cprintf("a des adresses differentes : le probleme\r\n");
    cprintf("est dans $Cn80, pas dans le bus.\r\n\n");

    /* Arpege de do majeur sur deux octaves : do4 mi4 sol4 do5 mi5 sol5.
     *
     * Les intervalles sont 0-4-7, PAS 0-4-8 : un pas regulier de quatre
     * demi-tons donnerait do-mi-sol#, c'est-a-dire un accord AUGMENTE. La
     * quinte juste est a sept demi-tons, la tierce majeure a quatre --
     * l'octave ne se divise pas en parts egales. */
    for (ch = 0; ch < 6; ++ch) {
        static const u8 triade[6] = { 48, 52, 55, 60, 64, 67 };
        cprintf("  voie %u  (AY #%u)\r\n", ch, (u8)(ch >= 3) + 1);
        note(ch, triade[ch], 12, 400);
    }

    /* Enveloppe materielle : une note qui s'eteint seule. Si la gamme marche
     * mais pas ceci, le probleme est dans r11-r13, pas dans le bus. */
    cprintf("\r\n  enveloppe (declin)\r\n");
    MB_REG(0, AY_TONE_A_LO) = (u8)(mb_note_period(60) & 0xFF);   /* do5 */
    MB_REG(0, AY_TONE_A_HI) = (u8)(mb_note_period(60) >> 8);
    MB_REG(0, AY_ENV_LO)    = 0x00;
    MB_REG(0, AY_ENV_HI)    = 0x20;
    MB_REG(0, AY_AMP_A)     = AY_AMP_ENV;
    MB_REG(0, AY_MIXER)     = 0x3E;
    MB_REG(0, AY_ENV_SHAPE) = AY_ENV_DECAY;
    mb_push(0, MB_MASK_ALL);        /* r13 compris : c'est lui qui declenche */
    wait_ms(1500);

    mb_silence();
    pause();
}

/* --- 3. la base de temps ------------------------------------------------- */

static volatile u16 hook_calls;
static void tick_hook(void) { ++hook_calls; }

void t_tick(void)
{
    u8  mode;
    u16 t0;

    clrscr();
    cprintf("3. LA BASE DE TEMPS\r\n\n");

    hook_calls = 0;
    mbt_hook(tick_hook);

    if (mbt_start(mbt_latch(MBT_HZ_50), MBT_IRQ)) {
        mode = MBT_IRQ;
        cprintf("mode IRQ obtenu (latch %u)\r\n", mbt_latch(MBT_HZ_50));
        cprintf("le tick doit avancer TOUT SEUL.\r\n");
    } else {
        cprintf("IRQ refusee -> repli polling.\r\n");
        cprintf("(pas de ProDOS ? table pleine ?)\r\n");
        if (!mbt_start(mbt_latch(MBT_HZ_50), MBT_POLL)) {
            cprintf("polling refuse aussi. abandon.\r\n");
            pause();
            return;
        }
        mode = MBT_POLL;
    }

    cprintf("\r\n50 Hz attendus : ~50 ticks/seconde.\r\n");
    cprintf("PERDUS compte les ticks manques --\r\n");
    cprintf("c'est la mesure du hoquet disque.\r\n\n");

    t0 = mbt_ticks;
    while (!kbhit()) {
        if (mode == MBT_POLL)
            mbt_poll();
        gotoxy(0, 11);
        cprintf("ticks %5u  hook %5u  perdus %5u\r\n",
                (u16)(mbt_ticks - t0), hook_calls, mbt_lost);
        /* Cout du handler NU : ProDOS, banques memoire, sauvegarde de la page
         * zero, et un hook qui ne fait qu'incrementer. C'est le plancher --
         * tout ce qu'un lecteur ajoutera viendra par-dessus. */
        cprintf("pic handler %5u cyc / %u  %u dep.",
                mbt_maxdur, mbt_period, mbt_over);
    }
    cgetc();

    mbt_stop();
    mbt_hook(0);
    pause();
}

/* --- 4. les bruitages ---------------------------------------------------- */
void t_fx(void)
{
    u8  k, i, id = 0;
    u16 spin = 0;

    clrscr();
    /* Quatre colonnes de dix caracteres : dix-neuf sons ne tiennent pas
     * autrement sur un ecran de quarante. */
    for (i = 0; i < SON_COUNT; ++i) {
        gotoxy((u8)((i % 4) * 10), (u8)(i / 4));
        cprintf("%c %-8s", touche(i), sons_noms[i]);
    }
    gotoxy(0, 6);
    cprintf("---------------------------------------\r\n");
    cprintf("1-9 A-J un son   Q sortir\r\n\n");
    cprintf("le COMPTEUR ne doit jamais se figer :\r\n");
    cprintf("un effet est ARME, pas joue -- c'est le\r\n");
    cprintf("tick qui le fait avancer.\r\n\n");
    cprintf("ils vivent sur l'AY #2, donc une musique\r\n");
    cprintf("sur l'AY #1 n'est pas coupee.");

    mbt_hook(mb_fx_tick);
    if (!mbt_start(mbt_latch(MBT_HZ_50), MBT_IRQ))
        mbt_start(mbt_latch(MBT_HZ_50), MBT_POLL);

    for (;;) {
        if (mbt_mode() == MBT_POLL)
            mbt_poll();
        gotoxy(0, 16);
        cprintf("compteur %5u   %-9s %s   ",
                ++spin, id ? sons_noms[id - 1] : "-",
                mb_fx_active() ? "(en cours)" : "          ");
        if (kbhit()) {
            k = cgetc();
            if (k == 'q' || k == 'Q')
                break;
            i = 0xFF;
            if (k >= '1' && k <= '9')            i = (u8)(k - '1');
            else if (k >= 'A' && k <= 'J')       i = (u8)(9 + k - 'A');
            else if (k >= 'a' && k <= 'j')       i = (u8)(9 + k - 'a');
            if (i < SON_COUNT) {
                id = (u8)(i + 1);
                /* La table appartient a la demo, pas a la bibliotheque. */
                mb_fx_play(sons[i]);        /* rend la main TOUT DE SUITE */
            }
        }
    }

    mb_fx_stop();
    mbt_stop();
    mbt_hook(0);
    mb_silence();
}

/* --- 5. musique ---------------------------------------------------------- */
/*
 * Les modules sont sur la disquette, charges a la demande dans A2M_BUF
 * ($0800). On ne les embarque pas dans le binaire : a 1,5 a 2 Ko piece ils y
 * tiendraient, mais le jour ou on en met vingt il faudrait tout rebatir. Un
 * fichier par morceau, c'est aussi ce qui permet d'en ajouter un sans
 * recompiler le player.
 */

/* Noms COURTS et sans souligne : ProDOS n'accepte que lettres, chiffres et
 * points, et AppleCommander transforme un souligne en point -- « fee_dragee »
 * devenait « FEE.DRAGEE.A2M » et le fopen echouait sans rien dire. */
/* Deux familles, et elles ne servent pas a la meme chose :
 *   - a SIX voies (2 AY) : la reduction d'orchestre, plus riche, mais elle
 *     prend les deux puces -- plus de bruitages possibles par-dessus ;
 *   - a TROIS voix (1 AY) : ecrites en partition TEXTE, donc corrigeables a
 *     la main, et elles laissent l'AY #2 libre pour les effets. C'est la
 *     configuration d'un jeu. */
/* Trois familles :
 *   - SIX voix, deux AY : reduction d'orchestre, riche, mais elle prend les
 *     deux puces et interdit les bruitages par-dessus ;
 *   - TROIS voix, un AY : ecrites en partition TEXTE, corrigeables a la main,
 *     et l'AY #2 reste libre pour les effets -- la configuration d'un jeu ;
 *   - un dump YM d'Atari, profil R, pour montrer que le format avale aussi
 *     une source dont on ignore tout des instruments.
 *
 * Le repertoire est choisi pour ce que chaque piece fait ressortir de la
 * carte : le contrepoint de Bach en stereo, l'arc dramatique de Grieg, le
 * xylophone de Saint-Saens qui est exactement notre enveloppe `pluck`. */
static const char *const tunes[] = {
    "BACH.A2M", "GRIEG.A2M", "JOPLIN.A2M",  /* 6 voix, 2 AY */
    "CANCAN.A2M", "FOSSILE.A2M", "BOURDON.A2M",
    "ELISE.A2M", "MENUET.A2M", "JOIE.A2M",  /* 3 voix, 1 AY : l'AY #2 reste
                                             * libre pour les bruitages */
    "RYTHME.A2M",                           /* arpeges + percussion */
    "ARABESQUE.A2M"                         /* 6 voix, 2 AY : premier jet, jamais ecoute */
};
#define N_TUNES (sizeof(tunes) / sizeof(tunes[0]))

/* Un module par instrument du catalogue (cf. demo/scores/instruments/), pour
 * les comparer a l'oreille sans quitter la disquette. Meme melodie pour les
 * sept timbres, meme progression pour les cinq accords, meme accompagnement
 * pour les trois percussions -- seul l'instrument change d'un fichier a
 * l'autre. Tous a 3 voix, un seul AY : X reste disponible partout. */
/* Sous-repertoire TIMBRES/ : la racine ProDOS de cette disquette n'a la place
 * que pour 25 entrees (le gabarit ne l'agrandit pas), et programme + 10
 * morceaux + 15 timbres l'auraient depassee. Un sous-dossier ne compte que
 * pour UNE entree en racine, quel que soit ce qu'il contient. */
static const char *const instr_tunes[] = {
    "TIMBRES/PLUCK.A2M", "TIMBRES/BASS.A2M",              /* tons simples */
    "TIMBRES/SUSTAIN.A2M", "TIMBRES/SOFT.A2M",
    "TIMBRES/ORGAN.A2M", "TIMBRES/PIANO.A2M", "TIMBRES/BRASS.A2M",
    "TIMBRES/MAJOR.A2M", "TIMBRES/MINOR.A2M",             /* accords sur 1 voix */
    "TIMBRES/SEVENTH.A2M", "TIMBRES/FIFTH.A2M", "TIMBRES/OCTAVE.A2M",
    "TIMBRES/DRUM.A2M", "TIMBRES/CYMBAL.A2M", "TIMBRES/WIND.A2M"  /* percussions */
};
#define N_INSTR (sizeof(instr_tunes) / sizeof(instr_tunes[0]))

/* Charge un module. Renvoie 0 si le fichier manque ou n'est pas un A2M. */
/* Le tick doit faire avancer les DEUX : la musique et l'effet arme. C'est le
 * cas d'usage d'un jeu, et c'est ce qui prouve que le lecteur ne s'approprie
 * que les puces de son module. */
static void hook_musique_et_fx(void)
{
    a2m_frame();
    mb_fx_tick();
}

static u16 tune_size;           /* octets reellement lus : la taille en RAM */

static u8 load_tune(const char *name)
{
    FILE  *f;
    size_t n;

    tune_size = 0;
    f = fopen(name, "rb");
    if (!f)
        return 0;
    n = fread(A2M_BUF, 1, A2M_BUFSZ, f);
    fclose(f);
    if (n < 48)                     /* meme pas un en-tete */
        return 0;
    tune_size = (u16)n;
    return a2m_check(A2M_BUF, tune_size);
}

/* Affiche un champ de 16 caracteres non termine par zero (cf. a2m.h). */
static void put16(const char *p)
{
    u8 i;
    for (i = 0; i < 16; ++i)
        cputc(p[i]);
}

/* Coeur commun a MUSIQUE et TIMBRES : meme ecran, meme clavier, seule la
 * liste change. `heading` fait exactement 7 caracteres -- comme "MUSIQUE" et
 * "TIMBRES" -- pour garder l'alignement de "/A2MB/" sans le recalculer. */
static void module_screen(const char *heading, const char *const *list, u8 n)
{
    u8  k, i, sel = 0, loaded = 0, redraw = 1;
    u16 last = 0, tot;
    static u8 fx_cycle;

    for (;;) {
        if (redraw) {
            clrscr();
            cprintf("%s                       /A2MB/\r\n", heading);
            cprintf("---------------------------------------");   /* 39, pas 40 :
             * la 40e colonne d'un ecran 40 colonnes fait passer a la ligne,
             * donc defiler tout l'ecran -- et la liste perd une entree. */
            /* Liste sur deux colonnes : elle doit tenir en haut et laisser
             * la place au detail, qui est ce qu'on vient vraiment lire. */
            /* Trois colonnes, et SANS le suffixe .A2M : quinze morceaux ne
             * tiennent pas autrement, et l'extension est la meme pour tous --
             * elle n'apprend rien et coute cinq colonnes par entree. */
            for (i = 0; i < n; ++i) {
                u8 j, j0 = 0;
                gotoxy((u8)((i % 3) * 13), (u8)(2 + i / 3));
                cprintf("%c.", touche(i));
                /* Saute un eventuel "SOUSREP/" : seul le NOM s'affiche, le
                 * chemin complet reste dans list[] pour le fopen(). */
                for (j = 0; list[i][j]; ++j)
                    if (list[i][j] == '/')
                        j0 = (u8)(j + 1);
                for (j = j0; list[i][j] && list[i][j] != '.'; ++j)
                    cputc(list[i][j]);
                for (j = (u8)(j - j0); j < 10; ++j)
                    cputc(' ');
            }
            gotoxy(0, 2 + (n + 2) / 3);
            cprintf("---------------------------------------");   /* 39, pas 40 :
             * la 40e colonne d'un ecran 40 colonnes fait passer a la ligne,
             * donc defiler tout l'ecran -- et la liste perd une entree. */
            gotoxy(0, 21);
            cprintf("---------------------------------------");
            gotoxy(0, 22);
            /* La plage de touches est CALCULEE, pas ecrite en dur : avec dix
             * morceaux on choisit par 1-9 puis A, jamais par « 10 ».
             * Le bruitage n'apparait PAS ici -- il ne vaut que pour un module
             * a un seul AY, et le bloc de detail le signale quand c'est le
             * cas. Une legende qui annonce une touche inoperante est pire que
             * pas de legende. */
            /* Touches : les chiffres 1-9, puis A et suivantes (cf. touche()).
             * `n` est un PARAMETRE, pas une constante -- module_screen() sert
             * MUSIQUE (10 entrees) et TIMBRES (15) : la legende doit refleter
             * la liste vraiment affichee, donc ce `if` est necessaire, pas du
             * code mort. Au-dela de 15 (touche 'F'), etendre a la main. */
            if (n <= 9)
                cprintf("1-%c charger  P pause  S stop  Q sortie", touche((u8)(n - 1)));
            else if (n == 10)
                cprintf("1-9,A charger  P pause  S stop  Q sortie");
            else
                cprintf("1-9,A-%c charger  P pause  S stop  Q sortie", touche((u8)(n - 1)));
            redraw = 0;
            last = 0;
        }

        /* Detail de la lecture. Rafraichi deux fois par seconde : cprintf de
         * cc65 coute des milliers de cycles, et a chaque trame il volait au
         * lecteur le temps qu'il lui fallait. */
        if (loaded && (u16)(a2m_pos() - last) >= 25) {
            last = a2m_pos();
            tot  = a2m_frames(A2M_BUF);

            gotoxy(0, 8);
            cprintf("  "); put16(a2m_title(A2M_BUF));  cprintf("\r\n");
            cprintf("  "); put16(a2m_author(A2M_BUF)); cprintf("\r\n\r\n");

            /* Le profil vient de l'EN-TETE du module, il n'est pas ecrit en
             * dur : le lecteur joue les deux. R = flux de registres (convertit
             * n'importe quoi), T = notes + instruments (quatre fois plus
             * compact, mais il faut connaitre les instruments). */
            cprintf("  profil %c   %u AY   %u Hz        \r\n",
                    (char)a2m_profile(A2M_BUF), A2M_BUF[15], a2m_hz(A2M_BUF));
            cprintf("  taille  %5u o   $%04X-$%04X   \r\n",
                    tune_size, (u16)A2M_BUF, (u16)((u16)A2M_BUF + tune_size));
            cprintf("  duree   %5u s   %u o/s        \r\n",
                    tot / a2m_hz(A2M_BUF),
                    tot ? (u16)(tune_size / (tot / a2m_hz(A2M_BUF) + 1)) : 0);
            cprintf("  trame   %5u / %-5u          \r\n", a2m_pos(), tot);
            cprintf("  etat    %s                 \r\n",
                    a2m_state() == A2M_PLAYING ? "en lecture"
                  : a2m_state() == A2M_PAUSED  ? "en pause  " : "arrete    ");
            cprintf("  pic %5u cyc / %u\r\n", mbt_maxdur, mbt_period);
            cprintf("  dont evts %5u   envel %5u  \r\n",
                    a2m_t_ev, a2m_t_env);
            cprintf("  tick %s %5u ticks %u perdus \r\n",
                    mbt_mode() == MBT_IRQ  ? "IRQ " :
                    mbt_mode() == MBT_POLL ? "POLL" : "----",
                    mbt_ticks, mbt_lost);
            /* Un module a UN AY laisse l'autre puce libre : les bruitages y
             * jouent PAR-DESSUS la musique, sans la couper. Un module a deux
             * AY prend tout -- et le dit. */
            if (A2M_BUF[15] == 1)
                cprintf("  X : bruitage sur l'AY #2  %s   ",
                        mb_fx_active() ? "(en cours)" : "          ");
            else
                cprintf("  (2 AY : plus de voie pour un effet)  ");
        }

        if (mbt_mode() == MBT_POLL)
            mbt_poll();

        if (!kbhit())
            continue;

        k = cgetc();
        if (k == 'q' || k == 'Q')
            break;
        sel = 0xFF;
        if (k >= '1' && k <= '9')                 sel = (u8)(k - '1');
        else if (k >= 'A' && k <= 'F')            sel = (u8)(9 + k - 'A');
        else if (k >= 'a' && k <= 'f')            sel = (u8)(9 + k - 'a');
        if (sel < n) {
            a2m_stop();
            mbt_stop();
            if (!load_tune(list[sel])) {
                clrscr();
                cprintf("\r\n%s : introuvable ou pas un A2M.\r\n", list[sel]);
                loaded = 0;
                pause();
                redraw = 1;
                continue;
            }
            loaded = 1;
            mbt_hook(hook_musique_et_fx);
            if (!mbt_start(a2m_latch(A2M_BUF), MBT_IRQ))
                mbt_start(a2m_latch(A2M_BUF), MBT_POLL);
            a2m_play(A2M_BUF, tune_size, 1);
            redraw = 1;
        } else if (k == 'p' || k == 'P') {
            a2m_pause((u8)(a2m_state() != A2M_PAUSED));
            last = 0;
        } else if (k == 's' || k == 'S') {
            a2m_stop();
            last = 0;
        } else if (k == 'x' || k == 'X') {
            /* Aucun test sur l'etat de la musique : c'est justement le point.
             * L'effet vit sur l'AY #2, le morceau sur l'AY #1, et a2m_play ne
             * reinitialise que les puces qui lui appartiennent. */
            if (loaded && A2M_BUF[15] == 1)
                mb_fx_play(sons[5 + (fx_cycle++ % 3)]);   /* HIT, MAGIC, DOOR */
            last = 0;
        }
    }

    a2m_stop();
    mbt_stop();
    mbt_hook(0);
    mb_silence();
}

void t_music(void)       { module_screen("MUSIQUE", tunes,       N_TUNES); }
void t_instruments(void) { module_screen("TIMBRES", instr_tunes, N_INSTR); }
