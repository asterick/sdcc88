/*-------------------------------------------------------------------------
  peep.c - source file for peephole optimizer helper functions

  Written By - Philipp Klaus Krause
  Copyright (C) 2020, Sebastian 'basxto' Riedel <sdcc@basxto.de>

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

  In other words, you are welcome to use, share and improve this program.
  You are forbidden to forbid anyone else to use, share and improve
  what you give them.   Help stamp out software-hoarding!
-------------------------------------------------------------------------*/

#include "common.h"
#include "SDCCicode.h"
#include "s1c88.h"
#include "SDCCglobl.h"
#include "SDCCpeeph.h"
#include "gen.h"

#define NOTUSEDERROR() do {werror(E_INTERNAL_ERROR, __FILE__, __LINE__, "error in notUsed()");} while(0)

#if 0
#define D(_s) { printf _s; fflush(stdout); }
#else
#define D(_s)
#endif

#define ISINST(l, i) (!STRNCASECMP((l), (i), sizeof(i) - 1) && (!(l)[sizeof(i) - 1] || isspace((unsigned char)((l)[sizeof(i) - 1]))))

typedef enum
{
  S4O_CONDJMP,
  S4O_WR_OP,
  S4O_RD_OP,
  S4O_TERM,
  S4O_VISITED,
  S4O_ABORT,
  S4O_CONTINUE
} S4O_RET;

static struct
{
  lineNode *head;
} _G;

extern bool s1c88_regs_used_as_parms_in_calls_from_current_function[IYH_IDX + 1];
extern bool s1c88_symmParm_in_calls_from_current_function;
extern bool s1c88_regs_preserved_in_calls_from_current_function[IYH_IDX + 1];

/*-----------------------------------------------------------------*/
/* univisitLines - clear "visited" flag in all lines               */
/*-----------------------------------------------------------------*/
static void
unvisitLines (lineNode *pl)
{
  for (; pl; pl = pl->next)
    pl->visited = FALSE;
}

#define AOP(op) op->aop
#define AOP_SIZE(op) AOP(op)->size

/*-----------------------------------------------------------------*/
/* incLabelJmpToCount - increment counter "jmpToCount" in entry    */
/* of the list labelHash                                           */
/*-----------------------------------------------------------------*/
static bool
incLabelJmpToCount (const char *label)
{
  labelHashEntry *entry;

  entry = getLabelRef (label, _G.head);
  if (!entry)
    return FALSE;
  entry->jmpToCount++;
  return TRUE;
}

/*-----------------------------------------------------------------*/
/* findLabel -                                                     */
/* 1. extracts label in the opcode pl                              */
/* 2. increment "label jump-to count" in labelHash                 */
/* 3. search lineNode with label definition and return it          */
/*-----------------------------------------------------------------*/
static lineNode *
findLabel (const lineNode *pl)
{
  char *p;
  lineNode *cpl;

  /* 1. extract label in opcode */

  /* In each S1C88 jumping opcode the label is at the end of the opcode */
  p = strlen (pl->line) - 1 + pl->line;

  /* scan backward until ',' or '\t' */
  for (; p > pl->line; p--)
    if (*p == ',' || isspace(*p))
      break;

  /* sanity check */
  if (p == pl->line)
    {
      NOTUSEDERROR();
      return NULL;
    }

  /* skip ',' resp. '\t' */
  ++p;

  /* 2. increment "label jump-to count" */
  if (!incLabelJmpToCount (p))
    return NULL;

  /* 3. search lineNode with label definition and return it */
  for (cpl = _G.head; cpl; cpl = cpl->next)
    if (cpl->isLabel &&
      strncmp (p, cpl->line, strlen(p)) == 0 &&
      cpl->line[strlen(p)] == ':')
        return cpl;

  return NULL;
}

/* Check if reading arg implies reading what. */
static bool argCont(const char *arg, const char *what)
{
  wassert (arg);

  while(isspace (*arg) || *arg == ',')
    arg++;

  if (arg[0] == '#' || arg[0] == '_')
    return false;

  if (arg[0] == '(' && arg[1] && arg[2] && (arg[2] != ')' && arg[3] != ')'))
    return false;

  if(*arg == '(')
    arg++;

  if (arg[0] == '#' || arg[0] == '_')
    return false;
    
  // Get suitable end to avoid reading into further arguments.
  const char *end = strchr(arg, ',');
  if (!end)
    end = arg + strlen(arg);

  const char *found = StrStr(arg, what);

  return(found && found < end);
}

static bool
s1c88MightBeParmInCallFromCurrentFunction(const char *what)
{
  if (strchr(what, 'l') && s1c88_regs_used_as_parms_in_calls_from_current_function[L_IDX])
    return TRUE;
  if (strchr(what, 'h') && s1c88_regs_used_as_parms_in_calls_from_current_function[H_IDX])
    return TRUE;
  if (strchr(what, 'b') && s1c88_regs_used_as_parms_in_calls_from_current_function[B_IDX])
    return TRUE;
  if (strchr(what, 'a') && s1c88_regs_used_as_parms_in_calls_from_current_function[A_IDX])
    return true;
  if (strstr(what, "iy") && (s1c88_regs_used_as_parms_in_calls_from_current_function[IYL_IDX] || s1c88_regs_used_as_parms_in_calls_from_current_function[IYH_IDX]))
    return true;

  return false;
}

/* Check if the flag implies reading what. */
static bool
s1c88MightReadFlagCondition(const char *cond, const char *what)
{
  while(isspace (*cond))
    cond++;

  /* S1C88 v/nv test the overflow flag V (token "vf"; replaces the z80 po/pe). */
  if(tolower(cond[0]) == 'v' || !STRNCASECMP(cond, "nv", 2))
    return !strcmp(what, "vf");
  if(tolower(cond[0]) == 'm' || tolower(cond[0]) == 'p')
    return !strcmp(what, "nf");

  // skip inverted conditions
  if(tolower(cond[0]) == 'n')
    cond++;

  if(tolower(cond[0]) == 'c')
    return !strcmp(what, "cf");
  if(tolower(cond[0]) == 'z')
    return !strcmp(what, "zf");
  return true;
}

/* Does the instruction name the SC flag register as an operand?  `ld a, sc`,
   `push sc` and `and/or/xor sc, #nn` READ the whole flag byte; `ld sc, a` and
   `pop sc` reload it wholesale. */
static bool
s1c88NamesSC(const lineNode *pl, bool as_source)
{
  const char *comma = strchr(pl->line, ',');
  if(ISINST(pl->line, "push"))
    return as_source && argCont(pl->line + 4, "sc");
  if(ISINST(pl->line, "pop"))
    return !as_source && argCont(pl->line + 3, "sc");
  if(ISINST(pl->line, "ld"))
    {
      if(!comma)
        return false;
      return as_source ? argCont(comma + 1, "sc") : (!strncmp(pl->line + 3, "sc", 2));
    }
  if(ISINST(pl->line, "and") || ISINST(pl->line, "or") || ISINST(pl->line, "xor"))
    {
      const char *arg = pl->line + (ISINST(pl->line, "or") ? 2 : 3);
      while(isspace(*arg))
        arg++;
      return !strncmp(arg, "sc", 2);
    }
  return false;
}

