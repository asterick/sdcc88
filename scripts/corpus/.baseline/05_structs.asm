;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _passp
	.globl _bget
	.globl _copyb
	.globl _readg
	.globl _dupp
	.globl _makep
	.globl _copyp
	.globl _sumxy
	.globl _setx
	.globl _getx
	.globl _gb
	.globl _gp
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_gp::
	.ds 4
_gb::
	.ds 14
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
; Function getx
; ---------------------------------
_getx::
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	ret
;	---------------------------------
; Function setx
; ---------------------------------
_setx::
	ld	(hl), a
	inc	hl
	ld	(hl), b
	ret
;	---------------------------------
; Function sumxy
; ---------------------------------
_sumxy::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	2 (sp), hl
	ld	hl, 2 (sp)
	ld	a, (hl)
	ld	-4 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-3 (ix), a
	ld	hl, 2 (sp)
	inc	hl
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	ld	a, l
	ld	b, h
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function copyp
; ---------------------------------
_copyp::
	push	hl
	pop	iy
	ld	l, a
	ld	h, b
	ld	b, #0x04
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ret
;	---------------------------------
; Function makep
; ---------------------------------
_makep::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	ld	6 (sp), ba
	ld	4 (sp), hl
	ld	a, -2 (ix)
	ld	-8 (ix), a
	ld	a, -1 (ix)
	ld	-7 (ix), a
	ld	a, -4 (ix)
	ld	-6 (ix), a
	ld	a, -3 (ix)
	ld	-5 (ix), a
	ld	hl, sp
	add	hl, #13
	ld	iy, (hl)
	ld	hl, sp
	add	hl, #0
	ld	b, #0x04
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function dupp
; ---------------------------------
_dupp::
	push	hl
	ld	hl, sp
	add	hl, #5
	ld	iy, (hl)
	pop	hl
	ld	b, #0x04
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ret
;	---------------------------------
; Function readg
; ---------------------------------
_readg::
	ld	hl, sp
	add	hl, #3
	ld	iy, (hl)
	ld	hl, #_gp
	ld	b, #0x04
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ret
;	---------------------------------
; Function copyb
; ---------------------------------
_copyb::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), hl
	ld	iy, 0 (sp)
	ld	hl, #_gb
	ld	b, #0x0e
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function bget
; ---------------------------------
_bget::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	add	hl, #0x000a
	ld	iy, sp
	add	iy, #0
	ld	b, #0x04
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function passp
; ---------------------------------
_passp::
	push	ix
	ld	ix,sp
	ld	hl, 7 (sp)
	inc	hl
	ld	7 (sp), hl
	ld	hl, sp
	add	hl, #5
	ld	iy, (hl)
	ld	hl, sp
	add	hl, #7
	ld	b, #0x04
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
