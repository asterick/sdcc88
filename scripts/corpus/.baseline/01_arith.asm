;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _acc
	.globl _shl_c
	.globl _shl_l
	.globl _shr_u
	.globl _shr_i
	.globl _shl_i
	.globl _mul_l
	.globl _div_u
	.globl _mod_i
	.globl _div_i
	.globl _mul_i
	.globl _sub_l
	.globl _add_l
	.globl _sub_i
	.globl _add_i
	.globl _sl
	.globl _ul
	.globl _si
	.globl _ui
	.globl _sc
	.globl _uc
	.globl __mullong
	.globl __divuint
	.globl __modsint
	.globl __divsint
	.globl __mulint
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_uc::
	.ds 1
_sc::
	.ds 1
_ui::
	.ds 2
_si::
	.ds 2
_ul::
	.ds 4
_sl::
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
; Function add_i
; ---------------------------------
_add_i::
	add	hl, ba
	ld	a, l
	ld	b, h
	ret
;	---------------------------------
; Function sub_i
; ---------------------------------
_sub_i::
	sub	ba, hl
	ret
;	---------------------------------
; Function add_l
; ---------------------------------
_add_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	ld	4 (sp), ba
	ld	6 (sp), hl
	ld	a, -4 (ix)
	add	a, 5 (ix)
	ld	-8 (ix), a
	ld	a, -3 (ix)
	adc	a, 6 (ix)
	ld	-7 (ix), a
	ld	a, -2 (ix)
	adc	a, 7 (ix)
	ld	-6 (ix), a
	ld	a, -1 (ix)
	adc	a, 8 (ix)
	ld	-5 (ix), a
	ld	a, -8 (ix)
	ld	b, -7 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function sub_l
; ---------------------------------
_sub_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	sub	a, 5 (ix)
	ld	-4 (ix), a
	ld	a, b
	sbc	a, 6 (ix)
	ld	-3 (ix), a
	ld	a, l
	sbc	a, 7 (ix)
	ld	-2 (ix), a
	ld	a, h
	sbc	a, 8 (ix)
	ld	-1 (ix), a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function mul_i
; ---------------------------------
_mul_i::
	bjump	__mulint
	ret
;	---------------------------------
; Function div_i
; ---------------------------------
_div_i::
	bjump	__divsint
	ret
;	---------------------------------
; Function mod_i
; ---------------------------------
_mod_i::
	bjump	__modsint
	ret
;	---------------------------------
; Function div_u
; ---------------------------------
_div_u::
	bjump	__divuint
	ret
;	---------------------------------
; Function mul_l
; ---------------------------------
_mul_l::
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
;	---------------------------------
; Function shl_i
; ---------------------------------
_shl_i::
	push	hl
	pop	iy
	ld	l, a
	ld	h, b
	inc	iy
	jrs	00104$
00103$:
	add	hl, hl
00104$:
	dec	iy
	jrs	NZ, 00103$
	ld	a, l
	ld	b, h
	ret
;	---------------------------------
; Function shr_i
; ---------------------------------
_shr_i::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	h, -2 (ix)
	ld	b, -1 (ix)
	inc	l
	jrs	00104$
00103$:
	sra	b
	ld	a, h
	rr	a
	ld	h, a
00104$:
	dec	l
	jrs	NZ, 00103$
	ld	a, h
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function shr_u
; ---------------------------------
_shr_u::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	h, -2 (ix)
	ld	b, -1 (ix)
	inc	l
	jrs	00104$
00103$:
	srl	b
	ld	a, h
	rr	a
	ld	h, a
00104$:
	dec	l
	jrs	NZ, 00103$
	ld	a, h
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function shl_l
; ---------------------------------
_shl_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	ld	hl, iy
	ld	-2 (ix), l
	ld	-1 (ix), h
	pop	hl
	ld	iy, 4 (sp)
	inc	iy
	jrs	00104$
00103$:
	add	a, a
	rl	b
	adc	hl, hl
00104$:
	dec	iy
	jrs	NZ, 00103$
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -6 (ix)
	ld	b, -5 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function shl_c
; ---------------------------------
_shl_c::
	ld	b, l
	inc	b
	jrs	00104$
00103$:
	add	a, a
00104$:
	djr	nz, 00103$
	ret
;	---------------------------------
; Function acc
; ---------------------------------
_acc::
	ld	hl, #_ui
	ld	a, (hl)
	add	a, #0x03
	ld	(hl), a
	jrs	NC, 00103$
	inc	hl
	inc	(hl)
00103$:
	ld	hl, #_si
	ld	a, (hl)
	add	a, #0xf9
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, #0xff
	ld	(hl), a
	ld	hl, #_ul
	ld	a, (hl)
	add	a, #0x09
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, #0x00
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, #0x00
	ld	(hl), a
	jrs	NC, 00104$
	inc	hl
	inc	(hl)
00104$:
	ld	hl, #_sl
	ld	a, (hl)
	add	a, #0xf5
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, #0xff
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, #0xff
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, #0xff
	ld	(hl), a
	ld	a, (_uc+0)
	inc	a
	ld	(_uc+0), a
	ld	a, (_sc+0)
	dec	a
	ld	(_sc+0), a
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
