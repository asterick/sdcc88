;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _sumbf
	.globl _setall
	.globl _setd
	.globl _setc
	.globl _setb
	.globl _seta
	.globl _getc
	.globl _getb
	.globl _geta
	.globl _gf
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_gf::
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
; Function geta
; ---------------------------------
_geta::
	ld	a, (hl)
	and	a, #0x01
	ld	b, #0x00
	ret
;	---------------------------------
; Function getb
; ---------------------------------
_getb::
	ld	a, (hl)
	rrc	a
	and	a, #0x07
	ld	b, #0x00
	ret
;	---------------------------------
; Function getc
; ---------------------------------
_getc::
	ld	a, (hl)
	rlc	a
	rlc	a
	rlc	a
	rlc	a
	and	a, #0x0f
	ld	b, #0x00
	ret
;	---------------------------------
; Function seta
; ---------------------------------
_seta::
	and	a, #0x01
	ld	b, a
	ld	a, (hl)
	and	a, #0xfe
	or	a, b
	ld	(hl), a
	ret
;	---------------------------------
; Function setb
; ---------------------------------
_setb::
	rlc	a
	and	a, #0x0e
	ld	b, a
	ld	a, (hl)
	and	a, #0xf1
	or	a, b
	ld	(hl), a
	ret
;	---------------------------------
; Function setc
; ---------------------------------
_setc::
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	b, a
	ld	a, (hl)
	and	a, #0x0f
	or	a, b
	ld	(hl), a
	ret
;	---------------------------------
; Function setd
; ---------------------------------
_setd::
	inc	hl
	ld	(hl), a
	ret
;	---------------------------------
; Function setall
; ---------------------------------
_setall::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), hl
	ld	hl, 0 (sp)
	ld	a, (hl)
	or	a, #0x01
	ld	(hl), a
	ld	hl, 0 (sp)
	ld	a, (hl)
	and	a, #0xf1
	or	a, #0x0a
	ld	(hl), a
	ld	hl, 0 (sp)
	ld	a, (hl)
	and	a, #0x0f
	or	a, #0x90
	ld	(hl), a
	ld	hl, 0 (sp)
	inc	hl
	ld	(hl), #0xc8
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function sumbf
; ---------------------------------
_sumbf::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	ld	4 (sp), hl
	ld	hl, 4 (sp)
	ld	a, (hl)
	and	a, #0x01
	ld	-4 (ix), a
	xor	a, a
	ld	-3 (ix), a
	ld	hl, 4 (sp)
	ld	a, (hl)
	rrc	a
	and	a, #0x07
	ld	l, #0x00
	add	a, -4 (ix)
	ld	-6 (ix), a
	ld	a, l
	adc	a, -3 (ix)
	ld	-5 (ix), a
	ld	hl, 4 (sp)
	ld	a, (hl)
	rlc	a
	rlc	a
	rlc	a
	rlc	a
	and	a, #0x0f
	ld	l, #0x00
	add	a, -6 (ix)
	ld	-4 (ix), a
	ld	a, l
	adc	a, -5 (ix)
	ld	-3 (ix), a
	ld	hl, 4 (sp)
	inc	hl
	ld	l, (hl)
	ld	h, #0x00
	ld	a, -4 (ix)
	ld	b, #0x00
	add	hl, ba
	ld	a, l
	ld	b, h
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
