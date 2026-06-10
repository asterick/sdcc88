// runner.cc — native host runner that wraps the minimon.js emulator core as a codegen
// test harness for the sdcc88 S1C88 port.
//
// The (pruned) emulator core in third_party/minimon-core is compiled natively and
// linked with this file, which provides the host glue the core expects
// (get_machine) plus a tiny test protocol:
//
//   - the .min ROM (romgen output; byte 0 = physical 0x2100) is loaded at
//     memory[0x2100] (RAM + cartridge are one unified writable array). The real
//     peripheral BIOS is pruned, so a tiny embedded test BIOS (tests/emu/bios.s) is
//     loaded into low ROM; PC is forced to it. It sets the documented reset register
//     state, installs the 0x0048 shutdown vector, and enters the cart — like hardware.
//   - host I/O mailbox (top of RAM, above the 0x1FF0 stack the BIOS parks): the test
//     CASES use these via emu.h; they are not runtime/crt0 code:
//       0x1FF4/0x1FF5 key-input request value (little-endian 10-bit keypad state)
//       0x1FF6       key-input "go" flag: guest stores nonzero, host applies the
//                    0x1FF4 value via update_inputs() (which raises any K0x/K1x edge
//                    IRQ) and clears the flag. Polled between every instruction, so
//                    a press requested from inside an ISR lands on the next step —
//                    this is how the nested-IRQ case preempts a running handler.
//       0x1FF8       char-out: guest stores a nonzero byte, host prints + clears it
//                    (host polls between every instruction, so one store per char
//                    can never be lost)
//   - volatile-probe MMIO register (machine.cc, in the 0x2000-0x20FF reg window):
//       0x2070       a side-effecting byte for testing that codegen honours
//                    `volatile`: a READ returns a counter then post-increments it,
//                    a WRITE seeds it. N volatile reads => N consecutive values, so
//                    a dropped/merged/hoisted/reordered volatile access diverges
//                    from the host model (tests/diff/cases/volatile.c).
//   - exit: the production crt0 calls the BIOS shutdown vector (`int (0x48)`) when
//     main returns; the shutdown routine halts with main's return value still in BA.
//     The host reads the exit code off BA on halt — there is NO exit RAM mailbox, so
//     no test/exit plumbing lives in the runtime. (There is one crt0 for everything.)
//   Crash / stray halt / step-budget timeout -> distinct exit codes 124..126.
//
// usage: runner rom.min [--max-steps=N] [--verbose]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "machine.h"
#include "bios_rom.h"	// generated: the test BIOS blob + BIOS_LOAD (see Makefile + bios.s)

// ---- host glue the core links against ----
extern "C" Machine::State* const get_machine() {
	static Machine::State machine_state;
	return &machine_state;
}

// ---- host I/O mailbox addresses (mirror emu.h) ----
// These are host-polled RAM cells the test CASES use (via emu.h); they are not
// runtime/crt0 code. The exit code is NOT a mailbox: it is main's return value,
// left in BA, read off the register file on halt (see below).
static const uint32_t MAILBOX_KEYS = 0x1FF4;	// ..0x1FF5, little-endian keypad state
static const uint32_t MAILBOX_KEYS_GO = 0x1FF6;	// guest sets nonzero -> host applies keys
static const uint32_t MAILBOX_CHAR = 0x1FF8;

// RAM and cartridge share one flat array now (m.memory), indexed by address.
static uint8_t ram_at(Machine::State& m, uint32_t addr) { return m.memory[addr]; }

static void dump_regs(Machine::State& m, uint64_t steps) {
	CPU::State& r = m.reg;
	fprintf(stderr,
		"[emu] steps=%llu pc=%02x:%04x sp=%04x ba=%04x hl=%04x ix=%04x iy=%04x "
		"sc=%02x ep=%02x xp=%02x yp=%02x nb=%02x br=%02x\n",
		(unsigned long long)steps, r.cb, r.pc, r.sp, r.ba(), r.hl(), r.ix, r.iy,
		r.sc, r.ep, r.xp, r.yp, r.nb, r.br);
}

