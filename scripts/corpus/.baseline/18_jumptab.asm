;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _keep_across
	.globl _dense_c
	.globl _dense
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
; Function dense
; ---------------------------------
_dense::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, -1 (ix)
	bit	a, #0x80
	jrs	NZ, 00109$
	ld	a, #0x07
	cp	a, -2 (ix)
	ld	a, #0x00
	sbc	a, -1 (ix)
	jrs	C, 00109$
	ld	a, -2 (ix)
	ld	b, #0x00
	ld	hl, #00126$
	add	hl, ba
	add	hl, ba
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	jp	hl
00126$:
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
	.dw	00106$
	.dw	00107$
	.dw	00108$
00101$:
	ld	ba, #0xa
	jrs	00110$
00102$:
	ld	ba, #0x15
	jrs	00110$
00103$:
	ld	ba, #0x21
	jrs	00110$
00104$:
	ld	ba, #0x2f
	jrs	00110$
00105$:
	ld	ba, #0x34
	jrs	00110$
00106$:
	ld	ba, #0x42
	jrs	00110$
00107$:
	ld	ba, #0x47
	jrs	00110$
00108$:
	ld	ba, #0x59
	jrs	00110$
00109$:
	ld	ba, #0xffff
00110$:
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function dense_c
; ---------------------------------
_dense_c::
	ld	b, a
	ld	a, #0x05
	sub	a, b
	jrs	C, 00107$
	ld	a, b
	ld	b, #0x00
	ld	hl, #00117$
	add	hl, ba
	add	hl, ba
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	jp	hl
00117$:
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
	.dw	00106$
00101$:
	ld	a, #0x61
	ret
00102$:
	ld	a, #0x62
	ret
00103$:
	ld	a, #0x63
	ret
00104$:
	ld	a, #0x64
	ret
00105$:
	ld	a, #0x65
	ret
00106$:
	ld	a, #0x66
	ret
00107$:
	ld	a, #0x3f
	ret
;	---------------------------------
; Function keep_across
; ---------------------------------
_keep_across::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	2 (sp), ba
	ld	0 (sp), hl
	ld	hl, #0x0000
	ld	a, -2 (ix)
	or	a, a
	or	a, -1 (ix)
	jrs	Z, 00101$
	ld	a, -2 (ix)
	dec	a
	or	a, -1 (ix)
	jrs	Z, 00102$
	ld	a, -2 (ix)
	sub	a, #0x02
	or	a, -1 (ix)
	jrs	Z, 00103$
	ld	a, -2 (ix)
	sub	a, #0x03
	or	a, -1 (ix)
	jrs	Z, 00104$
	ld	a, -2 (ix)
	sub	a, #0x04
	or	a, -1 (ix)
	jrs	Z, 00105$
	ld	a, -2 (ix)
	sub	a, #0x05
	or	a, -1 (ix)
	jrs	Z, 00106$
	jrs	00107$
00101$:
	ld	hl, #0x0001
	jrs	00107$
00102$:
	ld	hl, #0x0002
	jrs	00107$
00103$:
	ld	hl, #0x0003
	jrs	00107$
00104$:
	ld	hl, #0x0004
	jrs	00107$
00105$:
	ld	hl, #0x0005
	jrs	00107$
00106$:
	ld	hl, #0x0006
00107$:
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	ld	a, l
	ld	b, h
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
