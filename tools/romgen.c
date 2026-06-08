/*-------------------------------------------------------------------------
   romgen.c — turn the sdld88 Intel-HEX output into a flat Pokémon Mini .min ROM.

   C reimplementation of the former scripts/romgen.py (so the shipped toolchain has no Python
   dependency). Behaviour is identical.

   sdcc88 lays banked code at linker address (bank<<16)|logic (logic = the
   0x8000-0xFFFF window for banks >=1, or the 0x0000-0x7FFF common area for bank 0).
   The linker emits those 24-bit addresses in the .ihx via Intel-HEX extended-
   linear-address (type 04) records. We map each byte to its PHYSICAL cartridge
   address and write a flat image:

     bank 0 (common):  physical = logic                  (cart ROM uses 0x2100..0x7FFF)
     bank N (N>=1):    physical = N*0x8000 + (logic & 0x7FFF)

   __far DATA areas (task #9) use the PHYSICAL convention instead: the linker
   locates them at their true 24-bit physical address (so the codegen's EP page
   byte, (sym >> 16), is the address the data bus sees). Declare those ranges with
   --far=start-end (repeatable, hex ok); keep far-data banks disjoint from code.

   The .min file's byte 0 is physical 0x2100 (the cart header), so
   file_offset = physical - 0x2100.

     romgen in.ihx out.min [--far=start-end]...
-------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define CART_BASE 0x2100u
#define MAX_FAR   64

static struct { uint32_t lo, hi; } far_ranges[MAX_FAR];
static int n_far = 0;

/* parse two hex nibbles at p; returns the byte, advances *pp by 2 (caller checks bounds) */
static int hexbyte (const char *p)
{
  int hi, lo;
  char c;
  c = p[0];
  hi = (c >= '0' && c <= '9') ? c - '0'
     : (c >= 'a' && c <= 'f') ? c - 'a' + 10
     : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
  c = p[1];
  lo = (c >= '0' && c <= '9') ? c - '0'
     : (c >= 'a' && c <= 'f') ? c - 'a' + 10
     : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
  if (hi < 0 || lo < 0)
    return -1;
  return (hi << 4) | lo;
}

static int is_far (uint32_t a)
{
  int i;
  for (i = 0; i < n_far; i++)
    if (a >= far_ranges[i].lo && a <= far_ranges[i].hi)
      return 1;
  return 0;
}

