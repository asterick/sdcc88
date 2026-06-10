/*-------------------------------------------------------------------------
   romgen.c — turn the sdld88 Intel-HEX output into a Pokémon Mini ROM.

   Two output formats:

     romgen in.ihx out.min  [--far=start-end]...
        Flat .min ROM (byte 0 = physical 0x2100), the format emulators and
        flash carts consume. C reimplementation of the former scripts/romgen.py
        (so the shipped toolchain has no Python dependency); behaviour identical.

     romgen in.ihx out.minx [--minx] [--far=start-end]...
            [--map=file] [--noi=file] [--cdb=file]
            [--srcdir=dir]... [--no-src] [--embed=name=file]...
        MINX container (selected by the .minx output extension or --minx): a
        single chunk-tree binary built for a debugger that has NO filesystem
        access and parses NO structured text. romgen does all the text parsing
        on the host — the linker's .noi (symbols), .map (areas), and sdcc
        --debug .cdb (line/function records) — and promotes everything to
        binary tables: a sorted global line table, function extents, symbols,
        memory areas, and the source files themselves embedded for display.
        Sidecars are auto-discovered next to in.ihx (same stem); --map/--noi/
        --cdb override the path (and then must exist). Source files are found
        next to in.ihx / the cwd / each --srcdir. Format spec:
        docs/s1c88/minx-format.md; reference reader: tools/minxdump.c.

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

#define CART_BASE  0x2100u
#define MAX_FAR    64
#define MAX_EMBED  32
#define MAX_SRCDIR 16

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

/* ---------------- MINX container (see docs/s1c88/minx-format.md) ----------------

   16-byte header, then a flat sequence of chunks. Chunk = 4-byte ASCII id +
   u32 payload size + payload, padded to 4 (pad not counted in size, but part
   of the enclosing container/file). Container chunks (SRCS, FILE) hold a
   sequence of child chunks. Everything little-endian. */

#define MINX_VERSION 1
#define MINX_HDRSIZE 16

#define SYM_ROM   0x1   /* phys field valid (symbol maps into cart ROM) */
#define SYM_LOCAL 0x2   /* scoped symbol (a NoICE DEFS record)          */

#define AREA_ABS  0x1   /* absolute area */
#define AREA_OVR  0x2   /* overlay area  */

#define NO_FILE   0xFFFFFFFFu

