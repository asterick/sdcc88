;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _crit_fn
	.globl _crit_block
	.globl _isr_heavy
	.globl _isr_calls
	.globl _isr_simple
	.globl _counter
	.globl _hw
	.globl _handler
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_hw::
	.ds 1
_counter::
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
; Function isr_simple
; ---------------------------------
_isr_simple::
	push	ba
	push	hl
	push	iy
	ld	a, ep
	push	a
	ld	ep, #0x00
	ld	hl, (_counter)
	inc	hl
	ld	(_counter), hl
	pop	a
	ld	ep, a
	pop	iy
	pop	hl
	pop	ba
	rete
;	---------------------------------
; Function isr_calls
; ---------------------------------
_isr_calls::
	push	ba
	push	hl
	push	iy
	ld	a, ep
	push	a
	ld	ep, #0x00
	ld	a, #0x01
	ld	iy, #_hw
	ld	0 (iy), a
	bcall	_handler
	pop	a
	ld	ep, a
	pop	iy
	pop	hl
	pop	ba
	rete
;	---------------------------------
; Function isr_heavy
; ---------------------------------
_isr_heavy::
	push	ba
	push	hl
	push	iy
	ld	a, ep
	push	a
	ld	ep, #0x00
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	hl, (_counter)
	ld	a, l
	ld	b, h
	add	hl, hl
	add	hl, ba
	ld	0 (sp), hl
	ld	a, (_hw+0)
	ld	-2 (ix), a
	xor	a, a
	ld	-1 (ix), a
	ld	a, -4 (ix)
	ld	hl, #_counter
	add	a, -2 (ix)
	ld	(hl), a
	ld	a, -3 (ix)
	adc	a, -1 (ix)
	inc	hl
	ld	(hl), a
	ld	sp, ix
	pop	ix
	pop	a
	ld	ep, a
	pop	iy
	pop	hl
	pop	ba
	rete
;	---------------------------------
; Function crit_block
; ---------------------------------
_crit_block::
	push	sc
	or	sc, #0xc0
	ld	hl, (_counter)
	inc	hl
	ld	(_counter), hl
	pop	sc
	ret
;	---------------------------------
; Function crit_fn
; ---------------------------------
_crit_fn::
	push	sc
	or	sc, #0xc0
	xor	a, a
	ld	(_hw+0), a
	ld	a, (_hw+0)
	ld	(_counter+0), a
	xor	a, a
	ld	(_counter+1), a
	pop	sc
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
