# examples/hello — build a Pokémon Mini ROM (.min) with sdcc88.
#
#   make            build hello.min
#   make run        build + run on the bundled emulator (development)
#   make clean
#
# Point SDK at the toolchain. The default detects both homes of this Makefile:
# inside the repo (in-tree build under build/sdcc-4.5.0) and inside an unpacked
# SDK tarball (examples/hello two levels under the SDK root). Or override:
#   make SDK=/opt/sdcc88-sdk

SDK    ?= $(abspath $(if $(wildcard ../../bin/sdas88),../..,../../build/sdcc-4.5.0))
SDCC    = $(SDK)/bin/sdcc
ROMGEN  = $(SDK)/bin/romgen
RUNNER  = $(abspath ../../build/emu/runner)
export PATH := $(SDK)/bin:$(PATH)        # so the driver finds sdcpp/sdas88/sdldz80

NAME    = hello
CFLAGS  = -ms1c88 --opt-code-size

all: $(NAME).min

$(NAME).ihx: $(NAME).c
	$(SDCC) $(CFLAGS) $< -o $@

$(NAME).min: $(NAME).ihx
	$(ROMGEN) $< $@

run: $(NAME).min
	@test -x $(RUNNER) || test -d $(abspath ../../tests/emu) \
	  || { echo "no bundled emulator in the SDK package — run $(NAME).min on PokeMini"; exit 1; }
	@test -x $(RUNNER) || $(MAKE) -C $(abspath ../../tests/emu)
	$(RUNNER) $(NAME).min; echo "exit=$$?"

clean:
	rm -f $(NAME).ihx $(NAME).min $(NAME).asm $(NAME).rel \
	      $(NAME).lst $(NAME).sym $(NAME).map $(NAME).lk $(NAME).noi $(NAME).mem

.PHONY: all run clean
