; crt0.asm — guest startup for emulator-based codegen tests (tests/emu).
;
; Linked FIRST so that (a) __start lands at physical 0x2100, where the runner forces
; PC after reset, and (b) the .area declaration order below fixes the link layout:
; ROM: _HOME(0x2100) _CODE _GSINIT _GSFINAL _INITIALIZER -- RAM: _DATA(0x1000) _INITIALIZED.
;
; Establishes the port's C environment invariants (abi-decision.md): EP=XP=YP=0
; (the EP=0 invariant is load-bearing for all near (hl)/(iy) data access), SP at
; 0x1FF0 (below the runner mailbox at 0x1FF8..0x1FFC), interrupts left masked
; (reset SC=0xC0). Copies _INITIALIZER -> _INITIALIZED, runs _GSINIT, calls _main,
; stores the BA return value + the 0xA5 done-magic for the runner, then halts.
	.module crt0
	.globl	_main
	; linker-provided area bounds
	.globl	s__DATA
	.globl	l__DATA
	.globl	s__INITIALIZER
	.globl	s__INITIALIZED
	.globl	l__INITIALIZER

	; ---- ROM area order ----
	.area	_HOME
	.area	_CODE
	.area	_GSINIT
	.area	_GSFINAL
	.area	_INITIALIZER
	; ---- RAM area order ----
	.area	_DATA
	.area	_INITIALIZED

	.area	_DATA
__sdcc_fptr::			; runtime contract: 2-byte cell for banked fn-ptr dispatch
	.ds	2

	.area	_HOME
__start::
	ld	sp, #0x1FF0	; stack top, below the runner mailbox
	ld	a, #0x00
	ld	ep, a		; the EP=0 invariant
	ld	xp, a
	ld	yp, a
	bcall	gsinit
	bcall	_main		; exit code returns in BA
	ld	hl, #0x1FFA	; mailbox: exit code (LE) ...
	ld	(hl), a
	inc	hl
	ld	(hl), b
	inc	hl
	ld	(hl), #0xA5	; ... + done magic @0x1FFC
	halt

	; ---- static initialization ----
	; zero _DATA (C zero-init guarantee), copy l__INITIALIZER bytes from
	; _INITIALIZER (ROM) to _INITIALIZED (RAM), then fall through into the
	; per-module _GSINIT code; _GSFINAL holds the ret.
	.area	_GSINIT
gsinit::
	ld	iy, #s__DATA
	ld	ix, #l__DATA
	ld	a, #0x00
3$:
	cp	ix, #0x0000
	jrs	z, 4$
	ld	(iy), a
	inc	iy
	dec	ix
	jrs	3$
4$:
	ld	hl, #s__INITIALIZER
	ld	iy, #s__INITIALIZED
	ld	ix, #l__INITIALIZER
1$:
	cp	ix, #0x0000
	jrs	z, 2$
	ld	a, (hl)
	ld	(iy), a
	inc	hl
	inc	iy
	dec	ix
	jrs	1$
2$:

	.area	_GSFINAL
	ret
