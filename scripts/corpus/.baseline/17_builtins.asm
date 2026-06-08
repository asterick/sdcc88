;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _mc_wide
	.globl _mc_small
	.globl _sr_var
	.globl _sr_nul
	.globl _sr_lit
	.globl _sn_wide
	.globl _sn_loop
	.globl _sc_ret
	.globl _sc_plain
	.globl _ms_keep
	.globl _ms_wide
	.globl _ms_var
	.globl _ms_loop
	.globl _ms_tiny
	.globl _src
	.globl _buf
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_buf::
	.ds 600
_src::
	.ds 600
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
; Function ms_tiny
; ---------------------------------
_ms_tiny::
	ld	hl, #_buf
	ld	(hl), #0x55
	inc	hl
	ld	(hl), #0x55
	inc	hl
	ld	(hl), #0x55
	ret
;	---------------------------------
; Function ms_loop
; ---------------------------------
_ms_loop::
	ld	hl, #_buf
	ld	b, #0x28
00103$:
	ld	(hl), #0x00
	inc	hl
	djr	nz, 00103$
	ret
;	---------------------------------
; Function ms_var
; ---------------------------------
_ms_var::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	hl, #_buf
	ld	a, -2 (ix)
	ld	b, #0x07
00103$:
	ld	(hl), a
	inc	hl
	djr	nz, 00103$
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function ms_wide
; ---------------------------------
_ms_wide::
	ld	hl, #_buf
	push	ix
	ld	ix, #0x0258
00103$:
	ld	(hl), #0xaa
	inc	hl
	dec	ix
	jrs	NZ, 00103$
	pop	ix
	ret
;	---------------------------------
; Function ms_keep
; ---------------------------------
_ms_keep::
	ld	b, a
	ld	l, b
	xor	a, a
	push	b
	ld	a, l
	ld	hl, #_buf
	ld	b, #0x0a
00103$:
	ld	(hl), a
	inc	hl
	djr	nz, 00103$
	pop	b
	ld	a, b
	ret
;	---------------------------------
; Function sc_plain
; ---------------------------------
_sc_plain::
	ld	iy, #_buf
	ld	hl, #_src
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	or	a, a
	jrs	NZ, 00103$
	ret
;	---------------------------------
; Function sc_ret
; ---------------------------------
_sc_ret::
	ld	iy, #_buf
	ld	hl, #_src
	push	iy
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	or	a, a
	jrs	NZ, 00103$
	pop	hl
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function sn_loop
; ---------------------------------
_sn_loop::
	ld	iy, #_buf
	ld	hl, #_src
	ld	b, #0x0a
00103$:
	ld	a, (hl)
	inc	hl
	ld	0 (iy), a
	inc	iy
	or	a, a
	jrs	Z, 00104$
	djr	nz, 00103$
	ret
00104$:
	djr	nz, 00105$
	ret
00105$:
	ld	0 (iy), a
	inc	iy
	djr	nz, 00105$
	ret
;	---------------------------------
; Function sn_wide
; ---------------------------------
_sn_wide::
	ld	iy, #_buf
	ld	hl, #_src
	push	ix
	ld	ix, #0x012c
00103$:
	ld	a, (hl)
	inc	hl
	ld	0 (iy), a
	inc	iy
	or	a, a
	jrs	Z, 00104$
	dec	ix
	jrs	NZ, 00103$
	jrs	00106$
00104$:
	dec	ix
	jrs	Z, 00106$
00105$:
	ld	0 (iy), a
	inc	iy
	dec	ix
	jrs	NZ, 00105$
00106$:
	pop	ix
	ret
;	---------------------------------
; Function sr_lit
; ---------------------------------
_sr_lit::
	ld	hl, #_src
00104$:
	ld	a, (hl)
	cp	a, #0x78
	jrs	Z, 00103$
	inc	hl
	or	a, a
	jrs	NZ, 00104$
	ld	hl, #0x0000
00103$:
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function sr_nul
; ---------------------------------
_sr_nul::
	ld	hl, #_src
00104$:
	ld	a, (hl)
	cp	a, #0x00
	jrs	Z, 00103$
	inc	hl
	or	a, a
	jrs	NZ, 00104$
	ld	hl, #0x0000
00103$:
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function sr_var
; ---------------------------------
_sr_var::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	b, -2 (ix)
	ld	hl, #_src
00104$:
	ld	a, (hl)
	cp	a, b
	jrs	Z, 00103$
	inc	hl
	or	a, a
	jrs	NZ, 00104$
	ld	hl, #0x0000
00103$:
	ld	b, h
	ld	a, l
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function mc_small
; ---------------------------------
_mc_small::
	ld	iy, #_buf
	ld	hl, #_src
	ld	b, #0x10
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ret
;	---------------------------------
; Function mc_wide
; ---------------------------------
_mc_wide::
	ld	iy, #_buf
	ld	hl, #_src
	push	ix
	ld	ix, #0x012c
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	dec	ix
	jrs	NZ, 00103$
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