static bool
s1c88MightReadFlag(const lineNode *pl, const char *what)
{
  if(s1c88NamesSC(pl, true))   // ld a, sc / push sc / and|or|xor sc, #nn read every flag
    return true;

  if(ISINST(pl->line, "ld") ||
     ISINST(pl->line, "or") ||
     ISINST(pl->line, "cp") ||
     ISINST(pl->line, "di") ||
     ISINST(pl->line, "ei") ||
     ISINST(pl->line, "im") ||
     ISINST(pl->line, "in"))
    return false;
  if(ISINST(pl->line, "nop") ||
     ISINST(pl->line, "add") ||
     ISINST(pl->line, "sub") ||
     ISINST(pl->line, "and") ||
     ISINST(pl->line, "xor") ||
     ISINST(pl->line, "dec") ||
     ISINST(pl->line, "inc") ||
     ISINST(pl->line, "cpl") ||
     ISINST(pl->line, "bit") ||
     ISINST(pl->line, "res") ||
     ISINST(pl->line, "set") ||
     ISINST(pl->line, "pop") ||
     ISINST(pl->line, "rlc") ||
     ISINST(pl->line, "rrc") ||
     ISINST(pl->line, "sla") ||
     ISINST(pl->line, "sra") ||
     ISINST(pl->line, "srl") ||
     ISINST(pl->line, "scf") ||
     ISINST(pl->line, "cpd") ||
     ISINST(pl->line, "cpi") ||
     ISINST(pl->line, "ind") ||
     ISINST(pl->line, "ini") ||
     ISINST(pl->line, "ldd") ||
     ISINST(pl->line, "ldi") ||
     ISINST(pl->line, "neg") ||
     ISINST(pl->line, "rld") ||
     ISINST(pl->line, "rrd") ||
     ISINST(pl->line, "mlt") ||
     ISINST(pl->line, "div") ||
     ISINST(pl->line, "out"))
    return false;
  if(ISINST(pl->line, "halt") ||
     ISINST(pl->line, "rlca") ||
     ISINST(pl->line, "rrca") ||
     ISINST(pl->line, "cpdr") ||
     ISINST(pl->line, "cpir") ||
     ISINST(pl->line, "indr") ||
     ISINST(pl->line, "inir") ||
     ISINST(pl->line, "lddr") ||
     ISINST(pl->line, "ldir") ||
     ISINST(pl->line, "outd") ||
     ISINST(pl->line, "outi") ||
     ISINST(pl->line, "djnz"))
    return false;

  

  

  

  

  if(ISINST(pl->line, "rl") ||
     ISINST(pl->line, "rr") ||
     ISINST(pl->line, "rla") ||
     ISINST(pl->line, "rra") ||
     ISINST(pl->line, "sbc") ||
     ISINST(pl->line, "adc") ||
     ISINST(pl->line, "ccf"))
    return (!strcmp(what, "cf"));

  if(ISINST(pl->line, "push"))
    return (argCont(pl->line + 4, "af"));

  if(ISINST(pl->line, "ex"))
    return (argCont(pl->line + 2, "af"));

  // catch c, nc, z, nz, po, pe, p and m
  if(ISINST(pl->line, "jp") ||
     ISINST(pl->line, "jr"))
    return (strchr(pl->line, ',') && s1c88MightReadFlagCondition(pl->line + 2, what));

  // flags don't matter according to calling convention
  if(ISINST(pl->line, "reti") ||
     ISINST(pl->line, "retn"))
    return false;

  if(ISINST(pl->line, "call"))
    return (strchr(pl->line, ',') && s1c88MightReadFlagCondition(pl->line + 4, what));

  if(ISINST(pl->line, "ret"))
    return (pl->line[3] == '\t' && s1c88MightReadFlagCondition(pl->line + 3, what));

  // we don't know anything about this
  if(ISINST(pl->line, "rst"))
    return true;

  
    
  

  

  
 
  return true; // Fail-safe: we have no idea what happens at this line, so assume it might read anything.
}

