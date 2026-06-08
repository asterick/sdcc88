;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _use_va
	.globl _printf_like
	.globl _sum_va
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
; Function sum_va
; ---------------------------------
_sum_va::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	ld	hl, sp
	add	hl, #13
	ld	0 (sp), hl
	xor	a, a
	ld	-4 (ix), a
	ld	-3 (ix), a
	ld	a, 5 (ix)
	ld	-2 (ix), a
	ld	a, 6 (ix)
	ld	-1 (ix), a
00101$:
	ld	b, -2 (ix)
	ld	l, -1 (ix)
	ld	a, -2 (ix)
	add	a, #0xff
	ld	-2 (ix), a
	ld	a, -1 (ix)
	adc	a, #0xff
	ld	-1 (ix), a
	ld	a, -2 (ix)
	ld	5 (ix), a
	ld	a, -1 (ix)
	ld	6 (ix), a
	ld	a, l
	or	a, b
	jrs	Z, 00108$
	ld	hl, 0 (sp)
	ld	b, (hl)
	inc	hl
	ld	l, (hl)
	ld	a, b
	add	a, -4 (ix)
	ld	-4 (ix), a
	ld	a, l
	adc	a, -3 (ix)
	ld	-3 (ix), a
	ld	a, -6 (ix)
	add	a, #0x02
	ld	-6 (ix), a
	jrs	NC, 00101$
	ld	a, -5 (ix)
	inc	a
	ld	-5 (ix), a
	jrs	00101$
00108$:
	ld	a, -2 (ix)
	ld	5 (ix), a
	ld	a, -1 (ix)
	ld	6 (ix), a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function use_va
; ---------------------------------
_use_va::
	ld	hl, #0x0003
	push	hl
	ld	l, #0x02
	push	hl
	ld	l, #0x01
	push	hl
	ld	hl, #___str_0
	push	hl
	bcall	_printf_like
	add	sp, #8
	ret
___str_0:
	.ascii "x"
	.db 0x00
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
