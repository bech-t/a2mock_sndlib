; mb_irq.s -- handler d'interruption du tick, et son installation sous ProDOS.
;
; ///////////////////////////////////////////////////////////////////////////
; NON VALIDE SUR MACHINE NI SUR EMULATEUR. Ecrit avec soin, mais c'est
; precisement le genre de code dont on ne sait rien tant qu'il n'a pas tourne.
; Le jalon M1 (romset MAME complet) conditionne M2. Ne pas s'y fier avant.
; ///////////////////////////////////////////////////////////////////////////
;
; Trois pieges, et ce qu'on fait de chacun.
;
; 1. L'IRQ N'EST PAS FORCEMENT LA NOTRE. Sept slots peuvent interrompre. On
;    teste le drapeau T1 de NOTRE VIA et, s'il est absent, on rend la main a la
;    chaine (carry SET = « pas a moi », convention ProDOS). Sans ce test, on
;    volerait les interruptions d'une carte serie ou d'une horloge.
;
; 2. LA COMMUTATION MEMOIRE. Une IRQ peut tomber pendant que le programme a
;    bascule RAMRD/RAMWRT/ALTZP vers la memoire auxiliaire -- un pilote 80
;    colonnes passe son temps la-dedans. Un handler qui lit ses variables sans
;    rebasculer lit de la memoire aux : panne silencieuse et intermittente.
;    On FORCE la banque principale et on RESTAURE exactement l'etat trouve.
;    Sur un II+ ces bascules n'existent pas : on saute le passage (drapeau
;    _mbt_is_iie, pose une fois au demarrage).
;
; 3. LA PAGE ZERO DU RUNTIME C. Le hook est une fonction C. cc65 tient ses
;    variables de travail (sp, ptr1-4, tmp1-4, sreg...) en page zero, et le
;    programme principal peut etre en plein milieu de s'en servir. On sauve
;    donc TOUTE la ZP cc65 avant d'appeler le hook, et on la restaure apres.
;    Cout : ~420 cycles aller-retour. C'est cher, et c'est le prix d'un hook
;    ecrit en C -- un hook ecrit en asm n'en aurait pas besoin (optimisation
;    notee, pas faite : la correction d'abord).

        .export _mbt_isr, _mbt_irq_setslot
.ifdef A2MB_DEBUG
        .export _mbt_clock
.endif

