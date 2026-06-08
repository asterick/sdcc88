;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _branchy
	.globl _lcmp_lit
	.globl _litcmp
	.globl _lt_c
	.globl _eq_l
	.globl _lt_l
	.globl _gt_u
	.globl _lt_u
	.globl _ne_i
	.globl _eq_i
	.globl _ge_i
	.globl _le_i
	.globl _gt_i
	.globl _lt_i
	.globl _si
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_si::
	.ds 2
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
; Function lt_i
; ---------------------------------
_lt_i::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	ba, hl
	ld	a, #0x01
	jrs	LT, 00103$
	xor	a, a
00103$:
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function gt_i
; ---------------------------------
_gt_i::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	hl, ba
	ld	a, #0x01
	jrs	LT, 00103$
	xor	a, a
00103$:
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function le_i
; ---------------------------------
_le_i::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	hl, ba
	ld	a, #0x01
	jrs	LT, 00103$
	xor	a, a
00103$:
	xor	a, #0x01
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function ge_i
; ---------------------------------
_ge_i::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	ba, hl
	ld	a, #0x01
	jrs	LT, 00103$
	xor	a, a
00103$:
	xor	a, #0x01
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function eq_i
; ---------------------------------
_eq_i::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	hl, ba
	ld	a, #0x01
	jrs	Z, 00104$
	xor	a, a
00104$:
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function ne_i
; ---------------------------------
_ne_i::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	hl, ba
	ld	a, #0x01
	jrs	Z, 00104$
	xor	a, a
00104$:
	xor	a, #0x01
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function lt_u
; ---------------------------------
_lt_u::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	ba, hl
	ld	a, #0x00
	rl	a
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function gt_u
; ---------------------------------
_gt_u::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	hl, ba
	ld	a, #0x00
	rl	a
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function lt_l
; ---------------------------------
_lt_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	sub	a, 5 (ix)
	ld	a, -3 (ix)
	sbc	a, 6 (ix)
	ld	a, -2 (ix)
	sbc	a, 7 (ix)
	ld	a, -1 (ix)
	sbc	a, 8 (ix)
	ld	a, #0x01
	jrs	LT, 00103$
	xor	a, a
00103$:
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function eq_l
; ---------------------------------
_eq_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	sub	a, 5 (ix)
	jrs	NZ, 00103$
	ld	a, -3 (ix)
	sub	a, 6 (ix)
	jrs	NZ, 00103$
	ld	a, -2 (ix)
	sub	a, 7 (ix)
	jrs	NZ, 00103$
	ld	a, -1 (ix)
	sub	a, 8 (ix)
	ld	a, #0x01
	jrs	Z, 00104$
00103$:
	xor	a, a
00104$:
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function lt_c
; ---------------------------------
_lt_c::
	ld	h, a
	ld	b, l
	ld	a, h
	sub	a, b
	ld	a, #0x01
	jrs	LT, 00103$
	xor	a, a
00103$:
	ld	b, #0x00
	ret
;	---------------------------------
; Function litcmp
; ---------------------------------
_litcmp::
	ld	l, a
	ld	h, b
	cp	hl, #0x0064
	jrs	GE, 00102$
	ld	ba, #0x1
	ret
00102$:
	ld	ba, #0x2
	ret
;	---------------------------------
; Function lcmp_lit
; ---------------------------------
_lcmp_lit::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	sub	a, #0xe8
	ld	a, -3 (ix)
	sbc	a, #0x03
	ld	a, -2 (ix)
	sbc	a, #0x00
	ld	a, -1 (ix)
	sbc	a, #0x00
	jrs	GE, 00102$
	ld	ba, #0x1
	jrs	00103$
00102$:
	xor	a, a
	ld	b, a
00103$:
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function branchy
; ---------------------------------
_branchy::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	ba, hl
	jrs	GE, 00105$
	ld	hl, #0x0001
	ld	(_si), hl
	jrs	00107$
00105$:
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	cp	hl, ba
	jrs	NZ, 00102$
	ld	hl, #0x0002
	ld	(_si), hl
	jrs	00107$
00102$:
	ld	hl, #0x0003
	ld	(_si), hl
00107$:
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