static bool
s1c88MightRead(const lineNode *pl, const char *what)
{
  if(strcmp(what, "iyl") == 0 || strcmp(what, "iyh") == 0)
    what = "iy";
  if(strcmp(what, "ixl") == 0 || strcmp(what, "ixh") == 0)
    what = "ix";

  if(ISINST(pl->line, "call") && strcmp(what, "sp") == 0)
    return TRUE;

  if(strcmp(pl->line, "call\t__initrleblock") == 0 && (strchr(what, 'd') != 0 || strchr(what, 'e') != 0))
    return TRUE;

  if(strcmp(pl->line, "call\t___sdcc_call_hl") == 0 && (strchr(what, 'h') != 0 || strchr(what, 'l') != 0))
    return TRUE;

  if(strcmp(pl->line, "call\t___sdcc_call_iy") == 0 && strstr(what, "iy") != 0)
    return TRUE;

  if(strncmp(pl->line, "call\t___sdcc_bcall_", 19) == 0)
    if (strchr (what, pl->line[19]) != 0 || strchr (what, pl->line[20]) != 0 || strchr (what, pl->line[21]) != 0)
      return TRUE;

  if(ISINST(pl->line, "call") && strchr(pl->line, ',') == 0)
    {
      const symbol *f = findSym (SymbolTab, 0, pl->line + 6);
      if (f && IS_FUNC (f->type))
        return s1c88IsParmInCall(f->type, what);
      else // Fallback needed for calls through function pointers and for calls to literal addresses.
        return s1c88MightBeParmInCallFromCurrentFunction(what);
    }

  if(ISINST(pl->line, "reti") || ISINST(pl->line, "retn"))
    return(strcmp(what, "sp") == 0);

  if(ISINST(pl->line, "ret")) // S1C88: a ;pcall-tagged ret IS a function-pointer CALL (RET-dispatch) — its arguments are read by the callee
    return s1c88IsReturned(what) || (strstr(pl->line, ";pcall") && s1c88MightBeParmInCallFromCurrentFunction(what)) || strcmp(what, "sp") == 0;

  if(!strcmp(pl->line, "ex\t(sp), hl") || !strcmp(pl->line, "ex\t(sp),hl"))
    return(!strcmp(what, "h") || !strcmp(what, "l") || strcmp(what, "sp") == 0);
  if(!strcmp(pl->line, "ex\t(sp), ix") || !strcmp(pl->line, "ex\t(sp),ix"))
    return(!!strstr(what, "ix") || strcmp(what, "sp") == 0);
  if(!strcmp(pl->line, "ex\t(sp), iy") || !strcmp(pl->line, "ex\t(sp),iy"))
    return(!!strstr(what, "iy") || strcmp(what, "sp") == 0);
  if(!strcmp(pl->line, "ex\tde, hl") || !strcmp(pl->line, "ex\tde,hl"))
    return(!strcmp(what, "h") || !strcmp(what, "l") || !strcmp(what, "d") || !strcmp(what, "e"));
  if(ISINST(pl->line, "ld"))
    {
      if(argCont(strchr(pl->line, ','), what))
        return(true);
      if(*(strchr(pl->line, ',') - 1) == ')' && strstr(pl->line + 3, what) &&
        (strchr(pl->line, '#') == 0 || strchr(pl->line, '#') > strchr(pl->line, ',')) &&
        (strchr(pl->line, '_') == 0 || strchr(pl->line, '_') > strchr(pl->line, ',')))
        return(true);
      if (!strcmp(what, "sp") && strchr(pl->line, '(')) // Assume any indirect memory access to be a stack access. This avoids optimizing out stackframe setups for local variables (bug #3173).
        return(true);
      return(false);
    }

  //ld a, #0x00
  if((ISINST(pl->line, "xor") || ISINST(pl->line, "sub")) &&
     (!strcmp(pl->line+4, "a, a") || !strcmp(pl->line+4, "a,a") || (!strchr(pl->line, ',') && !strcmp(pl->line+4, "a"))))
    return(false);

  //ld a, #0x00
  if(!strcmp(pl->line, "and\ta, #0x00") || !strcmp(pl->line, "and\ta,#0x00") || !strcmp(pl->line, "and\t#0x00"))
    return(false);

  //ld a, #0xff
  if(!strcmp(pl->line, "or\ta, #0xff") || !strcmp(pl->line, "or\ta,#0xff") || !strcmp(pl->line, "or\t#0xff"))
    return(false);

  if (ISINST(pl->line, "adc") || ISINST(pl->line, "add") || ISINST(pl->line, "and") || ISINST(pl->line, "sbc") || ISINST(pl->line, "sub") || ISINST(pl->line, "xor"))
    {
      const char *arg;
      if (ISINST(pl->line, "multu"))
        arg = pl->line + 5;
      else if (ISINST(pl->line, "multuw"))
        arg = pl->line + 6;
      else
        arg = pl->line + 4;
      while(isspace(*arg))
        arg++;
      if(arg[0] == 'a' && arg[1] == ',')
        {
          if(!strcmp(what, "a"))
            return(true);
          arg += 2;
        }
      else if(!strncmp(arg, "hl", 2) && arg[2] == ',') // add hl, rr
        {
          if(!strcmp(what, "h") || !strcmp(what, "l"))
            return(true);
          arg += 3;
        }
      else if(!strncmp(arg, "ba", 2) && arg[2] == ',') // S1C88 16-bit `add/adc/sub/sbc ba, rr`
        {
          // argCont stops at the comma, so without this arm `sub ba, hl` was
          // scanned as "ba" only: HL was deemed unread and the load feeding it
          // deleted (`x - g` compiled to a bare `sub ba, hl`).
          if(!strcmp(what, "a") || !strcmp(what, "b"))
            return(true);
          arg += 3;
        }
      else if(!strncmp(arg, "sc", 2) && arg[2] == ',') // and/or/xor sc, #imm: flags only
        return(false);
      else if(!strncmp(arg, "sp", 2) && arg[2] == ',') // add sp, rr
        {
          if(!strcmp(what, "sp"))
            return(true);
          arg += 3;
        }
      else if(arg[0] == 'i') // add ix/y, rr
        {
          if(!strncmp(arg, what, 2))
            return(true);
          arg += 3;
        }
      return(argCont(arg, what));
    }

  if(ISINST(pl->line, "or") || ISINST(pl->line, "cp") )
    {
      const char *arg = pl->line + 3;
      while(isspace(*arg))
        arg++;
      if(*arg == 'a' && *(arg + 1) == ',')
        {
          if(!strcmp(what, "a"))
            return(true);
          arg += 2;
        }
      else if(!strncmp(arg, "hl", 2) && *(arg + 2) == ',')
        {
          if(!strcmp(what, "h") || !strcmp(what, "l"))
            return(true);
          arg += 3;
        }
      else if(!strncmp(arg, "ba", 2) && *(arg + 2) == ',') // S1C88 native 16-bit pair compare `cp ba, rr`
        {
          if(!strcmp(what, "a") || !strcmp(what, "b"))
            return(true);
          arg += 3;
        }
      else if(!strncmp(arg, "iy", 2) && *(arg + 2) == ',')
        {
          if(!strcmp(what, "iy"))
            return(true);
          arg += 3;
        }
      // argCont stops at the comma, so the first operand must be consumed above
      // for the SECOND operand to be scanned (e.g. `cp ba, hl` reads hl).
      return(argCont(arg, what));
    }

  if(ISINST(pl->line, "neg"))
    return(strcmp(what, "a") == 0);

  if(ISINST(pl->line, "pop"))
    return(strcmp(what, "sp") == 0);

  if(ISINST(pl->line, "push"))
    return(strstr(pl->line + 5, what) != 0 || strcmp(what, "sp") == 0);

  if(ISINST(pl->line, "dec") ||
     ISINST(pl->line, "inc"))
    {
      return(argCont(pl->line + 4, what));
    }

  if(ISINST(pl->line, "cpl"))
    return(!strcmp(what, "a"));

  if(ISINST(pl->line, "di") || ISINST(pl->line, "ei"))
    return(false);

  // Rotate and shift group
  if(ISINST(pl->line, "rlca") ||
     ISINST(pl->line, "rla")  ||
     ISINST(pl->line, "rrca") ||
     ISINST(pl->line, "rra")  ||
     ISINST(pl->line, "daa"))
    {
      return(strcmp(what, "a") == 0);
    }
  if(ISINST(pl->line, "rl") ||
     ISINST(pl->line, "rr"))
    {
      return(argCont(pl->line + 3, what));
    }
  if(ISINST(pl->line, "rlc") ||
     ISINST(pl->line, "sla") ||
     ISINST(pl->line, "rrc") ||
     ISINST(pl->line, "sra") ||
     ISINST(pl->line, "srl"))
    {
      return(argCont(pl->line + 4, what));
    }
  
  if((ISINST(pl->line, "rld") ||
     ISINST(pl->line, "rrd")))
    return(!!strstr("ahl", what));

  // Bit test: the S1C88 operand order is `bit reg, #mask` — the REGISTER is
  // the FIRST operand (before the comma).
  // Scanning the wrong side made notUsed() think `bit a, #0x01` doesn't read
  // A, so peephole 7 deleted the A reload before it (caught by tests/emu 03,
  // a wrong quotient out of the C-compiled __divuint). argCont stops at the
  // comma. set/res don't exist on the S1C88 (bits are done via and/or a,#mask)
  // but keep them conservative should one ever appear.
  if(ISINST(pl->line, "bit") ||
     ISINST(pl->line, "set") ||
     ISINST(pl->line, "res"))
    {
      return(argCont(pl->line + 4, what));
    }

  if (ISINST(pl->line, "ccf") || ISINST(pl->line, "scf") || ISINST(pl->line, "nop") || ISINST(pl->line, "halt"))
    return(false);

  if(ISINST(pl->line, "jp") || ISINST(pl->line, "jr"))
    return(false);

  if(ISINST(pl->line, "djnz"))
    return(strchr(what, 'b') != 0);

  if((ISINST(pl->line, "ldd") || ISINST(pl->line, "lddr") || ISINST(pl->line, "ldi") || ISINST(pl->line, "ldir")))
    return(strchr("bcdehl", *what));
  

  if((ISINST(pl->line, "cpd") || ISINST(pl->line, "cpdr") || ISINST(pl->line, "cpi") || ISINST(pl->line, "cpir")))
    return(strchr("abchl", *what));

  if(ISINST(pl->line, "out"))
    return(strstr(strchr(pl->line + 4, ','), what) != 0 || strstr(pl->line + 4, "(c)") && (!strcmp(what, "b") || !strcmp(what, "c")));
  if(ISINST(pl->line, "in"))
    return(!strstr(strchr(pl->line + 4, ','), "(c)") && !strcmp(what, "a") || strstr(strchr(pl->line + 4, ','), "(c)") && (!strcmp(what, "b") || !strcmp(what, "c")));

  

  if((ISINST(pl->line, "ini") || ISINST(pl->line, "ind") || ISINST(pl->line, "inir") || ISINST(pl->line, "indr") ||
    ISINST(pl->line, "outi") || ISINST(pl->line, "outd") || ISINST(pl->line, "otir") || ISINST(pl->line, "otdr")))
    return(strchr("bchl", *what));

  

  

  

  

  

  

  
    
  

  
    
  if (ISINST(pl->line, "lidr") || ISINST(pl->line, "lsddr") || ISINST(pl->line, "lsidr"))
    return(strchr("bcdehl", *what));

  

  

  

  

  /* TODO: Can we know anything about rst? */
  if(ISINST(pl->line, "rst"))
    return(true);
    
  return(true);
}

static bool
s1c88UncondJump(const lineNode *pl)
{
  if((ISINST(pl->line, "jp") || ISINST(pl->line, "jr")) &&
     strchr(pl->line, ',') == 0)
    return TRUE;
  return FALSE;
}

static bool
s1c88CondJump(const lineNode *pl)
{
  if(((ISINST(pl->line, "jp") || ISINST(pl->line, "jr")) &&
      strchr(pl->line, ',') != 0) ||
     ISINST(pl->line, "djnz"))
    return TRUE;
  return FALSE;
}

