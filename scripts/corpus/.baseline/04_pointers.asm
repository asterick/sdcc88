;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _walk
	.globl _swap_via_ptr
	.globl _deref2
	.globl _diff_p
	.globl _inc_p
	.globl _idx_set
	.globl _idx_i
	.globl _set_l
	.globl _get_l
	.globl _set_i
	.globl _get_i
	.globl _set_c
	.globl _get_c
	.globl _lp
	.globl _ip
	.globl _cp
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_cp::
	.ds 2
_ip::
	.ds 2
_lp::
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
; Function get_c
; ---------------------------------
_get_c::
	ld	a, (hl)
	ret
;	---------------------------------
; Function set_c
; ---------------------------------
_set_c::
	ld	(hl), a
	ret
;	---------------------------------
; Function get_i
; ---------------------------------
_get_i::
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	ret
;	---------------------------------
; Function set_i
; ---------------------------------
_set_i::
	ld	(hl), a
	inc	hl
	ld	(hl), b
	ret
;	---------------------------------
; Function get_l
; ---------------------------------
_get_l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
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
; Function set_l
; ---------------------------------
_set_l::
	push	ix
	ld	ix,sp
	push	hl
	pop	iy
	ld	hl, sp
	add	hl, #5
	ld	b, #0x04
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	pop	ix
	ret
;	---------------------------------
; Function idx_i
; ---------------------------------
_idx_i::
	add	a, a
	rl	b
	add	hl, ba
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	ret
;	---------------------------------
; Function idx_set
; ---------------------------------
_idx_set::
	push	ix
	ld	ix,sp
	push	iy
	add	a, a
	rl	b
	add	hl, ba
	ld	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -1 (ix)
	ld	(hl), a
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function inc_p
; ---------------------------------
_inc_p::
	inc	hl
	ld	b, h
	ld	a, l
	ret
;	---------------------------------
; Function diff_p
; ---------------------------------
_diff_p::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, l
	sub	a, -2 (ix)
	ld	l, a
	ld	a, h
	sbc	a, -1 (ix)
	ld	b, a
	sra	b
	ld	a, l
	rr	a
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function deref2
; ---------------------------------
_deref2::
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	ret
;	---------------------------------
; Function swap_via_ptr
; ---------------------------------
_swap_via_ptr::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	hl
	ld	6 (sp), hl
	ld	4 (sp), ba
	ld	hl, 6 (sp)
	ld	a, (hl)
	ld	-8 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-7 (ix), a
	ld	hl, 4 (sp)
	ld	a, (hl)
	ld	-6 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-5 (ix), a
	ld	hl, 6 (sp)
	ld	a, -6 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -5 (ix)
	ld	(hl), a
	ld	hl, 4 (sp)
	ld	a, -8 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -7 (ix)
	ld	(hl), a
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function walk
; ---------------------------------
_walk::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), hl
	ld	l, a
	ld	h, b
00101$:
	ld	a, l
	ld	b, h
	dec	hl
	or	a, b
	jrs	Z, 00104$
	push	hl
	ld	hl, 2 (sp)
	ld	(hl), #0x00
	pop	hl
	ld	a, -2 (ix)
	inc	a
	ld	-2 (ix), a
	jrs	NZ, 00101$
	ld	a, -1 (ix)
	inc	a
	ld	-1 (ix), a
	jrs	00101$
00104$:
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
