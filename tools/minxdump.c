/*-------------------------------------------------------------------------
   minxdump.c — validate and dump a MINX container (romgen's debug bundle).

   The reference consumer for docs/s1c88/minx-format.md: reads the whole file
   into one buffer (no filesystem assumptions beyond loading the file itself,
   no text parsing), walks the chunk tree, validates structure + CRC + table
   invariants, and prints everything. An emulator debugger can lift the
   walking/lookup code directly.

     minxdump file.minx                # validate + dump
     minxdump --rom=out.min file.minx  # also reconstruct the flat ROM
                                       # (memset 0xFF + copy each SEG)

   Exit: 0 valid, 1 validation failure, 2 usage/IO error.
-------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MINX_HDRSIZE 16
#define CART_BASE 0x2100u
#define NO_REF 0xFFFFFFFFu

/* TYPE kinds (must match romgen.c / the spec) */
enum
{
  TK_VOID = 0, TK_CHAR, TK_SHORT, TK_INT, TK_LONG, TK_FLOAT, TK_SBIT,
  TK_BITFIELD, TK_STRUCT, TK_ARRAY, TK_FUNCTION, TK_POINTER
};
#define TF_UNSIGNED 0x100

enum { LOC_NONE = 0, LOC_STATIC, LOC_STACK, LOC_REGS };

static int failures = 0;

static void fail (const char *what)
{
  fprintf (stderr, "minxdump: INVALID: %s\n", what);
  failures++;
}

static uint32_t rle16 (const unsigned char *p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8); }
static uint32_t rle32 (const unsigned char *p) { return rle16 (p) | (rle16 (p + 2) << 16); }

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

/* the file, mapped once */
static const unsigned char *file;
static size_t filesz;

/* tables located up-front so every chunk can cross-reference */
static const unsigned char *strtab;   static uint32_t strtablen;
static const unsigned char *typetab;  static uint32_t n_types;
static const unsigned char *functab;  static uint32_t n_funcs;
static uint32_t rom_flat_crc;         static int have_rom_crc;

static const char *str (uint32_t off)
{
  if (!strtab || off >= strtablen)
    { fail ("string offset out of range"); return "?"; }
  return (const char *) strtab + off;   /* table is validated NUL-terminated */
}

/* chunk iteration: [*pos, end) is a sequence of chunks; returns 0 at end */
static int next_chunk (size_t *pos, size_t end, char id[5], size_t *payload, uint32_t *size)
{
  if (*pos + 8 > end)
    return 0;
  memcpy (id, file + *pos, 4);
  id[4] = '\0';
  *size = rle32 (file + *pos + 4);
  *payload = *pos + 8;
  if (*payload + *size > end)
    { fail ("chunk overruns its container"); return 0; }
  *pos = (*payload + *size + 3u) & ~3u;   /* pad to 4 (not counted in size) */
  return 1;
}

static int find_chunk (size_t pos, size_t end, const char *want, size_t *payload, uint32_t *size)
{
  char id[5];
  while (next_chunk (&pos, end, id, payload, size))
    if (memcmp (id, want, 4) == 0)
      return 1;
  return 0;
}

/* SRCS file names + STRU names, collected for cross-reference printing */
#define MAX_FILES   1024
#define MAX_STRUCTS 1024
static const char *files[MAX_FILES];      static uint32_t n_files = 0;
static uint32_t struct_names[MAX_STRUCTS]; static uint32_t n_structs = 0;

static const char *file_name (uint32_t id)
{
  if (id == NO_REF) return "<none>";
  if (id >= n_files) { fail ("file id out of range"); return "?"; }
  return files[id];
}

