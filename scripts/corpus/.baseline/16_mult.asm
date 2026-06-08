;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _mul_ll
	.globl _mul_ii
	.globl _mul_cc_keep
	.globl _mul_cg
	.globl _mul_ucuc
	.globl _mul_cc16
	.globl _mul_cc
	.globl _mul_lit_storec
	.globl _mul_lit_store
	.globl _mul_lit_uc
	.globl _mul_lit_sc
	.globl _mul_lit_pressure
	.globl _mul_lit_livea
	.globl _mul_lit_liveb
	.globl _mul_litc10
	.globl _mul_litc7
	.globl _mul_litc5
	.globl _mul_lit6u
	.globl _mul_lit100
	.globl _mul_lit10
	.globl _mul_lit3
	.globl _gmc
	.globl _gmi
	.globl __mullong
	.globl __mulint
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_gmi::
	.ds 2
_gmc::
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
; Function mul_lit3
; ---------------------------------
_mul_lit3::
	ld	l, a
	ld	h, b
	add	hl, hl
	add	hl, ba
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function mul_lit10
; ---------------------------------
_mul_lit10::
	ld	l, a
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, ba
	add	hl, hl
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function mul_lit100
; ---------------------------------
_mul_lit100::
	ld	l, a
	ld	h, b
	add	hl, hl
	add	hl, ba
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, ba
	add	hl, hl
	add	hl, hl
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function mul_lit6u
; ---------------------------------
_mul_lit6u::
	ld	l, a
	ld	h, b
	add	hl, hl
	add	hl, ba
	add	hl, hl
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function mul_litc5
; ---------------------------------
_mul_litc5::
	ld	b, a
	add	a, a
	add	a, a
	add	a, b
	ret
;	---------------------------------
; Function mul_litc7
; ---------------------------------
_mul_litc7::
	ld	b, a
	add	a, a
	add	a, a
	add	a, a
	sub	a, b
	ret
;	---------------------------------
; Function mul_litc10
; ---------------------------------
_mul_litc10::
	ld	b, a
	add	a, a
	add	a, a
	add	a, b
	add	a, a
	ret
;	---------------------------------
; Function mul_lit_liveb
; ---------------------------------
_mul_lit_liveb::
	ld	b, l
	push	b
	ld	b, a
	add	a, a
	add	a, b
	pop	b
	add	a, b
	ret
;	---------------------------------
; Function mul_lit_livea
; ---------------------------------
_mul_lit_livea::
	ld	b, a
	push	b
	ld	a, l
	ld	b, h
	add	hl, hl
	add	hl, ba
	pop	b
	ld	a, b
	push	a
	push	sc
	ld	b, #0x00
	pop	sc
	pop	a
	add	hl, ba
	ld	a, l
	ld	b, h
	ret
;	---------------------------------
; Function mul_lit_pressure
; ---------------------------------
_mul_lit_pressure::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	ld	6 (sp), ba
	ld	4 (sp), hl
	push	hl
	ld	hl, iy
	ld	-6 (ix), l
	ld	-5 (ix), h
	pop	hl
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	ld	l, a
	ld	h, b
	add	hl, hl
	add	hl, ba
	ld	0 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	l, a
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, ba
	ld	a, -8 (ix)
	ld	b, -7 (ix)
	add	hl, ba
	ld	a, -6 (ix)
	ld	b, -5 (ix)
	add	hl, ba
	ld	a, l
	ld	b, h
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function mul_lit_sc
; ---------------------------------
_mul_lit_sc::
	ld	l, a
	rlc	a
	sbc	a, a
	ld	h, a
	ld	a, l
	ld	b, h
	add	hl, hl
	add	hl, hl
	add	hl, ba
	add	hl, hl
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function mul_lit_uc
; ---------------------------------
_mul_lit_uc::
	ld	l, a
	ld	h, #0x00
	ld	a, l
	ld	b, h
	add	hl, hl
	add	hl, ba
	add	hl, hl
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function mul_lit_store
; ---------------------------------
_mul_lit_store::
	ld	l, a
	ld	h, b
	add	hl, hl
	add	hl, ba
	ld	(_gmi), hl
	ret
;	---------------------------------
; Function mul_lit_storec
; ---------------------------------
_mul_lit_storec::
	ld	b, a
	add	a, a
	add	a, a
	add	a, b
	ld	(_gmc+0), a
	ret
;	---------------------------------
; Function mul_cc
; ---------------------------------
_mul_cc::
	mlt
	ld	a, l
	ret
;	---------------------------------
; Function mul_cc16
; ---------------------------------
_mul_cc16::
	mlt
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function mul_ucuc
; ---------------------------------
_mul_ucuc::
	mlt
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function mul_cg
; ---------------------------------
_mul_cg::
	ld	l, a
	ld	a, (_gmc+0)
	mlt
	ld	a, l
	ret
;	---------------------------------
; Function mul_cc_keep
; ---------------------------------
_mul_cc_keep::
	ld	b, a
	ld	a, h
	mlt
	ld	a, l
	add	a, b
	ret
;	---------------------------------
; Function mul_ii
; ---------------------------------
_mul_ii::
	bjump	__mulint
	ret
;	---------------------------------
; Function mul_ll
; ---------------------------------
_mul_ll::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	iy, 11 (sp)
	push	iy
	ld	iy, 11 (sp)
	push	iy
	bcall	__mullong
	pop	iy
	pop	iy
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
