;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _tail
	.globl _nested
	.globl _mixed
	.globl _chain
	.globl _call4
	.globl _call3
	.globl _call2
	.globl _call1
	.globl _ext_v
	.globl _ext_c
	.globl _ext_l
	.globl _ext_i
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
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
; Function call1
; ---------------------------------
_call1::
	bcall	_ext_i
	ld	l, a
	ld	h, b
	inc	hl
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function call2
; ---------------------------------
_call2::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	iy, 11 (sp)
	push	iy
	ld	iy, 11 (sp)
	push	iy
	bcall	_ext_l
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
;	---------------------------------
; Function call3
; ---------------------------------
_call3::
	ld	h, a
	ld	l, a
	bjump	_ext_c
	ret
;	---------------------------------
; Function call4
; ---------------------------------
_call4::
	ld	hl, #0x0004
	push	hl
	ld	iy, #0x0003
	ld	l, #0x02
	ld	ba, #0x1
	bcall	_ext_v
	add	sp, #2
	ret
;	---------------------------------
; Function chain
; ---------------------------------
_chain::
	bcall	_ext_i
	ld	l, a
	bcall	_ext_i
	ld	l, a
	bjump	_ext_i
	ret
;	---------------------------------
; Function mixed
; ---------------------------------
_mixed::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	bcall	_ext_i
	ld	l, a
	push	hl
	push	b
	ld	iy, 18 (sp)
	push	iy
	ld	iy, 18 (sp)
	push	iy
	ld	a, 5 (ix)
	ld	b, 6 (ix)
	ld	hl, 22 (sp)
	bcall	_ext_l
	pop	iy
	pop	iy
	ld	3 (sp), ba
	ld	5 (sp), hl
	pop	b
	pop	hl
	ld	a, l
	push	a
	push	sc
	ld	a, b
	rlc	a
	sbc	a, a
	ld	l, a
	ld	h, a
	pop	sc
	pop	a
	add	a, -8 (ix)
	ld	-4 (ix), a
	ld	a, b
	adc	a, -7 (ix)
	ld	-3 (ix), a
	ld	a, l
	adc	a, -6 (ix)
	ld	-2 (ix), a
	ld	a, h
	adc	a, -5 (ix)
	ld	-1 (ix), a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 6 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function nested
; ---------------------------------
_nested::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	ld	hl, iy
	ld	-2 (ix), l
	ld	-1 (ix), h
	ld	iy, hl
	pop	hl
	push	hl
	bcall	_ext_i
	ld	2 (sp), ba
	pop	hl
	ld	a, l
	ld	b, h
	bcall	_ext_i
	ld	l, a
	ld	h, b
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	push	hl
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	bcall	_ext_i
	ld	2 (sp), ba
	pop	hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	push	hl
	ld	a, 5 (ix)
	ld	b, 6 (ix)
	bcall	_ext_i
	ld	2 (sp), ba
	pop	hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	push	hl
	ld	a, 7 (ix)
	ld	b, 8 (ix)
	bcall	_ext_i
	ld	2 (sp), ba
	pop	hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	ld	a, l
	ld	b, h
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function tail
; ---------------------------------
_tail::
	bjump	_ext_i
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
