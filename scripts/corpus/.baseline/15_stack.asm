;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _local_array_addr
	.globl _recurse
	.globl _stacked_long
	.globl _stacked_args
	.globl _many_locals
	.globl _sink
	.globl __mulint
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
; Function many_locals
; ---------------------------------
_many_locals::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	ld	12 (sp), hl
	push	iy
	pop	hl
	add	a, #0x01
	ld	-14 (ix), a
	ld	a, b
	adc	a, #0x00
	ld	-13 (ix), a
	ld	a, -2 (ix)
	add	a, #0x02
	ld	-12 (ix), a
	ld	a, -1 (ix)
	adc	a, #0x00
	ld	-11 (ix), a
	inc	hl
	inc	hl
	inc	hl
	ld	4 (sp), hl
	ld	hl, 2 (sp)
	ld	a, -14 (ix)
	ld	b, -13 (ix)
	bcall	__mulint
	ld	6 (sp), ba
	ld	hl, 4 (sp)
	ld	a, -12 (ix)
	ld	b, -11 (ix)
	bcall	__mulint
	ld	8 (sp), ba
	ld	hl, 0 (sp)
	ld	a, -10 (ix)
	ld	b, -9 (ix)
	bcall	__mulint
	ld	10 (sp), ba
	ld	a, -8 (ix)
	add	a, -6 (ix)
	ld	l, a
	ld	a, -7 (ix)
	adc	a, -5 (ix)
	ld	h, a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	ld	a, l
	ld	b, h
	bcall	_sink
	ld	l, a
	ld	h, b
	ld	a, -14 (ix)
	ld	b, -13 (ix)
	add	hl, ba
	ld	a, -12 (ix)
	ld	b, -11 (ix)
	add	hl, ba
	ld	a, -10 (ix)
	ld	b, -9 (ix)
	add	hl, ba
	ld	a, -8 (ix)
	ld	b, -7 (ix)
	add	hl, ba
	ld	a, -6 (ix)
	ld	b, -5 (ix)
	add	hl, ba
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	add	hl, ba
	ld	a, l
	ld	b, h
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function stacked_args
; ---------------------------------
_stacked_args::
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
	add	hl, ba
	ld	a, l
	add	a, -2 (ix)
	ld	-4 (ix), a
	ld	a, h
	adc	a, -1 (ix)
	ld	-3 (ix), a
	ld	a, -4 (ix)
	add	a, 5 (ix)
	ld	-6 (ix), a
	ld	a, -3 (ix)
	adc	a, 6 (ix)
	ld	-5 (ix), a
	ld	a, -6 (ix)
	add	a, 7 (ix)
	ld	-4 (ix), a
	ld	a, -5 (ix)
	adc	a, 8 (ix)
	ld	-3 (ix), a
	ld	a, -4 (ix)
	add	a, 9 (ix)
	ld	l, a
	ld	a, -3 (ix)
	adc	a, 10 (ix)
	ld	b, a
	ld	a, l
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function stacked_long
; ---------------------------------
_stacked_long::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	ld	8 (sp), ba
	ld	10 (sp), hl
	ld	a, -4 (ix)
	add	a, 5 (ix)
	ld	-12 (ix), a
	ld	a, -3 (ix)
	adc	a, 6 (ix)
	ld	-11 (ix), a
	ld	a, -2 (ix)
	adc	a, 7 (ix)
	ld	-10 (ix), a
	ld	a, -1 (ix)
	adc	a, 8 (ix)
	ld	-9 (ix), a
	ld	a, -12 (ix)
	add	a, 9 (ix)
	ld	-8 (ix), a
	ld	a, -11 (ix)
	adc	a, 10 (ix)
	ld	-7 (ix), a
	ld	a, -10 (ix)
	adc	a, 11 (ix)
	ld	-6 (ix), a
	ld	a, -9 (ix)
	adc	a, 12 (ix)
	ld	-5 (ix), a
	ld	a, -8 (ix)
	ld	b, -7 (ix)
	ld	hl, 6 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function recurse
; ---------------------------------
_recurse::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, #0x01
	cp	a, -2 (ix)
	ld	a, #0x00
	sbc	a, -1 (ix)
	jrs	LT, 00102$
	ld	ba, #0x1
	jrs	00103$
00102$:
	ld	hl, 0 (sp)
	dec	hl
	ld	a, l
	ld	b, h
	bcall	_recurse
	ld	l, a
	ld	h, b
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	bcall	__mulint
00103$:
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function local_array_addr
; ---------------------------------
_local_array_addr::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	push	hl
	ld	18 (sp), ba
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
00102$:
	ld	hl, 20 (sp)
	add	hl, hl
	push	hl
	ld	hl, sp
	add	hl, #2
	ld	a, l
	ld	b, h
	pop	hl
	add	hl, ba
	push	hl
	ld	a, -2 (ix)
	ld	b, #0x00
	bcall	_sink
	ld	18 (sp), ba
	pop	hl
	ld	a, -6 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -5 (ix)
	ld	(hl), a
	ld	a, -2 (ix)
	inc	a
	ld	-2 (ix), a
	ld	hl, 20 (sp)
	cp	hl, #0x0008
	jrs	C, 00102$
	ld	a, -4 (ix)
	and	a, #0x07
	ld	l, a
	ld	h, #0x00
	add	hl, hl
	ld	b, h
	ld	a, l
	ld	hl, sp
	add	hl, ba
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
