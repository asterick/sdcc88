;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _div_ull
	.globl _mod_uu16v
	.globl _div_uu16v
	.globl _div_ii
	.globl _div_s10
	.globl _divmod_ss
	.globl _div_ss_keep
	.globl _div_ss16
	.globl _mod_ss
	.globl _div_ss
	.globl _dec3
	.globl _div16_keepb
	.globl _div16_g
	.globl _mod16_w
	.globl _mod16_10
	.globl _div16_10
	.globl _div_cond
	.globl _div_pp
	.globl _mod_gg
	.globl _div_gg
	.globl _divmod_uu
	.globl _div_chain
	.globl _div_keep
	.globl _div_lit200
	.globl _mod_u10
	.globl _div_u10
	.globl _div_uu16
	.globl _mod_uu
	.globl _div_uu
	.globl _gsr
	.globl _gsq
	.globl _gdx
	.globl _gdv
	.globl _gdu
	.globl _gdr
	.globl _gdq
	.globl __divulong
	.globl __moduint
	.globl __divuint
	.globl __divsint
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_gdq::
	.ds 1
_gdr::
	.ds 1
_gdu::
	.ds 1
_gdv::
	.ds 1
_gdx::
	.ds 2
_gsq::
	.ds 1
_gsr::
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
; Function div_uu
; ---------------------------------
_div_uu::
	push	ix
	ld	ix,sp
	push	a
	ld	b, a
	ld	-1 (ix), l
	ld	l, b
	ld	a, -1 (ix)
	ld	h, #0x00
	div
	ld	a, l
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function mod_uu
; ---------------------------------
_mod_uu::
	push	ix
	ld	ix,sp
	push	a
	ld	b, a
	ld	-1 (ix), l
	ld	l, b
	ld	a, -1 (ix)
	ld	h, #0x00
	div
	ld	a, h
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function div_uu16
; ---------------------------------
_div_uu16::
	push	ix
	ld	ix,sp
	push	a
	ld	b, a
	ld	-1 (ix), l
	ld	l, b
	ld	a, -1 (ix)
	ld	h, #0x00
	div
	ld	a, l
	ld	b, #0x00
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function div_u10
; ---------------------------------
_div_u10::
	ld	b, a
	ld	l, b
	ld	a, #0x0a
	ld	h, #0x00
	div
	ld	a, l
	ret
;	---------------------------------
; Function mod_u10
; ---------------------------------
_mod_u10::
	ld	b, a
	ld	l, b
	ld	a, #0x0a
	ld	h, #0x00
	div
	ld	a, h
	ret
;	---------------------------------
; Function div_lit200
; ---------------------------------
_div_lit200::
	ld	b, a
	ld	l, #0xc8
	ld	a, b
	ld	h, #0x00
	div
	ld	a, l
	ret
;	---------------------------------
; Function div_keep
; ---------------------------------
_div_keep::
	push	ix
	ld	ix,sp
	push	hl
	ld	b, a
	ld	-1 (ix), l
	ld	-2 (ix), h
	ld	l, b
	ld	a, -1 (ix)
	ld	h, #0x00
	div
	ld	a, l
	add	a, -2 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function div_chain
