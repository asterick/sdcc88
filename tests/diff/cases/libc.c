/* libc — differential coverage of the s1c88.lib string/memory subset vs host libc.
 *
 * Exercises the small C-library subset user code links from s1c88.lib, plus the
 * memcpy/memset the codegen inlines as builtins, against the host's native libc.
 *
 * SOUNDNESS (library semantics differ in IMPLEMENTATION-DEFINED corners — emit
 * only the bits the C standard pins down):
 *   - strcmp/strncmp/memcmp return a value whose SIGN is defined but whose
 *     MAGNITUDE is implementation-defined (glibc returns the byte difference;
 *     the s1c88 lib may return ±1). So normalize to sgn() = {-1,0,1} and emit
 *     that, never the raw int.
 *   - strchr/strrchr/strstr/memchr return a POINTER; emit its byte OFFSET from
 *     the buffer (a pointer difference — host/target sizeof cancels) or a 0xFFFF
 *     sentinel for "not found", never the raw pointer bit pattern.
 *   - After a copy/cat/set, emit the resulting BYTES (logical values, identical
 *     on both ends), never sizeof or the buffer address.
 *   - There is no target <string.h>; declare the subset ourselves with target
 *     widths (size_t == unsigned, 16-bit). strlen is intentionally absent — the
 *     target has no implementation (no lib member, no builtin).
 */
#include "harness.h"

/* memcpy/memset have no s1c88.lib member — the codegen provides them ONLY as
   inline builtins, and a plain prototyped call would emit an (unresolvable)
   external _memcpy/_memset. __builtin_memcpy/__builtin_memset is the real target
   path and is inlined identically by gcc on the host, so route through it. */
#define MEMCPY  __builtin_memcpy
#define MEMSET  __builtin_memset

#ifdef DIFF_HOST
#include <string.h>
#else
void *memmove(void *, const void *, unsigned);
int   memcmp(const void *, const void *, unsigned);
void *memchr(const void *, int, unsigned);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, unsigned);
char *strcat(char *, const char *);
char *strncat(char *, const char *, unsigned);
int   strcmp(const char *, const char *);
int   strncmp(const char *, const char *, unsigned);
char *strchr(const char *, int);
char *strrchr(const char *, int);
char *strstr(const char *, const char *);
#endif

/* defined sign, impl-defined magnitude -> normalize to {-1,0,1} */
static i8 sgn(int r) { return (i8)((r > 0) - (r < 0)); }

/* byte offset of a result pointer into base, or 0xFFFF if NULL */
static u16 off(const char *base, const char *p) { return p ? (u16)(p - base) : 0xFFFFu; }

void diff_run(void)
{
    u8 i;
    char a[24], b[24];

    /* --- memset --- */
    MEMSET(a, 0x5a, 8);
    EMIT_U8("memset0", a[0]); EMIT_U8("memset7", a[7]);
    MEMSET(a, 0, sizeof a);

    /* --- memcpy (forward, builtin) --- */
    MEMCPY(a, "ABCDEFGH", 8);
    for (i = 0; i < 8; i++) EMIT_U8("memcpy", a[i]);

    /* --- memmove with overlap, both directions --- */
    MEMCPY(a, "0123456789", 10);
    memmove(a + 2, a, 5);                 /* dst > src: must copy as if buffered */
    for (i = 0; i < 10; i++) EMIT_U8("mmv_up", a[i]);
    MEMCPY(a, "0123456789", 10);
    memmove(a, a + 2, 5);                 /* dst < src */
    for (i = 0; i < 10; i++) EMIT_U8("mmv_dn", a[i]);

    /* --- memcmp (equal / less / greater), sign only --- */
    MEMCPY(a, "needle", 7);
    MEMCPY(b, "needle", 7);
    EMIT_I8("mcmp_eq", sgn(memcmp(a, b, 7)));
    b[3] = 'X';                            /* 'X'(0x58) < 'd'(0x64) -> a > b */
    EMIT_I8("mcmp_gt", sgn(memcmp(a, b, 7)));
    EMIT_I8("mcmp_lt", sgn(memcmp(b, a, 7)));
    EMIT_I8("mcmp_n0", sgn(memcmp(a, b, 3)));  /* prefix equal -> 0 */

    /* --- memchr (found at offset / not found) --- */
    MEMCPY(a, "haystack", 9);
    EMIT_U16("mchr_k", off(a, (char *)memchr(a, 'k', 8)));
    EMIT_U16("mchr_x", off(a, (char *)memchr(a, 'z', 8)));

    /* --- strcpy / strncpy --- */
    strcpy(a, "hello");
    for (i = 0; i < 6; i++) EMIT_U8("scpy", a[i]);     /* includes the NUL */
    strncpy(a, "AB", 5);                   /* zero-pads to 5 */
    for (i = 0; i < 5; i++) EMIT_U8("sncpy_pad", a[i]);
    strncpy(a, "ABCDEF", 3);               /* truncates, no NUL written */
    for (i = 0; i < 3; i++) EMIT_U8("sncpy_trunc", a[i]);

    /* --- strcat / strncat --- */
    strcpy(a, "foo");
    strcat(a, "bar");
    for (i = 0; i < 7; i++) EMIT_U8("scat", a[i]);     /* "foobar\0" */
    strcpy(a, "foo");
    strncat(a, "barbaz", 3);               /* append 3 -> "foobar\0" */
    for (i = 0; i < 7; i++) EMIT_U8("sncat", a[i]);

    /* --- strcmp / strncmp, sign only --- */
    EMIT_I8("scmp_eq", sgn(strcmp("abc", "abc")));
    EMIT_I8("scmp_lt", sgn(strcmp("abc", "abd")));
    EMIT_I8("scmp_gt", sgn(strcmp("abd", "abc")));
    EMIT_I8("scmp_pre", sgn(strcmp("ab", "abc")));     /* shorter -> less */
    EMIT_I8("sncmp_eq", sgn(strncmp("abcZ", "abcQ", 3)));
    EMIT_I8("sncmp_lt", sgn(strncmp("abc", "abd", 3)));

    /* --- strchr / strrchr (offset / not found) --- */
    strcpy(a, "abcabc");
    EMIT_U16("schr", off(a, strchr(a, 'b')));          /* first 'b' = 1 */
    EMIT_U16("srchr", off(a, strrchr(a, 'b')));        /* last  'b' = 4 */
    EMIT_U16("schr_x", off(a, strchr(a, 'z')));        /* not found */
    EMIT_U16("schr_nul", off(a, strchr(a, '\0')));     /* points at terminator (6) */

    /* --- strstr (offset / not found) --- */
    strcpy(a, "the quick brown");
    EMIT_U16("sstr", off(a, strstr(a, "quick")));      /* 4 */
    EMIT_U16("sstr_hd", off(a, strstr(a, "the")));     /* 0 */
    EMIT_U16("sstr_x", off(a, strstr(a, "slow")));     /* not found */
    EMIT_U16("sstr_e", off(a, strstr(a, "")));         /* empty matches at 0 */
}
