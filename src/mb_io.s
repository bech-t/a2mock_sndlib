; mb_io.s -- coeur d'ecriture Mockingboard. Le seul chemin chaud de la lib.
;
; Pourquoi de l'asm ici et nulle part ailleurs : pousser 14 registres coute
; 14 x 6 = 84 ecritures VIA. En C (cc65), une ecriture a travers un pointeur
; volatile revient a ~50 cycles -> 4200 cycles par trame, soit 21 % du CPU a
; 50 Hz, rien que pour recopier des octets. Ici on est a 85 cycles par registre
; pousse (22 quand le bit du masque est a zero) : 1230 cycles pour une trame
; pleine de 14 registres, soit 6 % du CPU a 50 Hz. Compte sur le listing ca65,
; pas estime.
;
; Pourquoi du code AUTO-MODIFIANT : il faut adresser $Cn00/$Cn80 avec n connu
; seulement a l'execution. L'idiome habituel serait un pointeur en page zero et
; un (zp),y -- sauf que la cible apple2 de cc65 n'expose que 26 octets de ZP,
; integralement pris par le runtime C. Il n'y a rien a prendre.
;
; On ecrit donc l'octet de poids fort du slot DANS les instructions, une fois
; pour toutes (mb_io_setslot), et on adresse en absolu,X avec X = $00 pour
; l'AY #1 et X = $80 pour l'AY #2 -- les deux VIA ne different que par ce bit.
; Consequence : ces routines ne sont pas reentrantes (cf. la regle en tete de
; a2mb.h).

        .export _mb_io_setslot, _mb_push, _mb_reg, _mb_lock, _mb_unlock
        .import _mb_regs
        .import popa

; --- Registres 6522 (offsets depuis la base de la VIA) ---------------------
VIA_ORB      = $00      ; port B : lignes de controle AY (BDIR/BC1/RESET)
VIA_ORA      = $01      ; port A : bus donnees/adresse vers l'AY

; --- Commandes AY, via ORB (valeurs standard Mockingboard) -----------------
AY_INACTIVE  = $04
AY_LATCH     = $07      ; BDIR=1 BC1=1 : verrouille l'adresse de registre
AY_WRITE     = $06      ; BDIR=1 BC1=0 : ecrit la donnee

        .segment "DATA"

; Etat local. En DATA et non en BSS : ces octets sont patches/lus a chaud, et
; on veut qu'ils aient une valeur definie meme si mb_init n'a jamais tourne.
ayn:    .byte 0         ; AY courant (0/1), le temps d'un appel
maskl:  .byte 0
maskh:  .byte 0
tmpreg: .byte 0

        .segment "CODE"

; ---------------------------------------------------------------------------
; void __fastcall__ mb_io_setslot (unsigned char slot);
;
; Patche l'octet de poids fort des six acces VIA. Slot 1..7 -> $C1..$C7.
; A appeler AVANT tout autre appel de ce fichier.
; ---------------------------------------------------------------------------
_mb_io_setslot:
        and     #$07
        ora     #$C0            ; slot 4 -> $C4
        sta     ora1+2
        sta     ora2+2
        sta     orb1+2
        sta     orb2+2
        sta     orb3+2
        sta     orb4+2
        rts