; ---------------------------------
_div_chain::
	push	ix
	ld	ix,sp
	push	hl
	ld	b, a
	ld	-1 (ix), l
	ld	-2 (ix), h
	ld	l, b
	ld	a, -1 (ix)
	ld	h, #0x00
	div
	ld	b,l
	ld	a, -2 (ix)
	ld	h, #0x00
	div
	ld	a, l
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function divmod_uu
; ---------------------------------
_divmod_uu::
	push	ix
	ld	ix,sp
	push	a
	ld	b, a
	ld	-1 (ix), l
	ld	l, b
	ld	a, -1 (ix)
	ld	h, #0x00
	div
	ld	iy, #_gdq
	ld	0 (iy), l
	ld	l, b
	ld	a, -1 (ix)
	ld	h, #0x00
	div
	ld	a, h
	ld	(#_gdr), a
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function div_gg
; ---------------------------------
_div_gg::
	ld	iy, #_gdu
	ld	l, 0 (iy)
	ld	a, (_gdv+0)
	ld	h, #0x00
	div
	ld	a, l
	ret
;	---------------------------------
; Function mod_gg
; ---------------------------------
_mod_gg::
	ld	iy, #_gdu
	ld	l, 0 (iy)
	ld	a, (_gdv+0)
	ld	h, #0x00
	div
	ld	a, h
	ld	(#_gdr), a
	ret
;	---------------------------------
; Function div_pp
; ---------------------------------
_div_pp::
	push	ix
	ld	ix,sp
	push	a
	push	a
	push	sc
	ld	a, (hl)
	ld	-1 (ix), a
	pop	sc
	pop	a
	ld	l, a
	ld	h, b
	ld	b, (hl)
	ld	l, -1 (ix)
	ld	a, b
	ld	h, #0x00
	div
	ld	a, l
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function div_cond
; ---------------------------------
_div_cond::
	push	ix
	ld	ix,sp
	push	a
	ld	-1 (ix), a
	ld	b, l
	ld	l, -1 (ix)
	ld	a, b
	ld	h, #0x00
	div
	ld	b, l
	ld	a, #0x03
	sub	a, b
	jrs	NC, 00102$
	ld	a, #0x01
	jrs	00103$
00102$:
	xor	a, a
00103$:
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function div16_10
; ---------------------------------
_div16_10::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	2 (sp), ba
	ld	a, #0x0a
	ld	hl, 2 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	push	l
	ld	l, b
	div
	pop	h
	ld	0 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function mod16_10
; ---------------------------------
_mod16_10::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	2 (sp), ba
	ld	a, #0x0a
	ld	hl, 2 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	ld	l, b
	div
	ld	l, h
	ld	h, #0x00
	ld	0 (sp), hl
	ld	a, -4 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function mod16_w
; ---------------------------------
_mod16_w::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	2 (sp), ba
	ld	a, #0x64
	ld	hl, 2 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	ld	l, b
	div
	ld	l, h
	ld	h, #0x00
	ld	0 (sp), hl
	ld	a, -4 (ix)
	ld	b, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function div16_g
; ---------------------------------
_div16_g::
	push	ix
	ld	ix,sp
	push	hl
	ld	a, #0x64
	ld	hl, (_gdx)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	push	l
	ld	l, b
	div
	pop	h
	ld	0 (sp), hl
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function div16_keepb
; ---------------------------------
_div16_keepb::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	2 (sp), ba
	ld	b, l
	push	b
	ld	a, #0x0a
	ld	hl, 3 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	push	l
	ld	l, b
	div
	pop	h
	ld	1 (sp), hl
	pop	b
	ld	l, b
	ld	h, #0x00
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	ld	a, l
	ld	b, h
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function dec3
; ---------------------------------
_dec3::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	ld	0 (sp), ba
	ld	6 (sp), hl
	ld	a, -4 (ix)
	add	a, #0x02
	ld	-8 (ix), a
	ld	a, -3 (ix)
	adc	a, #0x00
	ld	-7 (ix), a
	ld	a, #0x0a
	ld	hl, 0 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	ld	l, b
	div
	ld	l, h
	ld	h, #0x00
	ld	4 (sp), hl
	ld	a, -6 (ix)
	ld	hl, 2 (sp)
	ld	(hl), a
	ld	a, #0x0a
	ld	hl, 0 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	push	l
	ld	l, b
	div
	pop	h
	ld	8 (sp), hl
	ld	a, -4 (ix)
	add	a, #0x01
	ld	-8 (ix), a
	ld	a, -3 (ix)
	adc	a, #0x00
	ld	-7 (ix), a
	ld	a, #0x0a
	ld	hl, 8 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	ld	l, b
	div
	ld	l, h
	ld	h, #0x00
	ld	4 (sp), hl
	ld	a, -6 (ix)
	ld	hl, 2 (sp)
	ld	(hl), a
	ld	a, #0x0a
	ld	hl, 8 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	push	l
	ld	l, b
	div
	pop	h
	ld	4 (sp), hl
	ld	a, #0x0a
	ld	hl, 4 (sp)
	ld	b, l
	ld	l, h
	ld	h, #0x00
	div
	ld	l, b
	div
	ld	l, h
	ld	h, #0x00
	ld	4 (sp), hl
	ld	a, -6 (ix)
	ld	hl, 6 (sp)
	ld	(hl), a
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function div_ss
; ---------------------------------
_div_ss::
	push	ix
	ld	ix,sp
	push	a
	ld	b, a
	ld	-1 (ix), l
	ld	l, b
	ld	a, -1 (ix)
	sep
	xor	a, b
	sub	a, b
	ld	h, a
	ld	a, b
	push	a
	ld	a, l
	sep
	xor	a, b
	sub	a, b
	ld	l, a
	pop	a
	xor	a, b
	push	a
	ld	a, h
	ld	h, #0x00
	div
	ld	a, l
	pop	b
	xor	a, b
	sub	a, b
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function mod_ss
; ---------------------------------
_mod_ss::
	push	ix
	ld	ix,sp
	push	a
	ld	b, a
	ld	-1 (ix), l
	ld	l, b
	ld	a, -1 (ix)
	sep
	xor	a, b
	sub	a, b
	ld	h, a
	ld	a, l
	sep
	xor	a, b
	sub	a, b
	ld	l, a
	ld	a, b
	push	a
	ld	a, h
	ld	h, #0x00
	div
	ld	a, h
	pop	b
	xor	a, b
	sub	a, b
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function div_ss16
; ---------------------------------
_div_ss16::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ld	b, a
	ld	-1 (ix), l
	ld	l, b
	ld	a, -1 (ix)
	sep
	xor	a, b
	sub	a, b
	ld	h, a
	ld	a, b
	push	a
	ld	a, l
	sep
	xor	a, b
	sub	a, b
	ld	l, a
	pop	a
	xor	a, b
	push	a
	ld	a, h
	ld	h, #0x00
	div
	ld	a, l
	pop	b
	xor	a, b
	sub	a, b
	sep
	ld	0 (sp), ba
	ld	a, -3 (ix)
	ld	b, -2 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function div_ss_keep
; ---------------------------------
_div_ss_keep::
	push	ix
	ld	ix,sp
	push	hl
	ld	b, a
	ld	-1 (ix), l
	ld	-2 (ix), h
	ld	l, b
	ld	a, -1 (ix)
	sep
	xor	a, b
	sub	a, b
	ld	h, a
	ld	a, b
	push	a
	ld	a, l
	sep
	xor	a, b
	sub	a, b
	ld	l, a
	pop	a
	xor	a, b
	push	a
	ld	a, h
	ld	h, #0x00
	div
	ld	a, l
	pop	b
	xor	a, b
	sub	a, b
	add	a, -2 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function divmod_ss
; ---------------------------------
_divmod_ss::
	push	ix
	ld	ix,sp
	push	a
	ld	b, a
	ld	-1 (ix), l
	push	b
	ld	l, b
	ld	a, -1 (ix)
	sep
	xor	a, b
	sub	a, b
	ld	h, a
	ld	a, b
	push	a
	ld	a, l
	sep
	xor	a, b
	sub	a, b
	ld	l, a
	pop	a
	xor	a, b
	push	a
	ld	a, h
	ld	h, #0x00
	div
	ld	a, l
	pop	b
	xor	a, b
	sub	a, b
	ld	(_gsq+0), a
	pop	b
	ld	l, b
	ld	a, -1 (ix)
	sep
	xor	a, b
	sub	a, b
	ld	h, a
	ld	a, l
	sep
	xor	a, b
	sub	a, b
	ld	l, a
	ld	a, b
	push	a
	ld	a, h
	ld	h, #0x00
	div
	ld	a, h
	pop	b
	xor	a, b
	sub	a, b
	ld	(_gsr+0), a
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function div_s10
; ---------------------------------
_div_s10::
	ld	b, a
	rlc	a
	sbc	a, a
	ld	hl, #0x000a
	ex	a, b
	bjump	__divsint
;	---------------------------------
; Function div_ii
; ---------------------------------
_div_ii::
	bjump	__divsint
	ret
;	---------------------------------
; Function div_uu16v
; ---------------------------------
_div_uu16v::
	bjump	__divuint
	ret
;	---------------------------------
; Function mod_uu16v
; ---------------------------------
_mod_uu16v::
	bjump	__moduint
	ret
;	---------------------------------
; Function div_ull
; ---------------------------------
_div_ull::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	iy, 11 (sp)
	push	iy
	ld	iy, 11 (sp)
	push	iy
	bcall	__divulong
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
