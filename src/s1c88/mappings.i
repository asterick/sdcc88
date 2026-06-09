static const ASM_MAPPING _asxxxx_z80_mapping[] = {
    /* We want to prepend the _ */
    { "area", ".area _%s" },
    { "areacode", ".area _%s" },
    { "areadata", ".area _%s" },
    { "areahome", ".area _%s" },
    { "*ixx", "%d (ix)" },
    { "*iyx", "%d (iy)" },
    { "*hl", "(hl)" },
    { "jphl", "jp hl" },
    { "di", "di" },
    { "ei", "ei" },
    { "ldahli",
      "ld a, (hl)\n"
      "inc\thl" },
    { "ldahld",
      "ld a, (hl)\n"
      "dec\thl" },
    { "lldahli",
      "ld (hl), a\n"
      "inc\thl" },
    { "lldahld",
      "ld (hl), a\n"
      "dec\thl" },
    { "ldahlsp",
      "ld hl, #%d\n"
      "add\thl, sp" },
    { "ldaspsp",
      "ld iy,#%d\n"
      "add\tiy,sp\n"
      "ld\tsp,iy" },
    { "mems", "(%s)" },
    /* S1C88 __far: the page byte (bits 16-23) of a 24-bit symbol address */
    { "bankimmeds", "#((%s) >> 16)" },
    { "enter",
      "push\tix\n"
      "ld\tix,sp" },
    { "enters",            /* S1C88: no enter helper in the runtime — inline */
      "push\tix\n"
      "ld\tix,sp" },
    { "pusha",
      "push af\n"
      "push\tbc\n"
      "push\tde\n"
      "push\thl\n"
      "push\tiy"
    },
    { "popa",
      "pop iy\n"
      "pop\thl\n"
      "pop\tde\n"
      "pop\tbc\n"
      "pop\taf"
    },
    { "adjustsp", "lda sp,-%d(sp)" },
    { "here", "." },
    { "optsdcc", ".optsdcc" },
    { NULL, NULL }
};

const ASM_MAPPINGS _s1c88_asxxxx_z80 = {
    &asm_asxxxx_mapping,
    _asxxxx_z80_mapping
};
