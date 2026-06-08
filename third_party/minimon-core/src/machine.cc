/*
ISC License

Copyright (c) 2019, Bryon Vandiver

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <stdint.h>

#include "machine.h"

extern "C" const char* get_version() {
	return "0.1.0";
}

extern "C" void cpu_reset(Machine::State& cpu) {
	// NB: the reset vector is read from the BIOS image, but with the LCD/blitter/
	// audio/RTC/etc. peripherals pruned the BIOS will not boot — the harness must
	// bypass it (the runner forces PC to the cartridge entry, 0x2100). Do not
	// rely on a clean reset-into-BIOS boot.
	cpu.reg.pc = cpu_read16(cpu, 0x0000);
	cpu.reg.sc = 0xC0;
	cpu.reg.ep = 0xFF;
	cpu.reg.xp = 0x00;
	cpu.reg.yp = 0x00;
	cpu.reg.nb = 0x01;

	trace_access(cpu, calc_pc(cpu), TRACE_INSTRUCTION | TRACE_BRANCH_TARGET);

	cpu.status = Machine::STATUS_NORMAL;
	cpu.osc1_overflow = 0;

	IRQ::reset(cpu);
	Timers::reset(cpu);
	Input::reset(cpu.input);
}

extern "C" void update_inputs(Machine::State& cpu, uint16_t value) {
	Input::update(cpu, value);
}

void cpu_clock(Machine::State& cpu, int cycles) {
	const int osc3 = cycles * OSC3_SPEED / CPU_SPEED;
	int osc1 = 0;

	cpu.osc1_overflow += osc3 * OSC1_SPEED;

	if (cpu.status <= Machine::STATUS_HALTED) {
		// derive elapsed OSC1 (32kHz) ticks so OSC1-sourced timers advance
		while (cpu.osc1_overflow >= OSC3_SPEED) {
			cpu.osc1_overflow -= OSC3_SPEED;
			osc1++;
		}

		Timers::clock(cpu, osc1, osc3);
	}

 	// OSC3 = 4mhz oscillator, OSC1 = 32khz oscillator
	cpu.clocks -= osc3;
}

extern "C" void cpu_step(Machine::State& cpu) {
	// We have an IRQ Scheduled
	IRQ::manage(cpu);

	// CPU Core steps
	if (cpu.status == Machine::STATUS_NORMAL) {
		trace_access(cpu, calc_pc(cpu), TRACE_INSTRUCTION);
		cpu_clock(cpu, inst_advance(cpu));
	} else {
		// Eat a cycle
		cpu_clock(cpu, 1);
	}
}

extern "C" void cpu_advance(Machine::State& cpu, int ticks) {
	cpu.clocks += ticks;

	while (cpu.clocks > 0) {
		cpu_step(cpu);
	}
}

static inline uint8_t cpu_read_reg(Machine::State& cpu, uint32_t address) {
	switch (address) {
	case 0x2020 ... 0x202A:
		return IRQ::read(cpu, address);
	case 0x2050 ... 0x2055:
		return Input::read(cpu.input, address);
	case 0x2010:
		// This should be handled properly
		return 0b010000;
	case 0x2018 ... 0x201D:
	case 0x2030 ... 0x203F:
	case 0x2048 ... 0x204F:
		return Timers::read(cpu, address);
	default:
		// open bus: unhandled register reads return the last bus value
		return cpu.bus_cap;
	}
}

static inline void cpu_write_reg(Machine::State& cpu, uint8_t data, uint32_t address) {
	switch (address) {
	case 0x2020 ... 0x202A:
		IRQ::write(cpu, data, address);
		break ;
	case 0x2050 ... 0x2055:
		Input::write(cpu.input, data, address);
		break ;
	case 0x2018 ... 0x201D:
	case 0x2030 ... 0x203F:
	case 0x2048 ... 0x204F:
		Timers::write(cpu, data, address);
		break ;
	default:
		// open bus: unhandled register writes are dropped
		break ;
	}
}

extern "C" uint8_t cpu_read(Machine::State& cpu, uint32_t address) {
	switch (address) {
		case 0x2000 ... 0x20FF:
			return cpu.bus_cap = cpu_read_reg(cpu, address);
		default:
			// Everything but the register window is one unified writable array:
			// the interrupt vector table (0x0000..), RAM (0x1000..0x1FFF), and
			// cartridge/far data (0x2100..). The BIOS is gone (it could not boot
			// with the peripherals pruned), so low memory is plain writable RAM —
			// which lets ISR tests install vectors at 2*N.
			return cpu.bus_cap = cpu.memory[address % sizeof(cpu.memory)];
	}
}

extern "C" void cpu_write(Machine::State& cpu, uint8_t data, uint32_t address) {
	cpu.bus_cap = data;

	switch (address) {
		case 0x2000 ... 0x20FF:
			cpu_write_reg(cpu, data, address);
			break ;
		default:
			cpu.memory[address % sizeof(cpu.memory)] = data;
			break ;
	}
}

/**
 * S1C88 Memory access helper functions
 **/


uint8_t cpu_read8(Machine::State& cpu, uint32_t address) {
	trace_access(cpu, address, TRACE_READ);
	return cpu.bus_cap = cpu_read(cpu, address);
}

void cpu_write8(Machine::State& cpu, uint8_t data, uint32_t address) {
	trace_access(cpu, address, TRACE_WRITE, data);
	cpu_write(cpu, cpu.bus_cap = data, address);
}

uint16_t cpu_read16(Machine::State& cpu, uint32_t address) {
	uint16_t lo = cpu_read8(cpu, address);
	address = ((address + 1) & 0xFFFF) | (address & 0xFF0000);
	return (cpu_read8(cpu, address) << 8) | lo;
}

void cpu_write16(Machine::State& cpu, uint16_t data, uint32_t address) {
	cpu_write8(cpu, (uint8_t) data, address);
	address = ((address + 1) & 0xFFFF) | (address & 0xFF0000);
	cpu_write8(cpu, data >> 8, address);
}

uint8_t cpu_imm8(Machine::State& cpu) {
	auto address = calc_pc(cpu);
	cpu.reg.pc++;

	trace_access(cpu, address, TRACE_IMMEDIATE);
	return cpu_read8(cpu, address);
}

uint16_t cpu_imm16(Machine::State& cpu) {
	uint8_t lo = cpu_imm8(cpu);
	return (cpu_imm8(cpu) << 8) | lo;
}

void cpu_push8(Machine::State& cpu, uint8_t t) {
	cpu_write8(cpu, t, --cpu.reg.sp);
}

uint8_t cpu_pop8(Machine::State& cpu) {
	return cpu_read8(cpu, cpu.reg.sp++);
}

void cpu_push16(Machine::State& cpu, uint16_t t) {
	cpu_push8(cpu, t >> 8);
	cpu_push8(cpu, (uint8_t)t);
}

uint16_t cpu_pop16(Machine::State& cpu) {
	uint16_t t = cpu_pop8(cpu);
	return (cpu_pop8(cpu) << 8) | t;
}