// TODO: z80 flags only partly implemented
static bool
s1c88SurelyWritesFlag(const lineNode *pl, const char *what)
{
  /* LD instruction is never change flags except LD A,I and LD A,R.
    But it is most popular instruction so place it first */
  if(ISINST(pl->line, "ld"))
    {
      if(!!strcmp(what, "vf") ||
          !argCont(pl->line+3, "a"))
        return false;
      const char *p = strchr(pl->line+4, ',');
      if (p == NULL)
        return false; /* unknown instruction */
      ++p;
      return argCont(p, "i") || argCont(p, "r");
    }

  if(ISINST(pl->line, "rlca") ||
     ISINST(pl->line, "rrca") ||
     ISINST(pl->line, "rra")  ||
     ISINST(pl->line, "rla"))
    return (!!strcmp(what, "zf") && !!strcmp(what, "nf") && !!strcmp(what, "vf"));

  // `ld sc, a` / `pop sc` reload the whole flag byte; `and/or/xor sc, #nn`
  // rewrite it from its OLD value (a read-modify-write), so they surely write
  // nothing in the dead-flag sense.
  if(s1c88NamesSC(pl, false))
    return !(ISINST(pl->line, "and") || ISINST(pl->line, "or") || ISINST(pl->line, "xor"));

  // ISA (instruction-set.md "Flags by sub-class"): ADD/ADC/SUB/SBC/CP/NEG set
  // Z C V N; AND/OR/XOR and BIT set only Z N (C V unchanged); the rotates and
  // shifts set C Z N and leave V.
  if(ISINST(pl->line, "adc") ||
     ISINST(pl->line, "sbc") ||
     ISINST(pl->line, "sub") ||
     ISINST(pl->line, "cp") ||
     ISINST(pl->line, "neg"))
    return true;

  if(ISINST(pl->line, "and") ||
     ISINST(pl->line, "xor") ||
     ISINST(pl->line, "or") ||
     ISINST(pl->line, "bit"))
    return (!strcmp(what, "zf") || !strcmp(what, "nf"));

  if(ISINST(pl->line, "rlc") ||
     ISINST(pl->line, "rrc") ||
     ISINST(pl->line, "sla") ||
     ISINST(pl->line, "sll") ||
     ISINST(pl->line, "sra") ||
     ISINST(pl->line, "srl") ||
     ISINST(pl->line, "rl") ||
     ISINST(pl->line, "rr"))
    return (!!strcmp(what, "vf"));

  if(ISINST(pl->line, "cpd") ||
     ISINST(pl->line, "cpi") ||
     ISINST(pl->line, "ind") ||
     ISINST(pl->line, "ini") ||
     ISINST(pl->line, "rrd"))
    return (!!strcmp(what, "cf"));

  if(ISINST(pl->line, "cpdr") ||
     ISINST(pl->line, "cpir") ||
     ISINST(pl->line, "indr") ||
     ISINST(pl->line, "inir") ||
     ISINST(pl->line, "otdr") ||
     ISINST(pl->line, "otir") ||
     ISINST(pl->line, "outd") ||
     ISINST(pl->line, "outi"))
    return (!!strcmp(what, "cf"));

  if(ISINST(pl->line, "ccf") ||
    ISINST(pl->line, "scf"))
    return (!strcmp(what, "cf"));     /* S1C88 scf/ccf touch only C */

  if(ISINST(pl->line, "cpl"))
    return false;                     /* S1C88 cpl sets none of Z/C/V/N */

  if(ISINST(pl->line, "inc") || ISINST(pl->line, "dec"))
    {
      const char *arg = pl->line + 4;
      // 8-bit inc/dec affects all flags other than c.
      if (strlen(arg) == 1 || // 8-bit register
        !strcmp(arg, "(hl)") ||
        !strcmp(pl->line + 6, "(ix)") ||
        !strcmp(pl->line + 6, "(iy)"))
        return (!!strcmp(what, "cf"));
      // S1C88 (unlike z80) 16-bit inc/dec of a GPR pair sets Z V N, leaves C.
      // SP arithmetic is excluded (its flag effect is not relied upon and the
      // manual's per-flag SP cells are unverified) — assume no write there.
      if (!strcmp(arg, "ba") || !strcmp(arg, "hl") || !strcmp(arg, "ix") || !strcmp(arg, "iy"))
        return (!!strcmp(what, "cf"));
      return false; // inc/dec sp (or unknown): conservatively no flag write.
    }

  if(ISINST(pl->line, "add"))
    {
      const char *arg = pl->line + 4;
      while (isspace ((unsigned char) *arg))
        arg++;
      // 8-bit `add a, X` sets Z C V N; S1C88 16-bit `add hl,X`/`add ba,X` ALSO
      // set Z C V N (z80's add hl,rr sets only C — this is the divergence that
      // makes the post-add byte-combine zero-test redundant on the S1C88).
      if ((arg[0] == 'a' && arg[1] == ',') ||
          !strncmp(arg, "hl,", 3) || !strncmp(arg, "hl ,", 4) ||
          !strncmp(arg, "ba,", 3) || !strncmp(arg, "ba ,", 4))
        return true;
      // add sp,X (or unknown): conservatively don't claim Z/S/P.
      return (!!strcmp(what, "zf") && !!strcmp(what, "nf") && !!strcmp(what, "vf"));
    }

  if(ISINST(pl->line, "ldd") ||
    ISINST(pl->line, "lddr") ||
    ISINST(pl->line, "ldi") ||
    ISINST(pl->line, "ldir"))
    return (!strcmp(what, "vf"));

  // pop af writes
  if(ISINST(pl->line, "pop"))
    return (argCont(pl->line + 4, "af"));

  // according to calling convention caller has to save flags
  if(ISINST(pl->line, "ret") ||
     ISINST(pl->line, "call"))
    return true;

  if(ISINST(pl->line, "rld") ||
    ISINST(pl->line, "rrd"))
    return (!strcmp(what, "vf"));

  if(ISINST(pl->line, "djnz") ||
    ISINST(pl->line, "ex") ||
    ISINST(pl->line, "nop") ||
    ISINST(pl->line, "out") ||
    ISINST(pl->line, "push") ||
    ISINST(pl->line, "res") ||
    ISINST(pl->line, "set"))
    return false;

  

  

  /* handle IN0 r,(n) and IN r,(c) instructions */
  if(ISINST(pl->line, "in0") || (!strncmp(pl->line, "in\t", 3) &&
     (!strcmp(pl->line+5, "(c)") || !strcmp(pl->line+5, "(bc)"))))
    return (!!strcmp(what, "cf"));

  

  

  if(ISINST(pl->line, "mlt"))
    return (true);

  if(ISINST(pl->line, "div")) /* S1C88 DIV: Z, N set per quotient; C, V written too */
    return (true);

  

  return false; // Fail-safe: we have no idea what happens at this line, so assume it writes nothing.
}

static bool
callSurelyWrites (const lineNode *pl, const char *what)
{
  const symbol *f = 0;
  if (ISINST(pl->line, "call") && !strchr(pl->line, ','))
    f = findSym (SymbolTab, 0, pl->line + 6);
  else if ((ISINST(pl->line, "jp") || ISINST(pl->line, "jr")) && !strchr(pl->line, ','))
    f = findSym (SymbolTab, 0, pl->line + 4);

  const bool *preserved_regs;

  if (f && (strlen(what) == 2 && what[1] == 'f')) // Flags are never preserved across function calls.
    return(true);

  if(!strcmp(what, "ix"))
    return(false);

  if(f)
    preserved_regs = f->type->funcAttrs.preserved_regs;
  else if (ISINST(pl->line, "call"))
    preserved_regs = s1c88_regs_preserved_in_calls_from_current_function;
  else // Err on the safe side for jp and jr - might not be a function call, might e.g. be a jump table.
    return (false);

  if (!strcmp (what, "a"))
    return !preserved_regs[A_IDX];
  if (!strcmp (what, "b"))
    return !preserved_regs[B_IDX];
  if (!strcmp (what, "l"))
    return !preserved_regs[L_IDX];
  if (!strcmp (what, "h"))
    return !preserved_regs[H_IDX];
  if (!strcmp (what, "iyl"))
    return !preserved_regs[IYL_IDX];
  if (!strcmp (what, "iyh"))
    return !preserved_regs[IYH_IDX];
  if (!strcmp (what, "iy"))
    return !preserved_regs[IYL_IDX] && !preserved_regs[IYH_IDX];

  return (false);
}

static bool
s1c88SurelyWrites (const lineNode *pl, const char *what)
{
  if(strcmp(what, "iyl") == 0 || strcmp(what, "iyh") == 0)
    what = "iy";
  if(strcmp(what, "ixl") == 0 || strcmp(what, "ixh") == 0)
    what = "ix";

  //ld a, #0x00
  if((ISINST(pl->line, "xor") || ISINST(pl->line, "sub")) && !strcmp(what, "a") &&
     (!strcmp(pl->line+4, "a, a") || !strcmp(pl->line+4, "a,a") || (!strchr(pl->line, ',') && !strcmp(pl->line+4, "a"))))
    return(true);

  //ld a, #0x00
  if(!strcmp(what, "a") && (!strcmp(pl->line, "and\ta, #0x00") || !strcmp(pl->line, "and\ta,#0x00") || !strcmp(pl->line, "and\t#0x00")))
    return(true);

  //ld a, #0xff
  if(!strcmp(what, "a") && (!strcmp(pl->line, "or\ta, #0xff") || !strcmp(pl->line, "or\ta,#0xff") || !strcmp(pl->line, "or\t#0xff")))
    return(true);

  if(ISINST(pl->line, "ld") && strncmp(pl->line + 3, "hl", 2) == 0 && (what[0] == 'h' || what[0] == 'l'))
    return(true);
  if(ISINST(pl->line, "ld") && strncmp(pl->line + 3, "de", 2) == 0 && (what[0] == 'd' || what[0] == 'e'))
    return(true);
  if(ISINST(pl->line, "ld") && strncmp(pl->line + 3, "bc", 2) == 0 && (what[0] == 'b' || what[0] == 'c'))
    return(true);
  if((ISINST(pl->line, "ld") || ISINST(pl->line, "in"))
    && strncmp(pl->line + 3, what, strlen(what)) == 0 && pl->line[3 + strlen(what)] == ',')
    return(true);

  

  if(ISINST(pl->line, "pop") && strstr(pl->line + 4, what))
    return(true);
  if (ISINST(pl->line, "call") && strchr(pl->line, ',') == 0)
    return (callSurelyWrites (pl, what));

  if(strcmp(pl->line, "ret") == 0)
    return true;

  

  

  

  

  

  

  

  return(false);
}