; --- Instrumentation, sous A2MB_DEBUG ------------------------------------
; Le chronometre du handler (pic de duree, depassements) est un outil de MISE
; AU POINT, pas de production. Il coute quatre lectures de VIA, une
; soustraction 16 bits et deux comparaisons a CHAQUE tick -- pour un
; renseignement qu'un jeu fini n'utilise pas.
;
; Restent toujours actifs, parce qu'ils sont bon marche et utiles en
; exploitation : _mbt_ticks (une incrementation) et _mbt_lost (une lecture
; d'IFR et un test), qui dit si la machine tient la cadence.
;
;   make lib                 -> avec (defaut de ce depot : banc de validation)
;   make lib A2MB_DEBUG=0    -> sans
        ; Les compteurs et le pointeur de hook sont DEFINIS dans mb_time.c,
        ; pas ici. Sans ca, le simple fait de lire mbt_ticks depuis une
        ; application en scrutation obligeait le lieur a embarquer tout ce
        ; fichier -- le handler d'interruption compris.
        .import _mbt_ticks, _mbt_lost
        .import _mbt_hookfn, _mbt_is_iie
.ifdef A2MB_DEBUG
        .import _mbt_maxdur, _mbt_period, _mbt_over
.endif
        .import __ZP_START__, __ZP_SIZE__

; --- Softswitches //e ------------------------------------------------------
RDRAMRD   = $C013       ; lecture d'etat : bit 7 = 1 -> auxiliaire
RDRAMWRT  = $C014
RDALTZP   = $C016
CLRAUXRD  = $C002       ; ecriture : lectures en RAM principale
SETAUXRD  = $C003
CLRAUXWR  = $C004
SETAUXWR  = $C005
CLRALTZP  = $C008
SETALTZP  = $C009

VIA_T1CL  = $04
VIA_IFR   = $0D
IFR_T1    = $40

        .segment "DATA"

.ifdef A2MB_DEBUG
t_in:         .word 0
.endif

sv_rd:        .byte 0         ; etat memoire sauve, le temps du handler
sv_wr:        .byte 0
sv_zp:        .byte 0
.ifdef A2MB_DEBUG
tmp_lo:       .byte 0
tmp_hi:       .byte 0
.endif

        .segment "BSS"
zpsave:       .res 32         ; copie de la page zero cc65 (26 octets utiles)

        .segment "CODE"

; ---------------------------------------------------------------------------
; void __fastcall__ mbt_irq_setslot (unsigned char slot);
; Patche l'adresse de la VIA #1 dans le handler. Comme mb_io.s : pas de page
; zero disponible, donc code auto-modifiant.
; ---------------------------------------------------------------------------
_mbt_irq_setslot:
        and     #$07
        ora     #$C0
        sta     rdifr+2
        sta     rdifr2+2
        sta     ackt1+2
.ifdef A2MB_DEBUG
        sta     rdhi1+2
        sta     rdlo1+2
        sta     rdhi2+2
        sta     rdlo2+2
        sta     rdhi3+2
        sta     rdlo3+2
.endif
        rts

.ifdef A2MB_DEBUG
; ---------------------------------------------------------------------------
; unsigned mbt_clock (void);
;
; Lit l'horloge de mesure (T1 de la VIA #2, free-run). Permet de chronometrer
; un morceau de code depuis le C : deux lectures, une soustraction. Le
; compteur DECOMPTE, donc entree - sortie = cycles ecoules.
; ---------------------------------------------------------------------------
_mbt_clock:
rdhi3:  lda     $C085
        tax
rdlo3:  lda     $C084
        rts
.endif

; ---------------------------------------------------------------------------
; _mbt_isr -- le handler. Convention ProDOS :
;   entree : A/X/Y deja sauves par le repartiteur ProDOS.
;   sortie : carry CLAIR  = « c'etait la mienne, servie »
;            carry ARME   = « pas a moi, passe au suivant »
;            RTS (pas RTI : c'est ProDOS qui fait le RTI).
; ---------------------------------------------------------------------------
_mbt_isr:
        ; --- 1. est-ce bien la notre ? -----------------------------------
rdifr:  lda     $C00D           ; VIA #1 IFR                  (+2 patche)
        and     #IFR_T1
        bne     @mine
        sec                     ; non : la chaine continue sans nous
        rts

@mine:
        ; --- acquitte : lire T1CL efface le drapeau. T1 est en free-run, il
        ; a deja recharge tout seul -- on ne derive donc pas.
ackt1:  lda     $C004           ; VIA #1 T1CL                 (+2 patche)

.ifdef A2MB_DEBUG
        ; --- horodatage d'entree ----------------------------------------
        ; T1C-H d'abord : sa lecture n'efface pas le drapeau, contrairement a
        ; celle de T1C-L. L'ordre inverse fausserait la detection de perte.
        ; Horloge de mesure : le T1 de la VIA #2 ($Cn85/$Cn84), en free-run.
        ; PAS celui de la VIA #1 : lire son T1C-L EFFACE le drapeau
        ; d'interruption, si bien que la mesure acquittait elle-meme des ticks
        ; que le processeur n'avait jamais vus -- l'instrument creait la panne
        ; qu'il devait mesurer. La VIA #2 ne sert pas de base de temps, son
        ; drapeau n'interesse personne.
rdhi1:  lda     $C085           ; VIA #2 T1C-H                (+2 patche)
        sta     t_in+1
rdlo1:  lda     $C084           ; VIA #2 T1C-L                (+2 patche)
        sta     t_in
.endif

        ; --- 2. memoire : forcer la banque principale --------------------
        lda     _mbt_is_iie
        beq     @nobank

        lda     RDRAMRD
        sta     sv_rd
        lda     RDRAMWRT
        sta     sv_wr
        lda     RDALTZP
        sta     sv_zp
        sta     CLRAUXRD        ; (l'ecriture compte, pas la valeur)
        sta     CLRAUXWR
        sta     CLRALTZP
@nobank:

        ; --- compteurs ---------------------------------------------------
        inc     _mbt_ticks
        bne     @nc
        inc     _mbt_ticks+1
@nc:

        ; --- 3. hook ------------------------------------------------------
        lda     _mbt_hookfn
        ora     _mbt_hookfn+1
        beq     hookdone        ; aucun hook installe

        ; Cible du JSR, patchee a chaque tick (12 cycles). C'est deliberement
        ; un JSR AUTO-MODIFIE et non un JMP (ptr) : le 6502 a un bug historique
        ; sur l'indirection quand le pointeur se termine en $xxFF -- il lit
        ; l'octet de poids fort en $xx00. On aurait pu aligner le pointeur pour
        ; rendre le cas impossible, sauf que le segment DATA d'apple2.cfg n'est
        ; lui-meme pas aligne : le .align aurait menti (cl65 le dit d'ailleurs :
        ; « Segment DATA isn't aligned properly »). Supprimer l'indirection
        ; supprime la classe de bug au lieu de parier dessus.
        lda     _mbt_hookfn
        sta     hookjsr+1
        lda     _mbt_hookfn+1
        sta     hookjsr+2

        ; sauvegarde de la page zero cc65
        ldx     #<__ZP_SIZE__
@zsave: lda     __ZP_START__-1,x
        sta     zpsave-1,x
        dex
        bne     @zsave

hookjsr: jsr    $FFFF           ; cible patchee juste au-dessus

        ldx     #<__ZP_SIZE__
@zrest: lda     zpsave-1,x
        sta     __ZP_START__-1,x
        dex
        bne     @zrest

hookdone:
        ; --- ticks perdus : le drapeau est-il DEJA revenu ? --------------
        ; Si oui, une periode entiere s'est ecoulee pendant qu'on travaillait
        ; (ou pendant qu'un acces disque masquait les IRQ). On le compte au
        ; lieu de faire semblant : c'est la mesure du hoquet, pas un bug.
rdifr2: lda     $C00D           ; relit l'IFR                 (+2 patche)
        and     #IFR_T1
        beq     @noloss
        inc     _mbt_lost
        bne     @noloss
        inc     _mbt_lost+1
@noloss:

.ifdef A2MB_DEBUG
        ; --- duree du handler, en cycles ---------------------------------
        ; T1 DECOMPTE : entree - sortie = cycles ecoules. Si la sortie est
        ; superieure a l'entree, le compteur a reboucle : le handler a dure
        ; plus d'une periode, on le note au maximum.
rdhi2:  lda     $C085           ; VIA #2 T1C-H
        sta     tmp_hi
rdlo2:  lda     $C084           ; VIA #2 T1C-L
        sta     tmp_lo
        sec
        lda     t_in
        sbc     tmp_lo
        sta     tmp_lo
        lda     t_in+1
        sbc     tmp_hi
        sta     tmp_hi
        ; L'horloge de mesure reboucle sur 65536 : une soustraction 16 bits
        ; donne DEJA la bonne duree, avec ou sans emprunt. Rien a rattraper.

        ; --- depassement : la duree atteint-elle la periode du tick ? -----
        ; C'est la seule definition utile. Le compteur precedent comptait les
        ; rebouclages de l'horloge de mesure -- un evenement sans rapport, qui
        ; affichait 81 « depassements » alors qu'il n'y en avait aucun.
        lda     tmp_lo
        cmp     _mbt_period
        lda     tmp_hi
        sbc     _mbt_period+1
        bcc     @nodep
        inc     _mbt_over
        bne     @nodep
        inc     _mbt_over+1
@nodep:
        ; --- garder le maximum -------------------------------------------
        lda     tmp_hi
        cmp     _mbt_maxdur+1
        bcc     @nomax
        bne     @setmax
        lda     tmp_lo
        cmp     _mbt_maxdur
        bcc     @nomax
@setmax:
        lda     tmp_lo
        sta     _mbt_maxdur
        lda     tmp_hi
        sta     _mbt_maxdur+1
@nomax:
.endif

        ; --- restauration de l'etat memoire ------------------------------
        lda     _mbt_is_iie
        beq     @out

        lda     sv_zp
        bpl     @zp0
        sta     SETALTZP
        bmi     @rd
@zp0:   sta     CLRALTZP
@rd:    lda     sv_rd
        bpl     @rd0
        sta     SETAUXRD
        bmi     @wr
@rd0:   sta     CLRAUXRD
@wr:    lda     sv_wr
        bpl     @wr0
        sta     SETAUXWR
        bmi     @out
@wr0:   sta     CLRAUXWR

@out:
        clc                     ; « servie »
        rts
