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
; table and routes a hardware IRQ to the CART slot the BIOS forwards it to (the
; permuted map on https://www.pokemon-mini.net/documentation/bios/); ROM is
; read-only, so handlers are NOT installed by writing low memory — each slot
; bjumps to the symbol `_irq_v<N>` (N = CART vector slot, see <pm.h> VEC_*),
; which YOU define.  The easy way is `__interrupt(N)` auto-wiring: an ISR with an
; explicit cart-slot number compiles its entry as `_irq_v<N>` automatically:
;
;     void on_tim1(void) __interrupt(VEC_TIM1_LO_UF) { ... }   // -> _irq_v6 (slot 6)
;
; (Equivalently you may still hand-name the function `irq_v6`.)  The s1c88 runtime
; library provides a default (do-nothing RETE) for every slot as a separate
; module, so a program that doesn't use interrupts links fine and you only
; override the vectors you want.
;
; Runtime contract (abi-decision.md): EP=XP=YP=0 (the EP=0 invariant, load-bearing for
; all near (hl)/(iy) access), __sdcc_fptr cell in near RAM for banked function-pointer
; dispatch, _INITIALIZER->_INITIALIZED copy. _DATA zero-init needs no loop: the BIOS
; clears all RAM at boot (and the emulator runner clears 0x0000-0x1FFF on load).
;
; BIOS RESET-STATE CONTRACT (assume this for any future change here). The Pokémon Mini
; BIOS enters the cartridge with a known register + peripheral state, so crt0 does NOT
; re-establish what the BIOS already guarantees:
;   - CPU registers: every register is 0 EXCEPT BA = 0xFFFF and NB = CB = 0x01.
;     (So EP=XP=YP=0 already; we still set EP=0 explicitly below as cheap insurance,
;      because it is the load-bearing near-pointer invariant.)
;   - SP: the BIOS parks it; crt0 leaves it untouched.
;   - MMIO: left exactly as the BIOS reset configured it — interrupts arrive masked
;     (SC=0xC0, IRQ-enable regs 0x2027-0x202A clear), so crt0 re-inits NO MMIO.
;--------------------------------------------------------------------------
	.module crt0
	.globl	_main
	; the 26 maskable ISR symbols (_irq_v1.._irq_v26) are declared .globl + bjumped
	; by the header trampoline loop below (one `.irp`), not enumerated here.
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
	bjump	__start			; slot 0 (reset) -> startup
	; slots 1..26 -> _irq_v1 .. _irq_v26.  The `'n` operator pastes the .irp
	; value n onto `_irq_v` to build each symbol name (declared + referenced),
	; so the 26 vectors are never enumerated by hand; the per-slot `.org` keeps
	; every slot pinned to its exact address (0x2102 + 6*n).  YOU define each
	; _irq_v<N> (or via `__interrupt(N)`); the runtime lib defaults each one to
	; its own standalone do-nothing RETE.
	.irp	n,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26
	.globl	_irq_v'n
	.org	0x2102 + 6 * n
	bjump	_irq_v'n
	.endm
	.org	0x2102 + 6 * 27		; = 0x21A4
	.ascii	"NINTENDO"		; BIOS watermark (required)

	;================================================================
	; Startup — in _CODE (relocatable), which the driver pins after the
	; header via code_loc (default 0x21D0).
	;================================================================
	.area	_CODE
__start::
	; Per the BIOS reset-state contract (see header): SP is parked by the BIOS
	; and MMIO is already in its reset configuration (interrupts masked), so crt0
	; touches neither. The only thing it pins is the load-bearing EP=0 near-pointer
	; invariant — already 0 from the BIOS, set explicitly as cheap insurance.
	ld	a, #0x00
	ld	ep, a			; the EP=0 invariant ...
	ld	xp, a			; ... near data/index pages all 0
	ld	yp, a

	bcall	gsinit			; copy _INITIALIZER -> _INITIALIZED (RAM is already 0)
	bcall	_main			; C entry; exit code returns in BA

	; main returned — hand off to the BIOS shutdown service via the software
	; interrupt vector at 0x0048 (the BIOS installs it). BA still holds main's
	; return value; the shutdown routine decides what to do with it (power down
	; on hardware; on the dev-emulator it halts and the host reads BA). This
	; keeps ALL exit/test plumbing in the BIOS, never in the production runtime.
	int	(0x48)
1$:
	halt				; defensive: shutdown should not return
	jrs	1$

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
