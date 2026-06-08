;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _setbits
	.globl _neg_l
	.globl _neg_i
	.globl _lnot
	.globl _lor
	.globl _land
	.globl _or_l
	.globl _and_l
	.globl _xor_i
	.globl _or_i
	.globl _and_i
	.globl _not_c
	.globl _xor_c
	.globl _or_c
	.globl _and_c
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
; Function and_c
; ---------------------------------
_and_c::
	ld	b, l
	and	a, b
	ret
;	---------------------------------
; Function or_c
; ---------------------------------
_or_c::
	ld	b, l
	or	a, b
	ret
;	---------------------------------
; Function xor_c
; ---------------------------------
_xor_c::
	ld	b, l
	xor	a, b
	ret
;	---------------------------------
; Function not_c
; ---------------------------------
_not_c::
	cpl	a
	ret
;	---------------------------------
; Function and_i
; ---------------------------------
_and_i::
	push	b
	ld	b, l
	and	a, b
	pop	b
	ld	l, a
	ld	a, b
	push	b
	ld	b, h
	and	a, b
	pop	b
	ld	b, a
	ld	a, l
	ret
;	---------------------------------
; Function or_i
; ---------------------------------
_or_i::
	push	b
	ld	b, l
	or	a, b
	pop	b
	ld	l, a
	ld	a, b
	push	b
	ld	b, h
	or	a, b
	pop	b
	ld	b, a
	ld	a, l
	ret
;	---------------------------------
; Function xor_i
; ---------------------------------
_xor_i::
	push	b
	ld	b, l
	xor	a, b
	pop	b
	ld	l, a
	ld	a, b
	push	b
	ld	b, h
	xor	a, b
	pop	b
	ld	b, a
	ld	a, l
	ret
;	---------------------------------
; Function and_l
; ---------------------------------
_and_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	and	a, 5 (ix)
	ld	-4 (ix), a
	ld	a, b
	and	a, 6 (ix)
	ld	-3 (ix), a
	ld	a, l
	and	a, 7 (ix)
	ld	-2 (ix), a
	ld	a, h
	and	a, 8 (ix)
	ld	-1 (ix), a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function or_l
; ---------------------------------
_or_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	or	a, 5 (ix)
	ld	-4 (ix), a
	ld	a, b
	or	a, 6 (ix)
	ld	-3 (ix), a
	ld	a, l
	or	a, 7 (ix)
	ld	-2 (ix), a
	ld	a, h
	or	a, 8 (ix)
	ld	-1 (ix), a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function land
; ---------------------------------
_land::
	or	a, b
	jrs	Z, 00103$
	ld	a, h
	push	b
	ld	b, l
	or	a, b
	pop	b
	jrs	NZ, 00104$
00103$:
	ld	l, #0x00
	jrs	00105$
00104$:
	ld	l, #0x01
00105$:
	ld	b, #0x00
	ld	a, l
	ret
;	---------------------------------
; Function lor
; ---------------------------------
_lor::
	or	a, b
	jrs	NZ, 00104$
	ld	a, h
	push	b
	ld	b, l
	or	a, b
	pop	b
	ld	l, #0x00
	jrs	Z, 00105$
00104$:
	ld	l, #0x01
00105$:
	ld	b, #0x00
	ld	a, l
	ret
;	---------------------------------
; Function lnot
; ---------------------------------
_lnot::
	ld	l, b
	ld	b, a
	ld	a, l
	or	a, b
	sub	a, #0x01
	ld	a, #0x00
	rl	a
	ld	b, #0x00
	ret
;	---------------------------------
; Function neg_i
; ---------------------------------
_neg_i::
	neg	a
	ld	l, a
	sbc	a, a
	sub	a, b
	ld	b, a
	ld	a, l
	ret
;	---------------------------------
; Function neg_l
; ---------------------------------
_neg_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	neg	a
	ld	-4 (ix), a
	ld	a, #0x00
	sbc	a, b
	ld	-3 (ix), a
	ld	a, #0x00
	push	b
	ld	b, l
	sbc	a, b
	pop	b
	ld	-2 (ix), a
	sbc	a, a
	push	b
	ld	b, h
	sub	a, b
	pop	b
	ld	-1 (ix), a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function setbits
; ---------------------------------
_setbits::
	ld	a, b
	or	a, #0x0f
	ld	l, #0x00
	ld	b, a
	ld	a, l
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
