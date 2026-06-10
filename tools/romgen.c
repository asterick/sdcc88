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
        --debug .cdb (line/function/type/variable records) — and promotes
        everything to binary tables: sparse ROM segments, a sorted global line
        table, function extents, symbols, memory areas, a type graph, variable
        locations (register / IX-relative stack / static address), and the
        source files themselves embedded for display. Sidecars are
        auto-discovered next to in.ihx (same stem); --map/--noi/--cdb override
        the path (and then must exist). Source files are found next to in.ihx /
        the cwd / each --srcdir. Format spec: docs/s1c88/minx-format.md;
        reference reader: tools/minxdump.c.

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

   The flat ROM image's byte 0 is physical 0x2100 (the cart header), so
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
   of the enclosing container/file). Container chunks (ROM, SRCS, FILE) hold a
   sequence of child chunks. Everything little-endian. */

#define MINX_VERSION 1
#define MINX_HDRSIZE 16

#define SYM_ROM    0x1  /* phys field valid (symbol maps into cart ROM) */
#define SYM_LOCAL  0x2  /* scoped symbol (a NoICE DEFS record)          */

#define AREA_ABS   0x1
#define AREA_OVR   0x2

/* TYPE record kinds (low byte of kind_flags) */
enum
{
  TK_VOID = 0, TK_CHAR, TK_SHORT, TK_INT, TK_LONG, TK_FLOAT, TK_SBIT,
  TK_BITFIELD,  /* extra = bit offset | (bit count << 16)  */
  TK_STRUCT,    /* extra = STRU chunk ordinal              */
  TK_ARRAY,     /* extra = element count, target = element */
  TK_FUNCTION,  /* target = return type                    */
  TK_POINTER    /* extra = cdb space letter, target = pointee */
};
#define TF_UNSIGNED 0x100

/* VAR location kinds */
enum { LOC_NONE = 0, LOC_STATIC, LOC_STACK, LOC_REGS };

/* VAR flags */
#define VF_FILESTATIC 0x1
#define VF_ARTIFICIAL 0x2   /* compiler spill temp (sloc<N>) — UIs may hide */
/* bits 8-15: the raw cdb address-space letter */

/* FUNC flags */
#define FN_FILESTATIC 0x1
#define FN_INTERRUPT  0x2
#define FN_HASFRAME   0x4   /* entry bytes are push ix ; ld ix,sp (A2 CF FA) */
/* bits 8-15: interrupt number; bits 16-23: register bank */

