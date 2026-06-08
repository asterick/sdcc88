;--------------------------------------------------------------------------
; bios.s — a minimal TEST-ONLY Pokémon Mini BIOS image for the emulator runner.
;
; The real PM BIOS validates the cart, sets the CPU into a known reset state,
; installs the low-memory IRQ forwarding vectors + its own service vectors, then
; enters the cartridge. The vendored emulator core has the peripheral BIOS pruned
; (it can't boot the real image), so this tiny stand-in does exactly the parts a
; cart depends on — and NOTHING test-specific leaks into the production crt0:
;
;   - establishes the documented BIOS reset register state (BA=0xFFFF, EP=XP=YP=0,
;     NB=0x01, SP parked) — so the runner no longer pokes CPU registers in C++;
;   - installs a SHUTDOWN routine and its vector at 0x0048, the hook the production
;     crt0 calls (`int (0x48)`) when main returns. The shutdown routine halts with
;     main's exit code still in BA, which the host runner reads off the register file;
;   - enters the cart through its reset trampoline at 0x2102 (`bjump __start`).
;
; The IRQ hardware->cart forwarding vectors (0x0000-0x003F) are still synthesized by
; the runner (they model the BIOS's permuted IRQ routing, not register state).
;
; Located by the runner build at a fixed low-ROM address (BIOS_LOAD); the runner
; loads these bytes there and forces PC to __bios after reset.
;--------------------------------------------------------------------------
	.module bios
	.area	_BIOS
__bios::
	; --- documented BIOS reset register state -------------------------------
	ld	a, #0x00
	ld	ep, a			; EP=0 (the near-pointer invariant)
	ld	xp, a			; XP=0
	ld	yp, a			; YP=0
	ld	ba, #0xFFFF		; BA=0xFFFF
	ld	nb, #0x01		; NB=0x01
	ld	sp, #0x1FF0		; park SP below the runner mailbox (0x1FF8)

	; --- install the shutdown service vector at 0x0048 ----------------------
	ld	hl, #__shutdown
	ld	(0x0048), hl

	; --- enter the cartridge via its reset trampoline (bjump __start) -------
	ld	hl, #0x2102
	jp	hl

	; --- shutdown service: reached via `int (0x48)` from the production crt0 -
	; when main returns. main's exit code is still in BA; just halt and let the
	; host read BA off the register file. (A real BIOS would power down here.)
__shutdown::
1$:
	halt
	jrs	1$
