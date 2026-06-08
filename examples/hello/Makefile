# examples/hello — build a Pokémon Mini ROM (.min) with sdcc88.
#
#   make            build hello.min
#   make run        build + run on the bundled emulator (development)
#   make clean
#
# Point SDK at the built toolchain. The default assumes you built in-tree
# (scripts/setup-sdk.sh); override for an installed SDK, e.g.:
#   make SDK=/opt/sdcc88

SDK    ?= $(abspath ../../build/sdcc-4.5.0)
SDCC    = $(SDK)/src/sdcc
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
	@test -x $(RUNNER) || $(MAKE) -C $(abspath ../../tests/emu)
	$(RUNNER) $(NAME).min; echo "exit=$$?"

clean:
	rm -f $(NAME).ihx $(NAME).min $(NAME).asm $(NAME).rel \
	      $(NAME).lst $(NAME).sym $(NAME).map $(NAME).lk $(NAME).noi $(NAME).mem

.PHONY: all run clean