int main (int argc, char **argv)
{
  const char *inpath = NULL, *outpath = NULL;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (strncmp (argv[i], "--far=", 6) == 0)
        {
          char *dash, *s = argv[i] + 6;
          if (n_far >= MAX_FAR) { fprintf (stderr, "romgen: too many --far ranges\n"); return 2; }
          dash = strchr (s, '-');
          if (!dash) { fprintf (stderr, "romgen: bad --far '%s' (want start-end)\n", argv[i]); return 2; }
          *dash = '\0';
          far_ranges[n_far].lo = (uint32_t) strtoul (s, NULL, 0);
          far_ranges[n_far].hi = (uint32_t) strtoul (dash + 1, NULL, 0);
          n_far++;
        }
      else if (!inpath)  inpath = argv[i];
      else if (!outpath) outpath = argv[i];
      else { fprintf (stderr, "romgen: unexpected argument '%s'\n", argv[i]); return 2; }
    }
  if (!inpath || !outpath)
    {
      fprintf (stderr, "usage: romgen in.ihx out.min [--far=start-end]...\n");
      return 2;
    }

  FILE *f = fopen (inpath, "r");
  if (!f) { perror (inpath); return 2; }

  unsigned char *img = NULL;
  size_t cap = 0, size = 0;   /* size = highest written offset + 1 */
  uint32_t hi = 0;            /* extended linear address (high 16 bits) */
  char line[600];             /* a hex record is <= 0xFF*2 + overhead */
  int rc = 0;
  static unsigned char bankhit[2048];   /* physical/0x8000 -> touched */

  while (fgets (line, sizeof line, f))
    {
      if (line[0] != ':')
        continue;
      int ln  = hexbyte (line + 1);
      int a1  = hexbyte (line + 3);
      int a0  = hexbyte (line + 5);
      int typ = hexbyte (line + 7);
      if (ln < 0 || a1 < 0 || a0 < 0 || typ < 0) { fprintf (stderr, "romgen: malformed record\n"); rc = 2; break; }
      unsigned addr = (unsigned) ((a1 << 8) | a0);

      if (typ == 0x04)               /* extended linear address */
        {
          int d1 = hexbyte (line + 9), d0 = hexbyte (line + 11);
          if (d1 < 0 || d0 < 0) { fprintf (stderr, "romgen: bad ELA record\n"); rc = 2; break; }
          hi = (uint32_t) ((d1 << 8) | d0);
        }
      else if (typ == 0x00)          /* data */
        {
          int j;
          for (j = 0; j < ln; j++)
            {
              int val = hexbyte (line + 9 + j * 2);
              if (val < 0) { fprintf (stderr, "romgen: bad data byte\n"); rc = 2; break; }
              uint32_t a = (hi << 16) | (addr + j);
              uint32_t phys;
              if (is_far (a))
                phys = a;            /* __far data: at its physical address */
              else
                {
                  uint32_t bank = a >> 16, logic = a & 0xFFFF;
                  /* Common-bank overflow guard (TODO #17): logic 0x8000-0xFFFF is
                     ALWAYS the CB-selected bank window, never the common bank
                     (0x0000-0x7FFF). Bank-0 content there means a non-banked area
                     (code / const data) outgrew the common bank — and near (2-byte)
                     pointers to that const data would silently address the wrong
                     bank. Fail loudly instead. Far code uses bank>=1 (linker addr
                     >= 0x10000); far data uses --far (is_far above). */
                  if (bank == 0 && logic >= 0x8000u)
                    {
                      fprintf (stderr,
                        "romgen: common-bank overflow — content at logic 0x%04x is past the\n"
                        "        common bank (0x2100-0x7FFF). Near pointers can't reach it; move\n"
                        "        code to a far bank (bcall/bjump) or const data to __far.\n",
                        logic);
                      rc = 2; break;
                    }
                  phys = (bank == 0) ? logic : bank * 0x8000u + (logic & 0x7FFF);
                }
              if (phys < CART_BASE)
                {
                  fprintf (stderr, "byte at phys 0x%06x is below the cart base 0x2100\n", phys);
                  rc = 2; break;
                }
              size_t off = phys - CART_BASE;
              if (off >= cap)
                {
                  size_t ncap = off + 1;
                  ncap += ncap / 2;          /* grow with headroom */
                  unsigned char *ni = realloc (img, ncap);
                  if (!ni) { fprintf (stderr, "romgen: out of memory\n"); rc = 2; break; }
                  memset (ni + cap, 0xFF, ncap - cap);   /* unused ROM = 0xFF */
                  img = ni; cap = ncap;
                }
              img[off] = (unsigned char) val;
              if (off + 1 > size) size = off + 1;
              { unsigned bank = (unsigned) (phys / 0x8000u); if (bank < 2048) bankhit[bank] = 1; }
            }
          if (rc) break;
        }
      else if (typ == 0x01)          /* EOF */
        break;
    }
  fclose (f);
  if (rc) { free (img); return rc; }

  FILE *o = fopen (outpath, "wb");
  if (!o) { perror (outpath); free (img); return 2; }
  if (size && fwrite (img, 1, size, o) != size) { perror (outpath); fclose (o); free (img); return 2; }
  fclose (o);

  int n_banks = 0, bi;
  for (bi = 0; bi < 2048; bi++)
    if (bankhit[bi]) n_banks++;

  printf ("wrote %s (%zu bytes, %d banks touched)\n", outpath, size, n_banks);
  free (img);
  return 0;
}
