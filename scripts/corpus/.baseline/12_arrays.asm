;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _find
	.globl _mcpyn
	.globl _mcpy16
	.globl _mcpy
	.globl _clear_buf
	.globl _fill
	.globl _sum_arr
	.globl _buf
	.globl _arr
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_arr::
	.ds 32
_buf::
	.ds 32
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
; Function sum_arr
; ---------------------------------
_sum_arr::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	hl, #0x0000
	ld	0 (sp), hl
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
00102$:
	ld	hl, 2 (sp)
	add	hl, hl
	ld	a, #<(_arr)
	ld	b, #>(_arr)
	add	hl, ba
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	add	a, -4 (ix)
	ld	-4 (ix), a
	ld	a, b
	adc	a, -3 (ix)
	ld	-3 (ix), a
	ld	a, -2 (ix)
	inc	a
	ld	-2 (ix), a
	ld	hl, 2 (sp)
	cp	hl, #0x0010
	jrs	C, 00102$
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fill
; ---------------------------------
_fill::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	2 (sp), ba
	ld	hl, #0x0000
00102$:
	ld	a, l
	ld	b, h
	add	a, a
	rl	b
	add	a, #<(_arr)
	ld	-4 (ix), a
	ld	a, b
	adc	a, #>(_arr)
	ld	-3 (ix), a
	push	hl
	ld	hl, 2 (sp)
	ld	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -1 (ix)
	ld	(hl), a
	pop	hl
	inc	hl
	cp	hl, #0x0010
	jrs	C, 00102$
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function clear_buf
; ---------------------------------
_clear_buf::
	ld	b, #0x00
00102$:
	ld	a, #<(_buf)
	add	a, b
	ld	l, a
	ld	a, #>(_buf)
	adc	a, #0x00
	ld	h, a
	ld	(hl), #0x00
	inc	b
	ld	a, b
	sub	a, #0x20
	jrs	C, 00102$
	ret
;	---------------------------------
; Function mcpy
; ---------------------------------
_mcpy::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	ld	2 (sp), hl
	push	ba
	ld	hl, iy
	ld	-2 (ix), l
	ld	-1 (ix), h
	pop	hl
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
	jrs	Z, 00104$
	ld	a, (hl)
	inc	hl
	push	hl
	ld	hl, 4 (sp)
	ld	(hl), a
	pop	hl
	ld	a, -4 (ix)
	inc	a
	ld	-4 (ix), a
	jrs	NZ, 00101$
	ld	a, -3 (ix)
	inc	a
	ld	-3 (ix), a
	jrs	00101$
00104$:
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function mcpy16
; ---------------------------------
_mcpy16::
	push	hl
	pop	iy
	ld	l, a
	ld	h, b
	ld	b, #0x10
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ret
;	---------------------------------
; Function mcpyn
; ---------------------------------
_mcpyn::
	push	ix
	ld	ix,sp
	push	iy
	push	ix
	ld	ix, 2 (sp)
	push	hl
	pop	iy
	ld	l, a
	ld	h, b
	cp	ix, #0x0000
	jrs	Z, 00104$
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	dec	ix
	jrs	NZ, 00103$
00104$:
	pop	ix
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function find
; ---------------------------------
_find::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	ld	6 (sp), hl
	ld	4 (sp), ba
	push	hl
	ld	hl, iy
	ld	-8 (ix), l
	ld	-7 (ix), h
	pop	hl
	ld	hl, #0x0000
	ld	0 (sp), hl
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
00105$:
	ld	a, -2 (ix)
	sub	a, -6 (ix)
	ld	a, -1 (ix)
	sbc	a, -5 (ix)
	jrs	GE, 00103$
	ld	hl, 8 (sp)
	add	hl, hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	a, -8 (ix)
	ld	b, -7 (ix)
	cp	hl, ba
	jrs	NZ, 00106$
	ld	a, -10 (ix)
	ld	b, -9 (ix)
	jrs	00107$
00106$:
	ld	a, -2 (ix)
	inc	a
	ld	-2 (ix), a
	jrs	NZ, 00132$
	ld	a, -1 (ix)
	inc	a
	ld	-1 (ix), a
00132$:
	ld	a, -2 (ix)
	ld	-10 (ix), a
	ld	a, -1 (ix)
	ld	-9 (ix), a
	jrs	00105$
00103$:
	ld	ba, #0xffff
00107$:
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
