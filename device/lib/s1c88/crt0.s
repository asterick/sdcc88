;--------------------------------------------------------------------------
; crt0.s — production startup for the Epson S1C88 / Pokémon Mini (sdcc -ms1c88)
;
; Builds the real Pokémon Mini cartridge header and establishes the C runtime
; environment, then calls _main. Assembled to crt0.rel; the sdcc driver links
; it first (port->linker.crt = "crt0.rel").
;
; LAYOUT. The cartridge header is an ABSOLUTE area pinned at physical 0x2100 (cartridge
; byte 0; see docs/s1c88/banked-branch.md), so its watermarks and vector slots sit at
; exact addresses that the linker cannot move (robust to a future bjump-relaxation
; pass).  The actual code is the relocatable _CODE area, which the driver pins right
; after the header via code_loc (default 0x21D0) — the reset trampoline bjumps to
; __start there.  ABS areas carry their own address, so no extra -b is needed.
;
; ROM header (hardware/BIOS-checked fields):
;   0x2100  "PM"                      2-byte cartridge marker
;   0x2102  reset vector slot         6 bytes: bjump __start
;   0x2108  26 IRQ vector slots       26 x 6 bytes: bjump _irq_v<N>
;   0x21A4  "NINTENDO"                8-byte BIOS watermark (REQUIRED)
; Each slot is a `bjump` (the linker resolves the target's bank: `ld nb,#bank ;
; jrl`, or 3 nops + jrl for a common-bank target) — exactly 6 bytes, so the BIOS
; dispatch math (slot N at 0x2102 + 6*N) lands on each jump AND the handler can
; live in ANY bank.  On real hardware the BIOS reads the 0x0000-0x00FF vector
; table and routes IRQ N to its 0x2102+6*N slot; ROM is read-only, so handlers
; are NOT installed by writing low memory — each slot bjumps to the symbol
; `_irq_v<N>` (vector number N, see <pm.h> VEC_*), which YOU define:
;
;     void irq_v8(void) __interrupt { ... }   // a TIM0 handler (VEC_TIM0 = 8)
;
; Every `_irq_v<N>` MUST be defined.  The s1c88 runtime library provides a
; default (do-nothing RETE) for each as a separate module, so a program that
; doesn't use interrupts links fine and you only override the vectors you want.
; (A future `__interrupt(N)` auto-wiring / weak-default pass is planned.)
;
; Runtime contract (abi-decision.md): EP=XP=YP=0 (the EP=0 invariant, load-bearing for
; all near (hl)/(iy) access), __sdcc_fptr cell in near RAM for banked function-pointer
; dispatch, _INITIALIZER->_INITIALIZED copy. _DATA zero-init needs no loop: the BIOS
; clears all RAM at boot (and the emulator runner clears 0x0000-0x1FFF on load).
;--------------------------------------------------------------------------
	.module crt0
	.globl	_main
	.globl	_irq_default
	; the 26 maskable interrupt service routines (vector 1..26).  Referenced by
	; the header trampolines; YOU define each (the runtime lib defaults them).
	.globl	_irq_v1,  _irq_v2,  _irq_v3,  _irq_v4,  _irq_v5,  _irq_v6,  _irq_v7
	.globl	_irq_v8,  _irq_v9,  _irq_v10, _irq_v11, _irq_v12, _irq_v13, _irq_v14
	.globl	_irq_v15, _irq_v16, _irq_v17, _irq_v18, _irq_v19, _irq_v20, _irq_v21
	.globl	_irq_v22, _irq_v23, _irq_v24, _irq_v25, _irq_v26
	; linker-provided area bounds (for gsinit)
	.globl	s__INITIALIZER
	.globl	s__INITIALIZED
	.globl	l__INITIALIZER

	;----------------------------------------------------------------
	; Area order (relocatable code/data).  The cartridge header is a
	; separate ABS area (below) at fixed addresses; _CODE holds the actual
	; code and is pinned by the driver AFTER the header (code_loc, default
	; 0x21D0).  _HOME must chain into ROM (else library home-code -> RAM).
	;----------------------------------------------------------------
	.area	_CODE
	.area	_HOME			; codegen/library home code — must chain into ROM
	.area	_GSINIT
	.area	_GSFINAL
	.area	_INITIALIZER
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
	; Cartridge header — an ABSOLUTE area, so every watermark and vector
	; slot is pinned to an EXACT address (not merely contiguous 6-byte
	; bjumps in a relocatable area).  A future linker bjump-size-relaxation
	; pass operates on relocatable code and skips ABS areas, so these
	; addresses can never shift: the BIOS dispatch (slot N at 0x2102 + 6*N)
	; and the 0x21A4 NINTENDO check stay put no matter how the trampolines
	; are optimized.  Each `.org` hard-pins one element (and asserts its
	; address at assembly time).
	;================================================================
	.area	_HEADER (ABS)

	.org	0x2100
	.ascii	"PM"			; cartridge marker
	.org	0x2102 + 6 * 0
	bjump	__start			; vector 0 (reset) -> startup
	.org	0x2102 + 6 * 1
	bjump	_irq_v1
	.org	0x2102 + 6 * 2
	bjump	_irq_v2
	.org	0x2102 + 6 * 3
	bjump	_irq_v3
	.org	0x2102 + 6 * 4
	bjump	_irq_v4
	.org	0x2102 + 6 * 5
	bjump	_irq_v5
	.org	0x2102 + 6 * 6
	bjump	_irq_v6
	.org	0x2102 + 6 * 7
	bjump	_irq_v7
	.org	0x2102 + 6 * 8
	bjump	_irq_v8
	.org	0x2102 + 6 * 9
	bjump	_irq_v9
	.org	0x2102 + 6 * 10
	bjump	_irq_v10
	.org	0x2102 + 6 * 11
	bjump	_irq_v11
	.org	0x2102 + 6 * 12
	bjump	_irq_v12
	.org	0x2102 + 6 * 13
	bjump	_irq_v13
	.org	0x2102 + 6 * 14
	bjump	_irq_v14
	.org	0x2102 + 6 * 15
	bjump	_irq_v15
	.org	0x2102 + 6 * 16
	bjump	_irq_v16
	.org	0x2102 + 6 * 17
	bjump	_irq_v17
	.org	0x2102 + 6 * 18
	bjump	_irq_v18
	.org	0x2102 + 6 * 19
	bjump	_irq_v19
	.org	0x2102 + 6 * 20
	bjump	_irq_v20
	.org	0x2102 + 6 * 21
	bjump	_irq_v21
	.org	0x2102 + 6 * 22
	bjump	_irq_v22
	.org	0x2102 + 6 * 23
	bjump	_irq_v23
	.org	0x2102 + 6 * 24
	bjump	_irq_v24
	.org	0x2102 + 6 * 25
	bjump	_irq_v25
	.org	0x2102 + 6 * 26
	bjump	_irq_v26
	.org	0x2102 + 6 * 27		; = 0x21A4
	.ascii	"NINTENDO"		; BIOS watermark (required)

	;================================================================
	; Startup — in _CODE (relocatable), which the driver pins after the
	; header via code_loc (default 0x21D0).
	;================================================================
	.area	_CODE
__start::
	; SP is left as the BIOS set it on reset — crt0 does not touch the stack.
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
_irq_default::
	rete

	;================================================================
	; Static initialization: copy _INITIALIZER (ROM) -> _INITIALIZED
	; (RAM), then fall through the per-module _GSINIT code; _GSFINAL
	; holds the ret. (C zero-init: see header — the BIOS clears RAM.)
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