static bool
s1c88SurelyReturns(const lineNode *pl)
{
  if(strcmp(pl->line, "ret") == 0)
    return TRUE;
  return FALSE;
}

/*-----------------------------------------------------------------*/
/* scan4op - "executes" and examines the assembler opcodes,        */
/* follows conditional and un-conditional jumps.                   */
/* Moreover it registers all passed labels.                        */
/*                                                                 */
/* Parameter:                                                      */
/*    lineNode **pl                                                */
/*       scanning starts from pl;                                  */
/*       pl also returns the last scanned line                     */
/*    const char *pReg                                             */
/*       points to a register (e.g. "ar0"). scan4op() tests for    */
/*       read or write operations with this register               */
/*    const char *untilOp                                          */
/*       points to NULL or a opcode (e.g. "push").                 */
/*       scan4op() returns if it hits this opcode.                 */
/*    lineNode **plCond                                            */
/*       If a conditional branch is met plCond points to the       */
/*       lineNode of the conditional branch                        */
/*                                                                 */
/* Returns:                                                        */
/*    S4O_ABORT                                                    */
/*       on error                                                  */
/*    S4O_VISITED                                                  */
/*       hit lineNode with "visited" flag set: scan4op() already   */
/*       scanned this opcode.                                      */
/*    S4O_FOUNDOPCODE                                              */
/*       found opcode and operand, to which untilOp and pReg are   */
/*       pointing to.                                              */
/*    S4O_RD_OP, S4O_WR_OP                                         */
/*       hit an opcode reading or writing from pReg                */
/*    S4O_CONDJMP                                                  */
/*       hit a conditional jump opcode. pl and plCond return the   */
/*       two possible branches.                                    */
/*    S4O_TERM                                                     */
/*       acall, lcall, ret and reti "terminate" a scan.            */
/*-----------------------------------------------------------------*/
static S4O_RET
scan4op (lineNode **pl, const char *what, const char *untilOp,
         lineNode **plCond)
{
  bool isFlag = (strlen(what) == 2 && what[1] == 'f');
  for (; *pl; *pl = (*pl)->next)
    {
      if (!(*pl)->line || (*pl)->isDebug || (*pl)->isComment || (*pl)->isLabel)
        continue;
      D(("Scanning %s for %s\n", (*pl)->line, what));
      /* don't optimize across inline assembler,
         e.g. isLabel doesn't work there */
      if ((*pl)->isInline)
        {
          D(("S4O_RD_OP: Inline asm\n"));
          return S4O_ABORT;
        }

      if ((*pl)->visited)
        {
          D(("S4O_VISITED\n"));
          return S4O_VISITED;
        }

      (*pl)->visited = TRUE;

      if(isFlag)
        {
          if(s1c88MightReadFlag(*pl, what))
            {
              D(("S4O_RD_OP (flag)\n"));
              return S4O_RD_OP;
            }
        }
      else
        {
          if (s1c88MightRead (*pl, what))
            {
              D (("S4O_RD_OP\n"));
              return S4O_RD_OP;
            }
        }

      if (s1c88UncondJump (*pl))
        {
          lineNode *tlbl = findLabel (*pl);
          if (!tlbl) // jp/jr could be a tail call.
            {
              const symbol *f = findSym (SymbolTab, 0, (*pl)->line + 4);
              if (f && s1c88IsParmInCall(f->type, what))
                {
                  D (("S4O_RD_OP\n"));
                  return S4O_RD_OP;
                }
              else if(callSurelyWrites (*pl, what))
                {
                  D (("S4O_WR_OP\n"));
                  return S4O_WR_OP;
                }
            }
          *pl = tlbl;
          if (!*pl)
            {
              D (("S4O_ABORT\n"));
              return S4O_ABORT;
            }
        }
      if (s1c88CondJump(*pl))
        {
          *plCond = findLabel (*pl);
          if (!*plCond)
            {
              D (("S4O_ABORT\n"));
              return S4O_ABORT;
            }
          D (("S4O_CONDJMP\n"));
          return S4O_CONDJMP;
        }

      if (isFlag)
        {
          if (s1c88SurelyWritesFlag (*pl, what))
            {
              D (("S4O_WR_OP (flag)\n"));
              return S4O_WR_OP;
            }
        }
      else
        {
        if(s1c88SurelyWrites(*pl, what))
          {
            D(("S4O_WR_OP\n"));
            return S4O_WR_OP;
          }
        }

      /* Don't need to check for de, hl since s1c88MightRead() does that */
      if(s1c88SurelyReturns(*pl))
        {
          D(("S4O_TERM\n"));
          return S4O_TERM;
        }
    }
  D(("S4O_ABORT\n"));
  return S4O_ABORT;
}

/*-----------------------------------------------------------------*/
/* doTermScan - scan through area 2. This small wrapper handles:   */
/* - action required on different return values                    */
/* - recursion in case of conditional branches                     */
/*-----------------------------------------------------------------*/
static bool
doTermScan (lineNode **pl, const char *what)
{
  lineNode *plConditional;

  for (;; *pl = (*pl)->next)
    {
      switch (scan4op (pl, what, NULL, &plConditional))
        {
          case S4O_TERM:
          case S4O_VISITED:
          case S4O_WR_OP:
            /* all these are terminating conditions */
            return TRUE;
          case S4O_CONDJMP:
            /* two possible destinations: recurse */
              {
                lineNode *pl2 = plConditional;
                D(("CONDJMP trying other branch first\n"));
                if (!doTermScan (&pl2, what))
                  return FALSE;
                D(("Other branch OK.\n"));
              }
            continue;
          case S4O_RD_OP:
          default:
            /* no go */
            return FALSE;
        }
    }
}

/* Regular 8 bit reg */
static bool
isReg(const char *what)
{
  if(strlen(what) != 1)
    return FALSE;
  switch(*what)
    {
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'h':
    case 'l':
      return TRUE;
    }
  return FALSE;
}

/* 8-Bit reg only accessible by 16-bit and undocumented instructions */
static bool
isUReg(const char *what)
{
  if(strcmp(what, "iyl") == 0 || strcmp(what, "iyh") == 0)
    return TRUE;
  if(strcmp(what, "ixl") == 0 || strcmp(what, "ixh") == 0)
    return TRUE;
  return FALSE;
}

static bool
isRegPair(const char *what)
{
  if(strlen(what) != 2)
    return FALSE;
  if(strcmp(what, "ba") == 0)	/* S1C88: B:A is a real splittable pair (b high, a low) */
    return TRUE;
  if(strcmp(what, "bc") == 0)
    return TRUE;
  if(strcmp(what, "de") == 0)
    return TRUE;
  if(strcmp(what, "hl") == 0)
    return TRUE;
  if(strcmp(what, "ix") == 0)
    return TRUE;
  if(strcmp(what, "iy") == 0)
    return TRUE;
  return FALSE;
}

/* Check that what is never read after endPl. */