#define NO_REF 0xFFFFFFFFu

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

   Promoted records (the full grammar is the SDCC manual's "CDB File Format"):

     T:F<module>$<name>[members]                struct/union layout
     F:<scope>$<lvl>$<blk>(<type>),<spc>,...    function (frame size, interrupt)
     S:<scope>$<lvl>$<blk>(<type>),<spc>,<onstk>,<offs>[,[regs]]  variable
     L:C$<file>$<line>$<lvl>$<blk>:<addr>       line table entry
     L:<scope>:<addr> / L:X<scope>:<addr>       address binding / function exit

   <scope> is G$<name> (global), F<module>$<name> (file-static), or
   L<module>.<function>$<name> (function-local). Stack offsets are relative to
   IX, the frame pointer set by the prologue (push ix ; ld ix,sp). */

struct scoperef
{
  char kind;              /* 'G', 'F', 'L' */
  char scopename[200];    /* module, or module.function for 'L' */
  char name[200];
  char level[24];         /* raw "<level>_<sub>" token */
  char block[24];
};

/* parse "<scope>$<name>$<level>$<block>" starting at p; *rest -> terminator */
static int parse_scoperef (const char *p, struct scoperef *r, const char **rest)
{
  const char *d;
  size_t n;
  memset (r, 0, sizeof *r);
  r->kind = *p;
  if (r->kind != 'G' && r->kind != 'F' && r->kind != 'L')
    return -1;
  p++;
  d = strchr (p, '$');
  if (!d) return -1;
  n = (size_t) (d - p);
  if (n >= sizeof r->scopename) return -1;
  memcpy (r->scopename, p, n); r->scopename[n] = '\0';
  p = d + 1;

  d = strchr (p, '$');
  if (!d) return -1;
  n = (size_t) (d - p);
  if (n == 0 || n >= sizeof r->name) return -1;
  memcpy (r->name, p, n); r->name[n] = '\0';
  p = d + 1;

  d = strchr (p, '$');
  if (!d) return -1;
  n = (size_t) (d - p);
  if (n >= sizeof r->level) return -1;
  memcpy (r->level, p, n); r->level[n] = '\0';
  p = d + 1;

  n = strcspn (p, "(:,");
  if (n >= sizeof r->block) return -1;
  memcpy (r->block, p, n); r->block[n] = '\0';
  *rest = p + n;
  return 0;
}

/* "1_0" -> level 1, sub 0; "0" -> level 0, sub 0 */
static void parse_level (const char *tok, uint32_t *level, uint32_t *sub)
{
  char *us;
  *level = (uint32_t) strtoul (tok, &us, 10);
  *sub = (us && *us == '_') ? (uint32_t) strtoul (us + 1, NULL, 10) : 0;
}

struct typerec  { uint32_t kind_flags, size, target, extra; };
struct member   { uint32_t name, offset, type; };
struct structdef
{
  char *name;
  char *rawmembers;          /* the [...] body, parsed in phase B */
  uint32_t name_off, size;
  struct member *members;
  uint32_t n_members, cap_members;
};
struct linerec  { uint32_t phys, file, line, scope; };
struct funcinfo
{
  struct scoperef ref;
  uint32_t entry, end;       /* physical */
  int have_entry, have_end;
  uint32_t rettype, frame, flags;
  uint32_t emit_index;       /* index in the emitted FUNC table */
};
struct varinfo
{
  struct scoperef ref;
  uint32_t name, type, levelblock, loc_kind, loc, flags;
  int is_funcvar;            /* S record whose type is a bare function: skip */
};

struct cdbinfo
{
  struct linerec   *lines;   uint32_t n_lines,   cap_lines;
  struct funcinfo  *funcs;   uint32_t n_funcs,   cap_funcs;
  struct varinfo   *vars;    uint32_t n_vars,    cap_vars;
  struct typerec   *types;   uint32_t n_types,   cap_types;
  struct structdef *structs; uint32_t n_structs, cap_structs;
  char            **files;   uint32_t n_files,   cap_files;
  buf *st;
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

static uint32_t type_intern (struct cdbinfo *ci, uint32_t kind_flags, uint32_t size,
                             uint32_t target, uint32_t extra)
{
  uint32_t i;
  for (i = 0; i < ci->n_types; i++)
    if (ci->types[i].kind_flags == kind_flags && ci->types[i].size == size
        && ci->types[i].target == target && ci->types[i].extra == extra)
      return i;
  ci->types = grow (ci->types, ci->n_types, &ci->cap_types, sizeof *ci->types);
  ci->types[ci->n_types].kind_flags = kind_flags;
  ci->types[ci->n_types].size = size;
  ci->types[ci->n_types].target = target;
  ci->types[ci->n_types].extra = extra;
  return ci->n_types++;
}

static uint32_t struct_ordinal (struct cdbinfo *ci, const char *name)
{
  uint32_t i;
  for (i = 0; i < ci->n_structs; i++)
    if (strcmp (ci->structs[i].name, name) == 0)
      return i;
  return NO_REF;
}

/* parse a cdb type chain "{<size>}<tok>,<tok>,...:<sign>"; returns TYPE index.
   Tokens, right to left (base first): SC/SS/SI/SL/SF/SV/SX primitives,
   SB<bitoff>$<bits> bitfield, ST<name> struct, then declarators DA<n>d array,
   DF function, DG/DC/DD/DX/DI/DP pointers (X = __far, 3 bytes). */
static uint32_t parse_typechain (struct cdbinfo *ci, const char *s, size_t len)
{
  char chain[256];
  uint32_t total = 0;
  int uns = 0;
  char *toks[16];
  int n_toks = 0, t;

  if (len >= sizeof chain) return NO_REF;
  memcpy (chain, s, len); chain[len] = '\0';

  char *p = chain;
  if (*p == '{') total = (uint32_t) strtoul (p + 1, &p, 10), p = strchr (p, '}') ? strchr (p, '}') + 1 : p;
  char *colon = strrchr (p, ':');
  if (colon) { uns = (colon[1] == 'U'); *colon = '\0'; }

  for (char *tok = strtok (p, ","); tok && n_toks < 16; tok = strtok (NULL, ","))
    toks[n_toks++] = tok;
  if (!n_toks) return NO_REF;

  /* base type (last token) */
  char *base = toks[n_toks - 1];
  uint32_t kf = 0, size = 0, extra = 0, idx;
  uint32_t uflag = uns ? TF_UNSIGNED : 0;
  if      (strcmp (base, "SC") == 0) kf = TK_CHAR  | uflag, size = 1;
  else if (strcmp (base, "SS") == 0) kf = TK_SHORT | uflag, size = 2;
  else if (strcmp (base, "SI") == 0) kf = TK_INT   | uflag, size = 2;
  else if (strcmp (base, "SL") == 0) kf = TK_LONG  | uflag, size = 4;
  else if (strcmp (base, "SF") == 0) kf = TK_FLOAT, size = 4;
  else if (strcmp (base, "SV") == 0) kf = TK_VOID,  size = 0;
  else if (strcmp (base, "SX") == 0) kf = TK_SBIT | uflag, size = 1;
  else if (strncmp (base, "SB", 2) == 0)
    {
      char *d = strchr (base + 2, '$');
      uint32_t bo = (uint32_t) strtoul (base + 2, NULL, 10);
      uint32_t bits = d ? (uint32_t) strtoul (d + 1, NULL, 10) : 1;
      kf = TK_BITFIELD | uflag; size = 1; extra = bo | (bits << 16);
    }
  else if (strncmp (base, "ST", 2) == 0)
    {
      uint32_t ord = struct_ordinal (ci, base + 2);
      kf = TK_STRUCT; extra = ord;
      size = (ord != NO_REF) ? ci->structs[ord].size : 0;
    }
  else
    return NO_REF;
  idx = type_intern (ci, kf, size, NO_REF, extra);

  /* declarators, innermost first */
  for (t = n_toks - 2; t >= 0; t--)
    {
      char *tok = toks[t];
      uint32_t prev_size = ci->types[idx].size;
      if (strncmp (tok, "DA", 2) == 0)
        {
          uint32_t count = (uint32_t) strtoul (tok + 2, NULL, 10);
          idx = type_intern (ci, TK_ARRAY, count * prev_size, idx, count);
        }
      else if (strcmp (tok, "DF") == 0)
        idx = type_intern (ci, TK_FUNCTION, 3, idx, 0);
      else if (tok[0] == 'D' && tok[1] && !tok[2])
        {
          uint32_t psize = (tok[1] == 'X' || tok[1] == 'C' || tok[1] == 'F') ? 3 : 2;
          idx = type_intern (ci, TK_POINTER, psize, idx, (uint32_t) tok[1]);
        }
      else
        return NO_REF;
    }
  if (total && ci->types[idx].size != total && ci->types[idx].kind_flags != TK_STRUCT)
    {
      /* trust the cdb's total size for the outermost type */
      uint32_t kf2 = ci->types[idx].kind_flags;
      idx = type_intern (ci, kf2, total, ci->types[idx].target, ci->types[idx].extra);
    }
  return idx;
}

/* phase B: parse a struct's raw member text:
   ({<offset>}S:S$<name>$0_0$0(<typechain>),Z,0,0)... */
static void parse_members (struct cdbinfo *ci, struct structdef *sd)
{
  const char *p = sd->rawmembers;
  uint32_t maxend = 0;

  while ((p = strstr (p, "({")) != NULL)
    {
      uint32_t off = (uint32_t) strtoul (p + 2, NULL, 10);
      const char *s = strstr (p, "S:S$");
      if (!s) break;
      s += 4;
      const char *d = strchr (s, '$');
      if (!d) break;
      char mname[200];
      size_t nl = (size_t) (d - s);
      if (nl == 0 || nl >= sizeof mname) break;
      memcpy (mname, s, nl); mname[nl] = '\0';
      const char *tp = strchr (d, '(');
      if (!tp) break;
      const char *te = strchr (tp, ')');
      if (!te) break;
      uint32_t ty = parse_typechain (ci, tp + 1, (size_t) (te - tp - 1));

      sd->members = grow (sd->members, sd->n_members, &sd->cap_members, sizeof *sd->members);
      sd->members[sd->n_members].name = strtab_add (ci->st, mname);
      sd->members[sd->n_members].offset = off;
      sd->members[sd->n_members].type = ty;
      sd->n_members++;
      if (ty != NO_REF && off + ci->types[ty].size > maxend)
        maxend = off + ci->types[ty].size;
      p = te;
    }
  if (sd->size < maxend)
    sd->size = maxend;
}

static struct funcinfo *find_func (struct cdbinfo *ci, const struct scoperef *r, int add)
{
  uint32_t i;
  for (i = 0; i < ci->n_funcs; i++)
    if (ci->funcs[i].ref.kind == r->kind
        && strcmp (ci->funcs[i].ref.scopename, r->scopename) == 0
        && strcmp (ci->funcs[i].ref.name, r->name) == 0)
      return &ci->funcs[i];
  if (!add) return NULL;
  ci->funcs = grow (ci->funcs, ci->n_funcs, &ci->cap_funcs, sizeof *ci->funcs);
  memset (&ci->funcs[ci->n_funcs], 0, sizeof ci->funcs[0]);
  ci->funcs[ci->n_funcs].ref = *r;
  ci->funcs[ci->n_funcs].rettype = NO_REF;
  ci->funcs[ci->n_funcs].emit_index = NO_REF;
  return &ci->funcs[ci->n_funcs++];
}

/* match a var to an L: address record: same scope + name + level + block */
static struct varinfo *find_var (struct cdbinfo *ci, const struct scoperef *r)
{
  uint32_t i;
  for (i = 0; i < ci->n_vars; i++)
    if (ci->vars[i].ref.kind == r->kind
        && strcmp (ci->vars[i].ref.scopename, r->scopename) == 0
        && strcmp (ci->vars[i].ref.name, r->name) == 0
        && strcmp (ci->vars[i].ref.level, r->level) == 0
        && strcmp (ci->vars[i].ref.block, r->block) == 0)
      return &ci->vars[i];
  return NULL;
}

/* S:/F: record tail after the type ")": ",<space>,<onstack>,<offset>[,[regs]]"
   (F records continue ",<interrupt>,<intno>,<regbank>") */
static int parse_tail (const char *p, char *space, int *onstack, long *offs, char *regs, size_t regsz)
{
  if (*p != ',') return -1;
  *space = p[1];
  p += 2;
  if (*p != ',') return -1;
  *onstack = (int) strtol (p + 1, (char **) &p, 10);
  if (*p != ',') return -1;
  *offs = strtol (p + 1, (char **) &p, 10);
  regs[0] = '\0';
  if (p[0] == ',' && p[1] == '[')
    {
      const char *e = strchr (p + 2, ']');
      if (e && (size_t) (e - p - 2) < regsz)
        {
          memcpy (regs, p + 2, (size_t) (e - p - 2));
          regs[e - p - 2] = '\0';
        }
    }
  return 0;
}

static void parse_cdb (const char *text, size_t textlen, struct cdbinfo *ci)
{
  const char *p;
  int pass;

  /* phase A: register struct names (so ST<name> refs resolve in any order) */
  for (p = text; p < text + textlen; )
    {
      const char *eol = memchr (p, '\n', (size_t) (text + textlen - p));
      size_t linelen = eol ? (size_t) (eol - p) : (size_t) (text + textlen - p);
      if (linelen > 4 && memcmp (p, "T:F", 3) == 0)
        {
          const char *d = memchr (p + 3, '$', linelen - 3);
          const char *lb = d ? memchr (d, '[', linelen - (size_t)(d - p)) : NULL;
          const char *rb = lb ? memchr (lb, ']', linelen - (size_t)(lb - p)) : NULL;
          if (d && lb && rb)
            {
              char name[200];
              size_t nl = (size_t) (lb - d - 1);
              if (nl > 0 && nl < sizeof name)
                {
                  memcpy (name, d + 1, nl); name[nl] = '\0';
                  if (struct_ordinal (ci, name) == NO_REF)
                    {
                      ci->structs = grow (ci->structs, ci->n_structs, &ci->cap_structs, sizeof *ci->structs);
                      memset (&ci->structs[ci->n_structs], 0, sizeof ci->structs[0]);
                      ci->structs[ci->n_structs].name = xstrdup (name);
                      ci->structs[ci->n_structs].name_off = strtab_add (ci->st, name);
                      char *raw = malloc ((size_t) (rb - lb));
                      if (!raw) { fprintf (stderr, "romgen: out of memory\n"); exit (2); }
                      memcpy (raw, lb + 1, (size_t) (rb - lb - 1));
                      raw[rb - lb - 1] = '\0';
                      ci->structs[ci->n_structs].rawmembers = raw;
                      ci->n_structs++;
                    }
                }
            }
        }
      p = eol ? eol + 1 : text + textlen;
    }

  /* phase A2: struct sizes from the members' own {total} chain sizes — done
     before any TK_STRUCT type record is interned, so declaration order (and
     struct-in-struct nesting) can't yield a zero-sized struct type */
  for (uint32_t s = 0; s < ci->n_structs; s++)
    {
      struct structdef *sd = &ci->structs[s];
      const char *m = sd->rawmembers;
      uint32_t maxend = 0;
      while ((m = strstr (m, "({")) != NULL)
        {
          uint32_t off = (uint32_t) strtoul (m + 2, NULL, 10);
          const char *tp = strstr (m + 2, "({");      /* the member's typechain */
          if (!tp) break;
          uint32_t total = (uint32_t) strtoul (tp + 2, NULL, 10);
          if (off + total > maxend) maxend = off + total;
          m = tp + 2;
        }
      sd->size = maxend;
    }

  /* phase B: member layouts */
  for (uint32_t s = 0; s < ci->n_structs; s++)
    parse_members (ci, &ci->structs[s]);

  /* phase C, two passes: F:/S: records first, then L: bindings (the linker
     appends L records, but don't depend on the order) */
  for (pass = 0; pass < 2; pass++)
    for (p = text; p < text + textlen; )
      {
        const char *eol = memchr (p, '\n', (size_t) (text + textlen - p));
        size_t linelen = eol ? (size_t) (eol - p) : (size_t) (text + textlen - p);
        char ln[512];
        const char *next = eol ? eol + 1 : text + textlen;
        if (linelen >= sizeof ln) { p = next; continue; }
        memcpy (ln, p, linelen);
        ln[linelen] = '\0';
        while (linelen && (ln[linelen-1] == '\r' || ln[linelen-1] == ' ')) ln[--linelen] = '\0';
        p = next;

        if (pass == 0 && (ln[0] == 'F' || ln[0] == 'S') && ln[1] == ':')
          {
            struct scoperef r;
            const char *rest;
            if (parse_scoperef (ln + 2, &r, &rest) != 0 || *rest != '(')
              continue;
            const char *te = strchr (rest, ')');
            if (!te) continue;
            uint32_t ty = parse_typechain (ci, rest + 1, (size_t) (te - rest - 1));
            char space = 0, regs[64];
            int onstack = 0;
            long offs = 0;
            if (parse_tail (te + 1, &space, &onstack, &offs, regs, sizeof regs) != 0)
              continue;

            int is_function = (ty != NO_REF
                               && (ci->types[ty].kind_flags & 0xFF) == TK_FUNCTION);
            if (ln[0] == 'F')
              {
                /* F:<scope>(type),<spc>,<onstk>,<frame>,<interrupt>,<intno>,<regbank> */
                struct funcinfo *fn = find_func (ci, &r, 1);
                fn->rettype = (ty != NO_REF) ? ci->types[ty].target : NO_REF;
                fn->frame = (uint32_t) offs;
                if (r.kind == 'F') fn->flags |= FN_FILESTATIC;
                const char *q = te + 1;
                int field = 0, intr = 0, intno = 0, rbank = 0;
                for (; *q; q++)
                  if (*q == ',')
                    {
                      field++;
                      if (field == 4) intr  = (int) strtol (q + 1, NULL, 10);
                      if (field == 5) intno = (int) strtol (q + 1, NULL, 10);
                      if (field == 6) rbank = (int) strtol (q + 1, NULL, 10);
                    }
                if (intr) fn->flags |= FN_INTERRUPT;
                fn->flags |= ((uint32_t) (intno & 0xFF) << 8) | ((uint32_t) (rbank & 0xFF) << 16);
              }
            else
              {
                if (is_function)
                  {
                    /* duplicate of the F record (frame size etc.) — FUNC covers it */
                    if (!find_func (ci, &r, 0))
                      {
                        struct funcinfo *fn = find_func (ci, &r, 1);
                        fn->rettype = ci->types[ty].target;
                        fn->frame = (uint32_t) offs;
                        if (r.kind == 'F') fn->flags |= FN_FILESTATIC;
                      }
                    continue;
                  }
                uint32_t level, sub;
                parse_level (r.level, &level, &sub);
                ci->vars = grow (ci->vars, ci->n_vars, &ci->cap_vars, sizeof *ci->vars);
                struct varinfo *v = &ci->vars[ci->n_vars++];
                memset (v, 0, sizeof *v);
                v->ref = r;
                v->name = strtab_add (ci->st, r.name);
                v->type = ty;
                v->levelblock = (level & 0xFF) | ((sub & 0xFF) << 8)
                              | (((uint32_t) strtoul (r.block, NULL, 10) & 0xFFFF) << 16);
                v->flags = ((uint32_t) (unsigned char) space << 8)
                         | (r.kind == 'F' ? VF_FILESTATIC : 0);
                if (strncmp (r.name, "sloc", 4) == 0
                    && r.name[4] && strspn (r.name + 4, "0123456789") == strlen (r.name + 4))
                  v->flags |= VF_ARTIFICIAL;
                if (onstack)
                  { v->loc_kind = LOC_STACK; v->loc = (uint32_t) offs; }
                else if (space == 'R')
                  {
                    if (regs[0]) { v->loc_kind = LOC_REGS; v->loc = strtab_add (ci->st, regs); }
                    else v->loc_kind = LOC_NONE;
                  }
                else
                  v->loc_kind = LOC_NONE;   /* static: address bound by an L record */
              }
          }
        else if (pass == 1 && strncmp (ln, "L:C$", 4) == 0)
          {
            /* L:C$<file>$<line>$<level>$<block>:<addr> */
            char *file = ln + 4;
            char *d1 = strchr (file, '$');
            if (!d1) continue;
            char *d2 = strchr (d1 + 1, '$');
            char *colon = strrchr (d1 + 1, ':');
            if (!colon) continue;
            *d1 = '\0';
            uint32_t lno = (uint32_t) strtoul (d1 + 1, NULL, 10);
            uint32_t level = 0, sub = 0, block = 0;
            if (d2 && d2 < colon)
              {
                parse_level (d2 + 1, &level, &sub);
                char *d3 = strchr (d2 + 1, '$');
                if (d3 && d3 < colon)
                  block = (uint32_t) strtoul (d3 + 1, NULL, 10);
              }
            uint32_t addr = (uint32_t) strtoul (colon + 1, NULL, 16);
            uint32_t phys;
            if (lno == 0 || phys_of (addr, &phys) != 0)
              continue;
            ci->lines = grow (ci->lines, ci->n_lines, &ci->cap_lines, sizeof *ci->lines);
            ci->lines[ci->n_lines].phys = phys;
            ci->lines[ci->n_lines].file = file_id (ci, file);
            ci->lines[ci->n_lines].line = lno;
            ci->lines[ci->n_lines].scope = (block & 0xFFFF) | ((level & 0xFF) << 16)
                                         | ((sub & 0xFF) << 24);
            ci->n_lines++;
          }
        else if (pass == 1 && ln[0] == 'L' && ln[1] == ':' && ln[2] != 'A')
          {
            /* L:<scope>:<addr> (address binding) or L:X<scope>:<addr> (fn exit) */
            int is_end = (ln[2] == 'X');
            struct scoperef r;
            const char *rest;
            if (parse_scoperef (ln + (is_end ? 3 : 2), &r, &rest) != 0 || *rest != ':')
              continue;
            uint32_t addr = (uint32_t) strtoul (rest + 1, NULL, 16);
            struct funcinfo *fn = find_func (ci, &r, 0);
            if (fn)
              {
                uint32_t phys;
                if (phys_of (addr, &phys) != 0)
                  continue;
                if (is_end) { fn->end = phys;   fn->have_end = 1; }
                else        { fn->entry = phys; fn->have_entry = 1; }
              }
            else if (!is_end)
              {
                struct varinfo *v = find_var (ci, &r);
                if (v && v->loc_kind == LOC_NONE)
                  {
                    /* static data: store the bus address (physical for ROM-
                       mapped const data; RAM/absolute addresses unchanged) */
                    uint32_t phys;
                    v->loc_kind = LOC_STATIC;
                    v->loc = (phys_of (addr, &phys) == 0) ? phys : addr;
                  }
              }
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
  return strcmp (x->ref.name, y->ref.name);
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

/* ---- IO register map: parsed from the device header (<pm.h>) ----
   Recognized define shapes (anything else — bit masks, VEC_* slots — is
   skipped): NAME _SFR8(off) / _SFR16(off) (off + 0x2000), and the literal
   pointer casts (*(volatile unsigned char|int *)addr). Names starting with
   '_' are internal macros, skipped. */

struct iorec { uint32_t name, addr, size, flags; };

static struct iorec *parse_io (const char *text, size_t textlen, buf *st, uint32_t *out_count)
{
  struct iorec *io = NULL;
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

      if (strncmp (ln, "#define ", 8) != 0)
        continue;
      char name[128];
      size_t nl = strspn (ln + 8, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
      if (nl == 0 || nl >= sizeof name || ln[8] == '_')
        continue;
      memcpy (name, ln + 8, nl); name[nl] = '\0';

      const char *rest = ln + 8 + nl, *m;
      uint32_t addr, size;
      if ((m = strstr (rest, "_SFR8(")) != NULL)
        addr = 0x2000 + (uint32_t) strtoul (m + 6, NULL, 0), size = 1;
      else if ((m = strstr (rest, "_SFR16(")) != NULL)
        addr = 0x2000 + (uint32_t) strtoul (m + 7, NULL, 0), size = 2;
      else if ((m = strstr (rest, "unsigned char *)")) != NULL)
        addr = (uint32_t) strtoul (m + 16, NULL, 0), size = 1;
      else if ((m = strstr (rest, "unsigned int *)")) != NULL)
        addr = (uint32_t) strtoul (m + 15, NULL, 0), size = 2;
      else
        continue;
      if (addr == 0)
        continue;
      io = grow (io, count, &cap, sizeof *io);
      io[count].name = strtab_add (st, name);
      io[count].addr = addr;
      io[count].size = size;
      io[count].flags = 0;
      count++;
    }
  *out_count = count;
  return io;
}

static int io_cmp (const void *a, const void *b)
{
  const struct iorec *x = a, *y = b;
  if (x->addr != y->addr) return x->addr < y->addr ? -1 : 1;
  return x->name < y->name ? -1 : x->name > y->name ? 1 : 0;
}

/* the device header ships with the toolchain: <bin>/../share/sdcc/include/s1c88/pm.h */
static char *default_io_path (const char *argv0)
{
  const char *slash = strrchr (argv0, '/');
  const char *tail = "/../share/sdcc/include/s1c88/pm.h";
  size_t dl = slash ? (size_t) (slash - argv0) : 1;
  char *p = malloc (dl + strlen (tail) + 1);
  if (!p) return NULL;
  memcpy (p, slash ? argv0 : ".", dl);
  strcpy (p + dl, tail);
  return p;
}

/* ------------------------------------------------------------------------- */

static void usage (void)
{
  fprintf (stderr,
    "usage: romgen in.ihx out.min  [--far=start-end]...\n"
    "       romgen in.ihx out.minx [--minx] [--far=start-end]...\n"
    "              [--map=file] [--noi=file] [--cdb=file] [--io=pm.h|--no-io]\n"
    "              [--srcdir=dir]... [--no-src] [--embed=name=file]...\n");
}

int main (int argc, char **argv)
{
  const char *inpath = NULL, *outpath = NULL;
  const char *map_arg = NULL, *noi_arg = NULL, *cdb_arg = NULL, *io_arg = NULL;
  const char *srcdirs[MAX_SRCDIR];
  struct { const char *name, *path; } embeds[MAX_EMBED];
  int n_embed = 0, n_srcdir = 0, opt_minx = 0, opt_nosrc = 0, opt_noio = 0;
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
      else if (strcmp (argv[i], "--no-io") == 0)           opt_noio = 1;
      else if (strncmp (argv[i], "--map=", 6) == 0)        map_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--noi=", 6) == 0)        noi_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--cdb=", 6) == 0)        cdb_arg = argv[i] + 6;
      else if (strncmp (argv[i], "--io=", 5) == 0)         io_arg = argv[i] + 5;
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
  unsigned char *wr = NULL;   /* parallel map: 1 = byte was written (SEG runs) */
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
                  img = ni;
                  unsigned char *nw = realloc (wr, ncap);
                  if (!nw) { fprintf (stderr, "romgen: out of memory\n"); rc = 2; break; }
                  wr = nw;
                  memset (img + cap, 0xFF, ncap - cap);   /* unused ROM = 0xFF */
                  memset (wr + cap, 0, ncap - cap);
                  cap = ncap;
                }
              img[off] = (unsigned char) val;
              wr[off] = 1;
              if (off + 1 > size) size = off + 1;
              { unsigned bank = (unsigned) (phys / 0x8000u); if (bank < 2048) bankhit[bank] = 1; }
            }
          if (rc) break;
        }
      else if (typ == 0x01)          /* EOF */
        break;
    }
  fclose (f);
  if (rc) { free (img); free (wr); return rc; }

  int n_banks = 0, bi;
  for (bi = 0; bi < 2048; bi++)
    if (bankhit[bi]) n_banks++;

  if (!opt_minx)
    {
      /* ---- flat .min ---- */
      FILE *o = fopen (outpath, "wb");
      if (!o) { perror (outpath); free (img); free (wr); return 2; }
      if (size && fwrite (img, 1, size, o) != size) { perror (outpath); fclose (o); free (img); free (wr); return 2; }
      fclose (o);
      printf ("wrote %s (%zu bytes, %d banks touched)\n", outpath, size, n_banks);
      free (img); free (wr);
      return 0;
    }

  /* ---- MINX container ---- */
  buf body = { NULL, 0, 0 }, strtab = { NULL, 0, 0 };
  unsigned char *mapdat = NULL, *noidat = NULL, *cdbdat = NULL;
  size_t mapsz = 0, noisz = 0, cdbsz = 0;
  struct minxsym *syms = NULL;
  struct arearec *areas = NULL;
  struct cdbinfo ci;
  uint32_t n_syms = 0, n_areas = 0, n_embedded_src = 0, n_funcs_emitted = 0, n_segs = 0, n_vars_emitted = 0;
  size_t p;
  char tmp[256];

  memset (&ci, 0, sizeof ci);
  ci.st = &strtab;

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
            { fprintf (stderr, "romgen: cannot read --%s file '%s'\n", side[i].what, side[i].arg); return 2; }
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

  /* IO register map: --io= must exist; the default (the toolchain's pm.h,
     found relative to this binary) is optional */
  struct iorec *io = NULL;
  uint32_t n_io = 0;
  if (!opt_noio)
    {
      unsigned char *iodat = NULL;
      size_t iosz = 0;
      if (io_arg)
        {
          iodat = read_file (io_arg, &iosz);
          if (!iodat)
            { fprintf (stderr, "romgen: cannot read --io file '%s'\n", io_arg); return 2; }
        }
      else
        {
          char *def = default_io_path (argv[0]);
          if (def) { iodat = read_file (def, &iosz); free (def); }
        }
      if (iodat)
        {
          io = parse_io ((const char *) iodat, iosz, &strtab, &n_io);
          if (n_io) qsort (io, n_io, sizeof *io, io_cmp);
          free (iodat);
        }
    }

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

  /* ROM — container of sparse SEG runs (only bytes the program defines) */
  p = ck_open (&body, "ROM ");
  {
    size_t off = 0;
    while (off < size)
      {
        if (!wr[off]) { off++; continue; }
        size_t start = off;
        while (off < size && wr[off]) off++;
        size_t sp2 = ck_open (&body, "SEG ");
        buf_u32 (&body, (uint32_t) (CART_BASE + start));
        buf_put (&body, img + start, off - start);
        ck_close (&body, sp2);
        n_segs++;
      }
  }
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

  /* TYPE — the deduped type graph */
  if (ci.n_types)
    {
      buf rec = { NULL, 0, 0 };
      for (i = 0; i < (int) ci.n_types; i++)
        {
          buf_u32 (&rec, ci.types[i].kind_flags);  buf_u32 (&rec, ci.types[i].size);
          buf_u32 (&rec, ci.types[i].target);      buf_u32 (&rec, ci.types[i].extra);
        }
      ck_blob (&body, "TYPE", rec.p, rec.len);
      free (rec.p);
    }

  /* STRU — one chunk per struct/union, ordinal order (TK_STRUCT.extra refs) */
  for (i = 0; i < (int) ci.n_structs; i++)
    {
      struct structdef *sd = &ci.structs[i];
      p = ck_open (&body, "STRU");
      buf_u32 (&body, sd->name_off);
      buf_u32 (&body, sd->size);
      buf_u32 (&body, sd->n_members);
      for (uint32_t m = 0; m < sd->n_members; m++)
        {
          buf_u32 (&body, sd->members[m].name);
          buf_u32 (&body, sd->members[m].offset);
          buf_u32 (&body, sd->members[m].type);
          buf_u32 (&body, 0);
        }
      ck_close (&body, p);
    }

  /* LINE — the global line table, sorted by physical address */
  if (ci.n_lines)
    {
      buf rec = { NULL, 0, 0 };
      for (i = 0; i < (int) ci.n_lines; i++)
        {
          buf_u32 (&rec, ci.lines[i].phys);  buf_u32 (&rec, ci.lines[i].file);
          buf_u32 (&rec, ci.lines[i].line);  buf_u32 (&rec, ci.lines[i].scope);
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
          uint32_t file = NO_REF, j;
          uint32_t fend = fn->have_end ? fn->end : fn->entry;
          for (j = 0; j < ci.n_lines; j++)
            if (ci.lines[j].phys >= fn->entry && ci.lines[j].phys <= fend)
              { file = ci.lines[j].file; break; }
          /* backtrace support: does the entry establish an IX frame?
             prologue = push ix ; ld ix,sp = A2 CF FA */
          if (fn->entry >= CART_BASE)
            {
              size_t eo = fn->entry - CART_BASE;
              if (eo + 3 <= size && wr[eo] && wr[eo+1] && wr[eo+2]
                  && img[eo] == 0xA2 && img[eo+1] == 0xCF && img[eo+2] == 0xFA)
                fn->flags |= FN_HASFRAME;
            }
          buf_u32 (&rec, strtab_add (&strtab, fn->ref.name));
          buf_u32 (&rec, fn->entry);
          buf_u32 (&rec, fend);
          buf_u32 (&rec, file);
          buf_u32 (&rec, fn->rettype);
          buf_u32 (&rec, fn->frame);
          buf_u32 (&rec, fn->flags);
          buf_u32 (&rec, 0);
          fn->emit_index = n_funcs_emitted++;
        }
      if (n_funcs_emitted)
        ck_blob (&body, "FUNC", rec.p, rec.len);
      free (rec.p);
    }

  /* VAR — globals + locals, sorted by (scope function, name). Local scope is
     resolved by function name (var scope "module.func" -> FUNC table index). */
  if (ci.n_vars)
    {
      struct emitvar { uint32_t scope, name, type, levelblock, loc_kind, loc, flags; };
      struct emitvar *ev = malloc ((size_t) ci.n_vars * sizeof *ev);
      if (!ev) { fprintf (stderr, "romgen: out of memory\n"); exit (2); }
      for (i = 0; i < (int) ci.n_vars; i++)
        {
          struct varinfo *v = &ci.vars[i];
          uint32_t scope = NO_REF;
          if (v->ref.kind == 'L')
            {
              const char *fname = strrchr (v->ref.scopename, '.');
              fname = fname ? fname + 1 : v->ref.scopename;
              for (uint32_t j = 0; j < ci.n_funcs; j++)
                if (ci.funcs[j].emit_index != NO_REF
                    && strcmp (ci.funcs[j].ref.name, fname) == 0)
                  { scope = ci.funcs[j].emit_index; break; }
            }
          ev[n_vars_emitted].scope = scope;
          ev[n_vars_emitted].name = v->name;
          ev[n_vars_emitted].type = v->type;
          ev[n_vars_emitted].levelblock = v->levelblock;
          ev[n_vars_emitted].loc_kind = v->loc_kind;
          ev[n_vars_emitted].loc = v->loc;
          ev[n_vars_emitted].flags = v->flags;
          n_vars_emitted++;
        }
      /* sort: globals (scope NO_REF) last, else by emitted FUNC index, then name */
      for (i = 1; i < (int) n_vars_emitted; i++)     /* insertion sort, n is small */
        {
          struct emitvar key = ev[i];
          int j = i - 1;
          while (j >= 0 && (ev[j].scope > key.scope
                            || (ev[j].scope == key.scope && ev[j].name > key.name)))
            { ev[j + 1] = ev[j]; j--; }
          ev[j + 1] = key;
        }
      buf rec = { NULL, 0, 0 };
      for (i = 0; i < (int) n_vars_emitted; i++)
        {
          buf_u32 (&rec, ev[i].name);       buf_u32 (&rec, ev[i].scope);
          buf_u32 (&rec, ev[i].type);       buf_u32 (&rec, ev[i].levelblock);
          buf_u32 (&rec, ev[i].loc_kind);   buf_u32 (&rec, ev[i].loc);
          buf_u32 (&rec, ev[i].flags);      buf_u32 (&rec, 0);
        }
      ck_blob (&body, "VAR ", rec.p, rec.len);
      free (rec.p);
      free (ev);
    }

  /* IO — the hardware register map (watch windows name the SFR space) */
  if (n_io)
    {
      buf rec = { NULL, 0, 0 };
      for (i = 0; i < (int) n_io; i++)
        {
          buf_u32 (&rec, io[i].name);  buf_u32 (&rec, io[i].addr);
          buf_u32 (&rec, io[i].size);  buf_u32 (&rec, io[i].flags);
        }
      ck_blob (&body, "IO  ", rec.p, rec.len);
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
        { fprintf (stderr, "romgen: cannot read --embed file '%s'\n", embeds[i].path); return 2; }
      p = ck_open (&body, "USR ");
      buf_u32 (&body, strtab_add (&strtab, embeds[i].name));
      buf_put (&body, edat, esz);
      ck_close (&body, p);
      free (edat);
    }

  /* NOTE — (key,val) string pairs; deterministic (no timestamps) */
  {
    buf rec = { NULL, 0, 0 };
    struct { const char *k; const char *v; } kv[14];
    int nkv = 0;
    char vbase[16], vrom[32], vbank[16], vseg[16], vsym[16], vline[16], vfunc[16], vvar[16], vtype[16], vcrc[16], vio[16];
    snprintf (vbase, sizeof vbase, "0x%04x", CART_BASE);
    snprintf (vrom,  sizeof vrom,  "%zu", size);
    snprintf (vcrc,  sizeof vcrc,  "0x%08x", crc32_buf (img, size));
    snprintf (vio,   sizeof vio,   "%u", n_io);
    snprintf (vbank, sizeof vbank, "%d", n_banks);
    snprintf (vseg,  sizeof vseg,  "%u", n_segs);
    snprintf (vsym,  sizeof vsym,  "%u", n_syms);
    snprintf (vline, sizeof vline, "%u", ci.n_lines);
    snprintf (vfunc, sizeof vfunc, "%u", n_funcs_emitted);
    snprintf (vvar,  sizeof vvar,  "%u", n_vars_emitted);
    snprintf (vtype, sizeof vtype, "%u", ci.n_types);
    kv[nkv].k = "generator"; kv[nkv++].v = "romgen (sdcc88)";
    kv[nkv].k = "source";    kv[nkv++].v = inpath;
    kv[nkv].k = "cart-base"; kv[nkv++].v = vbase;
    kv[nkv].k = "rom-bytes"; kv[nkv++].v = vrom;
    kv[nkv].k = "rom-crc32"; kv[nkv++].v = vcrc;   /* CRC-32 of the flat .min image */
    kv[nkv].k = "io-regs";   kv[nkv++].v = vio;
    kv[nkv].k = "banks";     kv[nkv++].v = vbank;
    kv[nkv].k = "segments";  kv[nkv++].v = vseg;
    kv[nkv].k = "symbols";   kv[nkv++].v = vsym;
    kv[nkv].k = "lines";     kv[nkv++].v = vline;
    kv[nkv].k = "functions"; kv[nkv++].v = vfunc;
    kv[nkv].k = "variables"; kv[nkv++].v = vvar;
    kv[nkv].k = "types";     kv[nkv++].v = vtype;
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
    if (!o) { perror (outpath); return 2; }
    memcpy (hdr, "MINX", 4);
    wle16 (hdr + 4, MINX_VERSION);
    wle16 (hdr + 6, 0);
    wle32 (hdr + 8, (uint32_t) (MINX_HDRSIZE + body.len));
    wle32 (hdr + 12, crc32_buf (body.p, body.len));
    if (fwrite (hdr, 1, sizeof hdr, o) != sizeof hdr
        || (body.len && fwrite (body.p, 1, body.len, o) != body.len)
        || fclose (o) != 0)
      { perror (outpath); return 2; }
  }

  printf ("wrote %s (minx: %zu ROM bytes in %u segments, %d banks, %u symbols, %u lines, "
          "%u functions, %u variables, %u types, %u/%u sources embedded)\n",
          outpath, size, n_segs, n_banks, n_syms, ci.n_lines,
          n_funcs_emitted, n_vars_emitted, ci.n_types, n_embedded_src, ci.n_files);

  free (img); free (wr);
  free (mapdat); free (noidat); free (cdbdat);
  free (syms); free (areas); free (io);
  for (i = 0; i < (int) ci.n_files; i++) free (ci.files[i]);
  for (i = 0; i < (int) ci.n_structs; i++)
    { free (ci.structs[i].name); free (ci.structs[i].rawmembers); free (ci.structs[i].members); }
  free (ci.files); free (ci.funcs); free (ci.vars); free (ci.lines); free (ci.types); free (ci.structs);
  free (body.p); free (strtab.p);
  return 0;
}
