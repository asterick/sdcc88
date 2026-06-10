;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _fcmp
	.globl _l2f
	.globl _f2l
	.globl _i2f
	.globl _f2i
	.globl _fdiv
	.globl _fmul
	.globl _fsub
	.globl _fadd
	.globl ___fslt
	.globl ___slong2fs
	.globl ___fs2slong
	.globl ___sint2fs
	.globl ___fs2sint
	.globl ___fsdiv
	.globl ___fsmul
	.globl ___fssub
	.globl ___fsadd
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
; Function fadd
; ---------------------------------
_fadd::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	iy, 11 (sp)
	push	iy
	ld	iy, 11 (sp)
	push	iy
	bcall	___fsadd
	pop	iy
	pop	iy
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fsub
; ---------------------------------
_fsub::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	iy, 11 (sp)
	push	iy
	ld	iy, 11 (sp)
	push	iy
	bcall	___fssub
	pop	iy
	pop	iy
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fmul
; ---------------------------------
_fmul::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	iy, 11 (sp)
	push	iy
	ld	iy, 11 (sp)
	push	iy
	bcall	___fsmul
	pop	iy
	pop	iy
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fdiv
; ---------------------------------
_fdiv::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	iy, 11 (sp)
	push	iy
	ld	iy, 11 (sp)
	push	iy
	bcall	___fsdiv
	pop	iy
	pop	iy
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function f2i
; ---------------------------------
_f2i::
	bjump	___fs2sint
	ret
;	---------------------------------
; Function i2f
; ---------------------------------
_i2f::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	bcall	___sint2fs
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function f2l
; ---------------------------------
_f2l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	bcall	___fs2slong
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function l2f
; ---------------------------------
_l2f::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	bcall	___slong2fs
	ld	0 (sp), ba
	ld	2 (sp), hl
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fcmp
; ---------------------------------
_fcmp::
	push	ix
	ld	ix,sp
	ld	iy, 7 (sp)
	push	iy
	ld	iy, 7 (sp)
	push	iy
	bcall	___fslt
	pop	hl
	pop	hl
	ld	b, #0x00
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
