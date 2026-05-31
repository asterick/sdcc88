/* s1c88.h — Epson S1C88 instruction definitions for sdas (sdcc88). */

/*
 *  Derived from z80.h of the ASxxxx assembler suite.
 *  Copyright (C) 1989-2009  Alan R. Baldwin
 *  S1C88 retarget (C) 2026, part of sdcc88.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

/*
 * Indirect addressing delimiters (the codegen emits sdas `(hl)` / `(ix+d)`).
 */
#define	LFIND	'('
#define	RTIND	')'

/*
 * Byte registers.  Field code from the unprefixed `LD A,r` block (40..43):
 * A=0, B=1, L=2, H=3.  (The S1C88 byte-addressable GP set is A,B,L,H only.)
 */
#define	A	0
#define	B	1
#define	L	2
#define	H	3

/*
 * 16-bit registers.  Pair field for inc/dec/push/pop, `ld rr,#imm`,
 * `ld rr,[hhll]` and the 16-bit ALU destination (BA=0, HL=1, IX=2, IY=3).
 * SP is encoded specially and kept as a distinct value here.
 */
#define	BA	0
#define	HL	1
#define	IX	2
#define	IY	3
#define	SP	4

/*
 * Branch condition codes (CARS/JRS/CARL/JRL): C=0, NC=1, Z=2, NZ=3.
 */
#define	CC_C	0
#define	CC_NC	1
#define	CC_Z	2
#define	CC_NZ	3

/*
 * Addressing modes returned by addr().
 * (Memory-indirect forms are added in a later version of the backend.)
 */
#define	S_IMMED	30		/* #nn / #mmnn                     */
#define	S_R8	31		/* byte register a/b/l/h           */
#define	S_R16	33		/* 16-bit register ba/hl/ix/iy/sp  */
#define	S_INDHL	34		/* (hl)                            */
#define	S_INDIX	35		/* (ix)        no displacement     */
#define	S_INDIY	36		/* (iy)        no displacement     */
#define	S_IDXIX	37		/* d(ix)       displacement in expr */
#define	S_IDXIY	38		/* d(iy)       displacement in expr */
#define	S_INDM	39		/* (hhll) / (label)   absolute     */

/*
 * Instruction classes (mne.m_type -> the machine() switch).
 * mne.m_valu carries the base opcode where noted.
 */
#define	S_LD	60		/* ld (dispatched on operand modes)        */
#define	S_ADD	61		/* m_valu = 8-bit base 0x00                */
#define	S_ADC	62		/* m_valu = 8-bit base 0x08                */
#define	S_SUB	63		/* m_valu = 8-bit base 0x10                */
#define	S_SBC	64		/* m_valu = 8-bit base 0x18                */
#define	S_AND	65		/* m_valu = 8-bit base 0x20 (8-bit only)   */
#define	S_OR	66		/* m_valu = 8-bit base 0x28 (8-bit only)   */
#define	S_CP	67		/* m_valu = 8-bit base 0x30                */
#define	S_XOR	68		/* m_valu = 8-bit base 0x38 (8-bit only)   */
#define	S_INC	69
#define	S_DEC	70
#define	S_PUSH	71
#define	S_POP	72
#define	S_EX	73
#define	S_INH	74		/* inherent (ret/rete/nop/...): m_valu = full opcode */

/*
 * Register / condition tables (defined in s1c88adr.c).
 */
struct adsym {
	char	a_str[4];	/* addressing string */
	int	a_val;		/* addressing mode value */
};

extern	struct	adsym	R8[];
extern	struct	adsym	R16[];
extern	struct	adsym	CND[];

	/* s1c88adr.c */
extern	int	addr();
extern	int	admode();
extern	int	srch();

	/* s1c88mch.c */
extern	VOID	machine();
extern	VOID	minit();
