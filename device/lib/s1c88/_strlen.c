/*-------------------------------------------------------------------------
   _strlen.c - part of the s1c88 string library

   SDCC 4.5.0 does not ship a generic device/lib/_strlen.c (strlen is provided
   only as hand asm by a few ports, none of them s1c88), so the s1c88 runtime
   had no strlen at all — neither a library member nor a codegen builtin. This
   repo-owned source fills that gap; build-runtime.sh prefers it over the
   (absent) upstream copy. Compiled through the s1c88 port like the other
   s1c88.lib members.
-------------------------------------------------------------------------*/

#include <string.h>

#undef strlen /* avoid conflict with any wrapper macro of the same name */

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p)
        p++;

    return (size_t)(p - s);
}