/* render a TYPE index as C-ish text, e.g. "unsigned char [3][4]", "struct point *" */
static void render_type (uint32_t idx, char *out, size_t n, int depth)
{
  if (idx == NO_REF) { snprintf (out, n, "?"); return; }
  if (idx >= n_types) { fail ("type index out of range"); snprintf (out, n, "?"); return; }
  if (depth > 12) { snprintf (out, n, "..."); return; }

  const unsigned char *t = typetab + (size_t) idx * 16;
  uint32_t kf = rle32 (t), target = rle32 (t + 8), extra = rle32 (t + 12);
  const char *u = (kf & TF_UNSIGNED) ? "unsigned " : "";
  char inner[96];

  switch (kf & 0xFF)
    {
    case TK_VOID:  snprintf (out, n, "void"); break;
    case TK_CHAR:  snprintf (out, n, "%schar", u); break;
    case TK_SHORT: snprintf (out, n, "%sshort", u); break;
    case TK_INT:   snprintf (out, n, "%sint", u); break;
    case TK_LONG:  snprintf (out, n, "%slong", u); break;
    case TK_FLOAT: snprintf (out, n, "float"); break;
    case TK_SBIT:  snprintf (out, n, "sbit"); break;
    case TK_BITFIELD:
      snprintf (out, n, "%sbits@%u:%u", u, extra & 0xFFFF, extra >> 16);
      break;
    case TK_STRUCT:
      if (extra != NO_REF && extra < n_structs)
        snprintf (out, n, "struct %s", str (struct_names[extra]));
      else
        snprintf (out, n, "struct ?");
      break;
    case TK_ARRAY:
      {
        /* collect dimensions outermost-first so char[3][4] reads like C */
        char dims[48] = "";
        uint32_t cur = idx;
        size_t dl = 0;
        while (cur != NO_REF && cur < n_types
               && (rle32 (typetab + (size_t) cur * 16) & 0xFF) == TK_ARRAY && dl < 32)
          {
            const unsigned char *a = typetab + (size_t) cur * 16;
            dl += (size_t) snprintf (dims + dl, sizeof dims - dl, "[%u]", rle32 (a + 12));
            cur = rle32 (a + 8);
          }
        render_type (cur, inner, sizeof inner, depth + 1);
        snprintf (out, n, "%s%s", inner, dims);
      }
      break;
    case TK_FUNCTION:
      render_type (target, inner, sizeof inner, depth + 1);
      snprintf (out, n, "fn() -> %s", inner);
      break;
    case TK_POINTER:
      render_type (target, inner, sizeof inner, depth + 1);
      snprintf (out, n, "%s%s *", inner, (extra == 'X') ? " __far" : "");
      break;
    default:
      fail ("unknown type kind");
      snprintf (out, n, "?");
    }
}

