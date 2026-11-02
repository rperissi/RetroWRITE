;
; RetroWrite II - overlay 12 (CoGS Share): slot register access
;
; The CoGS card is a FIFO behind four slot registers at $C080 + slot*16:
;   +0 DATA   read pops a byte, write pushes one
;   +1 STATUS bit 7 = a byte is waiting, bit 6 = room to write
;   +5 ID     $A2
;   +6 VERSION (nonzero)
; Every wait is bounded (cg_tmo units of about a second) and Esc aborts it,
; so a card that stops answering never hangs the word processor.
;
; cc65 fastcall: the last argument arrives in A/X, earlier ones come off
; the C stack with popax. ptr1/ptr2/tmp1/tmp2 are the runtime's scratch
; zero page and are free inside a leaf routine.
;

        .export         _cg_set, _cg_rd, _cg_wr, _cg_rdn, _cg_wrn, _cg_skip
        .export         _cg_err, _cg_tmo
        .import         popax
        .include        "zeropage.inc"

DATA    = $C080
STATUS  = $C081
KBD     = $C000
KBDSTRB = $C010

        .segment "OVLCBSS"
_cg_err:        .res 1          ; 0 ok, 1 timed out, 2 Esc pressed
_cg_tmo:        .res 1          ; timeout, units of ~1 s
cg_x:           .res 1          ; slot * 16

        .segment "OVLC"

; void __fastcall__ cg_set (unsigned char slot)
_cg_set:
        asl     a
        asl     a
        asl     a
        asl     a
        sta     cg_x
        lda     #0
        sta     _cg_err
        rts

; wait for STATUS bit 7 (a byte to read). X = slot*16 on exit.
; carry clear = ready, carry set = gave up (cg_err says why)
wait_rx:
        lda     #$80
        bne     wait_bit
; wait for STATUS bit 6 (room to write)
wait_tx:
        lda     #$40
wait_bit:
        sta     tmp2
        ldx     cg_x
        ldy     _cg_tmo
@unit:  lda     #0
        sta     ptr2
        sta     ptr2+1
@poll:  lda     STATUS,x
        and     tmp2
        bne     @ok
        inc     ptr2
        bne     @poll
        inc     ptr2+1
        bne     @poll
        lda     KBD                     ; a unit passed: Esc gives up
        cmp     #$9B
        beq     @esc
        dey
        bne     @unit
        lda     #1
        sta     _cg_err
        sec
        rts
@esc:   sta     KBDSTRB
        lda     #2
        sta     _cg_err
        sec
        rts
@ok:    clc
        rts

; unsigned char cg_rd (void)  -- 0 on timeout, see cg_err
_cg_rd:
        jsr     wait_rx
        bcs     @fail
        lda     DATA,x
        ldx     #0
        rts
@fail:  lda     #0
        tax
        rts

; void __fastcall__ cg_wr (unsigned char b)
_cg_wr:
        sta     tmp1
        jsr     wait_tx
        bcs     @done
        lda     tmp1
        sta     DATA,x
@done:  rts

; void __fastcall__ cg_rdn (void *dst, unsigned n)
_cg_rdn:
        sta     tmp1                    ; n lo (tmp2 is wait_bit's mask, so n hi goes in sreg)
        stx     sreg
        jsr     popax
        sta     ptr1
        stx     ptr1+1
@loop:  lda     tmp1
        ora     sreg
        beq     @done
        jsr     wait_rx
        bcs     @done
        lda     DATA,x
        ldy     #0
        sta     (ptr1),y
        inc     ptr1
        bne     :+
        inc     ptr1+1
:       lda     tmp1
        bne     :+
        dec     sreg
:       dec     tmp1
        jmp     @loop
@done:  rts

; void __fastcall__ cg_wrn (const void *src, unsigned n)
_cg_wrn:
        sta     tmp1
        stx     sreg
        jsr     popax
        sta     ptr1
        stx     ptr1+1
@loop:  lda     tmp1
        ora     sreg
        beq     @done
        jsr     wait_tx
        bcs     @done
        ldy     #0
        lda     (ptr1),y
        sta     DATA,x
        inc     ptr1
        bne     :+
        inc     ptr1+1
:       lda     tmp1
        bne     :+
        dec     sreg
:       dec     tmp1
        jmp     @loop
@done:  rts

; void __fastcall__ cg_skip (unsigned n)  -- read and discard n bytes
_cg_skip:
        sta     tmp1
        stx     sreg
@loop:  lda     tmp1
        ora     sreg
        beq     @done
        jsr     wait_rx
        bcs     @done
        lda     DATA,x
        lda     tmp1
        bne     :+
        dec     sreg
:       dec     tmp1
        jmp     @loop
@done:  rts
