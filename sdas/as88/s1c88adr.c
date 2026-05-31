/* s1c88adr.c — Epson S1C88 operand parser for sdas (sdcc88). */

/*
 *  Derived from z80adr.c of the ASxxxx assembler suite.
 *  Copyright (C) 1989-2009  Alan R. Baldwin
 *  S1C88 retarget (C) 2026, part of sdcc88.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.  See the GNU GPL for details.
 */

#include "asxxxx.h"
#include "s1c88.h"

/*
 * Read one operand into `esp' and return its addressing mode.
 *
 *	#n		S_IMMED		(value in expr)
 *	a/b/l/h		S_R8		(e_addr = register code)
 *	ba/hl/ix/iy/sp	S_R16		(e_addr = register code)
 *	label		S_USER		(value in expr)
 *
 * Memory-indirect forms ((hl), (ix+d), (hhll)) are added in a later version.
 */
int
addr(esp)
struct expr *esp;
{
	int c, indx;

	if ((c = getnb()) == '#') {
		expr(esp, 0);
		esp->e_mode = S_IMMED;
	} else {
		unget(c);
		if ((indx = admode(R8)) != 0) {
			esp->e_mode = S_R8;
			esp->e_addr = indx & 0xFF;
			esp->e_base.e_ap = NULL;
		} else
		if ((indx = admode(R16)) != 0) {
			esp->e_mode = S_R16;
			esp->e_addr = indx & 0xFF;
			esp->e_base.e_ap = NULL;
		} else {
			expr(esp, 0);
			esp->e_mode = S_USER;
		}
	}
	return (esp->e_mode);
}

/*
 * Search a specific addressing-mode table for a match at the input pointer.
 * Returns the table value (with the 0400 present-flag) on a match, else 0.
 */
int
admode(sp)
struct adsym *sp;
{
	char *ptr;
	int i;
	char *ips;

	ips = ip;
	unget(getnb());

	i = 0;
	while ( *(ptr = &sp[i].a_str[0]) ) {
		if (srch(ptr)) {
			return(sp[i].a_val);
		}
		i++;
	}
	ip = ips;
	return(0);
}

/*
 *	srch --- does the string at the input pointer match `str' ?
 */
int
srch(str)
char *str;
{
	char *ptr;
	ptr = ip;

	while (*ptr && *str) {
		if (ccase[*ptr & 0x007F] != ccase[*str & 0x007F])
			break;
		ptr++;
		str++;
	}
	if (ccase[*ptr & 0x007F] == ccase[*str & 0x007F]) {
		ip = ptr;
		return(1);
	}

	if (!*str)
		if (!(ctype[*ptr & 0x007F] & LTR16)) {
			ip = ptr;
			return(1);
		}
	return(0);
}

/*
 * Registers.  The 0400 bit marks "present" so a code of 0 (A, BA) still
 * returns nonzero from admode(); addr() masks it off with & 0xFF.
 */

struct	adsym	R8[] = {
    {	"a",	A|0400	},
    {	"b",	B|0400	},
    {	"l",	L|0400	},
    {	"h",	H|0400	},
    {	"",	0000	}
};

struct	adsym	R16[] = {
    {	"ba",	BA|0400	},
    {	"hl",	HL|0400	},
    {	"ix",	IX|0400	},
    {	"iy",	IY|0400	},
    {	"sp",	SP|0400	},
    {	"",	0000	}
};

/*
 * Condition codes (longer prefixes first so "nz"/"nc" match before "z"/"c").
 */
struct	adsym	CND[] = {
    {	"nz",	CC_NZ|0400	},
    {	"nc",	CC_NC|0400	},
    {	"z",	CC_Z|0400	},
    {	"c",	CC_C|0400	},
    {	"",	0000		}
};