bool
s1c88notUsed (const char *what, lineNode *endPl, lineNode *head)
{
  lineNode *pl;
  D(("Checking for %s\n", what));

  if(strcmp(what, "af") == 0)
    {
      if(!s1c88notUsed("a", endPl, head))
        return FALSE;
      what++;
    }

  /* S1C88 has FOUR flags: Z, C, V (overflow), N (negative).  The z80 nf
     (add-subtract) and hf (half-carry) do not exist here. */
  if(strcmp(what, "f") == 0)
    return s1c88notUsed("zf", endPl, head) && s1c88notUsed("cf", endPl, head) &&
           s1c88notUsed("nf", endPl, head) && s1c88notUsed("vf", endPl, head);

  if(strcmp(what, "iy") == 0)
    {
      if(IY_RESERVED)
        return FALSE;
      return(s1c88notUsed("iyl", endPl, head) && s1c88notUsed("iyh", endPl, head));
    }

  if(strcmp(what, "ix") == 0)
    return(s1c88notUsed("ixl", endPl, head) && s1c88notUsed("ixh", endPl, head));

  if(isRegPair(what))
    {
      char low[2], high[2];
      low[0] = what[1];
      high[0] = what[0];
      low[1] = 0;
      high[1] = 0;
      return(s1c88notUsed(low, endPl, head) && s1c88notUsed(high, endPl, head));
    }

  // L/V (Rabbit) names the overflow flag V on the S1C88
  if(!strcmp(what, "lf"))
    what = "vf";

  // SM83 does not use what it does not have
  // but this allows to write rules for all Z80ies
  

  // enable sp and flags
  if(!isReg(what) && !isUReg(what) &&
     strcmp(what, "sp") && strcmp(what+1, "f"))
    return FALSE;

  _G.head = head;

  unvisitLines (_G.head);

  pl = endPl->next;
  if (!doTermScan (&pl, what))
    return FALSE;

  return TRUE;
}

bool
s1c88notUsedFrom (const char *what, const char *label, lineNode *head)
{
  lineNode *cpl;

  for (cpl = head; cpl; cpl = cpl->next)
    {
      if (cpl->isLabel && !strncmp (label, cpl->line, strlen(label)))
        {
          return s1c88notUsed (what, cpl, head);
        }
    }

  return false;
}

bool
s1c88canAssign (const char *op1, const char *op2, const char *exotic)
{
  const char *dst, *src;

  // Indexed accesses: One is the indexed one, the other one needs to be a reg or immediate.
  if(exotic)
  {
    if(!strcmp(exotic, "ix") || !strcmp(exotic, "iy"))
    {
      if (isReg(op1))
        return TRUE;
    }
    else if(!strcmp(op2, "ix") || !strcmp(op2, "iy"))
    {
      /* S1C88: `ld d(ix),<reg>` is legal but `ld d(ix),#imm` is NOT (only
         `ld (hl),#nn` takes an immediate destination) — so do not report an
         immediate source as assignable to indexed memory. Otherwise peepholes
         like 9/9a fold `ld a,#0; ld d(ix),a` into the illegal `ld d(ix),#0`. */
      if (isReg(exotic))
        return TRUE;
    }

    return FALSE;
  }

  // Everything else.
  dst = op1;
  src = op2;

  // 8-bit regs can be assigned to each other directly.
  if(isReg(dst) && isReg(src))
    return true;
  if(HAS_IYL_INST) // With some restrictions also for iyl, iyh, ixl, ixh.
    {
      if (isUReg(dst) && isReg(src) && src[0] <= 'e')
        return true;
      if (isUReg(src) && isReg(dst) && dst[0] <= 'e')
        return true;    
      if (isUReg(dst) && isUReg(src) && src[1] == dst[1])
        return true;
    }

  // Immediates van be loaded into 8-bit registers.
  if((isReg(dst) || HAS_IYL_INST && isUReg(dst)) && src[0] == '#')
    return true;

  // Same if at most one of them is (hl).
  if ((isReg(dst)) && !strcmp(src, "(hl)"))
    return TRUE;
  if (!strcmp(dst, "(hl)") && (isReg(src)))
    return TRUE;

  // Can assign between a and (bc), (de), (hl+), (hl-)
  if(!strcmp(dst, "a") &&
     (!strcmp(src, "(bc)") || !strcmp(src, "(de)") || !strcmp(src, "(hl+)") || !strcmp(src, "(hl-)")))
    return TRUE;
  if((!strcmp(dst, "(bc)") || !strcmp(dst, "(de)") || !strcmp(src, "(hl+)") || !strcmp(src, "(hl-)"))
     && !strcmp(src, "a"))
    return TRUE;

  // Can load immediate values directly into registers and register pairs.
  if((isReg(dst) || isRegPair(dst) || !strcmp(src, "sp")) && src[0] == '#')
    return TRUE;

  if((!strcmp(dst, "a") || ((isRegPair(dst) || !strcmp(src, "sp")))) && !strncmp(src, "(#", 2))
    return TRUE;
  if(!strncmp(dst, "(#", 2) && (!strcmp(src, "a") || (isRegPair(src)) || !strcmp(src, "sp")))
    return TRUE;

  // Can load immediate values directly into (hl).
  if(!strcmp(dst, "(hl)") && src[0] == '#')
    return true;

  // Can load between hl / ix / iy and sp.
  if(!strcmp(dst, "sp") && (!strcmp(src, "hl") || !strcmp(src, "ix") || !strcmp(src, "iy")) ||
    (!strcmp(dst, "hl") || !strcmp(dst, "ix") || !strcmp(dst, "iy")) && !strcmp(src, "sp"))
    return true;

  return false;
}

static const char *
registerBaseName (const char *op)
{
  if (!strcmp (op, "d") || !strcmp (op, "e") || !strcmp (op, "(de)"))
    return "de";
  if (!strcmp (op, "b") || !strcmp (op, "c") || !strcmp (op, "(bc)"))
    return "bc";
  if (!strcmp (op, "h") || !strcmp (op, "l") || !strcmp (op, "(hl)") || !strcmp (op, "(hl+)")  || !strcmp (op, "(hl-)"))
    return "hl";
  if (!strcmp (op, "iyh") || !strcmp (op, "iyl") || strstr (op, "iy"))
    return "iy";
  if (!strcmp (op, "ixh") || !strcmp (op, "ixl") || strstr (op, "ix"))
    return "ix";
  if (!strcmp (op, "a"))
    return "af";
  return op;
}

// canJoinRegs(reg_hi reg_lo [dst]) returns TRUE,
bool s1c88canJoinRegs (const char **regs, char dst[20])
{
  //check for only 2 source registers
  if (!regs[0] || !regs[1] || regs[2])
    return FALSE;
  size_t l1 = strlen (regs[0]);
  size_t l2 = strlen (regs[1]);
  if (l1 + l2 >= 20)
    return FALSE;
  if (l1 == 0 || l2 == 0)
    {
      if (l1 == 0 && l2 == 0)
        return FALSE;
      strcpy (dst, registerBaseName (regs[l1 ? 0 : 1]));
    }
  else
    {
      memcpy (&dst[0], regs[0], l1);
      memcpy (&dst[l1], regs[1], l2 + 1); //copy including \0
    }
  if (!strcmp (dst, "ixhixl") || !strcmp (dst, "iyhiyl"))
    {
      
      dst[2] = '\0';
    }
  return isRegPair (dst);
}

bool s1c88canSplitReg (const char *reg, char dst[][16], int nDst)
{
  int i;
  if (nDst < 0 || nDst > 2)
    return FALSE;
  if (!strcmp (reg, "ba") || !strcmp (reg, "bc") || !strcmp (reg, "de") || !strcmp (reg, "hl"))
    {
      /* split into the two named byte registers, high (reg[0]) then low (reg[1]):
         ba -> b,a   bc -> b,c   de -> d,e   hl -> h,l */
      for (i = 0; i < nDst; ++i)
        {
          dst[i][0] = reg[i];
          dst[i][1] = '\0';
        }
    }
  else if (HAS_IYL_INST && (!strcmp (reg, "ix") || !strcmp (reg, "iy")))
    {
      for (i = 0; i < nDst; ++i)
        {
          dst[i][0] = reg[0];
          dst[i][1] = reg[1];
          dst[i][2] = "hl"[i];
          dst[i][3] = '\0';
        }
    }
  else
    return FALSE;

  return TRUE;
}

/* Operand classes for s1c88instructionSize.  The classes name the S1C88
   addressing modes as the codegen prints them: registers, `#imm`, the plain
   indirects `(hl)`/`(ix)`/`(iy)`, the displaced `d (ix)`/`d (iy)`/`d (sp)`,
   `(br:ll)`, and any other parenthesised expression = a 16-bit absolute
   `(hhll)`.  Anything else is OPC_OTHER and sized conservatively (large). */
enum s1c88_opclass
{
  OPC_NONE, OPC_R8, OPC_R16, OPC_SP, OPC_SPEC, OPC_IMM, OPC_IND,
  OPC_DISP_IXY, OPC_DISP_SP, OPC_BRLL, OPC_ABS, OPC_OTHER
};

