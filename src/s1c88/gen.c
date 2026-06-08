/*-------------------------------------------------------------------------
  gen.c - code generator for the Epson S1C88 (sdcc88; derived from the z80 port).

  Copyright (C) 1998, Sandeep Dutta . sandeep.dutta@usa.net
  Copyright (C) 1999, Jean-Louis VERN.jlvern@writeme.com
  Copyright (C) 2000, Michael Hope <michaelh@juju.net.nz>
  Copyright (C) 2011-2024, Philipp Klaus Krause pkk@spth.de, philipp@informatik.uni-frankfurt.de, philipp@colecovision.eu)
  Copyright (C) 2021-2022, Sebastian 'basxto' Riedel <sdcc@basxto.de>

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2, or (at your option) any
  later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
-------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "s1c88.h"
#include "gen.h"
#include "dbuf_string.h"

/* This is the down and dirty file with all kinds of kludgy & hacky
   stuff. This is what it is all about CODE GENERATION for a specific MCU.
   Some of the routines may be reusable, will have to see */

enum
{
  /* Set to enable debugging trace statements in the output assembly code. */
  DISABLE_DEBUG = 0
};

#define UNIMPLEMENTED do {wassertl (regalloc_dry_run, "Unimplemented"); cost (4000, 4000.0f);} while(0)

#undef DEBUG_DRY_COST

extern struct dbuf_s *codeOutBuf;

enum
{
  INT8MIN = -128,
  INT8MAX = 127
};

/** Enum covering all the possible register pairs.
 */
typedef enum
{
  PAIR_INVALID,
  PAIR_AF,
  PAIR_HL,
  PAIR_IY,
  PAIR_IX,
  PAIR_BA,        // S1C88 B:A — the 2nd ALU pair (16-bit scratch)
  NUM_PAIRS
} PAIR_ID;

static struct
{
  const char *name;
  const char *l;
  const char *h;
  int l_idx;
  int h_idx;
} _pairs[NUM_PAIRS] =
{
  {
    "??1", "?2", "?3", -1, -1
  },
  {
    "af", "f", "a", -1, A_IDX
  },
  {
    "hl", "l", "h", L_IDX, H_IDX
  },
  {
    "iy", "iyl", "iyh", IYL_IDX, IYH_IDX
  },
  {
    "ix", "ixl", "ixh", -1, -1
  },
  {
    "ba", "a", "b", A_IDX, B_IDX
  }
};

enum
{
  LSB,
  MSB16,
  MSB24,
  MSB32
};

enum asminst
{
  A_ADD,
  A_ADC,
  A_AND,
  A_CP,
  A_CPL,
  A_DEC,
  A_EX,
  A_INC,
  A_LD,
  A_NEG,
  A_OR,
  A_RL,
  A_RLA,
  A_RLC,
  A_RLCA,
  A_RLD,
  A_RR,
  A_RRA,
  A_RRC,
  A_RRCA,
  A_RRD,
  A_SBC,
  A_SLA,
  A_SRA,
  A_SRL,
  A_SUB,
  A_XOR,
  A_SWAP
};

static const char *asminstnames[] =
{
  "add",
  "adc",
  "and",
  "cp",
  "cpl",
  "dec",
  "ex",
  "inc",
  "ld",
  "neg",
  "or",
  "rl",
  "rla",
  "rlc",
  "rlca",
  "rld",
  "rr",
  "rra",
  "rrc",
  "rrca",
  "rrd",
  "sbc",
  "sla",
  "sra",
  "srl",
  "sub",
  "xor",
  "swap"
};

/** Code generator persistent data.
 */
static struct
{
  /** Used to optimise setting up of a pair by remembering what it
      contains and adjusting instead of reloading where possible.
  */
  struct
  {
    AOP_TYPE last_type;
    const char *base;   // For addresses
    unsigned int value; // For AOP_LIT
    int offset;
  } pairs[NUM_PAIRS];
  struct
  {
//    int last;
    int pushed;
    int param_offset;
    int offset;
    int pushedHL;
    int pushedBC;
    int pushedIY;
  } stack;

  struct
  {
    int pushedBC;
  } calleeSaves;

  bool omitFramePtr;
  int frameId;
  int receiveOffset;
  bool flushStatics;
  bool in_home;
  const char *lastFunctionName;
  iCode *current_iCode;
  bool preserveCarry;

  set *sendSet;

  struct
  {
    /** TRUE if the registers have already been saved. */
    bool saved;
  } saves;

  struct
  {
    allocTrace trace;
  } lines;

  struct
  {
    allocTrace aops;
  } trace;
} _G;

bool s1c88_regs_used_as_parms_in_calls_from_current_function[IYH_IDX + 1];
bool s1c88_symmParm_in_calls_from_current_function;
bool s1c88_regs_preserved_in_calls_from_current_function[IYH_IDX + 1];

static const char *aopGet (asmop *aop, int offset, bool bit16);

static struct asmop asmop_a, asmop_b, asmop_h, asmop_l, asmop_iyh, asmop_iyl, asmop_hl, asmop_ba, asmop_iy, asmop_hlba, asmop_hla, asmop_zero, asmop_one, asmop_mone;
static struct asmop *const ASMOP_A = &asmop_a;
static struct asmop *const ASMOP_B = &asmop_b;
static struct asmop *const ASMOP_H = &asmop_h;
static struct asmop *const ASMOP_L = &asmop_l;
static struct asmop *const ASMOP_IYH = &asmop_iyh;
static struct asmop *const ASMOP_IYL = &asmop_iyl;
static struct asmop *const ASMOP_HL = &asmop_hl;
static struct asmop *const ASMOP_BA = &asmop_ba;   /* S1C88 2nd ALU pair B:A */
static struct asmop *const ASMOP_IY = &asmop_iy;
static struct asmop *const ASMOP_HLBA = &asmop_hlba;   /* S1C88 long layout HL(high):BA(low) */
static struct asmop *const ASMOP_HLA = &asmop_hla;     /* 3-byte __far pointer: offset in HL, page in A (Epson HLP) */
static struct asmop *const ASMOP_ZERO = &asmop_zero;
static struct asmop *const ASMOP_ONE = &asmop_one;
static struct asmop *const ASMOP_MONE = &asmop_mone;

/* Indexed by register ordinal (asmopregs[idx]); order matches ralloc.h. */
static asmop *asmopregs[] = { &asmop_a, &asmop_b, &asmop_l, &asmop_h,
  0, 0, 0, /* the dropped C/D/E ordinals — never allocated, no asmop */
  &asmop_iyl, &asmop_iyh };

// Init aop as a an asmop for data in registers, as given by the -1-terminated array regidx.
static void
s1c88_init_reg_asmop(asmop *aop, const signed char *regidx)
{
  aop->type = AOP_REG;
  aop->size = 0;
  memset (aop->regs, -1, sizeof(aop->regs));
  
  for(int i = 0; regidx[i] >= 0; i++)
    {
      aop->aopu.aop_reg[i] = regsS1C88 + regidx[i];
      aop->regs[regidx[i]] = i;
      aop->size++;
    }

  aop->valinfo.anything = true;
}

void
s1c88_init_asmops (void)
{
  s1c88_init_reg_asmop(&asmop_a, (const signed char[]){A_IDX, -1});
  s1c88_init_reg_asmop(&asmop_b, (const signed char[]){B_IDX, -1});
  s1c88_init_reg_asmop(&asmop_h, (const signed char[]){H_IDX, -1});
  s1c88_init_reg_asmop(&asmop_l, (const signed char[]){L_IDX, -1});
  s1c88_init_reg_asmop(&asmop_iyh, (const signed char[]){IYH_IDX, -1});
  s1c88_init_reg_asmop(&asmop_iyl, (const signed char[]){IYL_IDX, -1});
  s1c88_init_reg_asmop(&asmop_ba, (const signed char[]){A_IDX, B_IDX, -1});   // BA = A(low):B(high)
  s1c88_init_reg_asmop(&asmop_hl, (const signed char[]){L_IDX, H_IDX, -1});
  s1c88_init_reg_asmop(&asmop_iy, (const signed char[]){IYL_IDX, IYH_IDX, -1});
  s1c88_init_reg_asmop(&asmop_hlba, (const signed char[]){A_IDX, B_IDX, L_IDX, H_IDX, -1});   // long = BA(low):HL(high)
  s1c88_init_reg_asmop(&asmop_hla, (const signed char[]){L_IDX, H_IDX, A_IDX, -1});   // __far ptr = offset(HL):page(A)
  
  asmop_zero.type = AOP_LIT;
  asmop_zero.aopu.aop_lit = constVal ("0");
  asmop_zero.size = 1;
  memset (asmop_zero.regs, -1, 9);
  asmop_zero.valinfo.anything = true;

  asmop_one.type = AOP_LIT;
  asmop_one.aopu.aop_lit = constVal ("1");
  asmop_one.size = 1;
  memset (asmop_one.regs, -1, 9);
  asmop_one.valinfo.anything = true;

  asmop_mone.type = AOP_LIT;
  asmop_mone.aopu.aop_lit = constVal ("-1");
  asmop_mone.size = 1;
  memset (asmop_mone.regs, -1, 9);
  asmop_mone.valinfo.anything = true;
}

static bool regalloc_dry_run;
static unsigned int regalloc_dry_run_cost; // Legacy: cost counted in bytes only (i.e. states have been ignored for corresponding instructions).
static unsigned long regalloc_dry_run_cost_bytes;
static float regalloc_dry_run_cost_states;
static float regalloc_dry_run_state_scale = 1.0f;

static void
cost (unsigned int bytes, float states)
{
  regalloc_dry_run_cost_bytes += bytes;
  regalloc_dry_run_cost_states += states * regalloc_dry_run_state_scale;
}

static void
cost2 (unsigned int bytes, unsigned int cycles)
{
  regalloc_dry_run_cost_bytes += bytes;
  regalloc_dry_run_cost_states += cycles * regalloc_dry_run_state_scale;
}

/*-----------------------------------------------------------------*/
/* isRegIdxPair - true, if specified index is register pair,       */
/*                rIdx changed to index of lower register          */
/*-----------------------------------------------------------------*/
static bool
isRegIdxPair (short *rIdx)
{
  switch (*rIdx)
    {
    case HL_IDX:
      *rIdx = L_IDX;
      break;
    case IY_IDX:
      *rIdx = IYL_IDX;
      break;
    default:
      return false;
    }
  return true;
}

/*-----------------------------------------------------------------*/
/* aopRegOffset - return register offset in the asmop              */
/*-----------------------------------------------------------------*/
static int
aopRegOffset (const asmop *aop, short rIdx)
{
  if (rIdx < 0 || aop->type != AOP_REG)
    return -1;

  if (!isRegIdxPair (&rIdx))
    return aop->regs[rIdx];

  int offset = aop->regs[rIdx];
  return (offset != -1 && aop->regs[rIdx+1] == offset+1) ? offset : -1;
}

/*-----------------------------------------------------------------*/
/* aopUseReg - return true if register is in the asmop             */
/*-----------------------------------------------------------------*/
static inline bool
aopRegUsed (const asmop *aop, short rIdx)
{
  if (aop->type != AOP_REG)
    return false;

  if (!isRegIdxPair (&rIdx))
    return aop->regs[rIdx];

  return aop->regs[rIdx] != -1 || aop->regs[rIdx+1] != -1;
}

/*-----------------------------------------------------------------*/
/* aopRegUsedRange - true if register is in specified position range [minPos;maxPos) */
/*-----------------------------------------------------------------*/
static inline bool
aopRegUsedRange (const asmop *aop, short rIdx, int minPos, int maxPos)
{
  if (aop->type != AOP_REG)
    return false;

  if (!isRegIdxPair (&rIdx))
    return aop->regs[rIdx] >= minPos && aop->regs[rIdx] < maxPos;

  return (aop->regs[rIdx] >= minPos && aop->regs[rIdx] < maxPos) ||
         (aop->regs[rIdx+1] >= minPos && aop->regs[rIdx+1] < maxPos);
}

/*-----------------------------------------------------------------*/
/* aopRS - asmop in register or on stack.                          */
/*-----------------------------------------------------------------*/
static bool
aopRS (const asmop *aop)
{
  return (aop->type == AOP_REG || aop->type == AOP_STK || aop->type == AOP_EXSTK);
}

/*-----------------------------------------------------------------*/
/* aopIsLitVal - asmop from offset is val.                         */
/* False negatives are possible.                                   */
/*-----------------------------------------------------------------*/
static bool
aopIsLitVal (const asmop *aop, int offset, int size, unsigned long long int val)
{
  wassert (size <= sizeof (unsigned long long int)); // Make sure we are not testing outside of argument val.

  for(; size; size--, offset++)
    {
      unsigned char b = val & 0xff;
      val >>= 8;

      // Leading zeroes
      if (aop->size <= offset && !b && aop->type != AOP_LIT)
        continue;

      // Information from generalized constant propagation analysis
      if (!aop->valinfo.anything && offset < 8 &&
        ((aop->valinfo.knownbitsmask >> (offset * 8)) & 0xff) == 0xff &&
        ((aop->valinfo.knownbits >> (offset * 8)) & 0xff) == b)
        continue;

      if (aop->type != AOP_LIT)
        return (false);

      if (byteOfVal (aop->aopu.aop_lit, offset) != b)
        return (false);
    }

  return (true);
}

/*-----------------------------------------------------------------*/
/* aopIsLitBit - asmop from offset is val.                         */
/* False negatives are possible.                                   */
/*-----------------------------------------------------------------*/
static bool
aopIsLitBit (const asmop *aop, int boffset, bool val)
{
  if (!aop->valinfo.anything && boffset < 64 &&
    ((aop->valinfo.knownbitsmask >> boffset) & 1) &&
    ((aop->valinfo.knownbits >> boffset) & 1) == val)
    return(true);

  return (false);
}

/*-----------------------------------------------------------------*/
/* aopIsNotLitVal - asmop from offset is not val.                  */
/* False negatives are possible.                                   */
/* Note that both aopIsLitVal and aopIsNotLitVal can be false for  */
/* same arguments: we might just not have enough information.      */
/*-----------------------------------------------------------------*/
static bool
aopIsNotLitVal (const asmop *aop, int offset, int size, unsigned long long int val)
{
  wassert (size <= sizeof (unsigned long long int)); // Make sure we are not testing outside of argument val.

  for(; size; size--, offset++)
    {
      unsigned char b = val & 0xff;
      val >>= 8;

      // Leading zeroes
      if (aop->size <= offset && b)
        return (true);

      // Information from generalized constant propagation analysis
      if (!aop->valinfo.anything && offset < 8)
        {
          unsigned char knownbitsmask = aop->valinfo.knownbitsmask >> (offset * 8);
          unsigned char knownbits = aop->valinfo.knownbits >> (offset * 8);

          if ((knownbits & knownbitsmask) != (b & knownbitsmask))
            return (true);
          if (!offset && aop->valinfo.min > 0 && aop->valinfo.max <= 255 &&
            (aop->valinfo.min > b || aop->valinfo.min < b))
            return (true);
        }

      if (aop->type != AOP_LIT)
        continue;

      if (byteOfVal (aop->aopu.aop_lit, offset) != b)
        return (true);
    }

  return (false);
}

/*-----------------------------------------------------------------*/
/* aopInReg - asmop from offset in the register                    */
/*-----------------------------------------------------------------*/
static inline bool
aopInReg (const asmop *aop, int offset, short rIdx)
{
  if (offset >= aop->size || offset < 0)
    return (false);

  return aopRegOffset (aop, rIdx) == offset;
}

/*-----------------------------------------------------------------*/
/* aopOnStack - asmop from offset on stack in consecutive memory   */
/*-----------------------------------------------------------------*/
static bool
aopOnStack (const asmop *aop, int offset, int size)
{
  if (!(aop->type == AOP_STK || aop->type == AOP_EXSTK))
    return (false);

  if (offset + size > aop->size)
    return (false);

  return (true);
}

static inline int
fpOffset (int aop_stk)
{
  return aop_stk + (aop_stk > 0 ? _G.stack.param_offset : 0);
}

static int
spOffset (int aop_stk)
{
  return fpOffset (aop_stk) + _G.stack.pushed + _G.stack.offset;
}

static bool
isRegDead (short rIdx, const iCode * ic)
{
  if (!isRegIdxPair (&rIdx))
    return !bitVectBitValue (ic->rSurv, rIdx);
  return !bitVectBitValue (ic->rSurv, rIdx) && !bitVectBitValue (ic->rSurv, rIdx+1);
}

static PAIR_ID
_getTempPairId (void)
{
  {
      return PAIR_HL;
    }
}

static const char *
_getTempPairName (void)
{
  return _pairs[_getTempPairId ()].name;
}

static bool
isPairInUse (PAIR_ID id, const iCode * ic)
{
  if (id == PAIR_BA)
    {
      return bitVectBitValue (ic->rMask, B_IDX) || bitVectBitValue (ic->rMask, A_IDX);
    }
  else
    {
      wassertl (0, "Only implemented for DE, BC and BA");
      return TRUE;
    }
}

static bool
isPairDead (PAIR_ID id, const iCode * ic)
{
  switch (id)
    {
    case PAIR_HL:
      return isRegDead (H_IDX, ic) && isRegDead (L_IDX, ic);
    case PAIR_IY:
      return isRegDead (IYH_IDX, ic) && isRegDead (IYL_IDX, ic);
    case PAIR_BA:
      return isRegDead (B_IDX, ic) && isRegDead (A_IDX, ic);
    default:
      wassertl (0, "Only implemented for HL, IY and BA");
      return FALSE;
    }
}

static PAIR_ID
getDeadPairId (const iCode *ic)
{
  /* S1C88: BA is the only dead-able scratch pair besides HL. */
  if (isPairDead (PAIR_BA, ic))
    {
      return PAIR_BA;
    }
  else
    {
      return PAIR_INVALID;
    }
}

static PAIR_ID
getFreePairId (const iCode *ic)
{
  /* S1C88: the only general-purpose scratch pair besides HL is BA (the 2nd ALU
     pair). isPairInUse(PAIR_BA) tests A and B, so
     this won't hand out BA while A (the accumulator) or B is live. */
  if (!isPairInUse (PAIR_BA, ic))
    {
      return PAIR_BA;
    }
  else
    {
      return PAIR_INVALID;
    }
}

static void
_tidyUp (char *buf)
{
  /* Clean up the line so that it is 'prettier' */
  /* If it is a label - can't do anything */
  if (!strchr (buf, ':'))
    {
      /* Change the first (and probably only) ' ' to a tab so
         everything lines up.
       */
      while (*buf)
        {
          if (*buf == ' ')
            {
              *buf = '\t';
              break;
            }
          buf++;
        }
    }
}

static void
_vemit2 (const char *szFormat, va_list ap)
{
  struct dbuf_s dbuf;
  char *buffer, *p, *nextp;

  dbuf_init (&dbuf, INITIAL_INLINEASM);

  dbuf_tvprintf (&dbuf, szFormat, ap);

  buffer = p = dbuf_detach_c_str (&dbuf);

  _tidyUp (p);

  /* Decompose multiline macros */
  while ((nextp = strchr (p, '\n')))
    {
      *nextp = '\0';
      emit_raw (p);
      p = nextp + 1;
    }

  emit_raw (p);

  dbuf_free (buffer);
}

static void
emitDebug (const char *szFormat, ...)
{
  if (!DISABLE_DEBUG && !regalloc_dry_run && options.verboseAsm)
    {
      va_list ap;

      va_start (ap, szFormat);
      _vemit2 (szFormat, ap);
      va_end (ap);
    }
}

static void
emit2 (const char *szFormat, ...)
{
  if (!regalloc_dry_run)
    {
      va_list ap;

      va_start (ap, szFormat);
      _vemit2 (szFormat, ap);
      va_end (ap);
    }
}

static PAIR_ID
getPartPairId (const asmop *aop, int offset)
{
  if (aop->size <= offset + 1 || offset < 0)
    return PAIR_INVALID;

  if (aop->type != AOP_REG)
    return PAIR_INVALID;

  wassert (aop->aopu.aop_reg[offset] && aop->aopu.aop_reg[offset + 1]);

  if ((aop->aopu.aop_reg[offset]->rIdx == L_IDX) && (aop->aopu.aop_reg[offset + 1]->rIdx == H_IDX))
    return PAIR_HL;
  if ((aop->aopu.aop_reg[offset]->rIdx == IYL_IDX) && (aop->aopu.aop_reg[offset + 1]->rIdx == IYH_IDX))
    return PAIR_IY;

  return PAIR_INVALID;
}

static PAIR_ID
getPairId_o (const asmop *aop, int offset)
{
  if (offset >= 0 && offset + 2 <= aop->size)
    {
      if (aop->type == AOP_REG)
        {
          wassert (aop->aopu.aop_reg[offset] && aop->aopu.aop_reg[offset + 1]);

          if ((aop->aopu.aop_reg[offset]->rIdx == L_IDX) && (aop->aopu.aop_reg[offset + 1]->rIdx == H_IDX))
            {
              return PAIR_HL;
            }
          if ((aop->aopu.aop_reg[offset]->rIdx == IYL_IDX) && (aop->aopu.aop_reg[offset + 1]->rIdx == IYH_IDX))
            {
              return PAIR_IY;
            }
        }
    }
  return PAIR_INVALID;
}

static PAIR_ID
getPairId (const asmop *aop)
{
  if (aop->size != 2)
    return PAIR_INVALID;
  return (getPairId_o (aop, 0));
}

/* The S1C88's two 16-bit ALU pairs are HL and BA (only these have the full
   ADD/ADC/SUB/SBC/CP cross-product). Return which one the 2-byte chunk at
   `offset` occupies, or PAIR_INVALID. Unlike getPairId_o this recognizes BA
   (A low, B high), which the generic helpers do not. */
static PAIR_ID
aluPairId (const asmop *aop, int offset)
{
  if (aop->type != AOP_REG || offset < 0 || offset + 2 > aop->size)
    return PAIR_INVALID;
  if (aop->aopu.aop_reg[offset]->rIdx == L_IDX && aop->aopu.aop_reg[offset + 1]->rIdx == H_IDX)
    return PAIR_HL;
  if (aop->aopu.aop_reg[offset]->rIdx == A_IDX && aop->aopu.aop_reg[offset + 1]->rIdx == B_IDX)
    return PAIR_BA;
  return PAIR_INVALID;
}


/*-----------------------------------------------------------------*/
/* s1c88_emitDebuggerSymbol - associate the current code location    */
/*   with a debugger symbol                                        */
/*-----------------------------------------------------------------*/
void
s1c88_emitDebuggerSymbol (const char *debugSym)
{
  genLine.lineElement.isDebug = 1;
  emit2 ("%s !equ !here", debugSym);
  emit2 ("!global", debugSym);
  genLine.lineElement.isDebug = 0;
}

// Todo: Handle IY correctly.
static unsigned char
ld_cost (const asmop *op1, int offset1, const asmop *op2, int offset2, bool count)
{
  AOP_TYPE op1type = op1->type;
  AOP_TYPE op2type = op2->type;

  if (offset2 >= op2->size)
    return (ld_cost (op1, offset1, ASMOP_ZERO, 0, count));

  /* Costs are symmetric */
  if (op1type != AOP_REG && (op2type == AOP_REG || op2type == AOP_DUMMY))
    {
      const asmop *tmp = op1;
      op1 = op2;
      op2 = tmp;
      op1type = op1->type;
      op2type = op2->type;
    }

  switch (op1type)
    {
    case AOP_REG:
    case AOP_DUMMY:
      switch (op2type)
        {
        case AOP_REG:
          
          if (op1->aopu.aop_reg[offset1]->rIdx == IYL_IDX || op1->aopu.aop_reg[offset1]->rIdx == IYH_IDX ||
            op2->aopu.aop_reg[offset2]->rIdx == IYL_IDX || op2->aopu.aop_reg[offset2]->rIdx == IYH_IDX)
            {
              if (count)
                cost2 (2, 8);
              return (2);
            }
        case AOP_DUMMY:
          {
              if (count)
                cost2 (1, 4);
              return (1);
            }
        case AOP_IMMD:
        case AOP_LIT:
          if (op1->aopu.aop_reg[offset1]->rIdx == IYL_IDX || op1->aopu.aop_reg[offset1]->rIdx == IYH_IDX)
            {
              if (count)
                cost2 (3, 11); // ld ir, #n
              return (3);
            }
          else
            {
              if (count)
                cost2 (2, 7); // ld r, #n
              return (2);
            }
        case AOP_STK:
          if (count)
            cost2 (3, 19); // ld r, d(ix)
          return (3);
        case AOP_HL:
          if (count)
            {
              cost2 (3, 10); // ld hl, #nn
              cost2 (1, 7); // ld r, (hl)
            }
          return (4);
        case AOP_EXSTK: // Approximation. Don't really know if this is really exstk at this point, anyway.
        case AOP_IY:
          if (count)
            {
              cost2 (4, 14); // ld iy, #nn
              cost2 (3, 19); // ld r, d(iy)
            }
          return (7);
        case AOP_PAIRPTR:
          if (op2->aopu.aop_pairId == PAIR_HL)
            {
              if (count)
                cost2 (1, 7); // ld r, (hl)
              return (1);
            }
          if (op2->aopu.aop_pairId == PAIR_IY || op2->aopu.aop_pairId == PAIR_IX)
            {
              if (count)
                cost2 (3, 19); // ld r, d(ix)
              return (3);
            }
          if (0)
            {
              if (count)
                {
                  cost2 (1, 7); // ld a, (rr)
                  if (!aopInReg (op1, 0, A_IDX))
                    cost2 (1, 4); // ld r, a
                }
              return ((aopInReg (op1, 0, A_IDX) || op1type == AOP_DUMMY) ? 1 : 2);
            }
        default:
          fprintf (stderr, "ld_cost op1: AOP_REG, op2: %d\n", (int) (op2type));
          wassert (0);
        }
    case AOP_IY:               /* 4 from ld iy, #... */
    case AOP_EXSTK:            /* 4 from ld iy, #... */
      switch (op2type)
        {
        case AOP_IMMD:
        case AOP_LIT:
          return (8);
        case AOP_STK:
        case AOP_HL:           /* 3 from ld hl, #... */
          return (10);
        case AOP_IY:
        case AOP_EXSTK:
          return (16);
        default:
          printf ("ld_cost op1: AOP_IY, op2: %d\n", (int) (op2type));
          wassert (0);
        }
    case AOP_STK:
      switch (op2type)
        {
        case AOP_IMMD:
        case AOP_LIT:
          if (count)
            cost2 (4, 19); // ld d(ix), n
          return (4);
        case AOP_STK:
          if (count)
            {
              cost2 (3, 19); // ld a, d(ix)
              cost2 (3, 19); // ld d(ix), a
            }
          return (6);
        case AOP_HL:
          if (count)
            {
              cost2 (3, 10); // ld hl, nn
              cost2 (1, 7); // ld a, (hl)
              cost2 (3, 19); // ld d(ix), a
            }
          return (7);
        case AOP_EXSTK:
        case AOP_IY:
          if (count)
            {
              cost2 (4, 14); // ld iy, #nn
              cost2 (3, 19); // ld a, d(iy)
              cost2 (3, 19); // ld d(ix), a
            }
          return (10);
        case AOP_PAIRPTR:
          if (count)
            cost2 (3, 19); // ld d(ix), a
          return (3 + ld_cost (ASMOP_A, 0, op2, offset2, count));
        default:
          printf ("ld_cost op1: AOP_STK, op2: %d\n", (int) (op2type));
          wassert (0);
        }
    case AOP_HL:
      if (count)
        cost2 (3, 10); // ld hl, #nn
      switch (op2type)
        {
        case AOP_REG:
        case AOP_DUMMY:
          if (count)
            cost2 (1, 7); // ld (hl), r
          return (4);
        case AOP_IMMD:
        case AOP_LIT:
          if (count)
            cost2 (2, 10); // ld (hl), n
          return (5);
        case AOP_STK:
          if (count)
            {
              cost2 (3, 19); // ld a, d(ix)
              cost2 (1, 7); // ld (hl), a
            }
          return (7);
        case AOP_HL:
          if (count)
            {
              cost2 (3, 10); // ld hl, #nn
              cost2 (1, 7); // ld a, (hl)
              cost2 (1, 7); // ld (hl), a
            }
          return (8);
        case AOP_EXSTK:
        case AOP_IY:
          if (count)
            {
              cost2 (4, 14); // ld iy, #nn
              cost2 (3, 19); // ld a, d(iy)
              cost2 (1, 7); // ld (hl), a
            }
          return (11);
        default:
          printf ("ld_cost op1: AOP_HL, op2: %d", (int) (op2type));
          wassert (0);
        }
    case AOP_LIT:
    case AOP_IMMD:
      wassertl (0, "Trying to assign a value to a literal");
      break;
    case AOP_PAIRPTR:
      switch (op2type)
        {
        case AOP_REG:
          if (op1->aopu.aop_pairId == PAIR_HL)
            {
              cost2 (1, 7);
              return (1);
            }
          else if (op1->aopu.aop_pairId == PAIR_IY || op1->aopu.aop_pairId == PAIR_IX)
            {
               cost2 (3, 19);
               return (3);
            }
          else
            wassert (0);
          break;
        case AOP_LIT:
          if (op1->aopu.aop_pairId == PAIR_HL)
            {
              cost2 (2, 10);
              return (2);
            }
          else if (op1->aopu.aop_pairId == PAIR_IY || op1->aopu.aop_pairId == PAIR_IX)
            {
               cost2 (4, 19);
               return (4);
            }
          else
            wassert (0);
          break;
        default:
          wassert (0);
        }
      break;
    default:
      printf ("ld_cost op1: %d\n", (int) (op1type));
      wassert (0);
    }
  return (12);                   // Fallback
}

static void
op8_cost (const asmop *op, int offset)
{
  switch (op->type)
    {
    case AOP_REG:
      if (op->aopu.aop_reg[offset]->rIdx == IYL_IDX || op->aopu.aop_reg[offset]->rIdx == IYH_IDX)
        {
          wassert (HAS_IYL_INST);
          cost (2, 2);
          return;
        }
    case AOP_DUMMY:
      cost2 (1, 4);
      return;
    case AOP_IMMD:
    case AOP_LIT:
      cost2 (2, 7);
      return;
    case AOP_STK:
      {
          cost2 (3, 19);
          return;
        }
      cost (1, 8); // add hl, sp
    case AOP_HL:
      cost2 (3 + 1, 10 + 7);
      return;
    case AOP_IY:               /* 4 from ld iy, #... */
    case AOP_EXSTK:            /* 4 from ld iy, #... */
      cost2 (4 + 3, 12 + 19);
      return;
    case AOP_PAIRPTR:
      if (op->aopu.aop_pairId == PAIR_HL)
        cost2 (1, 7);
      else if (op->aopu.aop_pairId == PAIR_IY || op->aopu.aop_pairId == PAIR_IX)
        cost2 (3, 19);
      else
        wassert (0);
      return;
    default:
      printf ("op8_cost op: %d\n", (int) (op->type));
      wassert (0);
    }
}

static void
incdec_cost (const asmop *op, int offset)
{
  switch (op->type)
    {
    case AOP_REG:
      if (op->aopu.aop_reg[offset]->rIdx == IYL_IDX || op->aopu.aop_reg[offset]->rIdx == IYH_IDX)
        {
          wassert (HAS_IYL_INST);
          cost (2, 2);
          return;
        }
    case AOP_DUMMY:
      cost2 (1, 4);
      return;
    case AOP_STK:
      {
          cost2 (3, 23);
          return;
        }
      cost (1, 8); // add hl, sp
    case AOP_HL:
      cost2 (3 + 1, 10 + 11);
      return;
    case AOP_IY:               /* 4 from ld iy, #... */
    case AOP_EXSTK:            /* 4 from ld iy, #... */
      cost2 (4 + 3, 14 + 23);
      return;
    case AOP_PAIRPTR:
      if (op->aopu.aop_pairId == PAIR_HL)
        {
          cost2 (1, 11);
          return;
        }
      if (op->aopu.aop_pairId == PAIR_IY || op->aopu.aop_pairId == PAIR_IX)
        {
          cost2 (3, 23);
          return;
        }
    default:
      printf ("op8_cost op: %d\n", (int) (op->type));
      wassert (0);
    }
}

static void
bit8_cost (const asmop *op)
{
  switch (op->type)
    {
    case AOP_REG:
    case AOP_DUMMY:
      cost2 (2, 8);
      return;
    case AOP_STK:
      {
          cost2 (4, 23);
          return;
        }
      cost (1, 8); // add hl, sp
    case AOP_HL:               /* 3 from ld hl, #... */
      cost2 (3 + 2, 10 + 15);
      return;
    case AOP_IY:               /* 4 from ld iy, #... */
    case AOP_EXSTK:            /* 4 from ld iy, #... */
      cost2 (4 + 4, 14 + 23);
      return;
    default:
      printf ("bit8_cost op: %d\n", (int) (op->type));
      wassert (0);
    }
}

static void
emit3Cost (enum asminst inst, const asmop *op1, int offset1, const asmop *op2, int offset2)
{
  if (op2 && offset2 >= op2->size)
    op2 = ASMOP_ZERO;

  switch (inst)
    {
    case A_CPL:
    case A_RLA:
    case A_RLCA:
    case A_RRA:
    case A_RRCA:
      cost2 (1, 4);
      return;
    case A_NEG:
      cost2 (2, 8);
      return;
    case A_RLD:
    case A_RRD:
      cost2 (2, 18);
      return;
    case A_LD:
      ld_cost (op1, offset1, op2, offset2, true);
      return;
    case A_ADD:
    case A_ADC:
    case A_AND:
    case A_CP:
    case A_OR:
    case A_SBC:
    case A_SUB:
    case A_XOR:
      op8_cost (op2, offset2);
      return;
    case A_DEC:
    case A_INC:
      incdec_cost (op1, offset1);
      return;
    case A_RL:
    case A_RLC:
    case A_RR:
    case A_RRC:
    case A_SLA:
    case A_SRA:
    case A_SRL:
    case A_SWAP:
      bit8_cost (op1);
      return;
    default:
      wassertl (0, "Tried get cost for unknown instruction");
    }
}

static void
emit3wCost (enum asminst inst, const asmop *op1, int offset1, const asmop *op2, int offset2)
{
  if (op2 && offset2 >= op2->size)
    op2 = ASMOP_ZERO;

  switch (inst)
    {
    case A_INC:
    case A_DEC:
      if (aopInReg (op1, offset1, IY_IDX))
        cost2 (2, 10);
      else
        cost2 (1, 6);
      return;
    case A_ADD:
      if (aopInReg (op1, offset1, IY_IDX))
        cost2 (2, 15);
      else
        cost2 (1, 11);
      return;
    case A_ADC:
    case A_SBC:
      cost2 (2, 15);
      return;
    case A_EX:
      cost2 (1, 4);
      return;
    default:
      wassertl (0, "Tried get cost for unknown instruction");
    }
}

static void
emit3_o (enum asminst inst, asmop *op1, int offset1, asmop *op2, int offset2)
{
  unsigned long cost, bytecost;
  float statecost;

  emit3Cost (inst, op1, offset1, op2, offset2);

  if (regalloc_dry_run)
    return;

  cost = regalloc_dry_run_cost;
  bytecost = regalloc_dry_run_cost_bytes;
  statecost = regalloc_dry_run_cost_states;
  if (!op1)
    emit2 ("%s", asminstnames[inst]);
  else if (!op2)
    emit2 ("%s %s", asminstnames[inst], aopGet (op1, offset1, FALSE));
  else
    {
      char *l = Safe_strdup (aopGet (op1, offset1, FALSE));
      emit2 ("%s %s, %s", asminstnames[inst], l, aopGet (op2, offset2, FALSE));
      Safe_free (l);
    }

  regalloc_dry_run_cost = cost;
  regalloc_dry_run_cost_bytes = bytecost;
  regalloc_dry_run_cost_states = statecost;
  //emitDebug(";emit3_o cost: %d total so far: %d", (int)emit3Cost(inst, op1, offset1, op2, offset2), (int)cost);
}

static void
emit3w_o (enum asminst inst, asmop *op1, int offset1, asmop *op2, int offset2)
{
  unsigned int cost, bytecost;
  float statecost;

  emit3wCost (inst, op1, offset1, op2, offset2);

  if (regalloc_dry_run)
    return;

  cost = regalloc_dry_run_cost;
  bytecost = regalloc_dry_run_cost_bytes;
  statecost = regalloc_dry_run_cost_states;
  if (!op1)
    emit2 ("%s", asminstnames[inst]);
  else if (!op2)
    emit2 ("%s %s", asminstnames[inst], aopGet (op1, offset1, true));
  else
    {
      char *l = Safe_strdup (aopGet (op1, offset1, true));
      emit2 ("%s %s, %s", asminstnames[inst], l, aopGet (op2, offset2, true));
      Safe_free (l);
    }

  regalloc_dry_run_cost = cost;
  regalloc_dry_run_cost_bytes = bytecost;
  regalloc_dry_run_cost_states = statecost;
  //emitDebug(";emit3_o cost: %d total so far: %d", (int)emit3Cost(inst, op1, offset1, op2, offset2), (int)cost);
}

static void
emit3 (enum asminst inst, asmop *op1, asmop *op2)
{
  emit3_o (inst, op1, 0, op2, 0);
}

static void
emit3w (enum asminst inst, asmop *op1, asmop *op2)
{
  emit3w_o (inst, op1, 0, op2, 0);
}

static void
_emitMove (const char *to, const char *from)
{
  if (STRCASECMP (to, from) != 0)
    {
      emit2 ("ld %s, %s", to, from);
    }
  else
    {
      // Optimise it out.
      // Could leave this to the peephole, but sometimes the peephole is inhibited.
    }
}

static void
_emitMove3 (asmop *to, int to_offset, asmop *from, int from_offset)
{
  /* Todo: Longer list of moves that can be optimized out. */
  if (to_offset == from_offset)
    {
      if (to->type == AOP_REG && from->type == AOP_REG && to->aopu.aop_reg[to_offset] == from->aopu.aop_reg[from_offset])
        return;
    }

  emit3_o (A_LD, to, to_offset, from, from_offset);
}

#if 0
static const char *aopNames[] =
{
  "AOP_INVALID",
  "AOP_LIT",
  "AOP_REG",
  "AOP_DIR",
  "AOP_SFR",
  "AOP_STK",
  "AOP_IMMD",
  "AOP_CRY",
  "AOP_IY",
  "AOP_HL",
  "AOP_EXSTK",
  "AOP_PAIRPT",
  "AOP_DUMMY"
};

static void
aopDump (const char *plabel, asmop * aop)
{
  int i;
  char regbuf[9];
  char *rbp = regbuf;

  emitDebug ("; Dump of %s: type %s size %u", plabel, aopNames[aop->type], aop->size);
  switch (aop->type)
    {
    case AOP_EXSTK:
    case AOP_STK:
      emitDebug (";  aop_stk %d", aop->aopu.aop_stk);
      break;
    case AOP_REG:
      for (i = aop->size - 1; i >= 0; i--)
        *rbp++ = *(aop->aopu.aop_reg[i]->name);
      *rbp = '\0';
      emitDebug (";  reg = %s", regbuf);
      break;
    case AOP_PAIRPTR:
      emitDebug (";  pairptr = (%s)", _pairs[aop->aopu.aop_pairId].name);

    default:
      /* No information. */
      break;
    }
}
#endif

static void
_moveA (const char *moveFrom)
{
  _emitMove ("a", moveFrom);
}

/* Load aop into A */
static void
_moveA3 (asmop * from, int offset)
{
  _emitMove3 (ASMOP_A, 0, from, offset);
}

static const char *
getPairName (asmop *aop)
{
  if (aop->type == AOP_REG)
    {
      switch (aop->aopu.aop_reg[0]->rIdx)
        {
        case L_IDX:
          return "hl";
          break;
        case IYL_IDX:
          return "iy";
          break;
        }
    }
  wassertl (0, "Tried to get the pair name of something that isn't a pair");
  return NULL;
}

/** Returns TRUE if the registers used in aop form a pair (BC, DE, HL) */
static bool
isPair (const asmop *aop)
{
  return (getPairId (aop) != PAIR_INVALID);
}

/** Returns TRUE if the registers used in aop cannot be split into high
    and low halves */
static bool
isUnsplitable (const asmop * aop)
{
  switch (getPairId (aop))
    {
    case PAIR_IX:
    case PAIR_IY:
      return TRUE;
    default:
      return FALSE;
    }
  return FALSE;
}

static void
spillPair (PAIR_ID pairId)
{
  _G.pairs[pairId].last_type = AOP_INVALID;
  _G.pairs[pairId].base = NULL;
}

/* Given a register name, spill the pair (if any) the register is part of */
static void
spillPairReg (const char *regname)
{
  if (strlen (regname) == 1)
    {
      switch (*regname)
        {
        case 'h':
        case 'l':
          spillPair (PAIR_HL);
          break;
        case 'd':
        case 'e':
          break;
        case 'b':
        case 'c':
          break;
        }
    }
}

/* swap pairs fiels type/base */
static void
swapPairs (PAIR_ID pair1Id, PAIR_ID pair2Id)
{
  /*AOP_TYPE tt = _G.pairs[pair1Id].last_type;
  _G.pairs[pair1Id].last_type = _G.pairs[pair2Id].last_type;
  _G.pairs[pair2Id].last_type = tt;
  const char *tb = _G.pairs[pair1Id].base;
  unsigned int tv = _G.pairs[pair1Id].value;
  _G.pairs[pair1Id].base = _G.pairs[pair2Id].base;
  _G.pairs[pair2Id].base = tb;
  _G.pairs[pair1Id].value = _G.pairs[pair2Id].value;
  _G.pairs[pair2Id].value = tv;*/
  
  // For now just spill both.
  spillPair (pair1Id);
  spillPair (pair2Id);
}

static void
_push (PAIR_ID pairId)
{
  if (pairId == PAIR_AF)
    {
      /* The S1C88 has no AF register. Save A and the flag register (SC)
         separately — PUSH leaves all flags unchanged, so `push a` doesn't
         disturb the flags that `push sc` then saves. 2 bytes on the stack,
         matching the S1C88 frame layout. */
      emit2 ("push a");
      cost2 (2, 11);
      emit2 ("push sc");
      cost2 (1, 11);
      _G.stack.pushed += 2;
      return;
    }
  emit2 ("push %s", _pairs[pairId].name);
  if (pairId == PAIR_IX || pairId == PAIR_IY)
  	cost2 (2, 15);
  else
  	cost2 (1, 11);
  _G.stack.pushed += 2;
}

static void
_pop (PAIR_ID pairId)
{
  if (pairId == PAIR_AF)
    {
      /* Mirror of _push(PAIR_AF): restore flags then A. POP leaves flags
         unchanged except POP SC (which reloads them), so `pop a` after keeps
         the just-restored flags. */
      emit2 ("pop sc");
      cost2 (1, 10);
      emit2 ("pop a");
      cost2 (2, 10);
      _G.stack.pushed -= 2;
      spillPair (PAIR_AF);
      return;
    }
  if (pairId != PAIR_INVALID)
    {
      emit2 ("pop %s", _pairs[pairId].name);
      if (pairId == PAIR_IX || pairId == PAIR_IY)
  	    cost2 (2, 14);
      else
  	    cost2 (1, 10);
      _G.stack.pushed -= 2;
      spillPair (pairId);
    }
}

static void
genMovePairPair (PAIR_ID srcPair, PAIR_ID dstPair)
{
  /* S1C88: the 16-bit transfers are fully orthogonal — LD dst, src exists
     for every pair combination of BA/HL/IX/IY (2 bytes, 2 cycles). */
  wassertl (srcPair != PAIR_AF && dstPair != PAIR_AF, "AF is not a transfer pair on the S1C88");
  emit2 ("ld %s, %s", _pairs[dstPair].name, _pairs[srcPair].name);
  cost2 (2, 0);
  _G.pairs[dstPair].last_type = _G.pairs[srcPair].last_type;
  _G.pairs[dstPair].base = _G.pairs[srcPair].base;
  _G.pairs[dstPair].value = _G.pairs[srcPair].value;
  _G.pairs[dstPair].offset = _G.pairs[srcPair].offset;
}


/*-----------------------------------------------------------------*/
/* newAsmop - creates a new asmOp                                  */
/*-----------------------------------------------------------------*/
static asmop *
newAsmop (short type)
{
  asmop *aop;

  aop = traceAlloc (&_G.trace.aops, Safe_alloc (sizeof (asmop)));
  aop->type = type;
  memset (aop->regs, -1, 9);
  aop->valinfo.anything = true;
  return aop;
}

/*-----------------------------------------------------------------*/
/* aopForSym - for a true symbol                                   */
/*-----------------------------------------------------------------*/
static asmop *
aopForSym (const iCode * ic, symbol * sym, bool requires_a)
{
  asmop *aop;
  memmap *space;

  wassert (ic);
  wassert (sym);
  wassert (sym->etype);

  space = SPEC_OCLS (sym->etype);

  /* if already has one */
  if (sym->aop)
    return sym->aop;

  /* Assign depending on the storage class */
  if (sym->onStack || sym->iaccess)
    {
      /* The pointer that is used depends on how big the offset is.
         Normally everything is AOP_STK, but for offsets of < -128 or
         > 127 an extended stack pointer is used.
       */
      if ((_G.omitFramePtr || sym->stack < INT8MIN || sym->stack > (int) (INT8MAX - getSize (sym->type))))
        {
          emitDebug ("; AOP_EXSTK for %s, _G.omitFramePtr %d, sym->stack %d, size %d", sym->rname, (int) (_G.omitFramePtr),
                     sym->stack, getSize (sym->type));
          sym->aop = aop = newAsmop (AOP_EXSTK);
        }
      else
        {
          emitDebug ("; AOP_STK for %s", sym->rname);
          sym->aop = aop = newAsmop (AOP_STK);
        }

      memset (aop->regs, -1, sizeof(aop->regs));
      aop->size = getSize (sym->type);
      aop->aopu.aop_stk = sym->stack;
      return aop;
    }

  /* special case for a function */
  if (IS_FUNC (sym->type))
    {
      sym->aop = aop = newAsmop (AOP_IMMD);
      aop->aopu.aop_immd = traceAlloc (&_G.trace.aops, Safe_strdup (sym->rname));
      aop->size = 2;
      return aop;
    }

  /* S1C88: __sfr space falls through to the ordinary absolute-memory aops —
     the hardware registers are memory-mapped (no separate I/O space), so
     AOP_SFR is never created. */

  /* only remaining is far space */
  /* in which case DPTR gets the address */
  if (IY_RESERVED)
    {
      /* emitDebug ("; AOP_HL for %s", sym->rname); */
      sym->aop = aop = newAsmop (AOP_HL);
    }
  else
    sym->aop = aop = newAsmop (AOP_IY);

  aop->size = getSize (sym->type);
  aop->aopu.aop_dir = sym->rname;

  /* if it is in code space */
  if (IN_CODESPACE (space))
    aop->code = 1;

  return aop;
}

/*-----------------------------------------------------------------*/
/* aopForRemat - rematerializes an object                          */
/*-----------------------------------------------------------------*/
static asmop *
aopForRemat (symbol *sym)
{
  iCode *ic = sym->rematiCode;
  int val = 0;
  asmop *aop;
  struct dbuf_s dbuf;

  wassert(ic);

  for (;;)
    {
      if (ic->op == '+')
        {
          if (isOperandLiteral (IC_RIGHT (ic)))
            {
              val += (int) operandLitValue (IC_RIGHT (ic));
              ic = OP_SYMBOL (IC_LEFT (ic))->rematiCode;
            }
          else
            {
              val += (int) operandLitValue (IC_LEFT (ic));
              ic = OP_SYMBOL (IC_RIGHT (ic))->rematiCode;
            }
        }
      else if (ic->op == '-')
        {
          val -= (int) operandLitValue (IC_RIGHT (ic));
          ic = OP_SYMBOL (IC_LEFT (ic))->rematiCode;
        }
      else if (IS_CAST_ICODE (ic))
        {
          ic = OP_SYMBOL (IC_RIGHT (ic))->rematiCode;
        }
      else if (ic->op == ADDRESS_OF)
        {
          val += (int) operandLitValue (IC_RIGHT (ic));
          break;
        }
      else
        break;
    }

  if (OP_SYMBOL (IC_LEFT (ic))->onStack)
    {
      aop = newAsmop (AOP_STL);
      aop->aopu.aop_stk = (long)(OP_SYMBOL (IC_LEFT (ic))->stack) + val;
    }
  else
    {
      aop = newAsmop (AOP_IMMD);

      dbuf_init (&dbuf, 128);
      if (val)
        {
          dbuf_tprintf (&dbuf, "(%s %c %d)", OP_SYMBOL (IC_LEFT (ic))->rname, val >= 0 ? '+' : '-', abs (val) & 0xffff);
        }
      else
        {
          dbuf_append_str (&dbuf, OP_SYMBOL (IC_LEFT (ic))->rname);
        }
      aop->aopu.aop_immd = traceAlloc (&_G.trace.aops, dbuf_detach_c_str (&dbuf));
    }

  return aop;
}

#if 0 // No longer used?
/*-----------------------------------------------------------------*/
/* regsInCommon - two operands have some registers in common       */
/*-----------------------------------------------------------------*/
static bool
regsInCommon (operand * op1, operand * op2)
{
  symbol *sym1, *sym2;
  int i;

  /* if they have registers in common */
  if (!IS_SYMOP (op1) || !IS_SYMOP (op2))
    return FALSE;

  sym1 = OP_SYMBOL (op1);
  sym2 = OP_SYMBOL (op2);

  if (sym1->nRegs == 0 || sym2->nRegs == 0)
    return FALSE;

  for (i = 0; i < sym1->nRegs; i++)
    {
      int j;
      if (!sym1->regs[i])
        continue;

      for (j = 0; j < sym2->nRegs; j++)
        {
          if (!sym2->regs[j])
            continue;

          if (sym2->regs[j] == sym1->regs[i])
            return TRUE;
        }
    }

  return FALSE;
}
#endif

/*-----------------------------------------------------------------*/
/* operandsEqu - equivalent                                        */
/*-----------------------------------------------------------------*/
static bool
operandsEqu (operand * op1, operand * op2)
{
  symbol *sym1, *sym2;

  /* if they not symbols */
  if (!IS_SYMOP (op1) || !IS_SYMOP (op2))
    return FALSE;

  sym1 = OP_SYMBOL (op1);
  sym2 = OP_SYMBOL (op2);

  /* if both are itemps & one is spilt
     and the other is not then false */
  if (IS_ITEMP (op1) && IS_ITEMP (op2) && sym1->isspilt != sym2->isspilt)
    return FALSE;

  /* if they are the same */
  if (sym1 == sym2)
    return 1;

  if (sym1->rname[0] && sym2->rname[0] && strcmp (sym1->rname, sym2->rname) == 0)
    return 2;

  /* if left is a tmp & right is not */
  if (IS_ITEMP (op1) && !IS_ITEMP (op2) && sym1->isspilt && (sym1->usl.spillLoc == sym2))
    return 3;

  if (IS_ITEMP (op2) && !IS_ITEMP (op1) && sym2->isspilt && sym1->level > 0 && (sym2->usl.spillLoc == sym1))
    return 4;

  return FALSE;
}

/*-----------------------------------------------------------------*/
/* sameRegs - two asmops have the same registers                   */
/*-----------------------------------------------------------------*/
static bool
sameRegs (const asmop *aop1, const asmop *aop2)
{
  int i;

  

  if (aop1 == aop2)
    return TRUE;

  if (!regalloc_dry_run && // Todo: Check if always enabling this even for dry runs tends to result in better code.
    (aop1->type == AOP_STK && aop2->type == AOP_STK ||
    aop1->type == AOP_EXSTK && aop2->type == AOP_EXSTK))
    return (aop1->aopu.aop_stk == aop2->aopu.aop_stk);

  if (aop1->type != AOP_REG || aop2->type != AOP_REG)
    return FALSE;

  if (aop1->size != aop2->size)
    return FALSE;

  for (i = 0; i < aop1->size; i++)
    if (aop1->aopu.aop_reg[i] != aop2->aopu.aop_reg[i])
      return FALSE;

  return TRUE;
}

/*-----------------------------------------------------------------*/
/* aopSame - two asmops refer to the same storage                  */
/*-----------------------------------------------------------------*/
static bool
aopSame (const asmop *aop1, int offset1, const asmop *aop2, int offset2, int size)
{
  if (aop1 == aop2 && offset1 == offset2)
    return (true);

  for(; size; size--, offset1++, offset2++)
    {
      if (offset1 >= aop1->size || offset2 >= aop2->size)
        return (false);

      if (aop1->type == AOP_REG && aop2->type == AOP_REG && // Same register
        aop1->aopu.aop_reg[offset1]->rIdx == aop2->aopu.aop_reg[offset2]->rIdx)
        continue;

      if (aopOnStack (aop1, offset1, 1) && aopOnStack (aop2, offset2, 1) && !regalloc_dry_run && // Same stack location - stack locations might change after register allocation, so make no assumption during dry run.
        aop1->aopu.aop_stk + offset1 == aop2->aopu.aop_stk + offset2)
        continue;

      if (aop1->type == AOP_LIT && aop2->type == AOP_LIT && // Same literal
        byteOfVal (aop1->aopu.aop_lit, offset1) == byteOfVal (aop2->aopu.aop_lit, offset2))
        continue;

      // Same file-scope variable.
      if ((aop1->type == AOP_DIR || aop1->type == AOP_HL || aop1->type == AOP_IY) &&
        (aop2->type == AOP_DIR || aop2->type == AOP_HL || aop2->type == AOP_IY) &&
        offset1 == offset2 && !strcmp(aop1->aopu.aop_dir, aop2->aopu.aop_dir))
        return (true);
  
      return (false);
    }

  return (true);
}

/*-----------------------------------------------------------------*/
/* aopOp - allocates an asmop for an operand  :                    */
/*-----------------------------------------------------------------*/
static void
aopOp (operand *op, const iCode *ic, bool result, bool requires_a)
{
  asmop *aop;
  symbol *sym;
  int i;

  if (!op)
    return;

  /* if this a literal */
  if (IS_OP_LITERAL (op)) /* TODO:  && !op->isaddr, handle address literals in a sane way */
    {
      op->aop = aop = newAsmop (AOP_LIT);
      aop->aopu.aop_lit = OP_VALUE (op);
      aop->size = getSize (operandType (op));
      if (!result)
        op->aop->valinfo = getOperandValinfo (ic, op);
      else if(ic->resultvalinfo)
        op->aop->valinfo = *ic->resultvalinfo;
      return;
    }

  /* if already has a asmop then continue */
  if (op->aop)
    {
      
      return;
    }

  /* if the underlying symbol has a aop */
  if (IS_SYMOP (op) && OP_SYMBOL (op)->aop)
    {
      op->aop = OP_SYMBOL (op)->aop;
      
      if (result && ic->resultvalinfo)
        valinfo_union (&(op->aop->valinfo), *ic->resultvalinfo);
      else if (result)
        op->aop->valinfo.anything = true;
      return;
    }

  /* if this is a true symbol */
  if (IS_TRUE_SYMOP (op))
    {
      op->aop = aopForSym (ic, OP_SYMBOL (op), requires_a);
      if (!result)
        op->aop->valinfo = getOperandValinfo (ic, op);
      else if(ic->resultvalinfo)
        op->aop->valinfo = *ic->resultvalinfo;
      return;
    }

  /* this is a temporary : this has
     only four choices :
     a) register
     b) spillocation
     c) rematerialize
     d) conditional
     e) can be a return use only */

  sym = OP_SYMBOL (op);

  /* if the type is a conditional */
  if (sym->regType == REG_CND)
    {
      aop = op->aop = sym->aop = newAsmop (AOP_CRY);
      aop->size = 0;
      return;
    }

  /* if it is spilt then two situations
     a) is rematerialize
     b) has a spill location */
  if (sym->isspilt || sym->nRegs == 0)
    {
      wassert (!sym->ruonly); // iTemp optimized out via ifxForOp shouldn'T reach here.

      wassert (!sym->accuse); // Should not happen anymore with curetn register allocator.

      /* rematerialize it NOW */
      if (sym->remat)
        {
          sym->aop = op->aop = aop = aopForRemat (sym);
          aop->size = getSize (sym->type);
          if (!result)
            aop->valinfo = getOperandValinfo (ic, op);
          else if(ic->resultvalinfo)
            aop->valinfo = *ic->resultvalinfo;
          return;
        }

      /* On-stack for dry run. */
      if (sym->nRegs && regalloc_dry_run)
        {
          sym->aop = op->aop = aop = newAsmop (_G.omitFramePtr ? AOP_EXSTK : AOP_STK);
          aop->size = getSize (sym->type);
          if (!result)
            aop->valinfo = getOperandValinfo (ic, op);
          else if(ic->resultvalinfo)
            aop->valinfo = *ic->resultvalinfo;
          return;
        }

      /* On stack. */
      if (sym->isspilt && sym->usl.spillLoc)
        {
          asmop *oldAsmOp = NULL;

          if (getSize (sym->type) != getSize (sym->usl.spillLoc->type))
            {
              /* force a new aop if sizes differ */
              oldAsmOp = sym->usl.spillLoc->aop;
              sym->usl.spillLoc->aop = NULL;
            }
          sym->aop = op->aop = aop = aopForSym (ic, sym->usl.spillLoc, requires_a);
          if (getSize (sym->type) != getSize (sym->usl.spillLoc->type))
            {
              /* Don't reuse the new aop, go with the last one */
              sym->usl.spillLoc->aop = oldAsmOp;
            }
          aop->size = getSize (sym->type);
          if (!result)
            aop->valinfo = getOperandValinfo (ic, op);
          else if(ic->resultvalinfo)
            aop->valinfo = *ic->resultvalinfo;
          return;
        }

      /* else must be a dummy iTemp */
      sym->aop = op->aop = aop = newAsmop (AOP_DUMMY);
      aop->size = getSize (sym->type);
      if (!result)
        aop->valinfo = getOperandValinfo (ic, op);
      else if(ic->resultvalinfo)
        aop->valinfo = *ic->resultvalinfo;
      return;
    }

  /* must be in a register */
  sym->aop = op->aop = aop = newAsmop (AOP_REG);
  aop->size = sym->nRegs;
  if (!result)
    aop->valinfo = getOperandValinfo (ic, op);
  else if(ic->resultvalinfo)
    aop->valinfo = *ic->resultvalinfo;
  memset (aop->regs, -1, sizeof(aop->regs));
  for (i = 0; i < sym->nRegs; i++)
    {
      wassertl (sym->regs[i], "Symbol in register, but no register assigned.");
      if(!sym->regs[i])
        fprintf(stderr, "Symbol %s at ic %d.\n", sym->name, ic->key);
      aop->aopu.aop_reg[i] = sym->regs[i];
      aop->regs[sym->regs[i]->rIdx] = i;
    }
}

// Get asmop for registers containing the return type of function
// Returns 0 if the function does not have a return value or it is not returned in registers.
static asmop *
aopRet (sym_link *ftype)
{
  wassert (IS_FUNC (ftype));

  // s1c88IsReturned()/s1c88IsRegArg() derive the live return/arg regs from aopRet/aopArg
  // automatically, so no manual sync is needed here; but peephole .def rules that name
  // return registers textually may need review when changing these.

  int size = getSize (ftype->next);

  const bool bigreturn = (size > 4) || IS_STRUCT (ftype->next);
  if (bigreturn)
    return (0);

  /* S1C88: every calling convention returns in the native registers — the
     legacy sdcccall(0)/smallc/fastcall register sets (L/HL/DEHL) named
     bytes that don't exist here.  Both caller and callee read aopRet, so the
     mapping stays consistent. */
  switch (size)
    {
    case 1:
      return (ASMOP_A);
    case 2:
      return ASMOP_BA;  // S1C88: int/short returned in BA
    case 3:
      return (ASMOP_HLA);   // S1C88: __far pointer returned offset-in-HL, page-in-A (Epson HLP)
    case 4:
      return (ASMOP_HLBA);  // S1C88: long returned in HL:BA
    default:
      return 0;
    }
}

// --- S1C88 Epson parameter passing (c-compiler.md §1.2.15 / §1.3.2) ------------------------------
// Arguments are assigned to registers by descending priority *per type*, with the byte-register file
// shared across the word pairs (placing a value in BA consumes A and B; in HL consumes L and H).
// aopArg() is stateless, so we replay args 1..i, tracking which byte registers (A,B,L,H) have already
// been consumed, and return the asmop assigned to arg i — or 0 when it falls through to the stack.
//
// Phase 1 supports only the byte-addressable registers (BA,HL for int; A,L,H,B for char; HL,BA for
// near pointers; HLBA for long/float). The Epson scheme would also use IX,IY (int 3rd/4th, near-ptr
// 1st/2nd), YP,XP (char 3rd/4th) and the far-pointer pairs; those, plus any overflow, go on the stack
// for now and are added in later phases.

// Bitmask (over the A_IDX/B_IDX/L_IDX/H_IDX ordinals) of the byte registers a register asmop occupies.
static unsigned
aopArgByteMask (const asmop *aop)
{
  unsigned m = 0;
  for (int k = 0; k < aop->size; k++)
    m |= 1u << aop->aopu.aop_reg[k]->rIdx;
  return m;
}

static asmop *aopArg (sym_link *ftype, int i);

/* true iff some argument of ftype is passed in IY (the transport must then be
   protected from IY-addressing scratch use until it is consumed) */
static bool
aopArgsUseIY (sym_link *ftype)
{
  if (IFFUNC_HASVARARGS (ftype) || !FUNC_ARGS (ftype))
    return false;
  int j = 1;
  for (value *arg = FUNC_ARGS (ftype); arg; arg = arg->next, j++)
    {
      asmop *a = aopArg (ftype, j);
      if (a && a->regs[IYL_IDX] >= 0)
        return true;
    }
  return false;
}

static asmop *
aopArgRegS1C88 (sym_link *ftype, int i)
{
  /* Phase 2: IY participates. IX is PERMANENTLY skipped (documented
     divergence): the callee prologue (`push ix; ld ix, sp`) establishes the
     frame pointer before genReceive could read an IX argument. YP/XP and the
     far-pointer pairs are phase 3; IYIX (long) would need IX byte ordinals. */
  static asmop *const list_char[] = { ASMOP_A, ASMOP_L, ASMOP_H, ASMOP_B };  // Epson also: YP,XP
  static asmop *const list_int[]  = { ASMOP_BA, ASMOP_HL, ASMOP_IY };        // Epson: BA,HL,IX,IY
  static asmop *const list_nptr[] = { ASMOP_HL, ASMOP_BA, ASMOP_IY };        // Epson: IY,IX,HL,BA — IY demoted to
                                                                             // overflow (divergence: the allocator
                                                                             // cannot hold operands in IY, so IY-first
                                                                             // would tax every pointer call with a
                                                                             // transport move)
  static asmop *const list_long[] = { ASMOP_HLBA };                          // Epson also: IYIX

  unsigned consumed = 0;
  value *arg = FUNC_ARGS (ftype);

  for (int j = 1; arg; j++, arg = arg->next)
    {
      asmop *const *list = 0;
      int n = 0;

      if (!IS_STRUCT (arg->type))          // structs/unions are always passed on the stack
        switch (getSize (arg->type))
          {
          case 1: list = list_char; n = 4; break;
          case 2: if (IS_PTR (arg->type)) { list = list_nptr; n = 3; } else { list = list_int; n = 3; } break;
          case 4: list = list_long; n = 1; break;
          // size 3 (far pointer) and anything larger -> stack (handled in a later phase)
          }

      asmop *chosen = 0;
      for (int k = 0; k < n; k++)
        {
          if (IY_RESERVED && list[k] == ASMOP_IY)   /* --reserve-iy keeps IY out of the ABI */
            continue;
          unsigned m = aopArgByteMask (list[k]);
          if (!(m & consumed))
            {
              chosen = list[k];
              consumed |= m;
              break;
            }
        }

      if (j == i)
        return chosen;                     // 0 => arg i is passed on the stack
    }

  return 0;
}

// Get asmop for registers containing a parameter
// Returns 0 if the parameter is passed on the stack
static asmop *
aopArg (sym_link *ftype, int i)
{
  wassert (IS_FUNC (ftype));

  if (IFFUNC_HASVARARGS (ftype))
    return 0;

  value *args = FUNC_ARGS(ftype);
  wassert (args);

  if (FUNC_ISZ88DK_FASTCALL (ftype))
    {
      /* S1C88: the fastcall argument uses the native registers. */
      if (i != 1 || IS_STRUCT (args->type))
        return 0;

      switch (getSize (args->type))
        {
        case 1:
          return ASMOP_A;
        case 2:
          return ASMOP_BA;
        case 4:
          return ASMOP_HLBA;
        default:
          return 0;
        }
    }
    
  // Old SDCC calling convention: Pass everything on the stack.
  if (FUNC_SDCCCALL (ftype) == 0 || FUNC_ISSMALLC (ftype) || IFFUNC_ISBANKEDCALL (ftype))
    return 0;

  wassert (FUNC_SDCCCALL (ftype) == 1);

  if (!FUNC_HASVARARGS (ftype))
    {
      int j;
      value *arg;

      for (j = 1, arg = args; j < i; j++, arg = arg->next)
        wassert (arg);

      if (IS_STRUCT (arg->type))
        return 0;

      return aopArgRegS1C88 (ftype, i);   // S1C88 Epson register-priority consumption model
    }

  return 0;
}

// Return true, iff ftype cleans up stack parameters.
static bool
isFuncCalleeStackCleanup (sym_link *ftype)
{
  wassert (IS_FUNC (ftype));

  const bool farg = !FUNC_HASVARARGS (ftype) && FUNC_ARGS (ftype) && IS_FLOAT (FUNC_ARGS (ftype)->type); 
  const bool bigreturn = (getSize (ftype->next) > 4) || IS_STRUCT (ftype->next);
  int stackparmbytes = bigreturn * 2;
  for (value *arg = FUNC_ARGS(ftype); arg && !FUNC_HASVARARGS(ftype); arg = arg->next)
    {
      int argsize = getSize (arg->type);
      if (argsize == 1 && FUNC_ISSMALLC (ftype)) // SmallC calling convention passes 8-bit stack arguments as 16 bit.
        argsize++;
      if (!SPEC_REGPARM (arg->etype))
        stackparmbytes += argsize;
    }
  if (!stackparmbytes)
    return false;

  if (IFFUNC_ISZ88DK_CALLEE (ftype))
    return true;

  /* S1C88 maximum mode: the return frame is 3 bytes (PCL PCH CB) and only
     RET can consume it (a jp can't restore CB), so the callee-cleanup
     epilogue tricks (pop hl ... jp hl) don't exist here. The CALLER cleans
     up stack parameters by default; only an explicit __z88dk_callee opts a
     function into the (bulkier) 3-byte frame-hop epilogue. */
  (void) farg;
  return false;
}

/*-----------------------------------------------------------------*/
/* freeAsmop - free up the asmop given to an operand               */
/*----------------------------------------------------------------*/
static void
freeAsmop (operand * op, asmop *aaop)
{
  asmop *aop;

  if (!op)
    aop = aaop;
  else
    aop = op->aop;

  if (!aop)
    return;

  if (aop->freed)
    goto dealloc;

  aop->freed = 1;

  if (getPairId (aop) == PAIR_HL)
    {
      spillPair (PAIR_HL);
    }

dealloc:
  /* all other cases just dealloc */
  if (op)
    {
      op->aop = NULL;
      if (IS_SYMOP (op))
        {
          OP_SYMBOL (op)->aop = NULL;
          /* if the symbol has a spill */
          if (SPIL_LOC (op))
            SPIL_LOC (op)->aop = NULL;
        }
    }

}

static bool
isLitWord (const asmop *aop)
{
  /*    if (aop->size != 2)
     return FALSE; */
  switch (aop->type)
    {
    case AOP_IMMD:
    case AOP_LIT:
      return TRUE;
    default:
      return FALSE;
    }
}

static const char *
aopGetLitWordLong (const asmop *aop, int offset, bool with_hash)
{
  static struct dbuf_s dbuf = { 0 };

  if (dbuf_is_initialized (&dbuf))
    {
      dbuf_set_length (&dbuf, 0);
    }
  else
    {
      dbuf_init (&dbuf, 128);
    }

  /* depending on type */
  switch (aop->type)
    {
    case AOP_HL:
    case AOP_IY:
    case AOP_IMMD:
      /* PENDING: for re-target */
      if (with_hash)
        {
          dbuf_tprintf (&dbuf, "!hashedstr + %d", aop->aopu.aop_immd, offset);
        }
      else if (offset == 0)
        {
          dbuf_tprintf (&dbuf, "%s", aop->aopu.aop_immd);
        }
      else
        {
          dbuf_tprintf (&dbuf, "%s + %d", aop->aopu.aop_immd, offset);
        }
      break;

    case AOP_LIT:
    {
      value *val = aop->aopu.aop_lit;
      /* if it is a float then it gets tricky */
      /* otherwise it is fairly simple */
      if (!IS_FLOAT (val->type))
        {
          unsigned long long v = ullFromVal (val);

          v >>= (offset * 8);

          dbuf_tprintf (&dbuf, with_hash ? "!immedword" : "!constword", (unsigned) (v & 0xffffu));
        }
      else
        {
          union
          {
            float f;
            unsigned char c[4];
          }
          fl;
          unsigned int i;

          /* it is type float */
          fl.f = (float) floatFromVal (val);

#ifdef WORDS_BIGENDIAN
          i = fl.c[3 - offset] | (fl.c[3 - offset - 1] << 8);
#else
          i = fl.c[offset] | (fl.c[offset + 1] << 8);
#endif
          dbuf_tprintf (&dbuf, with_hash ? "!immedword" : "!constword", i);
        }
    }
    break;

    case AOP_REG:
    case AOP_STK:
    case AOP_DIR:
    case AOP_STL:
    case AOP_CRY:
    case AOP_EXSTK:
    case AOP_PAIRPTR:
    case AOP_DUMMY:
      break;

    default:
      dbuf_destroy (&dbuf);
      fprintf (stderr, "aop->type: %d\n", aop->type);
      wassertl (0, "aopGetLitWordLong got unsupported aop->type");
      exit (0);
    }
  return dbuf_c_str (&dbuf);
}

static bool
isPtr (const char *s)
{
  if (!strcmp (s, "hl"))
    return TRUE;
  if (!strcmp (s, "ix"))
    return TRUE;
  if (!strcmp (s, "iy"))
    return TRUE;
  return FALSE;
}

static void
adjustPair (const char *pair, int *pold, int new_val)
{
  wassert (pair);

  while (*pold < new_val)
    {
      emit2 ("inc %s", pair);
      (*pold)++;
    }
  while (*pold > new_val)
    {
      emit2 ("dec %s", pair);
      (*pold)--;
    }
}

static void
spillCached (void)
{
  spillPair (PAIR_HL);
  spillPair (PAIR_IY);
}

static bool
requiresHL (const asmop *aop)
{
  switch (aop->type)
    {
    case AOP_IY:
      return FALSE;
    case AOP_HL:
    case AOP_EXSTK:
    case AOP_STL:
      return true;
    case AOP_STK:
      return (_G.omitFramePtr);
    case AOP_REG:
    {
      int i;
      for (i = 0; i < aop->size; i++)
        {
          wassert (aop->aopu.aop_reg[i]);
          if (aop->aopu.aop_reg[i]->rIdx == L_IDX || aop->aopu.aop_reg[i]->rIdx == H_IDX)
            return TRUE;
        }
    }
    case AOP_PAIRPTR:
      return (aop->aopu.aop_pairId == PAIR_HL);
    default:
      return FALSE;
    }
}

// Updated the internally cached value for a pair.
static void
updatePair (PAIR_ID pairId, int diff)
{
  if (_G.pairs[pairId].last_type == AOP_LIT)
    _G.pairs[pairId].value = (_G.pairs[pairId].value + (unsigned int)diff) & 0xffff;
  else if (_G.pairs[pairId].last_type == AOP_IMMD || _G.pairs[pairId].last_type == AOP_IY || _G.pairs[pairId].last_type == AOP_HL ||
    _G.pairs[pairId].last_type == AOP_STK || _G.pairs[pairId].last_type == AOP_EXSTK)
    _G.pairs[pairId].offset += diff;
}

// Return 0, if adjusting the old value in the pair is sufficient.
static int
fetchLitPair (PAIR_ID pairId, asmop *left, int offset, bool f_dead, bool dry)
{
  const char *pair = _pairs[pairId].name;
  char *l = Safe_strdup (aopGetLitWordLong (left, offset, FALSE));
  char *base_str = Safe_strdup (aopGetLitWordLong (left, 0, FALSE));

//  emitDebug (";fetchLitPair %s",  pair);

  wassert (pair);

  const char *base = base_str;

  // Make offset from aopGetLitWordLong explicit.
  if (strchr (base_str, '+') && base_str[0] == '(' && base_str[1] == '_' && strchr (base_str, ' '))
    {
      long xoffset = strtol(strchr (base_str, '+') + 1, 0, 0);
      if (abs(offset < 10000) && labs(xoffset) < 10000l)
        {
          *(strchr (base_str, ' ')) = 0;
          base++;
          offset += xoffset;
        }
    }

  if (isPtr (pair))
    {
      if (pairId == PAIR_HL || pairId == PAIR_IY)
        {
          if (pairId == PAIR_HL && base[0] == '0')      // Ugly workaround
            {
              unsigned int tmpoffset;
              const char *tmpbase;
              if (sscanf (base, "%xd", &tmpoffset) && (tmpbase = strchr (base, '+')))
                {
                  offset = tmpoffset;
                  base = tmpbase++;
                }
            }
          if ((_G.pairs[pairId].last_type == AOP_IMMD && left->type == AOP_IMMD) ||
              (_G.pairs[pairId].last_type == AOP_IY && left->type == AOP_IY) ||
              (_G.pairs[pairId].last_type == AOP_HL && left->type == AOP_HL))
            {
              if (!regalloc_dry_run && _G.pairs[pairId].base && !strcmp (_G.pairs[pairId].base, base))  // Todo: Exact cost.
                {
                  if (pairId == PAIR_HL && abs (_G.pairs[pairId].offset - offset) < 3)
                    {
                      if (dry) // Just report matching base
                        return (0);
                      adjustPair (pair, &_G.pairs[pairId].offset, offset);
                      goto adjusted;
                    }
                  if (pairId == PAIR_IY && offset == _G.pairs[pairId].offset)
                    {
                      if (dry) // Just report matching base
                        return (0);
                      goto adjusted;
                    }
                }
            }
        }

      if(dry)
        return(-1);
 
      if (pairId == PAIR_HL && left->type == AOP_LIT && _G.pairs[pairId].last_type == AOP_LIT)
        {
          unsigned new_low, new_high, old_low, old_high;
          new_low = byteOfVal (left->aopu.aop_lit, offset);
          new_high = byteOfVal (left->aopu.aop_lit, offset + 1);
          old_low = (_G.pairs[pairId].value >> 0) & 0xff;
          old_high = (_G.pairs[pairId].value >> 8) & 0xff;

          if (new_low == old_low && new_high == old_high)
            goto adjusted;
          else if (new_high == old_high && new_low == old_high)
            {
              emit3_o (A_LD, ASMOP_L, 0, ASMOP_H, 0);
              goto adjusted;
            }
          else if (new_low == old_low && new_high == old_low)
            {
              emit3_o (A_LD, ASMOP_H, 0, ASMOP_L, 0);
              goto adjusted;
            }
          /* Change lower byte only. */
          else if (new_high == old_high)
            {
              emit3_o (A_LD, ASMOP_L, 0, left, offset);
              goto adjusted;
            }
          /* Change upper byte only. */
          else if (new_low == old_low)
            {
              emit3_o (A_LD, ASMOP_H, 0, left, offset + 1);
              goto adjusted;
            }
        }
    }

  if(dry)
    return(-1);

  /* Both a lit on the right and a true symbol on the left */
  {
    emit2 ("ld %s, !hashedstr", pair, l);
    if (pairId == PAIR_IX || pairId == PAIR_IY)
      cost2 (4, 14);
    else
      cost2 (3, 10);
  }

adjusted:
  _G.pairs[pairId].last_type = left->type;
  if (left->type == AOP_LIT)
    _G.pairs[pairId].value = byteOfVal (left->aopu.aop_lit, offset) + (byteOfVal (left->aopu.aop_lit, offset + 1) << 8);
  else
    _G.pairs[pairId].base = traceAlloc (&_G.trace.aops, Safe_strdup (base));
  _G.pairs[pairId].offset = offset;
  Safe_free (base_str);
  Safe_free (l);

  return(0);
}

static PAIR_ID
makeFreePairId (const iCode * ic, bool * pisUsed)
{
  *pisUsed = FALSE;

  /* S1C88: BA is the only scratch pair besides HL. */
  if (ic != NULL && !bitVectBitValue (ic->rMask, A_IDX) && !bitVectBitValue (ic->rMask, B_IDX))
    return PAIR_BA;

  *pisUsed = TRUE;
  return PAIR_HL;
}

static void
genMove_o (asmop *result, int roffset, asmop *source, int soffset, int size, bool a_dead_global, bool hl_dead_global, bool iy_dead_global, bool f_dead);

static void
emit3_8alu (enum asminst inst, asmop *src, int soffset, const iCode *ic);

static void
emit3_shift (enum asminst inst, asmop *aop, int offset, const iCode *ic);

static void
emit3_incdec (enum asminst inst, asmop *aop, int offset, const iCode *ic);

/* If ic != 0, we can safely use isPairDead(). */
/* By now, genMove / genMove_o is as good or better than this for nearly all uses. */
static void
fetchPairLong (PAIR_ID pairId, asmop *aop, const iCode *ic, int offset)
{
  emitDebug (";fetchPairLong");

  if (aop->type == AOP_STL && !offset)
    {
      if (pairId == PAIR_IY)
        {
          emit2 ("ld iy, !immed%d", spOffset(aop->aopu.aop_stk));
          emit2 ("add iy, sp");
          cost2 (6, 29);
          return;
        }

      /* S1C88: a non-HL ALU-pair request computes in HL inside an `ex ba, hl`
         pivot (EX always goes through BA).
         The ld/add between the swaps touch neither A nor B. */
      wassert (pairId == PAIR_HL || pairId == PAIR_BA);
      if (pairId == PAIR_BA)
        emit3w (A_EX, ASMOP_BA, ASMOP_HL);
      emit2 ("ld hl, !immed%d", spOffset(aop->aopu.aop_stk));
      cost2 (3, 10);
      emit2 ("add hl, sp");
      cost2 (1, 11);
      if (pairId == PAIR_BA)
        emit3w (A_EX, ASMOP_BA, ASMOP_HL);
      spillPair (pairId);
      return;
    }
  else if (aop->type == AOP_STL && offset >= 2)
    {
      fetchLitPair (pairId, ASMOP_ZERO, 0, true, false);
      return;
    }
  else if (aop->type == AOP_STL)
    {
      UNIMPLEMENTED;
      return;
    }

  /* if this is rematerializable */
  if (isLitWord (aop))
    fetchLitPair (pairId, aop, offset, true, false);
  else
    {
      if (getPairId_o (aop, offset) == pairId)
        {
          /* Do nothing */
        }
      else if ((aop->type == AOP_STK || aop->type == AOP_EXSTK) && aop->size - offset >= 2 &&
               (pairId == PAIR_BA || pairId == PAIR_HL || pairId == PAIR_IX || pairId == PAIR_IY) &&
               (!regalloc_dry_run || aop->aopu.aop_stk > 0) &&
               (aop->aopu.aop_stk + offset + _G.stack.offset + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0) +
                _G.stack.pushed) >= -128 &&
               (aop->aopu.aop_stk + offset + _G.stack.offset + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0) +
                _G.stack.pushed) <= 127)
        {
          int sp_offset = aop->aopu.aop_stk + offset + _G.stack.offset +
                          (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0) + _G.stack.pushed;
          emit2 ("ld %s, %d (sp)", _pairs[pairId].name, sp_offset);
          cost (3, 6);
        }
      /* Getting the parameter by a pop / push sequence is cheaper when we have a free pair.
         Stack allocation can change after register allocation, so assume this optimization is not possible for the allocator's cost function (unless the stack location is for a parameter). */
      else if (aop->size - offset >= 2 && (aop->type == AOP_STK || aop->type == AOP_EXSTK) && (!regalloc_dry_run || aop->aopu.aop_stk > 0) && (aop->aopu.aop_stk + offset + _G.stack.offset + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0) + _G.stack.pushed) == 2 && ic && getFreePairId (ic) != PAIR_INVALID && getFreePairId (ic) != pairId)
        {
          PAIR_ID extrapair = getFreePairId (ic);
          _pop (extrapair);
          _pop (pairId);
          _push (pairId);
          _push (extrapair);
        }
      /* Todo: Use even cheaper ex hl, (sp) and ex iy, (sp) when possible. */
      else if (aop->size - offset >= 2 && (aop->type == AOP_STK || aop->type == AOP_EXSTK) && (!regalloc_dry_run || aop->aopu.aop_stk > 0) && (aop->aopu.aop_stk + offset + _G.stack.offset + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0) + _G.stack.pushed) == 0)
        {
          _pop (pairId);
          _push (pairId);
        }
      else if ((aop->type == AOP_IY || aop->type == AOP_HL) &&
        (aop->size >= 2 || pairId != PAIR_IY && optimize.allow_unsafe_read))
        {
          /* Instead of fetching relative to IY, just grab directly
             from the address IY refers to */
          emit2 ("ld %s, !mems", _pairs[pairId].name, aopGetLitWordLong (aop, offset, FALSE));
          if (pairId == PAIR_HL)
            cost2 (3, 16);
          else
            cost2 (4, 20);

          if (aop->size < 2)
            {
              emit2 ("ld %s, !zero", _pairs[pairId].h);
              cost2 (2, 7);
            }
        }
      /* we need to get it byte by byte */
      else if (pairId == PAIR_HL && (IY_RESERVED) && (aop->type == AOP_HL || aop->type == AOP_EXSTK) && requiresHL (aop))
        {
          if (!regalloc_dry_run)        // TODO: Fix this to get correct cost!
            aopGet (aop, offset, FALSE);
          switch (aop->size - offset)
            {
            case 1:
              emit2 ("ld l, !*hl");
              cost2 (1, 7);
              emit2 ("ld h, !immedbyte", 0u);
              cost2 (2, 7);
              break;
            default:
              wassertl (aop->size - offset > 1, "Attempted to fetch no data into HL");
              {
                  if (ic && bitVectBitValue (ic->rMask, A_IDX))
                    _push (PAIR_AF);

                  emit2 ("!ldahli");
                  {
                      cost2 (1, 7); // ld , a(hl)
                      cost2 (1, 6); // inc hl
                    }
                  emit2 ("ld h, !*hl");
                  cost2 (1, 7);
                  emit3 (A_LD, ASMOP_L, ASMOP_A);

                  if (ic && bitVectBitValue (ic->rMask, A_IDX))
                    _pop (PAIR_AF);
                }
              break;
            }
        }
      else if (pairId == PAIR_IY)
        {
          int fp_offset = aop->aopu.aop_stk + offset + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);
          int sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;
          if (isPair (aop))
            {
              _push (getPairId (aop));
              _pop (PAIR_IY);
            }
          else
            {
              bool isUsed;
              PAIR_ID id = makeFreePairId (ic, &isUsed);
              if (isUsed)
                _push (id);
              /* Can't load into parts, so load into HL then exchange. */
              genMove_o (id == PAIR_HL ? ASMOP_HL : ASMOP_BA, 0, aop, offset, 2, false, false, false, false);

              {
                  _push (id);
                  _pop (PAIR_IY);
                }
              if (isUsed)
                _pop (id);
            }
        }
      else if (isUnsplitable (aop))
        {
          _push (getPairId (aop));
          _pop (pairId);
        }
      else
        {
          int fp_offset = aop->aopu.aop_stk + offset + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);
          int sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;
          if (!regalloc_dry_run && !strcmp (aopGet (aop, offset + 1, FALSE), _pairs[pairId].l))    // aopGet (aop, offset + 1, FALSE) is problematic: It prevents calculation of exact cost, and results in redundant code being generated. Todo: Exact cost
            {
              _moveA3 (aop, offset);
              if (!regalloc_dry_run)
                emit2 ("ld %s, %s", _pairs[pairId].h, aopGet (aop, offset + 1, FALSE));
              ld_cost (pairId == PAIR_HL ? ASMOP_H : ASMOP_B, 0, aop, offset + 1, true);
              emit2 ("ld %s, a", _pairs[pairId].l);
              ld_cost (ASMOP_L, 0, ASMOP_A, 0, true);
            }
          else {
              if (pairId == PAIR_HL && (aopInReg (aop, offset, IYL_IDX) || aopInReg (aop, offset, IYH_IDX)))
                UNIMPLEMENTED;
              if (!aopInReg (aop, offset, _pairs[pairId].l_idx))
                {
                  if (!HAS_IYL_INST && (aopInReg (aop, offset, IYL_IDX) || aopInReg (aop, offset, IYH_IDX)))
                    UNIMPLEMENTED;
                  if (!regalloc_dry_run)
                    emit2 ("ld %s, %s", _pairs[pairId].l, aopGet (aop, offset, false));
                  ld_cost (pairId == PAIR_HL ? ASMOP_L : ASMOP_A, 0, aop, offset, true);
                }
              if (pairId == PAIR_HL && (aopInReg (aop, offset + 1, IYL_IDX) || aopInReg (aop, offset + 1, IYH_IDX)))
                UNIMPLEMENTED;
              if (!aopInReg (aop, offset + 1, _pairs[pairId].h_idx))
                {
                  if (!HAS_IYL_INST && (aopInReg (aop, offset + 1, IYL_IDX) || aopInReg (aop, offset + 1, IYH_IDX)))
                    UNIMPLEMENTED;
                  if (!regalloc_dry_run)
                    emit2 ("ld %s, %s", _pairs[pairId].h, aopGet (aop, offset + 1, false));
                  ld_cost (pairId == PAIR_HL ? ASMOP_H : ASMOP_B, 0, aop, offset + 1, true);
                }
            }
        }
      /* PENDING: check? */
      spillPair (pairId);
    }
}

static void
fetchPair (PAIR_ID pairId, asmop *aop)
{
  fetchPairLong (pairId, aop, NULL, 0);
}

static void
setupPairFromSP (PAIR_ID id, int offset)
{
  wassertl (id == PAIR_HL || id == PAIR_IY, "Setup relative to SP only implemented for HL, IY");

  if (_G.preserveCarry)
    {
      _push (PAIR_AF);
      cost2 (1, 11);
      offset += 2;
    }

  wassert (id == PAIR_HL || id == PAIR_IY);

  if (offset < INT8MIN || offset > INT8MAX || id == PAIR_IY)
    {
      struct dbuf_s dbuf;
      PAIR_ID lid = id;
      dbuf_init (&dbuf, sizeof(int) * 3 + 1);
      dbuf_printf (&dbuf, "%d", offset);
      emit2 ("ld %s, !hashedstr", _pairs[lid].name, dbuf_c_str (&dbuf));
      if (lid == PAIR_IY)
        cost2 (4, 14);
      else
        cost2 (3, 10);
      dbuf_destroy (&dbuf);
      emit2 ("add %s, sp", _pairs[lid].name);
      if (lid == PAIR_IY)
        cost2 (2, 15);
      else
        cost2 (1, 11);
    }
  else
    {
      emit2 ("!ldahlsp", offset);
      {
          cost2 (3, 10);
          cost2 (1, 11);
        }
    }


  if (_G.preserveCarry)
    {
      _pop (PAIR_AF);
      cost2 (1, 10);
      offset -= 2;
    }
    
  spillPair (id);
}

static void
shiftIntoPair (PAIR_ID id, asmop *aop);

/*-----------------------------------------------------------------*/
/* pointPairToAop() make a register pair point to a byte of an aop */
/*-----------------------------------------------------------------*/
static void pointPairToAop (PAIR_ID pairId, const asmop *aop, int offset)
{
  switch (aop->type)
    {
    case AOP_EXSTK:

    case AOP_STK:
      ; int abso = aop->aopu.aop_stk + offset + _G.stack.offset + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);

      if ((_G.pairs[pairId].last_type == AOP_STK || _G.pairs[pairId].last_type == AOP_EXSTK) && abs (_G.pairs[pairId].offset - abso) < (_G.preserveCarry ? 5 : 3))
        adjustPair (_pairs[pairId].name, &_G.pairs[pairId].offset, abso);
      else
        setupPairFromSP (pairId, abso + _G.stack.pushed);

      _G.pairs[pairId].offset = abso;

      break;

    // Legacy.
    case AOP_HL:
    case AOP_IY:
      fetchLitPair (pairId, (asmop *) aop, offset, true, false);
      _G.pairs[pairId].offset = offset;
      break;

    case AOP_PAIRPTR:
      wassert (!offset);

      shiftIntoPair (pairId, (asmop *) aop); // Legacy. Todo: eliminate uses of shiftIntoPair() ?

      break;

    default:
      wassertl (0, "Unsupported aop type for pointPairToAop()");
    }

  _G.pairs[pairId].last_type = aop->type;
}

// Weird function. Sometimes offset is used, sometimes not.
// Callers rely on that behaviour. Uses of this should be replaced
// by pointPairToAop() above after the 3.7.0 release.
static void
setupPair (PAIR_ID pairId, asmop *aop, int offset)
{
  switch (aop->type)
    {
    case AOP_IY:
      wassertl (pairId == PAIR_IY || pairId == PAIR_HL, "AOP_IY must be in IY or HL");
      fetchLitPair (pairId, aop, 0, true, false);
      break;

    case AOP_HL:
      wassertl (pairId == PAIR_HL, "AOP_HL must be in HL");

      fetchLitPair (pairId, aop, offset, true, false);
      _G.pairs[pairId].offset = offset;
      break;

    case AOP_EXSTK:
      wassertl (pairId == PAIR_IY || pairId == PAIR_HL, "The extended stack must be in IY or HL");

      {
        int offset = aop->aopu.aop_stk + _G.stack.offset;

        if (aop->aopu.aop_stk >= 0)
          offset += _G.stack.param_offset;

        if (_G.pairs[pairId].last_type == aop->type && abs(_G.pairs[pairId].offset - offset) <= 3)
          adjustPair (_pairs[pairId].name, &_G.pairs[pairId].offset, offset);
        else
          {
            struct dbuf_s dbuf;

            /* PENDING: Do this better. */
            if (_G.preserveCarry)
              _push (PAIR_AF);
            dbuf_init (&dbuf, 128);
            dbuf_printf (&dbuf, "%d", offset + _G.stack.pushed);
            emit2 ("ld %s, !hashedstr", _pairs[pairId].name, dbuf_c_str (&dbuf));
            dbuf_destroy (&dbuf);
            emit2 ("add %s, sp", _pairs[pairId].name);
            _G.pairs[pairId].last_type = aop->type;
            _G.pairs[pairId].offset = offset;
            if (_G.preserveCarry)
              _pop (PAIR_AF);
          }
      }
      break;

    case AOP_STK:
    {
      /* Doesnt include _G.stack.pushed */
      int abso = aop->aopu.aop_stk + offset + _G.stack.offset + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);

      assert (pairId == PAIR_HL);
      /* In some cases we can still inc or dec hl */
      if (_G.pairs[pairId].last_type == AOP_STK && abs (_G.pairs[pairId].offset - abso) < 3)
        {
          adjustPair (_pairs[pairId].name, &_G.pairs[pairId].offset, abso);
        }
      else
        {
          setupPairFromSP (PAIR_HL, abso + _G.stack.pushed);
        }
      _G.pairs[pairId].offset = abso;
      break;
    }

    case AOP_PAIRPTR:
      if (pairId != aop->aopu.aop_pairId)
        genMovePairPair (aop->aopu.aop_pairId, pairId);
      adjustPair (_pairs[pairId].name, &_G.pairs[pairId].offset, offset);
      break;

    default:
      wassert (0);
    }
  _G.pairs[pairId].last_type = aop->type;
}

static void
emitLabelSpill (symbol *tlbl)
{
  emitLabel (tlbl);
  spillCached ();
}

/*-----------------------------------------------------------------*/
/* aopGet - for fetching value of the aop                          */
/*-----------------------------------------------------------------*/
static const char *
aopGet (asmop *aop, int offset, bool bit16)
{
  static struct dbuf_s dbuf = { 0 };

  wassert_bt (!regalloc_dry_run);

  if (dbuf_is_initialized (&dbuf))
    {
      /* reuse the dynamically allocated buffer */
      dbuf_set_length (&dbuf, 0);
    }
  else
    {
      /* first time: initialize the dynamically allocated buffer */
      dbuf_init (&dbuf, 128);
    }

  /* offset is greater than size then zero */
  /* PENDING: this seems a bit screwed in some pointer cases. */
  if (offset > (aop->size - 1) && aop->type != AOP_LIT)
    {
      dbuf_tprintf (&dbuf, "!zero");
    }
  else
    {
      /* depending on type */
      switch (aop->type)
        {
        case AOP_DUMMY:
          dbuf_tprintf (&dbuf, bit16 ? "hl" : "a");
          break;

        case AOP_IMMD:
          /* PENDING: re-target */
          if (bit16)
            dbuf_tprintf (&dbuf, "!immedword", aop->aopu.aop_immd);
          else
            {
              switch (offset)
                {
                case 2:
                  /* S1C88 __far: byte 2 of a symbolic 24-bit address is the
                     real page, #((sym) >> 16) — never a bank tag or zero */
                  dbuf_tprintf (&dbuf, "!bankimmeds", aop->aopu.aop_immd);
                  break;

                case 1:
                  dbuf_tprintf (&dbuf, "!msbimmeds", aop->aopu.aop_immd);
                  break;

                case 0:
                  dbuf_tprintf (&dbuf, "!lsbimmeds", aop->aopu.aop_immd);
                  break;

                default:
                  dbuf_tprintf (&dbuf, "!zero");
                }
            }
          break;

        case AOP_DIR:
          wassert (0); // AOP_DIR: direct space, unused on the S1C88
          emit2 ("ld a, (%s+%d)", aop->aopu.aop_dir, offset);
          cost2 (3, 13);
          dbuf_append_char (&dbuf, 'a');
          break;

        case AOP_REG:
          if (bit16)
            {
              if (aopInReg (aop, offset, IY_IDX))
                dbuf_append_str (&dbuf, "iy");
              else
                {
                  dbuf_append_str (&dbuf, aop->aopu.aop_reg[offset + 1]->name);
                  dbuf_append_str (&dbuf, aop->aopu.aop_reg[offset]->name);
                }
            }
          else
            dbuf_append_str (&dbuf, aop->aopu.aop_reg[offset]->name);
          break;

        case AOP_HL:
          setupPair (PAIR_HL, aop, offset);
          dbuf_tprintf (&dbuf, "!*hl");
          break;

        case AOP_IY:
          setupPair (PAIR_IY, aop, offset);
          dbuf_tprintf (&dbuf, "!*iyx", offset);
          break;

        case AOP_EXSTK:
          if (!IY_RESERVED)
            {
              setupPair (PAIR_IY, aop, offset);
              dbuf_tprintf (&dbuf, "!*iyx", offset);
              break;
            }

        case AOP_STK:
          

          if (aop->type == AOP_EXSTK)
            {
              pointPairToAop (PAIR_HL, aop, offset);
              dbuf_tprintf (&dbuf, "!*hl");
            }
          else if (_G.omitFramePtr)
            {
              if (aop->aopu.aop_stk >= 0)
                offset += _G.stack.param_offset;
              setupPair (PAIR_IX, aop, offset);
              dbuf_tprintf (&dbuf, "!*ixx", offset);
            }
          else
            {
              if (aop->aopu.aop_stk >= 0)
                offset += _G.stack.param_offset;
              dbuf_tprintf (&dbuf, "!*ixx", aop->aopu.aop_stk + offset);
            }
          break;

        case AOP_CRY:
          wassertl (0, "Tried to fetch from a bit variable");
          break;

        case AOP_LIT:
          dbuf_append_str (&dbuf, aopLiteralLong (aop->aopu.aop_lit, offset, 1 + bit16));
          break;

        case AOP_PAIRPTR:
          /* S1C88: IX/IY use the displacement form relative to the fixed base
             set by shiftIntoPair — the pair is never moved (the move-
             protocol plus an offset displacement double-applied the offset).
             HL has no displacement form, so it keeps the move-protocol. */
          if (aop->aopu.aop_pairId == PAIR_IX)
            dbuf_tprintf (&dbuf, "!*ixx", offset);
          else if (aop->aopu.aop_pairId == PAIR_IY)
            dbuf_tprintf (&dbuf, "!*iyx", offset);
          else
            {
              setupPair (aop->aopu.aop_pairId, aop, offset);
              dbuf_tprintf (&dbuf, "!mems", _pairs[aop->aopu.aop_pairId].name);
            }
          break;

        default:
          dbuf_destroy (&dbuf);
          fprintf (stderr, "aop->type: %d\n", aop->type);
          wassertl (0, "aopGet got unsupported aop->type");
          exit (0);
        }
    }
  return dbuf_c_str (&dbuf);
}

static bool
isRegString (const char *s)
{
  if (!strcmp (s, "b") || !strcmp (s, "c") || !strcmp (s, "d") || !strcmp (s, "e") ||
      !strcmp (s, "a") || !strcmp (s, "h") || !strcmp (s, "l"))
    return TRUE;
  return FALSE;
}

static bool
isConstantString (const char *s)
{
  /* This is a bit of a hack... */
  return (*s == '#' || *s == '$');
}

#define AOP_NEEDSACC(x) ((x)->aop && (((x)->aop->type == AOP_CRY) || ((x)->aop->type == AOP_SFR)))
#define AOP_IS_PAIRPTR(x, p) ((x)->aop->type == AOP_PAIRPTR && (x)->aop->aopu.aop_pairId == (p))

static bool
canAssignToPtr (const char *s)
{
  if (isRegString (s))
    return TRUE;
  if (isConstantString (s))
    return TRUE;
  return FALSE;
}

static bool
canAssignToPtr3 (const asmop *aop)
{
  if (aop->type == AOP_REG)
    return (TRUE);
  if (aop->type == AOP_IMMD || aop->type == AOP_LIT)
    return (TRUE);
  return (FALSE);
}

/*-----------------------------------------------------------------*/
/* aopPut - puts a string for a aop                                */
/*-----------------------------------------------------------------*/
static void
aopPut (asmop *aop, const char *s, int offset)
{
  struct dbuf_s dbuf;

  wassert (!regalloc_dry_run);

  if (aop->size && offset > (aop->size - 1))
    {
      werror_bt (E_INTERNAL_ERROR, __FILE__, __LINE__, "aopPut got offset > aop->size");
      exit (0);
    }

  // PENDING
  dbuf_init (&dbuf, 128);
  dbuf_tprintf (&dbuf, s);
  s = dbuf_c_str (&dbuf);

  /* will assign value to value */
  /* depending on where it is of course */
  switch (aop->type)
    {
    case AOP_DUMMY:
      _moveA (s);               /* in case s is volatile */
      break;

    case AOP_DIR:
      /* Direct.  Hmmm. */
      wassert (0); // AOP_DIR: direct space, unused on the S1C88
      if (strcmp (s, "a"))
        emit2 ("ld a, %s", s);
      emit2 ("ld (%s+%d),a", aop->aopu.aop_dir, offset);
      break;

    case AOP_REG:
      if (!strcmp (aop->aopu.aop_reg[offset]->name, s))
        ;
      else if (!strcmp (s, "!*hl"))
        emit2 ("ld %s,!*hl", aop->aopu.aop_reg[offset]->name);
      else
        emit2 ("ld %s, %s", aop->aopu.aop_reg[offset]->name, s);
      spillPairReg (aop->aopu.aop_reg[offset]->name);
      break;

    case AOP_IY:
      if (!canAssignToPtr (s) || isConstantString (s))   /* S1C88: ld (iy+d),#imm illegal — route immediate through A */
        {
          emit2 ("ld a, %s", s);
          setupPair (PAIR_IY, aop, offset);
          emit2 ("ld !*iyx, a", offset);
        }
      else
        {
          setupPair (PAIR_IY, aop, offset);
          emit2 ("ld !*iyx, %s", offset, s);
        }
      break;

    case AOP_HL:
      /* PENDING: for re-target */
      if (!strcmp (s, "!*hl") || !strcmp (s, "(hl)") || !strcmp (s, "[hl]"))
        {
          emit2 ("ld a, !*hl");
          s = "a";
        }
      else if (strstr (s, "(ix)") || strstr (s, "(iy)"))
        {
          emit2 ("ld a, %s", s);
          s = "a";
        }
      setupPair (PAIR_HL, aop, offset);

      emit2 ("ld !*hl, %s", s);
      break;

    case AOP_EXSTK:
      if(!IY_RESERVED)
        {
          if (!canAssignToPtr (s) || isConstantString (s))   /* S1C88: ld (iy+d),#imm illegal — route immediate through A */
            {
              emit2 ("ld a, %s", s);
              setupPair (PAIR_IY, aop, offset);
              emit2 ("ld !*iyx, a", offset);
            }
          else
            {
              setupPair (PAIR_IY, aop, offset);
              emit2 ("ld !*iyx, %s", offset, s);
            }
          break;
       }

    case AOP_STK:
      if (aop->type == AOP_EXSTK)
        {
          /* PENDING: re-target */
          if (!strcmp (s, "!*hl") || !strcmp (s, "(hl)") || !strcmp (s, "[hl]"))
            {
              emit2 ("ld a, !*hl");
              s = "a";
            }
          pointPairToAop (PAIR_HL, aop, offset);
          if (!canAssignToPtr (s))
            {
              emit2 ("ld a, %s", s);
              emit2 ("ld !*hl, a");
            }
          else
            emit2 ("ld !*hl, %s", s);
        }
      else
        {
          if (aop->aopu.aop_stk >= 0)
            offset += _G.stack.param_offset;
          /* S1C88: `ld d(ix),<reg>` is legal but `ld d(ix),#imm` is not (only
             `ld (hl),#nn` takes an immediate destination) — so route immediates
             (and any non-register source) through A. */
          if (!canAssignToPtr (s) || isConstantString (s))
            {
              emit2 ("ld a, %s", s);
              emit2 ("ld !*ixx, a", aop->aopu.aop_stk + offset);
            }
          else
            {
              emit2 ("ld !*ixx, %s", aop->aopu.aop_stk + offset, s);
            }
        }
      break;

    case AOP_CRY:
      /* if bit variable */
      if (!aop->aopu.aop_dir)
        {
          emit2 ("ld a, !zero");
          emit2 ("rl a");
        }
      else
        {
          /* In bit space but not in C - cant happen */
          wassertl (0, "Tried to write into a bit variable");
        }
      break;

    case AOP_PAIRPTR:
      /* S1C88: displacement form off the fixed base for IX/IY (see aopGet);
         move-protocol only for HL. */
      if (aop->aopu.aop_pairId == PAIR_IX)
        emit2 ("ld !*ixx, %s", offset, s);
      else if (aop->aopu.aop_pairId == PAIR_IY)
        emit2 ("ld !*iyx, %s", offset, s);
      else
        {
          setupPair (aop->aopu.aop_pairId, aop, offset);
          emit2 ("ld !mems, %s", _pairs[aop->aopu.aop_pairId].name, s);
        }
      break;

    default:
      dbuf_destroy (&dbuf); fprintf (stderr, "AOP_DIR: %d\n",AOP_DIR);
      fprintf (stderr, "aop->type: %d\n", aop->type);
      werror (E_INTERNAL_ERROR, __FILE__, __LINE__, "aopPut got unsupported aop->type");
      exit (0);
    }
  dbuf_destroy (&dbuf);
}

// pop a register pair while not destroying one of the two registers in it (destroying tempreg instead, if available).
static void
poppairwithsavedreg (PAIR_ID pair, short survivingreg, short tempreg)
{
  if (tempreg >= 0)
    {
      emit2 ("ld %s, %s", regsS1C88[tempreg].name, regsS1C88[survivingreg].name);
      ld_cost (ASMOP_L, 0, ASMOP_H, 0, true);
      _pop (pair);
      emit2 ("ld %s, %s", regsS1C88[survivingreg].name, regsS1C88[tempreg].name);
      ld_cost (ASMOP_L, 0, ASMOP_H, 0, true);
      return;
    }

  // No tempreg, need to do it the hard way via stack access.
  bool isupperbyte = (survivingreg == B_IDX || survivingreg == H_IDX || survivingreg == IYH_IDX);
  _push (PAIR_AF); // Save flags
  _push (PAIR_HL); // Save hl
  emit2 ("ld hl, !immedword", 4 + isupperbyte);
  cost2 (3, 10);
  emit2 ("add hl, sp");
  cost2 (2, 15);
  emit2 ("ld (hl), %s", regsS1C88[survivingreg].name);
  cost2 (1, 7);
  _pop (PAIR_HL);
  _pop (PAIR_AF);
  _pop (pair);
}

// Move, but try not to. Preserves flags. Cannot use xor to zero, since xor resets the carry flag.
static void
cheapMove (asmop *to, int to_offset, asmop *from, int from_offset, bool a_dead)
{
#if 0
  emitDebug ("; cheapMove");
#endif

  if (aopInReg (to, to_offset, A_IDX))
    a_dead = true;

  if (from->type == AOP_STL)
    {
      if (from_offset > 2)
        {
          cheapMove (to, to_offset, ASMOP_ZERO, 0, a_dead);
          return;
        }

      // Need free a do be able to partially restore hl below.
      bool pushed_a = false;
      if ((aopInReg (to, to_offset, L_IDX) || aopInReg (to, to_offset, H_IDX)) && !a_dead)
        {
          _push (PAIR_AF);
          pushed_a = true;
        }

      _push (PAIR_HL);
      if (!pushed_a) // Preserve f
        _push (PAIR_AF);
      emit2 ("ld hl, !immed%d", spOffset(from->aopu.aop_stk));
      cost2 (3, 10);
      emit2 ("add hl, sp");
      cost2 (1, 11);
      if (!pushed_a)
        _pop (PAIR_AF);
      spillPair (PAIR_HL);
      cheapMove (to, to_offset, ASMOP_HL, from_offset, a_dead);
      if (aopInReg (to, to_offset, L_IDX))
        poppairwithsavedreg (PAIR_HL, H_IDX, A_IDX);
      else if (aopInReg (to, to_offset, H_IDX))
        poppairwithsavedreg (PAIR_HL, L_IDX, A_IDX);
      else
        _pop (PAIR_HL);

      if (pushed_a)
        _pop (PAIR_AF);

      return;
    }

  const bool from_index = aopInReg (from, from_offset, IYL_IDX) || aopInReg (from, from_offset, IYH_IDX);
  const bool to_index = aopInReg (to, to_offset, IYL_IDX) || aopInReg (to, to_offset, IYH_IDX);
  const bool index = to_index || from_index;

  if (to->type == AOP_REG && from->type == AOP_REG)
    {
      if (to->aopu.aop_reg[to_offset] == from->aopu.aop_reg[from_offset])
        return;

      if (!index ||
        HAS_IYL_INST && !aopInReg (to, to_offset, L_IDX) && !aopInReg (to, to_offset, H_IDX) && !aopInReg (from, from_offset, L_IDX) && !aopInReg (from, from_offset, H_IDX))
        {
          if (!regalloc_dry_run)
            aopPut (to, aopGet (from, from_offset, false), to_offset);
          ld_cost (to, to_offset, from, from_offset, true);
          spillPairReg (to->aopu.aop_reg[to_offset]->name);
          return;
        }
#if 0 // Might destroy carry. Would also mess up interrupts on TLCS-90.
      if (aopInReg (from, from_offset, IYH_IDX) && !to_index && a_dead)
        {
          _push(PAIR_IY);
          _pop (PAIR_AF);
          cheapMove (to, to_offset, ASMOP_A, 0, true);
          return;
        }
#endif
    }

  if (from->type != AOP_REG && from->type != AOP_LIT && aopIsLitVal (from, from_offset, 1, 0x00))
    {
      cheapMove (to, to_offset, ASMOP_ZERO, 0, a_dead);
      return;
    }
  else if (HAS_IYL_INST && to_index && (from->type == AOP_LIT || from->type == AOP_IMMD))
    {
      if (!regalloc_dry_run)
        aopPut (to, aopGet (from, from_offset, false), to_offset);
      ld_cost (to, 0, from_offset < from->size ? from : ASMOP_ZERO, from_offset, true);
      return;
    }
  else if (to_index && HAS_IYL_INST)
    {
      if (!a_dead)
        _push (PAIR_AF);
      cheapMove (ASMOP_A, 0, from, from_offset, true);
      if (!regalloc_dry_run)
        aopPut (to, "a", to_offset);
      ld_cost (to, to_offset, ASMOP_A, 0, true);
      spillPairReg (to->aopu.aop_reg[to_offset]->name);
      if (!a_dead)
        _pop (PAIR_AF);
      return;
    }

  if (to->type == AOP_REG && from_index && !to_index && - _G.stack.pushed - _G.stack.offset >= -128 && !_G.omitFramePtr)
    {
      _push(PAIR_IY);
      if (!regalloc_dry_run)
        emit2 ("ld %s, %d (ix)", aopGet (to, to_offset, false), - _G.stack.pushed - _G.stack.offset + aopInReg (from, from_offset, IYH_IDX));
      cost2 (3, 19);
      spillPairReg (to->aopu.aop_reg[to_offset]->name);
      _pop(PAIR_IY);
      return;
    }
   else if (to_index && !from_index && from->type == AOP_REG && - _G.stack.pushed - _G.stack.offset >= -128 && !_G.omitFramePtr)
    {
      _push(PAIR_IY);
      if (!regalloc_dry_run)
        emit2 ("ld %d (ix), %s", - _G.stack.pushed - _G.stack.offset + aopInReg (to, to_offset, IYH_IDX), aopGet (from, from_offset, false));
      cost2 (3, 19);
      _pop(PAIR_IY);
      return;
    }

  /* S1C88 access to an IY half (IX/IY are not byte-addressable).  Two legal
     idioms replace the index-register stack machinery:
       - the BA pivot `ex ba, iy; <ld between A/B and L/H>; ex ba, iy` — fully
         self-restoring (BA, the other IY half, everything) but only usable
         when the partner byte is L or H (A/B are overwritten between swaps);
       - HL staging `push hl; ld hl, iy; ...; [ld iy, hl;] pop hl` for any
         other partner (memory operands keep IY free for their addressing). */
  if (from_index && !to_index)          /* read an IY half */
    {
      if (aopInReg (to, to_offset, L_IDX) || aopInReg (to, to_offset, H_IDX))
        {
          emit2 ("ex ba, iy");
          cost2 (1, 0);
          emit3 (A_LD, aopInReg (to, to_offset, L_IDX) ? ASMOP_L : ASMOP_H,
                 aopInReg (from, from_offset, IYL_IDX) ? ASMOP_A : ASMOP_B);
          emit2 ("ex ba, iy");
          cost2 (1, 0);
        }
      else
        {
          _push (PAIR_HL);
          emit2 ("ld hl, iy");
          cost2 (2, 0);
          cheapMove (to, to_offset, aopInReg (from, from_offset, IYL_IDX) ? ASMOP_L : ASMOP_H, 0, a_dead);
          emit2 ("ld iy, hl");   /* a memory `to` may have re-pointed IY for its addressing */
          cost2 (2, 0);
          _pop (PAIR_HL);
        }
      return;
    }
  else if (to_index && !from_index)     /* write an IY half */
    {
      if (aopInReg (from, from_offset, L_IDX) || aopInReg (from, from_offset, H_IDX))
        {
          emit2 ("ex ba, iy");
          cost2 (1, 0);
          emit3 (A_LD, aopInReg (to, to_offset, IYL_IDX) ? ASMOP_A : ASMOP_B,
                 aopInReg (from, from_offset, L_IDX) ? ASMOP_L : ASMOP_H);
          emit2 ("ex ba, iy");
          cost2 (1, 0);
        }
      else
        {
          _push (PAIR_HL);
          emit2 ("ld hl, iy");
          cost2 (2, 0);
          cheapMove (aopInReg (to, to_offset, IYL_IDX) ? ASMOP_L : ASMOP_H, 0, from, from_offset, a_dead);
          emit2 ("ld iy, hl");
          cost2 (2, 0);
          _pop (PAIR_HL);
        }
      return;
    }
  else if (to_index && from_index)      /* IY half to IY half */
    {
      emit2 ("ex ba, iy");
      cost2 (1, 0);
      emit3 (A_LD, aopInReg (to, to_offset, IYL_IDX) ? ASMOP_A : ASMOP_B,
             aopInReg (from, from_offset, IYL_IDX) ? ASMOP_A : ASMOP_B);
      emit2 ("ex ba, iy");
      cost2 (1, 0);
      return;
    }

  if (from->type == AOP_IY && aopInReg (to, to_offset, A_IDX) && from_offset < from->size)
    {
      emit2 ("ld a, (%s+%d)", from->aopu.aop_dir, from_offset);
      cost2 (3, 13);
    }
  else if (aopInReg (from, from_offset, A_IDX) && to->type == AOP_IY)
    {
      wassert (to_offset < to->size);
      emit2 ("ld (%s+%d), a", to->aopu.aop_dir, to_offset);
      cost2 (3, 13);
    }
  else if (!aopInReg (to, to_offset, A_IDX) && !aopInReg (from, from_offset, A_IDX) && (from->type == AOP_DIR || (to->type == AOP_HL || to->type == AOP_IY || to->type == AOP_EXSTK || to->type == AOP_STK) && (from->type == AOP_HL || from->type == AOP_IY || from->type == AOP_EXSTK || from->type == AOP_STK) || (to->type == AOP_HL || to->type == AOP_EXSTK) && (aopInReg(from, from_offset, L_IDX) || aopInReg(from, from_offset, H_IDX))) || to->type == AOP_PAIRPTR && from->type == AOP_PAIRPTR)
    {
      if (!a_dead)
        _push (PAIR_AF);

      cheapMove (ASMOP_A, 0, from, from_offset, true);
      cheapMove (to, to_offset, ASMOP_A, 0, true);

      if (!a_dead)
        _pop (PAIR_AF);
    }
  else
    {
      if (!regalloc_dry_run)
        aopPut (to, aopGet (from, from_offset, false), to_offset);
      if (to->type == AOP_REG)
        spillPairReg (to->aopu.aop_reg[to_offset]->name);

      ld_cost (to, 0, from_offset < from->size ? from : ASMOP_ZERO, from_offset, true);
    }
}

static void
commitPair (asmop *aop, PAIR_ID id, const iCode *ic, bool dont_destroy) // Obsolete. Replace uses by genMove or genMove_o.
{
  int fp_offset = aop->aopu.aop_stk + (aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);
  int sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;

  if (getPairId (aop) == id)
    return;

  /* Stack positions will change, so do not assume this is possible in the cost function. */
  if (!regalloc_dry_run && (aop->type == AOP_STK || aop->type == AOP_EXSTK) && !sp_offset
      && ((id == PAIR_HL) || id == PAIR_IY) && !dont_destroy)
    {
      /* S1C88: direct SP-relative pair store,%s side effect
         of loading the old stack word was unused — the pair is spilled). */
      emit2 ("ld 0 (sp), %s", _pairs[id].name);
      cost2 (3, 0);
      spillPair (id);
    }
  else if (!regalloc_dry_run && (aop->type == AOP_STK || aop->type == AOP_EXSTK) && !sp_offset)
    {
      emit2 ("inc sp");
      cost2 (1, 6);
      emit2 ("inc sp");
      cost2 (1, 6);
      emit2 ("push %s", _pairs[id].name);
      if (id == PAIR_IY)
        cost2 (2, 15);
      else
        cost2 (1, 11);
    }

  else if (id == PAIR_HL && requiresHL (aop) && (IY_RESERVED && aop->type != AOP_HL && aop->type != AOP_IY))
    {
      /* S1C88: stage through A and B; save a live B. */
      bool save_b = ic && !isRegDead (B_IDX, ic);
      if (save_b)
        {
          emit2 ("push b");
          cost2 (1, 11);
          _G.stack.pushed += 1;
        }
      emit3 (A_LD, ASMOP_A, ASMOP_L);
      emit3 (A_LD, ASMOP_B, ASMOP_H);
      if (!regalloc_dry_run)
        {
          aopPut (aop, "a", 0);
          aopPut (aop, "b", 1);
        }
      ld_cost (aop, 0, ASMOP_A, 0, true);
      ld_cost (aop, 0, ASMOP_B, 0, true);
      if (save_b)
        {
          emit2 ("pop b");
          cost2 (1, 10);
          _G.stack.pushed -= 1;
        }
    }
  else
    {
      /* Special cases */
      if ((aop->type == AOP_IY || aop->type == AOP_HL) && aop->size == 2)
        {
          if (!regalloc_dry_run)
            emit2 ("ld !mems, %s", aopGetLitWordLong (aop, 0, FALSE), _pairs[id].name);
          if (id == PAIR_HL)
            cost2 (3, 16);
          else
            cost2 (4, 20);
        }
      else
        {
          switch (id)
            {
            case PAIR_BA:
              if (aop->type == AOP_REG && aop->aopu.aop_reg[0]->rIdx == B_IDX && aop->aopu.aop_reg[1]->rIdx == A_IDX)
                {
                  emit2 ("ex a, b");   /* result wants the halves swapped */
                  cost2 (1, 0);
                }
              else if (aop->type == AOP_REG && aop->aopu.aop_reg[0]->rIdx == B_IDX)
                {                      /* low half lands in B: write high first */
                  cheapMove (aop, 1, ASMOP_B, 0, true);
                  cheapMove (aop, 0, ASMOP_A, 0, true);
                }
              else
                {
                  cheapMove (aop, 0, ASMOP_A, 0, true);
                  cheapMove (aop, 1, ASMOP_B, 0, true);
                }
              break;
            case PAIR_HL:
              if (aop->type == AOP_REG && aop->aopu.aop_reg[0]->rIdx == H_IDX && aop->aopu.aop_reg[1]->rIdx == L_IDX)
                {
                  cheapMove (ASMOP_A, 0, ASMOP_L, 0, true);
                  cheapMove (aop, 1, ASMOP_H, 0, true);
                  cheapMove (aop, 0, ASMOP_A, 0, true);
                }
              else if (aop->type == AOP_REG && aop->aopu.aop_reg[0]->rIdx == H_IDX)     // Do not overwrite upper byte.
                {
                  cheapMove (aop, 1, ASMOP_H, 0, true);
                  cheapMove (aop, 0, ASMOP_L, 0, true);
                }
              else
                {
                  cheapMove (aop, 0, ASMOP_L, 0, true);
                  cheapMove (aop, 1, ASMOP_H, 0, true);
                }
              break;
            case PAIR_IY:
              cheapMove (aop, 0, ASMOP_IYL, 0, true);
              cheapMove (aop, 1, ASMOP_IYH, 0, true);
              break;
            default:
              wassertl (0, "Unknown pair id in commitPair()");
              fprintf (stderr, "pair %s\n", _pairs[id].name);
            }
        }
    }
}

/*-----------------------------------------------------------------*/
/* genCopyStack - Copy the value - stack to stack only             */
/*-----------------------------------------------------------------*/
static void
genCopyStack (asmop *result, int roffset, asmop *source, int soffset, int n, bool *assigned, int *size, bool a_free, bool hl_free, bool really_do_it_now)
{
  // Avoid overwriting source. Do not assume stack locations during dry run - they can change later.
  int dir = (!regalloc_dry_run && result->aopu.aop_stk + roffset > source->aopu.aop_stk + soffset && result->aopu.aop_stk + roffset < source->aopu.aop_stk + soffset + n) ? -1 : 1;

  for (int j = 0; j < n;)
    {
      int i = (dir >= 0) ? j : (n - j - 1);

      if (assigned[i])
        {
          j++;
          continue;
        }

      if (!aopOnStack (result, roffset + i, 1) || !aopOnStack (source, soffset + i, 1))
        {
          j++;
          continue;
        }
  
      int source_fp_offset = source->aopu.aop_stk + soffset + i + (source->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);
      int source_sp_offset = source_fp_offset + _G.stack.pushed + _G.stack.offset;
      int result_fp_offset = result->aopu.aop_stk + roffset + i + (result->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);
      int result_sp_offset = result_fp_offset + _G.stack.pushed + _G.stack.offset;
        
      if (result_fp_offset == source_fp_offset && !regalloc_dry_run) // Stack locations can change, so in dry run do not assume stack coalescing will happen.
        {
          assigned[i] = true;
          j++;
          continue;
        }

      

      if (a_free || really_do_it_now)
        {
          if ((requiresHL (result)  || requiresHL (source)) && !hl_free)
            _push (PAIR_HL);
          cheapMove (result, roffset + i, source, soffset + i, a_free);
          if ((requiresHL (result)  || requiresHL (source)) && !hl_free)
            _pop (PAIR_HL);
          assigned[i] = true;
          (*size)--;
          j++;
          continue;
        }

       j++;
    }

  wassertl_bt (*size >= 0, "genCopyStack() copied more than there is to be copied.");
}

/*-----------------------------------------------------------------*/
/* genCopy - Copy the value from one reg/stk asmop to another      */
/*-----------------------------------------------------------------*/
static void
genCopy (asmop *result, int roffset, asmop *source, int soffset, int sizex, bool a_dead, bool hl_dead)
{
  int regsize, size, n = (sizex < source->size - soffset) ? sizex : (source->size - soffset);
  bool assigned[8] = {false, false, false, false, false, false, false, false};
  bool a_free, hl_free;
  int cached_byte = -1;
  bool pushed_a = false;

  wassertl_bt (n <= 8, "Invalid size for genCopy().");
  wassertl_bt (aopRS (source), "Invalid source type.");
  wassertl_bt (aopRS (result), "Invalid result type.");
  
  a_dead |= (result->regs[A_IDX] >= roffset && result->regs[A_IDX] < roffset + sizex);
  hl_dead |= (result->regs[L_IDX] >= roffset && result->regs[L_IDX] < roffset + sizex && result->regs[H_IDX] >= roffset && result->regs[H_IDX] < roffset + sizex);

  size = n;
  regsize = 0;
  for (int i = 0; i < n; i++)
    regsize += (source->type == AOP_REG);

  // Do nothing for coalesced bytes.
  for (int i = 0; i < n; i++)
    if (result->type == AOP_REG && source->type == AOP_REG && result->aopu.aop_reg[roffset + i] == source->aopu.aop_reg[soffset + i])
      {
        assigned[i] = true;
        regsize--;
        size--;
      }
  
  // Move everything from registers to the stack.
  wassert (source->type != AOP_REG || source->size < 8);
  for (int i = 0; i < n && source->type == AOP_REG;)
    {
      bool a_free = a_dead && (source->regs[A_IDX] < soffset || assigned[source->regs[A_IDX] - soffset] || i == source->regs[A_IDX] - soffset);
      bool hl_free = hl_dead && (source->regs[L_IDX] < soffset || assigned[source->regs[L_IDX] - soffset] || i == source->regs[L_IDX] - soffset) && (source->regs[H_IDX] < soffset || assigned[source->regs[H_IDX] - soffset] || i == source->regs[H_IDX] - soffset);

      int fp_offset = result->aopu.aop_stk + (result->aopu.aop_stk > 0 ? _G.stack.param_offset : 0) + roffset + i;
      int sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;

      /* S1C88: direct SP-relative 16-bit store `ld dd(sp), {ba,hl}` (74/75, dd a
         signed byte) — no `ex (sp),hl` swap, and it doesn't clobber the source
         pair (so no hl_dead requirement). Only BA and HL have SP stores (IX/IY
         have only SP loads), which aluPairId matches exactly. */
      if (i + 1 < n && aopOnStack (result, roffset + i, 2) &&
        sp_offset >= -128 && sp_offset <= 127 &&
        aluPairId (source, soffset + i) != PAIR_INVALID &&
        !regalloc_dry_run) // Stack positions will change, so do not assume this is possible in the cost function.
        {
          emit2 ("ld %d (sp), %s", sp_offset, _pairs[aluPairId (source, soffset + i)].name);
          cost (3, 6);
          assigned[i] = true;
          assigned[i + 1] = true;
          regsize -= 2;
          size -= 2;
          i += 2;
        }
      else if (i + 1 < n && aopOnStack (result, roffset + i, 2) && getPairId_o (source, soffset + i) != PAIR_INVALID && !sp_offset && !regalloc_dry_run) // Stack positions will change, so do not assume this is possible in the cost function.
        {
          bool iy = aopInReg (source, soffset + i, IY_IDX);
          emit2 ("inc sp");
          emit2 ("inc sp");
          emit2 ("push %s", _pairs[getPairId_o (source, soffset + i)].name);
          cost2 (3 + iy, 23 + 4 * iy);
          assigned[i] = true;
          assigned[i + 1] = true;
          regsize -= 2;
          size -= 2;
          i += 2;  
        }
      else if (aopOnStack (result, roffset + i, 1) && requiresHL (result) && !hl_free)
        {
          _push(PAIR_HL);
          cheapMove (result, roffset + i, source, soffset + i, a_free);
          _pop(PAIR_HL);
          assigned[i] = true;
          regsize--;
          size--;
          i++;
        }
      else if (aopRS (source) && !aopOnStack (source, soffset + i, 1) && aopOnStack (result, roffset + i, 1))
        {
          cheapMove (result, roffset + i, source, soffset + i, a_free);
          assigned[i] = true;
          regsize--;
          size--;
          i++;
        }
      else // This byte is not a register-to-stack copy.
        i++;
    }

  // Copy (stack-to-stack) what we can with whatever free regs we have.
  a_free = a_dead;
  hl_free = hl_dead;
  for (int i = 0; i < n; i++)
    {
      asmop *operand;
      int offset;

      if (!assigned[i])
        {
          operand = source;
          offset = soffset + i;
        }
      else
        {
          operand = result;
          offset = roffset + i;
        }

      if (aopInReg (operand, offset, A_IDX))
        a_free = false;
      else if (aopInReg (operand, offset, L_IDX) || aopInReg (operand, offset, H_IDX))
        hl_free = false;
    }
  genCopyStack (result, roffset, source, soffset, n, assigned, &size, a_free, hl_free, false);

  // Now do the register shuffling.

  // Try to use:
  // TLCS-90 ld rr, rr
  // All: push rr / pop iy
  // All: push iy / pop rr
  for (int i = 0; i + 1 < n; i++)
    {
      if (assigned[i] || assigned[i + 1])
        continue;

      for (int j = 0; j + 1 < n; j++)
        {
          if (!assigned[j] && i != j && i + 1 != j && !aopOnStack(result, roffset + i, 2) && !aopOnStack(source, soffset + i, 1) &&
            (result->aopu.aop_reg[roffset + i] == source->aopu.aop_reg[soffset + j] || result->aopu.aop_reg[roffset + i + 1] == source->aopu.aop_reg[soffset + j]))
            goto skip_byte_push_iy; // We can't write this one without overwriting the source.
        }

      if (aopInReg (result, roffset + i, IY_IDX) && getPairId_o (source, soffset + i) != PAIR_INVALID ||
        getPairId_o (result, roffset + i) != PAIR_INVALID && aopInReg (source, soffset + i, IY_IDX))
        {
          _push (getPairId_o (source, soffset + i));
          _pop (getPairId_o (result, roffset + i));
        }
      else
        continue;

      regsize -= 2;
      size -= 2;
      assigned[i] = true;
      assigned[i + 1] = true;

skip_byte_push_iy:
        ;
    }


  while (regsize && result->type == AOP_REG && source->type == AOP_REG)
    {
      int i;

      // Find lowest byte that can be assigned and needs to be assigned.
      for (i = 0; i < n; i++)
        {
          if (assigned[i])
            continue;

          for (int j = 0; j < n; j++)
            {
              if (!assigned[j] && i != j && result->aopu.aop_reg[roffset + i] == source->aopu.aop_reg[soffset + j])
                goto skip_byte; // We can't write this one without overwriting the source.
            }

          break;                // Found byte that can be written safely.

skip_byte:
          ;
        }

      if (i < n)
        {
          cheapMove (result, roffset + i, source, soffset + i, false);       // We can safely assign a byte.
          regsize--;
          size--;
          assigned[i] = true;
          if (aopInReg (result, roffset + i, A_IDX))
            a_free = false;
          continue;
        }

      // No byte can be assigned safely (i.e. the assignment is a permutation). Cache one in the accumulator.

      /* S1C88: the accumulator-cache trick below is unsound when A itself is
         inside the permutation cycle — `ld a, <src>` destroys A's old value
         before the byte sourced FROM A is read.  (Latent in the original
         too, but the BA-first argument ABI makes the plain A<->B swap common
         here: a temp allocated {B,A} sent to the BA argument register used to
         have its swap silently DROPPED, crossing the argument bytes — caught
         at runtime by tests/emu 04.)  Handle the A<->B 2-cycle with the native
         EX A,B; flag any other cycle through A so the dry-run cost steers the
         allocator away from it. */
      if (cached_byte == -1 && result->regs[A_IDX] >= roffset && result->regs[A_IDX] < roffset + n &&
        !assigned[result->regs[A_IDX] - roffset])
        {
          const int ai = result->regs[A_IDX] - roffset;   /* byte whose target is A */
          const int bi = result->regs[B_IDX] - roffset;   /* byte whose target is B (if any) */
          if (result->regs[B_IDX] >= roffset && result->regs[B_IDX] < roffset + n && !assigned[bi] &&
            source->aopu.aop_reg[soffset + ai]->rIdx == B_IDX &&
            source->aopu.aop_reg[soffset + bi]->rIdx == A_IDX)
            {
              emit2 ("ex a, b");
              cost (1, 2);
              assigned[ai] = true;
              assigned[bi] = true;
              regsize -= 2;
              size -= 2;
              a_free = false;
              continue;
            }
          UNIMPLEMENTED;   /* permutation cycle through A that is not the plain A<->B swap */
        }

      if (cached_byte != -1)
        {
          // Already one cached. Can happen when the assignment is a permutation consisting of multiple cycles.
          cheapMove (result, roffset + cached_byte, ASMOP_A, 0, true);
          cached_byte = -1;
          continue;
        }

      for (i = 0; i < n; i++)
        if (!assigned[i])
          break;

      wassertl_bt (i != n, "genCopy error: Trying to cache non-existent byte in accumulator.");
      if (!a_free && !pushed_a)
        {
          _push (PAIR_AF);
          pushed_a = TRUE;
        }
      cheapMove (ASMOP_A, 0, source, soffset + i, true);
      regsize--;
      size--;
      assigned[i] = TRUE;
      cached_byte = i;
    }

  // Copy (stack-to-stack) what we can with whatever free regs we have now.
  a_free = a_dead;
  hl_free = hl_dead;
  for (int i = 0; i < n; i++)
    {
      if (!assigned[i])
        continue;
      if (aopInReg (result, roffset + i, A_IDX))
        a_free = false;
      else if (aopInReg (result, roffset + i, L_IDX) || aopInReg (result, roffset + i, H_IDX))
        hl_free = false;
    }
  genCopyStack (result, roffset, source, soffset, n, assigned, &size, a_free, hl_free, false);

  
  
  // Last, move everything else from stack to registers.
  wassert (result->type != AOP_REG || result->size < 8);
  for (int i = 0; i < n && result->type == AOP_REG;)
    {
      const int fp_offset = source->aopu.aop_stk + soffset + i + (source->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);
      const int sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;

      bool a_free = a_dead && (result->regs[A_IDX] < roffset || !assigned[result->regs[A_IDX] - roffset]);
      const bool hl_free = hl_dead && (result->regs[L_IDX] < roffset || !assigned[result->regs[L_IDX] - roffset]) && (result->regs[H_IDX] < roffset || !assigned[result->regs[H_IDX] - roffset]);

      if (assigned[i])
        {
          i++;
          continue;
        }
      else if (i + 1 < n && !assigned[i + 1] && (source->type == AOP_STK || source->type == AOP_EXSTK) &&
        sp_offset >= -128 && sp_offset <= 127 &&
        (getPairId_o (result, roffset + i) == PAIR_HL || getPairId_o (result, roffset + i) == PAIR_BA ||
         getPairId_o (result, roffset + i) == PAIR_IX || getPairId_o (result, roffset + i) == PAIR_IY) &&
        (!regalloc_dry_run || source->aopu.aop_stk > 0)) // Stack locations might change, unless its a parameter.
        {
          if (!regalloc_dry_run)
            emit2 ("ld %s, %d (sp)", _pairs[getPairId_o (result, roffset + i)].name, sp_offset);
          spillPair (getPairId_o (result, roffset + i));
          cost (3, 6);
          assigned[i] = true;
          assigned[i + 1] = true;
          size -= 2;
          i += 2;
        }
      else if (i + 1 < n && !assigned[i + 1] && (source->type == AOP_STK || source->type == AOP_EXSTK) &&
        !sp_offset && getPairId_o (result, roffset + i) != PAIR_INVALID &&
        !regalloc_dry_run) // Stack locations might change.
        {
          PAIR_ID pair = getPairId_o (result, roffset + i);
          _pop (pair);
          _push (pair);
          assigned[i] = true;
          assigned[i + 1] = true;
          size -= 2;
          i += 2;
        }
      else if (i + 1 < n && !assigned[i + 1] && (source->type == AOP_STK || source->type == AOP_EXSTK) && sp_offset == 2 && getPairId_o (result, roffset + i) != PAIR_INVALID && getPairId_o (result, roffset + i) != PAIR_HL && hl_free && (!regalloc_dry_run || source->aopu.aop_stk > 0) && !optimize.codeSpeed) // A bit slower, so don't do it when optimizing for speed. HL is the only possible extrapair.
        {
          PAIR_ID pair = getPairId_o (result, roffset + i);
          PAIR_ID extrapair = PAIR_HL;
          _pop (extrapair);
          _pop (pair);
          _push (pair);
          _push (extrapair);
          spillPair (extrapair);
          assigned[i] = true;
          assigned[i + 1] = true;
          size -= 2;
          i += 2;
        }
      else if (i + 1 < n && !assigned[i + 1] && (source->type == AOP_STK || source->type == AOP_EXSTK) && requiresHL (source) &&
        (aopInReg (result, roffset + i, HL_IDX) || aopInReg (result, roffset + i, H_IDX) && aopInReg (result, roffset + i + 1, L_IDX))) // Stack access might go through hl.
        {
          bool a_pushed = false;
          if (!a_free)
            {
              _push (PAIR_AF);
              a_pushed = true;
              a_free = true;
            }
          cheapMove (ASMOP_A, 0, source, soffset + i, true);
          cheapMove (result, roffset + i + 1, source, soffset + i + 1, false);
          cheapMove (result, roffset + i, ASMOP_A, 0, true);
          if (a_pushed)
            _pop (PAIR_AF);
          assigned[i] = true;
          assigned[i + 1] = true;
          size -= 2;
          i += 2;
        }
      else if (aopRS (result) && aopOnStack (source, soffset + i, 1) && !aopOnStack (result, roffset + i, 1))
        {
          if (requiresHL (source) && !hl_free && (aopInReg (result, roffset + i, L_IDX) || aopInReg (result, roffset + i, H_IDX)))
            {
              if (!a_free)
                _push (PAIR_AF);
              _push (PAIR_HL);
              cheapMove (ASMOP_A, 0, source, soffset + i, true);
              _pop (PAIR_HL);
              cheapMove (result, roffset + i, ASMOP_A, 0, true);
              if (!a_free)
                _pop (PAIR_AF);
            }
          else
            {
              if (requiresHL (source) && !hl_free)
                _push (PAIR_HL);
              cheapMove (result, roffset + i, source, soffset + i, a_free);
              if (requiresHL (source) && source->type != AOP_REG && !hl_free)
                _pop (PAIR_HL);
              }
          assigned[i] = true;
          size--;
          i++;
        }
      else // This byte is not a register-to-stack copy.
        i++;
    }

  // Free a reg to copy (stack-to-stack) whatever is left.
  if (size)
    {
      a_free = a_dead && (result->regs[A_IDX] < 0 || result->regs[A_IDX] >= roffset + source->size);
      hl_free = hl_dead && (result->regs[L_IDX] < 0 || result->regs[L_IDX] >= roffset + source->size) && (result->regs[H_IDX] < 0 || result->regs[H_IDX] >= roffset + source->size);
      if (!a_free)
        _push (PAIR_AF);
      genCopyStack (result, roffset, source, soffset, n, assigned, &size, true, hl_free, true);
      if (!a_free)
        _pop (PAIR_AF);
    }

  wassertl_bt (size >= 0, "genCopy() copied more than there is to be copied.");

  a_free = a_dead && (result->regs[A_IDX] < 0 || result->regs[A_IDX] >= roffset + source->size);

  // Place leading zeroes.

  // todo

  if (cached_byte != -1)
    cheapMove (result, roffset + cached_byte, ASMOP_A, 0, true);

  if (pushed_a)
    _pop (PAIR_AF);
}

/*-----------------------------------------------------------------*/
/* genMove_o - Copy part of one asmop to another                   */
/*-----------------------------------------------------------------*/
static void
genMove_o (asmop *result, int roffset, asmop *source, int soffset, int size, bool a_dead_global, bool hl_dead_global, bool iy_dead_global, bool f_dead)
{
  wassert (result);
  wassert (result->size >= roffset + size);
  emitDebug ("; genMove_o size %d result type %d source type %d hl_dead %d", size, result->type, source->type, hl_dead_global);

  a_dead_global |= result->type == AOP_REG && result->regs[A_IDX] >= roffset && result->regs[A_IDX] < roffset + size;
  hl_dead_global |= result->type == AOP_REG && result->regs[L_IDX] >= roffset && result->regs[L_IDX] < roffset + size && result->regs[H_IDX] >= roffset && result->regs[H_IDX] < roffset + size;
  iy_dead_global |= result->type == AOP_REG && result->regs[IYL_IDX] >= roffset && result->regs[IYL_IDX] < roffset + size && result->regs[IYH_IDX] >= roffset && result->regs[IYH_IDX] < roffset + size;

  if (aopSame (result, roffset, source, soffset, size))
    return;

  if ((result->type == AOP_REG || result->type == AOP_STK || result->type == AOP_EXSTK) && (source->type == AOP_REG || source->type == AOP_STK || source->type == AOP_EXSTK))
    {
      int csize = size > source->size - soffset ? source->size - soffset : size;
      genCopy (result, roffset, source, soffset, csize, a_dead_global, hl_dead_global);
      roffset += csize;
      size -= csize;
      bool a_dead = a_dead_global && result->regs[A_IDX] < roffset;
      bool hl_dead = hl_dead_global && result->regs[H_IDX] < roffset && result->regs[L_IDX] < roffset;
      bool iy_dead = iy_dead_global && result->regs[IYH_IDX] < roffset && result->regs[IYL_IDX] < roffset;
      genMove_o (result, roffset, ASMOP_ZERO, 0, size, a_dead, hl_dead, iy_dead, f_dead);
      return;
    }

  bool zeroed_a = false;
  long value_hl = -1;

  for (int i = 0; i < size;)
    {
      bool a_dead = a_dead_global && source->regs[A_IDX] <= soffset + i && (result->regs[A_IDX] < 0 || result->regs[A_IDX] >= roffset + i);
      bool hl_dead = hl_dead_global && source->regs[L_IDX] <= soffset + i && source->regs[H_IDX] <= soffset + i && (result->regs[L_IDX] < 0 || result->regs[L_IDX] >= roffset + i) && (result->regs[H_IDX] < 0 || result->regs[H_IDX] >= roffset + i);
      bool iy_dead = iy_dead_global && source->regs[IYL_IDX] <= soffset + i && source->regs[IYH_IDX] <= soffset + i && (result->regs[IYL_IDX] < 0 || result->regs[IYL_IDX] >= roffset + i) && (result->regs[IYH_IDX] < 0 || result->regs[IYH_IDX] >= roffset + i);

      if (source->type == AOP_STL && (soffset + i) >= 2)
        {
          genMove_o (result, roffset + i, ASMOP_ZERO, 0, size - i, a_dead, hl_dead, iy_dead, f_dead);
          return;
        }
      else if (source->type == AOP_STL && !(soffset + i) && getPairId_o(result, roffset) == PAIR_IY)
        {
          if (!f_dead)
            _push (PAIR_AF);
          emit2 ("ld iy, !immed%d", spOffset (source->aopu.aop_stk));
          emit2 ("add iy, sp");
          cost2 (6, 29);
          if (!f_dead)
            _pop (PAIR_AF);
          spillPair (PAIR_IY);
          i += 2;
          continue;
        }
      else if (source->type == AOP_STL)
        {
          if (!hl_dead && (result->regs[L_IDX] > roffset || result->regs[H_IDX] > roffset))
            UNIMPLEMENTED;
          if (!hl_dead)
            _push (PAIR_HL);
          if (i + soffset > 1)
            UNIMPLEMENTED;
          if (!f_dead)
            _push (PAIR_AF);
          emit2 ("ld hl, !immed%d", spOffset (source->aopu.aop_stk));
          cost2 (3, 10);
          emit2 ("add hl, sp");
          cost2 (1, 11);
          if (!f_dead)
            _pop (PAIR_AF);
          spillPair (PAIR_HL);
          genMove_o (result, roffset + i, ASMOP_HL, soffset + i, size, a_dead, true, iy_dead, f_dead);
          if (!hl_dead)
            _pop (PAIR_HL);
          i += 2;
          continue;
        }

      if (i + 1 < size && getPairId_o(source, soffset + i) != PAIR_INVALID &&
        (result->type == AOP_IY || result->type == AOP_DIR || result->type == AOP_HL && (getPairId_o(source, soffset + i) == PAIR_HL || !hl_dead)))
        {
          emit2 ("ld !mems, %s", aopGetLitWordLong (result, roffset + i, false), _pairs[getPairId_o(source, soffset + i)].name);
          if (getPairId_o(source, soffset + i) == PAIR_HL)
            cost2 (3, 16);
          else
            cost2 (4, 20);
          i += 2;
          continue;
        }
      else if (i + 1 < size && soffset + i + 1 < source->size && getPairId_o(result, roffset + i) != PAIR_INVALID &&
        (source->type == AOP_IY || source->type == AOP_DIR || source->type == AOP_HL && (getPairId_o(result, roffset + i) == PAIR_HL || !hl_dead)))
        {
          emit2 ("ld %s, !mems", _pairs[getPairId_o(result, roffset + i)].name, aopGetLitWordLong (source, soffset + i, false));
          if (getPairId_o(result, roffset + i) == PAIR_HL)
            cost2 (3, 16);
          else
            cost2 (4, 20);
          spillPair (getPairId_o(result, roffset + i));
          i += 2;
          continue;      
        }
      else if (i + 1 < size && getPairId_o(result, roffset + i) != PAIR_INVALID &&
        (source->type == AOP_LIT && !(aopIsLitVal (source, soffset + i, 2, 0x0000) && zeroed_a) || source->type == AOP_IMMD))
        {
          fetchLitPair (getPairId_o(result, roffset + i), source, soffset + i, f_dead, false);
          i += 2;
          continue;
        }
      else if (i + 1 < size &&
        (result->type == AOP_IY || result->type == AOP_DIR || result->type == AOP_HL) &&
        source->type == AOP_IMMD && hl_dead)
        {
          genMove_o (ASMOP_HL, 0, source, soffset + i, 2, a_dead, true, iy_dead, f_dead);
          genMove_o (result, roffset + i, ASMOP_HL, 0, 2, a_dead, true, iy_dead, f_dead);
          i += 2;
          continue;
        }

      // 16-bit load into register might be cheaper than 8-bit, if the latter has to go through a. For bc and de it is only worth it if a would have to be saved.
      if ((optimize.allow_unsafe_read || i + 1 == size && soffset + i + 1 <= source->size) && result->type == AOP_REG && !aopInReg (result, roffset + i, A_IDX) &&
        (i + 1 == size || soffset + i + 1 >= source->size) && (source->type == AOP_HL && fetchLitPair (PAIR_HL, source, soffset + i, f_dead, true) || source->type == AOP_IY))
        {
          bool upper = aopInReg (result, roffset + i, H_IDX) || aopInReg (result, roffset + i, IYH_IDX);
          PAIR_ID pair = PAIR_INVALID;
          if ((aopInReg (result, roffset + i, L_IDX) || aopInReg (result, roffset + i, H_IDX)) && hl_dead)
            pair = PAIR_HL;
          else if ((aopInReg (result, roffset + i, IYL_IDX) || aopInReg (result, roffset + i, IYH_IDX)) && iy_dead)
            pair = PAIR_IY;

          if (pair != PAIR_INVALID && soffset + i - upper >= 0 && (optimize.allow_unsafe_read || upper || soffset + i + 1 < source->size))
            {
              emit2 ("ld %s, !mems", _pairs[pair].name, aopGetLitWordLong (source, soffset + i - upper, false));
              if (pair == PAIR_HL)
                cost2 (3, 16);
              else
                cost2 (4, 20);
              i++;
              spillPair (pair);
              continue;
            }
        }

      // Cache a copy of zero in a.
      if (f_dead && !zeroed_a && a_dead && source->regs[A_IDX] <= i &&
        (size > 1 && result->type != AOP_REG && aopIsLitVal (source, soffset + i, 2, 0x0000) ||
        size == 1 && (result->type == AOP_HL && fetchLitPair (PAIR_HL, result, roffset + i, f_dead, true) || result->type == AOP_IY && fetchLitPair (PAIR_IY, result, roffset + i, f_dead, true)) && aopIsLitVal (source, soffset + i, 1, 0x00)))
        {
          emit3 (A_XOR, ASMOP_A, ASMOP_A);
          zeroed_a = true;
        }

      if (result->type == AOP_HL && a_dead_global && (!hl_dead_global || source->regs[L_IDX] >= i || source->regs[H_IDX] >= i) && source->regs[A_IDX] <= i)
        {
          if (source->type == AOP_HL)
            {
              emit2 ("ld a, !mems", aopGetLitWordLong (source, soffset + i, false));
              cost2 (3, 13);
            }
          else if (!aopIsLitVal (source, soffset + i, 1, 0x00) || !zeroed_a)
            {
              cheapMove (ASMOP_A, 0, source, soffset + i, true);
              zeroed_a = aopIsLitVal (source, soffset + i, 1, 0x00);
            }
          emit2 ("ld !mems, a", aopGetLitWordLong (result, roffset + i, FALSE));
          cost2 (3, 13);
        }
      else if (aopIsLitVal (source, soffset + i, 1, 0x00) && zeroed_a)
        {
          if (requiresHL (result) && result->type != AOP_REG && !hl_dead)
            _push (PAIR_HL);
          cheapMove (result, roffset + i, ASMOP_A, 0, false);
          if (requiresHL (result) && result->type != AOP_REG && !hl_dead)
            _pop (PAIR_HL);
        }
      else if (aopIsLitVal (source, soffset + i, 1, 0x00) && aopInReg (result, roffset + i, A_IDX) && f_dead)
        {
          emit3 (A_XOR, ASMOP_A, ASMOP_A);
          zeroed_a = true;
        }
      else
        {
          bool pushed_hl = false;
          bool via_a = false;
          bool premoved_a = false;

          if (!i && a_dead && // Avoid setting up hl or iy for a single byte.
            (source->type == AOP_HL && fetchLitPair (PAIR_HL, source, soffset + i, f_dead, true) || source->type == AOP_IY && fetchLitPair (PAIR_IY, source, soffset + i, f_dead, true)) &&
            result->type == AOP_REG && (i + 1 > size || soffset + i < source->size))
            {
              {
                  emit2 ("ld a, !mems", aopGetLitWordLong (source, soffset + i, false));
                  cost2 (3, 13);
                  via_a = true;
                  premoved_a = true;
                }
            }
          else if ((requiresHL (result) && result->type != AOP_REG || requiresHL (source) && source->type != AOP_REG && soffset + i < source->size) && !hl_dead)
            {
              via_a = aopInReg (result, roffset + i, L_IDX) || aopInReg (result, roffset + i, H_IDX);
              if (via_a && !a_dead)
                _push (PAIR_AF);
              if (via_a && source->type == AOP_HL)
                {
                  emit2 ("ld a, !mems", aopGetLitWordLong (source, soffset + i, false));
                  cost2 (3, 13);
                  premoved_a = true;
                }
              else
                {
                  _push (PAIR_HL);
                  pushed_hl = true;
                }
            }
          else if (result->type == AOP_IY && !iy_dead && !aopInReg (source, soffset + i, A_IDX))
            {
              via_a = true;
              if (!a_dead)
                _push (PAIR_AF);
            }
          else if (!premoved_a && source->type == AOP_IY && result->type == AOP_REG && a_dead && i == 0 && i + 1 == size) // Using free a is cheaper than using iy.
            via_a = true;
          if (!premoved_a)
            {
              bool save_iy = !iy_dead && source->type == AOP_IY && (result->type == AOP_REG && !via_a && !aopInReg (result, roffset + i, A_IDX));
              if (save_iy)
                _push (PAIR_IY);
              cheapMove (via_a ? ASMOP_A : result, via_a ? 0 : (roffset + i), source, soffset + i, via_a || a_dead);
              if (save_iy)
                _pop (PAIR_IY);
            }
          if (pushed_hl)
            _pop (PAIR_HL);
          if (via_a)
            {
              if (requiresHL (result) && result->type != AOP_REG && !hl_dead)
                {
                  if (result->type == AOP_HL)
                    {
                      emit2 ("ld !mems, a", aopGetLitWordLong (result, roffset + i, FALSE));
                      cost2 (3, 13);
                    }
                  else
                    {
                      _push (PAIR_HL);
                      cheapMove (result, roffset + i, ASMOP_A, 0, true);
                      _pop (PAIR_HL);
                    }
                }
              else
                cheapMove (result, roffset + i, ASMOP_A, 0, true);
              if (!a_dead)
                _pop (PAIR_AF);
            }
          zeroed_a = false;
        }

      i++;
    }
}

/*-----------------------------------------------------------------*/
/* genMove - Copy the value from one asmop to another              */
/*-----------------------------------------------------------------*/
static void
genMove (asmop *result, asmop *source, bool a_dead, bool hl_dead, bool iy_dead)
{
  wassert (result);
  if (!result)   // wassert only logs (does not abort), so guard the deref to fail cleanly, not SIGSEGV
    return;
  genMove_o (result, 0, source, 0, result->size, a_dead, hl_dead, iy_dead, true);
}

/*--------------------------------------------------------------------------*/
/* adjustStack - Adjust the stack pointer by n bytes.                       */
/*--------------------------------------------------------------------------*/
static void
adjustStack (int n, bool af_free, bool hl_free, bool iy_free)
{
  if(n != 0)
    emitDebug("; adjustStack by %d", n);
  _G.stack.pushed -= n;


  /* S1C88: move SP with native ops. The classic stack-adjust idiom this function otherwise emits
     (push/pop af, pop bc/de, inc/dec sp) is illegal or flag-unsafe here — there
     is no AF/BC/DE register, and even inc/dec sp set Z C V N.
       - reserve (n<0): push a filler pair/byte. PUSH leaves flags untouched and
         needs no free register (the pushed value is overwritten before use), so
         this is always valid regardless of register pressure.
       - free (n>0): when flags are dead, one `add sp,#n` (signed 16-bit imm,
         either direction); otherwise pop into a genuinely-free pair (HL/IY) to
         preserve flags. (An earlier version also took `de_free`/`bc_free` hints —
         meaningless on the S1C88, removed with the phantom registers.) */
  {
      while (n <= -2)                    /* reserve 2 bytes, flag-safe filler */
        {
          emit2 ("push hl");
          cost2 (1, 11);
          n += 2;
        }
      if (n <= -1)                       /* reserve a final odd byte */
        {
          emit2 ("push a");
          cost2 (2, 11);
          n += 1;
        }
      if (n >= 2 && af_free)             /* free, flags dead: one native instruction */
        {
          emit2 ("add sp, !immed%d", n);
          cost2 (4, 12);
          n = 0;
        }
      while (n >= 2 && hl_free)          /* free, keep flags: pop into a free pair */
        {
          emit2 ("pop hl");
          cost2 (1, 10);
          spillPair (PAIR_HL);
          n -= 2;
        }
      while (n >= 2 && iy_free)
        {
          emit2 ("pop iy");
          cost2 (1, 10);
          spillPair (PAIR_IY);
          n -= 2;
        }
      if (n != 0)                        /* nothing free / odd free byte: clobbers flags */
        {
          emit2 ("add sp, !immed%d", n);
          cost2 (4, 12);
          n = 0;
        }
      wassert (!n);
      return;
    }
}

/** Put Acc into a register set
 */
static void
outAcc (operand * result)
{
  int size = result->aop->size;
  if (size)
    {
      cheapMove (result->aop, 0, ASMOP_A, 0, true);
      size--;
      genMove_o (result->aop, 1, ASMOP_ZERO, 0, size, true, false, true, true);
    }
}

/** Take the value in carry and put it into a register
 */
static void
outBitC (operand *result)
{
  /* if the result is bit */
  if (result->aop->type == AOP_CRY)
    {
      if (!IS_OP_RUONLY (result) && !regalloc_dry_run)
        aopPut (result->aop, "c", 0);  // Todo: Cost.
    }
  else
    {
      emit3 (A_LD, ASMOP_A, ASMOP_ZERO);
      /* carry -> A bit 0. The S1C88 has no accumulator-rotate `rla`; use the
         operand form `rl a` (same carry-in-to-bit-0; its extra Z/S flag
         effects are unused here). */
      emit3 (A_RL, ASMOP_A, 0);
      outAcc (result);
    }
}

/*-----------------------------------------------------------------*/
/* toBoolean - emit code for or a,operator(sizeop)                 */
/*-----------------------------------------------------------------*/
static void
_toBoolean (const operand *oper, bool needflag)
{
  int size = oper->aop->size;
  sym_link *type = operandType (oper);
  int skipbyte;

  if (size == 1 && needflag)
    {
      cheapMove (ASMOP_A, 0, oper->aop, 0, true);
      emit3 (A_OR, ASMOP_A, ASMOP_A);
      return;
    }

  if (size == 2 && oper->aop->type == AOP_STL)
    {
      _push(PAIR_HL);
      genMove (ASMOP_HL, oper->aop, true, false, false);
      emit2 ("ld a, l");
      emit3_8alu (A_OR, ASMOP_HL, 1, 0);   /* S1C88: or a,h is illegal — route h through b */
      _pop (PAIR_HL);
      return;
    }

  // Special handling to not overwrite a.
  if (oper->aop->regs[A_IDX] >= 0)
    skipbyte = oper->aop->regs[A_IDX];
  else
    {
      cheapMove (ASMOP_A, 0, oper->aop, size - 1, true);
      skipbyte = size - 1;
    }

  if (IS_FLOAT (type))
    {
      if (skipbyte != size - 1)
        UNIMPLEMENTED;
      emit2 ("and a, #0x7f");   // clear sign bit (S1C88 has no res)
      cost2 (2, 8);
      skipbyte = size - 1;
    }
  while (size--)
    if (size != skipbyte)
      {
        if (!HAS_IYL_INST && (aopInReg (oper->aop, size, IYL_IDX) || aopInReg (oper->aop, size, IYH_IDX)))
          UNIMPLEMENTED;
        else
          emit3_8alu (A_OR, oper->aop, size, 0);   /* S1C88: route an L/H source through B (or a,l/h illegal) */
      }
}

/*-----------------------------------------------------------------*/
/* castBoolean - emit code for casting operand to boolean in a     */
/*-----------------------------------------------------------------*/
static void
_castBoolean (const operand *right)
{
  emitDebug ("; Casting to bool");

  /* Can do without OR-ing for small arguments */
  if (right->aop->size == 1 && !aopInReg (right->aop, 0, A_IDX))
    {
      emit3 (A_XOR, ASMOP_A, ASMOP_A);
      if (!HAS_IYL_INST && (aopInReg (right->aop, 0, IYL_IDX) || aopInReg (right->aop, 0, IYH_IDX)))
        UNIMPLEMENTED;
      else
        emit3 (A_CP, ASMOP_A, right->aop);
    }
  else
    {
      _toBoolean (right, FALSE);
      emit2 ("add a, !immedbyte", 0xffu);
      cost2 (2, 7);
      emit3 (A_LD, ASMOP_A, ASMOP_ZERO);
    }
  emit3 (A_RL, ASMOP_A, 0);
}

/* Shuffle src reg array into dst reg array. */
static void
regMove (const short *dst, const short *src, size_t n, bool preserve_a) // Todo: replace uses of this one by uses of genMove_o?
{
  bool assigned[9] = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE };
  int cached_byte = -1;
  size_t size = n;
  size_t i;
  bool pushed_a = FALSE;

  wassert (n <= 9);

  // We need to be able to handle any assignment here, ensuring not to overwrite any parts of the source that we still need.
  while (size)
    {
      // Find lowest byte that can be assigned and needs to be assigned.
      for (i = 0; i < n; i++)
        {
          size_t j;

          if (assigned[i])
            continue;

          for (j = 0; j < n; j++)
            {
              if (!assigned[j] && i != j && dst[i] == src[j])
                goto skip_byte; // We can't write this one without overwriting the source.
            }

          break;                // Found byte that can be written safely.

skip_byte:
          ;
        }

      if (i < n)
        {
          cheapMove (asmopregs[dst[i]], 0, asmopregs[src[i]], 0, false);       // We can safely assign a byte.
          size--;
          assigned[i] = TRUE;
          continue;
        }

      // No byte can be assigned safely (i.e. the assignment is a permutation). Cache one in the accumulator.

      if (cached_byte != -1)
        {
          // Already one cached. Can happen when the assignment is a permutation consisting of multiple cycles.
          cheapMove (asmopregs[dst[cached_byte]], 0, ASMOP_A, 0, true);
          cached_byte = -1;
          continue;
        }

      for (i = 0; i < n; i++)
        if (!assigned[i])
          break;

      wassertl (i != n, "regMove error: Trying to cache non-existent byte in accumulator.");
      if (preserve_a && !pushed_a)
        {
          _push (PAIR_AF);
          pushed_a = TRUE;
        }
      cheapMove (ASMOP_A, 0, asmopregs[src[i]], 0, true);
      size--;
      assigned[i] = TRUE;
      cached_byte = i;
    }

  if (cached_byte != -1)
    cheapMove (asmopregs[dst[cached_byte]], 0, ASMOP_A, 0, true);

  if (pushed_a)
    _pop (PAIR_AF);
}

/*-----------------------------------------------------------------*/
/* genNot - generate code for ! operation                          */
/*-----------------------------------------------------------------*/
static void
genNot (const iCode * ic)
{
  operand *left = IC_LEFT (ic);
  operand *result = IC_RESULT (ic);

  /* assign asmOps to operand & result */
  aopOp (left, ic, FALSE, TRUE);
  aopOp (result, ic, TRUE, FALSE);

  /* if in bit space then a special case */
  if (left->aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to negate a bit");
    }
  else if (IS_BOOL (operandType (left)))
    {
      cheapMove (ASMOP_A, 0, left->aop, 0, true);
      emit3 (A_XOR, ASMOP_A, ASMOP_ONE);
      cheapMove (result->aop, 0, ASMOP_A, 0, true);
      goto release;
    }
  

  _toBoolean (left, FALSE);

  /* Not of A:
     If A == 0, !A = 1
     else A = 0
     So if A = 0, A-1 = 0xFF and C is set, rotate C into reg. */
  emit3 (A_SUB, ASMOP_A, ASMOP_ONE);
  outBitC (result);

release:
  /* release the aops */
  freeAsmop (left, NULL);
  freeAsmop (result, NULL);
}

/* Pop saved regs from stack, taking care not to destroy result */
static void
restoreRegs (bool iy, bool bc, bool hl, const operand *result, const iCode *const ic)
{
  bool a_live, b_live, h_live, l_live, iyl_live, iyh_live;
  bool SomethingReturned;

  SomethingReturned = result && IS_ITEMP (result) && (OP_SYMBOL_CONST (result)->nRegs || OP_SYMBOL_CONST (result)->spildir)
                      || IS_TRUE_SYMOP (result);

  if (SomethingReturned)
    {
      bitVect *rv = s1c88_rUmaskForOp (result);
      a_live = bitVectBitValue (rv, A_IDX);
      b_live = bitVectBitValue (rv, B_IDX);
      h_live = bitVectBitValue (rv, H_IDX);
      l_live = bitVectBitValue (rv, L_IDX);
      iyh_live = bitVectBitValue (rv, IYH_IDX);
      iyl_live = bitVectBitValue (rv, IYL_IDX);
      freeBitVect (rv);
    }
  else
    {
      a_live = false;
      b_live = false;
      h_live = false;
      l_live = false;
      iyh_live = false;
      iyl_live = false;
    }

  if (ic)
    {
      if (!isRegDead (A_IDX, ic))
        a_live = true;
      
      
      if (!bc && !isRegDead (B_IDX, ic))
        b_live = true;
      
      if (!hl && !isRegDead (H_IDX, ic))
        h_live = true;
      if (!hl && !isRegDead (L_IDX, ic))
        l_live = true;
      if (!iy && !isRegDead (IYH_IDX, ic))
        iyh_live = true;
      if (!iy && !isRegDead (IYL_IDX, ic))
        iyl_live = true;
    }

  if (iy)
    {
      if (iyh_live && iyl_live)
        wassertl (0, "Shouldn't push IY if it's wiped out by the return");
      else if (iyh_live)
        poppairwithsavedreg (PAIR_IY, IYH_IDX, -1);
      else if (iyl_live)
        poppairwithsavedreg (PAIR_IY, IYL_IDX, -1);
      else
        _pop (PAIR_IY);
    }

  if (bc)
    {
      /* S1C88: the save was a 1-byte push of B alone (see genCall) */
      if (b_live)
        {
          emit2 ("inc sp");        /* result occupies B - discard the stale save */
          cost2 (1, 6);
        }
      else
        {
          emit2 ("pop b");
          cost2 (1, 10);
        }
      _G.stack.pushed -= 1;
    }

  if (hl)
    {
      if (h_live && l_live)
        wassertl (0, "Shouldn't push HL if it's wiped out by the return");
      else if (h_live && !a_live)
        poppairwithsavedreg (PAIR_HL, H_IDX, A_IDX);
      else if (h_live && !bc && !b_live)
        poppairwithsavedreg (PAIR_HL, H_IDX, B_IDX);
      else if (h_live)
        poppairwithsavedreg (PAIR_HL, H_IDX, -1);
      else if (l_live && !a_live)
        {
          /* Only restore H */
          _pop (PAIR_AF);
          emit2 ("ld h, a");
          cost2 (1, 4);
        }
      else if (l_live&& !a_live )
        poppairwithsavedreg (PAIR_HL, L_IDX, A_IDX);
      else if (l_live && !bc && !b_live )
        poppairwithsavedreg (PAIR_HL, L_IDX, B_IDX);
      else if (l_live)
        poppairwithsavedreg (PAIR_HL, L_IDX, -1);
      else
        _pop (PAIR_HL);
    }
}

static void
_saveRegsForCall (const iCode *ic, bool saveHLifused, bool dontsaveIY)
{
  /* Rules:
     o Stack parameters are pushed before this function enters
     o DE and BC may be used in this function.
     o HL and DE may be used to return the result.
     o HL and DE may be used to send variables.
     o DE and BC may be used to store the result value.
     o HL may be used in computing the sent value of DE
     o The iPushes for other parameters occur before any addSets

     Logic: (to be run inside the first iPush or if none, before sending)
     o Compute if DE, BC, HL, IY are in use over the call
     o Compute if DE is used in the send set
     o Compute if DE and/or BC are used to hold the result value
     o If (DE is used, or in the send set) and is not used in the result, push.
     o If BC is used and is not in the result, push
     o
     o If DE is used in the send set, fetch
     o If HL is used in the send set, fetch
     o Call
     o ...
   */

  sym_link *dtype = operandType (IC_LEFT (ic));
  sym_link *ftype = IS_FUNCPTR (dtype) ? dtype->next : dtype;

  if (IS_FUNCPTR (dtype))
    saveHLifused = true;
  if (!_G.saves.saved)
    {
      const bool call_preserves_b = ftype->funcAttrs.preserved_regs[B_IDX] && !s1c88IsParmInCall(ftype, "b");
      const bool call_preserves_h = ftype->funcAttrs.preserved_regs[H_IDX] && !s1c88IsParmInCall(ftype, "h");
      const bool call_preserves_l = ftype->funcAttrs.preserved_regs[L_IDX] && !s1c88IsParmInCall(ftype, "l");
      /* S1C88: of the dropped BC/DE bytes only B exists — it gets a 1-byte slot */
      const bool push_bc = !isRegDead (B_IDX, ic) && !call_preserves_b;
      const bool push_hl = !isRegDead (H_IDX, ic) && (!call_preserves_h || saveHLifused) || !isRegDead (L_IDX, ic) && (!call_preserves_l || saveHLifused);
      const bool push_iy = !dontsaveIY && (!isRegDead (IYH_IDX, ic) || !isRegDead (IYL_IDX, ic));

      if (push_hl)
        {
          _push (PAIR_HL);
          _G.stack.pushedHL = TRUE;
        }
      if (push_bc)
        {
          emit2 ("push b");
          cost2 (1, 11);
          _G.stack.pushed += 1;
          _G.stack.pushedBC = TRUE;
        }
      if (push_iy)
        {
          _push (PAIR_IY);
          _G.stack.pushedIY = TRUE;
        }

      if (!regalloc_dry_run)
        _G.saves.saved = TRUE;
    }
  else
    {
      /* Already saved. */
    }
}

/*-----------------------------------------------------------------*/
/* genIpush - genrate code for pushing this gets a little complex  */
/*-----------------------------------------------------------------*/
static void
genIpush (const iCode *ic)
{
  /* if this is not a parm push : ie. it is spill push
     and spill push is always done on the local stack */
  if (!ic->parmPush)
    {
      wassertl (0, "Encountered an unsupported spill push.");
      return;
    }

  /* Scan ahead until we find the function that we are pushing parameters to.
     Count the number of addSets on the way to figure out what registers
     are used in the send set.
   */
  int nAddSets = 0;
  iCode *walk = ic->next;

  while (walk)
    {
      if (walk->op == SEND && !_G.saves.saved && !regalloc_dry_run)
        nAddSets++;
      else if (walk->op == CALL || walk->op == PCALL)
        break; // Found it.

      walk = walk->next; // Keep looking.
    }
  if (!regalloc_dry_run && !_G.saves.saved && !regalloc_dry_run) /* Cost is counted at CALL or PCALL instead */
    _saveRegsForCall (walk, true, false); /* Caller saves, and this is the first iPush. */

  sym_link *ftype = operandType (IC_LEFT (walk));
  if (walk->op == PCALL)
    ftype = ftype->next;
  const bool smallc = IFFUNC_ISSMALLC (ftype);

  /* then do the push */
  aopOp (IC_LEFT (ic), ic, FALSE, FALSE);

  int size = IC_LEFT (ic)->aop->size;

  /* Registers already loaded by preceding SENDs for this same call hold live
     argument values until the call, but a literal/immediate SEND (e.g. a `char`
     arg) creates no live range, so isRegDead() reports the register free here.
     The S1C88
     pushes via BA, so using A/B as the push vehicle would clobber an
     already-sent argument. Mark the registers held by preceding SENDs occupied
     so they are excluded from the push below. (genSend does the symmetric scan.) */
  bool send_a = false, send_b = false, send_h = false, send_l = false, send_iyl = false, send_iyh = false;
  for (iCode *walk2 = ic->prev; walk2 && (walk2->op == SEND || walk2->op == IPUSH); walk2 = walk2->prev)
    {
      if (walk2->op != SEND)
        continue;
      asmop *warg = aopArg (ftype, walk2->argreg);
      if (!warg)
        continue;
      send_a |= (warg->regs[A_IDX] >= 0);
      send_b |= (warg->regs[B_IDX] >= 0);
      send_h |= (warg->regs[H_IDX] >= 0);
      send_l |= (warg->regs[L_IDX] >= 0);
      send_iyl |= (warg->regs[IYL_IDX] >= 0);
      send_iyh |= (warg->regs[IYH_IDX] >= 0);
    }

  if (size == 1 && smallc) /* The SmallC calling convention pushes 8-bit parameters as 16-bit values. */
    {
      if (IC_LEFT (ic)->aop->type == AOP_REG && IC_LEFT (ic)->aop->aopu.aop_reg[0]->rIdx == L_IDX)
        {
          emit2 ("push hl");
          cost2 (1, 11);
        }
      else if (isRegDead (HL_IDX, ic))
        {
          cheapMove (ASMOP_L, 0, IC_LEFT (ic)->aop, 0, true);
          emit2 ("push hl");
          cost2 (1, 11);
        }
      else if (isRegDead (A_IDX, ic))
        {
          cheapMove (ASMOP_A, 0, IC_LEFT (ic)->aop, 0, true);
          emit2 ("push a");      /* native 1-byte push */
          cost2 (2, 11);
        }
      else {
          /* all live: stage in L inside a reserved slot, hl) */
          emit2 ("push hl");                /* the slot */
          cost2 (1, 11);
          emit2 ("push hl");                /* save the live HL */
          cost2 (1, 11);
          _G.stack.pushed += 4;
          cheapMove (ASMOP_L, 0, IC_LEFT (ic)->aop, 0, false);
          emit2 ("ld %d (sp), hl", 2);
          cost2 (3, 0);
          emit2 ("pop hl");
          cost2 (1, 10);
          _G.stack.pushed -= 4;
          spillPair (PAIR_HL);
        }

      if (!regalloc_dry_run)
        _G.stack.pushed += 2;
      goto release;
    }

    while (size)
      {
        int d = 0;

        bool a_free = isRegDead (A_IDX, ic) && !send_a && (IC_LEFT (ic)->aop->regs[A_IDX] < 0 || IC_LEFT (ic)->aop->regs[A_IDX] >= size - 1);
        bool b_free = isRegDead (B_IDX, ic) && !send_b && (IC_LEFT (ic)->aop->regs[B_IDX] < 0 || IC_LEFT (ic)->aop->regs[B_IDX] >= size - 1);
        bool h_free = isRegDead (H_IDX, ic) && !send_h && (IC_LEFT (ic)->aop->regs[H_IDX] < 0 || IC_LEFT (ic)->aop->regs[H_IDX] >= size - 1);
        bool l_free = isRegDead (L_IDX, ic) && !send_l && (IC_LEFT (ic)->aop->regs[L_IDX] < 0 || IC_LEFT (ic)->aop->regs[L_IDX] >= size - 1);
        bool iyh_free = isRegDead (IYH_IDX, ic) && !send_iyh && (IC_LEFT (ic)->aop->regs[IYH_IDX] < 0 || IC_LEFT (ic)->aop->regs[IYH_IDX] >= size - 1);
        bool iyl_free = isRegDead (IYL_IDX, ic) && !send_iyl && (IC_LEFT (ic)->aop->regs[IYL_IDX] < 0 || IC_LEFT (ic)->aop->regs[IYL_IDX] >= size - 1);
        bool hl_free = isPairDead (PAIR_HL, ic) && (h_free || IC_LEFT (ic)->aop->regs[H_IDX] >= size - 2) && (l_free || IC_LEFT (ic)->aop->regs[L_IDX] >= size - 2);
        bool ba_free = isPairDead (PAIR_BA, ic) && (b_free || IC_LEFT (ic)->aop->regs[B_IDX] >= size - 2) && (a_free || IC_LEFT (ic)->aop->regs[A_IDX] >= size - 2);

        if (getPairId_o (IC_LEFT (ic)->aop, size - 2) != PAIR_INVALID || aluPairId (IC_LEFT (ic)->aop, size - 2) == PAIR_BA)
          {
            /* S1C88: getPairId_o doesn't recognize BA, so push the low word already
               in B:A directly (`push ba`) instead of shuffling it into phantom BC. */
            PAIR_ID pp = getPairId_o (IC_LEFT (ic)->aop, size - 2);
            if (pp == PAIR_INVALID)
              pp = PAIR_BA;
            emit2 ("push %s", _pairs[pp].name);
            if (pp == PAIR_IY)
              cost2 (2, 15);
            else
              cost2 (1, 11);
            d = 2;
          }
#if 0 // Fails regression tests. Simulator issue regarding flags?
        // Push a 16-bit zero via the flag-setup hack.
        
#endif
        else if (size >= 2 &&
          (hl_free || ba_free || !IY_RESERVED && isPairDead (PAIR_IY, ic) ||
          aopInReg (IC_LEFT (ic)->aop, size - 1, B_IDX) && a_free || b_free && aopInReg (IC_LEFT (ic)->aop, size - 2, A_IDX) ||
          aopInReg (IC_LEFT (ic)->aop, size - 1, H_IDX) && l_free || h_free && aopInReg (IC_LEFT (ic)->aop, size - 2, L_IDX)))
          {
            /* S1C88: the free-pair candidates are HL and BA (the byte-
               addressable pairs — genMove and the literal loop below can write
               their halves) and the index pair IY (loadable from any source
               via genMove_o, incl. 16-bit literals — just not byte-writable). */
            asmop *pair = 0;

            if (hl_free)
              pair = ASMOP_HL;
            else if (ba_free)
              pair = ASMOP_BA;
            else if (!IY_RESERVED && isPairDead (PAIR_IY, ic))
              pair = ASMOP_IY;

            /* one half already in place? finish that pair instead */
            if (aopInReg (IC_LEFT (ic)->aop, size - 1, H_IDX) && l_free || h_free && aopInReg (IC_LEFT (ic)->aop, size - 2, L_IDX))
              pair = ASMOP_HL;
            else if (aopInReg (IC_LEFT (ic)->aop, size - 1, B_IDX) && a_free || b_free && aopInReg (IC_LEFT (ic)->aop, size - 2, A_IDX))
              pair = ASMOP_BA;

            wassert (pair);
            PAIR_ID ppid = (pair == ASMOP_BA) ? PAIR_BA : getPairId (pair);
            genMove_o (pair, 0, IC_LEFT (ic)->aop, size - 2, 2, a_free, hl_free, true, true);
            emit2 ("push %s", _pairs[ppid].name);
            cost2 (1, 11);
            d = 2;

            // For hl and iy, genMove_o caches literal values better than this loop.
            while (ppid == PAIR_BA && IC_LEFT (ic)->aop->type == AOP_LIT && !IS_FLOAT (IC_LEFT (ic)->aop->aopu.aop_lit->type) && size - (d+2) >= 0)
              {
                unsigned long current = (ullFromVal(IC_LEFT (ic)->aop->aopu.aop_lit)>>((size - d    )*8)) & 0xFFFF;
                unsigned long next = (ullFromVal(IC_LEFT (ic)->aop->aopu.aop_lit)>>((size - (d+2))*8)) & 0xFFFF;
                if (current == next)
                  {
                    emitDebug ("; genIpush identical value again");
                    emit2 ("push %s", _pairs[ppid].name);
                    cost2 (1, 11);
                    d+=2;
                  }
                else if ((current & 0xFF) == (next & 0xFF))
                  {
                    emitDebug ("; genIpush similar value again");
                    emit2 ("ld %s, !immedbyte", _pairs[ppid].h, (unsigned)(next >> 8));
                    cost2 (2, 7);
                    emit2 ("push %s", _pairs[ppid].name);
                    cost2 (1, 11);
                    d+=2;
                  }
                else if ((current & 0xFF00) == (next & 0xFF00))
                  {
                    emitDebug ("; genIpush similar value again");
                    emit2 ("ld %s, !immedbyte", _pairs[ppid].l, (unsigned)(next & 0xffu));
                    cost2 (2, 7);
                    emit2 ("push %s", _pairs[ppid].name);
                    cost2 (1, 11);
                    d+=2;
                  }
                else
                  break;
              }
         }
       else if (size >= 2 && !IY_RESERVED && isPairDead (PAIR_IY, ic) && (IC_LEFT (ic)->aop->type == AOP_LIT || IC_LEFT (ic)->aop->type == AOP_IMMD))
         {
           genMove_o (ASMOP_IY, 0, IC_LEFT (ic)->aop, size - 2, 2, a_free, hl_free, true, true);
           emit2 ("push iy");
           cost2 (2, 1);
           d = 2;
         }
       else if (size >= 2)
         {
           /* All pairs are live. S1C88 has no `ex (sp), hl` (which would
              both store the value and restore HL): reserve the slot, save the
              live HL with a second push, store through SP, restore HL. */
           emit2 ("push hl");                /* the argument slot */
           cost2 (1, 11);
           emit2 ("push hl");                /* save the live HL */
           cost2 (1, 11);
           _G.stack.pushed += 4;
           genMove_o (ASMOP_HL, 0, IC_LEFT (ic)->aop, size - 2, 2, a_free, true, true, true);
           emit2 ("ld %d (sp), hl", 2);
           cost2 (3, 0);
           emit2 ("pop hl");
           cost2 (1, 10);
           _G.stack.pushed -= 4;
           spillPair (PAIR_HL);
           d = 2;
         }
       else if (aopInReg (IC_LEFT (ic)->aop, size - 1, A_IDX) || aopInReg (IC_LEFT (ic)->aop, size - 1, B_IDX) ||
         aopInReg (IC_LEFT (ic)->aop, size - 1, L_IDX) || aopInReg (IC_LEFT (ic)->aop, size - 1, H_IDX))
         {
           /* S1C88: native 1-byte register push. */
           if (!regalloc_dry_run)
             emit2 ("push %s", aopGet (IC_LEFT (ic)->aop, size - 1, FALSE));
           cost2 (2, 11);
           d = 1;
         }
       else if (a_free || b_free || h_free || l_free)
         {
           asmop *tmp = a_free ? ASMOP_A : b_free ? ASMOP_B : h_free ? ASMOP_H : ASMOP_L;
           const char *tname = a_free ? "a" : b_free ? "b" : h_free ? "h" : "l";
           genMove_o (tmp, 0, IC_LEFT (ic)->aop, size - 1, 1, a_free, h_free && l_free, iyh_free && iyl_free, true);
           emit2 ("push %s", tname);
           cost2 (2, 11);
           d = 1;
         }
      else {
           /* Every byte register is live: stage the byte in H inside a
              reserved pair slot, then drop the slot's low garbage byte. */
           emit2 ("push hl");                /* the (pair) argument slot */
           cost2 (1, 11);
           emit2 ("push hl");                /* save the live HL */
           cost2 (1, 11);
           _G.stack.pushed += 4;
           genMove_o (ASMOP_H, 0, IC_LEFT (ic)->aop, size - 1, 1, false, false, iyh_free && iyl_free, true);
           emit2 ("ld %d (sp), hl", 2);
           cost2 (3, 0);
           emit2 ("pop hl");
           cost2 (1, 10);
           emit2 ("inc sp");                 /* drop the slot's low byte */
           cost2 (1, 6);
           _G.stack.pushed -= 4;
           spillPair (PAIR_HL);
           d = 1;
         }

       if (!regalloc_dry_run)
          _G.stack.pushed += d;
       size -= d;
     }

release:
  freeAsmop (IC_LEFT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genPointerPush - generate code for pushing                      */
/*-----------------------------------------------------------------*/
static void
genPointerPush (const iCode *ic)
{
   /* Scan ahead until we find the function that we are pushing parameters to.
     Count the number of addSets on the way to figure out what registers
     are used in the send set.
   */
  int nAddSets = 0;
  iCode *walk = ic->next;

  while (walk)
    {
      if (walk->op == SEND && !_G.saves.saved && !regalloc_dry_run)
        nAddSets++;
      else if (walk->op == CALL || walk->op == PCALL)
        break; // Found it.

      walk = walk->next; // Keep looking.
    }
  if (!regalloc_dry_run && !_G.saves.saved) /* Cost is counted at CALL or PCALL instead */
    _saveRegsForCall (walk, true, false); /* Caller saves, and this is the first iPush. */

  sym_link *ftype = operandType (IC_LEFT (walk));
  if (walk->op == PCALL)
    ftype = ftype->next;
  const bool smallc = IFFUNC_ISSMALLC (ftype);

  /* then do the push */
  aopOp (IC_LEFT (ic), ic, false, false);

  wassertl (IC_RIGHT (ic), "IPUSH_VALUE_AT_ADDRESS without right operand");
  wassertl (IS_OP_LITERAL (IC_RIGHT (ic)), "IPUSH_VALUE_AT_ADDRESS with non-literal right operand");

  int offset = operandLitValue (IC_RIGHT(ic));

  wassert (!offset);
  wassert (!smallc);

  /* A preceding SEND for this same call may have already loaded an argument
     into a register we are about to clobber (the walk pointer HL, the A/B push
     vehicles). A literal/immediate SEND creates no live range, so isRegDead()
     reports the register free here — scan the preceding SENDs explicitly (as
     genIpush does) and treat those registers as occupied so the struct push
     does not destroy an already-sent register argument. This happens when a
     struct-by-value argument precedes a register argument in the parameter
     list, e.g. f(struct s, int y): y is sent to HL, then this push walks the
     struct through HL. */
  bool send_a = false, send_b = false, send_h = false, send_l = false, send_iyl = false, send_iyh = false;
  for (iCode *walk2 = ic->prev; walk2 && (walk2->op == SEND || walk2->op == IPUSH); walk2 = walk2->prev)
    {
      if (walk2->op != SEND)
        continue;
      asmop *warg = aopArg (ftype, walk2->argreg);
      if (!warg)
        continue;
      send_a   |= (warg->regs[A_IDX]   >= 0);
      send_b   |= (warg->regs[B_IDX]   >= 0);
      send_h   |= (warg->regs[H_IDX]   >= 0);
      send_l   |= (warg->regs[L_IDX]   >= 0);
      send_iyl |= (warg->regs[IYL_IDX] >= 0);
      send_iyh |= (warg->regs[IYH_IDX] >= 0);
    }

  /* The push needs HL as the walk pointer and A/B as the byte/word vehicle. A
     preceding SEND may have parked an argument in either pair (HL for the int/
     pointer overflow regs, BA when the struct is the FIRST argument so the next
     int arg takes BA). Save whichever pair holds a sent value into the dead IY
     across the push, then restore it before the call. Only one IY slot exists,
     so a call that parked sent args in BOTH pairs (e.g. f(struct, int, int))
     traps loudly rather than miscompiling. */
  bool stash_hl = !isRegDead (HL_IDX, ic) || send_h || send_l;
  bool stash_ba = send_a || send_b;

  if (stash_hl && stash_ba)
    UNIMPLEMENTED;

  /* A live A that is NOT a saved sent-arg can't be clobbered by the vehicle. */
  if (!isRegDead (A_IDX, ic) && !stash_ba)
    UNIMPLEMENTED;

  asmop *stashed = stash_hl ? ASMOP_HL : stash_ba ? ASMOP_BA : 0;
  if (stashed)
    {
      if (!isRegDead (IY_IDX, ic) || send_iyl || send_iyh)
        UNIMPLEMENTED;
      emit2 ("ld iy, %s", stash_hl ? "hl" : "ba");
      cost2 (2, 0);
    }

  genMove (ASMOP_HL, IC_LEFT (ic)->aop, true, true, stashed ? false : isRegDead (IY_IDX, ic));

  int size = getSize (operandType (ic->left)->next);
  /* With BA's sent value saved in IY, the physical A/B are free as the vehicle. */
  bool b_free = stash_ba ? true : (isRegDead (B_IDX, ic) && !send_b);
  if (size > 3)
    {
      emit2 ("add hl, !immed%d", size - 1);   /* native 16-bit immediate add */
      cost2 (3, 0);
    }
  else
    for(int i = 1; i < size; i++)
      emit3w (A_INC, ASMOP_HL, 0);

  for(int i = 0; i < size;)
    {
      if (i + 1 < size && b_free)
        {
          /* word: high byte -> B, low -> A, push ba (high half at the higher address) */
          emit2 ("ld b, !*hl");
          cost2 (1, 7);
          emit2 ("dec hl");
          cost2 (1, 6);
          emit2 ("ld a, !*hl");
          cost2 (1, 7);
          emit2 ("push ba");
          cost2 (1, 11);
          _G.stack.pushed += 2;
          i += 2;
        }
      else
        {
          emit2 ("ld a, !*hl");
          cost2 (1, 7);
          emit2 ("push a");                    /* native 1-byte push */
          cost2 (2, 11);
          if (!regalloc_dry_run)
            _G.stack.pushed++;
          i++;
        }

      if (i < size) // Both to save an instruction on the last byte, and to ensure we get the correct value as cached for hl.
        {
          emit2 ("dec hl");
          cost2 (1, 6);
        }
    }

  if (stashed)
    {
      emit2 ("ld %s, iy", stash_hl ? "hl" : "ba");
      cost2 (2, 0);
      spillPair (PAIR_IY);
      if (stash_ba)
        spillPair (PAIR_BA);
    }
  spillPair (PAIR_HL);

  freeAsmop (IC_LEFT (ic), 0);
}

/* This is quite unfortunate */
static void
setArea (int inHome)
{
  /*
     static int lastArea = 0;

     if (_G.in_home != inHome) {
     if (inHome) {
     const char *sz = port->mem.code_name;
     port->mem.code_name = "HOME";
     emit2("!area", CODE_NAME);
     port->mem.code_name = sz;
     }
     else
     emit2("!area", CODE_NAME); */
  _G.in_home = inHome;
  //    }
}

static bool
isInHome (void)
{
  return _G.in_home;
}

/** Emit the code for a register parameter
 */
static void genSend (const iCode *ic)
{
  aopOp (IC_LEFT (ic), ic, FALSE, FALSE);
  
  /* Caller saves, and this is the first push/send. */
  // Scan ahead until we find the function that we are pushing/sending parameters to.
  const iCode *walk;
  for (walk = ic->next; walk; walk = walk->next)
    {
      if (walk->op == CALL || walk->op == PCALL)
        break;
    }

  if (!_G.saves.saved && !regalloc_dry_run) // Cost is counted at CALL or PCALL instead
    _saveRegsForCall (walk, requiresHL (ic->left->aop) || ic->next->op != CALL, false);

  sym_link *ftype = IS_FUNCPTR (operandType (IC_LEFT (walk))) ? operandType (IC_LEFT (walk))->next : operandType (IC_LEFT (walk));
  asmop *argreg = aopArg (ftype, ic->argreg);
  
  wassert (argreg);

  // The register argument shall not overwrite a still-needed (i.e. as further parameter or function for the call) value.
  if (!aopSame (argreg, 0, ic->left->aop, 0, argreg->size))
    for (int i = 0; i < argreg->size; i++)
      if (!isRegDead (argreg->aopu.aop_reg[i]->rIdx, ic))
        for (iCode *walk2 = ic->next; walk2; walk2 = walk2->next)
          {
            if (walk2->op != CALL && walk2->left && IS_ITEMP (walk2->left))
              UNIMPLEMENTED;

            if (walk2->op == CALL || walk2->op == PCALL)
              break;
          }

  bool a_dead = isRegDead (A_IDX, ic);
  bool hl_dead = isPairDead (PAIR_HL, ic);
  bool iy_dead = true;

  for (iCode *walk2 = ic->prev; walk2 && walk2->op == SEND; walk2 = walk2->prev)
    {
      asmop *warg = aopArg (ftype, walk2->argreg);
      wassert (warg);
      a_dead &= (warg->regs[A_IDX] < 0);
      hl_dead &= (warg->regs[L_IDX] < 0 && warg->regs[H_IDX] < 0);
      iy_dead &= (warg->regs[IYL_IDX] < 0 && warg->regs[IYH_IDX] < 0);
    }

  genMove (argreg, IC_LEFT (ic)->aop, a_dead, hl_dead, iy_dead);
  
  for (int i = 0; i < IC_LEFT (ic)->aop->size; i++)
    if (!regalloc_dry_run)
      s1c88_regs_used_as_parms_in_calls_from_current_function[argreg->aopu.aop_reg[i]->rIdx] = true;

  freeAsmop (IC_LEFT (ic), NULL);
}

static void
genCall (const iCode *ic)
{
  sym_link *dtype = operandType (IC_LEFT (ic));
  sym_link *etype = getSpec (dtype);
  sym_link *ftype = IS_FUNCPTR (dtype) ? dtype->next : dtype;
  int i;
  int prestackadjust = 0;
  bool tailjump = false;

  for (i = 0; i < IYH_IDX + 1; i++)
    s1c88_regs_preserved_in_calls_from_current_function[i] |= ftype->funcAttrs.preserved_regs[i];

  _saveRegsForCall (ic, false, false);

  aopOp (IC_LEFT (ic), ic, false, false);

  const bool bigreturn = (getSize (ftype->next) > 4) || IS_STRUCT (ftype->next); // Return value of big type or returning struct or union.
  const bool SomethingReturned = IS_ITEMP (IC_RESULT (ic)) && (OP_SYMBOL (IC_RESULT (ic))->nRegs || OP_SYMBOL (IC_RESULT (ic))->spildir) ||
                       IS_TRUE_SYMOP (IC_RESULT (ic));

  bool a_not_parm = !s1c88IsParmInCall(ftype, "a");
  bool a_free = a_not_parm && ic->left->aop->regs[A_IDX] < 0;
  bool hl_not_parm = !s1c88IsParmInCall(ftype, "l") && !s1c88IsParmInCall(ftype, "h");
  bool hl_free = hl_not_parm && ic->left->aop->regs[L_IDX] < 0 && ic->left->aop->regs[H_IDX] < 0;

  
  if (SomethingReturned && !bigreturn)
    aopOp (IC_RESULT (ic), ic, true, false);

  if (bigreturn)
    {
      PAIR_ID pair;
      int fp_offset, sp_offset;

      /* S1C88: prefer IY for the hidden-buffer address whenever the argument
         ABI leaves it free — for direct calls too, not only PCALL.  The HL
         fallback stages through BA, and with sdcccall(1) the first two 16-bit
         args occupy BA AND HL, so a struct-return call with two int args has
         no scratch pair on that path (it used to hit the wassert below).  IY
         is caller-clobbered (it is an argument register, s19), so it is fair
         game after _saveRegsForCall. */
      pair = (!IY_RESERVED && !aopArgsUseIY (ftype)) ? PAIR_IY : PAIR_HL;
      if (!hl_free && pair == PAIR_HL)
        _push (PAIR_HL);
      aopOp (IC_RESULT (ic), ic, true, false);
      wassert (IC_RESULT (ic)->aop->type == AOP_STK || IC_RESULT (ic)->aop->type == AOP_EXSTK);
      fp_offset =
        IC_RESULT (ic)->aop->aopu.aop_stk + (IC_RESULT (ic)->aop->aopu.aop_stk >
            0 ? _G.stack.param_offset : 0);
      sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;
      {
          emit2 ("ld %s, !immedword", _pairs[pair].name, (unsigned)sp_offset);
          if (pair == PAIR_IY)
            cost2 (4, 14);
          else
            cost2 (3, 10);
          emit2 ("add %s, sp", _pairs[pair].name);
          if (pair == PAIR_IY)
            cost2 (2, 15);
          else
            cost2 (1, 11);
        }
      if (!hl_free && pair == PAIR_HL)
        {
          /* S1C88: stage the buffer address in BA while restoring HL
            . */
          wassert (isPairDead (PAIR_BA, ic));
          emit2 ("ld ba, hl");
          cost2 (2, 0);
          _pop (PAIR_HL);
          pair = PAIR_BA;
        }
      emit2 ("push %s", _pairs[pair].name);
      if (pair == PAIR_IY)
        cost2 (2, 15);
      else
        cost2 (1, 11);
      if (!regalloc_dry_run)
        _G.stack.pushed += 2;
      freeAsmop (IC_RESULT (ic), 0);
      hl_free = false;
    }

  // Check if we can do tail call optimization.
  else if (currFunc && !IFFUNC_ISISR (currFunc->type) &&
    !ic->parmBytes &&
    !_G.stack.pushedHL && !_G.stack.pushedBC && !_G.stack.pushedIY && // If for some reason something got pushed, we don't have the return address in place.
    (!isFuncCalleeStackCleanup (currFunc->type) || !ic->parmEscapeAlive && ic->op == CALL && 0 /* todo: test and enable depending on optimization goal (trades code size for RAM) */) &&
    !ic->localEscapeAlive &&
    !IFFUNC_ISBANKEDCALL (dtype) && !IFFUNC_ISZ88DK_SHORTCALL (ftype) &&
    (_G.omitFramePtr))
    {
      int limit = 16; // Avoid endless loops in the code putting us into an endless loop here.

      if (isFuncCalleeStackCleanup (currFunc->type))
        {
           const bool caller_bigreturn = currFunc->type->next && (getSize (currFunc->type->next) > 4) || IS_STRUCT (currFunc->type->next);
           int caller_stackparmbytes = caller_bigreturn * 2;
           for (value *caller_arg = FUNC_ARGS(currFunc->type); caller_arg; caller_arg = caller_arg->next)
             {
               wassert (caller_arg->sym);
               if (!SPEC_REGPARM (caller_arg->etype))
                 caller_stackparmbytes += getSize (caller_arg->sym->type);
             }
           prestackadjust += caller_stackparmbytes;
        }

      for (const iCode *nic = ic->next; nic && --limit;)
        {
          const symbol *targetlabel = 0;

          if (nic->op == LABEL)
            ;
          else if (nic->op == GOTO) // We dont have ebbi here, so we cant just use eBBWithEntryLabel (ebbi, ic->label). Search manually.
            targetlabel = IC_LABEL (nic);
          else if (nic->op == RETURN && (!IC_LEFT (nic) || SomethingReturned && IC_RESULT (ic)->key == IC_LEFT (nic)->key))
            targetlabel = returnLabel;
          else if (nic->op == ENDFUNCTION)
            {
              if (OP_SYMBOL (IC_LEFT (nic))->stack <= (ic->op == PCALL ? 1 : (optimize.codeSize ? 1 : 2)))
                {
                  prestackadjust = OP_SYMBOL (IC_LEFT (nic))->stack;
                  tailjump = true;
                  break;
                }
              prestackadjust = 0;
              break;
            }
          else
            {
              prestackadjust = 0;
              break;
            }

          if (targetlabel)
            {
              const iCode *nnic = 0;
              for (nnic = nic->next; nnic; nnic = nnic->next)
                if (nnic->op == LABEL && IC_LABEL (nnic)->key == targetlabel->key)
                  break;
              if (!nnic)
                for (nnic = nic->prev; nnic; nnic = nnic->prev)
                  if (nnic->op == LABEL && IC_LABEL (nnic)->key == targetlabel->key)
                    break;
              if (!nnic)
                {
                  prestackadjust = 0;
                  tailjump = false;
                  break;
                }

              nic = nnic;
            }
          else
            nic = nic->next;
        }
    }

  if (tailjump && SomethingReturned) // Explicitly check for matching registers, as otherwise calls between __sdcccall(1) and __z88dk_fastcall will go wrong.
    for (int i = 0; i < IC_RESULT (ic)->aop->size; i++)
      if (!aopInReg (aopRet (currFunc->type), 0, aopRet (ftype)->aopu.aop_reg[0]->rIdx))
        tailjump = false;

  /* An indirect tail-jump needs HL for `jp hl` (the only register-indirect
     transfer); with an HL argument the target must go through the call
     form instead, so don't convert. */
  if (tailjump && ic->op == PCALL &&
      (s1c88IsParmInCall (ftype, "l") || s1c88IsParmInCall (ftype, "h")))
    tailjump = false;

  const bool jump = tailjump || !ic->parmBytes && !bigreturn && ic->op != PCALL && !IFFUNC_ISBANKEDCALL (dtype) && !IFFUNC_ISZ88DK_SHORTCALL(ftype) && IFFUNC_ISNORETURN (ftype);

  if (ic->op == PCALL)
    {
      if (IFFUNC_ISBANKEDCALL (dtype))
        {
          werror (W_INDIR_BANKED);
        }
      else if (IFFUNC_ISZ88DK_SHORTCALL (ftype))
       {
          wassertl(0, "__z88dk_short_call via function pointer not implemented");
       }

      /* Function pointers are 3 bytes: (lo, hi, bank) — code symbols link
         as (bank<<16)|logic, so byte 2 of &f IS the target's bank. The
         dispatch loads NB and lets the branch's CB<-NB latch switch banks;
         the 3-byte MAXIMUM-mode frame restores the caller's bank on RET.
         NB-window discipline: at most ONE instruction between `ld nb, a`
         and the branch that consumes it (the linker's own `ld nb ; nop ;
         carl` shape — the hardware's post-NB-write interrupt blackout
         covers exactly this window). */
      const bool a_parm = s1c88IsParmInCall (ftype, "a");
      const bool hl_parm = s1c88IsParmInCall (ftype, "l") || s1c88IsParmInCall (ftype, "h");

      if (jump)
        {
          /* tail-jump: `ld nb, <bank>` + `jp hl` switches to the target's
             bank and reuses the caller's own CB:PC frame — the eventual
             RET still restores the original caller's bank. HL is free here
             (the tailjump conversion excluded HL-argument targets); a live
             A argument is parked around the bank load. */
          spillPair (PAIR_HL);
          adjustStack (prestackadjust, !a_parm, false, false);
          if (a_parm)
            {
              emit2 ("push a");
              cost (1, 3);
              _G.stack.pushed += 1;
            }
          genMove (ASMOP_HLA, IC_LEFT (ic)->aop, true, true, isPairDead (PAIR_IY, ic));
          emit2 ("ld nb, a");
          cost (2, 2);
          if (a_parm)
            {
              emit2 ("pop a");          /* the one in-window instruction */
              cost (1, 3);
              _G.stack.pushed -= 1;
            }
          emit2 ("!jphl");
          cost2 (1, 4);
        }
      else
        {
          /* Indirect CALL: stage offset into the __sdcc_fptr scratch cell
             (2 bytes of near RAM, provided by the runtime like the
             __div/__mul support routines), bank into NB, then the native
             indirect `call (hhll)` — it pushes the full 3-byte CB:PC
             return frame and the callee's RET restores it. The transport
             leaves every argument register untouched: HLA directly when
             free, IY+A when HL carries arguments, or parks around the
             stack as the universal fallback. Not reentrant against an ISR
             that itself makes an indirect call between the store and the
             call — documented. */
          adjustStack (prestackadjust, !a_parm, !hl_parm, false);
          emit2 (".globl __sdcc_fptr");

          if (hl_parm && !IY_RESERVED && !aopArgsUseIY (ftype))
            {
              /* offset through IY, bank through (parked) A */
              spillPair (PAIR_IY);
              genMove_o (ASMOP_IY, 0, IC_LEFT (ic)->aop, 0, 2, !a_parm, false, true, true);
              emit2 ("ld (__sdcc_fptr), iy");
              cost (3, 5);
              if (a_parm)
                {
                  emit2 ("push a");
                  cost (1, 3);
                  _G.stack.pushed += 1;
                }
              cheapMove (ASMOP_A, 0, IC_LEFT (ic)->aop, 2, true);
              emit2 ("ld nb, a");
              cost (2, 2);
              if (a_parm)
                {
                  emit2 ("pop a");      /* the one in-window instruction */
                  cost (1, 3);
                  _G.stack.pushed -= 1;
                }
            }
          else
            {
              /* through HLA; park live A/HL arguments around it (pop hl
                 lands before the bank load, pop a is the one in-window
                 instruction) */
              if (a_parm)
                {
                  emit2 ("push a");
                  cost (1, 3);
                  _G.stack.pushed += 1;
                }
              if (hl_parm)
                _push (PAIR_HL);
              spillPair (PAIR_HL);
              genMove (ASMOP_HLA, IC_LEFT (ic)->aop, true, true, isPairDead (PAIR_IY, ic));
              emit2 ("ld (__sdcc_fptr), hl");
              cost (3, 5);
              if (hl_parm)
                _pop (PAIR_HL);
              emit2 ("ld nb, a");
              cost (2, 2);
              if (a_parm)
                {
                  emit2 ("pop a");      /* the one in-window instruction */
                  cost (1, 3);
                  _G.stack.pushed -= 1;
                }
            }

          emit2 ("call (__sdcc_fptr)");
          cost (3, 9);
        }
    }
  else
    {
      /* make the call */
      if (IFFUNC_ISBANKEDCALL (dtype))
        {
          wassert (!prestackadjust);

          char *name = OP_SYMBOL (IC_LEFT (ic))->rname[0] ? OP_SYMBOL (IC_LEFT (ic))->rname : OP_SYMBOL (IC_LEFT (ic))->name;
          /* there 3 types of banked call:
               legacy - only if --legacy-banking is specified
               a:bc - only for __z88dk_fastcall __banked functions
               e:hl - default (may have optimal bank switch routine) */
          if (s1c88_opts.legacyBanking)
            {
              emit2 ("call ___sdcc_bcall");
              emit2 ("!dws", name);
              emit2 ("!dw !bankimmeds", name);
              regalloc_dry_run_cost += 7;
            }
          else if (IFFUNC_ISZ88DK_FASTCALL (ftype))
            {
              emit2 ("ld a, !hashedbankimmeds", name);
              emit2 ("ld bc, !hashedstr", name);
              emit2 ("call ___sdcc_bcall_abc");
              regalloc_dry_run_cost += 8;
            }
          else
            {
              spillPair (PAIR_HL);
              emit2 ("ld e, !hashedbankimmeds", name);
              emit2 ("ld hl, !hashedstr", name);
              emit2 ("call ___sdcc_bcall_ehl");
              regalloc_dry_run_cost += 8;
            }
        }
      else
        {
          if (currFunc && isFuncCalleeStackCleanup (currFunc->type) && prestackadjust && !IFFUNC_ISNORETURN (ftype)) // Copy return value into correct location on stack for tail call optimization.
            {
              wassert (0);
              /* todo: implement */
            }

          adjustStack (prestackadjust, false, hl_free, false);

          if (IS_LITERAL (etype))
            {
              emit2 (jump ? "jp !constword" : "call !constword", ulFromVal (OP_VALUE (IC_LEFT (ic))));
              if (jump)
                cost2 (3, 10);
              else
                cost2 (3, 17);
            }
          else if (IFFUNC_ISZ88DK_SHORTCALL(ftype))
            {
              int rst = ftype->funcAttrs.z88dk_shortcall_rst;
              int value = ftype->funcAttrs.z88dk_shortcall_val;
              emit2 ("rst !constbyte", (unsigned)rst);
              cost2 (1, 11);
              if (value < 256)
                emit2 ("defb !constbyte\n", (unsigned)value);
              else
                emit2 ("defw !constword\n", (unsigned)value);
              regalloc_dry_run_cost_bytes += 2 + (value >= 256);
            }
          else
            {
              /* Compiler-support routines (__mulint, __divsint, …) are created
                 by funcOfType() with cdef=1 and never enter SDCC's publics/
                 externs sets, so no `.globl` is emitted and the assembler then
                 rejects the (undefined) reference. Register them as global so
                 printPublics emits the needed `.globl`. (File-scope user C
                 externs already reach publics via the normal symbol-table path
                 — SDCCglue adds a used level-0 function — but a BLOCK-scope
                 `extern void f(void);` lives in an inner scope table the glue
                 never walks, so it needs the same registration. level > 0
                 can't duplicate the glue's entry, which is level-0-only.) */
              {
                symbol *csym = OP_SYMBOL (IC_LEFT (ic));
                if ((csym->cdef || csym->level > 0) && !IS_STATIC (csym->etype))
                  addSetIfnotP (&publics, csym);
              }

              /* Inter-function transfer: emit the banked pseudo-ops so the
                 LINKER picks the short/long branch form and inserts/omits the
                 `ld nb` bank switch (see docs/s1c88/banked-branch.md). Worst-
                 case slot is 6 bytes (ld nb,#bb + long branch); unused bytes
                 become nop. */
              emit2 ("%s %s", jump ? "bjump" : "bcall",
                (OP_SYMBOL (IC_LEFT (ic))->rname[0] ? OP_SYMBOL (IC_LEFT (ic))->rname : OP_SYMBOL (IC_LEFT (ic))->name));
              if (jump)
                cost2 (6, 13);
              else
                cost2 (6, 20);
            }
        }
    }
  spillCached ();

  freeAsmop (IC_LEFT (ic), 0);

  _G.stack.pushed += prestackadjust;

  /* Mark the registers as restored. */
  _G.saves.saved = false;

  /* adjust the stack for parameters if required */
  if ((ic->parmBytes || bigreturn) && (IFFUNC_ISNORETURN (ftype) || isFuncCalleeStackCleanup (ftype)))
    {
      if (!regalloc_dry_run)
        {
          _G.stack.pushed -= (ic->parmBytes + bigreturn * 2);
          s1c88_symmParm_in_calls_from_current_function = false;
        }
    }
  else if ((ic->parmBytes || bigreturn))
    {
      bool return_in_reg = SomethingReturned && !bigreturn;
      adjustStack (ic->parmBytes + bigreturn * 2,
        !return_in_reg || !aopRet (ftype) || aopRet (ftype)->regs[A_IDX] < 0 || aopRet (ftype)->regs[A_IDX] > IC_RESULT (ic)->aop->size,
        !return_in_reg || !aopRet (ftype) || (aopRet (ftype)->regs[L_IDX] < 0 || aopRet (ftype)->regs[L_IDX] > IC_RESULT (ic)->aop->size) && (aopRet (ftype)->regs[H_IDX] < 0 || aopRet (ftype)->regs[H_IDX] > IC_RESULT (ic)->aop->size),
        !IY_RESERVED);

      if (regalloc_dry_run)
        _G.stack.pushed += ic->parmBytes + bigreturn * 2;
    }

  /* if we need assign a result value */
  if (SomethingReturned && !bigreturn)
    {
      genMove (IC_RESULT (ic)->aop, aopRet (ftype), true, true, true);

      freeAsmop (IC_RESULT (ic), 0);
    }

  spillCached ();

  restoreRegs (_G.stack.pushedIY, _G.stack.pushedBC, _G.stack.pushedHL, IC_RESULT (ic), ic);
  _G.stack.pushedIY = FALSE;
  _G.stack.pushedBC = FALSE;
  _G.stack.pushedHL = FALSE;
}

/*-----------------------------------------------------------------*/
/* resultRemat - result  is rematerializable                       */
/*-----------------------------------------------------------------*/
static int
resultRemat (const iCode * ic)
{
  if (SKIP_IC (ic) || ic->op == IFX)
    return 0;

  if (IC_RESULT (ic) && IS_ITEMP (IC_RESULT (ic)))
    {
      const symbol *sym = OP_SYMBOL_CONST (IC_RESULT (ic));
      if (sym->remat && !POINTER_SET (ic) && sym->isspilt)
        return 1;
    }

  return 0;
}

/*-----------------------------------------------------------------*/
/* genFunction - generated code for function entry                 */
/*-----------------------------------------------------------------*/
static void
genFunction (const iCode * ic)
{
  bool stackParm;

  symbol *sym = OP_SYMBOL (IC_LEFT (ic));
  sym_link *ftype;

  bool bcInUse = FALSE;
  bool bigreturn;

  setArea (IFFUNC_NONBANKED (sym->type));
  _G.stack.pushed = 0;

  /* PENDING: Reset the receive offset as it
     doesn't seem to get reset anywhere else.
   */
  _G.receiveOffset = 0;
  _G.stack.param_offset = sym->type->funcAttrs.z88dk_params_offset;

  /* Record the last function name for debugging. */
  _G.lastFunctionName = sym->rname;

  /* Create the function header */
  emit2 ("!functionheader", sym->name);

  emitDebug (s1c88_assignment_optimal ? "; Register assignment is optimal." : "; Register assignment might be sub-optimal.");
  emitDebug ("; Stack space usage: %d bytes.", sym->stack);

  if (IFFUNC_BANKED (sym->type))
    {
      int bank_number = 0;
      for (int i  = strlen (options.code_seg)-1; i >= 0; i--)
        {
          if (!isdigit (options.code_seg[i]) && options.code_seg[i+1] != '\0')
            {
              bank_number = atoi (&options.code_seg[i+1]);
              break;
            }
        }
      emit2("!bequ", sym->rname, bank_number);
    }

  if (IS_STATIC (sym->etype))
    emit2 ("!functionlabeldef", sym->rname);
  else
    emit2 ("!globalfunctionlabeldef", sym->rname);

  if (!regalloc_dry_run)
    genLine.lineCurr->isLabel = 1;

  /* S1C88 __interrupt(n) auto-wiring: an ISR declared with an explicit cartridge
     IRQ slot number N (`void f(void) __interrupt(N)`) also defines the global
     label _irq_v<N> at its entry — the symbol the crt0 header trampoline
     (`bjump _irq_v<N>`) resolves to.  So the handler installs itself in vector
     slot N with no hand-naming of irq_v<N>.  N is the CARTRIDGE IRQ slot
     (1..26, the <pm.h> VEC_* values, per the PM BIOS forwarding table), NOT the
     raw hardware IRQ number; slot 0 is the reset vector. */
  if (IFFUNC_ISISR (sym->type) && FUNC_INTNO (sym->type) != INTNO_UNSPEC)
    {
      int vec = FUNC_INTNO (sym->type);
      if (vec < 1 || vec > 26)
        werror (W_CONST_RANGE, "in __interrupt(n): n must be a cartridge IRQ slot 1..26 (see <pm.h> VEC_*)");
      else
        {
          char vname[16];
          SNPRINTF (vname, sizeof (vname), "_irq_v%d", vec);
          emit2 ("!globalfunctionlabeldef", vname);
          if (!regalloc_dry_run)
            genLine.lineCurr->isLabel = 1;
        }
    }

  ftype = operandType (IC_LEFT (ic));

  if (IFFUNC_ISNAKED (ftype))
    {
      emitDebug ("; naked function: no prologue.");
      return;
    }

  /* if this is an interrupt service routine
     then save all potentially used registers. */
  if (IFFUNC_ISISR (sym->type))
    {
      /* S1C88: the interrupt sequence auto-saves CB:PC and SC (the flags +
         interrupt-priority mask) and RETE restores them, so the handler only
         saves the GP registers it may clobber — A,B (BA), L,H (HL), IY; IX is
         saved by the frame setup below.  There is no ei/di: interrupts of the
         same or lower priority are masked until RETE (re-set the SC priority
         level inside the routine to allow same-level nesting). */
      emit2 ("push ba");
      cost2 (1, 11);
      emit2 ("push hl");
      cost2 (1, 11);
      emit2 ("push iy");
      cost2 (2, 15);
      /* EP hygiene (__far, abi-decision.md #9): the interrupt may have hit
         inside a far-access window (EP != 0), and the handler's near codegen
         relies on the EP=0 invariant — EP is NOT in the hardware save set
         (only CB:PC and SC are), so save it and zero it; the epilogue
         restores it before RETE.  (A is free here: BA was just saved.) */
      emit2 ("ld a, ep");
      cost (2, 2);
      emit2 ("push a");
      cost (1, 3);
      emit2 ("ld ep, #0x00");
      cost (3, 3);
    }
  else
    {
      /* This is a non-ISR function.
         If critical function then turn interrupts off */
      if (IFFUNC_ISCRITICAL (sym->type))
        {
          /* S1C88 has no ei/di: mask all maskable interrupts by raising the SC
             interrupt-priority level to 3 (I1:I0 = bits 7:6).  Save the prior
             SC (level + flags) so the epilogue restores it — this handles the
             prior level and nesting.  `push sc` is 1 byte (param_offset += 1). */
          emit2 ("push sc");
          cost2 (1, 11);
          emit2 ("or sc, !immedbyte", 0xc0u);
          cost2 (2, 7);
          _G.stack.param_offset += 1;
        }
    }

  if (s1c88_opts.calleeSavesBC)
    {
      bcInUse = TRUE;
    }

  /* Detect which registers are used. */
  if (IFFUNC_CALLEESAVES (sym->type) && sym->regsUsed)
    {
      int i;
      for (i = 0; i < sym->regsUsed->size; i++)
        {
          if (bitVectBitValue (sym->regsUsed, i))
            {
              switch (i)
                {
                case B_IDX:
                  bcInUse = TRUE;
                  break;
                }
            }
        }
    }

  if (bcInUse)
    {
      /* S1C88: only the B byte exists — 1-byte callee-save slot */
      emit2 ("push b");
      _G.stack.param_offset += 1;
    }

  _G.calleeSaves.pushedBC = bcInUse;

  /* adjust the stack for the function */
//  _G.stack.last = sym->stack;

  bigreturn = (getSize (ftype->next) > 4) || IS_STRUCT (ftype->next);
  _G.stack.param_offset += bigreturn * 2;

  stackParm = FALSE;
  for (sym = setFirstItem (istack->syms); sym; sym = setNextItem (istack->syms))
    {
      if (sym->_isparm && !IS_REGPARM (sym->etype))
        {
          stackParm = TRUE;
          break;
        }
    }
  sym = OP_SYMBOL (IC_LEFT (ic));

  _G.omitFramePtr = s1c88_should_omit_frame_ptr;

  if (!s1c88_opts.noOmitFramePtr && !stackParm && !sym->stack)
    {
      if (!regalloc_dry_run)
        _G.omitFramePtr = true;
    }
  else if (sym->stack)
    {
      {
          if (!_G.omitFramePtr)
            emit2 ((optimize.codeSize && !s1c88IsParmInCall (sym->type, "l") && !s1c88IsParmInCall (sym->type, "h")) ? "!enters" : "!enter");
          adjustStack (-sym->stack, !s1c88IsParmInCall (sym->type, "a"), !s1c88IsParmInCall (sym->type, "l") && !s1c88IsParmInCall (sym->type, "h"), !IY_RESERVED);
        }
      _G.stack.pushed = 0;
    }
  else if (!_G.omitFramePtr)
    {
      emit2 ((optimize.codeSize && !s1c88IsParmInCall (sym->type, "l") && !s1c88IsParmInCall (sym->type, "h")) ? "!enters" : "!enter"); // !enters might result in a function call to a helper function.
    }

  _G.stack.offset = sym->stack;
  
  for (PAIR_ID pairId = 0; pairId < NUM_PAIRS; pairId++)
    spillPair (pairId);
}

/*-----------------------------------------------------------------*/
/* genEndFunction - generates epilogue for functions               */
/*-----------------------------------------------------------------*/
static void
genEndFunction (iCode *ic)
{
  symbol *sym = OP_SYMBOL (IC_LEFT (ic));
  /* __critical __interrupt without an interrupt number is the non-maskable interrupt */
  bool is_nmi = IFFUNC_ISCRITICAL (sym->type) && FUNC_INTNO (sym->type) == INTNO_UNSPEC;
  bool hl_free = !aopRet (sym->type) || aopRet (sym->type)->regs[L_IDX] < 0 && aopRet (sym->type)->regs[H_IDX] < 0;
  bool iy_free = !IY_RESERVED && (!aopRet (sym->type) || aopRet (sym->type)->regs[IYL_IDX] < 0 && aopRet (sym->type)->regs[IYH_IDX] < 0);

  wassert (!regalloc_dry_run);
  wassertl (!_G.stack.pushed, "Unbalanced stack.");

  if (IFFUNC_ISNAKED (sym->type) || IFFUNC_ISNORETURN (sym->type))
    {
      emitDebug (IFFUNC_ISNAKED (sym->type) ? "; naked function: No epilogue." : "; _Noreturn function: No epilogue.");
      return;
    }

  if (!regalloc_dry_run && IFFUNC_ISZ88DK_CALLEE (sym->type) && FUNC_HASVARARGS (sym->type))
    werror (E_Z88DK_CALLEE_VARARG); // We have no idea how many bytes on the stack we'd have to clean up.

  const bool bigreturn = (getSize (sym->type->next) > 4) || IS_STRUCT (sym->type->next);
  int stackparmbytes = bigreturn * 2;
  for (value *arg = FUNC_ARGS(sym->type); arg; arg = arg->next)
    {
      wassert (arg->sym);
      int argsize = getSize (arg->sym->type);
      if (argsize == 1 && FUNC_ISSMALLC (sym->type)) // SmallC calling convention passes 8-bit stack arguments as 16 bit.
        argsize++;
      if (!SPEC_REGPARM (arg->etype))
        stackparmbytes += argsize;
    }

  int poststackadjust = isFuncCalleeStackCleanup (sym->type) ? stackparmbytes : 0;

  if (!_G.omitFramePtr && sym->stack > (optimize.codeSize ? 2 : 1))
    {
      emit2 ("ld sp, ix");
      cost2 (2, 10);
    }
  else
    adjustStack (_G.stack.offset,
      !aopRet (sym->type)  || aopRet (sym->type)->regs[A_IDX] < 0,
      hl_free,
      iy_free);

  if(!_G.omitFramePtr)
    {
      emit2 ("pop ix");
      cost2 (2, 14);
    }

  wassertl(regalloc_dry_run || !(isFuncCalleeStackCleanup (sym->type) && _G.calleeSaves.pushedBC), "Unimplemented __z88dk_callee support for callee-saved b on callee side");
  if (_G.calleeSaves.pushedBC)
    {
      emit2 ("pop b");
      cost2 (1, 10);
      _G.calleeSaves.pushedBC = FALSE;
    }

  /* if this is an interrupt service routine
     then restore all potentially used registers. */
  if (IFFUNC_ISISR (sym->type))
    {
      /* Restore the GP registers saved by the prologue, in reverse order
         (RETE will restore SC/flags + the interrupt mask).  EP first — the
         interrupted code may have been inside a far-access window. */
      emit2 ("pop a");
      cost (1, 3);
      emit2 ("ld ep, a");
      cost (2, 2);
      emit2 ("pop iy");
      cost2 (2, 12);
      emit2 ("pop hl");
      cost2 (1, 10);
      emit2 ("pop ba");
      cost2 (1, 10);
    }
  else
    {
      /* This is a non-ISR function.
         If critical function then turn interrupts back on */
      if (IFFUNC_ISCRITICAL (sym->type))
        {
          /* Restore the SC (flags + interrupt-priority mask) saved by the
             prologue — `pop sc` doesn't touch A/HL, so the return value is
             safe and no flag/return-value shuffle is needed. */
          emit2 ("pop sc");
          cost2 (1, 10);
        }
    }

  if (poststackadjust)
    {
      wassertl(regalloc_dry_run || !IFFUNC_ISBANKEDCALL (sym->type), "Unimplemented __banked __z88dk_callee support on callee side");
      wassertl(regalloc_dry_run || !IFFUNC_HASVARARGS (sym->type), "__z88dk_callee function may to have variable arguments");
      wassertl(regalloc_dry_run || !IFFUNC_ISISR (sym->type), "__z88dk_callee makes no sense on an ISR");

      /* __z88dk_callee, MAXIMUM mode: the return frame is 3 bytes
         (PCL PCH CB) and only RET can consume it (no jp can restore CB),
         so pop-and-jp epilogue tricks are gone. Move the whole frame up
         over the parameter area byte-by-byte — top byte (CB) first, since
         the regions overlap when poststackadjust < 3 — then drop SP onto
         the moved frame and fall through to the plain ret. A and HL are
         the working registers, saved when they carry return-value bytes
         (the saves sit below the frame; `base` re-points the copy). */
      {
        const bool save_a_ret = aopRet (sym->type) && aopRet (sym->type)->regs[A_IDX] >= 0;
        const bool save_hl_ret = aopRet (sym->type) &&
          (aopRet (sym->type)->regs[L_IDX] >= 0 || aopRet (sym->type)->regs[H_IDX] >= 0);
        int base = 0;
        int k;

        if (save_hl_ret)
          {
            _push (PAIR_HL);
            base += 2;
          }
        if (save_a_ret)
          {
            emit2 ("push a");
            cost (1, 3);
            _G.stack.pushed += 1;
            base += 1;
          }
        for (k = 2; k >= 0; k--)
          {
            emit2 ("ld hl, !immed%d", base + k);
            cost2 (3, 10);
            emit2 ("add hl, sp");
            cost2 (1, 11);
            emit2 ("ld a, (hl)");
            cost2 (1, 7);
            emit2 ("add hl, !immed%d", poststackadjust);
            cost2 (3, 0);
            emit2 ("ld (hl), a");
            cost2 (1, 7);
          }
        if (save_a_ret)
          {
            emit2 ("pop a");
            cost (1, 3);
            _G.stack.pushed -= 1;
          }
        if (save_hl_ret)
          _pop (PAIR_HL);

        /* SP onto the moved frame (flags are dead in the epilogue) */
        adjustStack (poststackadjust, true, false, false);
      }
    }

  if (options.debug && currFunc)
    {
      debugFile->writeEndFunction (currFunc, ic, 1);
    }

  if (IFFUNC_ISISR (sym->type))
    {
      /* S1C88: every exception/interrupt routine returns via RETE, which pops
         the SC (flags + interrupt-priority mask) and CB:PC that the interrupt
         sequence pushed — restoring the mask automatically, so no `ei` is
         needed (critical or not), and NMI returns the same way. (No reti/retn:
         not present on the S1C88.) */
      (void) is_nmi;
      emit2 ("rete");
      cost2 (2, 14);
    }
  else
    {
      /* Both banked and non-banked just ret */
      emit2 ("ret");
      cost2 (1, 10);
    }

done:
  _G.flushStatics = 1;
  _G.stack.pushed = 0;
  _G.stack.offset = 0;

  emitDebug (";\tTotal %s function size at codegen: %u bytes.", sym->name, (unsigned int)regalloc_dry_run_cost);
}

/*-----------------------------------------------------------------*/
/* genRet - generate code for return statement                     */
/*-----------------------------------------------------------------*/
static void
genRet (const iCode *ic)
{
  /* Errk.  This is a hack until I can figure out how
     to cause dehl to spill on a call */
  int size, offset = 0;

  /* if we have no return value then
     just generate the "ret" */
  if (!IC_LEFT (ic))
    goto jumpret;

  /* we have something to return then
     move the return value into place */
  aopOp (IC_LEFT (ic), ic, FALSE, FALSE);
  size = IC_LEFT (ic)->aop->size;

  if (size <= 4 && !IS_STRUCT (operandType (IC_LEFT (ic))))
    {
 if (size > 0) // SDCC supports GCC extension of returning void
        {
          // A struct/union return-by-value (bigreturn) has aopRet()==NULL: the
          // result must be copied to the caller's hidden buffer, not moved to a
          // return register. `return *q` lowers to a small (pointer-sized) operand
          // here, so it slips past the size<=4 && !IS_STRUCT guard. Copying it is
          // not implemented yet (needs the bigreturn/ldir struct-copy work); flag
          // it cleanly instead of calling genMove with a NULL destination (SIGSEGV).
          if (aopRet (currFunc->type))
            genMove (aopRet (currFunc->type), IC_LEFT (ic)->aop, true, true, true);
          else
            {
              /* Struct/union return-by-value (bigreturn): aopRet()==NULL, and
                 `return *p` reaches genRet as a pointer-sized operand — the
                 address of the source struct.  Copy sizeof(return type) bytes
                 from [that pointer] (HL) to the caller's hidden return buffer
                 (IY, read from the hidden-pointer slot on the stack). */
              int structsize = getSize (currFunc->type->next);
              if (structsize < 1 || structsize > 255)
                UNIMPLEMENTED;   /* giant struct return — needs a 16-bit counter */
              else
                {
                  symbol *tlbl;
                  int off;
                  /* HL = source pointer FIRST — IC_LEFT is often the near-ptr
                     arg living in HL, so it must be read before HL is reused. */
                  genMove (ASMOP_HL, IC_LEFT (ic)->aop, true, true, true);
                  /* IY = hidden return-buffer pointer = *(sp+off).  Save the
                     source over the read (HL is the only pair that can deref
                     `(hl)`), so this works for any frame offset.  `off` is
                     computed after the push so it includes the saved word. */
                  _push (PAIR_HL);
                  off = _G.stack.offset + (_G.stack.param_offset - 2) + _G.stack.pushed + (_G.omitFramePtr ? 0 : 2) + 3 /* hidden return-ptr offset: locals/temps + param shifts MINUS the hidden-ptr's own 2-byte self-shift (added near genFunction for OTHER args) + saved IX (only if !omit) + the always-present 3-byte CB:PC max-mode return frame */;
                  setupPairFromSP (PAIR_HL, off);
                  emit2 ("ld iy, !*hl");
                  cost2 (2, 14);
                  _pop (PAIR_HL);
                  /* copy structsize bytes [HL] -> [IY] (A temp, B counter; both dead here) */
                  emit2 ("ld b, !immedbyte", (unsigned) structsize);
                  cost2 (2, 7);
                  tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
                  if (!regalloc_dry_run)
                    emitLabel (tlbl);
                  emit2 ("ld a, !*hl");
                  cost2 (1, 7);
                  emit2 ("ld !*iyx, a", 0);
                  cost2 (1, 7);
                  emit3w (A_INC, ASMOP_HL, 0);
                  emit3w (A_INC, ASMOP_IY, 0);
                  if (!regalloc_dry_run)
                    emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
                  regalloc_dry_run_cost += 2;
                  spillPair (PAIR_HL);
                  spillPair (PAIR_IY);
                }
            }
        }
    }
  else if (IC_LEFT (ic)->aop->type == AOP_LIT)
    {
      unsigned long long lit = ullFromVal (IC_LEFT (ic)->aop->aopu.aop_lit);
      setupPairFromSP (PAIR_HL, _G.stack.offset + (_G.stack.param_offset - 2) + _G.stack.pushed + (_G.omitFramePtr ? 0 : 2) + 3 /* hidden return-ptr offset: locals/temps + param shifts MINUS the hidden-ptr's own 2-byte self-shift (added near genFunction for OTHER args) + saved IX (only if !omit) + the always-present 3-byte CB:PC max-mode return frame */);
      emit2 ("!ldahli");
      regalloc_dry_run_cost += 6;
      emit2 ("ld h, !*hl");
      cost2 (1, 7);
      emit3 (A_LD, ASMOP_L, ASMOP_A);
      do
        {
          emit2 ("ld !*hl, !immedbyte", (unsigned)(lit & 0xffu));
          cost2 (2, 10);
          lit >>= 8;
          if (size > 1)
            emit3w (A_INC, ASMOP_HL, 0);
        }
      while (--size);
    }
  else if (IC_LEFT (ic)->aop->type == AOP_STK || IC_LEFT (ic)->aop->type == AOP_EXSTK || (IC_LEFT (ic)->aop->type == AOP_DIR || IC_LEFT (ic)->aop->type == AOP_IY))
    {
      setupPairFromSP (PAIR_HL, _G.stack.offset + (_G.stack.param_offset - 2) + _G.stack.pushed + (_G.omitFramePtr ? 0 : 2) + 3 /* hidden return-ptr offset: locals/temps + param shifts MINUS the hidden-ptr's own 2-byte self-shift (added near genFunction for OTHER args) + saved IX (only if !omit) + the always-present 3-byte CB:PC max-mode return frame */);
      /* IY = dest (the caller's hidden return-buffer pointer, 2 bytes via [HL]).
         S1C88 has no DE; the 16-bit `ld iy,(hl)` reads the pointer in one go. */
      emit2 ("ld iy, !*hl");
      cost2 (2, 14);
      if (IC_LEFT (ic)->aop->type == AOP_STK || IC_LEFT (ic)->aop->type == AOP_EXSTK)
        {
          int sp_offset, fp_offset;
          fp_offset =
            IC_LEFT (ic)->aop->aopu.aop_stk + (IC_LEFT (ic)->aop->aopu.aop_stk >
                0 ? _G.stack.param_offset : 0);
          sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;
          // TODO: find out if offset is okay
          emit2 ("!ldahlsp", sp_offset);
          spillPair (PAIR_HL);
          regalloc_dry_run_cost += 4;
        }
      else
        fetchLitPair (PAIR_HL, IC_LEFT (ic)->aop, 0, true, false);
      /* HL = source, IY = dest: copy `size` bytes with a native byte loop.
         At the epilogue A/B are dead, so B is a free loop counter and A the
         per-byte temp. (Struct returns are well under 255 bytes.) */
      {
        symbol *tlbl;
        emit2 ("ld b, !immedbyte", (unsigned) size);
        cost2 (2, 7);
        tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
        if (!regalloc_dry_run)
          emitLabel (tlbl);
        emit2 ("ld a, !*hl");
        cost2 (1, 7);
        emit2 ("ld !*iyx, a", 0);
        cost2 (1, 7);
        emit3w (A_INC, ASMOP_HL, 0);
        emit3w (A_INC, ASMOP_IY, 0);
        if (!regalloc_dry_run)
          emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
        regalloc_dry_run_cost += 2;
      }
      spillPair (PAIR_HL);
    }
  else
    {
      /* S1C88: read the caller's hidden buffer pointer in one 16-bit load
         and write through IY. */
      setupPairFromSP (PAIR_HL, _G.stack.offset + (_G.stack.param_offset - 2) + _G.stack.pushed + (_G.omitFramePtr ? 0 : 2) + 3 /* hidden return-ptr offset: locals/temps + param shifts MINUS the hidden-ptr's own 2-byte self-shift (added near genFunction for OTHER args) + saved IX (only if !omit) + the always-present 3-byte CB:PC max-mode return frame */);
      emit2 ("ld iy, !*hl");
      cost2 (2, 0);
      spillPair (PAIR_IY);
      do
        {
          cheapMove (ASMOP_A, 0, IC_LEFT (ic)->aop, offset++, true);
          emit2 ("ld !*iyx, a", 0);
          cost2 (1, 7);
          if (size > 1)
            emit3w (A_INC, ASMOP_IY, 0);
        }
      while (--size);
    }
  freeAsmop (IC_LEFT (ic), NULL);

jumpret:
  /* generate a jump to the return label
     if the next is not the return statement */
  if (!(ic->next && ic->next->op == LABEL && IC_LABEL (ic->next) == returnLabel))
    {
      if (!regalloc_dry_run)
        emit2 ("jp !tlabel", labelKey2num (returnLabel->key));
      cost2 (3, 10);
    }
}

/*-----------------------------------------------------------------*/
/* genLabel - generates a label                                    */
/*-----------------------------------------------------------------*/
static void
genLabel (const iCode * ic)
{
  /* special case never generate */
  if (IC_LABEL (ic) == entryLabel)
    return;

  emitLabelSpill (IC_LABEL (ic));
}

/*-----------------------------------------------------------------*/
/* genGoto - generates a ljmp                                      */
/*-----------------------------------------------------------------*/
static void
genGoto (const iCode * ic)
{
  emit2 ("jp !tlabel", labelKey2num (IC_LABEL (ic)->key));
}

/*-----------------------------------------------------------------*/
/* genPlusIncr :- does addition with increment if possible         */
/*-----------------------------------------------------------------*/
static bool
genPlusIncr (const iCode *ic)
{
  unsigned int icount;
  unsigned int size = IC_RESULT (ic)->aop->size;
  PAIR_ID resultId = getPairId (IC_RESULT (ic)->aop);

  /* will try to generate an increment */
  /* if the right side is not a literal
     we cannot */
  if (IC_RIGHT (ic)->aop->type != AOP_LIT)
    return FALSE;

  icount = (unsigned int) ulFromVal (IC_RIGHT (ic)->aop->aopu.aop_lit);

  /* If result is a pair */
  if (resultId != PAIR_INVALID)
    {
      bool delayed_move;
      if (isLitWord (IC_LEFT (ic)->aop))
        {
          fetchLitPair (getPairId (IC_RESULT (ic)->aop), IC_LEFT (ic)->aop, icount, true, false);
          return TRUE;
        }

      if (size == 2 && icount == 256 && ic->result->aop->type == AOP_REG && ic->left->aop->type == AOP_REG && ic->result->aop->aopu.aop_reg[0]->rIdx == ic->left->aop->aopu.aop_reg[0]->rIdx &&
        (HAS_IYL_INST || !aopInReg (ic->result->aop, 1, IYL_IDX) && !aopInReg (ic->result->aop, 1, IYH_IDX)))
        {
          emit3_o (A_INC, ic->result->aop, 1, 0, 0);
          return true;
        }

      if (icount == 255 && resultId != PAIR_IY)
        {
          fetchPair (resultId, IC_LEFT (ic)->aop);
          emit3w (A_DEC, ic->result->aop, 0);
          emit3_o (A_INC, ic->result->aop, 1, 0, 0);
          return true;
        }
      else if (icount == 257 && resultId != PAIR_IY)
        {
          fetchPair (resultId, IC_LEFT (ic)->aop);
          emit3w (A_INC, ic->result->aop, 0);
          emit3_o (A_INC, ic->result->aop, 1, 0, 0);
          return true;
        }
      

      if (isPair (IC_LEFT (ic)->aop) && getPairId (IC_LEFT (ic)->aop) != PAIR_IY && resultId == PAIR_HL && icount > 3)
        {
          if (getPairId (IC_LEFT (ic)->aop) == PAIR_HL)
            {
              PAIR_ID freep = getDeadPairId (ic);
              if (freep != PAIR_INVALID)
                {
                  fetchPair (freep, IC_RIGHT (ic)->aop);
                  emit2 ("add hl, %s", _pairs[freep].name);
                  cost2 (1, 11);
                  return TRUE;
                }
            }
          else
            {
              fetchPair (PAIR_HL, IC_RIGHT (ic)->aop);
              emit3w (A_ADD, ASMOP_HL, ic->left->aop);
              return true;
            }
        }
      if (icount > 5)
        return FALSE;
      /* Inc a pair */
      delayed_move = (getPairId (IC_RESULT (ic)->aop) == PAIR_IY && getPairId (IC_LEFT (ic)->aop) != PAIR_INVALID
                      && isPairDead (getPairId (IC_LEFT (ic)->aop), ic));
      if (!sameRegs (IC_LEFT (ic)->aop, IC_RESULT (ic)->aop))
        {
          if (icount > 3)
            return FALSE;
          if (!delayed_move)
            genMove (IC_RESULT (ic)->aop, IC_LEFT (ic)->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
        }
      while (icount--)
        {
          PAIR_ID pair = delayed_move ? getPairId (IC_LEFT (ic)->aop) : getPairId (IC_RESULT (ic)->aop);
          emit2 ("inc %s", _pairs[pair].name);
          if (pair == PAIR_IY)
            cost2 (2, 10);
          else
            cost2 (1, 6);
        }
      if (delayed_move)
        fetchPair (getPairId (IC_RESULT (ic)->aop), IC_LEFT (ic)->aop);
      return true;
    }

  if (isLitWord (IC_LEFT (ic)->aop) && size == 2 && isPairDead (PAIR_HL, ic))
    {
      fetchLitPair (PAIR_HL, IC_LEFT (ic)->aop, icount, true, false);
      genMove (IC_RESULT (ic)->aop, ASMOP_HL, isRegDead (A_IDX, ic), true, true);
      return true;
    }

  if (icount > 4) // Not worth it if the sequence of inc gets too long.
    return false;

  if (icount > 1 && size == 1 && aopInReg (IC_LEFT (ic)->aop, 0, A_IDX)) // add a, #n is cheaper than sequence of inc a.
    return false;

  if (size == 2 && getPairId (IC_LEFT (ic)->aop) != PAIR_INVALID && icount <= 3 && isPairDead (getPairId (IC_LEFT (ic)->aop), ic))
    {
      while (icount--)
        emit3w (A_INC, ic->left->aop, 0);
      genMove (IC_RESULT (ic)->aop, IC_LEFT (ic)->aop, isRegDead (A_IDX, ic), isPairDead(PAIR_HL, ic), true);
      return true;
    }

  if (size == 2 && icount <= 2 && isPairDead (PAIR_HL, ic) && (IC_LEFT (ic)->aop->type == AOP_HL || IC_LEFT (ic)->aop->type == AOP_IY))
    {
      genMove (ASMOP_HL, IC_LEFT (ic)->aop, isRegDead (A_IDX, ic), true, true);
      while (icount--)
        emit3w (A_INC, ASMOP_HL, 0);
      genMove (IC_RESULT (ic)->aop, ASMOP_HL, isRegDead (A_IDX, ic), true, true);
      return true;
    }

  /* if increment 16 bits in register */
  if (sameRegs (IC_LEFT (ic)->aop, IC_RESULT (ic)->aop) && size > 1 && icount == 1
    && (HAS_IYL_INST || size == 2 && getPairId (IC_RESULT (ic)->aop) != PAIR_INVALID || size >= 2 && !aopInReg (IC_RESULT (ic)->aop, 0, IYL_IDX) && !aopInReg (IC_RESULT (ic)->aop, 0, IYH_IDX) && !aopInReg (IC_RESULT (ic)->aop, 1, IYL_IDX) && !aopInReg (IC_RESULT (ic)->aop, 1, IYH_IDX)))
    {
      int offset = 0;
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
      while (size--)
        {
          if (offset)
            regalloc_dry_run_state_scale /= 256.0f; // Cycle cost contribution of upper byte additions is negligible
          if (aopIsLitVal (ic->result->aop, offset, size + 1, 0)) // Skip known leading zero result bytes.
            {
              offset += size;
              size = 0;
              break;
            }
          if (size == 1 && getPairId_o (IC_RESULT (ic)->aop, offset) != PAIR_INVALID)
            {
              emit3w_o (A_INC, ic->result->aop, offset, 0, 0);
              size--;
              offset += 2;
              break;
            }
          if (!HAS_IYL_INST && (aopInReg (IC_RESULT (ic)->aop, offset, IYL_IDX) || aopInReg (IC_RESULT (ic)->aop, offset, IYH_IDX)))
            UNIMPLEMENTED;
          else
            emit3_incdec (A_INC, IC_RESULT (ic)->aop, offset++, ic);   // S1C88: route [ix+d]/abs INC through A
          if (size)
            {
              if (!regalloc_dry_run)
                emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key));
              cost2 (3, 10); // Assume jump is taken (upper bytes are skipped).
            }
        }
      regalloc_dry_run_state_scale = 1.0f;
      if (!regalloc_dry_run)
        (IC_LEFT (ic)->aop->type == AOP_HL) ? emitLabelSpill (tlbl) : emitLabel (tlbl);
      else if (IC_LEFT (ic)->aop->type == AOP_HL)
        spillCached ();
      return TRUE;
    }

  /* if the sizes are greater than 1 then we cannot */
  if (IC_RESULT (ic)->aop->size > 1 || IC_LEFT (ic)->aop->size > 1)
    return FALSE;

  /* If the result is in a register then we can load then increment.
   */
  if (IC_RESULT (ic)->aop->type == AOP_REG)
    {
      cheapMove (IC_RESULT (ic)->aop, LSB, IC_LEFT (ic)->aop, LSB, true);
      while (icount--)
        if (!HAS_IYL_INST && (aopInReg (IC_RESULT (ic)->aop, 0, IYL_IDX) || aopInReg (IC_RESULT (ic)->aop, 0, IYH_IDX)))
          UNIMPLEMENTED;
        else
          emit3_o (A_INC, IC_RESULT (ic)->aop, 0, 0, 0);
      return TRUE;
    }

  /* we can if the aops of the left & result match or
     if they are in registers and the registers are the
     same */
  if (sameRegs (IC_LEFT (ic)->aop, IC_RESULT (ic)->aop))
    {
      while (icount--)
        emit3_incdec (A_INC, IC_LEFT (ic)->aop, 0, ic);   // S1C88: route [ix+d]/abs INC through A
      return TRUE;
    }

  return FALSE;
}

/*-----------------------------------------------------------------*/
/* outBitAcc - output a bit in acc                                 */
/*-----------------------------------------------------------------*/
static void
outBitAcc (operand * result)
{
  symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
  /* if the result is a bit */
  if (result->aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to write A into a bit");
    }
  else
    {
      if (!regalloc_dry_run)
        {
          emit2 ("jp Z, !tlabel", labelKey2num (tlbl->key));
          emit2 ("ld a, !one");
          emitLabel (tlbl);
        }
      // Assume that both values are equally likely.
      cost2 (3, 10);
      cost2 (1, 3.5f);
      outAcc (result);
    }
}

static bool
couldDestroyCarry (const asmop *aop)
{
  if (aop)
    {
      if (aop->type == AOP_EXSTK || aop->type == AOP_IY)
        {
          return TRUE;
        }
    }
  return FALSE;
}

static void
shiftIntoPair (PAIR_ID id, asmop *aop)
{

  emitDebug ("; Shift into pair");

  switch (id)
    {
    case PAIR_HL:
      setupPair (PAIR_HL, aop, 0);
      break;
    case PAIR_IY:
      setupPair (PAIR_IY, aop, 0);
      break;
    default:
      wassertl (0, "Internal error - hit default case");
    }

  aop->type = AOP_PAIRPTR;
  aop->aopu.aop_pairId = id;
  _G.pairs[id].offset = 0;
  _G.pairs[id].last_type = aop->type;
}

static void
setupToPreserveCarry (asmop *result, asmop *left, asmop *right, const iCode *ic)
{
  wassert (left && right);

  /* S1C88: the arrangement for three distinct carry-destroying operands
     (right -> HL, result -> a DE pointer, left via the cached IY extended-
     stack access) needs a third pointer pair we don't have.  Collapse left
     into result first — a carry-free copy ahead of the chain — then run the
     chain in place: left/result share the IY pointer, right gets HL. */
  if (couldDestroyCarry (right) && couldDestroyCarry (result) && couldDestroyCarry (left) &&
    left != result && right != result)
    {
      genMove (result, left, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
      shiftIntoPair (PAIR_HL, right);
      shiftIntoPair (PAIR_IY, result);
      left->type = AOP_PAIRPTR;
      left->aopu.aop_pairId = PAIR_IY;
      return;
    }

  {
      if (couldDestroyCarry (right) && couldDestroyCarry (result))
        {
          shiftIntoPair (PAIR_HL, right);
          /* check result again, in case right == result */
          if (couldDestroyCarry (result))
            /* left == result (in-place) or left is carry-safe: left shares
               the IY pointer / needs none. */
            shiftIntoPair (PAIR_IY, result);
        }
      else if (couldDestroyCarry (right))
        {
          if (getPairId (result) == PAIR_HL)
            _G.preserveCarry = TRUE;
          else
            shiftIntoPair (PAIR_HL, right);
        }
      else if (couldDestroyCarry (result))
        {
          shiftIntoPair (PAIR_HL, result);
        }
    }
}

/*-----------------------------------------------------------------*/
/* genPlus - generates code for addition                           */
/*-----------------------------------------------------------------*/
static void
genPlus (iCode * ic)
{
  int size, i, offset = 0;
  signed char cached[2];
  bool premoved, started;
  asmop *leftop;
  asmop *rightop;
  symbol *tlbl = 0;

  /* special cases :- */

  aopOp (IC_LEFT (ic), ic, FALSE, FALSE);
  aopOp (IC_RIGHT (ic), ic, FALSE, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE, FALSE);

  sym_link *resulttype = operandType (IC_RESULT (ic));
  unsigned topbytemask = (IS_BITINT (resulttype) && SPEC_USIGN (resulttype) && (SPEC_BITINTWIDTH (resulttype) % 8)) ?
    (0xff >> (8 - SPEC_BITINTWIDTH (resulttype) % 8)) : 0xff;
  bool maskedtopbyte = (topbytemask != 0xff);

  /* Swap the left and right operands if:

     if literal, literal on the right or
     if left requires ACC or right is already
     in ACC */
  if ((IC_LEFT (ic)->aop->type == AOP_LIT) || (AOP_NEEDSACC (IC_RIGHT (ic))) || aopInReg (IC_RIGHT (ic)->aop, 0, A_IDX) ||
    IC_LEFT (ic)->aop->regs[A_IDX] < 0 && IC_RIGHT (ic)->aop->type == AOP_STL)
    {
      operand *t = IC_RIGHT (ic);
      IC_RIGHT (ic) = IC_LEFT (ic);
      IC_LEFT (ic) = t;
    }

  leftop = IC_LEFT (ic)->aop;
  rightop = IC_RIGHT (ic)->aop;

  /* if both left & right are in bit
     space */
  if (IC_LEFT (ic)->aop->type == AOP_CRY && IC_RIGHT (ic)->aop->type == AOP_CRY)
    {
      /* Cant happen */
      wassertl (0, "Tried to add two bits");
    }

  /* if left in bit space & right literal */
  if (IC_LEFT (ic)->aop->type == AOP_CRY && IC_RIGHT (ic)->aop->type == AOP_LIT)
    {
      /* Can happen I guess */
      wassertl (0, "Tried to add a bit to a literal");
    }

  /* if I can do an increment instead
     of add then GOOD for ME */
  if (!maskedtopbyte && genPlusIncr (ic))
    goto release;

  size = IC_RESULT (ic)->aop->size;

  /* Special case when left and right are constant */
  if (!maskedtopbyte && isPair (IC_RESULT (ic)->aop))
    {
      char *left = Safe_strdup (aopGetLitWordLong (IC_LEFT (ic)->aop, 0, FALSE));
      const char *right = aopGetLitWordLong (IC_RIGHT (ic)->aop, 0, FALSE);

      if (IC_LEFT (ic)->aop->type == AOP_LIT && IC_RIGHT (ic)->aop->type == AOP_LIT && left && right)
        {
          struct dbuf_s dbuf;

          /* It's a pair */
          /* PENDING: fix */
          dbuf_init (&dbuf, 128);
          dbuf_printf (&dbuf, "!immed(%s + %s)", left, right);
          Safe_free (left);
          emit2 ("ld %s, %s", getPairName (IC_RESULT (ic)->aop), dbuf_c_str (&dbuf));
          dbuf_destroy (&dbuf);
          if (getPairId (ic->result->aop) == PAIR_IY)
            cost2 (4, 14);
          else
            cost2 (3, 10);
          goto release;
        }
      Safe_free (left);
    }

  

  if (!maskedtopbyte && (isPair (IC_RIGHT (ic)->aop) || isPair (IC_LEFT (ic)->aop)) && getPairId (IC_RESULT (ic)->aop) == PAIR_HL)
    {
      /* Fetch into HL then do the add */
      PAIR_ID left = getPairId (IC_LEFT (ic)->aop);
      PAIR_ID right = getPairId (IC_RIGHT (ic)->aop);

      spillPair (PAIR_HL);

      if (left == PAIR_HL && right != PAIR_INVALID && (right != PAIR_IY))
        {
          emit3w (A_ADD, ASMOP_HL, ic->right->aop);
          goto release;
        }
      else if (right == PAIR_HL && left != PAIR_INVALID && (left != PAIR_IY))
        {
          emit3w (A_ADD, ASMOP_HL, ic->left->aop);
          goto release;
        }
      else if (right != PAIR_INVALID && right != PAIR_HL && (right != PAIR_IY))
        {
          genMove_o (ASMOP_HL, 0, ic->left->aop, 0, 2, isRegDead (A_IDX, ic), true, isRegDead (IY_IDX, ic) && right != PAIR_IY, true);
          emit3w (A_ADD, ASMOP_HL, ic->right->aop);
          goto release;
        }
      else if (left != PAIR_INVALID && left != PAIR_HL && (left != PAIR_IY))
        {
          genMove_o (ASMOP_HL, 0, ic->right->aop, 0, 2, isRegDead (A_IDX, ic), true, isRegDead (IY_IDX, ic) && left != PAIR_IY, true);
          emit3w (A_ADD, ASMOP_HL, ic->left->aop);
          goto release;
        }
      else if (left == PAIR_HL && isPairDead (PAIR_BA, ic))
        {
          /* S1C88: BA is the only 2nd ALU pair. A live BA falls through. */
          genMove (ASMOP_BA, ic->right->aop, false, false, false);
          emit2 ("add hl, ba");
          cost2 (1, 11);
          goto release;
        }
      else if (right == PAIR_HL && isPairDead (PAIR_BA, ic))
        {
          genMove (ASMOP_BA, leftop, false, false, false);
          emit2 ("add hl, ba");
          cost2 (1, 11);
          goto release;
        }
      else
        {
          /* Can't do it */
        }
    }
  else if (!maskedtopbyte && size == 2 && getPairId (ic->result->aop) == PAIR_HL && isPairDead (PAIR_BA, ic) &&
    (ic->right->aop->type == AOP_LIT || ic->right->aop->type == AOP_IMMD || ic->left->aop->type == AOP_IMMD && (ic->right->aop->type == AOP_HL || ic->right->aop->type == AOP_IY)))
    {
      /* S1C88: BA is the only 2nd ALU pair. */
      genMove (ASMOP_HL, ic->left->aop, isRegDead (A_IDX, ic), true, isRegDead (IY_IDX, ic));
      genMove (ASMOP_BA, ic->right->aop, isRegDead (A_IDX, ic), false, isRegDead (IY_IDX, ic));
      emit2 ("add hl, ba");
      cost2 (1, 11);
      goto release;
    }

  // Handle AOP_EXSTK conflict with hl here, since setupToPreserveCarry() would cause problems otherwise.
  if (!maskedtopbyte && IC_RESULT (ic)->aop->type == AOP_EXSTK && size <= 2 && (getPairId (IC_LEFT (ic)->aop) == PAIR_HL || getPairId (IC_RIGHT (ic)->aop) == PAIR_HL) &&
    isPairDead (PAIR_BA, ic) && isPairDead (PAIR_HL, ic))
    {
      /* S1C88: BA is the only 2nd ALU pair. */
      fetchPair (PAIR_BA, getPairId (IC_LEFT (ic)->aop) == PAIR_HL ? IC_RIGHT (ic)->aop : IC_LEFT (ic)->aop);
      emit2 ("add hl, ba");
      cost2 (1, 11);
      spillPair (PAIR_HL);
      genMove (IC_RESULT (ic)->aop, ASMOP_HL, isRegDead (A_IDX, ic), true, true);
      goto release;
    }
  else if (!maskedtopbyte && getPairId (IC_RESULT (ic)->aop) == PAIR_IY &&
    (getPairId (IC_LEFT (ic)->aop) == PAIR_HL && isPair (IC_RIGHT (ic)->aop) && getPairId (IC_RIGHT (ic)->aop) != PAIR_IY || getPairId (IC_RIGHT (ic)->aop) == PAIR_HL && isPair (IC_LEFT (ic)->aop) && getPairId (IC_LEFT (ic)->aop) != PAIR_IY) &&
    isPairDead (PAIR_HL, ic))
    {
      PAIR_ID pair = (getPairId (IC_LEFT (ic)->aop) == PAIR_HL ? getPairId (IC_RIGHT (ic)->aop) : getPairId (IC_LEFT (ic)->aop));
      emit2 ("add hl, %s", _pairs[pair].name);
      cost2 (1, 11);
      _push (PAIR_HL);
      _pop (PAIR_IY);
      goto release;
    }
  else if (!maskedtopbyte && getPairId (ic->result->aop) == PAIR_IY &&
    ic->left->aop->type != AOP_IY && ic->right->aop->type != AOP_IY)
    {
      bool save_pair = FALSE;
      PAIR_ID pair;

      if (getPairId (IC_RIGHT (ic)->aop) == PAIR_IY || 0 || 0 ||
          ic->left->aop->regs[IYL_IDX] < 0 && ic->left->aop->regs[IYH_IDX] < 0 && (ic->right->aop->type == AOP_IMMD || ic->right->aop->type == AOP_LIT))
        {
          operand *t = IC_RIGHT (ic);
          IC_RIGHT (ic) = IC_LEFT (ic);
          IC_LEFT (ic) = t;
          leftop = IC_LEFT (ic)->aop;
          rightop = IC_RIGHT (ic)->aop;
        }
      /* S1C88: ADD IY takes BA or HL. */
      pair = getPairId (IC_RIGHT (ic)->aop);
      if (pair != PAIR_HL)
        {
          pair = PAIR_BA;
          if (!isPairDead (PAIR_BA, ic))
            save_pair = TRUE;
        }
      genMove (ASMOP_IY, IC_LEFT (ic)->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
      if (save_pair)
        _push (pair);
      asmop *raop = (pair == PAIR_HL) ? ASMOP_HL : ASMOP_BA;
      genMove (raop, ic->right->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
      emit2 ("add iy, %s", _pairs[pair].name);
      spillPair (PAIR_IY);
      cost2 (2, 15);
      if (save_pair)
        _pop (pair);
      goto release;
    }


  // Avoid overwriting operand in h or l when setupToPreserveCarry () loads hl - only necessary if carry is actually used during addition.
  premoved = FALSE;
  if (size > 1 &&
    !(size == 2 && isPair (leftop) && (rightop->type == AOP_LIT || rightop->type == AOP_IY)) && !(size == 2 && leftop->type == AOP_STL && isPairDead (PAIR_HL, ic))) // No need to setup if a single 16 bit addition is sufficient below.
    {
      if (!couldDestroyCarry (leftop) && (couldDestroyCarry (rightop) || couldDestroyCarry (IC_RESULT (ic)->aop)))
        {
          cheapMove (ASMOP_A, 0, leftop, offset, true);
          premoved = TRUE;
        }

      if ((requiresHL (IC_RESULT (ic)->aop) && IC_RESULT (ic)->aop->type != AOP_REG || requiresHL (leftop) && leftop->type != AOP_REG || requiresHL (rightop) && rightop->type != AOP_REG) &&
        (leftop->regs[L_IDX] > 0 || leftop->regs[H_IDX] > 0 || rightop->regs[L_IDX] > 0 || rightop->regs[H_IDX] > 0))
        UNIMPLEMENTED;
      setupToPreserveCarry (IC_RESULT (ic)->aop, leftop, rightop, ic);
    }
  // But if we don't actually want to use hl for the addition, it can make sense to setup an op to use cheaper hl instead of iy.
  if (size == 1 && !aopInReg(leftop, 0, H_IDX) && !aopInReg(leftop, 0, L_IDX) && !aopInReg(rightop, 0, H_IDX) && !aopInReg(rightop, 0, L_IDX) && isPairDead (PAIR_HL, ic))
    {
      if (couldDestroyCarry (IC_RESULT (ic)->aop) &&
        (IC_RESULT (ic)->aop == leftop || IC_RESULT (ic)->aop == rightop))
        shiftIntoPair (PAIR_HL, IC_RESULT (ic)->aop);
      else if (couldDestroyCarry (rightop))
        shiftIntoPair (PAIR_HL, rightop);
    }

  cached[0] = -1;
  cached[1] = -1;

  for (i = 0, started = false; i < size;)
    {
      bool maskedbyte = maskedtopbyte && (i + 1 == size);
      bool maskedword = maskedtopbyte && (i + 2 == size);

      const bool hl_dead = isPairDead (PAIR_HL, ic) &&
        leftop->regs[L_IDX] <= i && leftop->regs[H_IDX] <= i &&
        rightop->regs[L_IDX] <= i && rightop->regs[H_IDX] <= i &&
        (IC_RESULT (ic)->aop->regs[L_IDX] < 0 || IC_RESULT (ic)->aop->regs[L_IDX] >= i) && (IC_RESULT (ic)->aop->regs[H_IDX] < 0 || IC_RESULT (ic)->aop->regs[H_IDX] >= i);

      // Rematerialization of addresses on the stack.
      if (!maskedword && leftop->type == AOP_STL && !i && i + 1 < size && rightop->type == AOP_LIT && hl_dead)
        {
          emit2 ("ld hl, !immed%d", spOffset (leftop->aopu.aop_stk) + (ulFromVal (rightop->aopu.aop_lit) & 0xffff));
          cost2 (3, 10);
          emit2 ("add hl, sp");
          cost2 (1, 11);
          spillPair (PAIR_HL);
          started = true;
          genMove_o (IC_RESULT (ic)->aop, 0, ASMOP_HL, 0, 2, true, true, false, i + 2 == size);
          i += 2;
          continue;
        }
      else if (!maskedword && leftop->type == AOP_STL && !i && i + 1 < size && hl_dead && (size <= 2 || leftop->type != AOP_EXSTK /* (hl) would be pointed to result, overwritten by addition here */))
        {
          /* S1C88: the 2nd ALU pair is BA. Move the addend into
             BA and `add hl, ba`; save/restore BA when it isn't dead (never fall
             back to the nonexistent DE). Setting up HL from the AOP_STL address
             is `ld hl,#off; add hl,sp` — it never touches A/B, so BA survives. */
          const bool save_ba = !isPairDead (PAIR_BA, ic);
          if (save_ba)
            _push (PAIR_BA);
          genMove (ASMOP_BA, rightop, true, true, false);
          genMove (ASMOP_HL, leftop, true, true, false);
          emit2 ("add hl, ba");
          spillPair (PAIR_BA);
          cost2 (1, 11);
          started = true;
          if (save_ba)
            _pop (PAIR_BA);
          genMove_o (IC_RESULT (ic)->aop, 0, ASMOP_HL, 0, 2, true, true, false, i + 2 == size);
          i += 2;
          continue;
        }
      // Addition of interleaved pairs.
      else if (!maskedword && (!premoved || i) && leftop->size - i >= 2 && rightop->size - i >= 2 &&
        (aopInReg (IC_RESULT (ic)->aop, i, HL_IDX) || aopInReg (IC_RESULT (ic)->aop, i, IY_IDX) && !started))
        {
          const bool iy = aopInReg (IC_RESULT (ic)->aop, i, IY_IDX);
          PAIR_ID pair = PAIR_INVALID;

          if (aopInReg (leftop, i, iy ? IYL_IDX : L_IDX) && aopInReg (rightop, i + 1, iy ? IYH_IDX : H_IDX))
            {
              
            }
          else if (aopInReg (leftop, i + 1, iy ? IYH_IDX : H_IDX) && aopInReg (rightop, i, iy ? IYL_IDX : L_IDX))
            {
              
            }

          if (pair != PAIR_INVALID)
            {
              if (started)
                {
                  wassert (!iy);
                  emit2 ("adc hl, %s", _pairs[pair].name);
                  cost2 (2, 15);
                }
              else
                {
                  emit2 (iy ? "add iy, %s" : "add hl, %s", _pairs[pair].name);
                  started = true;
                  if (pair == PAIR_IY)
                    cost2 (2, 15);
                  else
                    cost2 (1, 11);
                }
              i += 2;
              continue;
            }
        }

      if (!maskedword && (!premoved || i) && !started && i == size - 2 && !i && isPair (rightop) && leftop->type == AOP_IMMD &&
        getPairId (rightop) != PAIR_HL && (getPairId (rightop) != PAIR_IY) &&
        isPairDead (PAIR_HL, ic))
        {
          genMove_o (ASMOP_HL, 0, IC_LEFT (ic)->aop, i, 2, true, true, true, true);
          emit3w (A_ADD, ASMOP_HL, ic->right->aop);
          started = true;
          spillPair (PAIR_HL);
          genMove_o (IC_RESULT (ic)->aop, i, ASMOP_HL, 0, 2, true, true, true, true);
          i += 2;
        }
     else  if (!maskedword && (!premoved || i) && !started && i == size - 2 && !i && isPair (leftop) && (rightop->type == AOP_LIT  || rightop->type == AOP_IMMD) &&
       getPairId (leftop) != PAIR_HL && (getPairId (leftop) != PAIR_IY) &&
       isPairDead (PAIR_HL, ic))
        {
          genMove_o (ASMOP_HL, 0, IC_RIGHT (ic)->aop, i, 2, true, true, true, true);
          emit3w (A_ADD, ASMOP_HL, ic->left->aop);
          started = true;
          spillPair (PAIR_HL);
          genMove_o (IC_RESULT (ic)->aop, i, ASMOP_HL, 0, 2, true, true, true, true);
          i += 2;
        }
      else if (!maskedword && (!premoved || i) && !started && i == size - 2 && !i && aopInReg (leftop, i, HL_IDX) &&
        isPair (rightop) && getPairId (rightop) != PAIR_HL && (getPairId (rightop) != PAIR_IY) &&
        isPairDead (PAIR_HL, ic))
        {
          emit3w (A_ADD, ASMOP_HL, ic->right->aop);
          started = true;
          genMove_o (IC_RESULT (ic)->aop, i, ASMOP_HL, 0, 2, true, true, true, true);
          i += 2;
        }
      else if (!maskedword && (!premoved || i) && !started && i == size - 2 && !i &&
        isPair (leftop) && getPairId (leftop) != PAIR_HL && (getPairId (leftop) != PAIR_IY) &&
        aopInReg (rightop, i, HL_IDX) && isPairDead (PAIR_HL, ic))
        {
          emit3w (A_ADD, ASMOP_HL, ic->left->aop);
          started = true;
          genMove_o (IC_RESULT (ic)->aop, i, ASMOP_HL, 0, 2, true, true, true, true);
          i += 2;
        }
      else if (!maskedword && (!premoved || i) && aopInReg (IC_RESULT (ic)->aop, i, HL_IDX) && aopInReg (leftop, i, HL_IDX) && (rightop->type == AOP_LIT && !aopIsLitVal (rightop, i, 1, 0) || rightop->type == AOP_IMMD))
        {
          PAIR_ID pair = getFreePairId (ic);
          bool pair_alive;
          if (pair == PAIR_INVALID)
            pair = PAIR_BA;     /* saved/restored below when alive */
          pair_alive = !isPairDead (pair, ic) ||
            IC_RESULT (ic)->aop->regs[_pairs[pair].l_idx] < i || IC_RESULT (ic)->aop->regs[_pairs[pair].h_idx] < i ||
            IC_LEFT (ic)->aop->regs[_pairs[pair].l_idx] >= i + 2 || IC_LEFT (ic)->aop->regs[_pairs[pair].h_idx] >= i + 2;
          if (pair_alive)
            _push (pair);
          fetchPairLong (pair, IC_RIGHT (ic)->aop, 0, i);
          if (started)
            {
              emit2 ("adc hl, %s", _pairs[pair].name);
              cost2 (2, 15);
            }
          else
            {
              emit2 ("add hl, %s", _pairs[pair].name);
              started = TRUE;
              cost2 (1, 11);
            }
          if (pair_alive)
            _pop (pair);
          i += 2;
        }
      // When adding registers the 16 bit addition results in smaller, faster code than an 8-bit addition.
      else if (!maskedbyte && (!premoved || i) && optimize.codeSize && !started && i == size - 1 && isPairDead (PAIR_HL, ic) && isRegDead (A_IDX, ic) && rightop->type == AOP_LIT && aopInReg (IC_RESULT (ic)->aop, i, L_IDX) && aopInReg (leftop, i, L_IDX))
        {
          /* Top-byte add into L with HL dead: the garbage in B only feeds H,
             which this result never reads. */
          emit2 ("ld a, !immedbyte", (ulFromVal (IC_RIGHT (ic)->aop->aopu.aop_lit)) & 0xffu);
          cost2 (2, 7);
          emit2 ("add hl, ba");
          cost2 (1, 11);
          started = true;
          i++;
        }
      // Skip over this byte.
      else if (!maskedbyte && !premoved && !started && (leftop->type == AOP_REG || IC_RESULT (ic)->aop->type == AOP_REG) && aopIsLitVal (rightop, i, 1, 0))
        {
          cheapMove (IC_RESULT (ic)->aop, i, leftop, i, true);
          i++;
        }
      // Conditional 16-bit inc.
      else if (!maskedword && i == size - 2 && started && aopIsLitVal (rightop, i, 2, 0) && (aopInReg (IC_RESULT (ic)->aop, i, HL_IDX) && aopInReg (leftop, i, HL_IDX) || aopInReg (IC_RESULT (ic)->aop, i, IY_IDX) && aopInReg (leftop, i, IY_IDX)))
        {
          if (!tlbl && !regalloc_dry_run)
            tlbl = newiTempLabel (0);

          if (!regalloc_dry_run)
            emit2 ("jp NC, !tlabel", labelKey2num (tlbl->key));
          cost2 (2, 12); // Assume branch is taken. Use cost of jr as the peephole optimizer can typically optimize this jp into jr. Do not emit jr directly to still allow jump-to-jump optimization.
          regalloc_dry_run_state_scale /= 256.0f; // Carry should be rare.
          emit3w_o (A_INC, leftop, i, 0, 0);
          i += 2;
        }
      // Conditional 8-bit inc.
      else if (!maskedbyte && i == size - 1 && started && aopIsLitVal (rightop, i, 1, 0) &&
        !aopInReg (leftop, i, A_IDX) && // adc a, #0 is cheaper than conditional inc.
        (i < leftop->size &&
        leftop->type == AOP_REG && IC_RESULT (ic)->aop->type == AOP_REG &&
        leftop->aopu.aop_reg[i]->rIdx == IC_RESULT (ic)->aop->aopu.aop_reg[i]->rIdx &&
        (HAS_IYL_INST || leftop->aopu.aop_reg[i]->rIdx != IYL_IDX && leftop->aopu.aop_reg[i]->rIdx != IYH_IDX) ||
        leftop->type == AOP_STK && leftop == IC_RESULT (ic)->aop ||
        leftop->type == AOP_PAIRPTR && leftop->aopu.aop_pairId == PAIR_HL))
        {
          if (!tlbl && !regalloc_dry_run)
            tlbl = newiTempLabel (0);
          if (!regalloc_dry_run)
            emit2 ("jp NC, !tlabel", labelKey2num (tlbl->key));
          cost2 (2, 12); // Assume branch is taken. Use cost of jr as the peephole optimizer can typically optimize this jp into jr. Do not emit jr directly to still allow jump-to-jump optimization.
          regalloc_dry_run_state_scale /= 256.0f; // Carry should be rare.
          emit3_incdec (A_INC, leftop, i, ic);   // S1C88: route [ix+d]/abs INC through A
          i++;
        }
      else if (!started && !premoved && aopIsLitVal (leftop, i, 1, 0))
        {
          cheapMove (ic->result->aop, i, rightop, i, true);
          i++;
        }
      else
        {
          if (!HAS_IYL_INST && (aopInReg (rightop, i, IYL_IDX) || aopInReg (rightop, i, IYH_IDX)))
            if (!premoved && !aopInReg (leftop, i, IYL_IDX) && !aopInReg (leftop, i, IYL_IDX))
              {
                operand *t = IC_RIGHT (ic);
                IC_RIGHT (ic) = IC_LEFT (ic);
                IC_LEFT (ic) = t;
                leftop = IC_LEFT (ic)->aop;
                rightop = IC_RIGHT (ic)->aop;
              }
            else // Can't handle both sides in iy.
              UNIMPLEMENTED;
          else if (rightop->type == AOP_STL && i < 2) // can't handle rematerialized stack location on the right efficiently.
            {
              operand *t = IC_RIGHT (ic);
              IC_RIGHT (ic) = IC_LEFT (ic);
              IC_LEFT (ic) = t;
              leftop = IC_LEFT (ic)->aop;
              rightop = IC_RIGHT (ic)->aop;
            }

          if (aopInReg (rightop, i, A_IDX) && !aopInReg (leftop, i, A_IDX)) // Make sure we don't overwrite the other operand.
            UNIMPLEMENTED;
          else if (!premoved)
            cheapMove (ASMOP_A, 0, leftop, i, true);
          else
            premoved = FALSE;

          if (!started && aopIsLitVal (rightop, i, 1, 0))
            ; // Skip over this byte.
          // We can use inc / dec only for the only, top non-zero byte, since it neither takes into account an existing carry nor does it update the carry.
          else if (!started && i == size - 1 && (aopIsLitVal (rightop, i, 1, 1) || aopIsLitVal (rightop, i, 1, 255)))
            {
              emit3 (aopIsLitVal (rightop, i, 1, 1) ? A_INC : A_DEC, ASMOP_A, 0);
              started = true;
            }
          else if (rightop->type == AOP_STL && i < 2)
            {
              _push (PAIR_HL);
              genMove (ASMOP_HL, rightop, false, true, false);
              emit3_8alu (started ? A_ADC : A_ADD, i ? ASMOP_H : ASMOP_L, 0, ic);
              started = true;
              _pop (PAIR_HL);
            }
          else if (!HAS_IYL_INST && (aopInReg (rightop, i, IYL_IDX) || aopInReg (rightop, i, IYH_IDX)))
            UNIMPLEMENTED;
          else
            {
              emit3_8alu (started ? A_ADC : A_ADD, rightop, i, ic);
              started = true;
            }
          if (maskedbyte)
            {
              emit2 ("and a, #0x%02x", topbytemask);
              cost2 (2, 7);
            }

          _G.preserveCarry = (i != size - 1);
          if (size &&
            (requiresHL (rightop) && rightop->size > i + 1 && rightop->type != AOP_REG || (requiresHL (leftop) && leftop->size > i + 1)
            && leftop->type != AOP_REG) && IC_RESULT (ic)->aop->type == AOP_REG
            && (IC_RESULT (ic)->aop->aopu.aop_reg[i]->rIdx == L_IDX
              || IC_RESULT (ic)->aop->aopu.aop_reg[i]->rIdx == H_IDX))
            {
              wassert (cached[0] == -1 || cached[1] == -1);
              cached[cached[0] == -1 ? 0 : 1] = offset++;
              _push (PAIR_AF);
            }
          // Avoid overwriting still-needed operand in h or l.
          else if (requiresHL (IC_RESULT (ic)->aop) && IC_RESULT (ic)->aop->type != AOP_REG && (IC_RESULT (ic)->aop->type == AOP_EXSTK || IC_RESULT (ic)->aop->type == AOP_PAIRPTR) &&
            (!isPairDead(PAIR_HL, ic) || i + 1 < size && IC_LEFT(ic)->aop->regs[L_IDX] > i || i + 1 < size && IC_LEFT(ic)->aop->regs[H_IDX] > i || i + 1 < size && IC_RIGHT(ic)->aop->regs[L_IDX] > i || i + 1 < size && IC_RIGHT(ic)->aop->regs[H_IDX] > i))
            {
              _push (PAIR_HL);
              cheapMove (IC_RESULT (ic)->aop, i, ASMOP_A, 0, true);
              _pop (PAIR_HL);
            }
          else
            cheapMove (IC_RESULT (ic)->aop, i, ASMOP_A, 0, true);
          i++;
        }
    }
  _G.preserveCarry = false;

  regalloc_dry_run_state_scale = 1.0f;
  if (tlbl)
    emitLabel (tlbl);

  for (size = 1; size >= 0; size--)
    if (cached[size] != -1)
      {
        if (IC_RESULT (ic)->aop->regs[A_IDX] >= 0 && IC_RESULT (ic)->aop->regs[A_IDX] != size) // Don't overwrite still-needed a below.
          UNIMPLEMENTED;
        _pop (PAIR_AF);
        cheapMove (IC_RESULT (ic)->aop, cached[size], ASMOP_A, 0, true);
      }

release:
  _G.preserveCarry = FALSE;
  freeAsmop (IC_LEFT (ic), NULL);
  freeAsmop (IC_RIGHT (ic), NULL);
  freeAsmop (IC_RESULT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genSubDec :- does subtraction with decrement if possible        */
/*-----------------------------------------------------------------*/
static bool
genMinusDec (const iCode *ic, asmop *result, asmop *left, asmop *right)
{
  unsigned int icount;
  unsigned int size = IC_RESULT (ic)->aop->size;

  /* will try to generate a decrement */
  /* if the right side is not a literal we cannot */
  if (right->type != AOP_LIT)
    return false;

  /* if the literal value of the right hand side
     is greater than 4 then it is not worth it */
  if ((icount = (unsigned int) ulFromVal (right->aopu.aop_lit)) > 2)
    return false;
  /* if decrement 16 bits in register */
  if (sameRegs (left, result) && (size > 1) && isPair (result))
    {
      while (icount--)
        {
          emit2 ("dec %s", getPairName (result));
          if (getPairId (result) == PAIR_IY)
            cost2 (2, 10);
          else
            cost2 (1, 6);
        }
      return true;
    }

  /* If result is a pair */
  if (isPair (IC_RESULT (ic)->aop))
    {
      fetchPair (getPairId (result), left);
      while (icount--)
        {
          if (!regalloc_dry_run)
            emit2 ("dec %s", getPairName (result));
          if (getPairId (result) == PAIR_IY)
            cost2 (2, 10);
          else
            cost2 (1, 6);
        }
      return true;
    }

  /* if decrement 16 bits in register */
  if (sameRegs (left, result) && size == 2 && isPairDead (_getTempPairId (), ic) && !(requiresHL (left) && _getTempPairId () == PAIR_HL))
    {
      fetchPair (_getTempPairId (), left);

      while (icount--)
        {
          if (!regalloc_dry_run)
            emit2 ("dec %s", _getTempPairName ());
          cost2 (1, 6);
        }

      commitPair (result, _getTempPairId (), ic, false);

      return true;
    }


  /* if the sizes are greater than 1 then we cannot */
  if (result->size > 1 || left->size > 1)
    return false;

  /* we can if the aops of the left & result match or if they are in
     registers and the registers are the same */
  if (sameRegs (left, result))
    {
      while (icount--)
        emit3_incdec (A_DEC, result, 0, ic);   // S1C88: route [ix+d]/abs DEC through A
      return true;
    }

  if (result->type == AOP_REG)
    {
      cheapMove (result, 0, left, 0, true);
      while (icount--)
        emit3 (A_DEC, result, 0);
      return true;
    }

  return false;
}

/*-----------------------------------------------------------------*/
/* genSub - generates code for subtraction                       */
/*-----------------------------------------------------------------*/
static void
genSub (const iCode *ic, asmop *result, asmop *left, asmop *right)
{
  int size, offset = 0;
  unsigned long long lit = 0L;

  sym_link *resulttype = operandType (IC_RESULT (ic));
  unsigned topbytemask = (IS_BITINT (resulttype) && SPEC_USIGN (resulttype) && (SPEC_BITINTWIDTH (resulttype) % 8)) ?
    (0xff >> (8 - SPEC_BITINTWIDTH (resulttype) % 8)) : 0xff;
  bool maskedtopbyte = (topbytemask != 0xff);

  /* special cases :- */
  /* if both left & right are in bit space */
  if (left->type == AOP_CRY && right->type == AOP_CRY)
    {
      wassertl (0, "Tried to subtract two bits");
      return;
    }

  /* if I can do an decrement instead of subtract then GOOD for ME */
  if (!maskedtopbyte && genMinusDec (ic, result, left, right) == TRUE)
    return;

  size = IC_RESULT (ic)->aop->size;

  /* S1C88 native 16-bit subtract. The core has a true 16-bit SUB/SBC on its
     two ALU pairs (HL, BA). The byte-wise "sub a,l / sbc a,h" idiom is *illegal* on
     the S1C88 (the 8-bit ALU source must be A or B, never L/H), so we must emit
     the native pair op whenever the right operand is a register pair.

     We compute in whichever ALU pair we can place `left` in, requiring `right`
     to be the *other* ALU pair (the two pairs are disjoint, so getting `left`
     into the compute pair never clobbers `right`). Literal/memory right still
     assemble legally via the byte loop below (8-bit imm/memory sources are
     fine), so we only intercept the register-register case here. */
  if (!maskedtopbyte && size == 2)
    {
      PAIR_ID leftpair = aluPairId (left, 0);
      PAIR_ID respair = aluPairId (result, 0);
      PAIR_ID compute = PAIR_INVALID;

      if (leftpair != PAIR_INVALID && aluPairId (right, 0) == (leftpair == PAIR_HL ? PAIR_BA : PAIR_HL))
        compute = leftpair;            /* left already in an ALU pair, right is the other */
      else if (respair != PAIR_INVALID && aluPairId (right, 0) == (respair == PAIR_HL ? PAIR_BA : PAIR_HL))
        compute = respair;             /* move left into the result pair, right is the other */

      if (compute != PAIR_INVALID)
        {
          PAIR_ID other = (compute == PAIR_HL) ? PAIR_BA : PAIR_HL;
          asmop *computeop = (compute == PAIR_HL) ? ASMOP_HL : ASMOP_BA;

          if (aluPairId (left, 0) != compute)
            genMove (computeop, left, false, false, false);
          spillPair (compute);
          emit2 ("sub %s, %s", _pairs[compute].name, _pairs[other].name);
          cost2 (2, 15);
          if (respair != compute)
            {
              spillPair (compute);
              genMove (result, computeop, false, false, false);
            }
          _G.preserveCarry = FALSE;
          return;
        }
    }

  if (right->type == AOP_LIT)
    {
      lit = ullFromVal (right->aopu.aop_lit);
      lit = -(long long) lit;
    }

  /* Same logic as genPlus */
  

  if ((requiresHL (result) && result->type != AOP_REG || requiresHL (left) && left->type != AOP_REG || requiresHL (right) && right->type != AOP_REG) &&
    (left->regs[L_IDX] > 0 || left->regs[H_IDX] > 0 || right->regs[L_IDX] > 0 || right->regs[H_IDX] > 0))
    UNIMPLEMENTED;
  setupToPreserveCarry (result, left, right, ic);

  /* if literal right, add a, #-lit, else normal subb */
  while (size)
    {
      bool maskedbyte = maskedtopbyte && (size == 1);
      bool maskedword = maskedtopbyte && (size == 2);

      
        
      bool l_dead = !(!isRegDead (L_IDX, ic) || left->regs[L_IDX] > offset || right->regs[L_IDX] > offset || result->regs[L_IDX] >= 0 && result->regs[L_IDX] < offset);
      bool h_dead = !(!isRegDead (H_IDX, ic) || left->regs[H_IDX] > offset || right->regs[H_IDX] > offset || result->regs[H_IDX] >= 0 && result->regs[H_IDX] < offset);
      bool hl_dead = l_dead && h_dead;
      bool pushed_hl = false;

      if (right->type != AOP_LIT)
        {
          if ((requiresHL (left) && left->type != AOP_REG || requiresHL (right) && right->type != AOP_REG) && !hl_dead)
            {
              _push (PAIR_HL);
              pushed_hl = true;
            }

          if ((aopInReg (right, offset, IYL_IDX)  || aopInReg (right, offset, IYH_IDX)) && !HAS_IYL_INST) // From here on all codepaths needs to use right as operand.
            UNIMPLEMENTED;
          else if (right->type == AOP_STL && offset < 2)
            {
              cheapMove (ASMOP_A, 0, left, offset, true);
              if (!hl_dead && !pushed_hl)
                {
                  _push (PAIR_HL);
                  pushed_hl = true;
                }
              genMove (ASMOP_HL, right, false, true, false);
              emit3_8alu (offset ? A_SBC : A_SUB, offset ? ASMOP_H : ASMOP_L, 0, ic);
            }
          else if (!offset)
            {
              if (aopIsLitVal (left, offset, 1, 0x00) && aopInReg (right, offset, A_IDX))
                emit3 (A_NEG, ASMOP_A, 0);   // S1C88 neg needs an explicit operand (neg a)
              else
                {
                  if (aopIsLitVal (left, offset, 1, 0x00) && !aopInReg (left, offset, A_IDX))
                    emit3 (A_XOR, ASMOP_A, ASMOP_A);
                  else
                    cheapMove (ASMOP_A, 0, left, offset, true);
                  if ((aopInReg (right, offset, L_IDX) || aopInReg (right, offset, H_IDX)) && pushed_hl)
                    {
                      _pop (PAIR_HL);
                      pushed_hl = false;
                    }
                  emit3_8alu (A_SUB, right, offset, ic);
                }
            }
          else if (aopIsLitVal (left, offset, 1, 0x00) && !aopInReg (left, offset, A_IDX) && size == 1) // For the last byte, we can do an optimization that results in the same value in a, but different carry.
            {
              emit3 (A_SBC, ASMOP_A, ASMOP_A);
              emit3_8alu (A_SUB, right, offset, ic);
            }
          else
            {
              cheapMove (ASMOP_A, 0, left, offset, true);
              if ((aopInReg (right, offset, L_IDX) || aopInReg (right, offset, H_IDX)) && pushed_hl)
                {
                  _pop (PAIR_HL);
                  pushed_hl = false;
                }
              emit3_8alu (A_SBC, right, offset, ic);
            }
        }
      else // right is a literal.
        {
          if (requiresHL (left) && left->type != AOP_REG && !hl_dead)
            {
              _push (PAIR_HL);
              pushed_hl = true;
            }

          cheapMove (ASMOP_A, 0, left, offset, true);

          /* first add without previous c */
          if (!offset)
            {
              if (size == 0 && (unsigned int) (lit & 0x0FFL) == 0xFF)
                emit3 (A_DEC, ASMOP_A, 0);
              else
                {
                  if (!regalloc_dry_run)
                    emit2 ("add a, !immedbyte", (unsigned int)(lit & 0x0fful));
                  cost2 (2, 7);
                }
            }
          else
            emit2 ("adc a, !immedbyte", (unsigned int)((lit >> (offset * 8)) & 0x0fful));
        }

      if (maskedbyte)
        {
          emit2 ("and a, #0x%02x", topbytemask);
          cost2 (2, 7);
        }

      if (pushed_hl)
        _pop (PAIR_HL);
      size--;
      _G.preserveCarry = !!size;
      cheapMove (result, offset++, ASMOP_A, 0, true);

      /* A result byte already stored into L or H corrupts a PAIRPTR-in-HL
         operand for the REMAINING bytes (setupPair AOP_PAIRPTR adjusts the
         cached pointer blindly -- it assumes the pair is never clobbered).
         Check the bytes written so far (offset was just incremented), not
         the upcoming one: byte 0 landing in L with byte 1 still to be read
         through (hl) is exactly the miscompile (caught by tests/emu 02). */
      if ((left->type == AOP_PAIRPTR && left->aopu.aop_pairId == PAIR_HL || right->type == AOP_PAIRPTR && right->aopu.aop_pairId == PAIR_HL) &&
        size &&
        (result->regs[L_IDX] >= 0 && result->regs[L_IDX] < offset || result->regs[H_IDX] >= 0 && result->regs[H_IDX] < offset))
        UNIMPLEMENTED;
    }

}

/*-----------------------------------------------------------------*/
/* genMinus - generates code for subtraction                       */
/*-----------------------------------------------------------------*/
static void
genMinus (const iCode *ic, const iCode *ifx)
{
  aopOp (IC_LEFT (ic), ic, FALSE, FALSE);
  aopOp (IC_RIGHT (ic), ic, FALSE, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE, FALSE);

  if (ifx && ifx->generated)
    {
      wassert (ic->result->aop->size == 1 && IS_OP_LITERAL (ic->right) && ullFromVal (OP_VALUE (ic->right)) == 1);

      if (ic->result->aop->type == AOP_REG && (!aopInReg (ic->result->aop, 0, IYL_IDX) && !aopInReg (ic->result->aop, 0, IYH_IDX) || HAS_IYL_INST))
        {
          cheapMove (ic->result->aop, 0, ic->left->aop, 0, isRegDead (A_IDX, ic));
          emit3 (A_DEC, ic->result->aop, 0);
          if (aopInReg (ic->result->aop, 0, B_IDX) && IC_TRUE (ifx)) // This jump can likely be optimized to djnz.
            {
              // cost2 can't handle negative costs, so we do this manually.
              regalloc_dry_run_cost_bytes--; 
              regalloc_dry_run_cost_states += -3.0 * regalloc_dry_run_state_scale;
            }
        }
      else
        {
          if (!isRegDead (A_IDX, ic))
            UNIMPLEMENTED;
          cheapMove (ASMOP_A, 0, ic->left->aop, 0, true);
          emit3 (A_DEC, ASMOP_A, 0);
          cheapMove (ic->result->aop, 0, ASMOP_A, 0, true);
        }
      if (IC_TRUE (ifx))
        emit2 ("jp NZ, !tlabel", labelKey2num (IC_TRUE (ifx)->key));
      else
        emit2 ("jp Z, !tlabel", labelKey2num (IC_FALSE (ifx)->key));
      cost2 (2, 9.5f); // Assume both branches equally likely. Assume jp will be optimized to jr.
    }
  else
    genSub (ic, ic->result->aop, ic->left->aop, ic->right->aop);

  _G.preserveCarry = FALSE;
  freeAsmop (IC_LEFT (ic), NULL);
  freeAsmop (IC_RIGHT (ic), NULL);
  freeAsmop (IC_RESULT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genUminusFloat - unary minus for floating points                */
/*-----------------------------------------------------------------*/
static void
genUminusFloat (const iCode *ic, operand *result, operand *op)
{
  emitDebug ("; genUminusFloat");

  /* for this we just need to flip the
     first bit then copy the rest in place */
     
  if (!isRegDead (A_IDX, ic))
    _push (PAIR_AF);

  cheapMove (ASMOP_A, 0, op->aop, MSB32, true);

  emit2 ("xor a,!immedbyte", 0x80u);
  cost2 (2, 7);
  cheapMove (result->aop, MSB32, ASMOP_A, 0, true);

  genMove_o (result->aop, 0, op->aop, 0, op->aop->size - 1, !aopInReg (result->aop, MSB32, A_IDX), false, true, true);
  
  if (!isRegDead (A_IDX, ic))
    _pop (PAIR_AF);
}

/*-----------------------------------------------------------------*/
/* genUminus - unary minus code generation                         */
/*-----------------------------------------------------------------*/
static void
genUminus (const iCode *ic)
{
  /* assign asmops */
  aopOp (IC_LEFT (ic), ic, FALSE, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE, FALSE);

  /* if both in bit space then special
     case */
  if (IC_RESULT (ic)->aop->type == AOP_CRY && IC_LEFT (ic)->aop->type == AOP_CRY)
    {
      wassertl (0, "Left and right are in bit space");
      goto release;
    }

  if (IS_FLOAT (operandType (IC_LEFT (ic))))
    genUminusFloat (ic, IC_RESULT (ic), IC_LEFT (ic));
  else
    genSub (ic, IC_RESULT (ic)->aop, ASMOP_ZERO, IC_LEFT (ic)->aop);

release:
  _G.preserveCarry = FALSE;
  freeAsmop (IC_LEFT (ic), NULL);
  freeAsmop (IC_RESULT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genMultOneChar - generates code for unsigned 8x8 multiplication */
/*-----------------------------------------------------------------*/
static void
genMultOneChar (const iCode * ic)
{
  asmop *result = ic->result->aop;

  if (ic->left->aop->size > 1 || ic->right->aop->size > 2)
    wassertl (0, "Large multiplication is handled through support function calls.");

  /* S1C88: native MLT computes HL <- L * A (unsigned 8x8->16, CE D8,
     2 bytes / 12 cycles; a MODEL1/3 instruction — present on the Pokémon
     Mini core). Replaces the shift-add loop, which needed a
     nonexistent DE pair and a B counter. Only A, L, H are touched. */

  /* A live with a non-operand value? Save it byte-granular — by rSurv the
     result is never in the saved set, so restoring after the result is
     written can't overwrite it. */
  const bool save_a = !isRegDead (A_IDX, ic) &&
    !aopInReg (ic->left->aop, 0, A_IDX) && !aopInReg (ic->right->aop, 0, A_IDX);
  if (save_a)
    {
      emit2 ("push a");
      cost2 (2, 11);
      _G.stack.pushed += 1;
    }

  /* Operands -> A and L (mlt is commutative): prefer the assignment that is
     already in place, and when one operand needs HL to be read (EXSTK and
     friends), make the register-resident one go to A so the HL-using load
     happens into L afterwards. A is loaded first; the L load must not
     scratch A. */
  {
    asmop *aop_a = ic->left->aop, *aop_l = ic->right->aop;
    if (aopInReg (aop_l, 0, A_IDX) || aopInReg (aop_a, 0, L_IDX) ||
        requiresHL (aop_a) && (aopInReg (aop_l, 0, L_IDX) || aopInReg (aop_l, 0, H_IDX)))
      {
        asmop *t = aop_a;
        aop_a = aop_l;
        aop_l = t;
      }
    cheapMove (ASMOP_A, 0, aop_a, 0, true);
    cheapMove (ASMOP_L, 0, aop_l, 0, false);
  }

  emit2 ("mlt");
  cost (2, 12);
  spillPair (PAIR_HL);
  spillPair (PAIR_BA);          /* A holds an operand byte now */

  genMove (result, ASMOP_HL, true, true, true);

  if (save_a)
    {
      emit2 ("pop a");
      cost2 (2, 10);
      _G.stack.pushed -= 1;
      spillPair (PAIR_BA);
    }
}


/*-----------------------------------------------------------------*/
/* genMult - generates code for multiplication                     */
/*-----------------------------------------------------------------*/
static void
genMult (iCode *ic)
{
  int val;
  /* If true then the final operation should be a subtract */
  bool active = false;
  bool byteResult;
  bool add_in_hl = false;
  int a_cost = 0, l_cost = 0;
  PAIR_ID pair;

  aopOp (IC_LEFT (ic), ic, FALSE, FALSE);
  aopOp (IC_RIGHT (ic), ic, FALSE, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE, FALSE);

  byteResult = (IC_RESULT (ic)->aop->size == 1);

  if (IC_LEFT (ic)->aop->size > 2 || IC_RIGHT (ic)->aop->size > 2)
    wassertl (0, "Large multiplication is handled through support function calls.");

  /* Swap left and right such that right is a literal */
  if (IC_LEFT (ic)->aop->type == AOP_LIT)
    {
      operand *t = IC_RIGHT (ic);
      IC_RIGHT (ic) = IC_LEFT (ic);
      IC_LEFT (ic) = t;
    }

  if (IC_RIGHT (ic)->aop->type != AOP_LIT)
    {
      genMultOneChar (ic);
      goto release;
    }

  wassertl (IC_RIGHT (ic)->aop->type == AOP_LIT, "Right must be a literal.");

  val = (int) ulFromVal (IC_RIGHT (ic)->aop->aopu.aop_lit);
  wassertl (val != 1, "Can't multiply by 1");

  // Try to use mlt.
  
no_mlt:

  

  /* S1C88: BA is the only 2nd 16-bit ALU pair (add hl, ba); there are no other scratch pairs. For the 8-bit accumulator loop the multiplicand
     lives in B (add/sub a, b): A is both the accumulator and BA's low byte, so
     it can't also hold the addend. The save/restore is decided after add_in_hl
     is known (below). */
  pair = PAIR_BA;

  /* Use 16-bit additions even for 8-bit result when the operands are in the right places. */
  if (byteResult)
    {
      if (!aopInReg (IC_LEFT (ic)->aop, 0, A_IDX))
        a_cost += ld_cost (ASMOP_A, 0, IC_LEFT (ic)->aop, 0, false);
      if (!aopInReg (IC_RESULT (ic)->aop, 0, A_IDX))
        a_cost += ld_cost (IC_RESULT (ic)->aop, 0, ASMOP_A, 0, false);
      if (IC_LEFT (ic)->aop->type != AOP_REG || IC_LEFT (ic)->aop->aopu.aop_reg[0]->rIdx != L_IDX)
        l_cost += ld_cost (ASMOP_L, 0, IC_LEFT (ic)->aop, 0, false);
      if (IC_RESULT (ic)->aop->type != AOP_REG || IC_RESULT (ic)->aop->aopu.aop_reg[0]->rIdx != L_IDX)
        l_cost += ld_cost (IC_RESULT (ic)->aop, 0, ASMOP_L, 0, false);
    }
  add_in_hl = (!byteResult || isPairDead (PAIR_HL, ic) && l_cost < a_cost);

  /* Save the live bytes this clobbers — byte-granular, because A and BA's low
     half are the same register: a pair-granular `push ba` could restore over a
     result byte landing in A (or save_a-restore could be skipped when only B is
     live). Every path clobbers A; B survives only the byte-loop-in-HL path
     (there the addend is A, and B's garbage high half feeds only H, which a
     byte result never reads). isRegDead() is false for bytes of the result, so
     the saved set is disjoint from the result and is restored after the result
     is written. */
  const bool save_a = !isRegDead (A_IDX, ic);
  const bool save_b = !(byteResult && add_in_hl) && !isRegDead (B_IDX, ic);
  if (save_a)
    {
      emit2 ("push a");
      cost2 (2, 11);
      _G.stack.pushed += 1;
    }
  if (save_b)
    {
      emit2 ("push b");
      cost2 (2, 11);
      _G.stack.pushed += 1;
    }

  if (byteResult)
    {
      if (add_in_hl)
        {
          /* Accumulate in L (HL is dead by the add_in_hl condition); the
             addend byte is A — BA's low half. */
          if (aopInReg (IC_LEFT (ic)->aop, 0, A_IDX))
            emit3 (A_LD, ASMOP_L, ASMOP_A);
          else
            {
              cheapMove (ASMOP_L, 0, IC_LEFT (ic)->aop, 0, true);
              emit3 (A_LD, ASMOP_A, ASMOP_L);
            }
        }
      else
        {
          /* Accumulate in A; the multiplicand is B. */
          cheapMove (ASMOP_A, 0, IC_LEFT (ic)->aop, 0, true);
          if (!aopInReg (IC_LEFT (ic)->aop, 0, B_IDX))
            emit3 (A_LD, ASMOP_B, ASMOP_A);
        }
    }
  else if (IC_LEFT (ic)->aop->size == 1 && !SPEC_USIGN (getSpec (operandType (IC_LEFT (ic)))))
    {
      /* Sign-extend the byte into both BA and HL. SEP is the native S1C88
         code-extension instruction: B <- sign of A. */
      cheapMove (ASMOP_A, 0, IC_LEFT (ic)->aop, 0, true);
      emit2 ("sep");
      cost (2, 3);
      emit3 (A_LD, ASMOP_L, ASMOP_A);
      emit3 (A_LD, ASMOP_H, ASMOP_B);
    }
  else
    {
      genMove (ASMOP_BA, IC_LEFT (ic)->aop, true, getPairId (IC_LEFT (ic)->aop) == PAIR_HL || isPairDead (PAIR_HL, ic), false);
      if (getPairId (IC_LEFT (ic)->aop) != PAIR_HL)
        {
          emit3 (A_LD, ASMOP_L, ASMOP_A);
          emit3 (A_LD, ASMOP_H, ASMOP_B);
        }
    }

  if (!add_in_hl) 
    {
      unsigned long long add, sub;
      int topbit, nonzero;

      wassert(!csdOfVal (&topbit, &nonzero, &add, &sub, IC_RIGHT (ic)->aop->aopu.aop_lit, 0xff));
      
      // If the leading digits of the cse are 1 0 -1 we can use 0 1 1 instead to reduce the number of shifts.
      if (topbit >= 2 && (add & (1ull << topbit)) && (sub & (1ull << (topbit - 2))))
        {
          add = (add & ~(1u << topbit)) | (3u << (topbit - 2));
          sub &= ~(1u << (topbit - 1));
          topbit--;
        }

      for (int bit = topbit - 1; bit >= 0; bit--)
        {
          emit3 (A_ADD, ASMOP_A, ASMOP_A);
          if ((add | sub) & (1ull << bit))
            emit3 (add & (1ull << bit) ? A_ADD : A_SUB, ASMOP_A, ASMOP_B);
        }
    }
  else // Don't try to use CSD for hl, since subtraction there is more expensive than addition.
    {
      unsigned int i = val;
      for (int count = 0; count < 16; count++)
        {
          if (count != 0 && active)
            emit3w (A_ADD, ASMOP_HL, ASMOP_HL);
          if (i & 0x8000u)
            {
              if (active)
                {
                  emit2 ("add hl, %s", _pairs[pair].name);
                  cost2 (1, 11);
                }
              active = true;
            }
          i <<= 1;
        }
      spillPair (PAIR_HL);
    }

  spillPair (PAIR_BA);  /* A (and possibly B) no longer hold what the cache thinks */

  genMove (IC_RESULT (ic)->aop, add_in_hl ? ASMOP_HL : ASMOP_A, true, add_in_hl || isPairDead (PAIR_HL, ic), true);

  /* Restore after the result is written: the saved bytes are live-past-the-ic
     non-result bytes, so they can't be overwritten by the result move (and the
     move may itself scratch A, which the pop then restores). */
  if (save_b)
    {
      emit2 ("pop b");
      cost2 (2, 10);
      _G.stack.pushed -= 1;
    }
  if (save_a)
    {
      emit2 ("pop a");
      cost2 (2, 10);
      _G.stack.pushed -= 1;
    }
  if (save_a || save_b)
    spillPair (PAIR_BA);

release:
  freeAsmop (IC_LEFT (ic), NULL);
  freeAsmop (IC_RIGHT (ic), NULL);
  freeAsmop (IC_RESULT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genDivMod - generates code for division / modulus via the       */
/* native DIV (CE D9): unsigned HL / A -> L quotient, H remainder. */
/* _hasNativeMulFor claims unsigned 8 / 8 and 16 / 8 (divisor an   */
/* unsigned char or a literal 1..255) plus signed 8 / 8 (negate-   */
/* fixup, not under --opt-code-size), so the claimed quotients     */
/* always fit (V never set). A zero divisor raises the hardware    */
/* zero-division exception (C UB). DIV leaves A (the divisor)      */
/* unchanged and clobbers HL — HLinst_ok keeps live non-operand    */
/* values out of HL across '/' and '%'.                            */
/*-----------------------------------------------------------------*/
static void
genDivMod (iCode *ic)
{
  operand *left = IC_LEFT (ic);   /* dividend */
  operand *right = IC_RIGHT (ic); /* divisor */
  operand *result = IC_RESULT (ic);

  aopOp (left, ic, FALSE, FALSE);
  aopOp (right, ic, FALSE, FALSE);
  aopOp (result, ic, TRUE, FALSE);

  wassertl (left->aop->size <= 2 && right->aop->size <= 2,
            "Wide division is handled through support function calls.");

  const bool two_byte = (left->aop->size == 2);
  /* The signed claim is 8 / 8 only; the C truncation semantics come out
     of the unsigned DIV by dividing magnitudes and applying the sign
     masks (quotient: dividend^divisor; remainder: the dividend's). */
  const bool sign = !SPEC_USIGN (getSpec (operandType (left)));
  wassertl (!(sign && two_byte), "Signed 16-bit division is handled through support function calls.");

  /* A live with a non-operand value? Save it byte-granular (the
     genMultOneChar scheme): by rSurv the result is never in the saved
     set, so restoring after the result is written can't overwrite it. */
  const bool save_a = !isRegDead (A_IDX, ic) &&
    !aopInReg (left->aop, 0, A_IDX) && !aopInReg (right->aop, 0, A_IDX);
  if (save_a)
    {
      emit2 ("push a");
      cost2 (2, 11);
      _G.stack.pushed += 1;
    }
  /* The 16-bit chain stages the dividend's low byte in B, and the
     signed path uses B as the sep sign-mask home. A live B is saved
     around the whole staging+work (the staging genMove may scratch B
     too); an operand byte living in B is read before the work
     overwrites it, so the restore is correct in every case. */
  const bool save_b = (two_byte || sign) && !isRegDead (B_IDX, ic);
  if (save_b)
    {
      emit2 ("push b");
      cost2 (2, 11);
      _G.stack.pushed += 1;
    }

  if (two_byte)
    {
      /* Stage dividend -> HL, divisor -> A, ordered so neither load
         clobbers the other operand. */
      const bool left_in_a = left->aop->regs[A_IDX] >= 0;
      if (requiresHL (right->aop) ||
          left_in_a && !aopInReg (right->aop, 0, A_IDX))
        {
          /* Dividend first (it may occupy A, or the divisor read needs
             HL): stage it, then fetch the divisor — bouncing the staged
             HL through the stack when the divisor read walks over it. */
          genMove (ASMOP_HL, left->aop, true, true, isPairDead (PAIR_IY, ic));
          if (requiresHL (right->aop))
            {
              emit2 ("push hl");
              cost2 (1, 11);
              _G.stack.pushed += 2;
              cheapMove (ASMOP_A, 0, right->aop, 0, true);
              emit2 ("pop hl");
              cost2 (1, 10);
              _G.stack.pushed -= 2;
            }
          else
            cheapMove (ASMOP_A, 0, right->aop, 0, true);
        }
      else
        {
          /* Divisor first (a no-op when it is already in A); the
             dividend load must then preserve A. */
          cheapMove (ASMOP_A, 0, right->aop, 0, true);
          genMove (ASMOP_HL, left->aop, false, true, isPairDead (PAIR_IY, ic));
        }

      /* The schoolbook base-256 chain: divide the high byte first, then
         the running remainder paired with the low byte. Both partial
         quotients fit in 8 bits (the remainder is < the divisor <= 255),
         so V is never set. DIV preserves A (the divisor) between steps. */
      emit3 (A_LD, ASMOP_B, ASMOP_L);   /* B = lo */
      emit3 (A_LD, ASMOP_L, ASMOP_H);   /* HL = 0:hi */
      cheapMove (ASMOP_H, 0, ASMOP_ZERO, 0, false);
      emit2 ("div");                    /* L = qhi, H = r */
      cost (2, 13);
      const bool need_qhi = (ic->op == '/' && result->aop->size > 1);
      if (need_qhi)
        {
          emit2 ("push l");
          cost2 (2, 11);
          _G.stack.pushed += 1;
        }
      emit3 (A_LD, ASMOP_L, ASMOP_B);   /* HL = r:lo */
      emit2 ("div");                    /* L = qlo, H = remainder */
      cost (2, 13);
      if (need_qhi)
        {
          emit2 ("pop h");              /* HL = qhi:qlo = the quotient */
          cost2 (2, 10);
          _G.stack.pushed -= 1;
        }
      spillPair (PAIR_HL);
      spillPair (PAIR_BA);              /* A holds the divisor, B the lo byte */

      goto result_move;
    }

  /* Stage dividend -> L, divisor -> A, 0 -> H, ordered so neither load
     clobbers the other operand. (For a signed division by a positive
     literal only the dividend is staged — the divisor's magnitude is
     loaded after the sign cluster has used A.) */
  if (sign && right->aop->type == AOP_LIT)
    cheapMove (ASMOP_L, 0, left->aop, 0, true);
  else
  {
    const bool left_in_a = aopInReg (left->aop, 0, A_IDX);
    const bool left_in_l = aopInReg (left->aop, 0, L_IDX);
    const bool left_in_h = aopInReg (left->aop, 0, H_IDX);
    const bool right_in_lh = aopInReg (right->aop, 0, L_IDX) || aopInReg (right->aop, 0, H_IDX);
    const bool right_needs_hl = requiresHL (right->aop);

    if (left_in_a && (right_in_lh || right_needs_hl) ||
        (left_in_l || left_in_h) && right_needs_hl)
      {
        /* The divisor load would clobber the dividend's home (A, or the
           HL the divisor read walks over): bounce the dividend through
           the stack, popping it straight into L. */
        emit2 (left_in_a ? "push a" : left_in_l ? "push l" : "push h");
        cost2 (2, 11);
        _G.stack.pushed += 1;
        cheapMove (ASMOP_A, 0, right->aop, 0, true);
        emit2 ("pop l");
        cost2 (2, 10);
        _G.stack.pushed -= 1;
      }
    else if (right_in_lh || right_needs_hl)
      {
        /* Divisor first (its home is about to be overwritten / it needs
           HL); the dividend load must then preserve A. */
        cheapMove (ASMOP_A, 0, right->aop, 0, true);
        cheapMove (ASMOP_L, 0, left->aop, 0, false);
      }
    else
      {
        /* Dividend first (may scratch A unless the divisor lives there),
           divisor last. */
        cheapMove (ASMOP_L, 0, left->aop, 0, !aopInReg (right->aop, 0, A_IDX));
        cheapMove (ASMOP_A, 0, right->aop, 0, true);
      }
  }
  if (!sign)
    {
      cheapMove (ASMOP_H, 0, ASMOP_ZERO, 0, false);

      emit2 ("div");
      cost (2, 13);
      spillPair (PAIR_HL);
      spillPair (PAIR_BA);      /* A holds the divisor now */
    }
  else
    {
      /* Branchless signed 8 / 8: |x| = (x ^ m) - m with m = sep's sign
         mask (B <- sign of A); divide the magnitudes; re-apply the
         result's sign the same way. Quotient sign mask = m(dividend) ^
         m(divisor), remainder mask = m(dividend) — C truncation-toward-
         zero semantics. For a positive literal divisor m(divisor) = 0,
         so both ops use m(dividend) and the divisor cluster is skipped. */
      const bool lit_pos = (right->aop->type == AOP_LIT);

      if (!lit_pos)
        {
          emit2 ("sep");                /* B = m(divisor) */
          cost (2, 3);
          emit3 (A_XOR, ASMOP_A, ASMOP_B);
          emit3 (A_SUB, ASMOP_A, ASMOP_B);
          emit3 (A_LD, ASMOP_H, ASMOP_A);   /* park |divisor| in H */
          if (ic->op == '/')
            {
              emit3 (A_LD, ASMOP_A, ASMOP_B);
              emit2 ("push a");         /* save m(divisor) */
              cost2 (2, 11);
              _G.stack.pushed += 1;
            }
        }

      emit3 (A_LD, ASMOP_A, ASMOP_L);
      emit2 ("sep");                    /* B = m(dividend) */
      cost (2, 3);
      emit3 (A_XOR, ASMOP_A, ASMOP_B);
      emit3 (A_SUB, ASMOP_A, ASMOP_B);
      emit3 (A_LD, ASMOP_L, ASMOP_A);   /* L = |dividend| */

      /* the result's sign mask -> stack */
      if (!lit_pos && ic->op == '/')
        {
          emit2 ("pop a");              /* A = m(divisor) */
          cost2 (2, 10);
          _G.stack.pushed -= 1;
          emit3 (A_XOR, ASMOP_A, ASMOP_B);  /* A = quotient mask */
        }
      else
        emit3 (A_LD, ASMOP_A, ASMOP_B);     /* A = m(dividend) */
      emit2 ("push a");
      cost2 (2, 11);
      _G.stack.pushed += 1;

      /* |divisor| -> A, 0 -> H, divide the magnitudes */
      if (lit_pos)
        cheapMove (ASMOP_A, 0, right->aop, 0, true);
      else
        emit3 (A_LD, ASMOP_A, ASMOP_H);
      cheapMove (ASMOP_H, 0, ASMOP_ZERO, 0, false);
      emit2 ("div");
      cost (2, 13);

      /* magnitude -> A, apply the sign mask */
      emit3 (A_LD, ASMOP_A, ic->op == '/' ? ASMOP_L : ASMOP_H);
      emit2 ("pop b");
      cost2 (2, 10);
      _G.stack.pushed -= 1;
      emit3 (A_XOR, ASMOP_A, ASMOP_B);
      emit3 (A_SUB, ASMOP_A, ASMOP_B);      /* A = the signed result */
      spillPair (PAIR_HL);
      spillPair (PAIR_BA);
    }

result_move:
  /* Unsigned: quotient in L for '/' (already qhi:qlo in HL for a wide
     16 / 8 quotient), remainder in H for '%'; all other result bytes
     are zero. Signed: the result byte is in A, wide results sep-extend.
     For a wide result build the full value in a pair first, then move
     it in one go (no partial-write aliasing with a result that itself
     lives in that pair). */
  if (sign)
    {
      if (result->aop->size == 1)
        cheapMove (result->aop, 0, ASMOP_A, 0, true);
      else
        {
          emit2 ("sep");        /* BA = the sign-extended result */
          cost (2, 3);
          spillPair (PAIR_BA);
          genMove (result->aop, ASMOP_BA, true, isPairDead (PAIR_HL, ic), isPairDead (PAIR_IY, ic));
        }
    }
  else if (result->aop->size == 1)
    cheapMove (result->aop, 0, ic->op == '/' ? ASMOP_L : ASMOP_H, 0, true);
  else
    {
      if (ic->op == '%')
        {
          emit3 (A_LD, ASMOP_L, ASMOP_H);
          cheapMove (ASMOP_H, 0, ASMOP_ZERO, 0, true);
        }
      else if (!two_byte)
        cheapMove (ASMOP_H, 0, ASMOP_ZERO, 0, true);
      genMove (result->aop, ASMOP_HL, true, true, isPairDead (PAIR_IY, ic));
    }

  if (save_b)
    {
      emit2 ("pop b");
      cost2 (2, 10);
      _G.stack.pushed -= 1;
      spillPair (PAIR_BA);
    }
  if (save_a)
    {
      emit2 ("pop a");
      cost2 (2, 10);
      _G.stack.pushed -= 1;
      spillPair (PAIR_BA);
    }

  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genDiv - generates code for division                            */
/*-----------------------------------------------------------------*/
static void
genDiv (iCode *ic)
{
  /* The divisions claimed in _hasNativeMulFor come here; everything
     else was converted to a support call by the middle end. */
  genDivMod (ic);
}

/*-----------------------------------------------------------------*/
/* genMod - generates code for modulus                             */
/*-----------------------------------------------------------------*/
static void
genMod (iCode *ic)
{
  genDivMod (ic);
}

/*-----------------------------------------------------------------*/
/* genIfxJump :- will create a jump depending on the ifx           */
/*-----------------------------------------------------------------*/
static void
genIfxJump (iCode * ic, char *jval)
{
  symbol *jlbl;
  const char *inst;

  /* if true label then we jump if condition
     supplied is true */
  if (IC_TRUE (ic))
    {
      jlbl = IC_TRUE (ic);
      if (!strcmp (jval, "a"))
        {
          emit3 (A_OR, ASMOP_A, ASMOP_A);
          cost2 (1, 4);
          inst = "NZ";
        }
      else if (!strcmp (jval, "z"))
        {
          inst = "Z";
        }
      else if (!strcmp (jval, "nz"))
        {
          inst = "NZ";
        }
      else if (!strcmp (jval, "c"))
        {
          inst = "C";
        }
      else if (!strcmp (jval, "nc"))
        {
          inst = "NC";
        }
      else if (!strcmp (jval, "m"))
        {
          inst = "M";
        }
      else if (!strcmp (jval, "p"))
        {
          inst = "P";
        }
      else if (!strcmp (jval, "nv"))	/* S1C88 overflow-clear */
        {
          inst = "NV";
        }
      else if (!strcmp (jval, "v"))	/* S1C88 overflow-set */
        {
          inst = "V";
        }
      else if (!strcmp (jval, "lt"))	/* S1C88 native signed less-than */
        {
          inst = "LT";
        }
      else
        {
          /* The buffer contains the bit on A that we should test */
          emit2 ("bit a, #0x%02x", 1u << atoi (jval));   // S1C88: bit reg,#mask
          cost2 (2, 8);
          inst = "NZ";
        }
    }
  else
    {
      /* false label is present */
      jlbl = IC_FALSE (ic);
      if (!strcmp (jval, "a"))
        {
          emit3 (A_OR, ASMOP_A, ASMOP_A);
          inst = "Z";
        }
      else if (!strcmp (jval, "z"))
        {
          inst = "NZ";
        }
      else if (!strcmp (jval, "nz"))
        {
          inst = "Z";
        }
      else if (!strcmp (jval, "c"))
        {
          inst = "NC";
        }
      else if (!strcmp (jval, "nc"))
        {
          inst = "C";
        }
      else if (!strcmp (jval, "m"))
        {
          inst = "P";
        }
      else if (!strcmp (jval, "p"))
        {
          inst = "M";
        }
      else if (!strcmp (jval, "nv"))	/* false of overflow-clear = overflow-set */
        {
          inst = "V";
        }
      else if (!strcmp (jval, "v"))	/* false of overflow-set = overflow-clear */
        {
          inst = "NV";
        }
      else if (!strcmp (jval, "lt"))	/* S1C88 native signed: false when >= */
        {
          inst = "GE";
        }
      else
        {
          /* The buffer contains the bit on A that we should test */
          emit2 ("bit a, #0x%02x", 1u << atoi (jval));   // S1C88: bit reg,#mask
          cost2 (2, 8);
          inst = "Z";
        }
    }
  /* S1C88 conditional long jump (jp cc -> assembler invert-and-skip) */
  if (!regalloc_dry_run)
    emit2 ("jp %s, !tlabel", inst, labelKey2num (jlbl->key));
  cost2 (3, 10.0f); // Assume either way equally likely.
}

#if DISABLED
static const char *
_getPairIdName (PAIR_ID id)
{
  return _pairs[id].name;
}
#endif

/* S1C88: emit an 8-bit ALU op `inst a, <src@soffset>` (sub/sbc/cp/and/or/...).
   The S1C88 8-bit ALU can only source A, B, memory or an
   immediate — never L or H. When src's byte is in L or H, route it through B.
   B is *always* saved with push/pop: in a multi-byte op the other (accumulator)
   operand may occupy B mid-operation even though isRegDead() reports B dead
   after the iCode, so an unconditional save is the only correct choice (e.g.
   `a & b` with a=BA, b=HL — the low byte's `ld b,l` would otherwise clobber
   a's high byte). A (the left operand) is preserved by construction; flags
   (incl. the carry chain) survive the push/pop. Any other operand is just
   emit3_o. */
static void
emit3_8alu (enum asminst inst, asmop *src, int soffset, const iCode *ic)
{
  (void) ic;
  if (!aopInReg (src, soffset, L_IDX) && !aopInReg (src, soffset, H_IDX))
    {
      emit3_o (inst, ASMOP_A, 0, src, soffset);
      return;
    }

  emit2 ("push b");
  cost2 (2, 11);
  cheapMove (ASMOP_B, 0, src, soffset, false);  /* ld b, l/h — A (left/acc) preserved */
  emit3 (inst, ASMOP_A, ASMOP_B);
  emit2 ("pop b");                              /* preserves flags + the carry chain */
  cost2 (2, 10);
}

/* S1C88: emit a shift/rotate `inst <aop[offset]>`. Shifts/rotates only target
   A, B, [HL] or [BR:ll] — never L, H, or [IX+d]. When the operand is anything
   else, route it through a shiftable scratch byte reg (A or B, whichever is not
   occupied by another byte of aop). `ld` is flag-neutral, so the carry chain
   across a multi-byte shift is preserved by the move in/out (and by push/pop of
   a live scratch). */
static void
emit3_shift (enum asminst inst, asmop *aop, int offset, const iCode *ic)
{
  if (aopInReg (aop, offset, A_IDX) || aopInReg (aop, offset, B_IDX))
    {
      emit3_o (inst, aop, offset, 0, 0);
      return;
    }

  /* Pick a shiftable scratch (A or B) not used by another byte of aop. */
  bool a_ok = (aop->regs[A_IDX] < 0);
  bool b_ok = (aop->regs[B_IDX] < 0);
  if (!a_ok && !b_ok)
    {
      UNIMPLEMENTED;          /* value occupies both A and B and spills into L/H */
      return;
    }
  bool use_a = a_ok && (isRegDead (A_IDX, ic) || !b_ok || !isRegDead (B_IDX, ic));
  asmop *scr = use_a ? ASMOP_A : ASMOP_B;
  bool dead = use_a ? isRegDead (A_IDX, ic) : isRegDead (B_IDX, ic);

  if (!dead)
    {
      emit2 (use_a ? "push a" : "push b");
      cost2 (2, 11);
    }
  cheapMove (scr, 0, aop, offset, false);   /* ld scr, <byte>  — flag-neutral */
  emit3 (inst, scr, 0);
  cheapMove (aop, offset, scr, 0, false);   /* ld <byte>, scr  — flag-neutral */
  if (!dead)
    {
      emit2 (use_a ? "pop a" : "pop b");
      cost2 (2, 10);
    }
}

/* S1C88: emit `inc`/`dec` of a single byte. The 8-bit INC/DEC target is only
   A, B, L, H, [HL] or [BR:ll] — never [IX+d]/[IY+d] or absolute memory. For an
   indexed/absolute operand, route through A (ld a,mem; inc/dec a; ld mem,a).
   PUSH/POP and 8-bit LD are all flag-neutral on the S1C88 (only `inc/dec a`
   touches Z), so the Z flag survives the store and any A save/restore — required
   by the multi-byte memory-increment carry-skip idiom (inc low; jr NZ; inc high). */
static void
emit3_incdec (enum asminst inst, asmop *aop, int offset, const iCode *ic)
{
  if (aopInReg (aop, offset, A_IDX) || aopInReg (aop, offset, B_IDX) ||
      aopInReg (aop, offset, L_IDX) || aopInReg (aop, offset, H_IDX))
    {
      emit3_o (inst, aop, offset, 0, 0);
      return;
    }

  bool a_dead = isRegDead (A_IDX, ic);
  if (!a_dead)
    {
      emit2 ("push a");
      cost2 (2, 11);
    }
  cheapMove (ASMOP_A, 0, aop, offset, false);   /* ld a, <byte>   — flag-neutral */
  emit3 (inst, ASMOP_A, 0);                      /* inc/dec a       — sets Z      */
  cheapMove (aop, offset, ASMOP_A, 0, false);    /* ld <byte>, a    — flag-neutral */
  if (!a_dead)
    {
      emit2 ("pop a");
      cost2 (2, 10);
    }
}

/* S1C88: BIT is `bit {a,b,[hl],[br:ll]},#nn` — a logical AND-with-mask that sets
   Z = !(operand & mask). A `bit n,r`-style test (bit n of r) maps to
   `bit r,#(1<<n)`: for a single-bit mask the Z result is identical. The operand
   must be A or B (we don't fast-path [HL]); an L/H/[ix+d]/abs operand is copied
   into a free byte reg (A or B) first. LD/PUSH/POP are flag-neutral, so the Z
   set by `bit` reaches a following branch. */
static void
emitBitTest (int bitno, asmop *aop, int offset, const iCode *ic)
{
  unsigned mask = 1u << (bitno & 7);
  if (aopInReg (aop, offset, A_IDX) || aopInReg (aop, offset, B_IDX))
    {
      cost2 (2, 8);
      if (!regalloc_dry_run)   /* aopGet asserts !regalloc_dry_run */
        emit2 ("bit %s, #0x%02x", aopGet (aop, offset, false), mask);
      return;
    }

  bool a_used = aop->regs[A_IDX] >= 0;
  bool b_used = aop->regs[B_IDX] >= 0;
  if (a_used && b_used)
    {
      UNIMPLEMENTED;            /* value spans A and B and spills into L/H */
      return;
    }
  asmop *scr = !a_used ? ASMOP_A : ASMOP_B;
  bool save = !(scr == ASMOP_A ? isRegDead (A_IDX, ic) : isRegDead (B_IDX, ic));
  if (save)
    {
      emit2 (scr == ASMOP_A ? "push a" : "push b");
      cost2 (2, 11);
    }
  cheapMove (scr, 0, aop, offset, false);          /* ld scr, <byte>  — flag-neutral */
  emit2 ("bit %s, #0x%02x", scr == ASMOP_A ? "a" : "b", mask);
  cost2 (2, 8);
  if (save)
    {
      emit2 (scr == ASMOP_A ? "pop a" : "pop b");
      cost2 (2, 10);
    }
}

/** Generic compare for > or <
 */
static void
genCmp (operand * left, operand * right, operand * result, iCode * ifx, int sign, const iCode * ic)
{
  int size, offset = 0;
  unsigned long long lit = 0ull;
  bool result_in_carry = FALSE;
  int a_always_byte = -1;
  bool started = false;
  bool inv = false;
  bool signed_native = false;	/* S1C88: branch on the native signed condition (jrs LT/GE) */
  bool signed_native_bool = false;	/* S1C88: materialise a bool from the native signed condition */

  /* if left & right are bit variables */
  if (left->aop->type == AOP_CRY && right->aop->type == AOP_CRY)
    {
      /* Can't happen on the S1C88 */
      wassertl (0, "Tried to compare two bits");
    }
  else
    {
      /* Do a long subtract of right from left. */
      size = max (left->aop->size, right->aop->size);

      

      // Preserve A if necessary
      if (ifx && size == 1 && !sign && aopInReg (left->aop, 0, A_IDX) && !isRegDead (A_IDX, ic) &&
        (right->aop->type == AOP_LIT || right->aop->type == AOP_REG && right->aop->aopu.aop_reg[offset]->rIdx != IYL_IDX && right->aop->aopu.aop_reg[offset]->rIdx != IYH_IDX || right->aop->type == AOP_STK))
        {
          emit3_8alu (A_CP, right->aop, 0, ic);   // S1C88: route L/H operand through B
          result_in_carry = true;
          goto release;
        }
      else if (ifx && size == 1 && !sign && aopInReg (right->aop, 0, A_IDX) && left->aop->type == AOP_LIT && ullFromVal (left->aop->aopu.aop_lit) < 255)
        {
          emit2 ("cp a, !immedbyte", (unsigned int)(ullFromVal (left->aop->aopu.aop_lit) + 1));
          cost2 (2, 7);
          result_in_carry = true;
          inv = true;
          goto release;
        }
        
      if (right->aop->type == AOP_LIT && !ullFromVal (right->aop->aopu.aop_lit)) // special case: comparison to 0. Do it here early.
        {
          if (!sign)
            {
              /* No sign so it's always false */
              emit3 (A_CP, ASMOP_A, ASMOP_A);
              result_in_carry = TRUE;
            }
          else
            {
              if (!(result->aop->type == AOP_CRY && result->aop->size) && ifx &&
                (left->aop->type == AOP_REG || left->aop->type == AOP_STK))
                {
                  emitBitTest (7, left->aop, left->aop->size - 1, ic);   // S1C88: bit reg,#0x80 (route L/H via A/B)
                  if (left->aop->type == AOP_REG)
                    cost2 (2, 8);
                  else
                    cost2 (4, 20);
                  genIfxJump (ifx, "nz");
                  return;
                }
             /* Just load in the top most bit */
             cheapMove (ASMOP_A, 0, left->aop, left->aop->size - 1, true);
             if (!(result->aop->type == AOP_CRY && result->aop->size) && ifx)
               {
                 genIfxJump (ifx, "7");
                 return;
               }
             else
               {
                  if (ifx)
                    {
                      genIfxJump (ifx, "nc");
                      return;
                    }
                  result_in_carry = FALSE;
                }
            }
          goto release;
        }

      

      

      if (right->aop->type == AOP_LIT)
        {
          lit = ullFromVal (right->aop->aopu.aop_lit);

          while (!((lit >> (offset * 8)) & 0xffull))
            {
              size--;
              offset++;
            }

          /* S1C88 native 16-bit compare against an immediate. cp {ba,hl},#imm
             sets Z C V N in one instruction, replacing both the unsigned byte
             chain and — crucially — the signed sign-mapping (xor #0x80 /
             rla / ccf / rra), which uses the illegal acc-rotates. Needs the
             whole 16-bit value in an ALU pair (no low bytes stripped). */
          if (size == 2 && offset == 0 &&
              !(result->aop->type == AOP_CRY && result->aop->size))
            {
              PAIR_ID lp = aluPairId (left->aop, 0);
              /* S1C88: if the operand isn't already in an ALU pair (e.g. it's on
                 the stack), load a copy into a dead HL so we can still use the
                 native `cp hl,#imm` + jrs LT/GE — instead of falling through to
                 the illegal ccf sign-flip below. */
              if (lp == PAIR_INVALID && !requiresHL (left->aop) && isPairDead (PAIR_HL, ic))
                {
                  fetchPair (PAIR_HL, left->aop);
                  lp = PAIR_HL;
                }
              if (lp != PAIR_INVALID)
                {
                  emit2 ("cp %s, !immedword", _pairs[lp].name, (unsigned) (lit & 0xffffu));
                  cost2 (3, 10);
                  started = true;
                  size = 0;
                  offset = 2;
                  goto fix;
                }
            }

          if (sign)             /* Map signed operands to unsigned ones. This pre-subtraction workaround to lack of signed comparison is cheaper than the post-subtraction one at fix. */
            {
              /* S1C88: do a plain byte-wise sub/sbc chain against the immediate
                 (legal: 8-bit ALU source is A + #imm) and branch on the native
                 S xor V via signed_native at fix: — no xor#0x80 / rl a / ccf /
                 rr a sign-flip (ccf is illegal on the S1C88). This covers the
                 long/odd-size cases the native `cp pair,#imm` above can't take.
                 The rare AOP_CRY (bit) result still needs the old sign-mapping. */
              if (!(result->aop->type == AOP_CRY && result->aop->size))
                {
                  cheapMove (ASMOP_A, 0, left->aop, offset, true);
                  emit2 ("sub a, !immedbyte", (unsigned) ((lit >> (offset * 8)) & 0xff));
                  cost2 (2, 7);
                  size--;
                  offset++;
                  while (size--)
                    {
                      cheapMove (ASMOP_A, 0, left->aop, offset, true);
                      emit2 ("sbc a, !immedbyte", (unsigned) ((lit >> (offset++ * 8)) & 0xff));
                      cost2 (2, 7);
                    }
                  started = true;
                  goto fix;
                }



              cheapMove (ASMOP_A, 0, left->aop, offset, true);
              if (size == 1)
                {
                  emit2 ("xor a, !immedbyte", 0x80u);
                  cost2 (2, 7);
                }
              emit2 ("sub a, !immedbyte", (unsigned)(((lit >> (offset * 8)) & 0xff) ^ (size == 1 ? 0x80 : 0x00)));
              cost2 (2, 7);
              size--;
              offset++;

              while (size--)
                {
                  cheapMove (ASMOP_A, 0, left->aop, offset, true);
                  if (!size)
                    {
                      emit3 (A_RL, ASMOP_A, 0);
                      emit2 ("ccf");
                      cost2 (1, 4);
                      emit3 (A_RR, ASMOP_A, 0);
                    }
                  /* Subtract through, propagating the carry */
                  emit2 ("sbc a, !immedbyte", (unsigned)(((lit >> (offset++ * 8)) & 0xff) ^ (size ? 0x00 : 0x80)));
                  cost2 (2, 7);
                }
              result_in_carry = true;
              goto release;
            }
        }

      /* S1C88 native 16-bit compare. The core has a true 16-bit CP that sets
         Z C V N, plus native signed branches (jrs LT/GE test S^V), so a single
         `cp <pair>,<pair>` replaces the byte-wise sub/sbc idiom — which is
         illegal here anyway (8-bit ALU source must be A or B, never L/H). We
         need both operands in the two ALU pairs (BA, HL). The fix/release
         blocks below then branch on S/V directly (signed) or the carry
         (unsigned), for both the ifx and the boolean-materialization cases. */
      if (size == 2 && offset == 0 &&
          !(result->aop->type == AOP_CRY && result->aop->size) &&
          left->aop->type != AOP_LIT && right->aop->type != AOP_LIT)
        {
          PAIR_ID lp = aluPairId (left->aop, 0);
          PAIR_ID rp = aluPairId (right->aop, 0);
          PAIR_ID pLeft = PAIR_INVALID, pRight = PAIR_INVALID;
          bool ok = true;

          if (lp != PAIR_INVALID && rp != PAIR_INVALID && lp != rp)
            { pLeft = lp; pRight = rp; }
          else if (lp != PAIR_INVALID && rp == PAIR_INVALID)
            { pLeft = lp; pRight = (lp == PAIR_HL) ? PAIR_BA : PAIR_HL; }
          else if (rp != PAIR_INVALID && lp == PAIR_INVALID)
            { pRight = rp; pLeft = (rp == PAIR_HL) ? PAIR_BA : PAIR_HL; }
          else
            ok = false;            /* neither in an ALU pair — leave to the byte path */

          /* The pair we load an operand into must be free, and an operand we
             load must not itself need HL (would clash with the other pair). */
          if (ok && lp != pLeft && (!isPairDead (pLeft, ic) || requiresHL (left->aop)))
            ok = false;
          if (ok && rp != pRight && (!isPairDead (pRight, ic) || requiresHL (right->aop)))
            ok = false;

          if (ok)
            {
              if (lp != pLeft)
                genMove (pLeft == PAIR_HL ? ASMOP_HL : ASMOP_BA, left->aop, false, false, false);
              if (rp != pRight)
                genMove (pRight == PAIR_HL ? ASMOP_HL : ASMOP_BA, right->aop, false, false, false);
              spillPair (pLeft);
              spillPair (pRight);
              emit2 ("cp %s, %s", _pairs[pLeft].name, _pairs[pRight].name);
              cost2 (2, 15);
              started = true;
              size = 0;
              offset = 2;
              goto fix;
            }
        }

      if (left->aop->type == AOP_LIT && !aopInReg (right->aop, offset, A_IDX) && isRegDead (A_IDX, ic))
        {
          bool pushed_hl = false;
          if (byteOfVal (left->aop->aopu.aop_lit, offset) == 0x00)
            emit3 (A_XOR, ASMOP_A, ASMOP_A);
          else
            cheapMove (ASMOP_A, 0, left->aop, offset, true);
          if (requiresHL (right->aop) && right->aop->type != AOP_REG && !isPairDead (PAIR_HL, ic))
            {
              _push (PAIR_HL);
              pushed_hl = true;
            }
          if (size > 1)
            {
              emit3_8alu (A_CP, right->aop, offset, ic);   // S1C88: route L/H operand through B
              started = true;
              a_always_byte = byteOfVal (left->aop->aopu.aop_lit, offset);
            }
          else
            emit3_8alu (A_SUB, right->aop, offset, ic);   // S1C88: route L/H operand through B
          if (pushed_hl)
            _pop (PAIR_HL);
          size--;
          offset++;
        }

      /* Subtract through, propagating the carry */
      while (size)
        {
          bool left_already_in_a = (left->aop->type == AOP_LIT && byteOfVal (left->aop->aopu.aop_lit, offset) == a_always_byte);

          if (started && !sign && aopIsLitVal (left->aop, offset, size, 0) && aopIsLitVal (right->aop, offset, size, 0)) // Skip leading zeroes.
            {
              offset += size;
              size = 0;
            }
          else if (size >= 2 && (!sign || size > 2) && !left_already_in_a && isPairDead (PAIR_HL, ic) && isPairDead (PAIR_BA, ic) &&
            left->aop->regs[A_IDX] < offset + 1 && left->aop->regs[B_IDX] < offset + 1 &&
            (getPartPairId (left->aop, offset) == PAIR_HL || left->aop->type == AOP_LIT || left->aop->type == AOP_IMMD || left->aop->type == AOP_HL || left->aop->type == AOP_IY) && (right->aop->type == AOP_LIT || right->aop->type == AOP_IMMD || right->aop->type == AOP_HL || right->aop->type == AOP_IY))
            {
              /* S1C88: the 16-bit borrow chain runs through BA. */
              genMove_o (ASMOP_BA, 0, right->aop, offset, 2, true, getPartPairId (left->aop, offset) != PAIR_HL, true, !offset);
              genMove_o (ASMOP_HL, 0, left->aop, offset, 2, false, true, true, !offset);
              if (!started)
                emit3 (A_CP, ASMOP_A, ASMOP_A);
              emit3w (A_SBC, ASMOP_HL, ASMOP_BA);
              spillPair (PAIR_HL);
              started = true;
              size -= 2;
              offset += 2;
            }
          else if (right->aop->type == AOP_STL &&
            isRegDead (B_IDX, ic) && left->aop->regs[B_IDX] <= offset)
            {
              if (!left_already_in_a)
                cheapMove (ASMOP_A, 0, left->aop, offset, true);
              cheapMove (ASMOP_B, 0, right->aop, offset, false);
              a_always_byte = -1;
              emit3_o (started ? A_SBC : A_SUB, ASMOP_A, 0, ASMOP_B, 0);
              started = true;
              size--;
              offset++;
            }
          else if (right->aop->type != AOP_STL && !aopInReg (right->aop, offset, A_IDX))
            {
              if (!left_already_in_a)
                cheapMove (ASMOP_A, 0, left->aop, offset, true);
              a_always_byte = -1;
              emit3_8alu (started ? A_SBC : A_SUB, right->aop, offset, ic);
              started = true;
              size--;
              offset++;
            }
          else
            {
              UNIMPLEMENTED;
              size--;
              offset++;
            }
        }

fix:
      /* Signed compare. The S1C88 has native signed-condition branches, so the
         common cases need no workaround; only an AOP_CRY (bit) result still needs
         a sign-correction fixup. */
      if (sign)
        {
          {
              /* The S1C88 has native signed-condition branches (jrs LT/GE test
                 N xor V), and the byte/word subtract above already left N and V
                 set correctly for the signed compare — so we branch on those
                 flags directly and skip the sign-correction entirely.
                 For an ifx the result is a conditional branch (signed_native ->
                 genIfxJump(ifx, "lt")); for a boolean result we materialise 0/1
                 from the same condition (signed_native_bool, handled in the
                 release block).  Only the rare AOP_CRY (bit) result still needs
                 the sign-correction fixup: when the subtract overflowed (V) the
                 high byte's sign bit is inverted vs. the true ordering, so flip
                 it — skipped via NV (overflow-clear) when there was no overflow. */
              if (!(result->aop->type == AOP_CRY && result->aop->size))
                {
                  if (ifx)
                    signed_native = true;
                  else
                    signed_native_bool = true;
                }
              else if (!regalloc_dry_run)
                {
                  symbol *tlbl = newiTempLabel (NULL);
                  emit2 ("jp NV, !tlabel", labelKey2num (tlbl->key));
                  cost2 (2, 12); // Assume no overflow.
                  emit2 ("xor a, !immedbyte", 0x80u);
                  cost (2, 0); // Assume no overflow.
                  emitLabelSpill (tlbl);
                }
              result_in_carry = FALSE;
            }
        }
      else
        result_in_carry = true;
    }

release:
  if (result->aop->type == AOP_CRY && result->aop->size)
    {
      wassert (!inv);
      if (!result_in_carry)
        {
          /* Shift the sign bit up into carry */
          emit3 (A_RLC, ASMOP_A, 0);
        }
      outBitC (result);
    }
  else
    {
      /* if the result is used in the next
         ifx conditional branch then generate
         code a little differently */
      if (ifx)
        {
          if (!result_in_carry)
            {
              wassert (!inv);
              if (signed_native)
                genIfxJump (ifx, "lt");		/* S1C88 native signed branch (jrs LT/GE) */
              else genIfxJump (ifx, "m");
            }
          else
            genIfxJump (ifx, inv ? "nc" : "c");
        }
      else if (signed_native_bool)
        {
          /* S1C88: materialise the boolean from the native signed condition.
             The compare above set S and V; jp LT/GE (-> jrs) selects 1 vs 0.
             ld/xor leave those flags intact until the branch. */
          symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
          emit2 ("ld a, !immedbyte", 1u);
          cost2 (2, 7);
          if (!regalloc_dry_run)
            emit2 ("jp LT, !tlabel", labelKey2num (tlbl->key));
          cost2 (3, 10.0f);
          emit3 (A_XOR, ASMOP_A, ASMOP_A);
          if (!regalloc_dry_run)
            emitLabel (tlbl);
          outAcc (result);
        }
      else
        {
          wassert (!inv);
          if (!result_in_carry)
            {
              /* Shift the sign bit up into carry */
              emit3 (A_RLC, ASMOP_A, 0);
            }
          outBitC (result);
        }
      /* leave the result in acc */
    }
}

/*-----------------------------------------------------------------*/
/* genCmpGt :- greater than comparison                             */
/*-----------------------------------------------------------------*/
static void
genCmpGt (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  sym_link *letype, *retype;
  int sign;

  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);

  sign = 0;
  if (IS_SPEC (operandType (left)) && IS_SPEC (operandType (right)))
    {
      letype = getSpec (operandType (left));
      retype = getSpec (operandType (right));
      sign = !(SPEC_USIGN (letype) | SPEC_USIGN (retype));
    }

  /* assign the asmops */
  aopOp (left, ic, FALSE, FALSE);
  aopOp (right, ic, FALSE, FALSE);
  aopOp (result, ic, TRUE, FALSE);

  if (!IS_BITVAR (operandType (left)) && (!IS_BITINT (operandType (left)) || !(SPEC_BITINTWIDTH (operandType (left)) % 8)) &&
    !IS_BITVAR (operandType (right)) && (!IS_BITINT (operandType (right)) || !(SPEC_BITINTWIDTH (operandType (right)) % 8)) &&
    aopIsLitBit (left->aop, left->aop->size * 8 - 1, false) && aopIsLitBit (right->aop, right->aop->size * 8 - 1, false))
    sign = 0;

  if (max (left->aop->size, right->aop->size) > 1 && (couldDestroyCarry (left->aop) || couldDestroyCarry (right->aop)))
    {
      if ((requiresHL (IC_RESULT (ic)->aop) && IC_RESULT (ic)->aop->type != AOP_REG || requiresHL (left->aop) && left->aop->type != AOP_REG || requiresHL (right->aop) && right->aop->type != AOP_REG) &&
        (left->aop->regs[L_IDX] > 0 || left->aop->regs[H_IDX] > 0 || right->aop->regs[L_IDX] > 0 || right->aop->regs[H_IDX] > 0) || !isPairDead (PAIR_HL, ic))
        UNIMPLEMENTED;
      else
        setupToPreserveCarry (result->aop, left->aop, right->aop, ic);
    }

  genCmp (right, left, result, ifx, sign, ic);

  _G.preserveCarry = FALSE;
  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genCmpLt - less than comparisons                                */
/*-----------------------------------------------------------------*/
static void
genCmpLt (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  sym_link *letype, *retype;
  int sign;

  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);

  sign = 0;
  if (IS_SPEC (operandType (left)) && IS_SPEC (operandType (right)))
    {
      letype = getSpec (operandType (left));
      retype = getSpec (operandType (right));
      sign = !(SPEC_USIGN (letype) | SPEC_USIGN (retype));
    }

  /* assign the asmops */
  aopOp (left, ic, FALSE, FALSE);
  aopOp (right, ic, FALSE, FALSE);
  aopOp (result, ic, TRUE, FALSE);

  if (!IS_BITVAR (operandType (left)) && (!IS_BITINT (operandType (left)) || !(SPEC_BITINTWIDTH (operandType (left)) % 8)) &&
    !IS_BITVAR (operandType (right)) && (!IS_BITINT (operandType (right)) || !(SPEC_BITINTWIDTH (operandType (right)) % 8)) &&
    aopIsLitBit (left->aop, left->aop->size * 8 - 1, false) && aopIsLitBit (right->aop, right->aop->size * 8 - 1, false))
    sign = 0;

  if (max (left->aop->size, right->aop->size) > 1 && (couldDestroyCarry (left->aop) || couldDestroyCarry (right->aop)))
    {
      if ((requiresHL (IC_RESULT (ic)->aop) && IC_RESULT (ic)->aop->type != AOP_REG || requiresHL (left->aop) && left->aop->type != AOP_REG || requiresHL (right->aop) && right->aop->type != AOP_REG) &&
        (left->aop->regs[L_IDX] > 0 || left->aop->regs[H_IDX] > 0 || right->aop->regs[L_IDX] > 0 || right->aop->regs[H_IDX] > 0) || !isPairDead (PAIR_HL, ic))
        UNIMPLEMENTED;
      else
        setupToPreserveCarry (result->aop, left->aop, right->aop, ic);
    }

  genCmp (left, right, result, ifx, sign, ic);

  _G.preserveCarry = FALSE;
  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* gencjneshort - compare and jump if not equal                    */
/* returns pair that still needs to be popped                      */
/*-----------------------------------------------------------------*/
static PAIR_ID
gencjneshort (operand *left, operand *right, symbol *lbl, const iCode *ic)
{
  int size = max (left->aop->size, right->aop->size);
  int offset = 0;
  bool a_result = false;

  /* Swap the left and right if it makes the computation easier */
  if (left->aop->type == AOP_LIT || aopInReg (right->aop, 0, A_IDX))
    {
      operand *t = right;
      right = left;
      left = t;
    }

  /* Non-destructive compare */
  if (aopInReg (left->aop, 0, A_IDX) && !isRegDead (A_IDX, ic) &&
    (right->aop->type == AOP_LIT ||
    right->aop->type == AOP_REG && (HAS_IYL_INST || right->aop->aopu.aop_reg[offset]->rIdx != IYL_IDX && right->aop->aopu.aop_reg[offset]->rIdx != IYH_IDX) ||
    right->aop->type == AOP_STK))
    {
      bool pushed_hl = false;
      if(requiresHL (right->aop) && right->aop->type != AOP_REG && !isPairDead(PAIR_HL, ic))
        {
          _push (PAIR_HL);
          pushed_hl = true;
        }
        
      if (right->aop->type == AOP_LIT && !byteOfVal (right->aop->aopu.aop_lit, 0))
        emit3 (A_OR, ASMOP_A, ASMOP_A);
      else
        emit3_8alu (A_CP, right->aop, 0, ic);   // S1C88: route L/H operand through B

      if (pushed_hl)
        _pop (PAIR_HL);

      if (!regalloc_dry_run)
        emit2 ("jp NZ, !tlabel", labelKey2num (lbl->key));
      cost2 (3, 10.0f); // Assume both branches equally likely, cp not optimzed into jr.
    }
  /* if the right side is a literal then anything goes */
  else if (right->aop->type == AOP_LIT)
    {
      while (size--)
        {
          bool pushed_hl = false;
          bool next_zero = size && !byteOfVal (right->aop->aopu.aop_lit, offset + 1);

          if(requiresHL (left->aop) && left->aop->type != AOP_REG && !isPairDead(PAIR_HL, ic))
            {
              _push (PAIR_HL);
              pushed_hl = true;
            }

          // Test for 0 can be done more efficiently using or
          if (!byteOfVal (right->aop->aopu.aop_lit, offset))
            {
              if (!a_result)
                {
                  cheapMove (ASMOP_A, 0, left->aop, offset, true);
                  emit3 (A_OR, ASMOP_A, ASMOP_A);
                }
              else
                emit3_8alu (A_OR, left->aop, offset, ic);

              a_result = TRUE;
            }
          else if ((aopInReg (left->aop, 0, A_IDX) && isRegDead (A_IDX, ic) ||
            left->aop->type == AOP_REG && left->aop->aopu.aop_reg[offset]->rIdx != IYL_IDX && left->aop->aopu.aop_reg[offset]->rIdx != IYH_IDX && !bitVectBitValue (ic->rSurv, left->aop->aopu.aop_reg[offset]->rIdx)) &&
            byteOfVal (right->aop->aopu.aop_lit, offset) == 0x01 && !next_zero)
            {
              emit3_o (A_DEC, left->aop, offset, 0, 0);
              a_result = aopInReg (left->aop, 0, A_IDX);
            }
          else if (isRegDead (A_IDX, ic) && left->aop->regs[A_IDX] < offset && size && byteOfVal (right->aop->aopu.aop_lit, offset) == 0xff &&
            (left->aop->type == AOP_REG || left->aop->type == AOP_STK) &&
            byteOfVal (right->aop->aopu.aop_lit, offset) == byteOfVal (right->aop->aopu.aop_lit, offset + 1))
            {
              cheapMove (ASMOP_A, 0, left->aop, offset, true);
              while (byteOfVal (right->aop->aopu.aop_lit, offset + 1) == 0xff && size)
                {
                  emit3_8alu (A_AND, left->aop, ++offset, ic);
                  size--;
                }
              emit3 (A_INC, ASMOP_A, 0);
              next_zero = size && !byteOfVal (right->aop->aopu.aop_lit, offset + 1);
              a_result = true;
            }
          else if ((aopInReg (left->aop, 0, A_IDX) && isRegDead (A_IDX, ic) ||
            left->aop->type == AOP_REG && left->aop->aopu.aop_reg[offset]->rIdx != IYL_IDX && left->aop->aopu.aop_reg[offset]->rIdx != IYH_IDX && !bitVectBitValue (ic->rSurv, left->aop->aopu.aop_reg[offset]->rIdx)) &&
            byteOfVal (right->aop->aopu.aop_lit, offset) == 0xff && !next_zero)
            {
              emit3_o (A_INC, left->aop, offset, 0, 0);
              a_result = aopInReg (left->aop, 0, A_IDX);
            }
          else
            {
              cheapMove (ASMOP_A, 0, left->aop, offset, true);

              if (byteOfVal (right->aop->aopu.aop_lit, offset) == 0x01)
                emit3 (A_DEC, ASMOP_A, 0);
              else if (byteOfVal (right->aop->aopu.aop_lit, offset) == 0xff)
                emit3 (A_INC, ASMOP_A, 0);
              else
                emit3_o (A_SUB, ASMOP_A, 0, right->aop, offset);

              a_result = true;
            }

          if (pushed_hl)
            _pop (PAIR_HL);

          // Only emit jump now if there is no following test for 0 (which would just or to a current result in a)
          if (!(next_zero && a_result))
            {
              if (!regalloc_dry_run)
                emit2 ("jp NZ, !tlabel", labelKey2num (lbl->key));
              cost2 (3, 10.0f); // Assume both branches equally likely, cp not optimzed into jr.
            }
          offset++;
        }
    }
  /* if the right side is in a register or
     pointed to by HL, IX or IY */
  else if (right->aop->type == AOP_REG ||
           right->aop->type == AOP_HL ||
           right->aop->type == AOP_IY ||
           right->aop->type == AOP_STK ||
           right->aop->type == AOP_EXSTK ||
           right->aop->type == AOP_IMMD ||
           AOP_IS_PAIRPTR (right, PAIR_HL) || AOP_IS_PAIRPTR (right, PAIR_IX) || AOP_IS_PAIRPTR (right, PAIR_IY))
    {
      while (size--)
        {
          bool hl_dead = isRegDead (HL_IDX, ic) && left->aop->regs[L_IDX] < offset && left->aop->regs[H_IDX] < offset && right->aop->regs[L_IDX] < offset && right->aop->regs[H_IDX] < offset;
          bool iy_dead = isRegDead (IY_IDX, ic) && left->aop->regs[IYL_IDX] < offset && left->aop->regs[IYH_IDX] < offset && right->aop->regs[IYL_IDX] < offset && right->aop->regs[IYH_IDX] < offset;

          if (aopInReg (right->aop, offset, A_IDX) || aopInReg (right->aop, offset, HL_IDX))
            {
              operand *t = right;
              right = left;
              left = t;
            }

          /* S1C88 native 16-bit equality compare. A 16-bit `cp` on the two ALU
             pairs (HL, BA) sets Z iff equal, replacing the `cp a,a; sbc hl,
             <bc/de>` idiom — BC/DE aren't S1C88 ALU pairs, so `sbc hl,bc` is
             illegal. Equality is symmetric, so we put the two 16-bit chunks in
             HL and BA (whichever way needs the fewest moves) and compare. At
             least one chunk must already be in an ALU pair; the other is loaded
             into the (disjoint) free pair. One pair is always BA, so A is part
             of an operand throughout — loads must not use A as scratch. */
          if (size >= 1)
            {
              PAIR_ID lp = aluPairId (left->aop, offset);
              PAIR_ID rp = aluPairId (right->aop, offset);
              PAIR_ID pLeft = PAIR_INVALID, pRight = PAIR_INVALID;
              bool ok = true;

              if (lp != PAIR_INVALID && rp != PAIR_INVALID && lp != rp)
                { pLeft = lp; pRight = rp; }
              else if (lp != PAIR_INVALID)
                { pLeft = lp; pRight = (lp == PAIR_HL) ? PAIR_BA : PAIR_HL; }
              else if (rp != PAIR_INVALID)
                { pRight = rp; pLeft = (rp == PAIR_HL) ? PAIR_BA : PAIR_HL; }
              else
                ok = false;            /* neither in an ALU pair — leave to the byte path */

              /* The pair we load an operand into must be free, and an operand we
                 load into HL must not itself need HL to be addressed. */
              if (ok && lp != pLeft && (!isPairDead (pLeft, ic) || pLeft == PAIR_HL && requiresHL (left->aop)))
                ok = false;
              if (ok && rp != pRight && (!isPairDead (pRight, ic) || pRight == PAIR_HL && requiresHL (right->aop)))
                ok = false;

              if (ok)
                {
                  if (lp != pLeft)
                    genMove_o (pLeft == PAIR_HL ? ASMOP_HL : ASMOP_BA, 0, left->aop, offset, 2, false, pLeft == PAIR_HL, true, true);
                  if (rp != pRight)
                    genMove_o (pRight == PAIR_HL ? ASMOP_HL : ASMOP_BA, 0, right->aop, offset, 2, false, pRight == PAIR_HL, true, true);
                  spillPair (pLeft);
                  spillPair (pRight);
                  emit2 ("cp %s, %s", _pairs[pLeft].name, _pairs[pRight].name);
                  cost2 (2, 15);
                  if (!regalloc_dry_run)
                    emit2 ("jp NZ, !tlabel", labelKey2num (lbl->key));
                  cost2 (3, 10.0f); // Assume both branches equally likely, jp not optimzed into jr.
                  offset += 2;
                  size--;
                  continue;
                }
            }

          if (!hl_dead)
            genMove_o (ASMOP_A, 0, left->aop, offset, 1, true, false, iy_dead, true);
          else
            cheapMove (ASMOP_A, 0, left->aop, offset, true);
          if (right->aop->type == AOP_LIT && byteOfVal (right->aop->aopu.aop_lit, offset) == 0 || right->aop->type == AOP_STL && offset >= 2)
            {
              emit3 (A_OR, ASMOP_A, ASMOP_A);
              if (!regalloc_dry_run)
                emit2 ("jp NZ, !tlabel", labelKey2num (lbl->key));
              cost2 (3, 10.0f); // Assume both branches equally likely, jp not optimzed into jr.
            }
          else if (right->aop->type == AOP_STL && offset < 2)
            {
              if (!hl_dead)
                _push (PAIR_HL);
              genMove_o (ASMOP_HL, 0, right->aop, 0, 2, false, true, false, true);
              emit3 (A_SUB, ASMOP_A, offset ? ASMOP_H : ASMOP_L);
              if (!hl_dead)
                _pop (PAIR_HL);
              if (!regalloc_dry_run)
                emit2 ("jp NZ, !tlabel", labelKey2num (lbl->key));
              cost2 (3, 10.0f); // Assume both branches equally likely, jp not optimzed into jr.
            }
          else
            {
              emit3_8alu (A_SUB, right->aop, offset, ic);
              if (!regalloc_dry_run)
                emit2 ("jp NZ, !tlabel", labelKey2num (lbl->key));
              cost2 (3, 10.0f); // Assume both branches equally likely, jp not optimzed into jr.
            }
          offset++;
        }
    }
  /* right is in direct space or a pointer reg, need both a & b */
  else
    {
      /* S1C88: B is the only register sub source besides A, so the byte
         temp is B inside a saved BA (PAIRPTR operands can only sit in
         HL/IX/IY, so BA never conflicts).  The caller pops the pair. */
      _push (PAIR_BA);
      while (size--)
        {
          cheapMove (ASMOP_B, 0, left->aop, offset, true);
          cheapMove (ASMOP_A, 0, right->aop, offset, true);
          emit2 ("sub a, b");
          cost2 (1, 4);
          if (!regalloc_dry_run)
            emit2 ("jp NZ, !tlabel", labelKey2num (lbl->key));
          cost2 (3, 10.0f); // Assume both branches equally likely, cp not optimzed into jr.
          offset++;
        }
      return PAIR_BA;
    }
  return PAIR_INVALID;
}

/*-----------------------------------------------------------------*/
/* gencjne - compare and jump if not equal                         */
/*-----------------------------------------------------------------*/
static void
gencjne (operand * left, operand * right, symbol * lbl, const iCode *ic)
{
  symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
  PAIR_ID pop;

  pop = gencjneshort (left, right, lbl, ic);

  /* PENDING: ?? */
  if (!regalloc_dry_run)
    {
      emit2 ("ld a,!one");
      emit2 ("jp !tlabel", labelKey2num (tlbl->key));
      emitLabelSpill (lbl);
      emit2 ("xor a,a");
      emitLabel (tlbl);
    }
  regalloc_dry_run_cost += 6;
  _pop (pop);
}

/*-----------------------------------------------------------------*/
/* genCmpEq - generates code for equal to                          */
/*-----------------------------------------------------------------*/
static void
genCmpEq (iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  bool hl_touched;

  aopOp ((left = IC_LEFT (ic)), ic, FALSE, FALSE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE, FALSE);
  aopOp ((result = IC_RESULT (ic)), ic, TRUE, FALSE);

  hl_touched = (IC_LEFT (ic)->aop->type == AOP_HL || IC_RIGHT (ic)->aop->type == AOP_HL);

  /* Swap operands if it makes the operation easier. ie if:
     1.  Left is a literal.
   */
  if (IC_LEFT (ic)->aop->type == AOP_LIT || IC_RIGHT (ic)->aop->type != AOP_LIT && IC_RIGHT (ic)->aop->type != AOP_REG
      && IC_LEFT (ic)->aop->type == AOP_REG)
    {
      operand *t = IC_RIGHT (ic);
      IC_RIGHT (ic) = IC_LEFT (ic);
      IC_LEFT (ic) = t;
    }

  if (ifx && !result->aop->size)
    {
      /* if they are both bit variables */
      if (left->aop->type == AOP_CRY && ((right->aop->type == AOP_CRY) || (right->aop->type == AOP_LIT)))
        {
          wassertl (0, "Tried to compare two bits");
        }
      else
        {
          PAIR_ID pop;
          symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
          pop = gencjneshort (left, right, tlbl, ic);
          if (IC_TRUE (ifx))
            {
              if (pop != PAIR_INVALID)
                {
                  emit2 ("pop %s", _pairs[pop].name);
                  cost2 (1, 10);
                }
              if (!regalloc_dry_run)
                emit2 ("jp !tlabel", labelKey2num (IC_TRUE (ifx)->key));
              regalloc_dry_run_cost += 3;
              if (!regalloc_dry_run)
                hl_touched ? emitLabelSpill (tlbl) : emitLabel (tlbl);
              else if (hl_touched)
                spillCached ();
              _pop (pop);
            }
          else
            {
              /* PENDING: do this better */
              symbol *lbl = regalloc_dry_run ? 0 : newiTempLabel (0);
              if (pop != PAIR_INVALID)
                {
                  emit2 ("pop %s", _pairs[pop].name);
                  cost2 (1, 10);
                }
              if (!regalloc_dry_run)
                emit2 ("jp !tlabel", labelKey2num (lbl->key));
              regalloc_dry_run_cost += 3;
              if (!regalloc_dry_run)
                hl_touched ? emitLabelSpill (tlbl) : emitLabel (tlbl);
              else if (hl_touched)
                spillCached ();
              _pop (pop);
              if (!regalloc_dry_run)
                {
                  emit2 ("jp !tlabel", labelKey2num (IC_FALSE (ifx)->key));
                  emitLabel (lbl);
                }
              regalloc_dry_run_cost += 3;
            }
        }
      goto release;
    }

  /* if they are both bit variables */
  if (left->aop->type == AOP_CRY && ((right->aop->type == AOP_CRY) || (right->aop->type == AOP_LIT)))
    {
      wassertl (0, "Tried to compare a bit to either a literal or another bit");
    }
  else
    {
      gencjne (left, right, regalloc_dry_run ? 0 : newiTempLabel (NULL), ic);
      if (result->aop->type == AOP_CRY && result->aop->size)
        {
          wassert (0);
        }
      if (ifx)
        {
          genIfxJump (ifx, "a");
          goto release;
        }
      /* if the result is used in an arithmetic operation
         then put the result in place */
      if (result->aop->type != AOP_CRY)
        genMove (result->aop, ASMOP_A, true, isPairDead (PAIR_HL, ic), true);
    }

release:
  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genAndOp - for && operation                                     */
/*-----------------------------------------------------------------*/
static void
genAndOp (const iCode * ic)
{
  operand *left, *right, *result;

  /* note here that && operations that are in an if statement are
     taken away by backPatchLabels only those used in arthmetic
     operations remain */
  aopOp ((left = IC_LEFT (ic)), ic, FALSE, TRUE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE, TRUE);
  aopOp ((result = IC_RESULT (ic)), ic, FALSE, FALSE);

  /* if both are bit variables */
  if (left->aop->type == AOP_CRY && right->aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to and two bits");
    }
  else
    {
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
      _toBoolean (left, TRUE);
      if (!regalloc_dry_run)
        emit2 ("jp Z, !tlabel", labelKey2num (tlbl->key));
      regalloc_dry_run_cost += 3;
      _toBoolean (right, FALSE);
      if (!regalloc_dry_run)
        emitLabel (tlbl);
      outBitAcc (result);
    }

  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genOrOp - for || operation                                      */
/*-----------------------------------------------------------------*/
static void
genOrOp (const iCode * ic)
{
  operand *left, *right, *result;

  /* note here that || operations that are in an
     if statement are taken away by backPatchLabels
     only those used in arthmetic operations remain */
  aopOp ((left = IC_LEFT (ic)), ic, FALSE, TRUE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE, TRUE);
  aopOp ((result = IC_RESULT (ic)), ic, FALSE, FALSE);

  /* if both are bit variables */
  if (left->aop->type == AOP_CRY && right->aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to OR two bits");
    }
  else
    {
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
      _toBoolean (left, TRUE);
      if (!regalloc_dry_run)
        emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key));
      regalloc_dry_run_cost += 3;
      _toBoolean (right, FALSE);
      if (!regalloc_dry_run)
        emitLabel (tlbl);
      outBitAcc (result);
    }

  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* isLiteralBit - test if lit == 2^n                               */
/*-----------------------------------------------------------------*/
static int
isLiteralBit (unsigned long lit)
{
  unsigned long pw[32] =
  {
    1L, 2L, 4L, 8L, 16L, 32L, 64L, 128L,
    0x100L, 0x200L, 0x400L, 0x800L,
    0x1000L, 0x2000L, 0x4000L, 0x8000L,
    0x10000L, 0x20000L, 0x40000L, 0x80000L,
    0x100000L, 0x200000L, 0x400000L, 0x800000L,
    0x1000000L, 0x2000000L, 0x4000000L, 0x8000000L,
    0x10000000L, 0x20000000L, 0x40000000L, 0x80000000L
  };
  int idx;

  for (idx = 0; idx < 32; idx++)
    if (lit == pw[idx])
      return idx;
  return -1;
}

/*-----------------------------------------------------------------*/
/* jmpTrueOrFalse -                                                */
/*-----------------------------------------------------------------*/
static void
jmpTrueOrFalse (iCode * ic, symbol * tlbl)
{
  // ugly but optimized by peephole
  // Using emitLabelSpill instead of emitLabel
  // We could jump there from locations with different values in hl.
  // This should be changed to a more efficient solution that spills
  // only what and when necessary.
  if (IC_TRUE (ic))
    {
      if (!regalloc_dry_run)
        {
          symbol *nlbl = newiTempLabel (NULL);
          emit2 ("jp !tlabel", labelKey2num (nlbl->key));
          emitLabelSpill (tlbl);
          emit2 ("jp !tlabel", labelKey2num (IC_TRUE (ic)->key));
          emitLabelSpill (nlbl);
        }
      regalloc_dry_run_cost += 6;
    }
  else
    {
      if (!regalloc_dry_run)
        {
          emit2 ("jp !tlabel", labelKey2num (IC_FALSE (ic)->key));
          emitLabelSpill (tlbl);
        }
      regalloc_dry_run_cost += 3;
    }
}

/*-----------------------------------------------------------------*/
/* genAnd  - code for and                                          */
/*-----------------------------------------------------------------*/
static void
genAnd (const iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  int size, offset = 0;
  unsigned long long lit = 0L;
  unsigned int bytelit = 0;

  aopOp ((left = IC_LEFT (ic)), ic, FALSE, FALSE);
  aopOp ((right = IC_RIGHT (ic)), ic, FALSE, FALSE);
  aopOp ((result = IC_RESULT (ic)), ic, TRUE, FALSE);

  bool pushed_a = false;
  bool a_free = isRegDead (A_IDX, ic) && left->aop->regs[A_IDX] <= 0 && right->aop->regs[A_IDX] <= 0;

  /* if left is a literal & right is not then exchange them */
  if ((left->aop->type == AOP_LIT && right->aop->type != AOP_LIT) || (AOP_NEEDSACC (right) && !AOP_NEEDSACC (left)))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  /* if result = right then exchange them */
  if (sameRegs (result->aop, right->aop) && !AOP_NEEDSACC (left))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  if (right->aop->type == AOP_LIT)
    lit = ullFromVal (right->aop->aopu.aop_lit);

  size = result->aop->size;

  if (left->aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to perform an AND with a bit as an operand");
      goto release;
    }

  /* Make sure A is on the left to not overwrite it. */
  if (aopInReg (right->aop, 0, A_IDX) ||
    !aopInReg (left->aop, 0, A_IDX) && isPair (right->aop) && (getPairId (right->aop) == PAIR_HL || getPairId (right->aop) == PAIR_IY))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  // if(val & 0xZZ)       - size = 0, ifx != FALSE  -
  // bit = val & 0xZZ     - size = 1, ifx = FALSE -
  if ((right->aop->type == AOP_LIT) && (result->aop->type == AOP_CRY) && (left->aop->type != AOP_CRY))
    {
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
      int sizel;

      sizel = left->aop->size;
      if (size)
        {
          /* PENDING: Test case for this. */
          emit2 ("scf");
          cost2 (1, 4);
        }
      while (sizel)
        {
          char *jumpcond = "NZ";

          if ((bytelit = ((lit >> (offset * 8)) & 0x0ffull)) == 0x00ull)
            {
              sizel--;
              offset++;
              continue;
            }

          /* Testing for the border bits of the accumulator destructively is cheap. */
          if ((isLiteralBit (bytelit) == 0 || isLiteralBit (bytelit) == 7) && aopInReg (left->aop, offset, A_IDX) && isRegDead (A_IDX, ic))
            {
              emit3 (isLiteralBit (bytelit) == 0 ? A_RRC : A_RLC, ASMOP_A, 0);
              jumpcond = "C";
              sizel--;
              offset++;
            }
          /* Testing for the inverse of the border bits of some 32-bit registers destructively is cheap. */
          /* More combinations would be possible, but this one is the one that is common in the floating-point library. */
          else if (left->aop->type == AOP_REG && sizel >= 2 && ((lit >> (offset * 8)) & 0xffffull) == 0x7fffull && (getPartPairId (left->aop, offset) == PAIR_HL && isPairDead (PAIR_HL, ic)))
            {
              /* the gate guarantees the pair is HL */
              emit3 (A_CP, ASMOP_A, ASMOP_A); // Clear carry.
              emit2 ("adc hl, hl"); // Cannot use add hl, hl instead, since it does not affect zero flag.
              cost2 (2, 15);
              sizel -= 2;
              offset += 2;
            }
          /* Testing for the border bits of some 16-bit registers destructively is cheap. */
          else if (left->aop->type == AOP_REG && sizel == 1 && (isLiteralBit (bytelit) == 7 && (left->aop->aopu.aop_reg[offset]->rIdx == H_IDX && isPairDead (PAIR_HL, ic) || left->aop->aopu.aop_reg[offset]->rIdx == IYH_IDX && isPairDead (PAIR_IY, ic))))
            {
              PAIR_ID pair;
              switch (left->aop->aopu.aop_reg[offset]->rIdx)
                {
                case L_IDX:
                case H_IDX:
                  pair = PAIR_HL;
                  break;
                case IYL_IDX:
                case IYH_IDX:
                  pair = PAIR_IY;
                  break;
                default:
                  pair = PAIR_INVALID;
                  wassertl (0, "Invalid pair");
                }
              if ((pair == PAIR_HL || pair == PAIR_IY) && isLiteralBit (bytelit) == 7)
                {
                  emit2 ("add %s, %s", _pairs[pair].name, _pairs[pair].name);
                  if (pair == PAIR_HL)
                    cost2 (1, 11);
                  else
                    cost2 (2, 15);
                }
              else if (isLiteralBit (bytelit) == 7)
                {
                  emit2 ("rl %s", _pairs[pair].name);
                  cost (1, 2);
                }
              else
                {
                  emit2 ("rr %s", _pairs[pair].name);
                  cost (1, 2);
                }
              jumpcond = "C";
              sizel--;
              offset++;
            }
          /* Non-destructive and when exactly one bit per byte is set. */
          else if (isLiteralBit (bytelit) >= 0 &&
            (left->aop->type == AOP_STK || aopInReg (left->aop, offset, A_IDX) || left->aop->type == AOP_HL || left->aop->type == AOP_IY ||
              left->aop->type == AOP_REG && !aopInReg (left->aop, offset, IYL_IDX) && !aopInReg (left->aop, offset, IYH_IDX)))
            {
              if (requiresHL (left->aop) && left->aop->type != AOP_REG)
                _push (PAIR_HL);
              emitBitTest (isLiteralBit (bytelit), left->aop, offset, ic);   // S1C88: bit reg,#mask (route L/H/[ix+d] via A/B)
              if (requiresHL (left->aop) && left->aop->type != AOP_REG)
                _pop (PAIR_HL);
              sizel--;
              offset++;
            }
          /* Z180 has non-destructive and. */
          else if (!isRegDead (A_IDX, ic) && bytelit == 0x0ff && !aopInReg (left->aop, offset, A_IDX) && left->aop->type == AOP_REG && !aopInReg (left->aop, offset, IYL_IDX) && !aopInReg (left->aop, offset, IYH_IDX))
            {
              emit3_shift (A_RLC, left->aop, offset, ic);
              emit3_shift (A_RRC, left->aop, offset, ic);
              sizel--;
              offset++;
            }
          /* Generic case, loading into accumulator and testing there. */
          else
            {
              if (!isRegDead (A_IDX, ic) || left->aop->regs[A_IDX] > offset || right->aop->regs[A_IDX] > offset)
                UNIMPLEMENTED;

              cheapMove (ASMOP_A, 0, left->aop, offset, true);
              if (isLiteralBit (bytelit) == 0 || isLiteralBit (bytelit) == 7)
                {
                  emit3 (isLiteralBit (bytelit) == 0 ? A_RRC : A_RLC, ASMOP_A, 0);
                  jumpcond = "C";
                }
              else if (bytelit != 0xffu)
                emit3_8alu (A_AND, right->aop, offset, ic);
              else
                emit3 (A_OR, ASMOP_A, ASMOP_A);     /* For the flags */
              sizel--;
              offset++;
            }
          if (size || ifx)  /* emit jmp only, if it is actually used */
            {
              if (!regalloc_dry_run)
                emit2 ("jp %s, !tlabel", jumpcond, labelKey2num (tlbl->key));
              regalloc_dry_run_cost += 3;
            }
        }
      // bit = left & literal
      if (size)
        {
          emit2 ("clr c");
          if (!regalloc_dry_run)
            emit2 ("!tlabeldef", labelKey2num (tlbl->key));
          regalloc_dry_run_cost += 3;
          genLine.lineCurr->isLabel = 1;
        }
      // if(left & literal)
      else
        {
          if (ifx)
            jmpTrueOrFalse (ifx, tlbl);
          goto release;
        }
      outBitC (result);
      goto release;
    }

  

  wassertl (result->aop->type != AOP_CRY, "Result of and is in a bit");

  for (int i = 0; i < size;)
    {
      bool hl_free = isPairDead (PAIR_HL, ic) &&
        (left->aop->regs[L_IDX] < i && left->aop->regs[H_IDX] < i && right->aop->regs[L_IDX] < i && right->aop->regs[H_IDX] < i) &&
        (result->aop->regs[L_IDX] < 0 || result->aop->regs[L_IDX] >= i) && (result->aop->regs[H_IDX] < 0 || result->aop->regs[H_IDX] >= i);

      if (isRegDead (A_IDX, ic) && left->aop->regs[A_IDX] <= i && right->aop->regs[A_IDX] <= i && (result->aop->regs[A_IDX] < 0 || result->aop->regs[A_IDX] >= i))
        a_free = true;

      if (pushed_a && (aopInReg (left->aop, i, A_IDX) || aopInReg (right->aop, i, A_IDX)))
        {
          _pop (PAIR_AF);
          if (!isRegDead (A_IDX, ic))
            _push (PAIR_AF);
          else
            pushed_a = false;
        }

      if (aopIsLitVal (result->aop, i, 1, 0x00) || aopIsLitVal (right->aop, i, 1, 0x00) || aopIsLitVal (right->aop, i, 1, 0xff))
        {
          unsigned int bytelit = (aopIsLitVal (result->aop, i, 1, 0x00) || aopIsLitVal (right->aop, i, 1, 0x00)) ? 0x00 : 0xff;

          int end;
          for(end = i; end < size && (!bytelit && aopIsLitVal (result->aop, end, 1, 0x00) || aopIsLitVal (right->aop, end, 1, bytelit)); end++);
            genMove_o (result->aop, i, bytelit == 0x00 ? ASMOP_ZERO : left->aop, i, end - i, a_free, hl_free, true, true);
          if (result->aop->regs[A_IDX] >= i && result->aop->regs[A_IDX] < end)
            a_free = false;
          i = end;
          continue;
        }

      if (right->aop->type == AOP_LIT)
        {
          bytelit = byteOfVal (right->aop->aopu.aop_lit, i);

          // S1C88 has no RES; clear bits with `and a,#mask`. AND's destination is
          // only A (no `and b`/`and (ix+d),#imm`) — other result aops fall through
          // to the general AND path.
          if (isLiteralBit (~bytelit & 0xffu) >= 0 && aopSame (result->aop, i, left->aop, i, 1) &&
            aopInReg (result->aop, i, A_IDX))
            {
              cheapMove (result->aop, i, left->aop, i, a_free);
              if (!regalloc_dry_run)
                emit2 ("and %s, #0x%02x", aopGet (result->aop, i, false), bytelit);
              cost2 (2, 8); // and r, #n
              if (aopInReg (result->aop, i, A_IDX))
                a_free = false;
              i++;
              continue;
            }
          
        }

      

      if (!a_free)
        {
          if (pushed_a)
            UNIMPLEMENTED;
          else
            _push (PAIR_AF);
          pushed_a = true;
          a_free = true;
        }

      // Use plain and in a.
      if (aopInReg (right->aop, i, A_IDX))
        {
          if (requiresHL (left->aop) && left->aop->type != AOP_REG && !hl_free)
            _push (PAIR_HL);
          if (!HAS_IYL_INST && (aopInReg (left->aop, i, IYL_IDX) || aopInReg (left->aop, i, IYH_IDX)))
            UNIMPLEMENTED;
          else
            emit3_8alu (A_AND, left->aop, i, ic);
          if (requiresHL (left->aop) && left->aop->type != AOP_REG && !hl_free)
            _pop (PAIR_HL);
        }
      else
        {
          if (requiresHL (left->aop) && left->aop->type != AOP_REG && !hl_free)
            _push (PAIR_HL);
          cheapMove (ASMOP_A, 0, left->aop, i, true);
          if (requiresHL (left->aop) && left->aop->type != AOP_REG && !hl_free)
            _pop (PAIR_HL);

          if (requiresHL (right->aop) && right->aop->type != AOP_REG && !hl_free)
            _push (PAIR_HL);

          if (((aopInReg (right->aop, i, IYL_IDX) || aopInReg (right->aop, i, IYH_IDX)) && !HAS_IYL_INST) && hl_free)
            {
              cheapMove (ASMOP_L, 0, left->aop, i, false);
              emit3 (A_AND, ASMOP_A, ASMOP_L);
            }
          else if (!HAS_IYL_INST && (aopInReg (right->aop, i, IYL_IDX) || aopInReg (right->aop, i, IYH_IDX)))
            UNIMPLEMENTED;
          else
            emit3_8alu (A_AND, right->aop, i, ic);
          if (requiresHL (right->aop) && right->aop->type != AOP_REG && !hl_free)
            _pop (PAIR_HL);
        }

      hl_free = isPairDead (PAIR_HL, ic) &&
        (left->aop->regs[L_IDX] <= i && left->aop->regs[H_IDX] <= i && right->aop->regs[L_IDX] <= i && right->aop->regs[H_IDX] <= i) &&
        (result->aop->regs[L_IDX] < 0 || result->aop->regs[L_IDX] >= i) && (result->aop->regs[H_IDX] < 0 || result->aop->regs[H_IDX] >= i);

      genMove_o (result->aop, i, ASMOP_A, 0, 1, true, hl_free, true, true);

      if (aopInReg (result->aop, i, A_IDX))
        a_free = false;

      i++;
    }
  if (pushed_a)
    _pop (PAIR_AF);

release:
  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genOr  - code for or                                            */
/*-----------------------------------------------------------------*/
static void
genOr (const iCode * ic, iCode * ifx)
{
  operand *left, *right, *result;
  int size, offset = 0;
  unsigned long long lit = 0;

  aopOp (IC_LEFT (ic), ic, FALSE, FALSE);
  aopOp (IC_RIGHT (ic), ic, FALSE, FALSE);
  aopOp (IC_RESULT (ic), ic, TRUE, FALSE);
  
  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);

  if (result->aop->type == AOP_REG && left->aop->type != AOP_REG && right->aop->type != AOP_REG)
    {
      if (!requiresHL (right->aop) || (result->aop->regs[L_IDX] < 0 && result->aop->regs[H_IDX] < 0))
        { /* only if not (right requires HL and result use HL) */
          genMove (result->aop, left->aop,
                   isRegDead (A_IDX, ic),
                   isPairDead (PAIR_HL, ic),
                   true);
          left = result;
        }
    }


  bool pushed_a = false;
  bool a_free = isRegDead (A_IDX, ic) && left->aop->regs[A_IDX] <= 0 && right->aop->regs[A_IDX] <= 0;

  /* if left is a literal & right is not then exchange them */
  if ((left->aop->type == AOP_LIT && right->aop->type != AOP_LIT) || (AOP_NEEDSACC (right) && !AOP_NEEDSACC (left)))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  /* if result = right then exchange them */
  if (sameRegs (result->aop, right->aop) && !AOP_NEEDSACC (left))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  if (right->aop->type == AOP_LIT)
    lit = ullFromVal (right->aop->aopu.aop_lit);

  size = result->aop->size;

  if (left->aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to OR where left is a bit");
      goto release;
    }

  /* Make sure A is on the left to not overwrite it. */
  if (aopInReg (right->aop, 0, A_IDX))
    {
      operand *tmp = right;
      right = left;
      left = tmp;
    }

  // if(val | 0xZZ)       - size = 0, ifx != FALSE  -
  // bit = val | 0xZZ     - size = 1, ifx = FALSE -
  if ((right->aop->type == AOP_LIT) && (result->aop->type == AOP_CRY) && (left->aop->type != AOP_CRY))
    {
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
      int sizel;

      sizel = left->aop->size;

      if (size)
        {
          wassertl (0, "Result is assigned to a bit");
        }
      /* PENDING: Modeled after the AND code which is inefficient. */
      while (sizel--)
        {
          if (isRegDead (A_IDX, ic) && left->aop->regs[A_IDX] <= offset && right->aop->regs[A_IDX] <= offset && (result->aop->regs[A_IDX] < 0 || result->aop->regs[A_IDX] >= offset))
            a_free = true;

          if (!a_free) // Hard to handle pop with ifx
            UNIMPLEMENTED;

          int bytelit = (lit >> (offset * 8)) & 0x0FFull;

          cheapMove (ASMOP_A, 0, left->aop, offset, true);

          if (bytelit != 0)
            emit3_8alu (A_OR, right->aop, offset, ic);
          else if (ifx)
            {
              /* For the flags */
              emit3 (A_OR, ASMOP_A, ASMOP_A);
            }

          if (ifx)              /* emit jmp only, if it is actually used */
            {
              if (!regalloc_dry_run)
                emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key));
              regalloc_dry_run_cost += 3;
            }

          offset++;
        }
      if (ifx)
        {
          jmpTrueOrFalse (ifx, tlbl);
        }
      goto release;
    }

  wassertl (result->aop->type != AOP_CRY, "Result of or is in a bit");

  for (int i = 0; i < size;)
    {
      bool hl_free = isPairDead (PAIR_HL, ic) &&
        (left->aop->regs[L_IDX] < i && left->aop->regs[H_IDX] < i && right->aop->regs[L_IDX] < i && right->aop->regs[H_IDX] < i) &&
        (result->aop->regs[L_IDX] < 0 || result->aop->regs[L_IDX] >= i) && (result->aop->regs[H_IDX] < 0 || result->aop->regs[H_IDX] >= i);

      if (isRegDead (A_IDX, ic) && left->aop->regs[A_IDX] <= i && right->aop->regs[A_IDX] <= i && (result->aop->regs[A_IDX] < 0 || result->aop->regs[A_IDX] >= i))
        a_free = true;

      if (pushed_a && (aopInReg (left->aop, i, A_IDX) || aopInReg (right->aop, i, A_IDX)))
        {
          _pop (PAIR_AF);
          if (!isRegDead (A_IDX, ic))
            _push (PAIR_AF);
          else
            pushed_a = false;
        }

      if (left->aop->type == AOP_REG && right->aop->type == AOP_REG && aopIsLitVal (left->aop, i, 1, 0x00) && aopIsLitVal (right->aop, i + 1, 1, 0x00) && (aopInReg (right->aop, i, L_IDX) && aopInReg (left->aop, i + 1, H_IDX) || aopInReg (right->aop, i, IYL_IDX) && aopInReg (left->aop, i + 1, IYH_IDX)))
        {
          asmop *source = ASMOP_HL;
          genMove_o (result->aop, i, source, 0, 2, isRegDead (A_IDX, ic), isRegDead (HL_IDX, ic), isRegDead (IY_IDX, ic), true);
          i += 2;
          continue;
        }
      else if (left->aop->type == AOP_REG && right->aop->type == AOP_REG && aopIsLitVal (left->aop, i + 1, 1, 0x00) && aopIsLitVal (right->aop, i, 1, 0x00) && (aopInReg (right->aop, i + 1, H_IDX) && aopInReg (left->aop, i, L_IDX) || aopInReg (right->aop, i + 1, IYH_IDX) && aopInReg (left->aop, i, IYL_IDX)) && (result->aop->type == AOP_DIR || result->aop->type == AOP_HL || result->aop->type == AOP_IY))
        {
          asmop *source = ASMOP_HL;
          genMove_o (result->aop, i, source, 0, 2, isRegDead (A_IDX, ic), isRegDead (HL_IDX, ic), isRegDead (IY_IDX, ic), true);
          i += 2;
          continue;
        }
      else if (aopIsLitVal (left->aop, i, 1, 0x00) && !pushed_a)
        {
          int end;
          for(end = i; end < size && aopIsLitVal (left->aop, end, 1, 0x00); end++);
            genMove_o (result->aop, i, right->aop, i, end - i, a_free, hl_free, true, true);
          if (result->aop->regs[A_IDX] >= i && result->aop->regs[A_IDX] < end)
            a_free = false;
          i = end;
          continue;
        }
      else if (aopIsLitVal (right->aop, i, 1, 0x00) && !pushed_a)
        {
          int end;
          for(end = i; end < size && aopIsLitVal (right->aop, end, 1, 0x00); end++);
            genMove_o (result->aop, i, left->aop, i, end - i, a_free, hl_free, true, true);
          if (result->aop->regs[A_IDX] >= i && result->aop->regs[A_IDX] < end)
            a_free = false;
          i = end;
          continue;
        }
      else if (aopIsLitVal (left->aop, i, 1, 0xff) || aopIsLitVal (right->aop, i, 1, 0xff))
        {
          int end;
          for(end = i; end < size && (aopIsLitVal (left->aop, end, 1, 0xff) || aopIsLitVal (right->aop, end, 1, 0xff)); end++);
            genMove_o (result->aop, i, ASMOP_MONE, i, end - i, a_free, hl_free, true, true);
          if (result->aop->regs[A_IDX] >= i && result->aop->regs[A_IDX] < end)
            a_free = false;
          i = end;
          continue;
        }

      if (right->aop->type == AOP_LIT)
        {
          int bytelit = byteOfVal (right->aop->aopu.aop_lit, i);

          // S1C88 has no SET; set bits with `or a,#mask`. OR's destination is only
          // A — other result aops fall through to the general OR path.
          if (isLiteralBit (bytelit) >= 0 && aopSame (result->aop, i, left->aop, i, 1) &&
            aopInReg (result->aop, i, A_IDX))
            {
              cheapMove (result->aop, i, left->aop, i, a_free);
              if (!regalloc_dry_run)
                emit2 ("or %s, #0x%02x", aopGet (result->aop, i, false), bytelit);
              cost2 (2, 8); // or r, #n
              if (aopInReg (result->aop, i, A_IDX))
                a_free = false;
              i++;
              continue;
            }
          
        }

      

      // Use plain or in a.
      if (!a_free)
        {
          wassert (!pushed_a);
          _push (PAIR_AF);
          pushed_a = true;
          a_free = true;
        }

      if (aopInReg (right->aop, i, A_IDX) || !HAS_IYL_INST && (aopInReg (right->aop, i, IYL_IDX) || aopInReg (right->aop, i, IYH_IDX)))
        {
          cheapMove (ASMOP_A, 0, right->aop, i, true);

          if (requiresHL (left->aop) && left->aop->type != AOP_REG && !hl_free)
            _push (PAIR_HL);
          if (((aopInReg (right->aop, i, IYL_IDX) || aopInReg (right->aop, i, IYH_IDX)) && !HAS_IYL_INST) && hl_free)
            {
              cheapMove (ASMOP_L, 0, left->aop, i, false);
              emit3 (A_OR, ASMOP_A, ASMOP_L);
            }
          else if (aopInReg (right->aop, i, A_IDX) ||
            !HAS_IYL_INST && (aopInReg (right->aop, i, IYL_IDX) || aopInReg (right->aop, i, IYH_IDX)))
            UNIMPLEMENTED;
          else
            emit3_8alu (A_OR, left->aop, i, ic);
          if (requiresHL (left->aop) && left->aop->type != AOP_REG && !hl_free)
            _pop (PAIR_HL);
        }
      else
        {
          if (requiresHL (left->aop) && left->aop->type != AOP_REG && !hl_free)
            _push (PAIR_HL);
          cheapMove (ASMOP_A, 0, left->aop, i, true);
          if (requiresHL (left->aop) && left->aop->type != AOP_REG && !hl_free)
            _pop (PAIR_HL);

          if (requiresHL (right->aop) && right->aop->type != AOP_REG && !hl_free)
            _push (PAIR_HL);
          emit3_8alu (A_OR, right->aop, i, ic);
          if (requiresHL (right->aop) && right->aop->type != AOP_REG && !hl_free)
            _pop (PAIR_HL);
        }

      hl_free = isPairDead (PAIR_HL, ic) &&
        (left->aop->regs[L_IDX] <= i && left->aop->regs[H_IDX] <= i && right->aop->regs[L_IDX] <= i && right->aop->regs[H_IDX] <= i) &&
        (result->aop->regs[L_IDX] < 0 || result->aop->regs[L_IDX] >= i) && (result->aop->regs[H_IDX] < 0 || result->aop->regs[H_IDX] >= i);

      genMove_o (result->aop, i, ASMOP_A, 0, 1, true, hl_free, true, true);
        
      if (aopInReg (result->aop, i, A_IDX))
        a_free = false;
      i++;
    }

  if (pushed_a)
    _pop (PAIR_AF);

release:
  freeAsmop (IC_LEFT (ic), NULL);
  freeAsmop (IC_RIGHT (ic), NULL);
  freeAsmop (IC_RESULT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genEor - code for xclusive or                                   */
/*-----------------------------------------------------------------*/
static void
genEor (const iCode *ic, iCode *ifx, asmop *result_aop, asmop *left_aop, asmop *right_aop)
{
  int size;
  bool pushed_a = false;

  bool a_free = isRegDead (A_IDX, ic) && left_aop->regs[A_IDX] <= 0 && right_aop->regs[A_IDX] <= 0;

  /* if left is a literal & right is not then exchange them */
  if ((left_aop->type == AOP_LIT && right_aop->type != AOP_LIT) || ((right_aop->type == AOP_CRY) && !(left_aop->type == AOP_CRY)))
    {
      asmop *taop = right_aop;
      right_aop = left_aop;
      left_aop = taop;
    }

  /* if result = right then exchange them */
  if (sameRegs (result_aop, right_aop) && !(left_aop->type == AOP_CRY))
    {
      asmop *taop = right_aop;
      right_aop = left_aop;
      left_aop = taop;
    }

  size = result_aop->size;

  if (left_aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to XOR a bit");
      return;
    }

  /* Make sure A is on the left to not overwrite it. */
  if (aopInReg (right_aop, 0, A_IDX))
    {
      wassert (!(left_aop->type == AOP_CRY));
      asmop *taop = right_aop;
      right_aop = left_aop;
      left_aop = taop;
    }

  // if(val & 0xZZ)       - size = 0, ifx != FALSE  -
  // bit = val & 0xZZ     - size = 1, ifx = FALSE -
  if ((right_aop->type == AOP_LIT) && (result_aop->type == AOP_CRY) && (left_aop->type != AOP_CRY))
    {
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
      int offset = 0;
      int sizel = left_aop->size;

      if (size)
        {
          /* PENDING: Test case for this. */
          wassertl (0, "Tried to XOR left against a literal with the result going into a bit");
        }
      while (sizel--)
        {
          if (isRegDead (A_IDX, ic) && left_aop->regs[A_IDX] <= offset && right_aop->regs[A_IDX] <= offset && (result_aop->regs[A_IDX] < 0 || result_aop->regs[A_IDX] >= offset))
            a_free = true;

          if (!a_free)
            {
              wassert (!pushed_a);
              _push (PAIR_AF);
              a_free = true;
              pushed_a = true;
              if (ifx) // The pop at the end is hard to deal with in case of ifx.
                UNIMPLEMENTED;
            }
          else if (pushed_a && (aopInReg (left_aop, offset, A_IDX) || aopInReg (right_aop, offset, A_IDX)))
            {
              _pop (PAIR_AF);
              if (!isRegDead (A_IDX, ic))
                _push (PAIR_AF);
              else
                pushed_a = false;
            }

          if (aopInReg (right_aop, offset, A_IDX))
            emit3_8alu (A_XOR, left_aop, offset, ic);
          else
            {
              cheapMove (ASMOP_A, 0, left_aop, offset, true);
              emit3_8alu (A_XOR, right_aop, offset, ic);
            }
          if (ifx)              /* emit jmp only, if it is actually used * */
            if (!regalloc_dry_run)
              emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key));
          regalloc_dry_run_cost += 3;
          offset++;
        }
      if (pushed_a)
        {
          _pop (PAIR_AF);
          pushed_a = false;
        }
      if (ifx)
        {
          jmpTrueOrFalse (ifx, tlbl);
        }
      else if (size)
        {
          wassertl (0, "Result of XOR was destined for a bit");
        }
      return;
    }

    // left & result in different registers
    if (result_aop->type == AOP_CRY)
      {
        wassertl (0, "Result of XOR is in a bit");
        return;
      }

    for (int i = 0; i < size;)
      {
        bool hl_free = isPairDead (PAIR_HL, ic) &&
          (left_aop->regs[L_IDX] < i && left_aop->regs[H_IDX] < i && right_aop->regs[L_IDX] < i && right_aop->regs[H_IDX] < i) &&
          (result_aop->regs[L_IDX] < 0 || result_aop->regs[L_IDX] >= i) && (result_aop->regs[H_IDX] < 0 || result_aop->regs[H_IDX] >= i);

        if (isRegDead (A_IDX, ic) && left_aop->regs[A_IDX] <= i && right_aop->regs[A_IDX] <= i && (result_aop->regs[A_IDX] < 0 || result_aop->regs[A_IDX] >= i))
          a_free = true;
            
        // normal case
        // result = left ^ right
        if (aopIsLitVal (right_aop, i, 1, 0x00))
          {
            int end;
            for (end = i; end < size && aopIsLitVal (right_aop, end, 1, 0x00); end++);
            if (pushed_a && left_aop->type == AOP_REG && left_aop->regs[A_IDX] >= i && left_aop->regs[A_IDX] < end)
              {
                if (result_aop->regs[A_IDX] >= 0 && result_aop->regs[A_IDX] < i)
                  UNIMPLEMENTED;
                _pop (PAIR_AF);
                if (!isRegDead (A_IDX, ic))
                  _push (PAIR_AF);
                else
                  pushed_a = false;
              }
            genMove_o (result_aop, i, left_aop, i, end - i, a_free, hl_free, true, true);
            if (result_aop->type == AOP_REG &&
              (left_aop->regs[result_aop->aopu.aop_reg[i]->rIdx] >= end || right_aop->regs[result_aop->aopu.aop_reg[i]->rIdx] >= end))
              UNIMPLEMENTED;
            if (result_aop->regs[A_IDX] >= i && result_aop->regs[A_IDX] < end)
              a_free = false;
            i = end;
            continue;
          }
        

        if (pushed_a && (aopInReg (left_aop, i, A_IDX) || aopInReg (right_aop, i, A_IDX)))
          {
            if (result_aop->regs[A_IDX] >= 0 && result_aop->regs[A_IDX] < i)
              UNIMPLEMENTED;
            _pop (PAIR_AF);
            if (!isRegDead (A_IDX, ic))
              _push (PAIR_AF);
            else
              pushed_a = false;
          }

        // faster than result <- left, anl result,right
        // and better if result is SFR
        if (!a_free)
          {
            if (pushed_a)
              UNIMPLEMENTED;
            else
              _push (PAIR_AF);
            a_free = true;
            pushed_a = true;
          }

        if (aopInReg (right_aop, i, A_IDX) && left_aop->type != AOP_STL)
          {
            if (requiresHL (left_aop) && left_aop->type != AOP_REG && !hl_free)
              _push (PAIR_HL);
            if (!HAS_IYL_INST && (aopInReg (left_aop, i, IYL_IDX) || aopInReg (left_aop, i, IYH_IDX)))
              UNIMPLEMENTED;
            else
              emit3_8alu (A_XOR, left_aop, i, ic);
            if (requiresHL (left_aop) && left_aop->type != AOP_REG && !hl_free)
              _pop (PAIR_HL);
          }
        else
          {
            if (requiresHL (left_aop) && left_aop->type != AOP_REG && !hl_free)
              _push (PAIR_HL);
            cheapMove (ASMOP_A, 0, left_aop, i, true);
            if (requiresHL (left_aop) && left_aop->type != AOP_REG && !hl_free)
               _pop (PAIR_HL);
            if (right_aop->type == AOP_LIT && byteOfVal (right_aop->aopu.aop_lit, i) == 0xff)
              emit3 (A_CPL, ASMOP_A, 0);   // S1C88 cpl needs an explicit operand (cpl a)
            else if (right_aop->type == AOP_STL || aopInReg (right_aop, i, IYL_IDX) || aopInReg (right_aop, i, IYH_IDX))
              {
                if (!hl_free)
                  _push (PAIR_HL);
                cheapMove (ASMOP_L, 0, right_aop, i, false);
                emit3_8alu (A_XOR, ASMOP_L, 0, ic);
                if (!hl_free)
                  _pop (PAIR_HL);
              }
            else
              {
                if (requiresHL (right_aop) && right_aop->type != AOP_REG && !hl_free)
                  _push (PAIR_HL);
                emit3_8alu (A_XOR, right_aop, i, ic);
                if (requiresHL (right_aop) && right_aop->type != AOP_REG && !hl_free)
                  _pop (PAIR_HL);
              }
          }
          
        hl_free = isPairDead (PAIR_HL, ic) &&
          (left_aop->regs[L_IDX] <= i && left_aop->regs[H_IDX] <= i && right_aop->regs[L_IDX] <= i && right_aop->regs[H_IDX] <= i) &&
          (result_aop->regs[L_IDX] < 0 || result_aop->regs[L_IDX] >= i) && (result_aop->regs[H_IDX] < 0 || result_aop->regs[H_IDX] >= i);

        genMove_o (result_aop, i, ASMOP_A, 0, 1, true, hl_free, true, true);

        if(result_aop->type == AOP_REG &&
          (left_aop->regs[result_aop->aopu.aop_reg[i]->rIdx] > i || right_aop->regs[result_aop->aopu.aop_reg[i]->rIdx] > i))
          UNIMPLEMENTED;
        if (aopInReg (result_aop, i, A_IDX))
          a_free = false;

        i++;
     }

  if (pushed_a)
    _pop (PAIR_AF);
}

/*-----------------------------------------------------------------*/
/* genXor - code for exclusive or                                   */
/*-----------------------------------------------------------------*/
static void
genXor (const iCode *ic, iCode *ifx)
{
  aopOp (IC_LEFT (ic), ic, false, false);
  aopOp (IC_RIGHT (ic), ic, false, false);
  aopOp (IC_RESULT (ic), ic, true, false);
  
  genEor (ic, ifx, IC_RESULT (ic)->aop, IC_LEFT (ic)->aop, IC_RIGHT (ic)->aop);
  
  freeAsmop (IC_LEFT (ic), NULL);
  freeAsmop (IC_RIGHT (ic), NULL);
  freeAsmop (IC_RESULT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genCpl - generate code for complement                           */
/*-----------------------------------------------------------------*/
static void
genCpl (const iCode *ic)
{
  /* assign asmOps to operand & result */
  aopOp (IC_LEFT (ic), ic, false, false);
  aopOp (IC_RESULT (ic), ic, true, false);

  genEor (ic, 0, IC_RESULT (ic)->aop, IC_LEFT (ic)->aop, ASMOP_MONE);

  /* release the aops */
  freeAsmop (IC_LEFT (ic), 0);
  freeAsmop (IC_RESULT (ic), 0);
}

/*-----------------------------------------------------------------*/
/* genRRC - rotate right with carry                                */
/*-----------------------------------------------------------------*/
static void
genRRC (const iCode *ic)
{
  bool pushed_a = false;

  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  operand *result = IC_RESULT (ic);

  aopOp (left, ic, false, false);
  aopOp (result, ic, true, false);

  int size = result->aop->size;

  wassert (size >= 2); // 8-bit rotations are handled in Rot1
  wassert (!(bitsForType (operandType (left)) % 8));
  wassert (IS_OP_LITERAL (right));

  int s = operandLitValueUll (right) % bitsForType (operandType (left));

  wassert (s == bitsForType (operandType (left)) - 1);

  int offset = size - 1;

  if (left->aop->type == AOP_REG || result->aop->type == AOP_STK ||
           result->aop->type == AOP_HL || result->aop->type == AOP_IY ||
           result->aop->type == AOP_EXSTK || result->aop->type == AOP_REG)
    {
      if (!isRegDead (A_IDX, ic))
        {
          _push (PAIR_AF);
          pushed_a = true;
        }
      if (left->aop->type != AOP_REG && !operandsEqu (result, left))
        {
          /* always prefer register operations */
          genMove_o (result->aop, 0, left->aop, 0, size, true, isPairDead (PAIR_HL, ic), true, true);
          left = result;
        }
      cheapMove (ASMOP_A, 0, left->aop, offset, true);
      emit3_o (A_RR, ASMOP_A, 0, 0, 0);
      while (--offset >= 0)
        emit3_shift (A_RR, left->aop, offset, ic);
      
      emit3_shift (A_RR, left->aop, size - 1, ic);
      if (!operandsEqu (result, left))
        genMove_o (result->aop, 0, left->aop, 0, size, true, isPairDead (PAIR_HL, ic), true, true);
    }
  else
    {
      if (!isRegDead (A_IDX, ic))
      {
        _push (PAIR_AF);
        pushed_a = true;
      }
      while (offset >= 0)
        {
          _moveA (aopGet (left->aop, offset, false));
          emit3_o (A_RR, ASMOP_A, 0, 0, 0);
          if (offset != size - 1)
            aopPut (result->aop, "a", offset);
          --offset;
        }
      _moveA (aopGet (left->aop, size - 1, false));
      emit3_o (A_RR, ASMOP_A, 0, 0, 0);
      aopPut (result->aop, "a", size - 1);
    }
  if (pushed_a)
    _pop (PAIR_AF);

  freeAsmop (IC_LEFT (ic), 0);
  freeAsmop (IC_RESULT (ic), 0);
}

/*-----------------------------------------------------------------*/
/* genRLC - generate code for rotate left                          */
/*-----------------------------------------------------------------*/
static void
genRLC (const iCode *ic)
{
  bool pushed_a = false;

  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  operand *result = IC_RESULT (ic);

  aopOp (left, ic, false, false);
  aopOp (result, ic, true, false);

  int size = result->aop->size;

  wassert (size >= 2); // 8-bit rotations are handled in Rot1
  wassert (!(bitsForType (operandType (left)) % 8));
  wassert (IS_OP_LITERAL (right));

  int s = operandLitValueUll (right) % bitsForType (operandType (left));

  wassert (s == 1);

  if (left->aop->type == AOP_REG || result->aop->type == AOP_STK ||
           result->aop->type == AOP_HL || result->aop->type == AOP_IY ||
           result->aop->type == AOP_EXSTK || result->aop->type == AOP_REG)
    {
      asmop *rotaop = result->aop;
      if (!isRegDead (A_IDX, ic))
        {
          _push (PAIR_AF);
          pushed_a = true;
        }
      if (size == 2 && (aopInReg (left->aop, 0, HL_IDX) && isRegDead (HL_IDX, ic)))
        rotaop = left->aop;
      genMove (rotaop, left->aop, true, isRegDead (HL_IDX, ic), isRegDead (IY_IDX, ic));
      cheapMove (ASMOP_A, 0, rotaop, size - 1, true);
      emit3 (A_RL, ASMOP_A, 0);
      
      for (int i = 0; i < size;)
        {
          if (i + 1 < size && aopInReg (rotaop, i, HL_IDX))
            {
              emit2 ("adc hl, hl");
              cost2 (2, 15);
              i += 2;
            }
          else {
              emit3_shift (A_RL, rotaop, i, ic);
              i++;
            }
        }
     genMove (result->aop, rotaop, true, isRegDead (HL_IDX, ic), isRegDead (IY_IDX, ic));
    }
  else
    {
      if (!isRegDead (A_IDX, ic))
      {
        _push (PAIR_AF);
        pushed_a = true;
      }

      for (int offset = 0; offset < size; ++offset)
        {
          _moveA (aopGet (left->aop, offset, false));
          emit3_o (A_RL, ASMOP_A, 0, 0, 0);
          if (offset != 0)
            aopPut (result->aop, "a", offset);
        }
      _moveA (aopGet (left->aop, 0, false));
      emit3_o (A_RL, ASMOP_A, 0, 0, 0);
      aopPut (result->aop, "a", 0);
    }

  if (pushed_a)
    _pop (PAIR_AF);

  freeAsmop (IC_LEFT (ic), 0);
  freeAsmop (IC_RESULT (ic), 0);
}

/*-----------------------------------------------------------------*/
/* genGetByte - generates code to get a single byte                */
/*-----------------------------------------------------------------*/
static void
genGetByte (const iCode *ic)
{
  operand *left, *right, *result;
  int offset;

  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);
  aopOp (left, ic, FALSE, FALSE);
  aopOp (right, ic, FALSE, FALSE);
  aopOp (result, ic, TRUE, FALSE);

  offset = (int) ulFromVal (right->aop->aopu.aop_lit) / 8;
  genMove_o (result->aop, 0, left->aop, offset, 1, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true, true);

  freeAsmop (result, NULL);
  freeAsmop (right, NULL);
  freeAsmop (left, NULL);
}

/*-----------------------------------------------------------------*/
/* genGetWord - generates code to get a 16-bit word                */
/*-----------------------------------------------------------------*/
static void
genGetWord (const iCode *ic)
{
  operand *left, *right, *result;
  int offset;

  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);
  aopOp (left, ic, FALSE, FALSE);
  aopOp (right, ic, FALSE, FALSE);
  aopOp (result, ic, TRUE, FALSE);

  offset = (int) ulFromVal (right->aop->aopu.aop_lit) / 8;
  genMove_o (result->aop, 0, left->aop, offset, 2, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true, true);

  freeAsmop (result, NULL);
  freeAsmop (right, NULL);
  freeAsmop (left, NULL);
}

/*-----------------------------------------------------------------*/
/* genGetAbit - generates code get a single bit                    */
/*-----------------------------------------------------------------*/
static void
genGetAbit (const iCode * ic)
{
  operand *left, *right, *result;
  int shCount;

  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);
  aopOp (left, ic, FALSE, FALSE);
  aopOp (right, ic, FALSE, FALSE);
  aopOp (result, ic, TRUE, FALSE);

  shCount = (int) ulFromVal (right->aop->aopu.aop_lit);

  /* get the needed byte into a */
  cheapMove (ASMOP_A, 0, left->aop, shCount / 8, true);
  shCount %= 8;
  
  if (result->aop->type == AOP_CRY)
    {
      
      if (shCount < 4)
        while (shCount-- >= 0)
          emit3_o (A_RRC, ASMOP_A, 0, 0, 0);
      else
        while (shCount++ < 8)
          emit3_o (A_RLC, ASMOP_A, 0, 0, 0);
      outBitC (result);
    }
  else
    {
      if (shCount < 5)
        while (shCount-- > 0)
          emit3_o (A_RRC, ASMOP_A, 0, 0, 0);
      else
        while (shCount++ < 8)
          emit3_o (A_RLC, ASMOP_A, 0, 0, 0);
      emit2 ("and a, !immedbyte", 0x01u);
      cost2 (2, 7);
      outAcc (result);
    }

  freeAsmop (result, NULL);
  freeAsmop (right, NULL);
  freeAsmop (left, NULL);
}

static void
emitRsh2 (asmop * aop, int size, int is_signed, const iCode *ic)
{
  int offset = 0;

  while (size--)
    {
      if (offset == 0)
        emit3_shift (is_signed ? A_SRA : A_SRL, aop, size, ic);
      else
        emit3_shift (A_RR, aop, size, ic);
      offset++;
    }
}

/*-----------------------------------------------------------------*/
/* shiftR2Left2Result - shift right two bytes from left to result  */
/*-----------------------------------------------------------------*/
static void
shiftR2Left2Result (const iCode *ic, operand *left, int offl, operand *result, int offr, int shCount, int is_signed)
{
  int size = 2;
  symbol *tlbl;

  if (!is_signed && aopSame (result->aop, offr, left->aop, offl, 2) && isPairDead (PAIR_HL, ic) && isRegDead (A_IDX, ic) &&
    (shCount == 4 || shCount == 5) &&
    (result->aop->type == AOP_DIR || result->aop->type == AOP_HL || result->aop->type == AOP_IY))
    {
      emit2 ("xor a, a");
      cost2 (1, 4);
      emit2 ("ld hl, !hashedstr+1", result->aop->aopu.aop_dir);
      cost2 (3, 10);
      emit3 (A_RRD, 0, 0);
      if (shCount == 5)
        {
          emit2 ("srl (hl)");
          cost2 (2, 15);
        }
      emit2 ("dec hl");
      cost2 (1, 6);
      emit3 (A_RRD, 0, 0);
      if (shCount == 5)
        {
          emit2 ("rr (hl)");
          cost2 (2, 15);
        }
      return;
    }
  else if ((getPairId (result->aop) == PAIR_HL || getPairId (left->aop) == PAIR_HL) && isPairDead (PAIR_HL, ic) &&
    shCount == 7 && is_signed)
    {
      tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
      genMove (ASMOP_HL, left->aop, isRegDead (A_IDX, ic), true, isRegDead (IY_IDX, ic));
      emit3w (A_ADD, ASMOP_HL, ASMOP_HL);
      emit3 (A_LD, ASMOP_L, ASMOP_H);
      emit3 (A_LD, ASMOP_H, ASMOP_ZERO);
      if (!regalloc_dry_run)
        emit2 ("jr nc,!tlabel", labelKey2num (tlbl->key));
      emit2 ("dec h");
      if (!regalloc_dry_run)
        emitLabel (tlbl);
      cost (3, 11.5f);
      genMove (result->aop, ASMOP_HL, isRegDead (A_IDX, ic), true, isRegDead (IY_IDX, ic));
      return;
    }
  // If the leading bits are all the same, we can shift the other way, and use efficient 16-bit addition for shifts.
  else if (shCount < 8 &&
    aopInReg (left->aop, 0, HL_IDX) && aopInReg (result->aop, 0, H_IDX) && isRegDead (L_IDX, ic) && isRegDead (A_IDX, ic) &&
    shCount >= 5 - !optimize.codeSpeed) // Smaller code size for 4 and above.
    {
      emit3 (A_XOR, ASMOP_A, ASMOP_A);
      emit2 ("add hl, hl");
      cost2 (1, 11);
      if (is_signed && (left->aop->valinfo.anything || left->aop->valinfo.min < 0))
        {
          tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
          if (!regalloc_dry_run)
            emit2 ("jr nc,!tlabel", labelKey2num (tlbl->key));
          emit2 ("dec a");
          cost2 (3, 11.5);
          if (!regalloc_dry_run)
            emitLabel (tlbl);
        }
      else if (!is_signed)
        emit3 (A_RL, ASMOP_A, 0);
      for (int i = 1; i < (8 - shCount); i++)
        {
          emit2 ("add hl, hl");
          cost2 (1, 11);
          emit3 (A_RL, ASMOP_A, 0);
        }
      genMove_o (result->aop, 1, ASMOP_A, 0, 1, true, false, isPairDead (PAIR_IY, ic), true);
      return;
    }

  if (isPair (result->aop) && !offr)
    fetchPairLong (getPairId (result->aop), left->aop, ic, offl);
  else
    genMove_o (result->aop, offr, left->aop, offl, 2, true, isPairDead (PAIR_HL, ic), true, true);

  if (shCount == 0)
    return;

  /*  if (result->aop->type == AOP_REG) { */

  /* Left is already in result - so now do the shift */
  /* Optimizing for speed by default. */
  if (!optimize.codeSize || shCount <= 2)
    {
      while (shCount--)
        emitRsh2 (result->aop, size, is_signed, ic);
    }
  else
    {
      bool use_b = (isRegDead (B_IDX, ic)
                    && !(result->aop->type == AOP_REG
                         && (result->aop->aopu.aop_reg[0]->rIdx == B_IDX || result->aop->aopu.aop_reg[1]->rIdx == B_IDX)));

      tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
      
      if (requiresHL (result->aop))
        spillPair (PAIR_HL);

      emit2 ("ld %s, !immedbyte", use_b ? "b" : "a", (unsigned)shCount);
      cost2 (2, 7);

      regalloc_dry_run_state_scale *= (unsigned)shCount;

      if (!regalloc_dry_run)
        emitLabel (tlbl);

      emitRsh2 (result->aop, size, is_signed, ic);

      if (use_b)
        {
          if (!regalloc_dry_run)
            emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
          cost2 (2, 13); // Assume jump taken.
        }
      else
        {
          emit3 (A_DEC, ASMOP_A, 0);
          if (!regalloc_dry_run)
            emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key));
          cost2 (2, 12); // Assume jump taken, and optimized to jr.
        }

      regalloc_dry_run_state_scale = 1.0f;
    }
}

/*-----------------------------------------------------------------*/
/* shiftL2Left2Result - shift left two bytes from left to result   */
/*-----------------------------------------------------------------*/
static void
shiftL2Left2Result (operand *left, operand *result, int shCount, const iCode *ic)
{
  asmop *shiftaop = result->aop;

  if (shCount == 7 && aopIsLitVal (left->aop, 1, 1, 0x00) && result->aop->type == AOP_REG &&
    result->aop->aopu.aop_reg[0]->rIdx != IYL_IDX && result->aop->aopu.aop_reg[1]->rIdx != IYL_IDX && result->aop->aopu.aop_reg[0]->rIdx != IYH_IDX && result->aop->aopu.aop_reg[1]->rIdx != IYH_IDX)
    {
      genMove_o (result->aop, 1, left->aop, 0, 1, isRegDead(A_IDX, ic), false, false, true);
      bool reuse_zero = left->aop->type == AOP_REG && !aopInReg (left->aop, 1, IYL_IDX) && !aopInReg (left->aop, 1, IYH_IDX) && !aopInReg (left->aop, 1, result->aop->aopu.aop_reg[1]->rIdx);
      genMove_o (result->aop, 0, reuse_zero ? left->aop : ASMOP_ZERO, 1, 1, isRegDead(A_IDX, ic) && !aopInReg (result->aop, 1, A_IDX), false, false, true);
      emit3_shift (A_SRL, result->aop, 1, ic);
      if (aopInReg (result->aop, 0, A_IDX))
        emit3 (A_RR, ASMOP_A, 0);
      else
        emit3_shift (A_RR, result->aop, 0, ic);
      return;
    }
  /* For a shift of 7 we can use cheaper right shifts */
  else if (shCount == 7 && left->aop->type == AOP_REG && !bitVectBitValue (ic->rSurv, left->aop->aopu.aop_reg[0]->rIdx) && result->aop->type == AOP_REG &&
    left->aop->aopu.aop_reg[0]->rIdx != IYL_IDX && left->aop->aopu.aop_reg[1]->rIdx != IYL_IDX && left->aop->aopu.aop_reg[0]->rIdx != IYH_IDX && left->aop->aopu.aop_reg[1]->rIdx != IYH_IDX &&
    result->aop->aopu.aop_reg[0]->rIdx != IYL_IDX && result->aop->aopu.aop_reg[1]->rIdx != IYL_IDX && result->aop->aopu.aop_reg[0]->rIdx != IYH_IDX && result->aop->aopu.aop_reg[1]->rIdx != IYH_IDX &&
    (optimize.codeSpeed || getPairId (result->aop) != PAIR_HL || getPairId (left->aop) != PAIR_HL)) /* but a sequence of add hl, hl might still be cheaper code-size wise */
    {
      // Handling the low byte in A with xor clearing is cheaper.
      bool special_a = (isRegDead (A_IDX, ic) && !aopInReg (left->aop, 0, A_IDX) && !aopInReg (left->aop, 1, A_IDX));
      asmop *lowbyte = special_a ? ASMOP_A : result->aop;

      if (special_a)
        emit3 (A_XOR, ASMOP_A, ASMOP_A);
      emit3_shift (A_RR, left->aop, 1, ic);
      emit3_o (A_LD, result->aop, 1, left->aop, 0);
      emit3_shift (A_RR, result->aop, 1, ic);
      if (!special_a)
        emit3_o (A_LD, result->aop, 0, ASMOP_ZERO, 0);
      if (aopInReg (lowbyte, 0, A_IDX))
        emit3 (A_RR, ASMOP_A, 0);
      else
        emit3_shift (A_RR, lowbyte, 0, ic);
      if (special_a)
        cheapMove (result->aop, 0, lowbyte, 0, true);
      return;
    }
  if ((result->aop->type == AOP_HL || result->aop->type == AOP_IY) && (left->aop->type == AOP_HL || left->aop->type == AOP_IY) && isPairDead (PAIR_HL, ic) && (shCount > 1 || !sameRegs (result->aop, left->aop)) || isPairDead (PAIR_HL, ic) && 0 && 1) // Shift in hl if we can cheaply move to de via ex later.
    {
      shiftaop = ASMOP_HL;
      genMove (ASMOP_HL, left->aop, isRegDead (A_IDX, ic), true, true);
    }
  else if (result->aop->type != AOP_REG && left->aop->type == AOP_REG && left->aop->size >= 2 && !bitVectBitValue (ic->rSurv, left->aop->aopu.aop_reg[0]->rIdx) && !bitVectBitValue (ic->rSurv, left->aop->aopu.aop_reg[1]->rIdx) ||
    getPairId (left->aop) == PAIR_HL && isPairDead (PAIR_HL, ic))
    shiftaop = left->aop;
  else
    genMove_o (result->aop, 0, left->aop, 0, 2, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true, true);

  if (shCount == 0)
    ;
  else if (getPairId (shiftaop) == PAIR_HL)
    {
      while (shCount--)
        emit3w (A_ADD, ASMOP_HL, ASMOP_HL);
    }
  else if (getPairId (shiftaop) == PAIR_IY)
    {
      while (shCount--)
        {
          emit2 ("add iy, iy");
          cost2 (2, 15);
        }
    }
  else
    {
      int size = 2;
      int offset = 0;
      
      bool use_b = (isRegDead (B_IDX, ic)
        && (shiftaop->type != AOP_REG || shiftaop->aopu.aop_reg[0]->rIdx != B_IDX && shiftaop->aopu.aop_reg[1]->rIdx != B_IDX));
                         
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);

      if (shiftaop->type == AOP_REG)
        {
          while (shCount--)
            {
              for (offset = 0; offset < size; offset++)
                if (aopInReg (shiftaop, offset, A_IDX))
                  emit3 (offset ? A_ADC : A_ADD, ASMOP_A, ASMOP_A);
                else
                  emit3_shift (offset ? A_RL : A_SLA, shiftaop, offset, ic);
            }
        }
      else
        {
          if (!use_b && !isRegDead (A_IDX, ic))
            _push (PAIR_AF);

          /* Left is already in result - so now do the shift */
          if (shCount > 1)
            {
              if (!regalloc_dry_run)
                {
                  emit2 ("ld %s, !immedbyte", use_b ? "b" : "a", (unsigned)shCount);
                  emitLabel (tlbl);
                }
              cost2 (2, 7);
              
              if (requiresHL (shiftaop))
                spillPair (PAIR_HL);
            }

          while (size--)
            {
              emit3_shift (offset ? A_RL : A_SLA, shiftaop, offset, ic);

              offset++;
            }
          if (shCount > 1)
            {
              if (use_b)
                {
                  if (!regalloc_dry_run)
                    emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
                  cost2 (2, 13); // Assume jump taken.
                }
              else
                {
                  emit3 (A_DEC, ASMOP_A, 0);
                  if (!regalloc_dry_run)
                    emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key));
                  cost2 (2, 12); // Assume jump taken, and optimized to jr.
                }
            }
          if (!use_b && !isRegDead (A_IDX, ic))
            _pop (PAIR_AF);
        }
    }

  sym_link *resulttype = operandType (IC_RESULT (ic));
  unsigned topbytemask = (IS_BITINT (resulttype) && SPEC_USIGN (resulttype) && (SPEC_BITINTWIDTH (resulttype) % 8)) ?
    (0xff >> (8 - SPEC_BITINTWIDTH (resulttype) % 8)) : 0xff;
  bool maskedtopbyte = (topbytemask != 0xff);
  if (maskedtopbyte)
    {
      bool pushed_a = false;
      if (!isRegDead (A_IDX, ic) || shiftaop->regs[A_IDX] >= 0 && shiftaop->regs[A_IDX] != result->aop->size - 1)
        {
          _push (PAIR_AF);
          pushed_a = true;
        }
      cheapMove (ASMOP_A, 0, shiftaop, result->aop->size - 1, true);
      emit2 ("and a, #0x%02x", topbytemask);
      cost2 (2, 7);
      cheapMove (shiftaop, result->aop->size - 1, ASMOP_A, 0, true);
      if (pushed_a)
        _pop (PAIR_AF);
    }

  if (shiftaop != result->aop)
    {
      if (isPair (result->aop))
        fetchPairLong (getPairId (result->aop), shiftaop, ic, 0);
      else
        genMove_o (result->aop, 0, shiftaop, 0, 2, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true, true);
    }
}

/*-----------------------------------------------------------------*/
/* genSwap - generates code to swap nibbles or bytes               */
/*-----------------------------------------------------------------*/
static void
genSwap (iCode * ic)
{
  operand *left, *result;
  asmop swapped_result_aop;
  
  left = IC_LEFT (ic);
  result = IC_RESULT (ic);
  aopOp (left, ic, false, false);
  aopOp (result, ic, true, false);
  bool pushed_a = false;
  switch (left->aop->size)
    {
    case 2: // swap bytes in word
      if (result->aop->type == AOP_REG) // Create result asmop with swapped bytes, let genMove handle the details.
        {
          signed char idxarray[3];
          idxarray[0] = result->aop->aopu.aop_reg[1]->rIdx;
          idxarray[1] = result->aop->aopu.aop_reg[0]->rIdx;
          idxarray[2] = -1;
          s1c88_init_reg_asmop (&swapped_result_aop, idxarray);
          genMove (&swapped_result_aop, left->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
          break;
        }

      if (sameRegs (result->aop, left->aop) || operandsEqu (result, left))
        {
          // avoid push/pop by finding a free register. S1C88: the only spare
          // byte GPR besides A is B (C/D/E don't exist); fall back to A+push.
          asmop *free_reg = ASMOP_A;
          if (isRegDead (B_IDX, ic))
            free_reg = ASMOP_B;
          cheapMove (ASMOP_A, 0, left->aop, 0, true);
          if (free_reg == ASMOP_A)
            {
              _push (PAIR_AF);
              pushed_a = true;
            }
          // if left and result are registers, this should get optimized away
          cheapMove (free_reg, 0, left->aop, 1, FALSE);
          cheapMove (result->aop, (free_reg == ASMOP_A ? 0 : 1), ASMOP_A, 0, FALSE);
          if(pushed_a){
            _pop (PAIR_AF);
          }
          cheapMove (result->aop, (free_reg == ASMOP_A ? 1 : 0), free_reg, 0, TRUE);
        }
      else
        {
          cheapMove (result->aop, 0, left->aop, 1, isRegDead (A_IDX, ic) && left->aop->regs[A_IDX] != 0);
          cheapMove (result->aop, 1, left->aop, 0, isRegDead (A_IDX, ic) && result->aop->regs[A_IDX] != 0);
        }
      break;

    case 4: // swap words in double word
        if (result->aop->type == AOP_REG) // Create result asmop with swapped words, let genMove handle the details.
        {
          signed char idxarray[5];
          idxarray[0] = result->aop->aopu.aop_reg[2]->rIdx;
          idxarray[1] = result->aop->aopu.aop_reg[3]->rIdx;
          idxarray[2] = result->aop->aopu.aop_reg[0]->rIdx;
          idxarray[3] = result->aop->aopu.aop_reg[1]->rIdx;
          idxarray[4] = -1;
          s1c88_init_reg_asmop(&swapped_result_aop, idxarray);
          genMove (&swapped_result_aop, left->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
          break;
        }
      if (operandsEqu (result, left) && left->aop->type == AOP_STK && spOffset (left->aop->aopu.aop_stk) == 0 && isPairDead (PAIR_HL, ic) && isPairDead (PAIR_BA, ic))
        { /* result & left are top of stack and both scratch pairs are free
            , hl; push hl) */
          _pop (PAIR_HL);
          _pop (PAIR_BA);
          _push (PAIR_HL);
          _push (PAIR_BA);
          break;
        }

      /* --== generic implementations ==-- */
      if (!operandsEqu (result, left))
        { /* result is registers or left differs than result */
          bool pushed = !isRegDead (A_IDX, ic);
          if (pushed)
            _push (PAIR_AF);
          cheapMove (ASMOP_A, 0, left->aop, 0, TRUE);
          cheapMove (result->aop, 0, left->aop, 2, FALSE);
          cheapMove (result->aop, 2, ASMOP_A, 0, FALSE);
          cheapMove (ASMOP_A, 0, left->aop, 1, TRUE);
          cheapMove (result->aop, 1, left->aop, 3, FALSE);
          cheapMove (result->aop, 3, ASMOP_A, 0, FALSE);
          if (pushed)
            _pop (PAIR_AF);
        }
      else
        {
          /* S1C88 temp-pair candidates: BA, HL, IY. */
          asmop *tmp = NULL;
          PAIR_ID tmppair;
          bool pushed = false;
          bool dead_a = isRegDead (A_IDX, ic);
          bool dead_hl = isPairDead (PAIR_HL, ic);
          if (isPairDead (PAIR_BA, ic))
            { tmp = ASMOP_BA; tmppair = PAIR_BA; }
          else if (dead_hl &&
              (left->aop->type != AOP_REG || !aopInReg (left->aop, 0, HL_IDX)))
            { tmp = ASMOP_HL; tmppair = PAIR_HL; }
          else if (!IY_RESERVED && isPairDead (PAIR_IY, ic) &&
                   (left->aop->type != AOP_REG || !aopInReg (left->aop, 0, IY_IDX)))
            { tmp = ASMOP_IY; tmppair = PAIR_IY; }
          else 
            {
              pushed = true;
              if ((left->aop->type != AOP_REG || !aopInReg (left->aop, 0, HL_IDX)))
                { tmp = ASMOP_HL; tmppair = PAIR_HL; }
              else
                { tmp = ASMOP_BA; tmppair = PAIR_BA; }
              _push (tmppair);
            }
          genMove_o (tmp, 0, left->aop, 0, 2, dead_a && tmp != ASMOP_BA, dead_hl && !aopInReg (left->aop, 2, HL_IDX), true, true);
          genMove_o (result->aop, 0, left->aop, 2, 2, dead_a && tmp != ASMOP_BA, dead_hl && (tmp != ASMOP_HL), true, true);
          genMove_o (result->aop, 2, tmp, 0, 2, dead_a && tmp != ASMOP_BA, dead_hl && !aopInReg (result->aop, 0, HL_IDX), true, true);
          if (pushed)
            _pop (tmppair);
        }
      break;
    default:
      wassertl (FALSE, "unsupported SWAP operand size");
    }

  freeAsmop (result, NULL);
  freeAsmop (left, NULL);
}

/*-----------------------------------------------------------------*/
/* AccRol - rotate left accumulator by known count                 */
/*-----------------------------------------------------------------*/
static void
AccRol (int shCount)
{
  shCount &= 0x0007;            // shCount : 0..7

  switch (shCount)
    {
    case 4:
      
      emit3 (A_RLC, ASMOP_A, 0);
    case 3:
      
      emit3 (A_RLC, ASMOP_A, 0);
    case 2:
      emit3 (A_RLC, ASMOP_A, 0);
    case 1:
      emit3 (A_RLC, ASMOP_A, 0);
    case 0:
      break;
    case 5:
      
      emit3 (A_RRC, ASMOP_A, 0);
    case 6:
      emit3 (A_RRC, ASMOP_A, 0);
    case 7:
      emit3 (A_RRC, ASMOP_A, 0);
      break;
    }
}

/*-----------------------------------------------------------------*/
/* AccLsh - left shift accumulator by known count                  */
/*-----------------------------------------------------------------*/
static void
AccLsh (unsigned int shCount)
{
  static const unsigned char SLMask[] =
  {
    0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80, 0x00
  };

  if (shCount <= 4)
    while (shCount--)
      emit3 (A_ADD, ASMOP_A, ASMOP_A);
  else
    {
      /* rotate left accumulator */
      AccRol (shCount);
      /* and kill the lower order bits */
      emit2 ("and a, !immedbyte", (unsigned)(SLMask[shCount]));
      cost2 (2, 7);
    }
}

/*-----------------------------------------------------------------*/
/* shiftL1Left2Result - shift left one byte from left to result    */
/*-----------------------------------------------------------------*/
static void
shiftL1Left2Result (operand *left, int offl, operand *result, int offr, unsigned int shCount, const iCode *ic)
{
  if (!shCount)
    cheapMove (result->aop, offr, left->aop, offl, isRegDead (A_IDX, ic));
  // add hl, hl is cheap in code size.
  else if (aopSame (result->aop, offr, left->aop, offr, 1) && !offr && shCount == 4 && isPairDead (PAIR_HL, ic) && isRegDead (A_IDX, ic) &&
    (result->aop->type == AOP_DIR || result->aop->type == AOP_HL || result->aop->type == AOP_IY))
    {
      emit2 ("xor a, a");
      cost2 (1, 4);
      pointPairToAop (PAIR_HL, result->aop, 0);
      emit3 (A_RLD, 0, 0);
    }
  /* If operand and result are the same we can shift in place.
     However shifting in acc using add is cheaper than shifting
     in place using sla; when shifting by more than 2 shifting in
     acc it is worth the additional effort for loading from / to acc. */
  else if (!aopInReg(result->aop, 0, A_IDX) && sameRegs (left->aop, result->aop) && shCount <= 2 && offr == offl)
    {
      while (shCount--)
        emit3_shift (A_SLA, result->aop, 0, ic);
    }
  else {
      if (!isRegDead (A_IDX, ic))
        _push (PAIR_AF);
      cheapMove (ASMOP_A, 0, left->aop, offl, true);
      /* shift left accumulator */
      AccLsh (shCount);
      cheapMove (result->aop, offr, ASMOP_A, 0, true);
      if (!isRegDead (A_IDX, ic))
        _pop (PAIR_AF);
    }


  sym_link *resulttype = operandType (IC_RESULT (ic));
  unsigned topbytemask = (IS_BITINT (resulttype) && SPEC_USIGN (resulttype) && (SPEC_BITINTWIDTH (resulttype) % 8)) ?
    (0xff >> (8 - SPEC_BITINTWIDTH (resulttype) % 8)) : 0xff;
  bool maskedtopbyte = (topbytemask != 0xff);
  if (maskedtopbyte)
    {
      bool pushed_a = false;
      if (!isRegDead (A_IDX, ic) || result->aop->regs[A_IDX] >= 0 && result->aop->regs[A_IDX] != result->aop->size - 1)
        {
          _push (PAIR_AF);
          pushed_a = true;
        }
      cheapMove (ASMOP_A, 0, result->aop, result->aop->size - 1, true);
      emit2 ("and a, #0x%02x", topbytemask);
      cost (1, 7);
      cheapMove (result->aop, result->aop->size - 1, ASMOP_A, 0, true);
      if (pushed_a)
        _pop (PAIR_AF);
    }
}

/*-----------------------------------------------------------------*/
/* genlshTwo - left shift two bytes by known amount                */
/*-----------------------------------------------------------------*/
static void
genlshTwo (operand *result, operand *left, unsigned int shCount, const iCode *ic)
{
  int size = result->aop->size;

  wassert (size == 2);

  if (shCount >= 8)
    {
      shCount -= 8;
      shiftL1Left2Result (left, 0, result, 1, shCount, ic);
      cheapMove (result->aop, 0, ASMOP_ZERO, 0, isRegDead (A_IDX, ic));
    }
  else
    shiftL2Left2Result (left, result, shCount, ic);
}

/*-----------------------------------------------------------------*/
/* genRot1 - generates code for rotation of 8-bit values           */
/*-----------------------------------------------------------------*/
static void
genRot1 (iCode *ic)
{
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  operand *result = IC_RESULT (ic);

  aopOp (left, ic, false, false);
  aopOp (result, ic, true, false);

  wassert (bitsForType (operandType (left)) == 8);
  wassert (IS_OP_LITERAL (right));

  int s = operandLitValueUll (right) % 8;
  
  if ((s == 1 || s == 7) && result->aop->type == AOP_REG && !aopInReg (result->aop, 0, A_IDX) && !(aopInReg (left->aop, 0, A_IDX) && isRegDead (A_IDX, ic)))
    {
      cheapMove (result->aop, 0, left->aop, 0, true);
      emit3 (s == 1 ? A_RLC : A_RRC, result->aop, 0);
    }
  else if ((s == 1 || s == 7) && left->aop->type == AOP_REG && !bitVectBitValue (ic->rSurv, left->aop->aopu.aop_reg[0]->rIdx) && !aopInReg (left->aop, 0, A_IDX))
    {
      emit3 (s == 1 ? A_RLC : A_RRC, left->aop, 0);
      cheapMove (result->aop, 0, left->aop, 0, true);
    }
  else if (s <= 2 && aopInReg (result->aop, 0, H_IDX) && isRegDead (L_IDX, ic) && (aopInReg (left->aop, 0, H_IDX) || aopInReg (left->aop, 0, L_IDX)))
    {
      if (aopInReg (left->aop, 0, H_IDX))
        emit3_o (A_LD, ASMOP_HL, 0, ASMOP_HL, 1);
      else
        emit3_o (A_LD, ASMOP_HL, 1, ASMOP_HL, 0);
      while (s--)
        emit3w (A_ADD, ASMOP_HL, ASMOP_HL);
    }
  else if (s == 4 && (aopInReg (left->aop, 0, A_IDX) || aopSame (result->aop, 0, left->aop, 0, 1)) &&
    (result->aop->type == AOP_DIR || result->aop->type == AOP_HL || result->aop->type == AOP_IY) && isPairDead (PAIR_HL, ic))
    {
      if (!isRegDead (A_IDX, ic))
        _push (PAIR_AF);
      if (!aopSame (result->aop, 0, left->aop, 0, 1))
        cheapMove (result->aop, 0, ASMOP_A, 0, false);
      else
        cheapMove (ASMOP_A, 0, result->aop, 0, true);
      pointPairToAop (PAIR_HL, result->aop, 0);
      emit3 (A_RRD, 0, 0);
      if (!isRegDead (A_IDX, ic))
        _pop (PAIR_AF);
    }
  else if ((s == 1 || s == 7) && aopSame (result->aop, 0, left->aop, 0, 1) && ((result->aop->type == AOP_EXSTK) || result->aop->type == AOP_DIR || result->aop->type == AOP_HL || result->aop->type == AOP_IY) && isPairDead (PAIR_HL, ic))
    {
      pointPairToAop (PAIR_HL, result->aop, 0);
      emit2 (s == 1 ? "rlc (hl)" : "rrc (hl)");
      cost2 (2, 15);
    }
  else if ((s == 1 || s == 7) && aopSame (result->aop, 0, left->aop, 0, 1) && result->aop->type == AOP_STK)
    {
      emit3 (s == 1 ? A_RLC : A_RRC, result->aop, 0);
    }
  else
    {
      if (!isRegDead (A_IDX, ic))
        _push (PAIR_AF);
      cheapMove (ASMOP_A, 0, left->aop, 0, true);
      AccRol (s);
      cheapMove (result->aop, 0, ASMOP_A, 0, true);
      if (!isRegDead (A_IDX, ic))
        _pop (PAIR_AF);
    }

  freeAsmop (left, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genRot - generates code for rotation                            */
/*-----------------------------------------------------------------*/
static void
genRot (iCode *ic)
{
  operand *left = IC_LEFT (ic);
  operand *right = IC_RIGHT (ic);
  unsigned int lbits = bitsForType (operandType (left));
  if (lbits == 8 && IS_OP_LITERAL (right))
    genRot1 (ic);
  else if (IS_OP_LITERAL (right) && operandLitValueUll (right) % lbits == 1)
    genRLC (ic);
  else if (IS_OP_LITERAL (right) && operandLitValueUll (right) % lbits == lbits - 1)
    genRRC (ic);
  else if (IS_OP_LITERAL (right) && (operandLitValueUll (right) % lbits) * 2 == lbits)
    genSwap (ic);
  else
    wassertl (0, "Unsupported rotation.");
}

/*------------------------------------------------------------------*/
/* genLeftShiftLiteral - left shifting by known count for size <= 2 */
/*------------------------------------------------------------------*/
static void
genLeftShiftLiteral (operand *left, operand *right, operand *result, const iCode *ic)
{
  unsigned int shCount = ulFromVal (right->aop->aopu.aop_lit);
  unsigned int size;

  freeAsmop (right, NULL);

  aopOp (left, ic, false, false);
  aopOp (result, ic, true, false);

  size = getSize (operandType (result));

  /* I suppose that the left size >= result size */
  wassert (getSize (operandType (left)) >= size);

  if (shCount >= (size * 8))
    genMove (result->aop, ASMOP_ZERO, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic) && left->aop->regs[H_IDX] < 0 && left->aop->regs[L_IDX] < 0, true);
  else
    {
      switch (size)
        {
        case 1:
          shiftL1Left2Result (left, 0, result, 0, shCount, ic);
          break;
        case 2:
          genlshTwo (result, left, shCount, ic);
          break;
        case 4:
          wassertl (0, "Shifting of longs should be handled by generic function.");
          break;
        default:
          wassert (0);
        }
    }
  freeAsmop (left, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genLeftShift - generates code for left shifting                 */
/*-----------------------------------------------------------------*/
static void
genLeftShift (const iCode *ic)
{
  int size, offset;
  symbol *tlbl = 0, *tlbl1 = 0;
  operand *left, *right, *result;
  asmop *shiftop;
  int countreg;
  bool count_iy = false;        /* S1C88: 16-bit IY loop counter (no free byte GPR) */
  bool shift_by_lit;
  int shiftcount = 0;
  int byteshift = 0;
  bool started;
  bool save_a_outer = false;

  right = IC_RIGHT (ic);
  left = IC_LEFT (ic);
  result = IC_RESULT (ic);

  aopOp (right, ic, FALSE, FALSE);

  /* if the shift count is known then do it
     as efficiently as possible */
  if (right->aop->type == AOP_LIT && getSize (operandType (result)) <= 2)
    {
      genLeftShiftLiteral (left, right, result, ic);
      freeAsmop (right, NULL);
      return;
    }

  /* Useful for the case of shifting a size > 2 value by a literal */
  shift_by_lit = right->aop->type == AOP_LIT;
  if (shift_by_lit)
    shiftcount = ulFromVal (right->aop->aopu.aop_lit);

  aopOp (result, ic, true, false);
  aopOp (left, ic, false, false);

  bool z80n_de = ((result->aop->size == 2 && (0 || 0) ||
    result->aop->size == 1 && (0 || 0))) && true;

  if (right->aop->type == AOP_REG && !bitVectBitValue (ic->rSurv, right->aop->aopu.aop_reg[0]->rIdx) && right->aop->aopu.aop_reg[0]->rIdx != IYL_IDX && (sameRegs (left->aop, result->aop) || left->aop->type != AOP_REG) &&
    (result->aop->type != AOP_REG ||
    result->aop->aopu.aop_reg[0]->rIdx != right->aop->aopu.aop_reg[0]->rIdx &&
    !aopInReg (result->aop, 0, right->aop->aopu.aop_reg[0]->rIdx) && !aopInReg (result->aop, 1, right->aop->aopu.aop_reg[0]->rIdx) && !aopInReg (result->aop, 2, right->aop->aopu.aop_reg[0]->rIdx) && !aopInReg (result->aop, 3, right->aop->aopu.aop_reg[0]->rIdx)))
    countreg = right->aop->aopu.aop_reg[0]->rIdx;
  else if (isRegDead (B_IDX, ic) && (sameRegs (left->aop, result->aop) || left->aop->type != AOP_REG || shift_by_lit) &&
    !aopInReg (result->aop, 0, B_IDX) && !aopInReg (result->aop, 1, B_IDX) && !aopInReg (result->aop, 2, B_IDX) && !aopInReg (result->aop, 3, B_IDX))
    countreg = B_IDX;
  else if (isRegDead (A_IDX, ic) && result->aop->regs[A_IDX] < 0 && left->aop->regs[A_IDX] < 0)
    countreg = A_IDX;
  else if (isRegDead (B_IDX, ic) && result->aop->regs[B_IDX] < 0 && left->aop->regs[B_IDX] < 0)
    countreg = B_IDX;
  /* S1C88: when all four byte GPRs (A/B/L/H) hold the value, count, or result —
     e.g. int<<int with value=BA, count=HL, result reusing HL — there is no free
     byte register for the counter. Use IY instead:
     `dec iy` sets Z (16-bit dec sets Z V N), so `dec iy; jr nz` is a valid loop. */
  else if (!IY_RESERVED && isPairDead (PAIR_IY, ic) &&
    result->aop->regs[IYL_IDX] < 0 && result->aop->regs[IYH_IDX] < 0 &&
    left->aop->regs[IYL_IDX] < 0 && left->aop->regs[IYH_IDX] < 0)
    {
      countreg = IYL_IDX;
      count_iy = true;
    }
  else {
      UNIMPLEMENTED;
      countreg = A_IDX;
    }

  

  save_a_outer = (!isRegDead (A_IDX, ic) && countreg == A_IDX && !(shift_by_lit && shiftcount == 1));
  
  if(save_a_outer)
    _push (PAIR_AF);
    
  if (!shift_by_lit && count_iy)
    genMove (ASMOP_IY, right->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
  else if (!shift_by_lit)
    cheapMove (asmopregs[countreg], 0, right->aop, 0, true);

  bool save_a_inner = (countreg == A_IDX && !shift_by_lit) &&
    !(left->aop->type == AOP_REG && result->aop->type != AOP_REG ||
    (left->aop->type == AOP_STK && canAssignToPtr3 (result->aop) || result->aop->type == AOP_STK && canAssignToPtr3 (left->aop)));

  shiftop = result->aop;
  if (result->aop->type != AOP_REG && left->aop->type == AOP_REG && result->aop->size == left->aop->size && left->aop->regs[countreg] < 0)
    {
      bool left_dead = true;
      for (int i = 0; i < left->aop->size; i++)
        left_dead &= isRegDead (left->aop->aopu.aop_reg[i]->rIdx, ic);
      if (left_dead)
        shiftop = left->aop;
    }

  /* now move the left to the result if they are not the
     same */
  if (!sameRegs (shiftop, left->aop) || shiftop->type == AOP_REG)
    {
      if (save_a_inner)
        _push (PAIR_AF);

      if (shift_by_lit)
      {
        byteshift = shiftcount / 8;
        shiftcount %= 8;
      }
      size = shiftop->size - byteshift;
      int lsize = left->aop->size - byteshift;

      bool hl_dead = isPairDead (PAIR_HL, ic) && (countreg != L_IDX && countreg != H_IDX || shift_by_lit);
      genMove_o (shiftop, byteshift, left->aop, 0, size <= lsize ? size : lsize, (save_a_inner || countreg != A_IDX) && (isRegDead (A_IDX, ic) || save_a_outer), hl_dead, !count_iy, true);
      hl_dead &= shiftop->regs[L_IDX] < byteshift && shiftop->regs[L_IDX] < byteshift;
      genMove_o (shiftop, 0, ASMOP_ZERO, 0, byteshift, (save_a_inner || countreg != A_IDX) && (isRegDead (A_IDX, ic) || save_a_outer), hl_dead, !count_iy, true);

      if (save_a_inner)
        _pop (PAIR_AF);
    }
  shiftop->valinfo.anything = true;

  if (!regalloc_dry_run)
    {
      tlbl = newiTempLabel (NULL);
      tlbl1 = newiTempLabel (NULL);
    }
  size = shiftop->size - byteshift;
  offset = byteshift;

  if (shift_by_lit && !shiftcount)
    goto end;
  if (shift_by_lit && shiftcount > 1)
    {
      if (count_iy)
        {
          emit2 ("ld iy, !immedword", (unsigned)shiftcount);
          cost2 (4, 14);
        }
      else
        {
          emit2 ("ld %s, !immedbyte", countreg == A_IDX ? "a" : regsS1C88[countreg].name, (unsigned)shiftcount);
          cost2 (2, 7);
        }
    }
  else if (!shift_by_lit && !aopIsNotLitVal (right->aop, 0, 1, 0))
    {
      emit2 ("inc %s", count_iy ? "iy" : (countreg == A_IDX ? "a" : regsS1C88[countreg].name));
      cost2 (1, 4);
      if (!regalloc_dry_run)
        emit2 ("jp !tlabel", labelKey2num (tlbl1->key));
      cost2 (3, 10);
    }
  if (!(shift_by_lit && shiftcount == 1) && !regalloc_dry_run)
    {
      emitLabel (tlbl);
      if (requiresHL (shiftop))
        spillPair (PAIR_HL);
    }

  started = false;
  regalloc_dry_run_state_scale = shift_by_lit ? shiftcount : 2;
  while (size)
    {
      if (size >= 2 && offset + 1 >= byteshift && shiftop->type == AOP_REG && (getPartPairId (shiftop, offset) == PAIR_HL || !started && getPartPairId (shiftop, offset) == PAIR_IY))
        {
          if (shiftop->aopu.aop_reg[offset]->rIdx == L_IDX)
            emit3w (started ? A_ADC : A_ADD, ASMOP_HL, ASMOP_HL);
          else if (shiftop->aopu.aop_reg[offset]->rIdx == IYL_IDX)
            emit3w (A_ADD, ASMOP_IY, ASMOP_IY);
          else
            wassert (0); /* partPair HL implies the L half; IY only when !started */

          started = true;
          size -= 2, offset += 2;
        }
      else
        {
          if (offset >= byteshift)
            {
              if (aopInReg (shiftop, offset, A_IDX))
                emit3 (started ? A_ADC : A_ADD, ASMOP_A, ASMOP_A);
              else
                emit3_shift (started ? A_RL : A_SLA, shiftop, offset, ic);
              started = true;
            }
          size--, offset++;
        }
    }

  if (!(shift_by_lit && shiftcount == 1))
    {
      if (!regalloc_dry_run)
        emitLabel (tlbl1);
      if (count_iy)
        {
          emit2 ("dec iy");
          cost2 (1, 4);
          if (!regalloc_dry_run)
            emit2 ("jr NZ,!tlabel", labelKey2num (tlbl->key));
          cost2 (2, 12); // Assume jump taken, and optimized to jr.
        }
      else if (countreg == B_IDX)
        {
          if (!regalloc_dry_run)
            emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
          cost2 (2, 13); // Assume jump taken.
        }
      else
        {
          emit2 ("dec %s", regsS1C88[countreg].name);
          cost2 (1, 4);
          if (!regalloc_dry_run)
            emit2 ("jr NZ,!tlabel", labelKey2num (tlbl->key));
          cost2 (2, 12); // Assume jump taken, and optimized to jr.
        }
    }

end:
  regalloc_dry_run_state_scale = 1.0f;

  if (!shift_by_lit && requiresHL (shiftop)) // Shift by 0 skips over hl adjustments.
    spillPair (PAIR_HL);

  sym_link *resulttype = operandType (IC_RESULT (ic));
  unsigned topbytemask = (IS_BITINT (resulttype) && SPEC_USIGN (resulttype) && (SPEC_BITINTWIDTH (resulttype) % 8)) ?
    (0xff >> (8 - SPEC_BITINTWIDTH (resulttype) % 8)) : 0xff;
  bool maskedtopbyte = (topbytemask != 0xff);
  if (maskedtopbyte)
    {
      bool pushed_a = false;
      if (!isRegDead (A_IDX, ic) || shiftop->regs[A_IDX] >= 0 && shiftop->regs[A_IDX] != result->aop->size - 1)
        {
          _push (PAIR_AF);
          pushed_a = true;
        }
      cheapMove (ASMOP_A, 0, shiftop, result->aop->size - 1, true);
      emit2 ("and a, #0x%02x", topbytemask);
      cost2 (2, 7);
      cheapMove (shiftop, result->aop->size - 1, ASMOP_A, 0, true);
      if (pushed_a)
        _pop (PAIR_AF);
    }

  genMove_o (result->aop, 0, shiftop, 0, result->aop->size, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true, true);

  if (save_a_outer)
    _pop (PAIR_AF);

  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* AccRsh - right shift accumulator by known count                 */
/*-----------------------------------------------------------------*/
static void
AccRsh (int shCount)
{
  if (shCount >= 2)
    {
      /* rotate right accumulator */
      AccRol (8 - shCount);
      /* and kill the higher order bits */
      if (!regalloc_dry_run)
        emit2 ("and a, !immedbyte", 0xffu >> shCount);
      cost2 (2, 7);
    }
  else if(shCount)
    emit3 (A_SRL, ASMOP_A, 0);
}

/*-----------------------------------------------------------------*/
/* genrshOne - right shift one byte by known amount                */
/*-----------------------------------------------------------------*/
static void
genrshOne (operand *result, operand *left, int shCount, int is_signed, const iCode *ic)
{
  /* Errk */
  int size = result->aop->size;

  wassert (size == 1);

  bool a_dead = isRegDead (A_IDX, ic);

  if (!is_signed && aopSame (result->aop, 0, left->aop, 0, 1) && shCount == 4 && isPairDead (PAIR_HL, ic) && isRegDead (A_IDX, ic) &&
    (result->aop->type == AOP_DIR || result->aop->type == AOP_HL || result->aop->type == AOP_IY))
    {
      emit3 (A_XOR, ASMOP_A, ASMOP_A);
      pointPairToAop (PAIR_HL, result->aop, 0);
      emit3 (A_RRD, 0, 0);
    }
  else if (!is_signed && // Shifting in the accumulator is cheap for unsigned operands.
    (aopInReg (result->aop, 0, A_IDX) ||
    result->aop->type != AOP_REG ||
    (shCount >= 4 + 2 * a_dead || shCount >= 2 * a_dead && aopInReg (left->aop, 0, A_IDX))))
    {
      if (!a_dead)
        _push (PAIR_AF);
      cheapMove (ASMOP_A, 0, left->aop, 0, true);
      AccRsh (shCount);
      cheapMove (result->aop, 0, ASMOP_A, 0, true);
      if (!a_dead)
        _pop (PAIR_AF);
    }
  else if (result->aop->type == AOP_REG && (aopInReg (result->aop, 0, A_IDX) || aopInReg (result->aop, 0, B_IDX)))
    {
      /* S1C88 shifts only a/b/(hl) — shift in the destination register only when
         it is A or B. */
      cheapMove (result->aop, 0, left->aop, 0, a_dead);

      while (shCount--)
        emit3 (is_signed ? A_SRA : A_SRL, result->aop, 0);
    }
  else
    {
      /* result is L/H or memory: shift in A, then store (unsigned uses the
         cheaper rlc-based AccRsh, as in shiftR1Left2Result). */
      if (!a_dead)
        _push (PAIR_AF);
      cheapMove (ASMOP_A, 0, left->aop, 0, true);
      if (is_signed)
        while (shCount--)
          emit3 (A_SRA, ASMOP_A, 0);
      else
        AccRsh (shCount);
      cheapMove (result->aop, 0, ASMOP_A, 0, true);
      if (!a_dead)
        _pop (PAIR_AF);
    }
}

/*-----------------------------------------------------------------*/
/* shiftR1Left2Result - shift right one byte from left to result   */
/*-----------------------------------------------------------------*/
static void
shiftR1Left2Result (operand *left, int offl, operand *result, int offr, int shCount, int sign)
{
  cheapMove (ASMOP_A, 0, left->aop, offl, true);
  if (sign)
    {
      while (shCount--)
        emit3 (sign ? A_SRA : A_SRL, ASMOP_A, 0);
    }
  else
    AccRsh (shCount);
  cheapMove (result->aop, offr, ASMOP_A, 0, true);
}

/*-----------------------------------------------------------------*/
/* genrshTwo - right shift two bytes by known amount               */
/*-----------------------------------------------------------------*/
static void
genrshTwo (const iCode *ic, operand *result, operand *left, int shCount, int sign)
{
  if (shCount >= 8)
    {
      shCount -= 8;
      if (shCount)
        shiftR1Left2Result (left, MSB16, result, LSB, shCount, sign);
      else
        cheapMove (result->aop, 0, left->aop, 1, isRegDead (A_IDX, ic));
      if (sign)
        {
          /* Sign extend the result */
          if (result->aop->type != AOP_REG && left->aop->type == AOP_REG)
            cheapMove (ASMOP_A, 0, left->aop, 1, true);
          else
            cheapMove (ASMOP_A, 0, result->aop, 0, true);
          emit3 (A_RLC, ASMOP_A, 0);
          emit3 (A_SBC, ASMOP_A, ASMOP_A);
          cheapMove (result->aop, 1, ASMOP_A, 0, true);
        }
      else
        cheapMove (result->aop, 1, ASMOP_ZERO, 0, true);
    }
  else
    shiftR2Left2Result (ic, left, LSB, result, LSB, shCount, sign);
}

/*-----------------------------------------------------------------*/
/* genRightShiftLiteral - right shifting by known count              */
/*-----------------------------------------------------------------*/
static void
genRightShiftLiteral (operand *left, operand *right, operand *result, const iCode *ic, int sign)
{
  unsigned int shCount = (unsigned int) ulFromVal (right->aop->aopu.aop_lit);
  unsigned int size;

  freeAsmop (right, NULL);

  aopOp (left, ic, false, false);
  aopOp (result, ic, true, false);

  size = getSize (operandType (result));

  /* I suppose that the left size >= result size */
  wassert (getSize (operandType (left)) >= size);

  if (shCount >= (size * 8))
    {
      if (!SPEC_USIGN (getSpec (operandType (left))))
        {
          cheapMove (ASMOP_A, 0, left->aop, 0, true);
          emit3 (A_RLC, ASMOP_A, 0);
          emit3 (A_SBC, ASMOP_A, ASMOP_A);
          while (size--)
            cheapMove (result->aop, size, ASMOP_A, 0, true);
        }
      else
        genMove (result->aop, ASMOP_ZERO, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
    }
  else
    {
      switch (size)
        {
        case 1:
          genrshOne (result, left, shCount, sign, ic);
          break;
        case 2:
          genrshTwo (ic, result, left, shCount, sign);
          break;
        case 4:
          wassertl (0, "Asked to shift right a long which should be handled in generic right shift function.");
          break;
        default:
          wassertl (0, "Entered default case in right shift delegate");
        }
    }
  freeAsmop (left, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genRightShift - generate code for right shifting                */
/*-----------------------------------------------------------------*/
static void
genRightShift (const iCode * ic)
{
  operand *right, *left, *result;
  asmop *shiftop;
  int size, offset, first = 1;
  bool is_signed;
  int countreg;
  bool shift_by_lit, shift_by_one, shift_by_zero;
  int shiftcount = 0;
  int byteoffset = 0;
  bool save_a;

  symbol *tlbl = 0, *tlbl1 = 0;

  right = IC_RIGHT (ic);
  left = IC_LEFT (ic);
  result = IC_RESULT (ic);

  /* if signed then we do it the hard way preserve the
     sign bit moving it inwards */
  is_signed = !SPEC_USIGN (getSpec (operandType (left)));

  aopOp (right, ic, FALSE, FALSE);

  /* if the shift count is known then do it
     as efficiently as possible */
  if (right->aop->type == AOP_LIT && getSize (operandType (result)) <= 2)
    {
      genRightShiftLiteral (left, right, result, ic, is_signed);
      freeAsmop (right, NULL);
      return;
    }

  /* Useful for the case of shifting a size > 2 value by a literal */
  shift_by_lit = right->aop->type == AOP_LIT;
  if (shift_by_lit)
    shiftcount = ulFromVal (right->aop->aopu.aop_lit);

  aopOp (result, ic, true, false);
  aopOp (left, ic, false, false);
    
  /* Count-register selection — an S1C88 constraint: only a/b/(hl) shift, so the multi-byte shift body
     routes the value's L/H (and memory) bytes through A as scratch
     (emit3_shift) — using A as the counter clobbers it mid-body and the loop
     never terminates. So the count must be a dead byte register the value
     (left/result) does not occupy, and NEVER A. B is preferred (cheap
     `djr nz`), then H/L; the count's own dead register is reused when suitable.
     (B/H/L are always safe: the body's scratch is A — or B only when the value
     is BA, in which case B is rejected here by the disjoint-from-value test.) */
  countreg = -1;
  if (right->aop->type == AOP_REG && !bitVectBitValue (ic->rSurv, right->aop->aopu.aop_reg[0]->rIdx) &&
      right->aop->aopu.aop_reg[0]->rIdx != IYL_IDX && right->aop->aopu.aop_reg[0]->rIdx != A_IDX &&
      (sameRegs (left->aop, result->aop) || left->aop->type != AOP_REG) &&
      left->aop->regs[right->aop->aopu.aop_reg[0]->rIdx] < 0 &&
      result->aop->regs[right->aop->aopu.aop_reg[0]->rIdx] < 0)
    countreg = right->aop->aopu.aop_reg[0]->rIdx;
  else
    {
      const int cand[3] = { B_IDX, H_IDX, L_IDX };
      for (int ci = 0; ci < 3 && countreg < 0; ci++)
        if (isRegDead (cand[ci], ic) && left->aop->regs[cand[ci]] < 0 && result->aop->regs[cand[ci]] < 0)
          countreg = cand[ci];
    }
  if (countreg < 0)
    {
      /* No byte register is free for the count in this allocation (left and
         result together span B/L/H, leaving only A — which the body needs as
         scratch). Make this combination prohibitively expensive so the register
         allocator picks a feasible one (e.g. value in BA, which leaves H/L free
         for the count); A keeps the dry-run emission well-formed. */
      UNIMPLEMENTED;
      countreg = A_IDX;
    }

  

  if (!shift_by_lit)
    cheapMove (countreg == A_IDX ? ASMOP_A : asmopregs[countreg], 0, right->aop, 0, true);

  save_a = (countreg == A_IDX && !shift_by_lit) &&
    !(left->aop->type == AOP_REG && result->aop->type != AOP_REG ||
    (left->aop->type == AOP_STK && canAssignToPtr3 (result->aop) || result->aop->type == AOP_STK && canAssignToPtr3 (left->aop)));

  shiftop = result->aop;
  if (result->aop->type != AOP_REG && left->aop->type == AOP_REG && result->aop->size == left->aop->size && left->aop->regs[countreg] < 0)
    {
      bool left_dead = true;
      for (int i = 0; i < left->aop->size; i++)
        left_dead &= isRegDead (left->aop->aopu.aop_reg[i]->rIdx, ic);
      if (left_dead)
        shiftop = left->aop;
    }

  /* now move the left to the shiftop if they are not the
     same */
  if (!sameRegs (shiftop, left->aop) || shiftop->type == AOP_REG)
    {
      int soffset = 0;
      size = shiftop->size;

      if (!is_signed && shift_by_lit)
      {
        byteoffset = shiftcount / 8;
        shiftcount %= 8;
        soffset = byteoffset;
        size -= byteoffset;
      }

      if (save_a)
        _push (PAIR_AF);

      bool hl_dead = isPairDead (PAIR_HL, ic) && (countreg != L_IDX && countreg != H_IDX || shift_by_lit);
      genMove_o (shiftop, 0, left->aop, soffset, size, true, hl_dead, true, true);
      hl_dead &= (shiftop->regs[L_IDX] < 0 || shiftop->regs[L_IDX] >= size) && (shiftop->regs[H_IDX] < 0 || shiftop->regs[H_IDX] >= size);
      genMove_o (shiftop, shiftop->size - byteoffset, ASMOP_ZERO, 0, byteoffset, true, hl_dead, true, true);

      if (save_a)
        _pop (PAIR_AF);
    }
  shiftop->valinfo.anything = true;

  shift_by_one = (shift_by_lit && shiftcount == 1);
  shift_by_zero = (shift_by_lit && shiftcount == 0);

  if (!regalloc_dry_run)
    {
      tlbl = newiTempLabel (NULL);
      tlbl1 = newiTempLabel (NULL);
    }
  size = result->aop->size;
  offset = size - 1;

  if (shift_by_zero)
    goto end;
  else if (shift_by_lit && shiftcount > 1)
    {
      emit2 ("ld %s, !immedbyte", countreg == A_IDX ? "a" : regsS1C88[countreg].name, (unsigned)shiftcount);
      cost2 (2, 7);
    }
  else if (!shift_by_lit && !aopIsNotLitVal (right->aop, 0, 1, 0))
    {
      emit2 ("inc %s", countreg == A_IDX ? "a" : regsS1C88[countreg].name);
      cost2 (1, 4);
      if (!regalloc_dry_run)
        emit2 ("jp !tlabel", labelKey2num (tlbl1->key));
      cost2 (3, 10);
    }
  if (!shift_by_one && !regalloc_dry_run)
    emitLabel (tlbl);

  if (!shift_by_one && requiresHL (shiftop))
    spillPair (PAIR_HL);
    
  regalloc_dry_run_state_scale = shift_by_lit ? shiftcount : 2;
  while (size)
    {
      if (!is_signed && first && byteoffset--) // Skip known 0 bytes
        size--, offset--;
      else if (first)
        {
          emit3_shift (is_signed ? A_SRA : A_SRL, shiftop, offset, ic);
          first = 0;
          size--, offset--;
        }
      else
        {
          emit3_shift (A_RR, shiftop, offset, ic);
          size--, offset--;
        }
    }

  if (!shift_by_one)
    {
      if (!regalloc_dry_run)
        emitLabel (tlbl1);
      if (countreg == B_IDX)
        {
          if (!regalloc_dry_run)
            emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
          cost2 (2, 13); // Assume jump taken.
        }
      else
        {
          emit2 ("dec %s", countreg == A_IDX ? "a" : regsS1C88[countreg].name);
          cost2 (1, 4);
          if (!regalloc_dry_run)
            emit2 ("jr NZ, !tlabel", labelKey2num (tlbl->key));
          cost2 (2, 12); // Assume jump taken, and optimized to jr.
        }
    }

end:
  regalloc_dry_run_state_scale = 1.0f;
  if (!shift_by_lit && requiresHL (shiftop)) // Shift by 0 skips over hl adjustments.
    spillPair (PAIR_HL);

  genMove_o (result->aop, 0, shiftop, 0, result->aop->size, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true, true);

  freeAsmop (left, NULL);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* unpackMaskA - generate masking code for unpacking last byte     */
/* of bitfield. And mask for unsigned, sign extension for signed.  */
/*-----------------------------------------------------------------*/
static void
unpackMaskA(sym_link *type, int len)
{
  if (SPEC_USIGN (type) || len != 1)
    {
      emit2 ("and a, !immedbyte", ((unsigned)-1 & 0xffu) >> (8 - len));
      cost2 (2, 7);
    }
  if (!SPEC_USIGN (type))
    {
      if (len == 1)
        {
          emit3 (A_RR, ASMOP_A, 0);
          emit3(A_SBC, ASMOP_A, ASMOP_A);
        }
      else
        {
          emit2 ("bit a, #0x%02x", 1u << (len - 1));   // S1C88: bit reg,#mask
          cost2 (2, 8);
          if (!regalloc_dry_run)
            {
              symbol *tlbl = newiTempLabel (NULL);
              emit2 ("jp Z, !tlabel", labelKey2num (tlbl->key));
              emit2 ("or a, !immedbyte", ((0xffu << len) & 0xffu));
              emitLabel (tlbl);
            }
          cost2 (4, 12); // Assume nonnegative, jp optimzed to jr.
        }
    }
}

/*-----------------------------------------------------------------*/
/* genUnpackBits - generates code for unpacking bits               */
/*-----------------------------------------------------------------*/
static void
genUnpackBits (operand *result, int pair, const iCode *ic)
{
  int offset = 0;               /* result byte offset */
  int rsize;                    /* result size */
  int rlen = 0;                 /* remaining bit-field length */
  sym_link *etype;              /* bit-field type information */
  unsigned blen;                /* bit-field length */
  unsigned bstr;                /* bit-field starting bit within byte */
  unsigned int pairincrement = 0;

  emitDebug ("; genUnpackBits");

  etype = getSpec (operandType (result));
  rsize = getSize (operandType (result));
  blen = SPEC_BLEN (etype);
  bstr = SPEC_BSTR (etype);

  /* If the bit-field length is less than a byte */
  if (blen < 8)
    {
      emit2 ("ld a, !mems", _pairs[pair].name);
      regalloc_dry_run_cost += (pair == PAIR_IX || pair == PAIR_IY) ? 3 : 1;
      AccRol (8 - bstr);
      unpackMaskA (etype, blen);
      cheapMove (result->aop, offset++, ASMOP_A, 0, true);
      goto finish;
    }

  if (getPairId (result->aop) == PAIR_HL ||
      result->aop->type == AOP_REG && rsize == 2 && (result->aop->aopu.aop_reg[0]->rIdx == L_IDX
          || result->aop->aopu.aop_reg[0]->rIdx == H_IDX))
    {
      emit2 ("!ldahli");
      regalloc_dry_run_cost++;
      if (result->aop->type != AOP_REG || result->aop->aopu.aop_reg[0]->rIdx != H_IDX)
        {
          emit2 ("ld h, !*hl");
          cost2 (1, 7);
          cheapMove (result->aop, offset++, ASMOP_A, 0, true);
          emit3 (A_LD, ASMOP_A, ASMOP_H);
        }
      else
        {
          emit2 ("ld l, !*hl");
          cost2 (1, 7);
          cheapMove (result->aop, offset++, ASMOP_A, 0, true);
          emit3 (A_LD, ASMOP_A, ASMOP_L);
        }
      unpackMaskA (etype, blen - 8);
      cheapMove (result->aop, offset++, ASMOP_A, 0, true);
      spillPair (PAIR_HL);
      return;
    }

  /* Bit field did not fit in a byte. Copy all
     but the partial byte at the end.  */
  for (rlen = blen; rlen >= 8; rlen -= 8)
    {
      emit2 ("ld a, !mems", _pairs[pair].name);
      cost2 (1, 7);
      cheapMove (result->aop, offset++, ASMOP_A, 0, true);
      if (rlen > 8)
        {
          emit2 ("inc %s", _pairs[pair].name);
          cost2 (1, 6);
          _G.pairs[pair].offset++;
          pairincrement++;
        }
    }

  /* Handle the partial byte at the end */
  if (rlen)
    {
      emit2 ("ld a, !mems", _pairs[pair].name);
      cost2 (1, 7);
      unpackMaskA (etype, rlen);
      cheapMove (result->aop, offset++, ASMOP_A, 0, true);
    }

finish:
  if (!isPairDead (pair, ic))
    while (pairincrement)
      {
        emit2 ("dec %s", _pairs[pair].name);
        cost2 (1, 6);
        pairincrement--;
        _G.pairs[pair].offset--;
      }

  if (offset < rsize)
    {
      asmop *source;

      if (SPEC_USIGN (etype))
        source = ASMOP_ZERO;
      else
        {
          /* signed bit-field: sign extension with 0x00 or 0xff */
          emit3 (A_RL, ASMOP_A, 0);
          emit3 (A_SBC, ASMOP_A, ASMOP_A);
          source = ASMOP_A;
        }
      rsize -= offset;
      while (rsize--)
        cheapMove (result->aop, offset++, source, 0, true);
    }
}

static void
_moveFrom_tpair_ (asmop * aop, int offset, PAIR_ID pair)
{
  emitDebug ("; _moveFrom_tpair_()");
  if (pair == PAIR_HL && aop->type == AOP_REG)
    {
      if (!regalloc_dry_run)
        aopPut (aop, "!*hl", offset);
      ld_cost (aop, 0, aopInReg (aop, 0, A_IDX) ? ASMOP_L : ASMOP_A, 0, true);
    }
  else
    {
      emit2 ("ld a, !mems", _pairs[pair].name);
      cost2 (1, 7);
      cheapMove (aop, offset, ASMOP_A, 0, true);
    }
}

static void offsetPair (PAIR_ID pair, int val)
{
  // S1C88: HL/IX/IY all have a native `add pair,#imm` (3 B), so no scratch pair
  // (DE/BC) and no push/pop are needed to add a constant offset. Small offsets
  // are still cheaper as inc/dec (1 B each).
  if (abs (val) < 4 || pair != PAIR_HL && pair != PAIR_IY && pair != PAIR_IX)
    {
      while (val)
        {
          emit2 (val > 0 ? "inc %s" : "dec %s", _pairs[pair].name);
          cost2 (1, 6);
          if (val > 0)
            val--;
          else
            val++;
        }
    }
  else
    {
      emit2 ("add %s, !immedword", _pairs[pair].name, (unsigned)val);
      cost2 (3, 10);   // add {hl,ix,iy},#mmnn
    }
}

/*------------------------------------------------------------------*/
/* init_stackop - initialize asmop for stack location                */
/*------------------------------------------------------------------*/
static void 
init_stackop (asmop *stackop, int size, long int stk_off)
{
  stackop->size = size;
  memset (stackop->regs, -1, 9);
  stackop->aopu.aop_stk = stk_off;

  if ((_G.omitFramePtr || stk_off < INT8MIN || stk_off > (int) (INT8MAX - size)))
    stackop->type = AOP_EXSTK;
  else
    stackop->type = AOP_STK;

  stackop->valinfo.anything = true;
}

/*-----------------------------------------------------------------*/
/* genFarUnpackBits - read a bit-field through a 3-byte __far      */
/* pointer.  The raw byte(s) are fetched via the HL+EP idiom; all  */
/* mask/shift/sign work happens at EP=0 in registers, and the      */
/* result store is plain near codegen afterwards — so any near-    */
/* writable result aop (registers, frame, absolutes) works.        */
/* char/int fields only (blen <= 16): wider fields keep the loud   */
/* UNIMPLEMENTED trap (dry-run cost steers the allocator away).    */
/*-----------------------------------------------------------------*/
static void
genFarUnpackBits (const iCode *ic, operand *left, operand *result, int rightval)
{
  sym_link *etype = getSpec (operandType (result));
  int rsize = getSize (operandType (result));
  unsigned blen = SPEC_BLEN (etype);
  unsigned bstr = SPEC_BSTR (etype);
  bool a_dead = isRegDead (A_IDX, ic);
  bool b_dead = isRegDead (B_IDX, ic);
  bool hl_dead = isPairDead (PAIR_HL, ic);
  bool result_in_a = result->aop->type == AOP_REG && result->aop->regs[A_IDX] >= 0;
  bool result_in_b = result->aop->type == AOP_REG && result->aop->regs[B_IDX] >= 0;
  bool result_in_hl = result->aop->type == AOP_REG && (result->aop->regs[L_IDX] >= 0 || result->aop->regs[H_IDX] >= 0);
  bool pushed_hl = false, pushed_a = false, pushed_b = false;
  /* B carries the second raw byte / the sign-or-zero high byte */
  bool need_b = blen > 8 || rsize == 2;

  if (rsize > 2 || blen > 16)
    {
      UNIMPLEMENTED;
      return;
    }
  wassert (blen <= 8 || bstr == 0);  /* multi-byte fields are byte-aligned */
  wassert (blen <= 8 || rsize == 2);

  if (!hl_dead && !result_in_hl)
    _push (PAIR_HL), pushed_hl = true;
  if (!a_dead && !result_in_a)
    {
      emit2 ("push a");
      cost (1, 3);
      _G.stack.pushed += 1;
      pushed_a = true;
    }
  if (need_b && !b_dead && !result_in_b)
    {
      emit2 ("push b");
      cost (1, 3);
      _G.stack.pushed += 1;
      pushed_b = true;
    }

  /* pointer -> HLA (offset -> HL, page -> A), page -> EP, displacement */
  genMove (ASMOP_HLA, left->aop, true, true, isPairDead (PAIR_IY, ic));
  emit2 ("ld ep, a");
  cost (2, 2);
  if (rightval)
    {
      emit2 ("add hl, !immed%d", rightval);
      cost (3, 3);
    }

  if (blen <= 8)
    {
      /* single byte: fetch raw, restore EP, then mask/extend in registers */
      emit2 ("ld a, !mems", "hl");
      cost (1, 2);
      emit2 ("ld ep, #0x00");
      cost (3, 3);
      if (bstr)
        AccRol (8 - bstr);
      if (blen < 8)
        unpackMaskA (etype, blen);
      if (rsize == 2)
        {
          if (!SPEC_USIGN (etype))
            {
              emit2 ("sep");        /* B = sign-extension of A */
              cost (2, 3);
            }
          else
            {
              emit2 ("ld b, #0x00");
              cost (2, 2);
            }
          spillPair (PAIR_BA);
          genMove_o (result->aop, 0, ASMOP_BA, 0, 2, true, true, isPairDead (PAIR_IY, ic), true);
        }
      else
        cheapMove (result->aop, 0, ASMOP_A, 0, true);
    }
  else
    {
      /* 8 < blen <= 16, byte-aligned: low byte -> B, high byte -> A,
         mask/extend the high byte, swap into BA order */
      emit2 ("ld b, !mems", "hl");
      cost (1, 2);
      emit2 ("inc hl");
      cost (1, 2);
      emit2 ("ld a, !mems", "hl");
      cost (1, 2);
      emit2 ("ld ep, #0x00");
      cost (3, 3);
      if (blen < 16)
        unpackMaskA (etype, blen - 8);
      emit2 ("ex a, b");            /* A = low, B = masked high */
      cost (1, 2);
      spillPair (PAIR_BA);
      genMove_o (result->aop, 0, ASMOP_BA, 0, 2, true, true, isPairDead (PAIR_IY, ic), true);
    }

  spillPair (PAIR_HL);

  if (pushed_b)
    {
      emit2 ("pop b");
      cost (1, 3);
      _G.stack.pushed -= 1;
    }
  if (pushed_a)
    {
      emit2 ("pop a");
      cost (1, 3);
      _G.stack.pushed -= 1;
    }
  if (pushed_hl)
    _pop (PAIR_HL);
}

/*-----------------------------------------------------------------*/
/* genFarPackBits - write a bit-field through a 3-byte __far       */
/* pointer.  Non-literal values are staged, shifted and masked at  */
/* EP=0 (parked in B), then the read-modify-write merge runs       */
/* through (hl) under EP — genPackBits' and/or-mask scheme on the  */
/* far byte.  char/int fields only (blen <= 16).                   */
/*-----------------------------------------------------------------*/
static void
genFarPackBits (const iCode *ic, operand *right, operand *result)
{
  sym_link *etype = getSpec (operandType (result)->next);
  unsigned blen = SPEC_BLEN (etype);
  unsigned bstr = SPEC_BSTR (etype);
  unsigned mask;
  unsigned long long litval;
  bool a_dead = isRegDead (A_IDX, ic);
  bool b_dead = isRegDead (B_IDX, ic);
  bool hl_dead = isPairDead (PAIR_HL, ic);
  bool ptr_in_b = result->aop->type == AOP_REG && result->aop->regs[B_IDX] >= 0;
  bool lit = right->aop->type == AOP_LIT;
  bool pushed_hl = false, pushed_a = false, pushed_b = false;

  if (blen > 16)
    {
      UNIMPLEMENTED;
      return;
    }
  wassert (blen <= 8 || bstr == 0);
  /* the non-literal value stages through A and B before the pointer claims
     HLA: a far pointer living in A or B would be corrupted */
  if (!lit && (ptr_in_b || result->aop->type == AOP_REG && result->aop->regs[A_IDX] >= 0))
    {
      UNIMPLEMENTED;
      return;
    }

  /* saves first (B is the value stash / merge partner for every non-literal
     shape), then the pops below run a, hl, b — LIFO */
  if (!lit && !b_dead)
    {
      emit2 ("push b");
      cost (1, 3);
      _G.stack.pushed += 1;
      pushed_b = true;
    }
  if (!hl_dead)
    _push (PAIR_HL), pushed_hl = true;
  if (!a_dead)
    {
      emit2 ("push a");
      cost (1, 3);
      _G.stack.pushed += 1;
      pushed_a = true;
    }

  /* non-literal value staging at EP=0 (the source read is plain near
     codegen here, so any near-readable aop works — registers, frame,
     EXSTK, absolutes):
       - sub-byte: shift + pre-mask, park in B for the merge
       - multi-byte: collect into BA, carry it across the pointer staging
         on the stack (the pop below is SP-paged — near-safe under EP) */
  bool val_on_stack = false;
  if (!lit)
    {
      if (blen < 8)
        {
          mask = ((0xffu << (blen + bstr)) | (0xffu >> (8 - bstr))) & 0xffu;
          cheapMove (ASMOP_A, 0, right->aop, 0, true);
          if (blen + bstr == 8)
            AccLsh (bstr);
          else
            {
              AccRol (bstr);
              emit2 ("and a, !immedbyte", ~mask & 0xffu);
              cost (2, 2);
            }
          emit3 (A_LD, ASMOP_B, ASMOP_A);
        }
      else
        {
          genMove (ASMOP_BA, right->aop, true, true, isPairDead (PAIR_IY, ic));
          _push (PAIR_BA);
          val_on_stack = true;
        }
    }

  /* pointer -> HLA, page -> EP */
  genMove (ASMOP_HLA, result->aop, true, true, isPairDead (PAIR_IY, ic));
  emit2 ("ld ep, a");
  cost (2, 2);
  if (val_on_stack)
    _pop (PAIR_BA);             /* A = value low, B = value high */

  if (blen < 8)
    {
      mask = ((0xffu << (blen + bstr)) | (0xffu >> (8 - bstr))) & 0xffu;
      if (lit)
        {
          litval = ulFromVal (right->aop->aopu.aop_lit);
          litval <<= bstr;
          litval &= (~mask) & 0xff;
          emit2 ("ld a, !mems", "hl");
          cost (1, 2);
          if ((mask | litval) != 0xff)
            {
              emit2 ("and a, !immedbyte", mask);
              cost (2, 2);
            }
          if (litval)
            {
              emit2 ("or a, !immedbyte", (unsigned)litval);
              cost (2, 2);
            }
        }
      else
        {
          emit2 ("ld a, !mems", "hl");
          cost (1, 2);
          emit2 ("and a, !immedbyte", mask);
          cost (2, 2);
          emit3 (A_OR, ASMOP_A, ASMOP_B);
        }
      emit2 ("ld !mems, a", "hl");
      cost (1, 2);
      emit2 ("ld ep, #0x00");
      cost (3, 3);
    }
  else
    {
      /* byte-aligned, one full byte + (blen > 8) a second full or partial
         byte; the non-literal value sits in BA (low in A, high in B) */
      if (lit)
        {
          if (!regalloc_dry_run)
            emit2 ("ld !mems, %s", "hl", aopGet (right->aop, 0, false));
          cost (2, 3);
        }
      else
        {
          emit2 ("ld !mems, a", "hl");
          cost (1, 2);
        }
      if (blen > 8)
        {
          unsigned rlen = blen - 8;
          emit2 ("inc hl");
          cost (1, 2);
          if (blen == 16)
            {
              /* second full byte */
              if (lit)
                {
                  if (!regalloc_dry_run)
                    emit2 ("ld !mems, %s", "hl", aopGet (right->aop, 1, false));
                  cost (2, 3);
                }
              else
                {
                  emit2 ("ld !mems, b", "hl");
                  cost (1, 2);
                }
            }
          else
            {
              /* partial high byte: read-modify-write merge */
              mask = (0xffu << rlen) & 0xffu;
              if (lit)
                {
                  litval = ullFromVal (right->aop->aopu.aop_lit);
                  litval >>= 8;
                  litval &= (~mask) & 0xff;
                  emit2 ("ld a, !mems", "hl");
                  cost (1, 2);
                  if ((mask | litval) != 0xff)
                    {
                      emit2 ("and a, !immedbyte", mask);
                      cost (2, 2);
                    }
                  if (litval)
                    {
                      emit2 ("or a, !immedbyte", (unsigned)litval);
                      cost (2, 2);
                    }
                }
              else
                {
                  emit3 (A_LD, ASMOP_A, ASMOP_B);   /* A = value high */
                  emit2 ("and a, !immedbyte", (~mask) & 0xffu);
                  cost (2, 2);
                  emit3 (A_LD, ASMOP_B, ASMOP_A);
                  emit2 ("ld a, !mems", "hl");
                  cost (1, 2);
                  emit2 ("and a, !immedbyte", mask);
                  cost (2, 2);
                  emit3 (A_OR, ASMOP_A, ASMOP_B);
                }
              emit2 ("ld !mems, a", "hl");
              cost (1, 2);
            }
        }
      emit2 ("ld ep, #0x00");
      cost (3, 3);
    }

  spillPair (PAIR_HL);

  if (pushed_a)
    {
      emit2 ("pop a");
      cost (1, 3);
      _G.stack.pushed -= 1;
    }
  if (pushed_hl)
    _pop (PAIR_HL);
  if (pushed_b)
    {
      emit2 ("pop b");
      cost (1, 3);
      _G.stack.pushed -= 1;
    }
}

/*-----------------------------------------------------------------*/
/* genFarPointerGet - read through a 3-byte __far (EP:offset)      */
/* pointer (abi-decision.md task #9).  Idiom: pointer staged into  */
/* HLA (offset -> HL, page -> A; genMove is overlap-safe), then    */
/* ld ep, a and an (hl) walk; every exit restores the EP=0         */
/* invariant via ld ep, #0 (which does not touch A).  While        */
/* EP != 0 only [hhll]/(hl) accesses are repaged; (ix+d), (iy+d)   */
/* and [sp+dd] keep their own page registers (XP/YP/SP page), so   */
/* frame and indexed result stores stay near-correct inside the    */
/* sequence — absolute (direct) stores do not and toggle EP per    */
/* byte.  A far object never straddles a 64K page boundary (the    */
/* Epson _far model), so neither the constant displacement add nor */
/* the inc hl walk ever carries into the page byte.                */
/*-----------------------------------------------------------------*/
static void
genFarPointerGet (const iCode *ic, operand *left, operand *right, operand *result, int rightval)
{
  int size = result->aop->size;
  bool a_dead = isRegDead (A_IDX, ic);
  bool b_dead = isRegDead (B_IDX, ic);
  bool hl_dead = isPairDead (PAIR_HL, ic);
  bool result_in_a = result->aop->type == AOP_REG && result->aop->regs[A_IDX] >= 0;
  bool result_in_b = result->aop->type == AOP_REG && result->aop->regs[B_IDX] >= 0;
  bool result_in_hl = result->aop->type == AOP_REG && (result->aop->regs[L_IDX] >= 0 || result->aop->regs[H_IDX] >= 0);
  bool pushed_hl = false, pushed_a = false, pushed_b = false;
  int o;

  if (IS_BITVAR (operandType (result)))
    {
      genFarUnpackBits (ic, left, result, rightval);
      return;
    }

  if (!size)
    return;

  /* No staging room for a wider register result: L/H hold the pointer and A
     the page; the allocator is steered away from this shape by the dry-run
     cost. */
  if (result->aop->type == AOP_REG && size > 2)
    {
      UNIMPLEMENTED;
      return;
    }
  /* Only register, frame (ix+d) and literal-addressable (absolute) results
     are supported: anything else ((hl)-routed EXSTK shapes etc.) would fight
     the far pointer for HL. */
  if (result->aop->type != AOP_REG && result->aop->type != AOP_STK
      && result->aop->type != AOP_HL && result->aop->type != AOP_IY)
    {
      UNIMPLEMENTED;
      return;
    }

  if (!hl_dead && !result_in_hl)
    _push (PAIR_HL), pushed_hl = true;
  if (!a_dead && !result_in_a)
    {
      emit2 ("push a");
      cost (1, 3);
      _G.stack.pushed += 1;
      pushed_a = true;
    }

  /* pointer -> HLA: offset -> HL, page -> A (overlap-safe for any source) */
  genMove (ASMOP_HLA, left->aop, true, true, isPairDead (PAIR_IY, ic));
  emit2 ("ld ep, a");
  cost (2, 2);
  if (rightval)
    {
      emit2 ("add hl, !immed%d", rightval);
      cost (3, 3);
    }

  if (size == 2 && aopInReg (result->aop, 0, L_IDX) && aopInReg (result->aop, 1, H_IDX))
    {
      /* result is exactly HL: low through A (free once EP is loaded), high
         straight into H — the final (hl) read uses the original L */
      emit2 ("ld a, !mems", "hl");
      cost (1, 2);
      emit2 ("inc hl");
      cost (1, 2);
      emit2 ("ld h, !mems", "hl");
      cost (1, 2);
      emit2 ("ld ep, #0x00");
      cost (3, 3);
      emit3 (A_LD, ASMOP_L, ASMOP_A);
    }
  else if (result->aop->type == AOP_REG)
    {
      /* collect into A (+ B), then place — result regs may include L/H,
         which genMove handles after the walk is done with the pointer */
      if (size == 2 && !b_dead && !result_in_b)
        {
          emit2 ("push b");
          cost (1, 3);
          _G.stack.pushed += 1;
          pushed_b = true;
        }
      emit2 ("ld a, !mems", "hl");
      cost (1, 2);
      if (size == 2)
        {
          emit2 ("inc hl");
          cost (1, 2);
          emit2 ("ld b, !mems", "hl");
          cost (1, 2);
        }
      emit2 ("ld ep, #0x00");
      cost (3, 3);
      genMove_o (result->aop, 0, ASMOP_BA, 0, size, true, true, isPairDead (PAIR_IY, ic), true);
      if (pushed_b)
        {
          emit2 ("pop b");
          cost (1, 3);
          _G.stack.pushed -= 1;
          pushed_b = false;
        }
    }
  else if (result->aop->type == AOP_HL || result->aop->type == AOP_IY)
    {
      /* absolute result: an EP-paged store (and aopPut for these types would
         either claim HL or emit an iy-literal store the peephole folds into
         an absolute) — keep the page in B, toggle EP around each store, and
         emit the store as a direct absolute (no HL, nothing to fold) */
      if (!b_dead)
        {
          emit2 ("push b");
          cost (1, 3);
          _G.stack.pushed += 1;
          pushed_b = true;
        }
      emit3 (A_LD, ASMOP_B, ASMOP_A);
      for (o = 0; o < size; o++)
        {
          if (o)
            {
              emit3 (A_LD, ASMOP_A, ASMOP_B);
              emit2 ("ld ep, a");
              cost (2, 2);
            }
          emit2 ("ld a, !mems", "hl");
          cost (1, 2);
          if (o != size - 1)
            {
              emit2 ("inc hl");
              cost (1, 2);
            }
          emit2 ("ld ep, #0x00");
          cost (3, 3);
          if (!regalloc_dry_run)
            emit2 ("ld !mems, a", aopGetLitWordLong (result->aop, o, FALSE));
          cost (3, 4);
        }
      if (pushed_b)
        {
          emit2 ("pop b");
          cost (1, 3);
          _G.stack.pushed -= 1;
          pushed_b = false;
        }
    }
  else
    {
      /* frame result, (ix+d): XP-paged, near-safe while EP is set, and the
         peephole never rewrites it as an absolute */
      for (o = 0; o < size; o++)
        {
          emit2 ("ld a, !mems", "hl");
          cost (1, 2);
          if (o != size - 1)
            {
              emit2 ("inc hl");
              cost (1, 2);
            }
          if (!regalloc_dry_run)
            aopPut (result->aop, "a", o);
          ld_cost (result->aop, o, ASMOP_A, 0, true);
        }
      emit2 ("ld ep, #0x00");
      cost (3, 3);
    }

  spillPair (PAIR_HL);

  if (pushed_a)
    {
      emit2 ("pop a");
      cost (1, 3);
      _G.stack.pushed -= 1;
    }
  if (pushed_hl)
    _pop (PAIR_HL);
}

/*-----------------------------------------------------------------*/
/* genFarPointerSet - write through a 3-byte __far (EP:offset)     */
/* pointer.  Same HLA + EP idiom and EP=0 invariant as             */
/* genFarPointerGet; the value is staged before the pointer claims */
/* HL and A (saves first, value last, so the mid-store pop ba      */
/* matches LIFO).                                                  */
/*-----------------------------------------------------------------*/
static void
genFarPointerSet (iCode *ic, operand *right, operand *result)
{
  int size = right->aop->size;
  bool a_dead = isRegDead (A_IDX, ic);
  bool b_dead = isRegDead (B_IDX, ic);
  bool hl_dead = isPairDead (PAIR_HL, ic);
  bool val_in_a = right->aop->type == AOP_REG && right->aop->regs[A_IDX] >= 0;
  bool val_in_b = right->aop->type == AOP_REG && right->aop->regs[B_IDX] >= 0;
  bool val_in_hl = right->aop->type == AOP_REG && (right->aop->regs[L_IDX] >= 0 || right->aop->regs[H_IDX] >= 0);
  /* literal-addressable (absolute) value sources: EP-paged reads, handled
     with the EP toggle below */
  bool val_abs = right->aop->type == AOP_HL || right->aop->type == AOP_IY;
  bool pushed_hl = false, pushed_a = false, pushed_b = false;
  bool val_on_stack = false, val_staged_b = false;
  int o;

  if (IS_BITVAR (operandType (result)->next))
    {
      genFarPackBits (ic, right, result);
      return;
    }

  if (!size)
    return;

  /* Only register, literal, frame (ix+d) and literal-addressable (absolute)
     values are supported: anything else ((hl)-routed EXSTK shapes etc.)
     would fight the far pointer for HL. */
  if (right->aop->type != AOP_REG && right->aop->type != AOP_LIT && right->aop->type != AOP_IMMD
      && right->aop->type != AOP_STK && !val_abs)
    {
      UNIMPLEMENTED;
      return;
    }
  /* register values wider than BA can't be staged (L/H/A go to the pointer) */
  if (right->aop->type == AOP_REG && size > 2 && (val_in_a || val_in_hl))
    {
      UNIMPLEMENTED;
      return;
    }
  /* a register-allocated far pointer using A/B would be corrupted by the
     value staging below (which runs before the pointer claims HLA) */
  if (result->aop->type == AOP_REG && (result->aop->regs[A_IDX] >= 0 || result->aop->regs[B_IDX] >= 0)
      && (right->aop->type == AOP_REG && size == 2 || size == 1 && (val_in_a || val_in_hl) || val_abs))
    {
      UNIMPLEMENTED;
      return;
    }

  /* saves first ... */
  if (!b_dead && (size == 1 && (val_in_a || val_in_hl)   /* B = byte stage */
                  || size == 2 && right->aop->type == AOP_REG /* BA stage */
                  || val_abs))                           /* B = page keep */
    {
      emit2 ("push b");
      cost (1, 3);
      _G.stack.pushed += 1;
      pushed_b = true;
    }
  if (!hl_dead)
    _push (PAIR_HL), pushed_hl = true;
  if (!a_dead)
    {
      emit2 ("push a");
      cost (1, 3);
      _G.stack.pushed += 1;
      pushed_a = true;
    }

  /* ... then stage a register value clear of A/L/H (the pointer's home) */
  if (right->aop->type == AOP_REG && size == 1 && (val_in_a || val_in_hl))
    {
      cheapMove (ASMOP_B, 0, right->aop, 0, false);
      val_staged_b = true;
    }
  else if (right->aop->type == AOP_REG && size == 2)
    {
      genMove (ASMOP_BA, right->aop, true, false, false);
      _push (PAIR_BA);
      val_on_stack = true;
    }

  /* pointer -> HLA, page -> EP */
  genMove (ASMOP_HLA, result->aop, true, true, isPairDead (PAIR_IY, ic));
  emit2 ("ld ep, a");
  cost (2, 2);

  if (val_on_stack)
    {
      _pop (PAIR_BA);
      emit2 ("ld !mems, a", "hl");
      cost (1, 2);
      emit2 ("inc hl");
      cost (1, 2);
      emit2 ("ld !mems, b", "hl");
      cost (1, 2);
      emit2 ("ld ep, #0x00");
      cost (3, 3);
    }
  else if (val_staged_b || right->aop->type == AOP_REG && size == 1 && val_in_b)
    {
      emit2 ("ld !mems, b", "hl");
      cost (1, 2);
      emit2 ("ld ep, #0x00");
      cost (3, 3);
    }
  else if (right->aop->type == AOP_LIT || right->aop->type == AOP_IMMD)
    {
      for (o = 0; o < size; o++)
        {
          if (!regalloc_dry_run)
            emit2 ("ld !mems, %s", "hl", aopGet (right->aop, o, false));
          cost (2, 3);
          if (o != size - 1)
            {
              emit2 ("inc hl");
              cost (1, 2);
            }
        }
      emit2 ("ld ep, #0x00");
      cost (3, 3);
    }
  else if (val_abs)
    {
      /* absolute (EP-paged!) value reads: keep the page in B, read each byte
         at EP=0 as a direct absolute (no HL, nothing for the peephole to
         fold), swap the page back in for the far store */
      emit3 (A_LD, ASMOP_B, ASMOP_A);
      emit2 ("ld ep, #0x00");
      cost (3, 3);
      for (o = 0; o < size; o++)
        {
          if (!regalloc_dry_run)
            emit2 ("ld a, !mems", aopGetLitWordLong (right->aop, o, FALSE));
          cost (3, 4);
          emit2 ("ex a, b");
          cost (1, 2);
          emit2 ("ld ep, a");
          cost (2, 2);
          emit2 ("ld !mems, b", "hl");
          cost (1, 2);
          if (o != size - 1)
            {
              emit2 ("inc hl");
              cost (1, 2);
            }
          emit2 ("ld ep, #0x00");
          cost (3, 3);
          if (o != size - 1)
            {
              emit3 (A_LD, ASMOP_B, ASMOP_A);
            }
        }
    }
  else
    {
      /* frame / indexed value (near-safe while EP is set): walk through A */
      for (o = 0; o < size; o++)
        {
          cheapMove (ASMOP_A, 0, right->aop, o, true);
          emit2 ("ld !mems, a", "hl");
          cost (1, 2);
          if (o != size - 1)
            {
              emit2 ("inc hl");
              cost (1, 2);
            }
        }
      emit2 ("ld ep, #0x00");
      cost (3, 3);
    }

  spillPair (PAIR_HL);

  if (pushed_a)
    {
      emit2 ("pop a");
      cost (1, 3);
      _G.stack.pushed -= 1;
    }
  if (pushed_hl)
    _pop (PAIR_HL);
  if (pushed_b)
    {
      emit2 ("pop b");
      cost (1, 3);
      _G.stack.pushed -= 1;
    }
}

/*-----------------------------------------------------------------*/
/* genPointerGet - generate code for pointer get                   */
/*-----------------------------------------------------------------*/
static void
genPointerGet (const iCode *ic)
{
  operand *left, *right, *result;
  int size, offset, rightval;
  int pair = PAIR_HL;
  bool pushed_pair = FALSE;
  bool pushed_a = FALSE;
  bool surviving_a = !isRegDead (A_IDX, ic);
  bool rightval_in_range;

  left = IC_LEFT (ic);
  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);
  bool bit_field = IS_BITVAR (operandType (result)); // Should be IS_BITVAR (operandType (left)->next), but conflicts with optimizations that reuses pointers (when reading from a union of a struct containing bit-fields and other types).

  aopOp (left, ic, false, false);
  aopOp (result, ic, true, false);
  size = result->aop->size;

  /* Historically GET_VALUE_AT_ADDRESS didn't have a right operand */
  wassertl (right, "GET_VALUE_AT_ADDRESS without right operand");
  wassertl (IS_OP_LITERAL (IC_RIGHT (ic)), "GET_VALUE_AT_ADDRESS with non-literal right operand");
  rightval = (int)operandLitValue (right);
  rightval_in_range = (rightval >= -128 && rightval + size - 1 < 127);

  /* 3-byte __far pointer read: EP-paged (hl) access — must run before the
     literal-address fast paths below, which would truncate the page byte */
  if (IS_FARPTR (operandType (left)) && left->aop->size == 3)
    {
      genFarPointerGet (ic, left, right, result, rightval);
      goto release;
    }


  if ((IY_RESERVED) && requiresHL (result->aop) && size > 1 && result->aop->type != AOP_REG)
    UNIMPLEMENTED; /* no S1C88 spare pointer under --reserve-iy */

  
  if ((left->aop->type == AOP_IMMD || left->aop->type == AOP_LIT && !rightval) && size == 1 && aopInReg (result->aop, 0, A_IDX) && !bit_field)
    {
      emit2 ("ld a, !mems", aopGetLitWordLong (left->aop, rightval, true));
      cost2 (3, 13);
      goto release;
    }
  else if ((left->aop->type == AOP_IMMD || left->aop->type == AOP_LIT && !rightval) && isPair (result->aop) && !bit_field)
    {
      PAIR_ID pair = getPairId (result->aop);
      emit2 ("ld %s, !mems", _pairs[pair].name, aopGetLitWordLong (left->aop, rightval, TRUE));
      if (pair == PAIR_HL)
        cost2 (3, 16);
      else
        cost2 (4, 20);
      goto release;
    }
  else if ((left->aop->type == AOP_IMMD) && getPartPairId (result->aop, 0) != PAIR_INVALID && getPartPairId (result->aop, 2) != PAIR_INVALID)
    {
      PAIR_ID pair;
      pair = getPartPairId (result->aop, 0);
      emit2 ("ld %s, !mems", _pairs[pair].name, aopGetLitWordLong (left->aop, rightval, TRUE));
      if (pair == PAIR_HL)
        cost2 (3, 16);
      else
        cost2 (4, 20);
      pair = getPartPairId (result->aop, 2);
      emit2 ("ld %s, !mems", _pairs[pair].name, aopGetLitWordLong (left->aop, rightval + 2, TRUE));
      if (pair == PAIR_HL)
        cost2 (3, 16);
      else
        cost2 (4, 20);
      goto release;
    }
  else if (left->aop->type == AOP_STL && !bit_field && size <= 4)
    {
      struct asmop saop;
      init_stackop (&saop, size, left->aop->aopu.aop_stk + rightval);
      genMove (result->aop, &saop, !surviving_a, isPairDead(PAIR_HL, ic), isPairDead(PAIR_IY, ic));
      goto release;
    }

  

  if (isPair (left->aop) && size == 1 && !bit_field && !rightval)
    {
      /* Just do it */
      if ((getPairId (left->aop) == PAIR_HL || getPairId (left->aop) == PAIR_IY) && result->aop->type == AOP_REG)
        {
          if (!regalloc_dry_run)        // Todo: More exact cost.
            {
              struct dbuf_s dbuf;

              dbuf_init (&dbuf, 128);
              dbuf_tprintf (&dbuf, "!mems", getPairName (left->aop));
              aopPut (result->aop, dbuf_c_str (&dbuf), 0);
              dbuf_destroy (&dbuf);
            }
          ld_cost (result->aop, 0, aopInReg(result->aop, 0, A_IDX) ? ASMOP_L : ASMOP_A, 0, true);
        }
      else
        {
          if (surviving_a && !pushed_a)
            _push (PAIR_AF), pushed_a = true;
          emit2 ("ld a, !mems", getPairName (left->aop));
          if (getPairId (left->aop) == PAIR_IY)
            cost2 (3, 19);
          else
            cost2 (1, 7);
          genMove (result->aop, ASMOP_A, true, isPairDead(PAIR_HL, ic), true);
        }

      goto release;
    }

  if (getPairId (left->aop) == PAIR_IY && !bit_field && rightval_in_range)
    {
      offset = 0;

      

      if (!size)
        goto release;

      /* Just do it */
      if (surviving_a && !pushed_a)
        _push (PAIR_AF), pushed_a = TRUE;

      while (size--)
        {
          if (!regalloc_dry_run)
            {
              struct dbuf_s dbuf;

              dbuf_init (&dbuf, 128);
              dbuf_tprintf (&dbuf, "!*iyx", rightval + offset);
              aopPut (result->aop, dbuf_c_str (&dbuf), offset);
              dbuf_destroy (&dbuf);
            }
          cost2 (3, 19); // Assume ld r, d(iy)
          offset++;
        }

      goto release;
    }

  // Large memory-to-memory transfer to a stack destination.  The S1C88 has
  // no ldir/DE, so copy with a native forward byte loop: HL = source pointer,
  // IY = dest (SP-relative address), B = count.
  if (!bit_field && (result->aop->type == AOP_STK || result->aop->type == AOP_EXSTK) && size > 2 && size <= 255)
    {
      int fp_offset, sp_offset;
      bool s_hl = !isPairDead (PAIR_HL, ic);
      bool s_iy = !isPairDead (PAIR_IY, ic);
      bool s_ba = !isRegDead (B_IDX, ic);
      bool s_af = !s_ba && !isRegDead (A_IDX, ic);
      symbol *tlbl;

      if (s_hl)
        _push (PAIR_HL);
      if (s_iy)
        _push (PAIR_IY);
      if (s_ba)
        _push (PAIR_BA);
      else if (s_af)
        _push (PAIR_AF);

      /* source pointer -> HL (IMMD folds rightval into the literal) */
      if (left->aop->type == AOP_IMMD)
        {
          emit2 ("ld hl, %s", aopGetLitWordLong (left->aop, rightval, TRUE));
          cost2 (3, 10);
        }
      else
        {
          genMove (ASMOP_HL, left->aop, true, true, true);
          if (rightval)
            offsetPair (PAIR_HL, rightval);
        }

      /* dest (stack address) -> IY */
      fp_offset = result->aop->aopu.aop_stk + (result->aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);
      sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;
      setupPairFromSP (PAIR_IY, sp_offset);

      emit2 ("ld b, !immedbyte", (unsigned) size);
      cost2 (2, 7);
      tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
      if (!regalloc_dry_run)
        emitLabel (tlbl);
      emit2 ("ld a, !*hl");
      cost2 (1, 7);
      emit2 ("ld !*iyx, a", 0);
      cost2 (1, 7);
      emit3w (A_INC, ASMOP_HL, 0);
      emit3w (A_INC, ASMOP_IY, 0);
      if (!regalloc_dry_run)
        emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
      regalloc_dry_run_cost += 2;

      spillPair (PAIR_HL);
      spillPair (PAIR_IY);

      if (s_ba)
        _pop (PAIR_BA);
      else if (s_af)
        _pop (PAIR_AF);
      if (s_iy)
        _pop (PAIR_IY);
      if (s_hl)
        _pop (PAIR_HL);

      goto release;
    }

  if (!surviving_a && (0 || 0) && isPairDead (getPairId (left->aop), ic) && abs(rightval) <= 2 && !bit_field && size < 2) // Use inc ss (size < 2 condition to avoid overwriting pair with result)
    pair = getPairId (left->aop);

  /* For now we always load into temp pair */
  /* if this is rematerializable */
  if ((0 || 0) && result->aop->type == AOP_STK && !rightval
      || getPairId (left->aop) == PAIR_IY && SPEC_BLEN (getSpec (operandType (result))) < 8 && rightval_in_range)
    pair = getPairId (left->aop);
  else
    {
      if (!isPairDead (pair, ic) && size > 1 && (getPairId (left->aop) != pair || rightval || bit_field || size > 2)) // For simple cases, restoring via dec is cheaper than push / pop.
        _push (pair), pushed_pair = TRUE;
      if (left->aop->type == AOP_IMMD)
        {
          emit2 ("ld %s, %s", _pairs[pair].name, aopGetLitWordLong (left->aop, rightval, TRUE));
          regalloc_dry_run_cost += 3;
          spillPair (pair);
          rightval = 0;
        }
      else if (pair == PAIR_HL && rightval > 2 && (0 || 0)) // Cheaper than moving to hl followed by offset adjustment.
        {
          emit2 ("ld hl, !immed%d", rightval);
          cost2 (3, 10);
          emit2 ("add hl, %s", _pairs[getPairId (left->aop)].name);
          cost2 (1, 11);
          spillPair (pair);
          rightval = 0;
        }
      else if (pair == PAIR_HL && left->aop->type == AOP_STL)
        {
          emit2 ("ld hl, !immed%d", spOffset (left->aop->aopu.aop_stk) + rightval);
          cost2 (3, 10);
          emit2 ("add hl, sp");
          cost2 (1, 11);
          spillPair (pair);
          rightval = 0;
        }
      else
        fetchPairLong (pair, left->aop, ic, 0);
    }

  /* if bit then unpack */
  if (bit_field)
    {
      offsetPair (pair, rightval);
      genUnpackBits (result, pair, ic);
      if (rightval)
        spillPair (pair);

      goto release;
    }

 if (pair == PAIR_HL && (getPairId (result->aop) == PAIR_HL || size == 2 && (aopInReg (result->aop, 0, L_IDX) || aopInReg (result->aop, 0, H_IDX))))
    {
      wassertl (size == 2, "HL must be of size 2");
      if (aopInReg (result->aop, 1, A_IDX))
        {
          offsetPair (pair, rightval + 1);
          emit2 ("!ldahld");
          if (!regalloc_dry_run)
            aopPut (result->aop, "!*hl", 0);
          regalloc_dry_run_cost += 3;
        }
      else
        {
          if (surviving_a && !pushed_a)
            _push (PAIR_AF), pushed_a = TRUE;
          offsetPair (pair, rightval);
          emit2 ("!ldahli");
          if (!regalloc_dry_run)
            aopPut (result->aop, "!*hl", 1);
          regalloc_dry_run_cost += 3;
          cheapMove (result->aop, 0, ASMOP_A, 0, true);
        }
      spillPair (PAIR_HL);
      goto release;
    }

  offsetPair (pair, rightval);

  if (pair == PAIR_HL
           || ((0 || 0)
               && result->aop->type == AOP_STK))
    {
      size = result->aop->size;
      offset = 0;
      int last_offset = 0;

      /* might use ld a,(hl) followed by ld d (iy),a */
      if ((result->aop->type == AOP_EXSTK || result->aop->type == AOP_STK) && surviving_a && !pushed_a)
        _push (PAIR_AF), pushed_a = TRUE;

      if (size >= 2 && pair == PAIR_HL && result->aop->type == AOP_REG)
        {
          int i, l = -10, h = -10, r;
          for (i = 0; i < size; i++)
            {
              if (result->aop->aopu.aop_reg[i]->rIdx == L_IDX)
                l = i;
              else if (result->aop->aopu.aop_reg[i]->rIdx == H_IDX)
                h = i;
            }

          if (l == -10 && h >= 0 && h < size - 1 || h == -10 && l >= 0 && l < size - 1) // One byte of result somewehere in hl. Just assign it last.
            {
              r = (l == -10 ? h : l);

              while (offset < size)
                {
                  if (offset != r)
                    _moveFrom_tpair_ (result->aop, offset, pair);

                  if (offset < size)
                    {
                      offset++;
                      emit2 ("inc %s", _pairs[pair].name);
                      cost2 (1, 6);
                      _G.pairs[pair].offset++;
                    }
                }

              for (size = offset; size != r; size--)
                {
                  emit2 ("dec %s", _pairs[pair].name);
                  cost2 (1, 6);
                }

              _moveFrom_tpair_ (result->aop, r, pair);

              // No fixup since result uses HL.
              spillPair (pair);
              goto release;
            }
          else if (l >= 0 && h >= 0)    // Two bytes of result somewehere in hl. Assign them last and use a for caching.
            {
              while (offset < size)
                {
                  last_offset = offset;

                  

                  if (offset != l && offset != h)
                    _moveFrom_tpair_ (result->aop, offset, pair);
                  offset++;

                  if (offset < size)
                    {
                      emit2 ("inc %s", _pairs[pair].name);
                      cost2 (1, 6);
                      _G.pairs[pair].offset++;
                    }
                }

              r = (l > h ? l : h);
              for (size = last_offset; size != r; size--)
                {
                  emit2 ("dec %s", _pairs[pair].name);
                  cost2 (1, 6);
                }
              if ((surviving_a || result->aop->regs[A_IDX] >= 0) && !pushed_a)
                _push (PAIR_AF), pushed_a = true;
              _moveFrom_tpair_ (ASMOP_A, 0, pair);

              r = (l < h ? l : h);
              for (; size != r; size--)
                {
                  emit2 ("dec %s", _pairs[pair].name);
                  cost2 (1, 6);
                }
              _moveFrom_tpair_ (result->aop, r, pair);

              r = (l > h ? l : h);
              cheapMove (result->aop, r, ASMOP_A, 0, true);

              // No fixup since result uses HL.
              spillPair (pair);
              goto release;
            }
        }

      while (offset < size)
        {
          if (result->aop->regs[A_IDX] >= 0 && result->aop->regs[A_IDX] < offset)
            surviving_a = true;

          last_offset = offset;

          

          // _moveFrom_tpair_ below might use a.
          if (result->aop->type != AOP_REG && surviving_a && !pushed_a)
            _push (PAIR_AF), pushed_a = true;
          _moveFrom_tpair_ (result->aop, offset++, pair);

          if (offset < size)
            {
              emit2 ("inc %s", _pairs[pair].name);
              cost2 (1, 6);
              _G.pairs[pair].offset++;
            }
        }
      /* Fixup HL back down */
      if (getPairId (left->aop) == pair && !isPairDead (pair, ic) && !pushed_pair)
        while (last_offset --> 0)
          {
            emit2 ("dec %s", _pairs[pair].name);
            cost2 (1, 6);
            _G.pairs[pair].offset--;
          }
       else if (rightval || result->aop->size)
         spillPair (pair);
    }
  else
    {
      size = result->aop->size;
      offset = 0;

      if (result->aop->regs[A_IDX] >= 0 && result->aop->regs[A_IDX] < offset)
        surviving_a = true;

      for (offset = 0; offset < size;)
        {
          if (surviving_a && !pushed_a)
            _push (PAIR_AF), pushed_a = true;

          /* PENDING: make this better */
          if ((pair == PAIR_HL) && result->aop->type == AOP_REG)
            {
              if (!regalloc_dry_run)
                aopPut (result->aop, "!*hl", offset++);
              ld_cost (result->aop, 0, aopInReg (result->aop, 0, A_IDX) ? ASMOP_L : ASMOP_A, 0, true);
            }
          else
            {
              emit2 ("ld a, !mems", _pairs[pair].name);
              cost2 (1, 7);
              cheapMove (result->aop, offset++, ASMOP_A, 0, true);
            }
          if (offset < size)
            {
              emit2 ("inc %s", _pairs[pair].name);
              cost2 (1, 6);
              _G.pairs[pair].offset++;
            }
        }
      if (!isPairDead (pair, ic))
        while (offset --> 1)
          {
            emit2 ("dec %s", _pairs[pair].name);
            cost2 (1, 6);
            _G.pairs[pair].offset--;
          }
      else if (rightval || result->aop->size)
         spillPair (pair);
    }

release:
  if (pushed_a)
    _pop (PAIR_AF);
  if (pushed_pair)
    _pop (pair);

  freeAsmop (left, NULL);
  freeAsmop (result, NULL);
}

static bool
isRegOrLit (asmop * aop)
{
  if (aop->type == AOP_REG || aop->type == AOP_LIT || aop->type == AOP_IMMD)
    return true;
  return false;
}


/*-----------------------------------------------------------------*/
/* genPackBits - generates code for packed bit storage             */
/*-----------------------------------------------------------------*/
static void
genPackBits (sym_link * etype, operand * right, int pair, const iCode * ic)
{
  int offset = 0;               /* source byte offset */
  int pair_offset = 0;
  int rlen = 0;                 /* remaining bit-field length */
  unsigned blen;                /* bit-field length */
  unsigned bstr;                /* bit-field starting bit within byte */
  unsigned long long litval;    /* source literal value (if AOP_LIT) */
  unsigned mask;                /* bitmask within current byte */
  unsigned int pairincrement = 0;

  emitDebug ("; genPackBits", "");

  blen = SPEC_BLEN (etype);
  bstr = SPEC_BSTR (etype);

  /* If the bit-field length is less than a byte */
  if (blen < 8)
    {
      mask = ((0xffu << (blen + bstr)) | (0xffu >> (8 - bstr))) & 0xffu;

      /* S1C88 has no set/res; a 1-bit literal store goes through the general
         and/or-with-mask path below (set bit = or a,#mask; clear = and a,#~mask). */
      if (right->aop->type == AOP_LIT)
        {
          /* Case with a bit-field length <8 and literal source */
          litval = (int) ulFromVal (right->aop->aopu.aop_lit);
          litval <<= bstr;
          litval &= (~mask) & 0xff;
          emit2 ("ld a, !mems", _pairs[pair].name);
          regalloc_dry_run_cost += (pair == PAIR_IX || pair == PAIR_IY) ? 3 : 1;
          if ((mask | litval) != 0xff)
            {
              emit2 ("and a, !immedbyte", mask);
              cost2 (2, 7);
            }
          if (litval)
            {
              emit2 ("or a, !immedbyte", (unsigned)litval);
              cost2 (2, 7);
            }
          emit2 ("ld !mems, a", _pairs[pair].name);
          regalloc_dry_run_cost += (pair == PAIR_IX || pair == PAIR_IY) ? 3 : 1;
          return;
        }
      /* (the S1C88 has no rld/rrd; nibble fields use the general and/or-mask merge below.) */
      else
        {
          /* Case with a bit-field length <8 and arbitrary source */
          cheapMove (ASMOP_A, 0, right->aop, 0, true);
          /* shift and mask source value */
          if (blen + bstr == 8)
            AccLsh (bstr);
          else
            {
              AccRol (bstr);
              emit2 ("and a, !immedbyte", ~mask & 0xffu);
              cost2 (2, 7);
            }

          /* S1C88: stash the shifted value in B for the read-modify-write
             merge — `or a,X` can only source A or B (C/D/E don't exist).
             B is the lone spare byte GPR; save it on the stack when live. */
          {
            bool save_b = !isRegDead (B_IDX, ic);
            if (save_b)
              {
                emit2 ("push b");
                cost2 (1, 11);
                _G.stack.pushed += 1;
              }
            emit3 (A_LD, ASMOP_B, ASMOP_A);
            emit2 ("ld a, !mems", _pairs[pair].name);
            regalloc_dry_run_cost += (pair == PAIR_IX || pair == PAIR_IY) ? 3 : 1;
            emit2 ("and a, !immedbyte", mask);
            cost2 (2, 7);
            emit3 (A_OR, ASMOP_A, ASMOP_B);
            emit2 ("ld !mems, a", _pairs[pair].name);
            regalloc_dry_run_cost += (pair == PAIR_IX || pair == PAIR_IY) ? 3 : 1;
            if (save_b)
              {
                emit2 ("pop b");
                cost2 (1, 10);
                _G.stack.pushed -= 1;
              }
          }
          return;
        }
    }

  /* Bit length is greater than 7 bits. In this case, copy  */
  /* all except the partial byte at the end                 */
  for (rlen = blen; rlen >= 8; rlen -= 8)
    {
      cheapMove (ASMOP_A, 0, right->aop, offset++, true);
      if (pair == PAIR_IX || pair == PAIR_IY)
        {
          emit2 ("ld %d !mems, a", pair_offset, _pairs[pair].name);
          regalloc_dry_run_cost += 3;
        }
      else
        {
          emit2 ("ld !mems, a", _pairs[pair].name);
          cost2 (1, 7);
        }
      if (rlen > 8 && pair != PAIR_IX && pair != PAIR_IY)
        {
          emit2 ("inc %s", _pairs[pair].name);
          cost2 (1, 6);
          pairincrement++;
          _G.pairs[pair].offset++;
        }
      else
        pair_offset++;
    }

  /* If there was a partial byte at the end */
  if (rlen)
    {
      mask = ((unsigned char)-1 << rlen) & 0xffu;

      if (right->aop->type == AOP_LIT)
        {
          /* Case with partial byte and literal source */
          litval = ullFromVal (right->aop->aopu.aop_lit);
          litval >>= (blen - rlen);
          litval &= (~mask) & 0xff;

          if (pair == PAIR_IX || pair == PAIR_IY)
            {
              emit2 ("ld a, %d !mems", pair_offset, _pairs[pair].name);
              cost2 (3, 19);
            }
          else
            {
              emit2 ("ld a, !mems", _pairs[pair].name);
              cost2 (1, 7);
            }

          if ((mask | litval) != 0xff)
            {
              emit2 ("and a, !immedbyte", mask);
              cost2 (2, 7);
            }
          if (litval)
            {
              emit2 ("or a, !immedbyte", (unsigned)litval);
              cost2 (2, 7);
            }
        }
      else
        {
          /* Case with partial byte and arbitrary source */
          cheapMove (ASMOP_A, 0, right->aop, offset++, true);
          emit2 ("and a, !immedbyte", (~mask) & 0xffu);
          cost2 (2, 7);

          /* S1C88: stash the shifted value in B for the merge (see the
             blen<8 case above); the trailing store writes A back. */
          {
            bool save_b = !isRegDead (B_IDX, ic);
            if (save_b)
              {
                emit2 ("push b");
                cost2 (1, 11);
                _G.stack.pushed += 1;
              }
            emit3 (A_LD, ASMOP_B, ASMOP_A);

            if (pair == PAIR_IX || pair == PAIR_IY)
              {
                emit2 ("ld a, %d !mems", pair_offset, _pairs[pair].name);
                regalloc_dry_run_cost += 3;
              }
            else
              {
                emit2 ("ld a, !mems", _pairs[pair].name);
                cost2 (1, 7);
              }

            emit2 ("and a, !immedbyte", mask);
            cost2 (2, 7);
            emit3 (A_OR, ASMOP_A, ASMOP_B);
            if (save_b)
              {
                emit2 ("pop b");
                cost2 (1, 10);
                _G.stack.pushed -= 1;
              }
          }
        }
      if (pair == PAIR_IX || pair == PAIR_IY)
        {
          emit2 ("ld %d !mems, a", pair_offset, _pairs[pair].name);
          regalloc_dry_run_cost += 3;
        }
      else
        {
          emit2 ("ld !mems, a", _pairs[pair].name);
          regalloc_dry_run_cost += 1;
        }
    }
  if (!isPairDead(pair, ic))
    while (pairincrement)
      {
        emit2 ("dec %s", _pairs[pair].name);
        cost2 (1, 6);
        pairincrement--;
        _G.pairs[pair].offset--;
      }
}

/*-----------------------------------------------------------------*/
/* genPointerSet - stores the value into a pointer location        */
/*-----------------------------------------------------------------*/
static void
genPointerSet (iCode *ic)
{
  int size, offset = 0;
  int last_offset = 0;
  operand *right, *result;
  PAIR_ID pairId = PAIR_HL;
  bool pushed_a = false;
  bool pushed_pair = false;
  bool surviving_a = !isRegDead (A_IDX, ic);

  right = IC_RIGHT (ic);
  result = IC_RESULT (ic);

  wassert (operandType (result)->next);
  bool bit_field = IS_BITVAR (operandType (result)->next);

  aopOp (result, ic, FALSE, FALSE);
  aopOp (right, ic, FALSE, FALSE);

  size = right->aop->size;

  /* 3-byte __far pointer write: EP-paged (hl) access — must run before the
     near fast paths below */
  if (IS_FARPTR (operandType (result)) && result->aop->size == 3)
    {
      genFarPointerSet (ic, right, result);
      goto release;
    }

  if (IY_RESERVED && !(isRegOrLit (right->aop) || right->aop->type == AOP_STK))
    UNIMPLEMENTED;  /* reserve-regs-iy + a value that needs HL to read: no third pointer
                       exists — the s19 documented limit */
  if (isPair (result->aop) && isPairDead (getPairId (result->aop), ic) && !(size > 1 && sameRegs (result->aop, right->aop)))
    pairId = getPairId (result->aop);

  

  /* Handle the exceptions first */
  if (isPair (result->aop) && size == 1 && !bit_field)
    {
      /* Just do it */
      const char *pair = getPairName (result->aop);
      if (canAssignToPtr3 (right->aop) && isPtr (pair))        // Todo: correct cost for pair iy.
        {
          if (!regalloc_dry_run)
            emit2 ("ld !mems, %s", pair, aopGet (right->aop, 0, FALSE));
          if (getPairId (result->aop) == PAIR_HL)
            cost2 (1, 7); // Assume ld (hl), r
          else if (aopInReg (right->aop, 0, A_IDX))
            cost2 (1, 7); // Assume ld (rr), a
          else
            {
              ld_cost (ASMOP_A, 0, right->aop, 0, true);
              cost2 (1, 7); // Assume ld (rr), a
            }
        }
      else
        {
          if (surviving_a && !pushed_a && !aopInReg (right->aop, 0, A_IDX))
            _push (PAIR_AF), pushed_a = TRUE;
          genMove_o (ASMOP_A, 0, right->aop, 0, 1, true,
            pairId != PAIR_HL && isPairDead (PAIR_HL, ic) && right->aop->regs[L_IDX] < offset && right->aop->regs[H_IDX] < offset, pairId != PAIR_IY && isPairDead (PAIR_IY, ic) && right->aop->regs[IYL_IDX] < offset && right->aop->regs[IYH_IDX] < offset, true);
          emit2 ("ld !mems, a", pair);
          cost2 (1, 7); // Assume ld (rr), a
        }
      goto release;
    }

  /* Rematerialized stack location */
  if (result->aop->type == AOP_STL && !bit_field && size <= 4)
    {
      struct asmop saop;
      init_stackop (&saop, size, result->aop->aopu.aop_stk);
      genMove (&saop, right->aop, isRegDead (A_IDX, ic), isPairDead(PAIR_HL, ic), isPairDead(PAIR_IY, ic));
      goto release;
    }

  // Large memory-to-memory transfer from a stack source.  The S1C88 has no
  // ldir/DE, so copy with a native forward byte loop: IY = dest pointer,
  // HL = source (SP-relative), B = count.  size here is a scalar store (<= 8),
  // well within the byte counter's range.
  if ((right->aop->type == AOP_STK || right->aop->type == AOP_EXSTK) && size > 2 && size <= 255)
    {
      int fp_offset, sp_offset;
      bool s_iy = !isPairDead (PAIR_IY, ic);
      bool s_hl = !isPairDead (PAIR_HL, ic);
      bool s_ba = !isRegDead (B_IDX, ic);
      bool s_af = !s_ba && !isRegDead (A_IDX, ic);
      symbol *tlbl;

      if (s_iy)
        _push (PAIR_IY);
      if (s_hl)
        _push (PAIR_HL);
      if (s_ba)
        _push (PAIR_BA);
      else if (s_af)
        _push (PAIR_AF);

      fetchPair (PAIR_IY, result->aop);

      fp_offset = right->aop->aopu.aop_stk + (right->aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0);
      sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;
      emit2 ("!ldahlsp", sp_offset);
      regalloc_dry_run_cost += 4;

      emit2 ("ld b, !immedbyte", (unsigned) size);
      cost2 (2, 7);
      tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
      if (!regalloc_dry_run)
        emitLabel (tlbl);
      emit2 ("ld a, !*hl");
      cost2 (1, 7);
      emit2 ("ld !*iyx, a", 0);
      cost2 (1, 7);
      emit3w (A_INC, ASMOP_HL, 0);
      emit3w (A_INC, ASMOP_IY, 0);
      if (!regalloc_dry_run)
        emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
      regalloc_dry_run_cost += 2;

      spillPair (PAIR_HL);
      spillPair (PAIR_IY);

      if (s_ba)
        _pop (PAIR_BA);
      else if (s_af)
        _pop (PAIR_AF);
      if (s_hl)
        _pop (PAIR_HL);
      if (s_iy)
        _pop (PAIR_IY);
      goto release;
    }

  if (getPairId (result->aop) == PAIR_IY && !bit_field)
    {
      /* Just do it */
      while (size--)
        {
          if (canAssignToPtr3 (right->aop))
            {
              if (!regalloc_dry_run)
                emit2 ("ld !*iyx, %s", offset, aopGet (right->aop, offset, FALSE));
              if (right->aop->type == AOP_LIT)
                cost2 (4, 19); // ld d (iy), n
              else
                cost2 (3, 19); // ld d (iy), r
            }
          else
            {
              cheapMove (ASMOP_A, 0, right->aop, offset, true);
              emit2 ("ld !*iyx, a", offset);
              cost2 (3, 19); // ld d (iy), r
            }
          offset++;
        }
      goto release;
    }
  else if (getPairId (result->aop) == PAIR_HL && !isPairDead (PAIR_HL, ic) && !bit_field)
    {
      while (offset < size)
        {
          last_offset = offset;

          if (isRegOrLit (right->aop))
            {
              if (!regalloc_dry_run)
                emit2 ("ld !mems, %s", _pairs[PAIR_HL].name, aopGet (right->aop, offset, FALSE));
              ld_cost (aopInReg (right->aop, offset, A_IDX) ? ASMOP_L : ASMOP_A, 0, right->aop, offset, true);
              offset++;
            }
          else
            {
              if (surviving_a && !pushed_a && (!aopInReg (right->aop, 0, A_IDX) || offset))
                _push (PAIR_AF), pushed_a = TRUE;
              genMove_o (ASMOP_A, 0, right->aop, offset, 1, true, false, false, true);
              emit2 ("ld !mems, a", _pairs[PAIR_HL].name);
              cost2 (1, 7);
              offset++;
            }

          if (offset < size)
            {
              emit3w (A_INC, ASMOP_HL, 0);
              _G.pairs[PAIR_HL].offset++;
            }
        }

      /* Fixup HL back down */
      while (last_offset --> 0)
        emit3w (A_DEC, ASMOP_HL, 0);
      goto release;
    }

  if (!bit_field && isLitWord (result->aop) && size == 2 && offset == 0 && !sameRegs (result->aop, right->aop) && (right->aop->type == AOP_REG && getPairId (right->aop) != PAIR_INVALID || isLitWord (right->aop)))
    {
      bool save_hl = false;
      if (isLitWord (right->aop))
        {
          /* HL is the only scratch pair for the staged literal — save a live HL. */
          pairId = PAIR_HL;
          if (!isPairDead (PAIR_HL, ic))
            {
              _push (PAIR_HL);
              save_hl = true;
            }
          fetchPairLong (pairId, right->aop, ic, 0);
        }
      else
        pairId = getPairId (right->aop);
      emit2 ("ld !mems, %s", aopGetLitWordLong (result->aop, offset, FALSE), _pairs[pairId].name);
      if (pairId == PAIR_HL)
        cost2 (3, 16);
      else
        cost2 (4, 20);
      if (save_hl)
        _pop (PAIR_HL);
      goto release;
    }
  if (!bit_field && isLitWord (result->aop) && size == 4 && offset == 0 &&
    (getPartPairId (right->aop, 0) != PAIR_INVALID && getPartPairId (right->aop, 2) != PAIR_INVALID || isLitWord (right->aop)))
    {
      if (isLitWord (right->aop))
        {
          pairId = PAIR_HL;
          fetchPairLong (pairId, right->aop, ic, 0);
        }
      else
        pairId = getPartPairId (right->aop, 0);
      emit2 ("ld !mems, %s", aopGetLitWordLong (result->aop, offset, FALSE), _pairs[pairId].name);
      if (pairId == PAIR_HL)
        cost2 (3, 16);
      else
        cost2 (4, 20);
      if (isLitWord (right->aop))
        {
          pairId = PAIR_HL;
          fetchPairLong (pairId, right->aop, ic, 2);
        }
      else
        pairId = getPartPairId (right->aop, 2);
      emit2 ("ld (%s+%d), %s", aopGetLitWordLong (result->aop, offset, FALSE),2,  _pairs[pairId].name); // Handling of literal addresses is somewhat broken, use explicit offset as workaround.
      regalloc_dry_run_cost += (pairId == PAIR_HL) ? 3 : 4;
      goto release;
    }

  if (getPairId (result->aop) != pairId &&
    (right->aop->regs[_pairs[pairId].l_idx] >= 0 || right->aop->regs[_pairs[pairId].h_idx] >= 0))
    UNIMPLEMENTED;

  /* if the operand is already in dptr
     then we do nothing else we move the value to dptr */
  if (bit_field && getPairId (result->aop) != PAIR_INVALID && (getPairId (result->aop) != PAIR_IY || SPEC_BLEN (getSpec (operandType (result)->next)) < 8 || isPairDead (getPairId (result->aop), ic)))   /* Avoid destroying result by increments */
    pairId = getPairId (result->aop);
  else
    {
      if (!isPairDead (pairId, ic) && getPairId (result->aop) != pairId)
        {
          _push (pairId);
          pushed_pair = true;
        }
      genMove (pairId == PAIR_HL ? ASMOP_HL : ASMOP_IY, result->aop,
        isRegDead(A_IDX, ic) && right->aop->regs[A_IDX] < 0,
        isPairDead (PAIR_HL, ic) && right->aop->regs[L_IDX] < 0 && right->aop->regs[H_IDX] < 0,
        isPairDead (PAIR_IY, ic) && right->aop->regs[IYL_IDX] < 0 && right->aop->regs[IYH_IDX] < 0);
    }
  /* so hl now contains the address */
  /*freeAsmop (result, NULL, ic);*/

  /* if bit then pack */
  if (bit_field)
    {
      genPackBits (getSpec (operandType (result)->next), right, pairId, ic);
      goto release;
    }
  else
    {
      bool zero_a = false;

      for (offset = 0; offset < size;)
        {
          last_offset = offset;

          

          if (!zero_a && offset + 1 < size && aopIsLitVal (right->aop, offset, 2, 0x0000) && !surviving_a)
            {
              emit2 ("xor a, a");
              cost2 (1, 4);
              zero_a = true;
            }

          if (aopIsLitVal (right->aop, offset, 1, 0x00) && zero_a)
            {
              emit2 ("ld !mems, a", _pairs[pairId].name);
              cost2 (1, 7);
            }
          else if (isRegOrLit (right->aop) && pairId == PAIR_HL)
            {
              if (!regalloc_dry_run)
                emit2 ("ld !mems, %s", _pairs[pairId].name, aopGet (right->aop, offset, FALSE));
              ld_cost (aopInReg (right->aop, offset, A_IDX) ? ASMOP_L : ASMOP_A, 0, right->aop, offset, true);
            }
          else
            {
              if (surviving_a && !pushed_a && (!aopInReg (right->aop, 0, A_IDX) || offset))
                _push (PAIR_AF), pushed_a = true;
              genMove_o (ASMOP_A, 0, right->aop, offset, 1, true,
                pairId != PAIR_HL && isPairDead (PAIR_HL, ic) && right->aop->regs[L_IDX] < offset && right->aop->regs[H_IDX] < offset, pairId != PAIR_IY && isPairDead (PAIR_IY, ic) && right->aop->regs[IYL_IDX] < offset && right->aop->regs[IYH_IDX] < offset, true);
              zero_a = false;
              emit2 ("ld !mems, a", _pairs[pairId].name);
              cost2 (1, 7);
            }
          offset++;

          if (offset < size)
            {
              if (right->aop->regs[_pairs[pairId].l_idx] >= offset || right->aop->regs[_pairs[pairId].h_idx] >= offset)
                UNIMPLEMENTED;
              emit2 ("inc %s", _pairs[pairId].name);
              cost2 (1, 6);
              _G.pairs[pairId].offset++;
            }
        }
      /* Restore operand in pair. */
      if (!isPairDead (pairId, ic) && getPairId (result->aop) == pairId)
        while(last_offset --> 0)
          {
            emit2 ("dec %s", _pairs[pairId].name);
            cost2 (1, 6);
            _G.pairs[pairId].offset--;
          }
    }
release:
  if (pushed_pair)
    _pop (pairId);
  if (pushed_a)
    _pop (PAIR_AF);

  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genIfx - generate code for Ifx statement                        */
/*-----------------------------------------------------------------*/
static void
genIfx (iCode *ic, iCode *popIc)
{
  operand *cond = IC_COND (ic);
  int isbit = 0;

  aopOp (cond, ic, FALSE, TRUE);

  /* Special case: Condition is bool */
  if (IS_BOOL (operandType (cond)) && !aopInReg (cond->aop, 0, A_IDX) && !aopInReg (cond->aop, 0, IYL_IDX) && !aopInReg (cond->aop, 0, IYH_IDX))
    {
      emitBitTest (0, cond->aop, 0, ic);   // S1C88: bit reg,#0x01 (route L/H/mem via A/B)
      if (!regalloc_dry_run)
        genIfxJump (ic, "nz");

      goto release;
    }
  else if (cond->aop->size == 1 && !isRegDead (A_IDX, ic) &&
    (aopInReg (cond->aop, 0, B_IDX) || 0 || 0 || 0 || aopInReg (cond->aop, 0, H_IDX) || aopInReg (cond->aop, 0, L_IDX)))
    {
      emit3 (A_INC, cond->aop, 0);
      emit3 (A_DEC, cond->aop, 0);
      genIfxJump (ic, "nz");

      goto release;
    }
  else if (cond->aop->type != AOP_CRY)
    _toBoolean (cond, !popIc);
  else
    isbit = 1;
  /* the result is now in the accumulator */
  freeAsmop (cond, NULL);

  /* if the condition is  a bit variable */
  if (isbit && IS_ITEMP (cond) && SPIL_LOC (cond))
    genIfxJump (ic, SPIL_LOC (cond)->rname);
  else if (isbit && !IS_ITEMP (cond))
    genIfxJump (ic, OP_SYMBOL (cond)->rname);
  else
    genIfxJump (ic, popIc ? "a" : "nz");

  return;

release:

  freeAsmop (cond, NULL);

  return;
}

/*-----------------------------------------------------------------*/
/* genAddrOf - generates code for address of                       */
/*-----------------------------------------------------------------*/
static void
genAddrOf (const iCode * ic)
{
  symbol *sym;
  PAIR_ID pair;
  operand *right = IC_RIGHT (ic);
  wassert (IS_TRUE_SYMOP (IC_LEFT (ic)));
  wassert (right && IS_OP_LITERAL (IC_RIGHT (ic)));
  sym = OP_SYMBOL (IC_LEFT (ic));
  aopOp (IC_RESULT (ic), ic, true, false);

  if (sym->onStack)
    {
      int fp_offset = sym->stack + (sym->stack > 0 ? _G.stack.param_offset : 0) + (int)operandLitValue (right);
      int sp_offset = fp_offset + _G.stack.pushed + _G.stack.offset;
      bool in_fp_range = !_G.omitFramePtr && (fp_offset >= -128 && fp_offset < 128);

      pair = (getPairId (IC_RESULT (ic)->aop) == PAIR_IY) ? PAIR_IY : PAIR_HL;
      spillPair (pair);
      setupPairFromSP (pair, sp_offset);
    }
  else if (IC_RESULT (ic)->aop->size == 3)
    {
      /* the address of a __far object: a full 24-bit value — bytes 0-1 the
         16-bit offset, byte 2 the page (#((sym) >> 16)).  Route through a
         3-byte IMMD asmop so genMove places all three bytes into any result
         shape (the 2-byte commitPair path below would drop the page). */
      struct dbuf_s dbuf;
      struct asmop taop;

      dbuf_init (&dbuf, 128);
      if ((long)(operandLitValue (right)))
        dbuf_printf (&dbuf, "%s + %ld", sym->rname, (long)(operandLitValue (right)));
      else
        dbuf_printf (&dbuf, "%s", sym->rname);
      taop.type = AOP_IMMD;
      taop.size = 3;
      taop.aopu.aop_immd = dbuf_c_str (&dbuf);
      memset (taop.regs, -1, sizeof(taop.regs));
      taop.valinfo.anything = true;

      genMove (IC_RESULT (ic)->aop, &taop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), isPairDead (PAIR_IY, ic));

      dbuf_destroy (&dbuf);
      freeAsmop (IC_RESULT (ic), NULL);
      return;
    }
  else
    {
      pair = getPairId (IC_RESULT (ic)->aop);
      if (pair == PAIR_INVALID)
        {
          pair = PAIR_HL;
          spillPair (pair);
        }
      emit2 ("ld %s, !hashedstr+%ld", _pairs[pair].name, sym->rname, (long)(operandLitValue (right)));
      if (pair == PAIR_IY)
        cost2 (4, 14);
      else
        cost2 (3, 10);
    }

  commitPair (IC_RESULT (ic)->aop, pair, ic, FALSE);

  /* the address of a stack object is near: page 0 in byte 2 */
  if (IC_RESULT (ic)->aop->size == 3)
    genMove_o (IC_RESULT (ic)->aop, 2, ASMOP_ZERO, 0, 1, isRegDead (A_IDX, ic) && IC_RESULT (ic)->aop->regs[A_IDX] < 0, false, false, true);

  freeAsmop (IC_RESULT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genAssign - generate code for assignment                        */
/*-----------------------------------------------------------------*/
static void
genAssign (const iCode *ic)
{
  operand *result, *right;
  int size, offset;

  result = IC_RESULT (ic);
  right = IC_RIGHT (ic);
  
  const bool hl_dead = isPairDead (PAIR_HL, ic);

  /* Dont bother assigning if they are the same */
  if (operandsEqu (IC_RESULT (ic), IC_RIGHT (ic)))
    return;

  aopOp (right, ic, FALSE, FALSE);
  aopOp (result, ic, TRUE, FALSE);

  /* if they are the same registers */
  if (sameRegs (right->aop, result->aop))
    {
      emitDebug ("; (locations are the same)");
      goto release;
    }

  /* if the result is a bit */
  if (result->aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to assign to a bit");
    }

  /* general case */
  size = result->aop->size;
  offset = 0;


  if (isPair (result->aop) && getPairId (result->aop) != PAIR_IY ||
    isPair (right->aop) && result->aop->type == AOP_IY && size == 2)
    genMove (result->aop, right->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), true);
  else if (size == 2 && isPairDead (PAIR_HL, ic) && ((right->aop->type == AOP_STK && !_G.omitFramePtr || right->aop->type == AOP_IY || right->aop->type == AOP_LIT) && result->aop->type == AOP_IY || right->aop->type == AOP_IY && (result->aop->type == AOP_STK && !_G.omitFramePtr || result->aop->type == AOP_IY) || right->aop->type == AOP_LIT && (result->aop->type == AOP_STK || result->aop->type == AOP_EXSTK) && (result->aop->aopu.aop_stk + offset + _G.stack.offset + (result->aop->aopu.aop_stk > 0 ? _G.stack.param_offset : 0) + _G.stack.pushed) == 0)) // Use ld d(sp), hl
    {
      fetchPair (PAIR_HL, right->aop);
      genMove (result->aop, ASMOP_HL, isRegDead (A_IDX, ic), true, isPairDead (PAIR_IY, ic));
    }
  else if (size == 2 && getPairId (right->aop) != PAIR_INVALID)
    genMove (result->aop, right->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), isPairDead (PAIR_IY, ic));

  else
    {
      if ((result->aop->type == AOP_REG || result->aop->type == AOP_STK || result->aop->type == AOP_EXSTK || result->aop->type == AOP_IY || result->aop->type == AOP_HL) && (right->aop->type == AOP_REG || right->aop->type == AOP_STK || right->aop->type == AOP_EXSTK || right->aop->type == AOP_LIT || right->aop->type == AOP_IMMD || right->aop->type == AOP_DIR || right->aop->type == AOP_IY || right->aop->type == AOP_HL))
        genMove (result->aop, right->aop, isRegDead (A_IDX, ic), isPairDead (PAIR_HL, ic), isPairDead (PAIR_IY, ic));
      else
        while (size--)
          {
            const bool hl_free = hl_dead &&
              (right->aop->regs[L_IDX] <= offset) && (right->aop->regs[H_IDX] <= offset) &&
              (result->aop->regs[L_IDX] < 0 || result->aop->regs[L_IDX] >= offset) && (result->aop->regs[H_IDX] < 0 || result->aop->regs[H_IDX] >= offset);
            const bool save_hl = !hl_free && ((IY_RESERVED) && (requiresHL (right->aop) || requiresHL (result->aop)));

            if (save_hl)
              _push (PAIR_HL);
            cheapMove (result->aop, offset, right->aop, offset, isRegDead (A_IDX, ic));
            if (save_hl)
              {
                _pop (PAIR_HL);
                spillPair (PAIR_HL);
              }
            offset++;
          }
    }

release:
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genJumpTab - generate code for jump table                       */
/*-----------------------------------------------------------------*/
static void
genJumpTab (const iCode *ic)
{
  symbol *jtab = NULL;
  operand *jtcond = IC_JTCOND (ic);
  bool pushed_pair = false;
  PAIR_ID pair;

  aopOp (jtcond, ic, false, false);

  wassert (isPairDead (PAIR_HL, ic));

  if (!regalloc_dry_run)
    jtab = newiTempLabel (NULL);

  

  /* S1C88: BA is the scratch pair for the table-offset addition (HL is the
     table pointer). */
  pair = PAIR_BA;
  if (!isPairDead (PAIR_BA, ic))
    {
      _push (PAIR_BA);
      pushed_pair = true;
    }

  genMove (ASMOP_BA, jtcond->aop, true, true, isPairDead (PAIR_IY, ic));

  if (!regalloc_dry_run)
    emit2 ("ld hl, !immed!tlabel", labelKey2num (jtab->key));
  cost2 (3, 10);
  emit2 ("add hl, %s", _pairs[pair].name);
  cost2 (1, 11);
  emit2 ("add hl, %s", _pairs[pair].name);
  cost2 (1, 11);
  spillPair (PAIR_HL);

  {
      emit2 ("ld a, !*hl");
      cost2 (1, 7);
      emit2 ("inc hl");
      cost2 (1, 6);
      emit2 ("ld h, !*hl");
      cost2 (1, 7);
      emit3 (A_LD, ASMOP_L, ASMOP_A);
    }

jump:
  if (pushed_pair)
    _pop (pair);

  emit2 ("!jphl");
  cost2 (1, 4);

  if (!regalloc_dry_run)
    {
      emitLabelSpill (jtab);
      for (jtab = setFirstItem (IC_JTLABELS (ic)); jtab; jtab = setNextItem (IC_JTLABELS (ic)))
        emit2 (".dw !tlabel", labelKey2num (jtab->key));
    }
  // regalloc_dry_run_cost += 3 // doesn't matter and might overflow cost

  freeAsmop (IC_JTCOND (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genCast - gen code for casting                                  */
/*-----------------------------------------------------------------*/
static void
genCast (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  operand *right = IC_RIGHT (ic);
  sym_link *resulttype = operandType (result);
  sym_link *righttype = operandType (right);
  int size;
  bool surviving_a = !isRegDead (A_IDX, ic);
  bool pushed_a = FALSE;

  /* if they are equivalent then do nothing */
  if (operandsEqu (IC_RESULT (ic), IC_RIGHT (ic)))
    return;

  aopOp (right, ic, false, false);
  aopOp (result, ic, true, false);

  /* if the result is a bit */
  if (result->aop->type == AOP_CRY)
    {
      wassertl (0, "Tried to cast to a bit");
    }

  unsigned topbytemask = (IS_BITINT (resulttype) && (SPEC_BITINTWIDTH (resulttype) % 8)) ?
   (0xff >> (8 - SPEC_BITINTWIDTH (resulttype) % 8)) : 0xff;

  // Cast to _BitInt can require mask of top byte.
  if (IS_BITINT (resulttype) && (SPEC_BITINTWIDTH (resulttype) % 8) && bitsForType (resulttype) < bitsForType (righttype))
    {
      if (!isRegDead (A_IDX, ic) || result->aop->regs[A_IDX] >= 0 && result->aop->regs[A_IDX] != result->aop->size - 1)
        _push (PAIR_AF), pushed_a = true;
      genMove (result->aop, right->aop, true, isPairDead (PAIR_HL, ic), isPairDead (PAIR_IY, ic));
      cheapMove (ASMOP_A, 0, result->aop, result->aop->size - 1, true);
      emit2 ("and a, #0x%02x", topbytemask);
      cost2 (2, 7);
      if (!SPEC_USIGN (resulttype)) // Sign-extend
        {
          symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (0);
          emit2 ("bit a, #0x%02x", 1u << (int)(SPEC_BITINTWIDTH (resulttype) % 8 - 1));   // S1C88: bit reg,#mask
          cost2 (2, 8);
          if (!regalloc_dry_run)
            emit2 ("jr z, !tlabel", labelKey2num (tlbl->key));
          emit2 ("or a, #0x%02x", ~topbytemask & 0xffu);
          regalloc_dry_run_cost += 4;
          emitLabel (tlbl);
        }
      cheapMove (result->aop, result->aop->size - 1, ASMOP_A, 0, true);

      goto release;
    }

  /* casting to bool */
  
  if (IS_BOOL (resulttype))
    {
      _castBoolean (right);
      outAcc (result);
      goto release;
    }

  /* if they are the same size or less */
  if (result->aop->size <= right->aop->size)
    {
      genAssign (ic);
      goto release;
    }

  // Now we know that the size of destination is greater than the size of the source

  /* now depending on the sign of the destination */
  size = result->aop->size - right->aop->size;
  
  /* Unsigned or not an integral type - fill with zeros */
  if (IS_BOOL (righttype) || !IS_SPEC (righttype) || SPEC_USIGN (righttype) || right->aop->type == AOP_CRY)
    {
      genMove_o (result->aop, 0, right->aop, 0, right->aop->size, !surviving_a, isPairDead (PAIR_HL, ic), isPairDead (PAIR_IY, ic), true);
      surviving_a |= (result->aop->regs[A_IDX] >= 0 && result->aop->regs[A_IDX] < right->aop->size);
      bool hl_dead = isPairDead (PAIR_HL, ic) && (result->aop->regs[L_IDX] < 0 || result->aop->regs[L_IDX] >= right->aop->size) && (result->aop->regs[H_IDX] < 0 || result->aop->regs[H_IDX] >= right->aop->size);
      bool iy_dead = true && (result->aop->regs[IYL_IDX] < 0 || result->aop->regs[IYL_IDX] >= right->aop->size) && (result->aop->regs[IYH_IDX] < 0 || result->aop->regs[IYH_IDX] >= right->aop->size);
      /* Zero-filling the upper bytes into memory needs a scratch (xor a,a; ld
         (mem),a) and clobbers A — genMove_o does NOT honor a_dead for a
         ZERO->memory fill. If A is live across the cast (e.g. it still holds the
         source value needed by a following compare), preserve it here, exactly
         as the signed path below does. Otherwise a `(unsigned long)x` whose x is
         also compared miscompiles: the compare reads a zeroed A. */
      if (surviving_a && !pushed_a)
        _push (PAIR_AF), pushed_a = true;
      genMove_o (result->aop, right->aop->size, ASMOP_ZERO, 0, size, true, hl_dead, iy_dead, true);
    }
  else
    {
      bool maskedtopbyte = IS_BITINT (resulttype) && (SPEC_BITINTWIDTH (resulttype) % 8) && SPEC_USIGN (resulttype);
      genMove_o (result->aop, 0, right->aop, 0, right->aop->size - 1, !surviving_a, isPairDead (PAIR_HL, ic), isPairDead (PAIR_IY, ic), true);
      bool iy_dead = true && (result->aop->regs[IYL_IDX] < 0 || result->aop->regs[IYL_IDX] >= right->aop->size) && (result->aop->regs[IYH_IDX] < 0 || result->aop->regs[IYH_IDX] >= right->aop->size);
      bool hl_dead = isPairDead (PAIR_HL, ic) && (result->aop->regs[L_IDX] < 0 || result->aop->regs[L_IDX] >= right->aop->size) && (result->aop->regs[H_IDX] < 0 || result->aop->regs[H_IDX] >= right->aop->size);
      if (result->aop->type == AOP_REG && right->aop->type == AOP_REG && // Overwritten last byte of right operand
        result->aop->regs[right->aop->aopu.aop_reg[right->aop->size - 1]->rIdx] >= 0 && result->aop->regs[right->aop->aopu.aop_reg[right->aop->size - 1]->rIdx] < right->aop->size - 1)
        UNIMPLEMENTED;
      int offset = right->aop->size - 1;
      surviving_a |= (result->aop->regs[A_IDX] >= 0 && result->aop->regs[A_IDX] < offset);
      if (surviving_a && !pushed_a)
        _push (PAIR_AF), pushed_a = true;

      genMove_o (ASMOP_A, 0, right->aop, offset, 1, true, hl_dead, iy_dead, true);
      if (right->aop->type != AOP_REG || result->aop->type != AOP_REG || right->aop->aopu.aop_reg[offset] != result->aop->aopu.aop_reg[offset])
        cheapMove (result->aop, offset, ASMOP_A, 0, true);
      offset++;
      
      surviving_a |= (result->aop->regs[A_IDX] >= 0 && result->aop->regs[A_IDX] < offset);
      if (surviving_a && !pushed_a)
        _push (PAIR_AF), pushed_a = true;

      /* we need to extend the sign */
      emit3 (A_RLC, ASMOP_A, 0);

      /* S1C88: no `sbc hl, hl` — the byte-wise `sbc a, a` path below serves
         the 16-bit case too. */
      {
          emit3 (A_SBC, ASMOP_A, ASMOP_A);
          while (size--)
            {
              if (!size && maskedtopbyte) // For casts from signed integers to wider unsigned _BitInt
                {
                  emit2 ("and a, #0x%02x", topbytemask);
                  cost2 (2, 7);
                }
              cheapMove (result->aop, offset++, ASMOP_A, 0, true);
            }
        }
    }

release:
  if (pushed_a)
    _pop (PAIR_AF);
  freeAsmop (right, NULL);
  freeAsmop (result, NULL);
}

/*-----------------------------------------------------------------*/
/* genReceive - generate code for a receive iCode                  */
/*-----------------------------------------------------------------*/
static void
genReceive (const iCode *ic)
{
  operand *result = IC_RESULT (ic);
  aopOp (result, ic, true, false);
  
  wassert (currFunc && ic->argreg);

  bool dead_regs[IYH_IDX + 1];
  
  for (int i = 0; i <= IYH_IDX; i++)
    dead_regs[i] = isRegDead (i, ic);

  for(iCode *nic = ic->next; nic && nic->op == RECEIVE; nic = nic->next)
    {
      asmop *narg = aopArg (currFunc->type, nic->argreg);
      wassert (narg);
      for (int i = 0; i < narg->size; i++)
        dead_regs[narg->aopu.aop_reg[i]->rIdx] = false;
    }
    
  if (result->aop->type == AOP_REG)
    for (int i = 0; i < result->aop->size; i++)
      if (!dead_regs[result->aop->aopu.aop_reg[i]->rIdx])
        UNIMPLEMENTED;

  genMove (result->aop, aopArg (currFunc->type, ic->argreg), dead_regs[A_IDX], dead_regs[L_IDX] && dead_regs[H_IDX], dead_regs[IYL_IDX] && dead_regs[IYH_IDX]);

  freeAsmop (IC_RESULT (ic), NULL);
}

/*-----------------------------------------------------------------*/
/* genDummyRead - generate code for dummy read of volatiles        */
/*-----------------------------------------------------------------*/
static void
genDummyRead (const iCode * ic)
{
  operand *op;
  int size, offset;

  op = IC_RIGHT (ic);
  if (op && IS_SYMOP (op))
    {
      aopOp (op, ic, FALSE, FALSE);

      /* general case */
      size = op->aop->size;
      offset = 0;

      while (size--)
        {
          _moveA3 (op->aop, offset);
          offset++;
        }

      freeAsmop (op, NULL);
    }

  op = IC_LEFT (ic);
  if (op && IS_SYMOP (op))
    {
      aopOp (op, ic, FALSE, FALSE);

      /* general case */
      size = op->aop->size;
      offset = 0;

      while (size--)
        {
          _moveA3 (op->aop, offset);
          offset++;
        }

      freeAsmop (op, NULL);
    }
}

/*-----------------------------------------------------------------*/
/* genCritical - generate code for start of a critical sequence    */
/*-----------------------------------------------------------------*/
static void
genCritical (const iCode * ic)
{
  /* S1C88 has no ei/di: a critical section masks all maskable interrupts by
     raising the SC interrupt-priority level to 3 (I1:I0 = bits 7:6).  The prior
     SC (level + flags) is saved so genEndCritical can restore it (correct under
     nesting and for whatever the prior level was). */
  if (IC_RESULT (ic))
    {
      /* Thread the saved state through the result itemp (the body may use the
         stack, so don't keep it there). */
      aopOp (IC_RESULT (ic), ic, true, false);
      emit2 ("ld a, sc");
      cost2 (2, 8);
      cheapMove (IC_RESULT (ic)->aop, 0, ASMOP_A, 0, true);
      emit2 ("or sc, !immedbyte", 0xc0u);
      cost2 (2, 7);
      freeAsmop (IC_RESULT (ic), NULL);
    }
  else
    {
      emit2 ("push sc");
      cost2 (1, 11);
      emit2 ("or sc, !immedbyte", 0xc0u);
      cost2 (2, 7);
    }
}

/*-----------------------------------------------------------------*/
/* genEndCritical - generate code for end of a critical sequence   */
/*-----------------------------------------------------------------*/
static void
genEndCritical (const iCode * ic)
{
  /* Restore the SC (interrupt-priority level + flags) saved by genCritical. */
  if (IC_RIGHT (ic))
    {
      /* Saved state was threaded through an itemp: load it and write SC. */
      aopOp (IC_RIGHT (ic), ic, FALSE, TRUE);
      cheapMove (ASMOP_A, 0, IC_RIGHT (ic)->aop, 0, true);
      emit2 ("ld sc, a");
      cost2 (2, 8);
      freeAsmop (IC_RIGHT (ic), NULL);
    }
  else
    {
      emit2 ("pop sc");
      cost2 (1, 10);
    }
}

/* (The genArrayInit/__initrleblock machinery was deleted: the port sets
   arrayInitializerSuppported = FALSE, so ARRAYINIT iCodes are never generated.) */

/* Load source -> HL and dest -> IY without clobbering each other (the S1C88
   byte-copy register layout).  IY is not byte-addressable, so loading it never
   touches L/H, and loading HL never touches IY; the only hazard is a source
   overlapping the other destination, handled by ordering + genMove's dead-set.
   A must be free (saved or dead) for use as scratch. */
static void
setupHLSrcIYDst (const operand *from, const operand *to)
{
  bool from_in_iy = aopInReg (from->aop, 0, IYL_IDX) || aopInReg (from->aop, 1, IYH_IDX) ||
                    aopInReg (from->aop, 0, IYH_IDX) || aopInReg (from->aop, 1, IYL_IDX);
  bool from_in_hl = aopInReg (from->aop, 0, L_IDX) || aopInReg (from->aop, 1, H_IDX) ||
                    aopInReg (from->aop, 0, H_IDX) || aopInReg (from->aop, 1, L_IDX);
  if (from_in_iy)
    {
      genMove (ASMOP_HL, from->aop, true, true, false);
      genMove (ASMOP_IY, to->aop,   true, true, true);
    }
  else
    {
      genMove (ASMOP_IY, to->aop,   true, !from_in_hl, true);
      genMove (ASMOP_HL, from->aop, true, true, false);
    }
}

static void
genBuiltInMemcpy (const iCode *ic, int nparams, operand **pparams)
{
  int i;
  operand *from, *to, *count;
  unsigned int n;

  for (i = 0; i < nparams; i++)
    aopOp (pparams[i], ic, FALSE, FALSE);

  wassertl (nparams == 3, "Built-in memcpy() must have three parameters.");

  count = pparams[2];
  from = pparams[1];
  to = pparams[0];

  if (pparams[2]->aop->type != AOP_LIT)
    n = UINT_MAX;
  else if (!(n = (unsigned int) ulFromVal (pparams[2]->aop->aopu.aop_lit))) /* Check for zero length copy. */
    goto done;

  /* S1C88 has no ldir and no DE pair.  Copy with a forward byte loop
       HL = source, IY = dest
     which matches memcpy()'s no-overlap (forward) semantics, exactly like
     ldir.  The counter is B for count <= 255 (djr nz) or the borrowed frame
     pointer IX for larger counts (dec ix; jrs nz — IX isn't referenced inside
     the loop, so it is saved/restored around it).  A variable (non-literal)
     count uses the IX-counter loop below with a `cp ix,#0` zero guard. */
  if (n != UINT_MAX)
    {
      bool wide = (n > 255);
      bool s_hl = !isPairDead (PAIR_HL, ic);
      bool s_iy = !isPairDead (PAIR_IY, ic);
      /* A is the per-byte temp; B the narrow loop counter.  If B is needed and
         live, save BA (covers A too, keeps the stack even); else save just A
         via AF when live. */
      bool s_ba = !wide && !isRegDead (B_IDX, ic);
      bool s_af = !s_ba && !isRegDead (A_IDX, ic);
      symbol *tlbl;

      if (s_hl)
        _push (PAIR_HL);
      if (s_iy)
        _push (PAIR_IY);
      if (s_ba)
        _push (PAIR_BA);
      else if (s_af)
        _push (PAIR_AF);

      setupHLSrcIYDst (from, to);

      if (!wide)
        {
          emit2 ("ld b, !immedbyte", (unsigned) n);
          cost2 (2, 7);
        }
      else
        {
          _push (PAIR_IX);                          /* borrow the frame pointer */
          emit2 ("ld ix, !immedword", (unsigned) n);
          cost2 (3, 10);
        }

      tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
      if (!regalloc_dry_run)
        emitLabel (tlbl);
      emit2 ("ld a, !*hl");
      cost2 (1, 7);
      emit2 ("ld !*iyx, a", 0);
      cost2 (1, 7);
      emit3w (A_INC, ASMOP_HL, 0);
      emit3w (A_INC, ASMOP_IY, 0);
      if (!wide)
        {
          if (!regalloc_dry_run)
            emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
          regalloc_dry_run_cost += 2;
        }
      else
        {
          emit2 ("dec ix");
          cost2 (2, 10);
          if (!regalloc_dry_run)
            emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key)); /* peephole -> jrs NZ */
          regalloc_dry_run_cost += 2;
          _pop (PAIR_IX);
        }

      spillPair (PAIR_HL);
      spillPair (PAIR_IY);
      if (s_ba)
        _pop (PAIR_BA);
      else if (s_af)
        _pop (PAIR_AF);
      if (s_iy)
        _pop (PAIR_IY);
      if (s_hl)
        _pop (PAIR_HL);

      goto done;
    }

  /* Variable (runtime) count: the same native byte loop, with the count held
     in the borrowed frame pointer IX (16-bit counter) and a `cp ix,#0` zero
     guard (a byte loop entered with count==0 would otherwise wrap to 65536,
     exactly like a block copy with count==0). */
  {
    bool s_hl = !isPairDead (PAIR_HL, ic);
    bool s_iy = !isPairDead (PAIR_IY, ic);
    bool s_af = !isRegDead (A_IDX, ic);          /* A is the per-byte temp */
    symbol *tlbl, *elbl;

    if (s_hl)
      _push (PAIR_HL);
    if (s_iy)
      _push (PAIR_IY);
    if (s_af)
      _push (PAIR_AF);
    _push (PAIR_IX);                              /* borrow the frame pointer */

    /* Count -> IX first, while it is still in its source location.  The HL/IY
       loads below use A as scratch but never touch IX, so the count survives;
       and they are 16-bit pair loads (register moves or `ld pair,dd(sp)`), so
       they do not need IX as a frame pointer. */
    fetchPair (PAIR_IX, count->aop);

    /* Source -> HL, dest -> IY (clobber-safe ordering; A is free). */
    setupHLSrcIYDst (from, to);

    tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);
    elbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);

    emit2 ("cp ix, !immedword", 0u);             /* count == 0 ? */
    cost2 (3, 12);
    if (!regalloc_dry_run)
      emit2 ("jp Z, !tlabel", labelKey2num (elbl->key));   /* peephole -> jrs Z */
    regalloc_dry_run_cost += 2;

    if (!regalloc_dry_run)
      emitLabel (tlbl);
    emit2 ("ld a, !*hl");
    cost2 (1, 7);
    emit2 ("ld !*iyx, a", 0);
    cost2 (1, 7);
    emit3w (A_INC, ASMOP_HL, 0);
    emit3w (A_INC, ASMOP_IY, 0);
    emit2 ("dec ix");
    cost2 (2, 10);
    if (!regalloc_dry_run)
      emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key));   /* peephole -> jrs NZ */
    regalloc_dry_run_cost += 2;
    if (!regalloc_dry_run)
      emitLabel (elbl);

    _pop (PAIR_IX);
    spillPair (PAIR_HL);
    spillPair (PAIR_IY);
    if (s_af)
      _pop (PAIR_AF);
    if (s_iy)
      _pop (PAIR_IY);
    if (s_hl)
      _pop (PAIR_HL);
  }

done:
  freeAsmop (count, NULL);
  freeAsmop (to, NULL);
  freeAsmop (from, NULL);

  /* No need to assign result - would have used ordinary memcpy() call instead. */
}

static void
setupForMemset (const iCode *ic, const operand *dst, const operand *c, bool direct_c)
{
  /* Both are in regs. Let regMove() do the shuffling. */
  if (dst->aop->type == AOP_REG && !direct_c && c->aop->type == AOP_REG)
    {
      const short larray[2] = {L_IDX, H_IDX};
      short oparray[2];
      bool early_a = c->aop->type == AOP_REG && (c->aop->aopu.aop_reg[0]->rIdx == L_IDX || c->aop->aopu.aop_reg[0]->rIdx == H_IDX);

      if (early_a)
        cheapMove (ASMOP_A, 0, c->aop, 0, true);

      oparray[0] = dst->aop->aopu.aop_reg[0]->rIdx;
      oparray[1] = dst->aop->aopu.aop_reg[1]->rIdx;

      regMove (larray, oparray, 2, early_a);

      if (!early_a)
        cheapMove (ASMOP_A, 0, c->aop, 0, true);
    }
  else if (c->aop->type == AOP_REG && requiresHL (c->aop))
    {
      cheapMove (ASMOP_A, 0, c->aop, 0, true);
      if (dst->aop->type == AOP_EXSTK)
        _push (PAIR_AF);
      fetchPair (PAIR_HL, dst->aop);
      if (dst->aop->type == AOP_EXSTK)
        _pop (PAIR_AF);
    }
  else
    {
      bool a_free = isRegDead (A_IDX, ic) && !aopInReg (c->aop, 0, A_IDX);
      genMove (ASMOP_HL, dst->aop, a_free, true, false);
      if (!direct_c)
        {
          if (requiresHL (c->aop))
            _push (PAIR_HL);
          cheapMove (ASMOP_A, 0, c->aop, 0, true);
          if (requiresHL (c->aop))
            _pop (PAIR_HL);
        }
    }
}

static void
genBuiltInMemset (const iCode *ic, int nParams, operand **pparams)
{
  operand *dst, *c, *n;
  bool direct_c, direct_cl;
  unsigned size;
  bool saved_HL = false, saved_AF = false, saved_B = false;

  wassertl (nParams == 3, "Built-in memset() must have three parameters");

  dst = pparams[0];
  c = pparams[1];
  n = pparams[2];

  aopOp (c, ic, FALSE, FALSE);
  aopOp (dst, ic, FALSE, FALSE);
  aopOp (n, ic, FALSE, FALSE);

  wassertl (n->aop->type == AOP_LIT, "Last parameter to builtin memset() must be literal.");

  if (n->aop->type != AOP_LIT || !(size = ulFromVal (n->aop->aopu.aop_lit)))
    goto done;

  /* S1C88: fill with a native `ld (hl), x; inc hl` walk.  (hl)-stores exist
     for #imm and A/B/L/H; L/H are consumed by the destination pointer, so the
     fill value is usable directly only as a literal or from A/B.  The looped
     forms use B as the counter (or the borrowed frame pointer IX when
     size > 255), so there the fill value must not sit in B. */
  direct_c = (c->aop->type == AOP_LIT || c->aop->type == AOP_REG &&
              (c->aop->aopu.aop_reg[0]->rIdx == A_IDX || c->aop->aopu.aop_reg[0]->rIdx == B_IDX));
  direct_cl = (c->aop->type == AOP_LIT || c->aop->type == AOP_REG &&
               c->aop->aopu.aop_reg[0]->rIdx == A_IDX);

  if (!isPairDead (PAIR_HL, ic))
    {
      _push (PAIR_HL);
      saved_HL = true;
    }
  /* A is the fill register whenever the value can't be used directly. */
  if ((size <= 4 && !direct_c || size > 4 && !direct_cl) && !isRegDead (A_IDX, ic) && !aopInReg (c->aop, 0, A_IDX))
    {
      _push (PAIR_AF);
      saved_AF = true;
    }

  if (size <= 4) /* straight-line */
    {
      setupForMemset (ic, dst, c, direct_c);

      while (size--)
        {
          if (!regalloc_dry_run)
            emit2 ("ld !*hl, %s", aopGet (direct_c ? c->aop : ASMOP_A, 0, FALSE));
          cost2 (1, 7);
          if (size)
            emit3w (A_INC, ASMOP_HL, 0);
        }
    }
  else if (size <= 255) /* B-counter loop */
    {
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);

      /* B becomes the counter; save it when live — even when it holds the fill
         value (setupForMemset reads it into A first, the pop restores it). */
      if (!isRegDead (B_IDX, ic))
        {
          emit2 ("push b");
          cost2 (1, 11);
          _G.stack.pushed += 1;
          saved_B = true;
        }

      setupForMemset (ic, dst, c, direct_cl);

      emit2 ("ld b, !immedbyte", (unsigned) size);
      cost2 (2, 7);
      if (!regalloc_dry_run)
        {
          emitLabel (tlbl);
          emit2 ("ld !*hl, %s", aopGet (direct_cl ? c->aop : ASMOP_A, 0, FALSE));
          emit2 ("inc hl");
          emit2 ("djr nz, !tlabel", labelKey2num (tlbl->key));
        }
      regalloc_dry_run_cost += 5;
    }
  else /* wide: borrow the frame pointer IX as a 16-bit counter */
    {
      symbol *tlbl = regalloc_dry_run ? 0 : newiTempLabel (NULL);

      setupForMemset (ic, dst, c, direct_cl);

      _push (PAIR_IX);
      emit2 ("ld ix, !immedword", (unsigned) size);
      cost2 (3, 10);
      if (!regalloc_dry_run)
        {
          emitLabel (tlbl);
          emit2 ("ld !*hl, %s", aopGet (direct_cl ? c->aop : ASMOP_A, 0, FALSE));
          emit2 ("inc hl");
        }
      regalloc_dry_run_cost += 3;
      emit2 ("dec ix");
      cost2 (2, 10);
      if (!regalloc_dry_run)
        emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key)); /* peephole -> jrs NZ */
      regalloc_dry_run_cost += 2;
      _pop (PAIR_IX);
    }

done:
  spillPair (PAIR_HL);

  freeAsmop (n, NULL);
  freeAsmop (c, NULL);
  freeAsmop (dst, NULL);

  if (saved_B)
    {
      emit2 ("pop b");
      cost2 (1, 10);
      _G.stack.pushed -= 1;
    }
  if (saved_AF)
    _pop (PAIR_AF);
  if (saved_HL)
    _pop (PAIR_HL);

  /* No need to assign result - would have used ordinary memset() call instead. */
}

static void
genBuiltInStrcpy (const iCode *ic, int nParams, operand **pparams)
{
  operand *dst, *src;
  bool saved_HL = false, saved_IY = false;
  int i;
  bool SomethingReturned;

  SomethingReturned = IS_ITEMP (IC_RESULT (ic)) && (OP_SYMBOL (IC_RESULT (ic))->nRegs || OP_SYMBOL (IC_RESULT (ic))->spildir) ||
                      IS_TRUE_SYMOP (IC_RESULT (ic));

  wassertl (nParams == 2, "Built-in strcpy() must have two parameters.");

  dst = pparams[0];
  src = pparams[1];

  for (i = 0; i < nParams; i++)
    aopOp (pparams[i], ic, FALSE, FALSE);

  if (!isRegDead (A_IDX, ic))
    UNIMPLEMENTED;

  if (!isPairDead (PAIR_HL, ic))
    {
      _push (PAIR_HL);
      saved_HL = true;
    }
  if (!isPairDead (PAIR_IY, ic))
    {
      _push (PAIR_IY);
      saved_IY = true;
    }

  /* src -> HL, dst -> IY; native byte loop, NUL byte is copied too.
     The 16-bit incs clobber Z on the S1C88, so the copied byte is re-tested
     with `or a, a` after them (A is untouched by the incs). */
  setupHLSrcIYDst (src, dst);

  if (SomethingReturned)
    _push (PAIR_IY);                       /* the returned original dst */

  if (!regalloc_dry_run)
    {
      symbol *tlbl = newiTempLabel (NULL);
      emitLabel (tlbl);
      emit2 ("ld a, !*hl");
      emit2 ("ld !*iyx, a", 0);
      emit2 ("inc hl");
      emit2 ("inc iy");
      emit2 ("or a, a");
      emit2 ("jp NZ, !tlabel", labelKey2num (tlbl->key)); /* peephole -> jrs NZ */
    }
  regalloc_dry_run_cost += 8;

  spillPair (PAIR_HL);
  spillPair (PAIR_IY);

  if (SomethingReturned)
    aopOp (IC_RESULT (ic), ic, true, false);

  if (!SomethingReturned || SomethingReturned && getPairId (IC_RESULT (ic)->aop) != PAIR_INVALID)
    {
      if (SomethingReturned)
        _pop (getPairId (IC_RESULT (ic)->aop));
      if (saved_IY)
        _pop (PAIR_IY);
      if (saved_HL)
        _pop (PAIR_HL);
    }
  else
    {
      _pop (PAIR_HL);
      genMove (IC_RESULT (ic)->aop, ASMOP_HL, true, true, true);

      restoreRegs (saved_IY, false, saved_HL, IC_RESULT (ic), ic);
    }

  if (SomethingReturned)
    freeAsmop (IC_RESULT (ic), NULL);
  freeAsmop (src, NULL);
  freeAsmop (dst, NULL);
}

static void
genBuiltInStrncpy (const iCode *ic, int nparams, operand **pparams)
{
  int i;
  operand *s1, *s2, *n;
  unsigned size;
  bool wide;
  bool saved_HL = false, saved_IY = false, saved_B = false;

  for (i = 0; i < nparams; i++)
    aopOp (pparams[i], ic, FALSE, FALSE);

  wassertl (nparams == 3, "Built-in strncpy() must have three parameters.");
  wassertl (pparams[2]->aop->type == AOP_LIT, "Last parameter to builtin strncpy() must be literal.");

  s1 = pparams[0];
  s2 = pparams[1];
  n = pparams[2];

  if (!(size = ulFromVal (n->aop->aopu.aop_lit)))
    goto done;
  wide = (size > 255);

  if (!isRegDead (A_IDX, ic))
    UNIMPLEMENTED;

  if (!isPairDead (PAIR_HL, ic))
    {
      _push (PAIR_HL);
      saved_HL = true;
    }
  if (!isPairDead (PAIR_IY, ic))
    {
      _push (PAIR_IY);
      saved_IY = true;
    }
  if (!wide && !isRegDead (B_IDX, ic))
    {
      emit2 ("push b");
      cost2 (1, 11);
      _G.stack.pushed += 1;
      saved_B = true;
    }

  /* src -> HL, dst -> IY; copy up to n bytes, stopping after a copied NUL,
     then zero-fill the remainder (A still holds the NUL).  The counter is
     the remaining-store count: B (djr nz) or the borrowed frame pointer IX
     when n > 255.  The 16-bit incs clobber Z, so the copied byte is
     re-tested with `or a, a`; the counter dec provides the loop Z. */
  setupHLSrcIYDst (s2, s1);

  if (!wide)
    {
      emit2 ("ld b, !immedbyte", (unsigned) size);
      cost2 (2, 7);
      if (!regalloc_dry_run)
        {
          symbol *copy = newiTempLabel (NULL);
          symbol *padc = newiTempLabel (NULL);
          symbol *pad = newiTempLabel (NULL);
          symbol *end = newiTempLabel (NULL);
          emitLabel (copy);
          emit2 ("ld a, !*hl");
          emit2 ("inc hl");
          emit2 ("ld !*iyx, a", 0);
          emit2 ("inc iy");
          emit2 ("or a, a");
          emit2 ("jp Z, !tlabel", labelKey2num (padc->key));   /* copied the NUL */
          emit2 ("djr nz, !tlabel", labelKey2num (copy->key));
          emit2 ("jp !tlabel", labelKey2num (end->key));       /* n bytes, no NUL */
          emitLabel (padc);
          emit2 ("djr nz, !tlabel", labelKey2num (pad->key));  /* account the NUL store */
          emit2 ("jp !tlabel", labelKey2num (end->key));
          emitLabel (pad);
          emit2 ("ld !*iyx, a", 0);                            /* A == 0 here */
          emit2 ("inc iy");
          emit2 ("djr nz, !tlabel", labelKey2num (pad->key));
          emitLabel (end);
        }
      regalloc_dry_run_cost += 18;
    }
  else
    {
      _push (PAIR_IX);                       /* borrow the frame pointer */
      emit2 ("ld ix, !immedword", (unsigned) size);
      cost2 (3, 10);
      if (!regalloc_dry_run)
        {
          symbol *copy = newiTempLabel (NULL);
          symbol *padc = newiTempLabel (NULL);
          symbol *pad = newiTempLabel (NULL);
          symbol *end = newiTempLabel (NULL);
          emitLabel (copy);
          emit2 ("ld a, !*hl");
          emit2 ("inc hl");
          emit2 ("ld !*iyx, a", 0);
          emit2 ("inc iy");
          emit2 ("or a, a");
          emit2 ("jp Z, !tlabel", labelKey2num (padc->key));
          emit2 ("dec ix");
          emit2 ("jp NZ, !tlabel", labelKey2num (copy->key));
          emit2 ("jp !tlabel", labelKey2num (end->key));
          emitLabel (padc);
          emit2 ("dec ix");                                    /* account the NUL store */
          emit2 ("jp Z, !tlabel", labelKey2num (end->key));
          emitLabel (pad);
          emit2 ("ld !*iyx, a", 0);
          emit2 ("inc iy");
          emit2 ("dec ix");
          emit2 ("jp NZ, !tlabel", labelKey2num (pad->key));
          emitLabel (end);
        }
      regalloc_dry_run_cost += 26;
      _pop (PAIR_IX);
    }

  spillPair (PAIR_HL);
  spillPair (PAIR_IY);

  if (saved_B)
    {
      emit2 ("pop b");
      cost2 (1, 10);
      _G.stack.pushed -= 1;
    }
  if (saved_IY)
    _pop (PAIR_IY);
  if (saved_HL)
    _pop (PAIR_HL);

done:
  freeAsmop (n, NULL);
  freeAsmop (s2, NULL);
  freeAsmop (s1, NULL);
}

static void
genBuiltInStrchr (const iCode *ic, int nParams, operand **pparams)
{
  operand *s, *c;
  bool saved_HL = false, saved_B = false;
  int i;
  bool SomethingReturned;
  bool direct_c;

  SomethingReturned = IS_ITEMP (IC_RESULT (ic)) && (OP_SYMBOL (IC_RESULT (ic))->nRegs || OP_SYMBOL (IC_RESULT (ic))->spildir) ||
                      IS_TRUE_SYMOP (IC_RESULT (ic));

  wassertl (nParams == 2, "Built-in strchr() must have two parameters.");

  s = pparams[0];
  c = pparams[1];

  for (i = 0; i < nParams; i++)
    aopOp (pparams[i], ic, FALSE, FALSE);

  if (SomethingReturned)
    aopOp (IC_RESULT (ic), ic, true, false);

  if (!isRegDead (A_IDX, ic))
    UNIMPLEMENTED;

  /* HL is the scan pointer ((hl) is the only byte deref); the sought char is
     compared as `cp a, #nn` (literal) or `cp a, b` (B is the only register
     cp source besides A itself). */
  direct_c = (c->aop->type == AOP_LIT || aopInReg (c->aop, 0, B_IDX));

  if (!isPairDead (PAIR_HL, ic))
    {
      _push (PAIR_HL);
      saved_HL = true;
    }
  if (!direct_c && !isRegDead (B_IDX, ic))
    {
      emit2 ("push b");
      cost2 (1, 11);
      _G.stack.pushed += 1;
      saved_B = true;
    }

  if (!direct_c)
    cheapMove (ASMOP_B, 0, c->aop, 0, true);
  fetchPair (PAIR_HL, s->aop);

  /* The compare runs before `inc hl`, so the found-pointer is exact; the
     16-bit inc clobbers Z, so the NUL test (`or a, a`) runs after it
     (A is untouched by the inc).  c == '\0' correctly matches the
     terminator via the first compare. */
  if (!regalloc_dry_run)
    {
      symbol *tlbl1 = newiTempLabel (NULL);  /* found / out */
      symbol *tlbl2 = newiTempLabel (NULL);  /* loop */
      emitLabel (tlbl2);
      emit2 ("ld a, !*hl");
      if (direct_c && c->aop->type == AOP_LIT)
        emit2 ("cp a, %s", aopGet (c->aop, 0, FALSE));
      else
        emit2 ("cp a, b");
      emit2 ("jp Z, !tlabel", labelKey2num (tlbl1->key));
      emit2 ("inc hl");
      emit2 ("or a, a");
      emit2 ("jp NZ, !tlabel", labelKey2num (tlbl2->key));
      emit2 ("ld hl, !immedword", 0u);       /* NUL hit: NULL */
      emitLabel (tlbl1);
    }
  regalloc_dry_run_cost += 11;
  spillPair (PAIR_HL);

  if (SomethingReturned)
    genMove (IC_RESULT (ic)->aop, ASMOP_HL, true, true, true);

  if (saved_B)
    {
      emit2 ("pop b");
      cost2 (1, 10);
      _G.stack.pushed -= 1;
    }
  if (saved_HL)
    {
      if (SomethingReturned && (aopInReg (IC_RESULT (ic)->aop, 0, L_IDX) || aopInReg (IC_RESULT (ic)->aop, 0, H_IDX) ||
                                aopInReg (IC_RESULT (ic)->aop, 1, L_IDX) || aopInReg (IC_RESULT (ic)->aop, 1, H_IDX)))
        UNIMPLEMENTED; /* cannot both restore HL and return the result in it */
      else
        _pop (PAIR_HL);
    }

  if (SomethingReturned)
    freeAsmop (IC_RESULT (ic), NULL);
  freeAsmop (c, NULL);
  freeAsmop (s, NULL);
}

/*-----------------------------------------------------------------*/
/* genBuiltIn - calls the appropriate function to generate code    */
/* for a built in function                                         */
/*-----------------------------------------------------------------*/
static void
genBuiltIn (iCode *ic)
{
  operand *bi_parms[MAX_BUILTIN_ARGS];
  int nbi_parms;
  iCode *bi_iCode;
  symbol *bif;

  /* get all the arguments for a built in function */
  bi_iCode = getBuiltinParms (ic, &nbi_parms, bi_parms);

  /* which function is it */
  bif = OP_SYMBOL (IC_LEFT (bi_iCode));

  wassertl (!ic->prev || ic->prev->op != SEND || !ic->prev->builtinSEND, "genBuiltIn() must be called on first SEND icode only.");

  if (!strcmp (bif->name, "__builtin_memcpy"))
    {
      genBuiltInMemcpy (bi_iCode, nbi_parms, bi_parms);
    }
  else if (!strcmp (bif->name, "__builtin_strcpy"))
    {
      genBuiltInStrcpy (bi_iCode, nbi_parms, bi_parms);
    }
  else if (!strcmp (bif->name, "__builtin_strncpy"))
    {
      genBuiltInStrncpy (bi_iCode, nbi_parms, bi_parms);
    }
  else if (!strcmp (bif->name, "__builtin_strchr"))
    {
      genBuiltInStrchr (bi_iCode, nbi_parms, bi_parms);
    }
  else if (!strcmp (bif->name, "__builtin_memset"))
    {
      genBuiltInMemset (bi_iCode, nbi_parms, bi_parms);
    }
  else
    {
      wassertl (0, "Unknown builtin function encountered");
    }
}

/*-------------------------------------------------------------------------------------*/
/* genS1C88iCode - generate S1C88 code for a single iCode instruction           */
/*-------------------------------------------------------------------------------------*/
static void
genS1C88iCode (iCode * ic)
{
  genLine.lineElement.ic = ic;

  /* if the result is marked as
     spilt and rematerializable or code for
     this has already been generated then
     do nothing */
  if (resultRemat (ic))
    {
      emitDebug ("; skipping iCode since result will be rematerialized");
      return;
    }

  if (ic->generated)
    {
      emitDebug ("; skipping generated iCode");
      return;
    }

  /* depending on the operation */
  switch (ic->op)
    {
    case '!':
      emitDebug ("; genNot");
      genNot (ic);
      break;

    case '~':
      emitDebug ("; genCpl");
      genCpl (ic);
      break;

    case UNARYMINUS:
      emitDebug ("; genUminus");
      genUminus (ic);
      break;

    case IPUSH:
      emitDebug ("; genIpush");
      genIpush (ic);
      break;

    case IPUSH_VALUE_AT_ADDRESS:
      emitDebug ("; genPointerPush");
      genPointerPush (ic);
      break;

    case CALL:
    case PCALL:
      emitDebug ("; genCall");
      genCall (ic);
      break;

    case FUNCTION:
      emitDebug ("; genFunction");
      genFunction (ic);
      break;

    case ENDFUNCTION:
      emitDebug ("; genEndFunction");
      genEndFunction (ic);
      break;

    case RETURN:
      emitDebug ("; genRet");
      genRet (ic);
      break;

    case LABEL:
      emitDebug ("; genLabel");
      genLabel (ic);
      break;

    case GOTO:
      emitDebug ("; genGoto");
      genGoto (ic);
      break;

    case '+':
      emitDebug ("; genPlus");
      genPlus (ic);
      break;

    case '-':
      emitDebug ("; genMinus");
      genMinus (ic, ic->next->op == IFX ? ic->next : 0);
      break;

    case '*':
      emitDebug ("; genMult");
      genMult (ic);
      break;

    case '/':
      emitDebug ("; genDiv");
      genDiv (ic);
      break;

    case '%':
      emitDebug ("; genMod");
      genMod (ic);
      break;

    case '>':
      emitDebug ("; genCmpGt");
      genCmpGt (ic, ifxForOp (IC_RESULT (ic), ic));
      break;

    case '<':
      emitDebug ("; genCmpLt");
      genCmpLt (ic, ifxForOp (IC_RESULT (ic), ic));
      break;

    case LE_OP:
    case GE_OP:
    case NE_OP:

      /* note these two are xlated by algebraic equivalence
         during parsing SDCC.y */
      werror (E_INTERNAL_ERROR, __FILE__, __LINE__, "got '>=' or '<=' shouldn't have come here");
      break;

    case EQ_OP:
      emitDebug ("; genCmpEq");
      genCmpEq (ic, ifxForOp (IC_RESULT (ic), ic));
      break;

    case AND_OP:
      emitDebug ("; genAndOp");
      genAndOp (ic);
      break;

    case OR_OP:
      emitDebug ("; genOrOp");
      genOrOp (ic);
      break;

    case '^':
      emitDebug ("; genXor");
      genXor (ic, ifxForOp (IC_RESULT (ic), ic));
      break;

    case '|':
      emitDebug ("; genOr");
      genOr (ic, ifxForOp (IC_RESULT (ic), ic));
      break;

    case BITWISEAND:
      emitDebug ("; genAnd");
      genAnd (ic, ifxForOp (IC_RESULT (ic), ic));
      break;

    case INLINEASM:
      emitDebug ("; genInline");
      genInline (ic);
      break;

    case GETABIT:
      emitDebug ("; genGetAbit");
      genGetAbit (ic);
      break;

    case GETBYTE:
      emitDebug ("; genGetByte");
      genGetByte (ic);
      break;

    case GETWORD:
      emitDebug ("; genGetWord");
      genGetWord (ic);
      break;

    case ROT:
      emitDebug ("; genRot");
      genRot (ic);
      break;

    case LEFT_OP:
      emitDebug ("; genLeftShift");
      genLeftShift (ic);
      break;

    case RIGHT_OP:
      emitDebug ("; genRightShift");
      genRightShift (ic);
      break;

    case GET_VALUE_AT_ADDRESS:
      emitDebug ("; genPointerGet");
      genPointerGet (ic);
      break;

    case '=':

      if (POINTER_SET (ic))
        {
          emitDebug ("; genPointerSet");
          genPointerSet (ic);
        }
      else
        {
          emitDebug ("; genAssign");
          genAssign (ic);
        }
      break;

    case IFX:
      emitDebug ("; genIfx");
      genIfx (ic, NULL);
      break;

    case ADDRESS_OF:
      emitDebug ("; genAddrOf");
      genAddrOf (ic);
      break;

    case JUMPTABLE:
      emitDebug ("; genJumpTab");
      genJumpTab (ic);
      break;

    case CAST:
      emitDebug ("; genCast");
      genCast (ic);
      break;

    case RECEIVE:
      emitDebug ("; genReceive");
      genReceive (ic);
      break;

    case SEND:
      if (ic->builtinSEND)
        {
          emitDebug ("; genBuiltIn");
          genBuiltIn (ic);
        }
      else
        {
          emitDebug ("; genSend");
          genSend (ic);
        }
      break;

    case ARRAYINIT:
      wassertl (0, "ARRAYINIT is never generated (arrayInitializerSuppported = FALSE)");
      break;

    case DUMMY_READ_VOLATILE:
      emitDebug ("; genDummyRead");
      genDummyRead (ic);
      break;

    case CRITICAL:
      emitDebug ("; genCritical");
      genCritical (ic);
      break;

    case ENDCRITICAL:
      emitDebug ("; genEndCritical");
      genEndCritical (ic);
      break;

    default:
      wassertl (0, "Unknown iCode");
    }
}

float
dryS1C88iCode (iCode * ic)
{
  regalloc_dry_run = true;
  regalloc_dry_run_cost = 0;
  regalloc_dry_run_cost_bytes = 0;
  regalloc_dry_run_cost_states = 0;

  initGenLineElement ();
  _G.omitFramePtr = s1c88_should_omit_frame_ptr;

  genS1C88iCode (ic);

  destroy_line_list ();
  freeTrace (&_G.trace.aops);

  {
    int pairId;
    for (pairId = 0; pairId < NUM_PAIRS; pairId++)
      spillPair (pairId);
  }

  const unsigned int state_cost_divider = 8u << (optimize.codeSize * 3 + !optimize.codeSpeed * 3);

  // Hack, since the legacy regalloc_dry_run_cost is still used in some places.
  regalloc_dry_run_cost_bytes += regalloc_dry_run_cost;
  regalloc_dry_run_cost_states += regalloc_dry_run_cost * 4; // Assume 4 states per byte.

  // Compensate for typically lower state count of some targets
  

  return (regalloc_dry_run_cost_bytes + regalloc_dry_run_cost_states * ic->count / state_cost_divider);
}

#ifdef DEBUG_DRY_COST
static void
dryS1C88Code (iCode * lic)
{
  iCode *ic;

  for (ic = lic; ic; ic = ic->next)
    if (ic->op != FUNCTION && ic->op != ENDFUNCTION && ic->op != LABEL && ic->op != GOTO && ic->op != INLINEASM)
      {
        printf ("; iCode %d total cost: %f ", ic->key, dryS1C88iCode (ic));
        const unsigned int state_cost_divider = 8u << (optimize.codeSize * 3 + !optimize.codeSpeed * 3);
        printf ("(%f + %f * %f * 0.0001 / %u\n", (float)regalloc_dry_run_cost_bytes, regalloc_dry_run_cost_states, ic->count, state_cost_divider);
      }
}
#endif

/*-------------------------------------------------------------------------------------*/
/* genS1C88Code - generate S1C88 code for a block of instructions                        */
/*-------------------------------------------------------------------------------------*/
void
genS1C88Code (iCode * lic)
{
#ifdef DEBUG_DRY_COST
  dryS1C88Code (lic);
#endif

  iCode *ic;
  int cln = 0;
  regalloc_dry_run = false;

  initGenLineElement ();

  memset(s1c88_regs_used_as_parms_in_calls_from_current_function, 0, sizeof(bool) * (IYH_IDX + 1));
  s1c88_symmParm_in_calls_from_current_function = TRUE;
  memset(s1c88_regs_preserved_in_calls_from_current_function, 0, sizeof(bool) * (IYH_IDX + 1));

  /* if debug information required */
  if (options.debug && currFunc)
    {
      debugFile->writeFunction (currFunc, lic);
    }

  /* Generate Code for all instructions */
  for (ic = lic; ic; ic = ic->next)
    {
      if (ic->lineno && cln != ic->lineno)
        {
          if (options.debug)
            debugFile->writeCLine (ic);
          if (!options.noCcodeInAsm)
            emit2 (";%s:%d: %s", ic->filename, ic->lineno, printCLine (ic->filename, ic->lineno));
          cln = ic->lineno;
        }
      if (options.iCodeInAsm)
        {
          const char *iLine = printILine (ic);
          emit2 (";ic:%d: %s", ic->key, iLine);
          dbuf_free (iLine);
        }
      //regalloc_dry_run_cost = 0;
      regalloc_dry_run_cost_bytes = 0;
      regalloc_dry_run_cost_states = 0;
      genS1C88iCode (ic);

#if 0 // Helpful to debug "Unbalanced stack" errors.
      printf("After ic %d (op %d): _G.stack.pushed: %d\n", ic->key, ic->op, _G.stack.pushed);
#endif

#ifdef DEBUG_DRY_COST
      emit2 ("; iCode %d (count %f) total costs: %u %lu %f\n", ic->key, ic->count, regalloc_dry_run_cost, regalloc_dry_run_cost_bytes, regalloc_dry_run_cost_states);
#endif
    }

  /* now we are ready to call the
     peep hole optimizer */
  if (!options.nopeep)
    peepHole (&genLine.lineHead);

  /* This is unfortunate */
  /* now do the actual printing */
  {
    struct dbuf_s *buf = codeOutBuf;
    if (isInHome () && codeOutBuf == &code->oBuf)
      codeOutBuf = &home->oBuf;
    printLine (genLine.lineHead, codeOutBuf);
    if (_G.flushStatics)
      {
        flushStatics ();
        _G.flushStatics = 0;
      }
    codeOutBuf = buf;
  }

  {
    int pairId;
    for (pairId = 0; pairId < NUM_PAIRS; pairId++)
      spillPair (pairId);
  }

  destroy_line_list ();
  freeTrace (&_G.trace.aops);
}

// Check if what is returned by the curent function.
bool
s1c88IsReturned(const char *what)
{
  if (!strcmp(what, "iy"))
    return (s1c88IsReturned ("iyl") || s1c88IsReturned ("iyh"));

  const asmop *retaop = aopRet (currFunc->type);

  if (!retaop)
    return false;
  for (int i = 0; i < retaop->size; i++)
    if (!strcmp(retaop->aopu.aop_reg[i]->name, what))
      return true;
  return false;
}

// Check if what is part of the ith argument (counting from 1) to a function of type ftype.
// If what is 0, just check if the ith argument is in registers.
bool
s1c88IsRegArg(struct sym_link *ftype, int i, const char *what)
{
  if (what && !strcmp(what, "iy"))
    return (s1c88IsRegArg (ftype, i, "iyl") || s1c88IsRegArg (ftype, i, "iyh"));

  const asmop *argaop = aopArg (ftype, i);

  if (!argaop)
    return false;
    
  if (!what)
    return true;
    
  for (int i = 0; i < argaop->size; i++)
    if (!strcmp(argaop->aopu.aop_reg[i]->name, what))
      return true;

  return false; 
}

bool
s1c88IsParmInCall(sym_link *ftype, const char *what)
{
  const value *args;
  int i;

  for (i = 1, args = FUNC_ARGS (ftype); args; args = args->next, i++)
    if (s1c88IsRegArg(ftype, i, what))
      return true;
  return false;
}

