; mb_prodos.s -- ALLOC_INTERRUPT / DEALLOC_INTERRUPT de ProDOS 8.
;
; Pourquoi passer par ProDOS plutot que par le vecteur $3FE : ProDOS tient sa
; PROPRE chaine de handlers d'interruption. Un vecteur pose derriere son dos
; serait ecrase au premier appel MLI -- ou, pire, ecraserait le sien.
;
; L'appel MLI est un JSR SUIVI de trois octets de donnees (commande + adresse
; du bloc de parametres) : c'est pour ca que ce code ne peut pas s'ecrire dans
; le __asm__ en ligne de cc65, qui refuse les pseudo-instructions.
;
; Retour MLI : carry CLAIR et A = 0 si tout va bien ; carry ARME et A = code
; d'erreur sinon.

        .export _pd_alloc_irq, _pd_dealloc_irq

MLI = $BF00

        .segment "DATA"

; Blocs de parametres. Les champs DOIVENT se suivre : ProDOS lit le bloc en
; memoire, pas champ par champ.
apb:    .byte   2               ; nombre de parametres
apnum:  .byte   0               ; [sortie] numero d'interruption alloue (1..4)
apadr:  .word   0               ; adresse du handler

dpb:    .byte   1
dpnum:  .byte   0               ; numero a rendre

        .segment "CODE"

; ---------------------------------------------------------------------------
; unsigned char __fastcall__ pd_alloc_irq (void *handler);
; Renvoie le numero d'interruption alloue (1..4), ou 0 si ProDOS a refuse
; (table pleine, ou pas de ProDOS).
; ---------------------------------------------------------------------------
_pd_alloc_irq:
        sta     apadr           ; handler arrive dans A/X (__fastcall__)
        stx     apadr+1
        lda     #2
        sta     apb
        jsr     MLI
        .byte   $40             ; ALLOC_INTERRUPT
        .word   apb
        bcs     @err
        lda     apnum
        ldx     #$00
        rts
@err:   lda     #$00            ; refus : l'appelant se rabat sur le polling
        ldx     #$00
        rts

; ---------------------------------------------------------------------------
; void __fastcall__ pd_dealloc_irq (unsigned char num);
; ---------------------------------------------------------------------------
_pd_dealloc_irq:
        sta     dpnum
        lda     #1
        sta     dpb
        jsr     MLI
        .byte   $41             ; DEALLOC_INTERRUPT
        .word   dpb
        rts