static enum s1c88_opclass
s1c88_opclass (const char *op)
{
  size_t n;
  const char *paren;

  if (!op)
    return OPC_NONE;
  while (isspace ((unsigned char) *op))
    op++;
  if (*op == '#')
    return OPC_IMM;
  if (*op == '(')
    {
      if (!STRNCASECMP (op, "(hl)", 4) || !STRNCASECMP (op, "(ix)", 4) || !STRNCASECMP (op, "(iy)", 4))
        return OPC_IND;
      if (!STRNCASECMP (op, "(br:", 4))
        return OPC_BRLL;
      return OPC_ABS;
    }
  /* `d (ix)` — displacement first, then the register.  Only look inside THIS
     operand: op1 is handed over with the rest of the line attached. */
  paren = strchr (op, '(');
  if (paren && strchr (op, ',') && strchr (op, ',') < paren)
    paren = NULL;
  if (paren)
    {
      if (!STRNCASECMP (paren, "(ix)", 4) || !STRNCASECMP (paren, "(iy)", 4))
        return OPC_DISP_IXY;
      if (!STRNCASECMP (paren, "(sp)", 4))
        return OPC_DISP_SP;
      return OPC_OTHER;
    }
  n = strcspn (op, ", \t");
  if (n == 1 && strchr ("abhlABHL", op[0]))
    return OPC_R8;
  if (n == 2)
    {
      if (!STRNCASECMP (op, "sp", 2))
        return OPC_SP;
      if (!STRNCASECMP (op, "ba", 2) || !STRNCASECMP (op, "hl", 2) || !STRNCASECMP (op, "ix", 2) ||
          !STRNCASECMP (op, "iy", 2) || !STRNCASECMP (op, "pc", 2))
        return OPC_R16;
      if (!STRNCASECMP (op, "br", 2) || !STRNCASECMP (op, "sc", 2) || !STRNCASECMP (op, "ep", 2) ||
          !STRNCASECMP (op, "xp", 2) || !STRNCASECMP (op, "yp", 2) || !STRNCASECMP (op, "nb", 2) ||
          !STRNCASECMP (op, "ip", 2))
        return OPC_SPEC;
    }
  return OPC_OTHER;
}

/* Byte size of `ld dst, src` per instruction-set.md §4.3.3 (8-bit and 16-bit
   transfer tables).  Unknown combinations return 4: an OVER-estimate only
   costs a missed jrs shortening, an under-estimate silently overflows one. */
static int
s1c88_ldSize (const char *op1, const char *op2)
{
  enum s1c88_opclass d = s1c88_opclass (op1), s = s1c88_opclass (op2);

  switch (d)
    {
    case OPC_R8:                        /* ld a/b/l/h, X */
      switch (s)
        {
        case OPC_R8: case OPC_IND:           return 1;   /* 4x..5x / 45..5F */
        case OPC_IMM: case OPC_SPEC: case OPC_BRLL: return 2; /* Bx nn / CE Cx / 44 ll */
        case OPC_DISP_IXY:                   return 3;   /* CE 4x dd */
        case OPC_ABS:                        return 4;   /* CE Dx ll hh */
        default:                             return 4;
        }
    case OPC_SPEC:                      /* ld br/sc/nb/ep/xp/yp, a|#imm */
      if (s == OPC_R8)
        return 2;                                        /* CE Cx */
      if (s == OPC_IMM)
        return (!STRNCASECMP (op1, "br", 2) || !STRNCASECMP (op1, "sc", 2)) ? 2 : 3; /* B4 hh / 9F nn / CE Cx pp */
      return 4;
    case OPC_IND:                       /* ld (hl)/(ix)/(iy), X */
      switch (s)
        {
        case OPC_R8: case OPC_IND:           return 1;   /* 6x..7x */
        case OPC_IMM: case OPC_BRLL: case OPC_R16: return 2; /* B5 nn / 6C ll / CF C4.. */
        case OPC_DISP_IXY:                   return 3;   /* CE 6x dd */
        default:                             return 4;
        }
    case OPC_DISP_IXY:                  /* ld d (ix), r8 : CE 4x dd */
      return s == OPC_R8 ? 3 : 4;
    case OPC_DISP_SP:                   /* ld d (sp), rr : CF 7x dd */
      return s == OPC_R16 ? 3 : 4;
    case OPC_BRLL:                      /* ld (br:ll), X */
      switch (s)
        {
        case OPC_R8: case OPC_IND:           return 2;   /* 78 ll / 7D ll */
        case OPC_IMM:                        return 3;   /* DD ll nn */
        default:                             return 4;
        }
    case OPC_ABS:                       /* ld (hhll), X */
      switch (s)
        {
        case OPC_R8:                         return 4;   /* CE D4 ll hh */
        case OPC_R16:                        return 3;   /* BC ll hh */
        default:                             return 4;
        }
    case OPC_R16:
    case OPC_SP:                        /* ld ba/hl/ix/iy/sp, X */
      switch (s)
        {
        case OPC_R16: case OPC_SP: case OPC_IND: return 2; /* CF Ex / CF Fx / CF Cx */
        case OPC_IMM:                        return d == OPC_SP ? 4 : 3; /* CF 6E nn mm / C4 nn mm */
        case OPC_DISP_SP: case OPC_ABS:      return 3;   /* CF 7x dd / B8 ll hh */
        default:                             return 4;
        }
    default:
      return 4;
    }
}