int main (int argc, char **argv)
{
  const char *inpath = NULL, *rompath = NULL;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (strncmp (argv[i], "--rom=", 6) == 0) rompath = argv[i] + 6;
      else if (argv[i][0] == '-')
        { fprintf (stderr, "usage: minxdump [--rom=out.min] file.minx\n"); return 2; }
      else if (!inpath) inpath = argv[i];
      else { fprintf (stderr, "usage: minxdump [--rom=out.min] file.minx\n"); return 2; }
    }
  if (!inpath)
    { fprintf (stderr, "usage: minxdump [--rom=out.min] file.minx\n"); return 2; }

  FILE *f = fopen (inpath, "rb");
  long fn;
  unsigned char *fb;
  if (!f) { perror (inpath); return 2; }
  if (fseek (f, 0, SEEK_END) != 0 || (fn = ftell (f)) < 0 || fseek (f, 0, SEEK_SET) != 0)
    { perror (inpath); fclose (f); return 2; }
  fb = malloc ((size_t) fn + 1);
  if (!fb) { fprintf (stderr, "minxdump: out of memory\n"); fclose (f); return 2; }
  if (fn && fread (fb, 1, (size_t) fn, f) != (size_t) fn)
    { perror (inpath); fclose (f); return 2; }
  fclose (f);
  fb[fn] = '\0';
  file = fb;
  filesz = (size_t) fn;

  /* --- header --- */
  if (filesz < MINX_HDRSIZE || memcmp (file, "MINX", 4) != 0)
    { fail ("not a MINX file (bad magic)"); return 1; }
  uint32_t version = rle16 (file + 4);
  uint32_t hdr_size = rle32 (file + 8);
  uint32_t hdr_crc = rle32 (file + 12);
  if (version != 1) fail ("unsupported version");
  if (hdr_size != filesz) fail ("header file-size field mismatch");
  if (crc32_buf (file + MINX_HDRSIZE, filesz - MINX_HDRSIZE) != hdr_crc)
    fail ("body CRC mismatch");
  printf ("MINX version %u, %zu bytes, crc %s\n",
          version, filesz, failures ? "BAD" : "ok");
  if (failures) return 1;

  size_t top = MINX_HDRSIZE, top_end = filesz;

  /* --- locate the cross-referenced tables first --- */
  {
    size_t pay; uint32_t sz;
    if (!find_chunk (top, top_end, "STR ", &pay, &sz) || sz == 0)
      fail ("missing/empty STR chunk");
    else
      {
        strtab = file + pay;
        strtablen = sz;
        if (strtab[0] != '\0')      fail ("STR offset 0 is not the empty string");
        if (strtab[sz - 1] != '\0') fail ("STR is not NUL-terminated");
      }
    if (find_chunk (top, top_end, "TYPE", &pay, &sz))
      {
        if (sz % 16) fail ("TYPE size not a multiple of 16");
        typetab = file + pay;
        n_types = sz / 16;
      }
    if (find_chunk (top, top_end, "FUNC", &pay, &sz))
      {
        if (sz % 32) fail ("FUNC size not a multiple of 32");
        functab = file + pay;
        n_funcs = sz / 32;
      }
    if (find_chunk (top, top_end, "SRCS", &pay, &sz))
      {
        size_t pos = pay, end = pay + sz, fpay; uint32_t fsz;
        char id[5];
        while (next_chunk (&pos, end, id, &fpay, &fsz))
          {
            if (memcmp (id, "FILE", 4) != 0) continue;
            size_t npay; uint32_t nsz;
            static char namebuf[MAX_FILES][256];
            if (n_files >= MAX_FILES) { fail ("too many FILE chunks"); break; }
            if (find_chunk (fpay, fpay + fsz, "NAME", &npay, &nsz) && nsz < 256)
              {
                memcpy (namebuf[n_files], file + npay, nsz);
                namebuf[n_files][nsz] = '\0';
              }
            else
              strcpy (namebuf[n_files], "?");
            files[n_files] = namebuf[n_files];
            n_files++;
          }
      }
    /* STRU chunks repeat at top level; collect names in ordinal order */
    {
      size_t pos = top; uint32_t csz; char id[5]; size_t cpay;
      while (next_chunk (&pos, top_end, id, &cpay, &csz))
        if (memcmp (id, "STRU", 4) == 0 && csz >= 12 && n_structs < MAX_STRUCTS)
          struct_names[n_structs++] = rle32 (file + cpay);
    }
    /* reconstruct the flat ROM's CRC so a NOTE rom-crc32 key can be verified */
    if (find_chunk (top, top_end, "ROM ", &pay, &sz))
      {
        size_t spos = pay, send = pay + sz, spay; uint32_t ssz;
        char sid[5];
        uint32_t maxend = 0;
        while (next_chunk (&spos, send, sid, &spay, &ssz))
          if (memcmp (sid, "SEG ", 4) == 0 && ssz >= 4)
            {
              uint32_t phys = rle32 (file + spay), len = ssz - 4;
              if (phys >= CART_BASE && phys - CART_BASE + len > maxend)
                maxend = phys - CART_BASE + len;
            }
        unsigned char *flat = malloc (maxend ? maxend : 1);
        if (flat)
          {
            memset (flat, 0xFF, maxend);
            spos = pay;
            while (next_chunk (&spos, send, sid, &spay, &ssz))
              if (memcmp (sid, "SEG ", 4) == 0 && ssz >= 4)
                {
                  uint32_t phys = rle32 (file + spay), len = ssz - 4;
                  if (phys >= CART_BASE && phys - CART_BASE + len <= maxend)
                    memcpy (flat + (phys - CART_BASE), file + spay + 4, len);
                }
            rom_flat_crc = crc32_buf (flat, maxend);
            have_rom_crc = 1;
            free (flat);
          }
      }
  }

  /* --- walk + dump top-level chunks --- */
  {
    size_t pos = top, pay; uint32_t sz;
    char id[5];
    uint32_t stru_ord = 0;
    while (next_chunk (&pos, top_end, id, &pay, &sz))
      {
        if (memcmp (id, "ROM ", 4) == 0)
          {
            /* container of SEG runs: {u32 phys} + bytes */
            size_t spos = pay, send = pay + sz, spay; uint32_t ssz;
            char sid[5];
            uint32_t nseg = 0, maxend = 0, prevend = 0, total = 0;
            printf ("ROM\n");
            while (next_chunk (&spos, send, sid, &spay, &ssz))
              {
                if (memcmp (sid, "SEG ", 4) != 0 || ssz < 4)
                  { fail ("non-SEG child in ROM"); continue; }
                uint32_t phys = rle32 (file + spay), len = ssz - 4;
                if (phys < CART_BASE)        fail ("SEG below the cart base");
                if (phys < prevend)          fail ("SEG overlaps/unsorted");
                prevend = phys + len;
                if (prevend - CART_BASE > maxend) maxend = prevend - CART_BASE;
                total += len;
                printf ("  SEG 0x%06x..0x%06x (%u bytes)\n", phys, phys + len - 1, len);
                nseg++;
              }
            printf ("  %u segments, %u defined bytes, flat extent %u bytes\n", nseg, total, maxend);
            if (rompath)
              {
                unsigned char *flat = malloc (maxend ? maxend : 1);
                if (!flat) { fprintf (stderr, "minxdump: out of memory\n"); return 2; }
                memset (flat, 0xFF, maxend);
                spos = pay;
                while (next_chunk (&spos, send, sid, &spay, &ssz))
                  if (memcmp (sid, "SEG ", 4) == 0 && ssz >= 4)
                    {
                      uint32_t phys = rle32 (file + spay), len = ssz - 4;
                      if (phys >= CART_BASE && phys - CART_BASE + len <= maxend)
                        memcpy (flat + (phys - CART_BASE), file + spay + 4, len);
                    }
                FILE *o = fopen (rompath, "wb");
                if (!o || (maxend && fwrite (flat, 1, maxend, o) != maxend) || fclose (o) != 0)
                  { perror (rompath); return 2; }
                free (flat);
              }
          }
        else if (memcmp (id, "AREA", 4) == 0)
          {
            if (sz % 16) fail ("AREA size not a multiple of 16");
            printf ("AREA   %u areas\n", sz / 16);
            for (uint32_t r = 0; r + 16 <= sz; r += 16)
              {
                const unsigned char *rec = file + pay + r;
                uint32_t fl = rle32 (rec + 12);
                printf ("  %-24s base 0x%06x size 0x%06x%s%s\n",
                        str (rle32 (rec)), rle32 (rec + 4), rle32 (rec + 8),
                        (fl & 1) ? " ABS" : "", (fl & 2) ? " OVR" : "");
              }
          }
        else if (memcmp (id, "SYM ", 4) == 0)
          {
            if (sz % 16) fail ("SYM size not a multiple of 16");
            printf ("SYM    %u symbols\n", sz / 16);
            uint32_t prev = 0;
            for (uint32_t r = 0; r + 16 <= sz; r += 16)
              {
                const unsigned char *rec = file + pay + r;
                uint32_t value = rle32 (rec + 4), phys = rle32 (rec + 8), fl = rle32 (rec + 12);
                if (value < prev) fail ("SYM not sorted by value");
                prev = value;
                printf ("  %-32s value 0x%06x", str (rle32 (rec)), value);
                if (fl & 1) printf (" phys 0x%06x", phys);
                if (fl & 2) printf (" local");
                printf ("\n");
              }
          }
        else if (memcmp (id, "TYPE", 4) == 0)
          {
            printf ("TYPE   %u types\n", n_types);
            char txt[160];
            for (uint32_t r = 0; r < n_types; r++)
              {
                const unsigned char *rec = file + pay + (size_t) r * 16;
                render_type (r, txt, sizeof txt, 0);
                printf ("  [%u] %-32s size %u\n", r, txt, rle32 (rec + 4));
              }
          }
        else if (memcmp (id, "STRU", 4) == 0)
          {
            if (sz < 12 || (sz - 12) % 16) { fail ("bad STRU payload"); continue; }
            uint32_t nm = rle32 (file + pay), ssize = rle32 (file + pay + 4),
                     count = rle32 (file + pay + 8);
            if (count != (sz - 12) / 16) fail ("STRU member count mismatch");
            printf ("STRU   [%u] struct %s  size %u, %u members\n", stru_ord++, str (nm), ssize, count);
            char txt[160];
            for (uint32_t m = 0; m < count && 12 + (m + 1) * 16 <= sz; m++)
              {
                const unsigned char *rec = file + pay + 12 + (size_t) m * 16;
                render_type (rle32 (rec + 8), txt, sizeof txt, 0);
                printf ("    +%-4u %-24s %s\n", rle32 (rec + 4), str (rle32 (rec)), txt);
              }
          }
        else if (memcmp (id, "LINE", 4) == 0)
          {
            if (sz % 16) fail ("LINE size not a multiple of 16");
            printf ("LINE   %u lines\n", sz / 16);
            uint32_t prev = 0;
            for (uint32_t r = 0; r + 16 <= sz; r += 16)
              {
                const unsigned char *rec = file + pay + r;
                uint32_t phys = rle32 (rec), fid = rle32 (rec + 4), lno = rle32 (rec + 8);
                uint32_t scope = rle32 (rec + 12);
                if (phys < prev) fail ("LINE not sorted by address");
                prev = phys;
                printf ("  0x%06x  %s:%u  (block %u level %u)\n",
                        phys, file_name (fid), lno, scope & 0xFFFF, (scope >> 16) & 0xFF);
              }
          }
        else if (memcmp (id, "FUNC", 4) == 0)
          {
            printf ("FUNC   %u functions\n", n_funcs);
            uint32_t prev = 0;
            char txt[160];
            for (uint32_t r = 0; r < n_funcs; r++)
              {
                const unsigned char *rec = file + pay + (size_t) r * 32;
                uint32_t entry = rle32 (rec + 4), fend = rle32 (rec + 8), fid = rle32 (rec + 12);
                uint32_t ret = rle32 (rec + 16), frame = rle32 (rec + 20), fl = rle32 (rec + 24);
                if (entry < prev) fail ("FUNC not sorted by entry");
                if (fend < entry) fail ("FUNC end before entry");
                prev = entry;
                render_type (ret, txt, sizeof txt, 0);
                printf ("  %-28s 0x%06x..0x%06x  %s  -> %s, frame %u%s%s%s",
                        str (rle32 (rec)), entry, fend, file_name (fid), txt, frame,
                        (fl & 0x1) ? ", static" : "", (fl & 0x2) ? ", interrupt" : "",
                        (fl & 0x4) ? ", frame-ptr" : "");
                if (fl & 0x2) printf (" %u", (fl >> 8) & 0xFF);
                printf ("\n");
              }
          }
        else if (memcmp (id, "VAR ", 4) == 0)
          {
            if (sz % 32) fail ("VAR size not a multiple of 32");
            printf ("VAR    %u variables\n", sz / 32);
            char txt[160];
            for (uint32_t r = 0; r + 32 <= sz; r += 32)
              {
                const unsigned char *rec = file + pay + r;
                uint32_t scope = rle32 (rec + 4), ty = rle32 (rec + 8);
                uint32_t lk = rle32 (rec + 16), loc = rle32 (rec + 20), fl = rle32 (rec + 24);
                const char *sc = "<global>";
                if (fl & 0x1) sc = "<file-static>";
                if (scope != NO_REF)
                  {
                    if (functab && scope < n_funcs)
                      sc = str (rle32 (functab + (size_t) scope * 32));
                    else
                      { fail ("VAR scope function index out of range"); sc = "?"; }
                  }
                render_type (ty, txt, sizeof txt, 0);
                printf ("  %-24s in %-16s %-28s ", str (rle32 (rec)), sc, txt);
                switch (lk)
                  {
                  case LOC_STATIC: printf ("@0x%06x", loc); break;
                  case LOC_STACK:  printf ("ix%+d", (int32_t) loc); break;
                  case LOC_REGS:   printf ("reg %s", str (loc)); break;
                  case LOC_NONE:   printf ("-"); break;
                  default: fail ("unknown VAR location kind"); printf ("?");
                  }
                if (fl & 0x2) printf ("  (artificial)");
                printf ("\n");
              }
          }
        else if (memcmp (id, "SRCS", 4) == 0)
          {
            printf ("SRCS   %u files\n", n_files);
            size_t fpos = pay, fend2 = pay + sz, fpay; uint32_t fsz;
            char fid[5];
            uint32_t ord = 0;
            while (next_chunk (&fpos, fend2, fid, &fpay, &fsz))
              {
                if (memcmp (fid, "FILE", 4) != 0) continue;
                size_t tpay; uint32_t tsz = 0;
                int has_text = find_chunk (fpay, fpay + fsz, "TEXT", &tpay, &tsz);
                printf ("  [%u] %-28s %s (%u bytes)\n", ord, file_name (ord),
                        has_text ? "embedded" : "name only", has_text ? tsz : 0);
                ord++;
              }
          }
        else if (memcmp (id, "USR ", 4) == 0)
          {
            if (sz < 4) { fail ("short USR chunk"); continue; }
            printf ("USR    %-28s %u bytes\n", str (rle32 (file + pay)), sz - 4);
          }
        else if (memcmp (id, "IO  ", 4) == 0)
          {
            if (sz % 16) fail ("IO size not a multiple of 16");
            printf ("IO     %u registers\n", sz / 16);
            uint32_t prev = 0;
            for (uint32_t r = 0; r + 16 <= sz; r += 16)
              {
                const unsigned char *rec = file + pay + r;
                uint32_t addr = rle32 (rec + 4);
                if (addr < prev) fail ("IO not sorted by address");
                prev = addr;
                printf ("  %-24s 0x%06x size %u\n", str (rle32 (rec)), addr, rle32 (rec + 8));
              }
          }
        else if (memcmp (id, "NOTE", 4) == 0)
          {
            if (sz % 8) fail ("NOTE size not a multiple of 8");
            printf ("NOTE   %u entries\n", sz / 8);
            for (uint32_t r = 0; r + 8 <= sz; r += 8)
              {
                const char *k = str (rle32 (file + pay + r));
                const char *v = str (rle32 (file + pay + r + 4));
                printf ("  %s = %s\n", k, v);
                if (strcmp (k, "rom-crc32") == 0 && have_rom_crc
                    && (uint32_t) strtoul (v, NULL, 0) != rom_flat_crc)
                  fail ("rom-crc32 does not match the SEG-reconstructed flat ROM");
              }
          }
        else if (memcmp (id, "STR ", 4) == 0)
          printf ("STR    %u bytes\n", sz);
        else
          printf ("%.4s   %u bytes (unknown chunk, skipped)\n", id, sz);
      }
  }

  if (failures)
    {
      fprintf (stderr, "minxdump: %d validation failure(s)\n", failures);
      return 1;
    }
  printf ("ok\n");
  return 0;
}
