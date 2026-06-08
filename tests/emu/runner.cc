// runner.cc — native host runner that wraps the minimon.js emulator core as a codegen
// test harness for the sdcc88 S1C88 port.
//
// The (pruned) emulator core in third_party/minimon-core is compiled natively and
// linked with this file, which provides the host glue the core expects
// (get_machine / debug_print) plus a tiny test protocol:
//
//   - the .min ROM (romgen.py output; byte 0 = physical 0x2100) is loaded at
//     memory[0x2100] (RAM + cartridge are one unified writable array), the BIOS is
//     bypassed (PC forced to 0x2100 where crt0 lives), and the CPU is stepped
//     instruction-by-instruction.
//   - guest mailbox (top of RAM, kept above the 0x1FF0 stack top set by crt0.asm):
//       0x1FF8       char-out: guest stores a nonzero byte, host prints + clears it
//                    (host polls between every instruction, so one store per char
//                    can never be lost)
//       0x1FFA/0x1FFB exit code (little-endian BA, stored by crt0 from main's return)
//       0x1FFC       0xA5 magic: crt0 sets it right before `halt`, so a halt without
//                    it is a stray halt, not a clean exit
//   - `halt` (STATUS_HALTED) + magic -> process exit code = guest exit code.
//     Crash / stray halt / step-budget timeout -> distinct exit codes 124..126.
//
// usage: runner rom.min [--max-steps=N] [--verbose]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "machine.h"

// ---- host glue the core links against ----
extern "C" Machine::State* const get_machine() {
	static Machine::State machine_state;
	return &machine_state;
}

extern "C" void debug_print(const void* data) {
	// the core's dprintf() formats into a C string and hands it here
	fprintf(stderr, "[emu] %s\n", (const char*)data);
}

// ---- test protocol addresses (mirror tests/emu/crt0.asm + emu.h) ----
static const uint32_t MAILBOX_CHAR = 0x1FF8;
static const uint32_t MAILBOX_EXIT = 0x1FFA;	// ..0x1FFB, little-endian
static const uint32_t MAILBOX_MAGIC = 0x1FFC;
static const uint8_t  MAGIC_DONE = 0xA5;

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

	// reset, then bypass the real (pruned) BIOS. Two boot conventions, auto-detected:
	//
	//  (a) Production crt0: a real Pokémon Mini cartridge header with the "PM" marker
	//      at 0x2100. We act as a minimal BIOS — synthesize the 0x0000-0x00FF interrupt
	//      vector table from the cart's 6-byte jump slots (vector N's handler address =
	//      slot N at 0x2102 + 6*N), then enter via the reset vector (word at 0x0000 =
	//      the reset slot 0x2102, which runs `ld nb,#0 ; jrl __start`). This is what the
	//      real BIOS does after validating the cart, so the production startup + ISR
	//      dispatch run exactly as on hardware.
	//
	//  (b) Test crt0 (tests/emu/crt0.asm): code lives directly at 0x2100 (no "PM"
	//      marker). PC is forced there. Kept for the existing emu cases.
	//
	// Memory is always accessible (control has no enable side effects).
	cpu_reset(m);
	m.reg.cb = 0;
	m.reg.nb = 0;
	if (m.memory[0x2100] == 'P' && m.memory[0x2101] == 'M') {
		for (int v = 0; v < 27; v++) {		// reset + 26 maskable IRQ slots
			uint32_t slot = 0x2102 + 6 * v;
			m.memory[2 * v]     = slot & 0xFF;
			m.memory[2 * v + 1] = (slot >> 8) & 0xFF;
		}
		m.reg.pc = m.memory[0] | (m.memory[1] << 8);	// reset vector (= 0x2102)
		if (verbose) fprintf(stderr, "[emu] PM cart: vectors synthesized, entry via reset slot 0x%04X\n", m.reg.pc);
	} else {
		m.reg.pc = 0x2100;	// test crt0: code at the cartridge base
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
	}
	fflush(stdout);

	if (m.status == Machine::STATUS_HALTED && ram_at(m, MAILBOX_MAGIC) == MAGIC_DONE) {
		int code = ram_at(m, MAILBOX_EXIT) | (ram_at(m, MAILBOX_EXIT + 1) << 8);
		if (verbose) { fprintf(stderr, "[emu] clean exit, code=%d\n", code); dump_regs(m, steps); }
		return code > 123 ? 123 : code;	// keep clear of the harness codes below
	}
	if (m.status == Machine::STATUS_HALTED) {
		fprintf(stderr, "[emu] STRAY HALT (no exit magic)\n"); dump_regs(m, steps); return 124;
	}
	if (m.status != Machine::STATUS_NORMAL) {
		fprintf(stderr, "[emu] CPU CRASHED (undefined op?)\n"); dump_regs(m, steps); return 125;
	}
	fprintf(stderr, "[emu] TIMEOUT after %llu steps\n", (unsigned long long)steps);
	dump_regs(m, steps);
	return 126;
}