; ---------------------------------------------------------------------------
; ayw -- ecrit UN registre AY. Interne, non exporte.
;
;   entree :  X = offset VIA ($00 = AY #1, $80 = AY #2)
;             Y = numero de registre (0..13)
;             A = valeur
;   sortie :  X et Y preserves, A detruit.
;
; Sequence imposee par l'AY : presenter l'adresse, verrouiller, revenir a
; INACTIVE, presenter la donnee, ecrire, revenir a INACTIVE. Chaque etape
; repasse par INACTIVE -- ce n'est pas de la prudence, c'est le protocole.
; ---------------------------------------------------------------------------
ayw:    pha                     ; garde la valeur
        tya
ora1:   sta     $C001,x         ; ORA <- numero de registre   (+2 patche)
        lda     #AY_LATCH
orb1:   sta     $C000,x         ; ORB <- LATCH                (+2 patche)
        lda     #AY_INACTIVE
orb2:   sta     $C000,x         ;                             (+2 patche)
        pla
ora2:   sta     $C001,x         ; ORA <- valeur               (+2 patche)
        lda     #AY_WRITE
orb3:   sta     $C000,x         ; ORB <- WRITE                (+2 patche)
        lda     #AY_INACTIVE
orb4:   sta     $C000,x         ;                             (+2 patche)
        rts

; ---------------------------------------------------------------------------
; void __fastcall__ mb_push (unsigned char ay, unsigned int mask);
;
; Pousse les registres designes par `mask` (bit i = registre i), valeurs prises
; dans mb_regs[ay*16 + i].
;
; cc65 __fastcall__ : le DERNIER argument (mask, 16 bits) arrive dans A/X ; les
; precedents sont sur la pile C, a depiler avec popa.
; ---------------------------------------------------------------------------
_mb_push:
        sta     maskl
        stx     maskh
        jsr     popa            ; A = ay          (popa detruit X et Y)
        and     #$01
        sta     ayn

        ; --- base de lecture dans mb_regs : + 0 ou + 16 -------------------
        ; On patche l'operande du LDA plutot que de sacrifier un index : X est
        ; pris par l'offset VIA et Y par le numero de registre. Il ne reste
        ; rien pour indexer aussi le banc d'AY.
        asl     a
        asl     a
        asl     a
        asl     a               ; ay * 16
        clc
        adc     #<_mb_regs
        sta     @rdval+1
        lda     #>_mb_regs
        adc     #$00            ; propage la retenue
        sta     @rdval+2

        ; --- offset VIA : $00 ou $80 -------------------------------------
        ldx     #$00
        lda     ayn
        beq     @via0
        ldx     #$80
@via0:
        ldy     #$00            ; numero de registre courant

        ; --- saut des huit registres bas s'ils sont tous absents ---------
        ; La boucle coute 22 cycles par bit IGNORE. Or le cas le plus frequent
        ; est de ne pousser que les amplitudes (r8-r10) et le mixer (r7) :
        ; on balayait alors sept a huit registres pour rien, deux fois par
        ; trame. Un masque bas nul se saute d'un coup.
        lda     maskl
        bne     @loop
        lda     maskh
        sta     maskl
        lda     #$00
        sta     maskh
        ldy     #8

        ; --- boucle sur les 14 registres ---------------------------------
        ; On decale le masque a droite : le bit 0 (registre 0) sort en premier,
        ; donc les registres partent dans l'ordre croissant. C'est l'ordre
        ; qu'impose le format A2M (les valeurs y suivent le masque dans l'ordre
        ; des numeros de registre), et le meme ordre est utile a l'oreille :
        ; la periode (r0/r1) est posee avant l'amplitude (r8) qui la rend
        ; audible, jamais l'inverse.
@loop:  lsr     maskh
        ror     maskl
        bcc     @skip
@rdval:  lda     $FFFF,y         ; mb_regs[base + Y]           (+1/+2 patches)
        jsr     ayw
@skip:  iny
        cpy     #14
        bcc     @loop
        rts

; ---------------------------------------------------------------------------
; void __fastcall__ mb_reg (unsigned char ay, unsigned char reg,
;                           unsigned char val);
;
; Un seul registre. Met aussi a jour l'image RAM mb_regs : elle ne doit jamais
; mentir sur ce que la carte a recu, sinon le prochain mb_push reecrirait une
; valeur perimee.
;
; cc65 : val arrive dans A ; reg puis ay se depilent (sommet = reg).
; ---------------------------------------------------------------------------
_mb_reg:
        pha                     ; val -> pile MATERIELLE (popa touche la pile C)
        jsr     popa            ; A = reg
        sta     tmpreg
        jsr     popa            ; A = ay
        and     #$01
        sta     ayn

        ; mb_regs[ay*16 + reg] <- val
        asl     a
        asl     a
        asl     a
        asl     a
        clc
        adc     tmpreg
        tay
        pla                     ; val
        pha
        sta     _mb_regs,y

        ; ecriture materielle
        ldx     #$00
        lda     ayn
        beq     @via0
        ldx     #$80
@via0:  ldy     tmpreg
        pla
        jmp     ayw             ; ayw fait le rts

; ---------------------------------------------------------------------------
; Section critique. Court, exprès : une IRQ manquee est un tick perdu.
; ---------------------------------------------------------------------------
_mb_lock:
        sei
        rts

_mb_unlock:
        cli
        rts