int main(int argc, char** argv) {
	const char* rom_path = nullptr;
	uint64_t max_steps = 100000000ull;	// ~1e8 instructions; plenty for unit tests
	bool verbose = false;

	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--max-steps=", 12)) max_steps = strtoull(argv[i] + 12, nullptr, 0);
		else if (!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v")) verbose = true;
		else if (argv[i][0] == '-') { fprintf(stderr, "unknown option: %s\n", argv[i]); return 2; }
		else rom_path = argv[i];
	}
	if (!rom_path) {
		fprintf(stderr, "usage: %s rom.min [--max-steps=N] [--verbose]\n", argv[0]);
		return 2;
	}

	Machine::State& m = *get_machine();

	// load the .min image at its physical base (file offset 0 == phys 0x2100)
	FILE* f = fopen(rom_path, "rb");
	if (!f) { perror(rom_path); return 2; }
	memset(m.memory, 0xFF, sizeof(m.memory));	// unprogrammed ROM reads 0xFF
	memset(m.memory, 0x00, 0x2000);		// low memory (interrupt vectors) + RAM power on clear
	size_t n = fread(m.memory + 0x2100, 1, sizeof(m.memory) - 0x2100, f);
	fclose(f);
	if (n == 0) { fprintf(stderr, "%s: empty ROM\n", rom_path); return 2; }
	if (verbose) fprintf(stderr, "[emu] loaded %zu bytes at phys 0x2100\n", n);

	// reset, then bypass the real (pruned) BIOS. A real Pokémon Mini cartridge has
	// the "PM" marker at 0x2100; we act as the BIOS for it (below). We synthesize the
	// 0x0000-0x00FF interrupt vector table from the cart's 6-byte jump slots (vector
	// N's handler address = slot N at 0x2102 + 6*N) — this is the BIOS IRQ routing,
	// not register state — then hand off to the embedded test BIOS, which sets the
	// CPU/boot state and enters the cart. (A non-"PM" image is booted bare at 0x2100
	// as a fallback; nothing we build produces one anymore.)
	//
	// Memory is always accessible (control has no enable side effects).
	cpu_reset(m);
	const bool is_pm = (m.memory[0x2100] == 'P' && m.memory[0x2101] == 'M');
	if (is_pm) {
		// The PM BIOS does NOT identity-map hardware IRQs to cart vector slots: it
		// forwards a permuted subset (gaps where a hardware IRQ has no cart vector:
		// $01/$02 NMI, $11/$12 unused, $13 cart-eject = BIOS-handled). cart2hw[c] is
		// the hardware IRQ number serviced by cart slot c (0..26); we install cart
		// slot c's trampoline address (0x2102 + 6*c) at hardware vector 2*cart2hw[c],
		// exactly as the BIOS routes it. (https://www.pokemon-mini.net/documentation/bios/)
		static const int cart2hw[27] = {
			0x00,                                           // 0  reset
			0x03, 0x04,                                     // 1,2  PRC copy/frame
			0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,             // 3..8  Timer2/1/3
			0x0B, 0x0C, 0x0D, 0x0E,                         // 9..12 32/8/2/1 Hz
			0x0F, 0x10,                                     // 13,14 IR / shock
			0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, // 15..22 power/right/left/down/up/C/B/A
			0x1D, 0x1E, 0x1F,                               // 23..25 (no cart description on the BIOS page)
			0x14,                                           // 26  cartridge IRQ
		};
		for (int c = 0; c < 27; c++) {
			uint32_t slot = 0x2102 + 6 * c;
			int hw = cart2hw[c];
			m.memory[2 * hw]     = slot & 0xFF;
			m.memory[2 * hw + 1] = (slot >> 8) & 0xFF;
		}
		// Boot through the embedded TEST BIOS (tests/emu/bios.s) instead of
		// poking CPU registers here. Loaded into low ROM, it establishes the
		// documented reset state in real S1C88 code (BA=0xFFFF, EP=XP=YP=0,
		// NB=0x01, SP parked), installs the 0x0048 shutdown vector, and enters
		// the cart via its reset trampoline (0x2102) — exactly like hardware. MMIO
		// is left in the cpu_reset config (interrupts masked: SC=0xC0 + enables 0),
		// which the production crt0 trusts. Forced PC = the BIOS entry.
		memcpy(m.memory + BIOS_LOAD, bios_rom, sizeof(bios_rom));
		m.reg.pc = BIOS_LOAD;
		if (verbose) fprintf(stderr, "[emu] PM cart: entering via test BIOS @0x%04X, vectors synthesized\n", BIOS_LOAD);
	} else {
		// Fallback: a non-"PM" image — boot bare at the cartridge base with the plain
		// reset bank state. (Unused by our suites now that every case is a PM cart.)
		m.reg.cb = 0;
		m.reg.nb = 0;
		m.reg.pc = 0x2100;
	}

	uint64_t steps = 0;
	while (m.status == Machine::STATUS_NORMAL && steps < max_steps) {
		cpu_step(m);
		m.clocks = 0;	// stepping manually; keep the budget counter from underflowing
		steps++;

		uint8_t c = ram_at(m, MAILBOX_CHAR);
		if (c) {
			putchar(c);
			m.memory[MAILBOX_CHAR] = 0;
		}

		// key-input request: the guest drives a keypad change (and any K0x/K1x edge
		// IRQ it implies) by writing the 16-bit value + a nonzero go flag. Applying
		// it here, between instructions, is what lets a press requested from inside a
		// running ISR preempt it on the next step (nested-IRQ coverage).
		if (ram_at(m, MAILBOX_KEYS_GO)) {
			uint16_t keys = ram_at(m, MAILBOX_KEYS) | (ram_at(m, MAILBOX_KEYS + 1) << 8);
			update_inputs(m, keys);
			m.memory[MAILBOX_KEYS_GO] = 0;
		}
	}
	fflush(stdout);

	if (m.status == Machine::STATUS_HALTED) {
		// The production crt0 calls the BIOS shutdown vector (`int (0x48)`) when main
		// returns; the BIOS shutdown routine halts with main's return value still in
		// BA (the ABI return register). Read the exit code straight off BA — there is
		// no RAM exit mailbox, so no test/exit plumbing lives in the runtime.
		(void)is_pm;
		int code = m.reg.ba();
		if (verbose) { fprintf(stderr, "[emu] clean exit (BA), code=%d\n", code); dump_regs(m, steps); }
		return code > 123 ? 123 : code;	// keep clear of the harness codes below
	}
	if (m.status != Machine::STATUS_NORMAL) {
		fprintf(stderr, "[emu] CPU CRASHED (undefined op?)\n"); dump_regs(m, steps); return 125;
	}
	fprintf(stderr, "[emu] TIMEOUT after %llu steps\n", (unsigned long long)steps);
	dump_regs(m, steps);
	return 126;
}
