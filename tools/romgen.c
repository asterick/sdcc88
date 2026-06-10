/*-------------------------------------------------------------------------
   romgen.c — turn the sdld88 Intel-HEX output into a Pokémon Mini ROM.

   Two output formats:

     romgen in.ihx out.min  [--far=start-end]...
        Flat .min ROM (byte 0 = physical 0x2100), the format emulators and
        flash carts consume. C reimplementation of the former scripts/romgen.py
        (so the shipped toolchain has no Python dependency); behaviour identical.

     romgen in.ihx out.minx [--minx] [--far=start-end]...
            [--map=file] [--noi=file] [--cdb=file] [--embed=name=file]...
        MINX container (selected by the .minx output extension or --minx): a
        single sectioned binary — ELF-like — that rolls the ROM image, a parsed
        binary symbol table (from the linker's NoICE .noi), and the link-time
        debug artifacts (.map/.noi/.cdb, embedded verbatim) into one file.
        Sidecars are auto-discovered next to in.ihx (same stem); --map/--noi/
        --cdb override the path (and then must exist), --embed adds arbitrary
        extra sections. Format spec: docs/s1c88/minx-format.md.

   Address mapping (both formats): sdcc88 lays banked code at linker address
   (bank<<16)|logic (logic = the 0x8000-0xFFFF window for banks >=1, or the
   0x0000-0x7FFF common area for bank 0). The linker emits those 24-bit
   addresses in the .ihx via Intel-HEX extended-linear-address (type 04)
   records. We map each byte to its PHYSICAL cartridge address:

     bank 0 (common):  physical = logic                  (cart ROM uses 0x2100..0x7FFF)
     bank N (N>=1):    physical = N*0x8000 + (logic & 0x7FFF)

   __far DATA areas (task #9) use the PHYSICAL convention instead: the linker
   locates them at their true 24-bit physical address (so the codegen's EP page
   byte, (sym >> 16), is the address the data bus sees). Declare those ranges with
   --far=start-end (repeatable, hex ok); keep far-data banks disjoint from code.

   The ROM image's byte 0 is physical 0x2100 (the cart header), so
   file_offset = physical - 0x2100.
-------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define CART_BASE 0x2100u
#define MAX_FAR   64
#define MAX_EMBED 32

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

/* linker address -> physical cart address.
   Returns 0 ok, 1 below the cart base, 2 common-bank overflow (TODO #17:
   logic 0x8000-0xFFFF is ALWAYS the CB-selected bank window, never the common
   bank — bank-0 content there means a non-banked area outgrew the common bank,
   and near pointers to it would silently address the wrong bank). */
static int phys_of (uint32_t a, uint32_t *out)
{
  if (is_far (a))
    *out = a;                  /* __far data: at its physical address */
  else
    {
      uint32_t bank = a >> 16, logic = a & 0xFFFF;
      if (bank == 0 && logic >= 0x8000u)
        return 2;
      *out = (bank == 0) ? logic : bank * 0x8000u + (logic & 0x7FFF);
    }
  return (*out >= CART_BASE) ? 0 : 1;
}

/* ---------------- MINX container (see docs/s1c88/minx-format.md) ---------------- */

#define MINX_VERSION  1
#define MINX_HDRSIZE  32
#define MINX_ENTSIZE  32

enum minx_sec_type
{
  SEC_ROM    = 1,   /* flat ROM image (== the .min payload), addr = 0x2100 */
  SEC_SYMTAB = 2,   /* binary symbol records (16 bytes each)               */
  SEC_STRTAB = 3,   /* NUL-terminated strings; offset 0 = ""               */
  SEC_NOTE   = 4,   /* key=value metadata text                             */
  SEC_MAP    = 5,   /* linker .map, verbatim                               */
  SEC_NOI    = 6,   /* linker NoICE .noi, verbatim                         */
  SEC_CDB    = 7,   /* SDCC --debug .cdb, verbatim                         */
  SEC_USER   = 0x100 /* --embed sections: 0x100, 0x101, ... in CLI order   */
};

#define SYM_ROM   0x1   /* phys field valid (symbol maps into cart ROM) */
#define SYM_LOCAL 0x2   /* scoped symbol (a NoICE DEFS record)          */

struct minxsym { uint32_t name, value, phys, flags; };

/* growable byte buffer */
typedef struct { unsigned char *p; size_t len, cap; } buf;

