;--------------------------------------------------------------------------
; crt0.s — production startup for the Epson S1C88 / Pokémon Mini (sdcc -ms1c88)
;
; Builds the real Pokémon Mini cartridge header and establishes the C runtime
; environment, then calls _main. Assembled to crt0.rel; the sdcc driver links
; it first (port->linker.crt = "crt0.rel").
;
; ROM header (cartridge byte 0 == physical 0x2100; see docs/s1c88/banked-branch.md
; bank table). Hardware/BIOS-checked fields:
;   0x2100  "PM"                      2-byte cartridge marker
;   0x2102  reset vector slot         6 bytes: ld nb,#page ; jrl __start
;   0x2108  26 IRQ vector slots       26 x 6 bytes -> _irq_default (RETE)
;   0x21A4  "NINTENDO"                8-byte BIOS watermark (REQUIRED)
; Each vector slot is exactly 6 bytes (ld nb,#bb = CE C4 bb, 3B; jrl = F3 rr qq,
; 3B) so the BIOS dispatch math (slot N at 0x2102 + 6*N) lands on each jump. On
; real hardware the BIOS reads the 0x0000-0x00FF vector table and routes IRQ N
; to its 0x2102+6*N slot; the optional secondary header tail (0x21BC) is dropped
; (unchecked by hardware).
;
; Runtime contract (abi-decision.md): EP=XP=YP=0 (the EP=0 invariant, load-bearing
; for all near (hl)/(iy) access), __sdcc_fptr cell in near RAM for banked
; function-pointer dispatch, _DATA zero-init + _INITIALIZER->_INITIALIZED copy.
;--------------------------------------------------------------------------
	.module crt0
	.globl	_main
	.globl	_irq_default
	; linker-provided area bounds (for gsinit)
	.globl	s__INITIALIZER
	.globl	s__INITIALIZED
	.globl	l__INITIALIZER

	;----------------------------------------------------------------
	; ROM area order (cart, bank 0 common area): header first, then code.
	;----------------------------------------------------------------
	.area	_HEADER
	.area	_HOME
	.area	_CODE
	.area	_GSINIT
	.area	_GSFINAL
	.area	_INITIALIZER
	;---- RAM area order ----
	.area	_DATA
	.area	_INITIALIZED

	;----------------------------------------------------------------
	; __sdcc_fptr — runtime contract: 2-byte near-RAM cell for the
	; banked function-pointer dispatch (`call (__sdcc_fptr)`).
	;----------------------------------------------------------------
	.area	_DATA
__sdcc_fptr::
	.ds	2

	;================================================================
	; Cartridge header @ 0x2100 (linker: -b _HEADER=0x2100)
	;================================================================
	.area	_HEADER
	.ascii	"PM"			; 0x2100 cartridge marker

	; 0x2102 reset vector slot -> __start (page 0 = common bank)
	ld	nb, #0
	jrl	__start

	; 0x2108 26 maskable IRQ vector slots -> _irq_default
	.rept	26
	ld	nb, #0
	jrl	_irq_default
	.endm

	.ascii	"NINTENDO"		; 0x21A4 BIOS watermark (required)

	;================================================================
	; Startup
	;================================================================
	.area	_HOME
__start::
	ld	sp, #0x1FF0		; stack top (near RAM; below the emu mailbox)
	ld	a, #0x00
	ld	ep, a			; the EP=0 invariant ...
	ld	xp, a			; ... near data/index pages all 0
	ld	yp, a

	; mask all maskable interrupts at boot (IRQ enable regs 0x2027-0x202A)
	ld	a, #0x00
	ld	(0x2027), a
	ld	(0x2028), a
	ld	(0x2029), a
	ld	(0x202A), a

	bcall	gsinit			; copy _INITIALIZER -> _INITIALIZED (RAM is already 0)
	bcall	_main			; C entry; exit code returns in BA

	; emu-test exit protocol (harmless RAM writes on real hardware):
	; store the BA return value + the 0xA5 done-magic, then halt.
	ld	hl, #0x1FFA
	ld	(hl), a
	inc	hl
	ld	(hl), b
	inc	hl
	ld	(hl), #0xA5
1$:
	halt
	jrs	1$			; stay halted if a wake event resumes us

	;----------------------------------------------------------------
	; Default IRQ handler — return-from-exception (restores SC).
	;----------------------------------------------------------------
	.area	_HOME
_irq_default::
	rete

	;================================================================
	; Static initialization: copy _INITIALIZER (ROM) -> _INITIALIZED
	; (RAM), then run the per-module _GSINIT code; _GSFINAL holds the
	; ret. The C zero-init guarantee for _DATA needs no work here: the
	; Pokémon Mini BIOS clears all RAM at boot (and the emulator runner
	; clears 0x0000-0x1FFF on load), so _DATA starts zeroed.
	;================================================================
	.area	_GSINIT
gsinit::
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
