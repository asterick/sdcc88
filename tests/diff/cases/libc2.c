/* libc2 — differential coverage of the s1c88.lib stdlib/ctype additions vs host.
 *
 * Covers the standard, differentially-testable part of the Tier-1 libc expansion:
 * ctype classification + case mapping, atoi/atol, abs/labs, strspn/strcspn/strpbrk/
 * strtok, memccpy.
 *
 * SOUNDNESS:
 *   - ctype isXXX() return an int whose nonzero-ness is defined but whose exact
 *     value is impl-defined -> emit the BOOLEAN (!!r), not the raw int.
 *   - keep atoi results inside int16 range (host int is 32-bit, target 16-bit).
 *   - pointer results -> byte offset / 0xFFFF sentinel (sizeof cancels).
 *
 * Build-verified but not differentially tested here: itoa/ltoa/uitoa/ultoa (the
 * non-standard __itoa/__ltoa, no host equivalent), qsort/bsearch (their __reentrant
 * comparator type can't be expressed in a host-cpp'd source), and rand/srand (a PRNG
 * stream is not host/target comparable). These link and run; they just aren't diffed.
 */
#include "harness.h"

#ifdef DIFF_HOST
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
/* itoa/ltoa are non-standard; on the host, shim base-10 via sprintf so both ends
   produce the same decimal text. */
#else
char *strcpy(char *, const char *);
int atoi(const char *);
long atol(const char *);
int abs(int);
long labs(long);
unsigned strspn(const char *, const char *);
unsigned strcspn(const char *, const char *);
char *strpbrk(const char *, const char *);
char *strtok(char *, const char *);
void *memccpy(void *, const void *, int, unsigned);
int isalnum(int), isalpha(int), isblank(int), iscntrl(int), isdigit(int), isgraph(int);
int islower(int), isprint(int), ispunct(int), isspace(int), isupper(int), isxdigit(int);
int tolower(int), toupper(int);
#endif

static u16 off(const char *base, const char *p) { return p ? (u16)(p - base) : 0xFFFFu; }

void diff_run(void)
{
    u8 i;
    char a[24], b[24];

    /* --- ctype over the ASCII range (boolean classification + case map) --- */
    for (i = 0; i < 128; i++) {
        EMIT_U8("isalnum", !!isalnum(i));  EMIT_U8("isalpha", !!isalpha(i));
        EMIT_U8("isblank", !!isblank(i));  EMIT_U8("iscntrl", !!iscntrl(i));
        EMIT_U8("isdigit", !!isdigit(i));  EMIT_U8("isgraph", !!isgraph(i));
        EMIT_U8("islower", !!islower(i));  EMIT_U8("isprint", !!isprint(i));
        EMIT_U8("ispunct", !!ispunct(i));  EMIT_U8("isspace", !!isspace(i));
        EMIT_U8("isupper", !!isupper(i));  EMIT_U8("isxdigit", !!isxdigit(i));
        EMIT_U8("tolower", (u8)tolower(i)); EMIT_U8("toupper", (u8)toupper(i));
    }

    /* --- atoi / atol (kept in range; signs, spaces, trailing junk) --- */
    EMIT_I16("atoi_p", (i16)atoi("12345"));
    EMIT_I16("atoi_n", (i16)atoi("-9876"));
    EMIT_I16("atoi_sp", (i16)atoi("   42abc"));
    EMIT_I16("atoi_z", (i16)atoi("nope"));
    EMIT_I32("atol_p", (i32)atol("123456789"));
    EMIT_I32("atol_n", (i32)atol("-2000000000"));

    /* --- abs / labs --- */
    EMIT_I16("abs", (i16)abs(-31000));   EMIT_I16("abs2", (i16)abs(123));
    EMIT_I32("labs", (i32)labs(-2000000000L));

    /* --- strspn / strcspn / strpbrk --- */
    EMIT_U16("sspn", (u16)strspn("aabbcXY", "abc"));        /* 5 */
    EMIT_U16("scspn", (u16)strcspn("hello world", " "));    /* 5 */
    strcpy(a, "find.the:sep");
    EMIT_U16("spbrk", off(a, strpbrk(a, ":.")));            /* 4 */
    EMIT_U16("spbrk_x", off(a, strpbrk(a, "0123456789")));  /* none */

    /* --- strtok (sequential tokens; emit each token's first char) --- */
    strcpy(a, "a,bb,,ccc");
    { char *t = strtok(a, ","); EMIT_U8("tok1", t ? t[0] : 0xFF);
      t = strtok(0, ",");       EMIT_U8("tok2", t ? t[0] : 0xFF);
      t = strtok(0, ",");       EMIT_U8("tok3", t ? t[0] : 0xFF);
      t = strtok(0, ",");       EMIT_U8("tok4", t ? (u8)0 : 0xFF); }

    /* --- memccpy (stop after copying the delimiter) --- */
    for (i = 0; i < 8; i++) b[i] = 0;
    { void *e = memccpy(b, "ab|cd", '|', 8);
      EMIT_U16("mccpy_end", off(b, (char *)e));   /* 3: one past '|' */
      for (i = 0; i < 3; i++) EMIT_U8("mccpy", b[i]); }
}