int
s1c88instructionSize (lineNode *pl)
{
  const char *op1start, *op2start;
  enum s1c88_opclass c1, c2;

  /* move to the first operand:
   * leading spaces are already removed, skip the mnemonic */
  for (op1start = pl->line; *op1start && !isspace (*op1start); ++op1start);

  /* skip the spaces between mnemonic and the operand */
  while (isspace (*op1start))
    ++op1start;
  if (!(*op1start))
    op1start = NULL;

  if (op1start)
    {
      /* move to the second operand:
       * find the comma and skip the following spaces */
      op2start = strchr(op1start, ',');
      if (op2start)
        {
          do
            ++op2start;
          while (isspace (*op2start));

          if ('\0' == *op2start)
            op2start = NULL;
        }
    }
  else
    op2start = NULL;

  c1 = s1c88_opclass (op1start);
  c2 = s1c88_opclass (op2start);

  /* The sizes below are the S1C88 encodings (docs/s1c88/instruction-set.md
     §4.3.3).  scripts/insn-size-check.sh assembles every line the compiler
     emits and compares against this model (SDCC88_INSN_SIZE_DUMP=1, see the
     dump loop at the end of genS1C88Code).  An UNDER-estimate lets the jp->jrs
     peephole (labelInRange) emit a short branch whose displacement overflows;
     an over-estimate only costs a missed shortening. */

  if(ISINST(pl->line, "ld"))
    return (op1start && op2start) ? s1c88_ldSize (op1start, op2start) : 4;

  /* Exchange / swap: ex a,b (CC), ex a,(hl) (CD), ex ba,rr (C8..CB), swap (F6/F7) */
  if(ISINST(pl->line, "ex") || ISINST(pl->line, "swap"))
    return(1);

  /* Push / pop: pairs and the special registers are 1 byte (A0..AF); the
     byte registers a/b/l/h and all/ale are CF-prefixed (2). */
  if(ISINST(pl->line, "push") || ISINST(pl->line, "pop"))
    {
      if(c1 == OPC_R8 || (op1start && (!STRNCASECMP(op1start, "all", 3) || !STRNCASECMP(op1start, "ale", 3))))
        return(2);
      return(1);
    }

  /* and/or/xor sc, #nn (9C/9D/9E nn) — the interrupt-level mask ops */
  if(c1 == OPC_SPEC && op1start && !STRNCASECMP(op1start, "sc", 2) &&
     (ISINST(pl->line, "and") || ISINST(pl->line, "or") || ISINST(pl->line, "xor")))
    return(2);

  /* 16-bit ALU (dst in ba/hl/ix/iy/sp):
       reg, reg                      -> 2 (CF xx)
       add/sub/cp  reg, #imm         -> 3
       adc/sbc     reg, #imm         -> 4 (CF-prefixed)
       add/sub/cp  sp,  #imm         -> 4 (CF 68/6A/6C ll hh) */
  if ((c1 == OPC_R16 || c1 == OPC_SP) && op2start &&
      (ISINST(pl->line, "add") || ISINST(pl->line, "adc") || ISINST(pl->line, "sub") ||
       ISINST(pl->line, "sbc") || ISINST(pl->line, "cp")))
    {
      if (c2 != OPC_IMM)
        return (2);
      if (ISINST(pl->line, "adc") || ISINST(pl->line, "sbc") || c1 == OPC_SP)
        return (4);
      return (3);
    }

  /* cp b/l/h, #nn (CE BC..BE nn); cp br, #hh (CE BF hh) */
  if(ISINST(pl->line, "cp") && c2 == OPC_IMM && op1start && STRNCASECMP(op1start, "a", 1))
    return(3);

  /* 8-bit ALU with the accumulator: op a, X */
  if(c1 == OPC_R8 && op2start && !STRNCASECMP(op1start, "a", 1) &&
     (ISINST(pl->line, "add") || ISINST(pl->line, "adc") || ISINST(pl->line, "sub") || ISINST(pl->line, "sbc") ||
      ISINST(pl->line, "cp")  || ISINST(pl->line, "and") || ISINST(pl->line, "or")  || ISINST(pl->line, "xor")))
    {
      switch (c2)
        {
        case OPC_R8: case OPC_IND:  return(1);   /* 00..3F */
        case OPC_IMM: case OPC_BRLL: return(2);  /* 02 nn / 04 ll */
        case OPC_DISP_IXY: case OPC_ABS: return(3); /* CE 0x dd / 05 ll hh */
        default: return(4);
        }
    }
  /* 8-bit ALU on memory: op (hl), a|#nn|(ix)|(iy) — CE 04.. (2) / CE 05 nn (3) */
  if(c1 == OPC_IND && op2start &&
     (ISINST(pl->line, "add") || ISINST(pl->line, "adc") || ISINST(pl->line, "sub") || ISINST(pl->line, "sbc") ||
      ISINST(pl->line, "cp")  || ISINST(pl->line, "and") || ISINST(pl->line, "or")  || ISINST(pl->line, "xor")))
    return(c2 == OPC_IMM ? 3 : 2);

  /* Increment / decrement: registers, pairs, sp and (hl) are 1 byte; (br:ll) 2 */
  if(ISINST(pl->line, "inc") || ISINST(pl->line, "dec"))
    {
      if(c1 == OPC_R8 || c1 == OPC_R16 || c1 == OPC_SP || c1 == OPC_IND || c1 == OPC_SPEC)
        return(1);
      if(c1 == OPC_BRLL)
        return(2);
      return(4);
    }

  /* Rotates / shifts: CE 8x/9x (2); the (br:ll) form carries ll (3) */
  if(ISINST(pl->line, "rlc") || ISINST(pl->line, "rl")  || ISINST(pl->line, "rrc") || ISINST(pl->line, "rr") ||
     ISINST(pl->line, "sla") || ISINST(pl->line, "sll") || ISINST(pl->line, "sra") || ISINST(pl->line, "srl"))
    return(c1 == OPC_BRLL ? 3 : 2);

  /* Bit test: bit a,b (94) = 1; bit a/b/(hl), #nn (96/97/95 nn) = 2; bit (br:ll),#nn = 3 */
  if(ISINST(pl->line, "bit"))
    {
      if(c1 == OPC_BRLL)
        return(3);
      return(c2 == OPC_R8 ? 1 : 2);
    }

  /* cpl / neg: CE A0..A7 (2), (br:ll) form 3 */
  if(ISINST(pl->line, "cpl") || ISINST(pl->line, "neg"))
    return(c1 == OPC_BRLL ? 3 : 2);

  /* S1C88 PC-relative branches.  jrs/cars are 2 bytes for the basic conditions
     (c/nc/z/nz) and the unconditional form, but 3 bytes for the CE-prefixed
     signed/flag conditions (lt/le/gt/ge/v/nv/p/m/f0..nf3).  jrl/carl are 3 bytes;
     djr is 2.  Sizing these keeps labelInRange() accurate so the peephole can
     shorten jp/jrl -> jrs across them. */
  if(ISINST(pl->line, "jrs") || ISINST(pl->line, "cars"))
    {
      if(op2start &&
         STRNCASECMP(op1start, "c,", 2) && STRNCASECMP(op1start, "nc,", 3) &&
         STRNCASECMP(op1start, "z,", 2) && STRNCASECMP(op1start, "nz,", 3))
        return(3);	/* signed/flag condition -> CE-prefixed */
      return(2);
    }
  if(ISINST(pl->line, "jrl") || ISINST(pl->line, "carl"))
    return(3);
  if(ISINST(pl->line, "djr"))
    return(2);

  if(ISINST(pl->line, "jp"))
    {
      if(!STRNCASECMP(op1start, "(hl)", 4) || !STRNCASECMP(op1start, "hl", 2))
        return(1);	/* jp hl: F4 */
      if(!STRNCASECMP(op1start, "(ix)", 4) || !STRNCASECMP(op1start, "(iy)", 4))
        return(2);
      /* S1C88: a signed/flag-conditional jp is the assembler's fixed
         invert-and-skip lowering (jrs <inv>,+4 ; jrl e) = 6 bytes; a basic
         c/nc/z/nz jp assembles as jrl cc = 3 bytes. */
      if(op2start &&
         STRNCASECMP(op1start, "c,", 2) && STRNCASECMP(op1start, "nc,", 3) &&
         STRNCASECMP(op1start, "z,", 2) && STRNCASECMP(op1start, "nz,", 3))
        return(6);
      return(3);
    }

  /* ret (F8), rete (F9), rets (FA) */
  if(ISINST(pl->line, "ret") || ISINST(pl->line, "rete") || ISINST(pl->line, "rets"))
    return(1);

  if(ISINST(pl->line, "call"))
    return(3);

  /* banked pseudo-ops.  DYNAMIC size by condition class (matches sdas88):
       unconditional / basic c,nc,z,nz -> ld nb,#bank ; carl/jrl[cc]      = 6
       short-only signed/flag cc        -> jrs inv,+7 ; ld nb,#bank ; ...  = 9
     The codegen only emits the unconditional form; the 9-byte case is for
     hand-written `bjump/bcall <signed cc>, target`.  NOTE: these sizes MUST
     equal what the assembler emits — a mismatch silently corrupts branch-range
     decisions.  scripts/insn-size-check.sh guards the contract. */
  if(ISINST(pl->line, "bcall") || ISINST(pl->line, "bjump"))
    {
      const char *p = pl->line;
      while(*p && *p != ' ' && *p != '\t') p++;     /* skip mnemonic */
      while(*p == ' ' || *p == '\t') p++;           /* to operand 1  */
      if(strchr(p, ',')                              /* a cc is present, and  */
         && strncmp(p, "c,", 2) && strncmp(p, "nc,", 3)
         && strncmp(p, "z,", 2) && strncmp(p, "nz,", 3))  /* it's not basic   */
        return(9);                                   /* short-only -> invert+skip */
      return(6);
    }

  /* One-byte misc: nop (FF), pack (DE), upck (DF) */
  if(ISINST(pl->line, "nop") || ISINST(pl->line, "pack") || ISINST(pl->line, "upck"))
    return(1);

  /* CE-page misc: sep (CE A8), mlt (CE D8), div (CE D9), halt (CE AE), slp (CE AF) */
  if(ISINST(pl->line, "sep") || ISINST(pl->line, "mlt") || ISINST(pl->line, "div") ||
     ISINST(pl->line, "halt") || ISINST(pl->line, "slp"))
    return(2);

  if(ISINST(pl->line, ".db") || ISINST(pl->line, ".byte"))
    {
      int i, j;
      for(i = 1, j = 0; pl->line[j]; i += pl->line[j] == ',', j++);
      return(i);
    }

  if(ISINST(pl->line, ".dw") || ISINST(pl->line, ".word"))
    {
      int i, j;
      for(i = 1, j = 0; pl->line[j]; i += pl->line[j] == ',', j++);
      return(i * 2);
    }

  /* If the instruction is unrecognized, we shouldn't try to optimize.  */
  /* For all we know it might be some .ds or similar possibly long line */
  /* Return a large value to discourage optimization.                   */
  if (pl->ic)
    werrorfl(pl->ic->filename, pl->ic->lineno, W_UNRECOGNIZED_ASM, __func__, 999, pl->line);
  else
    werrorfl("unknown", 0, W_UNRECOGNIZED_ASM, __func__, 999, pl->line);
  return(999);
}


bool s1c88symmParmStack (const char *name)
{
  if (!strcmp (name, "___sdcc_enter_ix"))
   return false;
  return s1c88_symmParm_in_calls_from_current_function;
}

