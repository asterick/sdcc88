# Builds the native test runner from the vendored minimon emulator core + runner.cc.
# Objects and the generated opcode table land in build/emu/ (gitignored).
CORE     := $(abspath $(CURDIR)/../../third_party/minimon-core)
BUILD    := $(abspath $(CURDIR)/../../build/emu)
RUNNER   := $(BUILD)/runner

SOURCES  := $(wildcard $(CORE)/src/*.cc)
OBJECTS  := $(patsubst $(CORE)/src/%.cc,$(BUILD)/%.o,$(SOURCES)) $(BUILD)/runner.o
CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -I$(CORE)/include

all: $(RUNNER)

$(BUILD):
	mkdir -p $(BUILD)

# the opcode dispatch table (included as "../table.h" by operations.cc) is
# GENERATED but checked in, pinned at the vendored version — regenerate with
# `python3 tools/table.py > table.h` inside third_party/minimon-core if the
# tools/ inputs ever change
$(BUILD)/%.o: $(CORE)/src/%.cc $(CORE)/table.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/runner.o: runner.cc | $(BUILD)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(RUNNER): $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS)

clean:
	rm -rf $(BUILD)

.PHONY: all clean
