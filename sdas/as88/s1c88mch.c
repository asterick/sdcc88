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

/*
 * PC-relative helper: returns 1 if the target is in the current area (so the
 * displacement can be computed here), else sets up the relocation base and
 * returns 0 (the linker resolves it via R_PCR).
 */
int
mchpcr(esp)
struct expr *esp;
{
	if (esp->e_base.e_ap == dot.s_area) {
		return(1);
	}
	if (esp->e_flag == 0 && esp->e_base.e_ap == NULL) {
		esp->e_flag = 1;
		esp->e_base.e_sp = &sym[1];	/* the absolute symbol '.__.ABS.' */
	}
	return(0);
}

VOID
machine(mp)
struct mne *mp;
{
	int op, rf, t1, t2, v1, v2, cfb, base;
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
		if (t1 == S_R8 && v1 == A && t2 == S_CREG) {	/* ld a,cr */
			if (v2 == CR_BR)      { outab(0xCE); outab(0xC0); }
			else if (v2 == CR_SC) { outab(0xCE); outab(0xC1); }
			else if (v2 >= CR_NB && v2 <= CR_YP) { outab(0xCE); outab(0xC8 + (v2 - CR_NB)); }
			else xerr('a', "No `ld a,<cr>' for this control register.");
		} else if (t1 == S_CREG && t2 == S_R8 && v2 == A) {	/* ld cr,a */
			if (v1 == CR_BR)      { outab(0xCE); outab(0xC2); }
			else if (v1 == CR_SC) { outab(0xCE); outab(0xC3); }
			else if (v1 >= CR_NB && v1 <= CR_YP) { outab(0xCE); outab(0xCC + (v1 - CR_NB)); }
			else xerr('a', "No `ld <cr>,a' for this control register.");
		} else if (t1 == S_CREG && t2 == S_IMMED) {		/* ld cr,#imm */
			if (v1 == CR_BR)      { outab(0xB4); outrb(&e2, 0); }	/* ld br,#hh */
			else if (v1 == CR_SC) { outab(0x9F); outrb(&e2, 0); }	/* ld sc,#nn */
			else if (v1 >= CR_NB && v1 <= CR_YP)
				{ outab(0xCE); outab(0xC4 + (v1 - CR_NB)); outrb(&e2, 0); } /* ld nb/ep/xp/yp,#pp */
			else xerr('a', "No `ld <cr>,#imm' for this control register.");
		} else if (t1 == S_R8) {			/* ld r8, <src> */
			switch (t2) {
			case S_R8:    outab(0x40 + (v1 << 3) + v2);		break;
			case S_IMMED: outab(0xB0 + v1); outrb(&e2, 0);		break;
			case S_INDHL: outab(0x40 + (v1 << 3) + 5);		break;
			case S_INDIX: outab(0x40 + (v1 << 3) + 6);		break;
			case S_INDIY: outab(0x40 + (v1 << 3) + 7);		break;
			case S_IDXIX: outab(0xCE); outab(0x40 + (v1 << 3)); outrb(&e2, R_SGND); break;
			case S_IDXIY: outab(0xCE); outab(0x41 + (v1 << 3)); outrb(&e2, R_SGND); break;
			case S_INDM:  outab(0xCE); outab(0xD0 + v1); outrw(&e2, 0); break;
			case S_BRLL:  outab(0x44 + (v1 << 3)); outrb(&e2, 0);	break;	/* ld r8,(br:ll) */
			default:      xerr('a', "Invalid Addressing Mode.");	break;
			}
		} else if (t1 == S_BRLL && t2 == S_R8) {
			outab(0x78 + v2); outrb(&e1, 0);		/* ld (br:ll),r8 — 78..7B */
		} else if (t1 == S_BRLL && t2 == S_IMMED) {
			outab(0xDD); outrb(&e1, 0); outrb(&e2, 0);	/* ld (br:ll),#nn */
		} else if ((t1 == S_INDHL || t1 == S_INDIX || t1 == S_INDIY) && t2 == S_R8) {
			outab((t1 == S_INDHL ? 0x68 : t1 == S_INDIX ? 0x60 : 0x70) + v2);
		} else if ((t1 == S_IDXIX || t1 == S_IDXIY) && t2 == S_R8) {
			outab(0xCE);
			outab(0x44 + (v2 << 3) + (t1 == S_IDXIY ? 1 : 0));
			outrb(&e1, R_SGND);
		} else if (t1 == S_INDM && t2 == S_R8) {
			outab(0xCE); outab(0xD4 + v2); outrw(&e1, 0);
		} else if (t1 == S_R16) {		/* ld rr, <src> */
			if (t2 == S_IMMED) {
				if (v1 == SP) { outab(0xCF); outab(0x6E); }	/* ld sp,#mmnn */
				else outab(0xC4 + v1);				/* ld rr,#mmnn */
				outrw(&e2, 0);
			} else if (t2 == S_INDM) {
				if (v1 == SP) { outab(0xCF); outab(0x78); }	/* ld sp,(hhll) */
				else outab(0xB8 + v1);				/* ld rr,(hhll) */
				outrw(&e2, 0);
			} else if (t2 == S_R16) {
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
			} else if (t2 == S_INDHL || t2 == S_INDIX || t2 == S_INDIY) {
				if (v1 > IY)
					xerr('a', "Only ba/hl/ix/iy load from (rr).");
				else {				/* ld rr,(hl)/(ix)/(iy) — CF,C0/D0/D8+rr */
					outab(0xCF);
					outab((t2 == S_INDHL ? 0xC0 : t2 == S_INDIX ? 0xD0 : 0xD8) + v1);
				}
			} else if (t2 == S_IDXSP) {
				if (v1 > IY)
					xerr('a', "Only ba/hl/ix/iy load from dd(sp).");
				else {				/* ld rr,dd(sp) — CF,70+rr */
					outab(0xCF); outab(0x70 + v1); outrb(&e2, R_SGND);
				}
			} else {
				xerr('a', "Invalid Addressing Mode.");
			}
		} else if ((t1 == S_INDHL || t1 == S_INDIX || t1 == S_INDIY) && t2 == S_R16) {
			if (v2 > IY)
				xerr('a', "Only ba/hl/ix/iy store to (rr).");
			else {				/* ld (hl)/(ix)/(iy),rr — CF,C4/D4/DC+rr */
				outab(0xCF);
				outab((t1 == S_INDHL ? 0xC4 : t1 == S_INDIX ? 0xD4 : 0xDC) + v2);
			}
		} else if ((t1 == S_INDHL || t1 == S_INDIX || t1 == S_INDIY) && t2 == S_IMMED) {
			outab(t1 == S_INDHL ? 0xB5 : t1 == S_INDIX ? 0xB6 : 0xB7);	/* ld (mem),#nn */
			outrb(&e2, 0);
		} else if (t1 == S_IDXSP && t2 == S_R16) {
			if (v2 > IY)
				xerr('a', "Only ba/hl/ix/iy store to dd(sp).");
			else {				/* ld dd(sp),rr — CF,74+rr */
				outab(0xCF); outab(0x74 + v2); outrb(&e1, R_SGND);
			}
		} else if (t1 == S_INDM && t2 == S_R16) {
			if (v2 == SP) { outab(0xCF); outab(0x7C); }	/* ld (hhll),sp */
			else outab(0xBC + v2);				/* ld (hhll),rr */
			outrw(&e1, 0);
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
				outab(op + 2);			/* op a,#nn */
				outrb(&e2, 0);
			} else if (t2 == S_INDHL) {
				outab(op + 3);			/* op a,(hl) */
			} else if (t2 == S_INDIX) {
				outab(op + 6);			/* op a,(ix) */
			} else if (t2 == S_INDIY) {
				outab(op + 7);			/* op a,(iy) */
			} else if (t2 == S_INDM) {
				outab(op + 5); outrw(&e2, 0);	/* op a,(hhll) */
			} else if (t2 == S_IDXIX) {
				outab(0xCE); outab(op + 0); outrb(&e2, R_SGND);	/* op a,d(ix) */
			} else if (t2 == S_IDXIY) {
				outab(0xCE); outab(op + 1); outrb(&e2, R_SGND);	/* op a,d(iy) */
			} else if (t2 == S_BRLL) {
				outab(op + 4); outrb(&e2, 0);		/* op a,(br:ll) */
			} else {
				xerr('a', "Invalid Addressing Mode.");
			}
		} else if (t1 == S_R16) {		/* 16-bit ALU (add/adc/sub/sbc/cp) */
			cfb = (rf == S_ADD) ? 0x00 : (rf == S_ADC) ? 0x04 :
			      (rf == S_SUB) ? 0x08 : (rf == S_SBC) ? 0x0C :
			      (rf == S_CP)  ? 0x18 : -1;
			if (t2 == S_R16 && v2 <= IY && (v1 == BA || v1 == HL)) {
				if (cfb < 0)			/* CF,(cfb + dst*0x20 + src) */
					xerr('a', "No 16-bit form for this operation.");
				else { outab(0xCF); outab(cfb + (v1 == HL ? 0x20 : 0x00) + v2); }
			} else if (t2 == S_R16 && (v2 == BA || v2 == HL) &&
				   (v1 == IX || v1 == IY || v1 == SP)) {
				base = -1;		/* add/sub ix/iy/sp,ba|hl (+ cp sp) — CF,40..5D */
				if (rf == S_ADD)      base = (v1 == IX) ? 0x40 : (v1 == IY) ? 0x42 : 0x44;
				else if (rf == S_SUB) base = (v1 == IX) ? 0x48 : (v1 == IY) ? 0x4A : 0x4C;
				else if (rf == S_CP && v1 == SP) base = 0x5C;
				if (base < 0)
					xerr('a', "No 16-bit form for this register/operation.");
				else { outab(0xCF); outab(base + (v2 == HL ? 1 : 0)); }
			} else if (t2 == S_IMMED) {
				if ((rf == S_ADD || rf == S_SUB || rf == S_CP) && v1 <= IY) {
					outab((rf == S_ADD ? 0xC0 :		/* add/sub/cp ba..iy,#mmnn */
					       rf == S_SUB ? 0xD0 : 0xD4) + v1);
					outrw(&e2, 0);
				} else if ((rf == S_ADD || rf == S_SUB || rf == S_CP) && v1 == SP) {
					outab(0xCF);				/* add/sub/cp sp,#mmnn */
					outab(rf == S_ADD ? 0x68 : rf == S_SUB ? 0x6A : 0x6C);
					outrw(&e2, 0);
				} else if ((rf == S_ADC || rf == S_SBC) && (v1 == BA || v1 == HL)) {
					outab(0xCF);				/* adc/sbc ba|hl,#mmnn */
					outab((rf == S_ADC ? 0x60 : 0x62) + (v1 == HL ? 1 : 0));
					outrw(&e2, 0);
				} else {
					xerr('a', "No 16-bit immediate form for this register/operation.");
				}
			} else {
				xerr('a', "Invalid Addressing Mode.");
			}
		} else if (t1 == S_BRLL) {		/* and/or/xor/cp (br:ll),#nn — D8/D9/DA/DB */
			if (t2 == S_IMMED &&
			    (rf == S_AND || rf == S_OR || rf == S_XOR || rf == S_CP)) {
				outab(rf == S_AND ? 0xD8 : rf == S_OR ? 0xD9 :
				      rf == S_XOR ? 0xDA : 0xDB);
				outrb(&e1, 0);		/* ll */
				outrb(&e2, 0);		/* nn */
			} else {
				xerr('a', "Only and/or/xor/cp (br:ll),#nn.");
			}
		} else if (t1 == S_INDHL) {		/* op [hl],src — CE,op+{a:4,#nn:5,(ix):6,(iy):7} */
			if (t2 == S_R8 && v2 == A)	{ outab(0xCE); outab(op + 4); }
			else if (t2 == S_IMMED)		{ outab(0xCE); outab(op + 5); outrb(&e2, 0); }
			else if (t2 == S_INDIX)		{ outab(0xCE); outab(op + 6); }
			else if (t2 == S_INDIY)		{ outab(0xCE); outab(op + 7); }
			else xerr('a', "Invalid [hl]-destination ALU form.");
		} else if (t1 == S_CREG) {		/* and/or/xor sc,#nn ; cp br,#hh */
			if (v1 == CR_SC && t2 == S_IMMED &&
			    (rf == S_AND || rf == S_OR || rf == S_XOR)) {
				outab(rf == S_AND ? 0x9C : rf == S_OR ? 0x9D : 0x9E);
				outrb(&e2, 0);
			} else if (v1 == CR_BR && t2 == S_IMMED && rf == S_CP) {
				outab(0xCE); outab(0xBF); outrb(&e2, 0);
			} else {
				xerr('a', "Invalid control-register ALU form.");
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
		} else if (t1 == S_CREG && v1 == CR_BR) {
			outab(rf == S_INC ? 0x84 : 0x8C);		/* inc/dec br */
		} else if (t1 == S_INDHL) {
			outab(rf == S_INC ? 0x86 : 0x8E);		/* inc/dec (hl) */
		} else if (t1 == S_BRLL) {
			outab(rf == S_INC ? 0x85 : 0x8D); outrb(&e1, 0);	/* inc/dec (br:ll) */
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
		} else if (t1 == S_R8) {
			outab(0xCF);					/* push/pop a/b/l/h */
			outab((rf == S_PUSH ? 0xB0 : 0xB4) + v1);
		} else if (t1 == S_CREG) {				/* push/pop br/ep/ip/sc */
			base = (v1 == CR_BR) ? 0 : (v1 == CR_EP) ? 1 :
			       (v1 == CR_IP) ? 2 : (v1 == CR_SC) ? 3 : -1;
			if (base < 0)
				xerr('a', "Only br/ep/ip/sc are stackable control registers.");
			else
				outab((rf == S_PUSH ? 0xA4 : 0xAC) + base);
		} else if (t1 == S_PALL) {				/* push/pop all/ale */
			outab(0xCF);
			outab((rf == S_PUSH ? 0xB8 : 0xBC) + v1);
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
		} else if (t1 == S_R8 && v1 == A && t2 == S_INDHL) {
			outab(0xCD);			/* ex a,(hl) */
		} else {
			xerr('a', "Invalid Addressing Mode.");
		}
		break;

	case S_JP:				/* jp hl / jp (kk) */
		t1 = addr(&e1);
		v1 = (int) (e1.e_addr & 0xFF);
		if (t1 == S_R16 && v1 == HL)
			outab(0xF4);		/* jp hl */
		else if (t1 == S_INDM)
			{ outab(0xFD); outrb(&e1, 0); }	/* jp (kk) — 8-bit vector */
		else
			xerr('a', "jp takes hl or a (kk) vector (use jrl for a label).");
		break;

	case S_INT:				/* int (kk) — FC,kk */
		t1 = addr(&e1);
		if (t1 == S_INDM)
			{ outab(0xFC); outrb(&e1, 0); }
		else
			xerr('a', "int takes a (kk) vector operand.");
		break;

	case S_CALL:				/* call (hhll)  — absolute indirect */
		t1 = addr(&e1);
		if (t1 == S_INDM) {
			outab(0xFB);
			outrw(&e1, 0);
		} else {
			xerr('a', "Only `call (hhll)' supported (use carl for a label target).");
		}
		break;

	case S_JRS:				/* jrs [cc,] e  — short relative, 8-bit disp */
	case S_CARS:				/* cars [cc,] e */
		cfb = (rf == S_JRS) ? 0xE4 : 0xE0;
		if ((v1 = admode(CND)) != 0) {
			outab(cfb + (v1 & 0xFF));	/* basic c/nc/z/nz: E4..E7 / E0..E3 */
			comma(1);
		} else if ((v1 = admode(CNDE)) != 0) {	/* extended signed/flag cc (CE-page) */
			outab(0xCE);
			outab((rf == S_JRS ? 0xE0 : 0xF0) + (v1 & 0xFF));
			comma(1);
		} else {
			outab(op);			/* F1 / F0 unconditional */
		}
		expr(&e2, 0);
		if (mchpcr(&e2)) {
			v2 = (int) (e2.e_addr - dot.s_addr - 1);
			if (pass == 2 && ((v2 < -128) || (v2 > 127)))
				xerr('a', "Branching Range Exceeded.");
			outab(v2);
		} else {
			outrb(&e2, R_PCR);
		}
		break;

	case S_JRL:				/* jrl [cc,] e  — long relative, 16-bit disp */
	case S_CARL:				/* carl [cc,] e */
		cfb = (rf == S_JRL) ? 0xEC : 0xE8;
		if ((v1 = admode(CND)) != 0) {
			outab(cfb + (v1 & 0xFF));	/* EC..EF / E8..EB */
			comma(1);
		} else {
			outab(op);			/* F3 / F2 unconditional */
		}
		expr(&e2, 0);
		if (mchpcr(&e2)) {
			v2 = (int) (e2.e_addr - dot.s_addr - 2);
			outab(v2 & 0xFF);
			outab((v2 >> 8) & 0xFF);
		} else {
			outrw(&e2, R_PCR);
		}
		break;

	case S_DJR:				/* djr nz, e */
		if ((v1 = admode(CND)) != 0 && (v1 & 0xFF) == CC_NZ) {
			outab(0xF5);
			comma(1);
		} else {
			xerr('a', "djr requires the NZ condition.");
		}
		expr(&e2, 0);
		if (mchpcr(&e2)) {
			v2 = (int) (e2.e_addr - dot.s_addr - 1);
			if (pass == 2 && ((v2 < -128) || (v2 > 127)))
				xerr('a', "Branching Range Exceeded.");
			outab(v2);
		} else {
			outrb(&e2, R_PCR);
		}
		break;

	case S_ROT:		/* sla/sll/sra/srl/rl/rlc/rr/rrc/cpl/neg — CE,op+{a:0,b:1,[hl]:3} */
		t1 = addr(&e1);
		v1 = (int) (e1.e_addr & 0xFF);
		if (t1 == S_R8 && v1 == A)
			{ outab(0xCE); outab(op + 0); }
		else if (t1 == S_R8 && v1 == B)
			{ outab(0xCE); outab(op + 1); }
		else if (t1 == S_BRLL)
			{ outab(0xCE); outab(op + 2); outrb(&e1, 0); }
		else if (t1 == S_INDHL)
			{ outab(0xCE); outab(op + 3); }
		else
			xerr('a', "Operand must be a, b, (br:ll) or (hl).");
		break;

	case S_SWAP:		/* swap a (F6) / swap (hl) (F7) — unprefixed */
		t1 = addr(&e1);
		v1 = (int) (e1.e_addr & 0xFF);
		if (t1 == S_R8 && v1 == A)
			outab(0xF6);
		else if (t1 == S_INDHL)
			outab(0xF7);
		else
			xerr('a', "swap operand must be a or (hl).");
		break;

	case S_INHE:		/* CE-prefixed inherent: mlt/div/sep/halt/slp */
		outab(0xCE);
		outab(op);
		break;

	case S_BIT:		/* bit a,b (94) / bit a,#nn (96) / bit b,#nn (97) / bit (hl),#nn (95) */
		t1 = addr(&e1);
		comma(1);
		t2 = addr(&e2);
		v1 = (int) (e1.e_addr & 0xFF);
		v2 = (int) (e2.e_addr & 0xFF);
		if (t1 == S_R8 && v1 == A && t2 == S_R8 && v2 == B)
			outab(0x94);				/* bit a,b */
		else if (t1 == S_R8 && v1 == A && t2 == S_IMMED)
			{ outab(0x96); outrb(&e2, 0); }		/* bit a,#nn */
		else if (t1 == S_R8 && v1 == B && t2 == S_IMMED)
			{ outab(0x97); outrb(&e2, 0); }		/* bit b,#nn */
		else if (t1 == S_INDHL && t2 == S_IMMED)
			{ outab(0x95); outrb(&e2, 0); }		/* bit (hl),#nn */
		else if (t1 == S_BRLL && t2 == S_IMMED)
			{ outab(0xDC); outrb(&e1, 0); outrb(&e2, 0); }	/* bit (br:ll),#nn */
		else
			xerr('a', "Invalid bit form.");
		break;

	case S_PCALL:		/* bcall [cc,] target — banked smart call */
	case S_PJUMP:		/* bjump [cc,] target — banked smart jump  */
		/* optional basic condition (c/nc/z/nz); the CE-page signed conditions
		   are short-only and not supported by the always-long slot. */
		if ((v1 = admode(CND)) != 0) {
			v1 &= 0xFF;
			comma(1);
		} else {
			v1 = -1;			/* unconditional */
		}
		expr(&e1, 0);				/* the (link-resolved) target */
		if (rf == S_PCALL)
			op = (v1 < 0) ? 0xF2 : (0xE8 + v1);	/* carl / carl cc */
		else
			op = (v1 < 0) ? 0xF3 : (0xEC + v1);	/* jrl  / jrl cc  */
		/* Emit the slot:  ld nb,#<bank> ; <carl|jrl> target
		   - the NB byte carries R_S1C88_BANK: the linker writes the target's bank
		     (its address >> 16) or NOPs the whole `ld nb` when the bank is 0 or
		     the current bank.
		   - the displacement carries the standard R_PCR: the 16-bit write masks
		     off the bank difference, leaving the logic-relative displacement. */
		outab(0xCE); outab(0xC4);		/* ld nb opcode  */
		/* Bank byte.  TODO: emit the R_S1C88_BANK reloc so the linker writes the
		   target's bank (its address >> 16) or NOPs the ld nb.  That needs the
		   24-bit/escape relocation path (asout.c:534), which is gated on the
		   target identity — sdas88 currently self-reports TARGET_ID_UNKNOWN and
		   takes the 16-bit path that truncates the mode.  Until a TARGET_ID_S1C88
		   identity enables it, emit bank 0 (correct for common-area / single-bank
		   targets); the displacement below already links correctly. */
		/* Bank field carrying R_S1C88_BANK — a 2-byte [bank][pad] field via
		   outrw() (which emits exactly 2 bytes and advances the counter by 2, so
		   nothing desyncs; the escape mode reaches the linker because the s1c88
		   identity routes word relocations through the escape path).  sdld88
		   writes the target's bank (its linker address >> 16; code areas at
		   (bank<<16)|logic) into [bank] and a NOP (FF) into [pad] — giving
		   `ld nb,#bank ; nop ; carl/jrl`.  When that bank is 0 (common) or the
		   current area's, it NOPs the whole `ld nb`+pad instead. */
		outrw(&e1, R_S1C88_BANK);		/* [bank][pad] */
		outab(op);				/* carl/jrl opcode */
		outrw(&e1, R_PCR);			/* disp16 (+ R_PCR reloc -> target) — logic-relative */
		break;

	default:
		xerr('a', "Unknown instruction class.");
		break;
	}
}
