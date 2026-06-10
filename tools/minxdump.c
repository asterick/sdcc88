/*-------------------------------------------------------------------------
   minxdump.c — validate and dump a MINX container (romgen's debug bundle).

   The reference consumer for docs/s1c88/minx-format.md: reads the whole file
   from one buffer (no filesystem assumptions beyond loading the file itself,
   no text parsing), walks the chunk tree, validates structure + CRC + table
   invariants, and prints everything. An emulator debugger can lift the
   walking/lookup code directly.

     minxdump file.minx              # validate + dump
     minxdump --rom=out.min file.minx  # also extract the flat ROM payload

   Exit: 0 valid, 1 validation failure, 2 usage/IO error.
-------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MINX_HDRSIZE 16

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

/* the string table, located up-front so every chunk can resolve names */
static const unsigned char *strtab;
static uint32_t strtablen;

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

/* SRCS file names, collected so LINE/FUNC can print and bounds-check ids */
#define MAX_FILES 1024
static const char *files[MAX_FILES];
static uint32_t n_files = 0;

static const char *file_name (uint32_t id)
{
  if (id == 0xFFFFFFFFu) return "<none>";
  if (id >= n_files) { fail ("file id out of range"); return "?"; }
  return files[id];
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
  long n;
  unsigned char *fb;
  if (!f) { perror (inpath); return 2; }
  if (fseek (f, 0, SEEK_END) != 0 || (n = ftell (f)) < 0 || fseek (f, 0, SEEK_SET) != 0)
    { perror (inpath); fclose (f); return 2; }
  fb = malloc ((size_t) n + 1);
  if (!fb) { fprintf (stderr, "minxdump: out of memory\n"); fclose (f); return 2; }
  if (n && fread (fb, 1, (size_t) n, f) != (size_t) n)
    { perror (inpath); fclose (f); return 2; }
  fclose (f);
  fb[n] = '\0';
  file = fb;
  filesz = (size_t) n;

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

  /* --- locate STR first: everything else resolves names through it --- */
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
  }

  /* --- pre-scan SRCS for file names (LINE/FUNC reference them by ordinal) --- */
  {
    size_t pay; uint32_t sz;
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
  }

  /* --- walk + dump top-level chunks --- */
  {
    size_t pos = top, pay; uint32_t sz;
    char id[5];
    while (next_chunk (&pos, top_end, id, &pay, &sz))
      {
        if (memcmp (id, "ROM ", 4) == 0)
          {
            if (sz < 4) { fail ("short ROM chunk"); continue; }
            uint32_t load = rle32 (file + pay);
            printf ("ROM    %u bytes, load 0x%06x\n", sz - 4, load);
            if (rompath)
              {
                FILE *o = fopen (rompath, "wb");
                if (!o || fwrite (file + pay + 4, 1, sz - 4, o) != sz - 4 || fclose (o) != 0)
                  { perror (rompath); return 2; }
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
        else if (memcmp (id, "LINE", 4) == 0)
          {
            if (sz % 16) fail ("LINE size not a multiple of 16");
            printf ("LINE   %u lines\n", sz / 16);
            uint32_t prev = 0;
            for (uint32_t r = 0; r + 16 <= sz; r += 16)
              {
                const unsigned char *rec = file + pay + r;
                uint32_t phys = rle32 (rec), fid = rle32 (rec + 4), lno = rle32 (rec + 8);
                if (phys < prev) fail ("LINE not sorted by address");
                prev = phys;
                printf ("  0x%06x  %s:%u\n", phys, file_name (fid), lno);
              }
          }
        else if (memcmp (id, "FUNC", 4) == 0)
          {
            if (sz % 16) fail ("FUNC size not a multiple of 16");
            printf ("FUNC   %u functions\n", sz / 16);
            uint32_t prev = 0;
            for (uint32_t r = 0; r + 16 <= sz; r += 16)
              {
                const unsigned char *rec = file + pay + r;
                uint32_t entry = rle32 (rec + 4), fend = rle32 (rec + 8), fid = rle32 (rec + 12);
                if (entry < prev) fail ("FUNC not sorted by entry");
                if (fend < entry) fail ("FUNC end before entry");
                prev = entry;
                printf ("  %-32s 0x%06x..0x%06x  %s\n",
                        str (rle32 (rec)), entry, fend, file_name (fid));
              }
          }
        else if (memcmp (id, "SRCS", 4) == 0)
          {
            printf ("SRCS   %u files\n", n_files);
            size_t fpos = pay, fend = pay + sz, fpay; uint32_t fsz;
            char fid[5];
            uint32_t ord = 0;
            while (next_chunk (&fpos, fend, fid, &fpay, &fsz))
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
        else if (memcmp (id, "NOTE", 4) == 0)
          {
            if (sz % 8) fail ("NOTE size not a multiple of 8");
            printf ("NOTE   %u entries\n", sz / 8);
            for (uint32_t r = 0; r + 8 <= sz; r += 8)
              printf ("  %s = %s\n", str (rle32 (file + pay + r)), str (rle32 (file + pay + r + 4)));
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
