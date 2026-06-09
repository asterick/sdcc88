;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _multi
	.globl _hilo
	.globl _combine
	.globl _rot3
	.globl _swap_args
	.globl _swap_l
	.globl _swap_i
	.globl _gl2
	.globl _gl1
	.globl _gi2
	.globl _gi1
	.globl ___mulsint2slong
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_gi1::
	.ds 2
_gi2::
	.ds 2
_gl1::
	.ds 4
_gl2::
	.ds 4
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
; Function swap_i
; ---------------------------------
_swap_i::
	ld	hl, (_gi1)
	ld	a, (_gi2+0)
	ld	(_gi1+0), a
	ld	a, (_gi2+1)
	ld	(_gi1+1), a
	ld	(_gi2), hl
	ret
;	---------------------------------
; Function swap_l
; ---------------------------------
_swap_l::
	ld	a, (_gl1)
	ld	b, a
	ld	a, (_gl1+1)
	ld	hl, (_gl1 + 2)
	push	a
	push	sc
	ld	a, (_gl2+0)
	ld	(_gl1+0), a
	pop	sc
	push	sc
	ld	a, (_gl2+1)
	ld	(_gl1+1), a
	pop	sc
	push	sc
	ld	a, (_gl2+2)
	ld	(_gl1+2), a
	pop	sc
	push	sc
	ld	a, (_gl2+3)
	ld	(_gl1+3), a
	pop	sc
	pop	a
	ld	iy, #_gl2
	ld	0 (iy), b
	ld	(_gl2+1), a
	ld	(_gl2 + 2), hl
	ret
;	---------------------------------
; Function swap_args
; ---------------------------------
_swap_args::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	ld	6 (sp), hl
	ld	4 (sp), ba
	ld	hl, 6 (sp)
	ld	a, (hl)
	ld	-8 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-7 (ix), a
	ld	hl, 4 (sp)
	ld	a, (hl)
	ld	-6 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-5 (ix), a
	ld	hl, 6 (sp)
	ld	a, -6 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -5 (ix)
	ld	(hl), a
	ld	hl, 4 (sp)
	ld	a, -8 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -7 (ix)
	ld	(hl), a
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function rot3
; ---------------------------------
_rot3::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	2 (sp), hl
	push	iy
	pop	hl
	add	a, a
	rl	b
	ld	0 (sp), ba
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	add	a, a
	rl	b
	add	a, a
	rl	b
	xor	a, -4 (ix)
	ld	-4 (ix), a
	ld	a, b
	xor	a, -3 (ix)
	ld	-3 (ix), a
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	xor	a, -4 (ix)
	ld	l, a
	ld	a, h
	xor	a, -3 (ix)
	ld	b, a
	ld	a, l
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function combine
; ---------------------------------
_combine::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	ex	a, b
	ld	8 (sp), hl
	ld	l, a
	rlc	a
	sbc	a, a
	ld	h, a
	ld	-8 (ix), b
	ld	-7 (ix), l
	xor	a, a
	ld	-10 (ix), a
	ld	-9 (ix), a
	ld	b, -2 (ix)
	ld	a, -1 (ix)
	ex	a, b
	or	a, -10 (ix)
	ld	-6 (ix), a
	ld	a, b
	or	a, -9 (ix)
	ld	-5 (ix), a
	ld	a, -8 (ix)
	ld	-4 (ix), a
	ld	a, -7 (ix)
	ld	-3 (ix), a
	ld	a, -6 (ix)
	ld	b, -5 (ix)
	ld	hl, 6 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function hilo
; ---------------------------------
_hilo::
	push	ix
	ld	ix,sp
	push	hl
	ex	a, b
	ld	0 (sp), hl
	ld	l, a
	ld	a, b
	add	a, -2 (ix)
	ld	h, a
	ld	a, l
	adc	a, -1 (ix)
	ld	b, a
	ld	a, h
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function multi
; ---------------------------------
_multi::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	ld	8 (sp), ba
	ld	6 (sp), hl
	push	hl
	ld	hl, iy
	ld	-6 (ix), l
	ld	-5 (ix), h
	ld	iy, hl
	pop	hl
	ld	a, -2 (ix)
	ld	hl, #_gi1
	add	a, -4 (ix)
	ld	(hl), a
	ld	a, -1 (ix)
	adc	a, -3 (ix)
	inc	hl
	ld	(hl), a
	ld	a, -6 (ix)
	ld	hl, #_gi2
	add	a, 5 (ix)
	ld	(hl), a
	ld	a, -5 (ix)
	adc	a, 6 (ix)
	inc	hl
	ld	(hl), a
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	ld	hl, 6 (sp)
	bcall	___mulsint2slong
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -10 (ix)
	ld	(_gl1+0), a
	ld	a, -9 (ix)
	ld	(_gl1+1), a
	ld	a, -8 (ix)
	ld	(_gl1+2), a
	ld	a, -7 (ix)
	ld	(_gl1+3), a
	ld	a, -6 (ix)
	ld	b, -5 (ix)
	ld	hl, 15 (sp)
	bcall	___mulsint2slong
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -10 (ix)
	ld	(_gl2+0), a
	ld	a, -9 (ix)
	ld	(_gl2+1), a
	ld	a, -8 (ix)
	ld	(_gl2+2), a
	ld	a, -7 (ix)
	ld	(_gl2+3), a
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
