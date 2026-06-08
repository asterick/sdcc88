;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _tail_ptr
	.globl _dispatch
	.globl _reset_member
	.globl _call_member
	.globl _via_void
	.globl _via_ptr
	.globl _table
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_table::
	.ds 12
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
; Function via_ptr
; ---------------------------------
_via_ptr::
	push	ix
	ld	ix,sp
	.globl	__sdcc_fptr
	push	a
	ld	hl, 6 (sp)
	ld	a, 7 (ix)
	ld	(__sdcc_fptr), hl
	ld	nb, a
	pop	a
	call	(__sdcc_fptr)
	pop	ix
	ret
;	---------------------------------
; Function via_void
; ---------------------------------
_via_void::
	ld	hl, 3 (sp)
	push	hl
	ld	iy, sp
	add	iy, #5
	ld	a, 2 (iy)
	pop	hl
	ld	nb, a
	jp	hl
;	---------------------------------
; Function call_member
; ---------------------------------
_call_member::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	a
	push	hl
	ld	hl, iy
	ld	-2 (ix), l
	ld	-1 (ix), h
	pop	hl
	push	ba
	ld	iy, sp
	add	iy, #2
	ld	b, #0x03
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	pop	ba
	ld	hl, 3 (sp)
	.globl	__sdcc_fptr
	ld	iy, 0 (sp)
	ld	(__sdcc_fptr), iy
	push	a
	ld	a, -3 (ix)
	ld	nb, a
	pop	a
	call	(__sdcc_fptr)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function reset_member
; ---------------------------------
_reset_member::
	inc	hl
	inc	hl
	inc	hl
	ld	b, (hl)
	inc	hl
	inc	hl
	ld	a, (hl)
	dec	hl
	ld	h, (hl)
	ld	l, b
	ld	nb, a
	jp	hl
;	---------------------------------
; Function dispatch
; ---------------------------------
_dispatch::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	hl
	push	a
	ld	5 (sp), ba
	ld	3 (sp), hl
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	ld	l, a
	ld	h, b
	add	hl, hl
	add	hl, ba
	ld	a, #<(_table)
	ld	b, #>(_table)
	add	hl, ba
	ld	iy, sp
	add	iy, #0
	ld	b, #0x03
00103$:
	ld	a, (hl)
	ld	0 (iy), a
	inc	hl
	inc	iy
	djr	nz, 00103$
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	.globl	__sdcc_fptr
	push	a
	ld	hl, 1 (sp)
	ld	a, -5 (ix)
	ld	(__sdcc_fptr), hl
	ld	nb, a
	pop	a
	call	(__sdcc_fptr)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function tail_ptr
; ---------------------------------
_tail_ptr::
	push	ix
	ld	ix,sp
	.globl	__sdcc_fptr
	push	a
	ld	hl, 6 (sp)
	ld	a, 7 (ix)
	ld	(__sdcc_fptr), hl
	ld	nb, a
	pop	a
	call	(__sdcc_fptr)
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
