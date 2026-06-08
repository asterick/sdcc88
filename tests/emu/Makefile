# Builds the native test runner from the vendored minimon emulator core + runner.cc.
# Objects and the generated opcode table land in build/emu/ (gitignored).
CORE     := $(abspath $(CURDIR)/../../third_party/minimon-core)
BUILD    := $(abspath $(CURDIR)/../../build/emu)
RUNNER   := $(BUILD)/runner

SOURCES  := $(wildcard $(CORE)/src/*.cc)
OBJECTS  := $(patsubst $(CORE)/src/%.cc,$(BUILD)/%.o,$(SOURCES)) $(BUILD)/runner.o
# The core headers (machine.h in particular) define the shared Machine::State
# layout that EVERY translation unit compiles against. They are NOT auto-tracked
# by the pattern rules below, so list them as an explicit prerequisite: editing a
# header (e.g. adding a struct field) must rebuild ALL objects together, or stale
# objects keep an old struct layout and read/write fields at the wrong offset
# (this silently broke HALT once — different .o files disagreed on cpu.status's
# offset). See HANDOFF.md.
HEADERS  := $(wildcard $(CORE)/include/*.h) $(CORE)/table.h
CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -I$(CORE)/include -I$(BUILD)

# The test BIOS (tests/emu/bios.s) is assembled + linked at a fixed low-ROM
# address and embedded into the runner as a byte array (bios_rom.h, generated in
# $(BUILD)). It establishes the documented reset state + the 0x0048 shutdown
# vector so the runner pokes no CPU registers and every cart boots like hardware.
SDAS     := $(abspath $(CURDIR)/../../build/sdcc-4.5.0/bin/sdas88)
SDLD     := $(abspath $(CURDIR)/../../build/sdcc-4.5.0/bin/sdldz80)
BIOS_LOAD := 0x0050
BIOS_ROM := $(BUILD)/bios_rom.h

all: $(RUNNER)

$(BUILD):
	mkdir -p $(BUILD)

# assemble + link the BIOS, then turn the Intel-hex into a C byte array.
$(BIOS_ROM): bios.s | $(BUILD)
	$(SDAS) -o $(BUILD)/bios.rel bios.s
	$(SDLD) -nwxi -b _BIOS=$(BIOS_LOAD) $(BUILD)/bios.ihx $(BUILD)/bios.rel
	awk 'BEGIN{lo=-1;hi=0} /^:/{ \
	      ll=strtonum("0x" substr($$0,2,2)); a=strtonum("0x" substr($$0,4,4)); \
	      t=substr($$0,8,2); if(t!="00")next; if(lo<0||a<lo)lo=a; \
	      for(i=0;i<ll;i++){b[a+i]=substr($$0,10+i*2,2); if(a+i>=hi)hi=a+i+1} } \
	    END{ printf "static const unsigned BIOS_LOAD = 0x%04X;\n", lo; \
	         printf "static const unsigned char bios_rom[] = {"; \
	         for(x=lo;x<hi;x++) printf "0x%s,", (x in b)?b[x]:"00"; \
	         printf "};\n" }' $(BUILD)/bios.ihx > $@

# the opcode dispatch table (included as "../table.h" by operations.cc) is
# GENERATED but checked in, pinned at the vendored version — regenerate with
# `python3 tools/table.py > table.h` inside third_party/minimon-core if the
# tools/ inputs ever change
$(BUILD)/%.o: $(CORE)/src/%.cc $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/runner.o: runner.cc $(HEADERS) $(BIOS_ROM) | $(BUILD)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(RUNNER): $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS)

clean:
	rm -rf $(BUILD)

.PHONY: all clean
