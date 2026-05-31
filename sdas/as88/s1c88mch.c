/* s1c88mch.c — Epson S1C88 instruction encoder for sdas (sdcc88). */

/*
 *  Derived from z80mch.c of the ASxxxx assembler suite.
 *  Copyright (C) 1989-2009  Alan R. Baldwin
 *  S1C88 retarget (C) 2026, part of sdcc88.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.  See the GNU GPL for details.
 *
 *  Status: v0 — register / immediate forms (no memory-indirect operands yet).
 *  Encodings are from docs/s1c88/instruction-set.md Appendix A (the unprefixed
 *  page) and the 16-bit CF-prefixed page.
 */

#include "asxxxx.h"
#include "s1c88.h"

char	*cpu	= "Epson S1C88";
char	*dsft	= "asm";

VOID
minit()
{
	hilo = 0;		/* S1C88 is little-endian */
	exprmasks(4);		/* set up expression/address masks (≤ 4-byte space) */
}

VOID
machine(mp)
struct mne *mp;
{
	int op, rf, t1, t2, v1, v2, cfb;
	struct expr e1, e2;

	clrexpr(&e1);
	clrexpr(&e2);
	op = (int) mp->m_valu;
	rf = mp->m_type;

	t1 = t2 = v1 = v2 = 0;

	switch (rf) {

	case S_INH:				/* ret/rete/rets/nop: full opcode in op */
		outab(op);
		break;

	case S_LD:
		t1 = addr(&e1);
		comma(1);
		t2 = addr(&e2);
		v1 = (int) (e1.e_addr & 0xFF);
		v2 = (int) (e2.e_addr & 0xFF);
		if (t1 == S_R8 && t2 == S_R8) {
			outab(0x40 + (v1 << 3) + v2);		/* ld r8,r8' (40..5F)   */
		} else if (t1 == S_R8 && t2 == S_IMMED) {
			outab(0xB0 + v1);			/* ld r8,#nn  (B0..B3)  */
			outrb(&e2, 0);
		} else if (t1 == S_R16 && t2 == S_IMMED) {
			if (v1 == SP) {
				xerr('a', "ld sp,#imm not yet supported.");
			} else {
				outab(0xC4 + v1);		/* ld rr,#mmnn (C4..C7) */
				outrw(&e2, 0);
			}
		} else if (t1 == S_R16 && t2 == S_R16) {
			if (v1 <= IY && v2 <= IY) {
				outab(0xCF);			/* ld rr,rr' (CF,E0..EF) */
				outab(0xE0 + (v1 << 2) + v2);
			} else if (v2 == SP) {			/* ld rr,sp */
				outab(0xCF);
				outab(v1 == BA ? 0xF8 : v1 == HL ? 0xF4 :
				      v1 == IX ? 0xFA : 0xFE);
			} else if (v1 == SP) {			/* ld sp,rr (CF,F0..F3) */
				outab(0xCF);
				outab(0xF0 + v2);
			} else {
				xerr('a', "Invalid Addressing Mode.");
			}
		} else {
			xerr('a', "Invalid Addressing Mode.");
		}
		break;

	case S_ADD:
	case S_ADC:
	case S_SUB:
	case S_SBC:
	case S_AND:
	case S_OR:
	case S_CP:
	case S_XOR:
		t1 = addr(&e1);
		comma(1);
		t2 = addr(&e2);
		v1 = (int) (e1.e_addr & 0xFF);
		v2 = (int) (e2.e_addr & 0xFF);
		if (t1 == S_R8) {			/* 8-bit ALU: op a,src (dst = A) */
			if (v1 != A) {
				xerr('a', "8-bit ALU destination must be A.");
			} else if (t2 == S_R8) {
				if (v2 == A)
					outab(op + 0);
				else if (v2 == B)
					outab(op + 1);
				else
					xerr('a', "8-bit ALU source must be A or B.");
			} else if (t2 == S_IMMED) {
				outab(op + 2);
				outrb(&e2, 0);
			} else {
				xerr('a', "Invalid Addressing Mode.");
			}
		} else if (t1 == S_R16) {		/* 16-bit ALU (add/adc/sub/sbc/cp) */
			cfb = (rf == S_ADD) ? 0x00 : (rf == S_ADC) ? 0x04 :
			      (rf == S_SUB) ? 0x08 : (rf == S_SBC) ? 0x0C :
			      (rf == S_CP)  ? 0x18 : -1;
			if (cfb < 0) {
				xerr('a', "No 16-bit form for this operation.");
			} else if (t2 == S_R16 && v2 <= IY && (v1 == BA || v1 == HL)) {
				outab(0xCF);			/* CF,(cfb + dst*0x20 + src) */
				outab(cfb + (v1 == HL ? 0x20 : 0x00) + v2);
			} else if (t2 == S_IMMED &&
				   (rf == S_ADD || rf == S_SUB || rf == S_CP) && v1 <= IY) {
				outab((rf == S_ADD ? 0xC0 :		/* add rr,#mmnn  */
				       rf == S_SUB ? 0xD0 : 0xD4) + v1);	/* sub/cp rr,#   */
				outrw(&e2, 0);
			} else {
				xerr('a', "Invalid Addressing Mode.");
			}
		} else {
			xerr('a', "Invalid Addressing Mode.");
		}
		break;

	case S_INC:
	case S_DEC:
		t1 = addr(&e1);
		v1 = (int) (e1.e_addr & 0xFF);
		if (t1 == S_R8) {
			outab((rf == S_INC ? 0x80 : 0x88) + v1);	/* inc/dec a/b/l/h */
		} else if (t1 == S_R16) {
			if (v1 == SP)
				outab(rf == S_INC ? 0x87 : 0x8F);	/* inc/dec sp */
			else
				outab((rf == S_INC ? 0x90 : 0x98) + v1);/* inc/dec ba/hl/ix/iy */
		} else {
			xerr('a', "Invalid Addressing Mode.");
		}
		break;

	case S_PUSH:
	case S_POP:
		t1 = addr(&e1);
		v1 = (int) (e1.e_addr & 0xFF);
		if (t1 == S_R16 && v1 <= IY) {
			outab((rf == S_PUSH ? 0xA0 : 0xA8) + v1);	/* push/pop ba/hl/ix/iy */
		} else {
			xerr('a', "Invalid Addressing Mode.");
		}
		break;

	case S_EX:
		t1 = addr(&e1);
		comma(1);
		t2 = addr(&e2);
		v1 = (int) (e1.e_addr & 0xFF);
		v2 = (int) (e2.e_addr & 0xFF);
		if (t1 == S_R16 && v1 == BA && t2 == S_R16 && v2 >= HL && v2 <= SP) {
			outab(0xC7 + v2);		/* ex ba,hl/ix/iy/sp = C8..CB */
		} else if (t1 == S_R8 && v1 == A && t2 == S_R8 && v2 == B) {
			outab(0xCC);			/* ex a,b */
		} else {
			xerr('a', "Invalid Addressing Mode.");
		}
		break;

	default:
		xerr('a', "Unknown instruction class.");
		break;
	}
}
