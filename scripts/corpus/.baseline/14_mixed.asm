;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _accumulate
	.globl _crc
	.globl _matrix
	.globl _fsm
	.globl _state
	.globl __mulint
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_state::
	.ds 1
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _INITIALIZED
;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area _DABS (ABS)
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area _HOME
	.area _GSINIT
	.area _GSFINAL
	.area _GSINIT
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area _HOME
	.area _HOME
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area _CODE
;	---------------------------------
; Function fsm
; ---------------------------------
_fsm::
	or	a, a
	jrs	Z, 00101$
	cp	a, #0x01
	jrs	Z, 00102$
	cp	a, #0x02
	jrs	Z, 00103$
	sub	a, #0x05
	jrs	Z, 00104$
	jrs	00105$
00101$:
	ld	a, #0x01
	ld	(#_state),a
	ld	ba, #0xa
	ret
00102$:
	ld	a, #0x02
	ld	(#_state),a
	ld	ba, #0x14
	ret
00103$:
	xor	a, a
	ld	(_state+0), a
	ld	ba, #0x1e
	ret
00104$:
	ld	a, #0x05
	ld	(#_state),a
	ld	ba, #0x32
	ret
00105$:
	ld	ba, #0xffff
	ret
;	---------------------------------
; Function matrix
; ---------------------------------
_matrix::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	xor	a, a
	ld	-6 (ix), a
	ld	-5 (ix), a
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
00104$:
	ld	hl, 16 (sp)
	add	hl, hl
	push	hl
	ld	hl, sp
	add	hl, #2
	ld	a, l
	ld	b, h
	pop	hl
	add	hl, ba
	ld	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	(hl), #0x00
	ld	a, -2 (ix)
	inc	a
	ld	-2 (ix), a
	ld	hl, 16 (sp)
	cp	hl, #0x0004
	jrs	C, 00104$
	xor	a, a
	ld	-4 (ix), a
	ld	-3 (ix), a
00114$:
	ld	a, -4 (ix)
	ld	b, #0x00
	sla	a
	ld	l, a
	rl	b
	ld	a, l
	ld	hl, sp
	add	hl, ba
	ld	8 (sp), hl
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
00106$:
	ld	hl, 8 (sp)
	ld	a, (hl)
	ld	-8 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-7 (ix), a
	ld	a, -2 (ix)
	ld	b, #0x00
	sla	a
	ld	l, a
	rl	b
	ld	a, l
	ld	hl, sp
	add	hl, ba
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	a, -8 (ix)
	ld	b, -7 (ix)
	bcall	__mulint
	add	a, -6 (ix)
	ld	-6 (ix), a
	ld	a, b
	adc	a, -5 (ix)
	ld	-5 (ix), a
	ld	a, -2 (ix)
	inc	a
	ld	-2 (ix), a
	ld	hl, 16 (sp)
	cp	hl, #0x0004
	jrs	C, 00106$
	ld	a, -4 (ix)
	inc	a
	ld	-4 (ix), a
	ld	hl, 14 (sp)
	cp	hl, #0x0004
	jrs	C, 00114$
	ld	a, -6 (ix)
	ld	b, -5 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function crc
; ---------------------------------
_crc::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	ld	4 (sp), ba
	ld	a, #0xff
	ld	-4 (ix), a
	ld	a, #0xff
	ld	-3 (ix), a
00101$:
	ld	a, -2 (ix)
	ld	-6 (ix), a
	ld	a, -1 (ix)
	ld	-5 (ix), a
	ld	a, -2 (ix)
	add	a, #0xff
	ld	-2 (ix), a
	ld	a, -1 (ix)
	adc	a, #0xff
	ld	-1 (ix), a
	ld	a, -5 (ix)
	or	a, -6 (ix)
	jrs	Z, 00103$
	ld	b, (hl)
	inc	hl
	xor	a, a
	push	a
	push	sc
	ld	a, -4 (ix)
	xor	a, b
	ld	-6 (ix), a
	ld	a, -3 (ix)
	ld	-5 (ix), a
	pop	sc
	pop	a
	ld	a, -6 (ix)
	ld	-4 (ix), a
	ld	a, -5 (ix)
	ld	-3 (ix), a
	srl	a
	ld	-3 (ix), a
	ld	a, -4 (ix)
	rr	a
	ld	-4 (ix), a
	ld	a, -6 (ix)
	bit	a, #0x01
	jrs	Z, 00106$
	ld	ba, #0xa001
	jrs	00107$
00106$:
	xor	a, a
	ld	b, a
00107$:
	xor	a, -4 (ix)
	ld	-4 (ix), a
	ld	a, b
	xor	a, -3 (ix)
	ld	-3 (ix), a
	jrs	00101$
00103$:
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function accumulate
; ---------------------------------
_accumulate::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	ld	4 (sp), ba
	xor	a, a
	ld	-4 (ix), a
	ld	-3 (ix), a
	ld	-2 (ix), a
	ld	-1 (ix), a
00101$:
	ld	a, -6 (ix)
	ld	-8 (ix), a
	ld	a, -5 (ix)
	ld	-7 (ix), a
	ld	a, -6 (ix)
	add	a, #0xff
	ld	-6 (ix), a
	ld	a, -5 (ix)
	adc	a, #0xff
	ld	-5 (ix), a
	ld	a, -7 (ix)
	or	a, -8 (ix)
	jrs	Z, 00103$
	ld	b, (hl)
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	-10 (ix), b
	ld	-9 (ix), a
	rlc	a
	sbc	a, a
	ld	-8 (ix), a
	ld	-7 (ix), a
	ld	a, -10 (ix)
	add	a, -4 (ix)
	ld	-4 (ix), a
	ld	a, -9 (ix)
	adc	a, -3 (ix)
	ld	-3 (ix), a
	ld	a, -8 (ix)
	adc	a, -2 (ix)
	ld	-2 (ix), a
	ld	a, -7 (ix)
	adc	a, -1 (ix)
	ld	-1 (ix), a
	jrs	00101$
00103$:
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 8 (sp)
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
