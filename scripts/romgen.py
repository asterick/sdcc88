#!/usr/bin/env python3
# romgen.py — turn the sdld88 Intel-HEX output into a flat Pokémon Mini .min ROM.
#
# sdcc88 lays banked code at linker address (bank<<16)|logic (logic = the 0x8000-0xFFFF window for
# banks >=1, or the 0x0000-0x7FFF common area for bank 0).  The linker emits those 24-bit addresses in
# the .ihx via Intel-HEX extended-linear-address (type 04) records.  This maps each byte to its
# PHYSICAL cartridge address and writes a flat image:
#
#   bank 0 (common):  physical = logic                       (cart ROM uses 0x2100..0x7FFF)
#   bank N (N>=1):    physical = N*0x8000 + (logic & 0x7FFF)
#
# The .min file's byte 0 is physical 0x2100 (the cart header), so file_offset = physical - 0x2100.
#
#   romgen.py in.ihx out.min
import sys

def main():
    if len(sys.argv) != 3:
        sys.exit("usage: romgen.py in.ihx out.min")
    hi = 0           # extended linear address (high 16 bits)
    mem = {}         # file_offset -> byte
    for line in open(sys.argv[1]):
        line = line.strip()
        if not line.startswith(':'):
            continue
        b = bytes.fromhex(line[1:])
        ln, addr, typ = b[0], (b[1] << 8) | b[2], b[3]
        data = b[4:4+ln]
        if typ == 0x04:                      # extended linear address
            hi = (data[0] << 8) | data[1]
        elif typ == 0x00:                    # data
            for i, val in enumerate(data):
                a = (hi << 16) | (addr + i)
                bank, logic = a >> 16, a & 0xFFFF
                phys = logic if bank == 0 else bank * 0x8000 + (logic & 0x7FFF)
                off = phys - 0x2100
                if off < 0:
                    sys.exit("byte at phys 0x%06x is below the cart base 0x2100" % phys)
                mem[off] = val
        elif typ == 0x01:                    # EOF
            break
    size = max(mem) + 1 if mem else 0
    img = bytearray(b'\xff' * size)          # unused ROM = 0xFF
    for off, val in mem.items():
        img[off] = val
    open(sys.argv[2], 'wb').write(img)
    print("wrote %s (%d bytes, %d banks touched)" %
          (sys.argv[2], size, len({(0x2100 + o) // 0x8000 for o in mem})))

if __name__ == "__main__":
    main()