static void wle16 (unsigned char *p, uint32_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
static void wle32 (unsigned char *p, uint32_t v) { wle16 (p, v & 0xFFFF); wle16 (p + 2, v >> 16); }

/* growable byte buffer; allocation failure is fatal (one-shot CLI tool) */
typedef struct { unsigned char *p; size_t len, cap; } buf;

static void buf_put (buf *b, const void *src, size_t n)
{
  if (b->len + n > b->cap)
    {
      size_t ncap = b->cap ? b->cap : 256;
      while (ncap < b->len + n) ncap += ncap / 2 + 16;
      unsigned char *np = realloc (b->p, ncap);
      if (!np) { fprintf (stderr, "romgen: out of memory\n"); exit (2); }
      b->p = np; b->cap = ncap;
    }
  memcpy (b->p + b->len, src, n);
  b->len += n;
}

static void buf_u32 (buf *b, uint32_t v)
{
  unsigned char t[4];
  wle32 (t, v);
  buf_put (b, t, 4);
}

/* add a string to the string table; offset 0 is always "" */
static uint32_t strtab_add (buf *st, const char *s)
{
  uint32_t off;
  if (st->len == 0) buf_put (st, "", 1);
  off = (uint32_t) st->len;
  buf_put (st, s, strlen (s) + 1);
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

/* ---- chunk writer: open/close nest, blobs are one-shot ---- */

static void ck_pad (buf *b)
{
  static const unsigned char z[3] = { 0, 0, 0 };
  size_t r = b->len & 3;
  if (r) buf_put (b, z, 4 - r);
}

static size_t ck_open (buf *b, const char *id)
{
  buf_put (b, id, 4);
  buf_u32 (b, 0);              /* size, patched by ck_close */
  return b->len - 4;
}

static void ck_close (buf *b, size_t szpos)
{
  wle32 (b->p + szpos, (uint32_t) (b->len - (szpos + 4)));
  ck_pad (b);
}

static void ck_blob (buf *b, const char *id, const void *data, size_t n)
{
  size_t p = ck_open (b, id);
  buf_put (b, data, n);
  ck_close (b, p);
}

/* whole file -> malloc'd buffer (binary, NUL-padded); NULL if unreadable */
static unsigned char *read_file (const char *path, size_t *out_size)
{
  FILE *f = fopen (path, "rb");
  long n;
  unsigned char *p;
  if (!f) return NULL;
  if (fseek (f, 0, SEEK_END) != 0 || (n = ftell (f)) < 0 || fseek (f, 0, SEEK_SET) != 0)
    { fclose (f); return NULL; }
  p = malloc ((size_t) n + 1);
  if (!p) { fclose (f); return NULL; }
  if (n && fread (p, 1, (size_t) n, f) != (size_t) n)
    { fclose (f); free (p); return NULL; }
  fclose (f);
  p[n] = '\0';
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

static char *xstrdup (const char *s)
{
  char *p = malloc (strlen (s) + 1);
  if (!p) { fprintf (stderr, "romgen: out of memory\n"); exit (2); }
  strcpy (p, s);
  return p;
}

static void *grow (void *p, uint32_t count, uint32_t *cap, size_t elem)
{
  if (count < *cap) return p;
  *cap = *cap ? *cap * 2 : 64;
  p = realloc (p, (size_t) *cap * elem);
  if (!p) { fprintf (stderr, "romgen: out of memory\n"); exit (2); }
  return p;
}

/* ---- symbols: NoICE .noi DEF/DEFS records ----
   Lines look like "DEF _main 0x2196" (PagedAddress may prefix "bank:", which
   sdldz80 never does for this port — handled anyway). */

struct minxsym { uint32_t name, value, phys, flags; };

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

      syms = grow (syms, count, &cap, sizeof *syms);
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

/* ---- debug records: sdcc --debug .cdb ----
   We promote the linker-resolved records to binary tables:
     L:C$<file>$<line>$<level>$<block>:<HEXADDR>   -> line table entry
     F:G$<name>$...                                -> <name> is a function
     L:G$<name>$...:<HEXADDR>                      -> function entry address
     L:XG$<name>$...:<HEXADDR>                     -> function exit label (the
                                                      final return instruction)
   Everything else (S:/T: type and variable records) is debugger fidelity we
   haven't promoted yet — reserved for a future TYPE/VAR chunk pair. */

struct linerec  { uint32_t phys, file, line, rsv; };
struct funcinfo { char *name; uint32_t entry, end; int have_entry, have_end; };

struct cdbinfo
{
  struct linerec  *lines; uint32_t n_lines,  cap_lines;
  struct funcinfo *funcs; uint32_t n_funcs,  cap_funcs;
  char           **files; uint32_t n_files,  cap_files;
};

static uint32_t file_id (struct cdbinfo *ci, const char *name)
{
  uint32_t i;
  for (i = 0; i < ci->n_files; i++)
    if (strcmp (ci->files[i], name) == 0)
      return i;
  ci->files = grow (ci->files, ci->n_files, &ci->cap_files, sizeof *ci->files);
  ci->files[ci->n_files] = xstrdup (name);
  return ci->n_files++;
}

static struct funcinfo *find_func (struct cdbinfo *ci, const char *name, int add)
{
  uint32_t i;
  for (i = 0; i < ci->n_funcs; i++)
    if (strcmp (ci->funcs[i].name, name) == 0)
      return &ci->funcs[i];
  if (!add) return NULL;
  ci->funcs = grow (ci->funcs, ci->n_funcs, &ci->cap_funcs, sizeof *ci->funcs);
  memset (&ci->funcs[ci->n_funcs], 0, sizeof ci->funcs[0]);
  ci->funcs[ci->n_funcs].name = xstrdup (name);
  return &ci->funcs[ci->n_funcs++];
}

static void parse_cdb (const char *text, size_t textlen, struct cdbinfo *ci)
{
  const char *p = text, *end = text + textlen;

  while (p < end)
    {
      const char *eol = memchr (p, '\n', (size_t) (end - p));
      size_t linelen = eol ? (size_t) (eol - p) : (size_t) (end - p);
      char ln[512];
      if (linelen >= sizeof ln) { p = eol ? eol + 1 : end; continue; }
      memcpy (ln, p, linelen);
      ln[linelen] = '\0';
      while (linelen && (ln[linelen-1] == '\r' || ln[linelen-1] == ' ')) ln[--linelen] = '\0';
      p = eol ? eol + 1 : end;

      if (strncmp (ln, "F:G$", 4) == 0)
        {
          char *dollar = strchr (ln + 4, '$');
          if (!dollar) continue;
          *dollar = '\0';
          find_func (ci, ln + 4, 1);
        }
      else if (strncmp (ln, "L:C$", 4) == 0)
        {
          /* L:C$hello.c$26$0_0$12:21E4 */
          char *file = ln + 4;
          char *d1 = strchr (file, '$');
          if (!d1) continue;
          char *colon = strrchr (d1 + 1, ':');
          if (!colon) continue;
          *d1 = '\0';
          uint32_t lno  = (uint32_t) strtoul (d1 + 1, NULL, 10);
          uint32_t addr = (uint32_t) strtoul (colon + 1, NULL, 16);
          uint32_t phys;
          if (lno == 0 || phys_of (addr, &phys) != 0)
            continue;
          ci->lines = grow (ci->lines, ci->n_lines, &ci->cap_lines, sizeof *ci->lines);
          ci->lines[ci->n_lines].phys = phys;
          ci->lines[ci->n_lines].file = file_id (ci, file);
          ci->lines[ci->n_lines].line = lno;
          ci->lines[ci->n_lines].rsv  = 0;
          ci->n_lines++;
        }
      else if (strncmp (ln, "L:G$", 4) == 0 || strncmp (ln, "L:XG$", 5) == 0)
        {
          int is_end = (ln[2] == 'X');
          char *name = ln + (is_end ? 5 : 4);
          char *dollar = strchr (name, '$');
          char *colon = strrchr (name, ':');
          if (!dollar || !colon || colon < dollar) continue;
          *dollar = '\0';
          uint32_t addr = (uint32_t) strtoul (colon + 1, NULL, 16);
          uint32_t phys;
          struct funcinfo *fn = find_func (ci, name, 0);
          if (!fn || phys_of (addr, &phys) != 0)
            continue;
          if (is_end) { fn->end = phys;   fn->have_end = 1; }
          else        { fn->entry = phys; fn->have_entry = 1; }
        }
    }
}

static int line_cmp (const void *a, const void *b)
{
  const struct linerec *x = a, *y = b;
  if (x->phys != y->phys) return x->phys < y->phys ? -1 : 1;
  if (x->line != y->line) return x->line < y->line ? -1 : 1;
  if (x->file != y->file) return x->file < y->file ? -1 : 1;
  return 0;
}

static int func_cmp (const void *a, const void *b)
{
  const struct funcinfo *x = a, *y = b;
  if (x->entry != y->entry) return x->entry < y->entry ? -1 : 1;
  return strcmp (x->name, y->name);
}

/* ---- areas: the linker .map area table ----
   Rows look like "_CODE  0021D0  000B12 =  2834. bytes (REL,CON)"; the table
   header and the ".  .ABS." pseudo-area don't survive the sscanf. Best-effort:
   a malformed map yields fewer areas, never a bad record. */

struct arearec { uint32_t name, base, size, flags; };

static struct arearec *parse_map (const char *text, size_t textlen, buf *st, uint32_t *out_count)
{
  struct arearec *areas = NULL;
  uint32_t count = 0, cap = 0;
  const char *p = text, *end = text + textlen;

  while (p < end)
    {
      const char *eol = memchr (p, '\n', (size_t) (end - p));
      size_t linelen = eol ? (size_t) (eol - p) : (size_t) (end - p);
      char ln[512];
      if (linelen >= sizeof ln) { p = eol ? eol + 1 : end; continue; }
      memcpy (ln, p, linelen);
      ln[linelen] = '\0';
      p = eol ? eol + 1 : end;

      if (!strstr (ln, ". bytes ("))
        continue;
      char name[128];
      uint32_t base, size;
      if (sscanf (ln, "%127s %x %x", name, &base, &size) != 3)
        continue;
      uint32_t flags = 0, i;
      if (strstr (ln, "ABS")) flags |= AREA_ABS;
      if (strstr (ln, "OVR")) flags |= AREA_OVR;
      for (i = 0; i < count; i++)        /* page reprints: keep first */
        if (strcmp ((const char *) st->p + areas[i].name, name) == 0)
          break;
      if (i < count) continue;
      areas = grow (areas, count, &cap, sizeof *areas);
      areas[count].name  = strtab_add (st, name);
      areas[count].base  = base;
      areas[count].size  = size;
      areas[count].flags = flags;
      count++;
    }
  *out_count = count;
  return areas;
}

/* ------------------------------------------------------------------------- */

static void usage (void)
{
  fprintf (stderr,
    "usage: romgen in.ihx out.min  [--far=start-end]...\n"
    "       romgen in.ihx out.minx [--minx] [--far=start-end]...\n"
    "              [--map=file] [--noi=file] [--cdb=file]\n"
    "              [--srcdir=dir]... [--no-src] [--embed=name=file]...\n");
}

int main (int argc, char **argv)
{
  const char *inpath = NULL, *outpath = NULL;
  const char *map_arg = NULL, *noi_arg = NULL, *cdb_arg = NULL;
  const char *srcdirs[MAX_SRCDIR];
  struct { const char *name, *path; } embeds[MAX_EMBED];
  int n_embed = 0, n_srcdir = 0, opt_minx = 0, opt_nosrc = 0;
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
      else if (strcmp (argv[i], "--no-src") == 0)          opt_nosrc = 1;
      else if (strncmp (argv[i], "--map=", 6) == 0)        map_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--noi=", 6) == 0)        noi_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--cdb=", 6) == 0)        cdb_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--srcdir=", 9) == 0)
        {
          if (n_srcdir >= MAX_SRCDIR) { fprintf (stderr, "romgen: too many --srcdir\n"); return 2; }
          srcdirs[n_srcdir++] = argv[i] + 9;
        }
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
  buf body = { NULL, 0, 0 }, strtab = { NULL, 0, 0 };
  unsigned char *mapdat = NULL, *noidat = NULL, *cdbdat = NULL;
  size_t mapsz = 0, noisz = 0, cdbsz = 0;
  struct minxsym *syms = NULL;
  struct arearec *areas = NULL;
  struct cdbinfo ci;
  uint32_t n_syms = 0, n_areas = 0, n_embedded_src = 0, n_funcs_emitted = 0;
  size_t p;
  char tmp[256];

  memset (&ci, 0, sizeof ci);

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
          char *sp = sidecar_path (inpath, side[i].ext);
          if (sp) { *side[i].dat = read_file (sp, side[i].sz); free (sp); }
        }
    }

  if (noidat)
    syms = parse_noi ((const char *) noidat, noisz, &strtab, &n_syms);
  if (mapdat)
    areas = parse_map ((const char *) mapdat, mapsz, &strtab, &n_areas);
  if (cdbdat)
    parse_cdb ((const char *) cdbdat, cdbsz, &ci);

  if (ci.n_lines)
    {
      qsort (ci.lines, ci.n_lines, sizeof *ci.lines, line_cmp);
      uint32_t w = 1;                 /* drop exact duplicates */
      for (i = 1; i < (int) ci.n_lines; i++)
        if (line_cmp (&ci.lines[i], &ci.lines[w-1]) != 0)
          ci.lines[w++] = ci.lines[i];
      ci.n_lines = w;
    }
  if (ci.n_funcs)
    qsort (ci.funcs, ci.n_funcs, sizeof *ci.funcs, func_cmp);

  /* ROM */
  p = ck_open (&body, "ROM ");
  buf_u32 (&body, CART_BASE);
  buf_put (&body, img, size);
  ck_close (&body, p);

  /* AREA */
  if (n_areas)
    {
      buf rec = { NULL, 0, 0 };
      for (i = 0; i < (int) n_areas; i++)
        {
          buf_u32 (&rec, areas[i].name);  buf_u32 (&rec, areas[i].base);
          buf_u32 (&rec, areas[i].size);  buf_u32 (&rec, areas[i].flags);
        }
      ck_blob (&body, "AREA", rec.p, rec.len);
      free (rec.p);
    }

  /* SYM */
  if (n_syms)
    {
      buf rec = { NULL, 0, 0 };
      qsort (syms, n_syms, sizeof *syms, sym_cmp);
      for (i = 0; i < (int) n_syms; i++)
        {
          buf_u32 (&rec, syms[i].name);  buf_u32 (&rec, syms[i].value);
          buf_u32 (&rec, syms[i].phys);  buf_u32 (&rec, syms[i].flags);
        }
      ck_blob (&body, "SYM ", rec.p, rec.len);
      free (rec.p);
    }

  /* LINE — the global line table, sorted by physical address */
  if (ci.n_lines)
    {
      buf rec = { NULL, 0, 0 };
      for (i = 0; i < (int) ci.n_lines; i++)
        {
          buf_u32 (&rec, ci.lines[i].phys);  buf_u32 (&rec, ci.lines[i].file);
          buf_u32 (&rec, ci.lines[i].line);  buf_u32 (&rec, 0);
        }
      ck_blob (&body, "LINE", rec.p, rec.len);
      free (rec.p);
    }

  /* FUNC — entry/exit extents; file = file of the first line record inside */
  if (ci.n_funcs)
    {
      buf rec = { NULL, 0, 0 };
      for (i = 0; i < (int) ci.n_funcs; i++)
        {
          struct funcinfo *fn = &ci.funcs[i];
          if (!fn->have_entry) continue;
          uint32_t file = NO_FILE, j;
          uint32_t fend = fn->have_end ? fn->end : fn->entry;
          for (j = 0; j < ci.n_lines; j++)
            if (ci.lines[j].phys >= fn->entry && ci.lines[j].phys <= fend)
              { file = ci.lines[j].file; break; }
          buf_u32 (&rec, strtab_add (&strtab, fn->name));
          buf_u32 (&rec, fn->entry);
          buf_u32 (&rec, fend);
          buf_u32 (&rec, file);
          n_funcs_emitted++;
        }
      if (n_funcs_emitted)
        ck_blob (&body, "FUNC", rec.p, rec.len);
      free (rec.p);
    }

  /* SRCS — one FILE container per source file: NAME + (unless --no-src) TEXT.
     File ids in LINE/FUNC are ordinals into this sequence. */
  if (ci.n_files)
    {
      size_t srcs = ck_open (&body, "SRCS");
      const char *indir_end = strrchr (inpath, '/');
      for (i = 0; i < (int) ci.n_files; i++)
        {
          size_t fc = ck_open (&body, "FILE");
          ck_blob (&body, "NAME", ci.files[i], strlen (ci.files[i]));
          if (!opt_nosrc)
            {
              unsigned char *text = NULL;
              size_t textsz = 0;
              int d;
              text = read_file (ci.files[i], &textsz);
              if (!text && indir_end)
                {
                  snprintf (tmp, sizeof tmp, "%.*s/%s", (int) (indir_end - inpath), inpath, ci.files[i]);
                  text = read_file (tmp, &textsz);
                }
              for (d = 0; !text && d < n_srcdir; d++)
                {
                  snprintf (tmp, sizeof tmp, "%s/%s", srcdirs[d], ci.files[i]);
                  text = read_file (tmp, &textsz);
                }
              if (text)
                {
                  ck_blob (&body, "TEXT", text, textsz);
                  free (text);
                  n_embedded_src++;
                }
              else
                fprintf (stderr, "romgen: warning: source '%s' not found (try --srcdir=); name embedded without text\n",
                         ci.files[i]);
            }
          ck_close (&body, fc);
        }
      ck_close (&body, srcs);
    }

  /* USR  — --embed payloads: u32 name (strtab) + raw bytes */
  for (i = 0; i < n_embed; i++)
    {
      size_t esz;
      unsigned char *edat = read_file (embeds[i].path, &esz);
      if (!edat)
        { fprintf (stderr, "romgen: cannot read --embed file '%s'\n", embeds[i].path); free (img); return 2; }
      p = ck_open (&body, "USR ");
      buf_u32 (&body, strtab_add (&strtab, embeds[i].name));
      buf_put (&body, edat, esz);
      ck_close (&body, p);
      free (edat);
    }

  /* NOTE — (key,val) string pairs; deterministic (no timestamps) */
  {
    buf rec = { NULL, 0, 0 };
    struct { const char *k; const char *v; } kv[8];
    int nkv = 0;
    char vsrc[64], vrom[32], vbank[16], vsym[16], vline[16], vfunc[16];
    snprintf (vrom,  sizeof vrom,  "%zu", size);
    snprintf (vbank, sizeof vbank, "%d", n_banks);
    snprintf (vsym,  sizeof vsym,  "%u", n_syms);
    snprintf (vline, sizeof vline, "%u", ci.n_lines);
    snprintf (vfunc, sizeof vfunc, "%u", n_funcs_emitted);
    snprintf (vsrc,  sizeof vsrc,  "0x%04x", CART_BASE);
    kv[nkv].k = "generator"; kv[nkv++].v = "romgen (sdcc88)";
    kv[nkv].k = "source";    kv[nkv++].v = inpath;
    kv[nkv].k = "cart-base"; kv[nkv++].v = vsrc;
    kv[nkv].k = "rom-bytes"; kv[nkv++].v = vrom;
    kv[nkv].k = "banks";     kv[nkv++].v = vbank;
    kv[nkv].k = "symbols";   kv[nkv++].v = vsym;
    kv[nkv].k = "lines";     kv[nkv++].v = vline;
    kv[nkv].k = "functions"; kv[nkv++].v = vfunc;
    for (i = 0; i < nkv; i++)
      {
        buf_u32 (&rec, strtab_add (&strtab, kv[i].k));
        buf_u32 (&rec, strtab_add (&strtab, kv[i].v));
      }
    for (i = 0; i < n_far; i++)
      {
        snprintf (tmp, sizeof tmp, "0x%x-0x%x", far_ranges[i].lo, far_ranges[i].hi);
        buf_u32 (&rec, strtab_add (&strtab, "far"));
        buf_u32 (&rec, strtab_add (&strtab, tmp));
      }
    ck_blob (&body, "NOTE", rec.p, rec.len);
    free (rec.p);
  }

  /* STR — last: every name is interned by now */
  if (strtab.len == 0) buf_put (&strtab, "", 1);
  ck_blob (&body, "STR ", strtab.p, strtab.len);

  /* header + body */
  {
    unsigned char hdr[MINX_HDRSIZE];
    FILE *o = fopen (outpath, "wb");
    if (!o) { perror (outpath); free (img); return 2; }
    memcpy (hdr, "MINX", 4);
    wle16 (hdr + 4, MINX_VERSION);
    wle16 (hdr + 6, 0);
    wle32 (hdr + 8, (uint32_t) (MINX_HDRSIZE + body.len));
    wle32 (hdr + 12, crc32_buf (body.p, body.len));
    if (fwrite (hdr, 1, sizeof hdr, o) != sizeof hdr
        || (body.len && fwrite (body.p, 1, body.len, o) != body.len)
        || fclose (o) != 0)
      { perror (outpath); free (img); return 2; }
  }

  printf ("wrote %s (minx: %zu ROM bytes, %d banks, %u symbols, %u lines, %u functions, %u/%u sources embedded)\n",
          outpath, size, n_banks, n_syms, ci.n_lines, n_funcs_emitted, n_embedded_src, ci.n_files);

  free (img);
  free (mapdat); free (noidat); free (cdbdat);
  free (syms); free (areas);
  for (i = 0; i < (int) ci.n_files; i++) free (ci.files[i]);
  for (i = 0; i < (int) ci.n_funcs; i++) free (ci.funcs[i].name);
  free (ci.files); free (ci.funcs); free (ci.lines);
  free (body.p); free (strtab.p);
  return 0;
}