static int buf_put (buf *b, const void *src, size_t n)
{
  if (b->len + n > b->cap)
    {
      size_t ncap = b->cap ? b->cap : 256;
      while (ncap < b->len + n) ncap += ncap / 2 + 16;
      unsigned char *np = realloc (b->p, ncap);
      if (!np) return -1;
      b->p = np; b->cap = ncap;
    }
  memcpy (b->p + b->len, src, n);
  b->len += n;
  return 0;
}

/* add a string to the string table; offset 0 is always "" */
static uint32_t strtab_add (buf *st, const char *s)
{
  uint32_t off;
  if (st->len == 0 && buf_put (st, "", 1) < 0) return 0;
  off = (uint32_t) st->len;
  if (buf_put (st, s, strlen (s) + 1) < 0) return 0;
  return off;
}

static uint32_t crc32_buf (const unsigned char *p, size_t n)
{
  uint32_t c = 0xFFFFFFFFu;
  size_t i; int k;
  for (i = 0; i < n; i++)
    {
      c ^= p[i];
      for (k = 0; k < 8; k++)
        c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
  return ~c;
}

/* whole file -> malloc'd buffer (binary); NULL if unreadable */
static unsigned char *read_file (const char *path, size_t *out_size)
{
  FILE *f = fopen (path, "rb");
  long n;
  unsigned char *p;
  if (!f) return NULL;
  if (fseek (f, 0, SEEK_END) != 0 || (n = ftell (f)) < 0 || fseek (f, 0, SEEK_SET) != 0)
    { fclose (f); return NULL; }
  p = malloc (n ? (size_t) n : 1);
  if (!p) { fclose (f); return NULL; }
  if (n && fread (p, 1, (size_t) n, f) != (size_t) n)
    { fclose (f); free (p); return NULL; }
  fclose (f);
  *out_size = (size_t) n;
  return p;
}

/* in.ihx -> in.map / in.noi / in.cdb (or just append when there's no .ihx suffix) */
static char *sidecar_path (const char *inpath, const char *ext)
{
  size_t n = strlen (inpath);
  size_t stem = (n > 4 && strcmp (inpath + n - 4, ".ihx") == 0) ? n - 4 : n;
  char *p = malloc (stem + strlen (ext) + 1);
  if (!p) return NULL;
  memcpy (p, inpath, stem);
  strcpy (p + stem, ext);
  return p;
}

/* parse NoICE DEF/DEFS records into symbol records (names go to the strtab).
   Lines look like "DEF _main 0x2196" (PagedAddress may prefix "bank:", which
   sdldz80 never does for this port — handled anyway). Other record types
   (FILE/FUNC/ENDF/LINE/...) stay available verbatim in the NOI section. */
static struct minxsym *parse_noi (const char *text, size_t textlen, buf *st, uint32_t *out_count)
{
  struct minxsym *syms = NULL;
  uint32_t count = 0, cap = 0;
  const char *p = text, *end = text + textlen;

  while (p < end)
    {
      const char *eol = memchr (p, '\n', (size_t) (end - p));
      const char *line = p;
      size_t linelen = eol ? (size_t) (eol - p) : (size_t) (end - p);
      p = eol ? eol + 1 : end;

      uint32_t flags = 0;
      if (linelen > 4 && memcmp (line, "DEF ", 4) == 0)
        line += 4, linelen -= 4;
      else if (linelen > 5 && memcmp (line, "DEFS ", 5) == 0)
        line += 5, linelen -= 5, flags |= SYM_LOCAL;
      else
        continue;

      const char *sp = memchr (line, ' ', linelen);
      if (!sp) continue;
      size_t namelen = (size_t) (sp - line);

      char valtok[64];
      size_t vlen = linelen - namelen - 1;
      if (vlen == 0 || vlen >= sizeof valtok) continue;
      memcpy (valtok, sp + 1, vlen);
      valtok[vlen] = '\0';
      /* strip trailing CR/space */
      while (vlen && (valtok[vlen-1] == '\r' || valtok[vlen-1] == ' ')) valtok[--vlen] = '\0';

      char *vp = valtok, *colon = strchr (valtok, ':');
      uint32_t value, bank = 0;
      if (colon) { *colon = '\0'; bank = (uint32_t) strtoul (vp, NULL, 16); vp = colon + 1; }
      value = (uint32_t) strtoul (vp, NULL, 16) | (bank << 16);

      char name[256];
      if (namelen == 0 || namelen >= sizeof name) continue;
      memcpy (name, line, namelen); name[namelen] = '\0';

      uint32_t phys;
      if (phys_of (value, &phys) == 0)
        flags |= SYM_ROM;
      else
        phys = 0xFFFFFFFFu;

      if (count == cap)
        {
          uint32_t ncap = cap ? cap * 2 : 64;
          struct minxsym *ns = realloc (syms, ncap * sizeof *ns);
          if (!ns) { free (syms); *out_count = 0; return NULL; }
          syms = ns; cap = ncap;
        }
      syms[count].name  = strtab_add (st, name);
      syms[count].value = value;
      syms[count].phys  = phys;
      syms[count].flags = flags;
      count++;
    }
  *out_count = count;
  return syms;
}

static int sym_cmp (const void *a, const void *b)
{
  const struct minxsym *x = a, *y = b;
  if (x->value != y->value) return x->value < y->value ? -1 : 1;
  return x->name < y->name ? -1 : x->name > y->name ? 1 : 0;
}

struct section
{
  uint32_t type, name, addr;
  const unsigned char *data;
  size_t size;
  uint32_t fileoff;
};

static void wle16 (unsigned char *p, uint32_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static void wle32 (unsigned char *p, uint32_t v) { wle16 (p, v & 0xFFFF); wle16 (p + 2, v >> 16); }

static int write_minx (const char *outpath, struct section *secs, int nsec, int strtab_index)
{
  unsigned char hdr[MINX_HDRSIZE], ent[MINX_ENTSIZE];
  static const unsigned char pad[4] = { 0, 0, 0, 0 };
  uint32_t off = MINX_HDRSIZE + (uint32_t) nsec * MINX_ENTSIZE;
  int i;

  for (i = 0; i < nsec; i++)
    {
      off = (off + 3u) & ~3u;
      secs[i].fileoff = off;
      off += (uint32_t) secs[i].size;
    }

  FILE *o = fopen (outpath, "wb");
  if (!o) { perror (outpath); return -1; }

  memset (hdr, 0, sizeof hdr);
  memcpy (hdr, "MINX", 4);
  wle16 (hdr + 4,  MINX_VERSION);
  wle16 (hdr + 6,  MINX_HDRSIZE);
  wle32 (hdr + 8,  (uint32_t) nsec);
  wle32 (hdr + 12, MINX_HDRSIZE);
  wle32 (hdr + 16, MINX_ENTSIZE);
  wle32 (hdr + 20, (uint32_t) strtab_index);
  wle32 (hdr + 24, off);                       /* total file size */
  wle32 (hdr + 28, 0);                         /* flags */
  if (fwrite (hdr, 1, sizeof hdr, o) != sizeof hdr) goto werr;

  for (i = 0; i < nsec; i++)
    {
      memset (ent, 0, sizeof ent);
      wle32 (ent + 0,  secs[i].type);
      wle32 (ent + 4,  secs[i].name);
      wle32 (ent + 8,  secs[i].fileoff);
      wle32 (ent + 12, (uint32_t) secs[i].size);
      wle32 (ent + 16, secs[i].addr);
      wle32 (ent + 20, crc32_buf (secs[i].data, secs[i].size));
      if (fwrite (ent, 1, sizeof ent, o) != sizeof ent) goto werr;
    }

  off = MINX_HDRSIZE + (uint32_t) nsec * MINX_ENTSIZE;
  for (i = 0; i < nsec; i++)
    {
      uint32_t aligned = (off + 3u) & ~3u;
      if (aligned != off && fwrite (pad, 1, aligned - off, o) != aligned - off) goto werr;
      if (secs[i].size && fwrite (secs[i].data, 1, secs[i].size, o) != secs[i].size) goto werr;
      off = aligned + (uint32_t) secs[i].size;
    }
  if (fclose (o) != 0) { perror (outpath); return -1; }
  return 0;
werr:
  perror (outpath);
  fclose (o);
  return -1;
}

/* ------------------------------------------------------------------------- */

static void usage (void)
{
  fprintf (stderr,
    "usage: romgen in.ihx out.min  [--far=start-end]...\n"
    "       romgen in.ihx out.minx [--minx] [--far=start-end]...\n"
    "              [--map=file] [--noi=file] [--cdb=file] [--embed=name=file]...\n");
}

int main (int argc, char **argv)
{
  const char *inpath = NULL, *outpath = NULL;
  const char *map_arg = NULL, *noi_arg = NULL, *cdb_arg = NULL;
  struct { const char *name, *path; } embeds[MAX_EMBED];
  int n_embed = 0, opt_minx = 0;
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
      else if (strcmp (argv[i], "--minx") == 0)            opt_minx = 1;
      else if (strncmp (argv[i], "--map=", 6) == 0)        map_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--noi=", 6) == 0)        noi_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--cdb=", 6) == 0)        cdb_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--embed=", 8) == 0)
        {
          char *eq = strchr (argv[i] + 8, '=');
          if (!eq || eq == argv[i] + 8 || !eq[1])
            { fprintf (stderr, "romgen: bad --embed '%s' (want name=file)\n", argv[i]); return 2; }
          if (n_embed >= MAX_EMBED) { fprintf (stderr, "romgen: too many --embed sections\n"); return 2; }
          *eq = '\0';
          embeds[n_embed].name = argv[i] + 8;
          embeds[n_embed].path = eq + 1;
          n_embed++;
        }
      else if (argv[i][0] == '-' && argv[i][1] == '-')
        { fprintf (stderr, "romgen: unknown option '%s'\n", argv[i]); usage (); return 2; }
      else if (!inpath)  inpath = argv[i];
      else if (!outpath) outpath = argv[i];
      else { fprintf (stderr, "romgen: unexpected argument '%s'\n", argv[i]); return 2; }
    }
  if (!inpath || !outpath)
    {
      usage ();
      return 2;
    }
  {
    size_t ol = strlen (outpath);
    if (ol > 5 && strcmp (outpath + ol - 5, ".minx") == 0)
      opt_minx = 1;
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
              switch (phys_of (a, &phys))
                {
                case 2:
                  /* Common-bank overflow guard (TODO #17): see phys_of(). Far code
                     uses bank>=1 (linker addr >= 0x10000); far data uses --far. */
                  fprintf (stderr,
                    "romgen: common-bank overflow — content at logic 0x%04x is past the\n"
                    "        common bank (0x2100-0x7FFF). Near pointers can't reach it; move\n"
                    "        code to a far bank (bcall/bjump) or const data to __far.\n",
                    a & 0xFFFF);
                  rc = 2; break;
                case 1:
                  fprintf (stderr, "byte at phys 0x%06x is below the cart base 0x2100\n", phys);
                  rc = 2; break;
                }
              if (rc) break;
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

  int n_banks = 0, bi;
  for (bi = 0; bi < 2048; bi++)
    if (bankhit[bi]) n_banks++;

  if (!opt_minx)
    {
      /* ---- flat .min ---- */
      FILE *o = fopen (outpath, "wb");
      if (!o) { perror (outpath); free (img); return 2; }
      if (size && fwrite (img, 1, size, o) != size) { perror (outpath); fclose (o); free (img); return 2; }
      fclose (o);
      printf ("wrote %s (%zu bytes, %d banks touched)\n", outpath, size, n_banks);
      free (img);
      return 0;
    }

  /* ---- MINX container ---- */
  struct section secs[8 + MAX_EMBED];
  int nsec = 0, strtab_index;
  buf strtab = { NULL, 0, 0 }, note = { NULL, 0, 0 }, symbin = { NULL, 0, 0 };
  unsigned char *mapdat = NULL, *noidat = NULL, *cdbdat = NULL;
  size_t mapsz = 0, noisz = 0, cdbsz = 0;
  struct minxsym *syms = NULL;
  uint32_t n_syms = 0;
  char tmp[256];

  /* sidecars: explicit paths must exist; auto-discovered ones are optional */
  struct { const char *arg, *ext, *what; unsigned char **dat; size_t *sz; } side[] = {
    { map_arg, ".map", "map", &mapdat, &mapsz },
    { noi_arg, ".noi", "noi", &noidat, &noisz },
    { cdb_arg, ".cdb", "cdb", &cdbdat, &cdbsz },
  };
  for (i = 0; i < 3; i++)
    {
      if (side[i].arg)
        {
          *side[i].dat = read_file (side[i].arg, side[i].sz);
          if (!*side[i].dat)
            { fprintf (stderr, "romgen: cannot read --%s file '%s'\n", side[i].what, side[i].arg); free (img); return 2; }
        }
      else
        {
          char *p = sidecar_path (inpath, side[i].ext);
          if (p) { *side[i].dat = read_file (p, side[i].sz); free (p); }
        }
    }

  secs[nsec].type = SEC_ROM;
  secs[nsec].name = strtab_add (&strtab, "rom");
  secs[nsec].addr = CART_BASE;
  secs[nsec].data = img;
  secs[nsec].size = size;
  nsec++;

  if (noidat)
    {
      syms = parse_noi ((const char *) noidat, noisz, &strtab, &n_syms);
      if (syms && n_syms)
        {
          qsort (syms, n_syms, sizeof *syms, sym_cmp);
          for (i = 0; i < (int) n_syms; i++)
            {
              unsigned char rec[16];
              wle32 (rec + 0,  syms[i].name);
              wle32 (rec + 4,  syms[i].value);
              wle32 (rec + 8,  syms[i].phys);
              wle32 (rec + 12, syms[i].flags);
              if (buf_put (&symbin, rec, sizeof rec) < 0)
                { fprintf (stderr, "romgen: out of memory\n"); free (img); return 2; }
            }
          secs[nsec].type = SEC_SYMTAB;
          secs[nsec].name = strtab_add (&strtab, "symtab");
          secs[nsec].addr = 0;
          secs[nsec].data = symbin.p;
          secs[nsec].size = symbin.len;
          nsec++;
        }
    }

  if (mapdat)
    {
      secs[nsec].type = SEC_MAP; secs[nsec].name = strtab_add (&strtab, "map");
      secs[nsec].addr = 0; secs[nsec].data = mapdat; secs[nsec].size = mapsz; nsec++;
    }
  if (noidat)
    {
      secs[nsec].type = SEC_NOI; secs[nsec].name = strtab_add (&strtab, "noi");
      secs[nsec].addr = 0; secs[nsec].data = noidat; secs[nsec].size = noisz; nsec++;
    }
  if (cdbdat)
    {
      secs[nsec].type = SEC_CDB; secs[nsec].name = strtab_add (&strtab, "cdb");
      secs[nsec].addr = 0; secs[nsec].data = cdbdat; secs[nsec].size = cdbsz; nsec++;
    }
  for (i = 0; i < n_embed; i++)
    {
      size_t esz;
      unsigned char *edat = read_file (embeds[i].path, &esz);
      if (!edat)
        { fprintf (stderr, "romgen: cannot read --embed file '%s'\n", embeds[i].path); free (img); return 2; }
      secs[nsec].type = SEC_USER + (uint32_t) i;
      secs[nsec].name = strtab_add (&strtab, embeds[i].name);
      secs[nsec].addr = 0; secs[nsec].data = edat; secs[nsec].size = esz; nsec++;
    }

  /* NOTE: build metadata text (no timestamps — output is deterministic) */
  {
    int n = snprintf (tmp, sizeof tmp,
                      "format=minx%d\ngenerator=romgen (sdcc88)\nsource=%s\n"
                      "cart-base=0x%04x\nrom-bytes=%zu\nbanks=%d\nsymbols=%u\n",
                      MINX_VERSION, inpath, CART_BASE, size, n_banks, n_syms);
    if (n < 0 || buf_put (&note, tmp, (size_t) n) < 0)
      { fprintf (stderr, "romgen: out of memory\n"); free (img); return 2; }
    for (i = 0; i < n_far; i++)
      {
        n = snprintf (tmp, sizeof tmp, "far=0x%x-0x%x\n", far_ranges[i].lo, far_ranges[i].hi);
        if (n < 0 || buf_put (&note, tmp, (size_t) n) < 0)
          { fprintf (stderr, "romgen: out of memory\n"); free (img); return 2; }
      }
  }
  secs[nsec].type = SEC_NOTE;
  secs[nsec].name = strtab_add (&strtab, "note");
  secs[nsec].addr = 0;
  secs[nsec].data = note.p;
  secs[nsec].size = note.len;
  nsec++;

  /* string table last: every section name is interned by now */
  strtab_index = nsec;
  secs[nsec].type = SEC_STRTAB;
  secs[nsec].name = strtab_add (&strtab, "strtab");
  secs[nsec].addr = 0;
  secs[nsec].data = strtab.p;
  secs[nsec].size = strtab.len;
  nsec++;

  if (write_minx (outpath, secs, nsec, strtab_index) != 0)
    { free (img); return 2; }

  printf ("wrote %s (%d sections: %zu ROM bytes, %d banks touched, %u symbols)\n",
          outpath, nsec, size, n_banks, n_syms);
  free (img);
  free (mapdat); free (noidat); free (cdbdat);
  free (syms); free (symbin.p); free (note.p); free (strtab.p);
  return 0;
}
