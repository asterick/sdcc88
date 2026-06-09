/*-------------------------------------------------------------------------
  main.c - Epson S1C88 port definitions (sdcc88; derived from the z80 port).

  Michael Hope <michaelh@juju.net.nz> 2001
  Copyright (C) 2021, Sebastian 'basxto' Riedel <sdcc@basxto.de>

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

#include <sys/stat.h>
#include "s1c88.h"
#include "SDCCsystem.h"
#include "SDCCutil.h"
#include "SDCCargs.h"
#include "dbuf_string.h"

#define OPTION_BO               "-bo"
#define OPTION_BA               "-ba"
#define OPTION_CODE_SEG         "--codeseg"
#define OPTION_CONST_SEG        "--constseg"
#define OPTION_DATA_SEG         "--dataseg"
#define OPTION_CALLEE_SAVES_BC  "--callee-saves-bc"
#define OPTION_NO_STD_CRT0      "--no-std-crt0"
#define OPTION_RESERVE_IY       "--reserve-regs-iy"
#define OPTION_FRAMEPOINTER     "--fno-omit-frame-pointer"
#define OPTION_EMIT_EXTERNS     "--emit-externs"
#define OPTION_LEGACY_BANKING   "--legacy-banking"
#define OPTION_SDCCCALL         "--sdcccall"
#define OPTION_ALLOW_UNDOC_INST "--allow-undocumented-instructions"

static char _s1c88_defaultRules[] = {
#include "peeph.rul"
};







S1C88_OPTS s1c88_opts;


static OPTION _s1c88_options[] = {
  {0, OPTION_CALLEE_SAVES_BC, &s1c88_opts.calleeSavesBC, "Force a called function to always save BC"},
  {0, OPTION_BO,              NULL, "<num> use code bank <num>"},
  {0, OPTION_BA,              NULL, "<num> use data bank <num>"},
  {0, OPTION_CODE_SEG,        &options.code_seg, "<name> use this name for the code segment", CLAT_STRING},
  {0, OPTION_CONST_SEG,       &options.const_seg, "<name> use this name for the const segment", CLAT_STRING},
  {0, OPTION_DATA_SEG,        &options.data_seg, "<name> use this name for the data segment", CLAT_STRING},
  {0, OPTION_NO_STD_CRT0,     &options.no_std_crt0, "Do not link default crt0.rel"},
  {0, OPTION_RESERVE_IY,      &s1c88_opts.reserveIY, "Do not use IY (incompatible with --fomit-frame-pointer)"},
  {0, OPTION_FRAMEPOINTER,    &s1c88_opts.noOmitFramePtr, "Do not omit frame pointer"},
  {0, OPTION_EMIT_EXTERNS,    NULL, "Emit externs list in generated asm"},
  {0, OPTION_LEGACY_BANKING,  &s1c88_opts.legacyBanking, "Use legacy method to call banked functions"},
  {0, OPTION_SDCCCALL,        &options.sdcccall, "Set ABI version for default calling convention", CLAT_INTEGER},
  {0, OPTION_ALLOW_UNDOC_INST,&options.allow_undoc_inst, "Allow use of undocumented instructions"},
  {0, NULL}
};


static struct
{
  // Determine if we can put parameters in registers
  struct
  {
    int n;
    struct sym_link *ftype;
  } regparam;
}
_G;

static char *_keywords[] = {
  "sfr",
  "nonbanked",
  "banked",
  "at",
  "far",                        /* __far = 3-byte (24-bit EP:offset) data pointer / far-space object */
  "near",                       /* __near = explicit 2-byte data pointer (the default) */
  "_naked",
  "critical",
  "interrupt",
  "z88dk_fastcall",
  "z88dk_callee",
  "smallc",
  "z88dk_shortcall",
  "z88dk_params_offset",
  NULL
};



extern PORT s1c88_port;

#include "mappings.i"

static builtins _s1c88_builtins[] = {
  {"__builtin_memcpy", "vg*", 3, {"vg*", "Cvg*", "Ui"}},
  {"__builtin_strcpy", "cg*", 2, {"cg*", "Ccg*"}},
  {"__builtin_strncpy", "cg*", 3, {"cg*", "Ccg*", "Ui"}},
  {"__builtin_strchr", "cg*", 2, {"Ccg*", "i"}},
  {"__builtin_memset", "vg*", 3, {"vg*", "i", "Ui"}},
  {NULL, NULL, 0, {NULL}}
};

extern reg_info s1c88_regs[];
extern void s1c88_init_asmops (void);
extern reg_info *regsS1C88;

static void
_s1c88_init (void)
{
  /* The S1C88 toolchain is asxxxx/sdas-only (see CLAUDE.md); there is one asm
     dialect, so no asm-type selection. */
  asm_addTree (&_s1c88_asxxxx_z80);

  regsS1C88 = s1c88_regs;
  s1c88_init_asmops ();
}










static void
_reset_regparm (struct sym_link *ftype)
{
  _G.regparam.n = 0;
  _G.regparam.ftype = ftype;
  if (IFFUNC_ISZ88DK_FASTCALL (ftype) && IFFUNC_HASVARARGS (ftype))
    werror (E_Z88DK_FASTCALL_PARAMETERS);
}

static int
_reg_parm (sym_link *l, bool reentrant)
{
  if (IFFUNC_ISZ88DK_FASTCALL (_G.regparam.ftype))
    {
      if (_G.regparam.n)
        werror (E_Z88DK_FASTCALL_PARAMETERS);
      if (getSize (l) > 4)
        werror (E_Z88DK_FASTCALL_PARAMETER);
    }

  bool is_regarg = s1c88IsRegArg (_G.regparam.ftype, ++_G.regparam.n, 0);

  return (is_regarg ? _G.regparam.n : 0);
}

enum
{
  P_BANK = 1,
  P_CODESEG,
  P_CONSTSEG,
};

static int
do_pragma (int id, const char *name, const char *cp)
{
  struct pragma_token_s token;
  int err = 0;
  int processed = 1;

  init_pragma_token (&token);

  switch (id)
    {
    case P_BANK:
      {
        struct dbuf_s buffer;

        dbuf_init (&buffer, 128);

        cp = get_pragma_token (cp, &token);

        switch (token.type)
          {
          case TOKEN_EOL:
            err = 1;
            break;

          case TOKEN_INT:
            dbuf_printf (&buffer, "CODE_%d", token.val.int_val);
            break;

          default:
            {
              const char *str = get_pragma_string (&token);

              dbuf_append_str (&buffer, (0 == strcmp ("BASE", str)) ? "HOME" : str);
            }
            break;
          }

        cp = get_pragma_token (cp, &token);
        if (TOKEN_EOL != token.type)
          {
            err = 1;
            break;
          }

        dbuf_c_str (&buffer);
        options.code_seg = (char *) dbuf_detach (&buffer);
      }
      break;

    case P_CODESEG:
    case P_CONSTSEG:
      {
        char *segname;

        cp = get_pragma_token (cp, &token);
        if (token.type == TOKEN_EOL)
          {
            err = 1;
            break;
          }

        segname = Safe_strdup (get_pragma_string (&token));

        cp = get_pragma_token (cp, &token);
        if (token.type != TOKEN_EOL)
          {
            Safe_free (segname);
            err = 1;
            break;
          }

        if (id == P_CODESEG)
          {
            if (options.code_seg)
              Safe_free (options.code_seg);
            options.code_seg = segname;
          }
        else
          {
            if (options.const_seg)
              Safe_free (options.const_seg);
            options.const_seg = segname;
          }
      }
      break;

    default:
      processed = 0;
      break;
    }

  get_pragma_token (cp, &token);

  if (1 == err)
    werror (W_BAD_PRAGMA_ARGUMENTS, name);

  free_pragma_token (&token);
  return processed;
}

static struct pragma_s pragma_tbl[] = {
  {"bank", P_BANK, 0, do_pragma},
  {"codeseg", P_CODESEG, 0, do_pragma},
  {"constseg", P_CONSTSEG, 0, do_pragma},
  {NULL, 0, 0, NULL},
};

static int
_process_pragma (const char *s)
{
  return process_pragma_tbl (pragma_tbl, s);
}




static bool
_parseOptions (int *pargc, char **argv, int *i)
{
  if (argv[*i][0] == '-')
    {
      {
          if (!strncmp (argv[*i], OPTION_BO, sizeof (OPTION_BO) - 1))
            {
              /* ROM bank */
              int bank = getIntArg (OPTION_BO, argv, i, *pargc);
              struct dbuf_s buffer;

              dbuf_init (&buffer, 16);
              dbuf_printf (&buffer, "CODE_%u", bank);
              dbuf_c_str (&buffer);
              options.code_seg = (char *) dbuf_detach (&buffer);
              return TRUE;
            }
          else if (!strncmp (argv[*i], OPTION_BA, sizeof (OPTION_BA) - 1))
            {
              /* RAM bank */
              int bank = getIntArg (OPTION_BA, argv, i, *pargc);
              struct dbuf_s buffer;

              dbuf_init (&buffer, 16);
              dbuf_printf (&buffer, "DATA_%u", bank);
              dbuf_c_str (&buffer);
              options.data_seg = (char *) dbuf_detach (&buffer);
              return TRUE;
            }
        }

      if (!strncmp (argv[*i], OPTION_EMIT_EXTERNS, sizeof (OPTION_EMIT_EXTERNS) - 1))
        {
          port->assembler.externGlobal = 1;
          return true;
        }
    }
  return FALSE;
}

static void
_setValues (void)
{
  const char *s;
  struct dbuf_s dbuf;

  if (options.nostdlib == FALSE)
    {
      const char *s;
      char *path;
      struct dbuf_s dbuf;

      dbuf_init (&dbuf, PATH_MAX);

      for (s = setFirstItem (libDirsSet); s != NULL; s = setNextItem (libDirsSet))
        {
          path = buildCmdLine2 ("-k\"%s" DIR_SEPARATOR_STRING "{port}\" ", s);
          dbuf_append_str (&dbuf, path);
          Safe_free (path);
        }
      path = buildCmdLine2 ("-l\"{port}.lib\"", s);
      dbuf_append_str (&dbuf, path);
      Safe_free (path);

      setMainValue ("z80libspec", dbuf_c_str (&dbuf));
      dbuf_destroy (&dbuf);

      for (s = setFirstItem (libDirsSet); s != NULL; s = setNextItem (libDirsSet))
        {
          struct stat stat_buf;

          path = buildCmdLine2 ("%s" DIR_SEPARATOR_STRING "{port}" DIR_SEPARATOR_STRING "crt0{objext}", s);
          if (stat (path, &stat_buf) == 0)
            {
              Safe_free (path);
              break;
            }
          else
            Safe_free (path);
        }

      if (s == NULL)
        setMainValue ("z80crt0", "\"crt0{objext}\"");
      else
        {
          struct dbuf_s dbuf;

          dbuf_init (&dbuf, 128);
          dbuf_printf (&dbuf, "\"%s\"", path);
          setMainValue ("z80crt0", dbuf_c_str (&dbuf));
          dbuf_destroy (&dbuf);
        }
    }
  else
    {
      setMainValue ("z80libspec", "");
      setMainValue ("z80crt0", "");
    }

  setMainValue ("z80extralibfiles", (s = joinStrSet (libFilesSet)));
  Safe_free ((void *) s);
  setMainValue ("z80extralibpaths", (s = joinStrSet (libPathsSet)));
  Safe_free ((void *) s);

  setMainValue ("z80outputtypeflag", "-i");
  setMainValue ("z80outext", ".ihx");

  setMainValue ("stdobjdstfilename", "{dstfilename}{objext}");
  setMainValue ("stdlinkdstfilename", "{dstfilename}{z80outext}");

  setMainValue ("z80extraobj", (s = joinStrSet (relFilesSet)));
  Safe_free ((void *) s);

  dbuf_init (&dbuf, 128);
  dbuf_printf (&dbuf, "-b_CODE=0x%04X -b_DATA=0x%04X", options.code_loc, options.data_loc);
  setMainValue ("z80bases", dbuf_c_str (&dbuf));
  dbuf_destroy (&dbuf);
}

static void
_finaliseOptions (void)
{
  port->mem.default_local_map = data;
  port->mem.default_globl_map = data;
  /* S1C88: IX/IY are index-only (never byte-allocated), so num_regs stays at
     A,B,L,H regardless of the --reserve-iy option. */

  _setValues ();
}

static void
_setDefaultOptions (void)
{
  options.nopeep = 0;
  options.stackAuto = 1;
  /* first the options part */
  options.intlong_rent = 1;
  options.float_rent = 1;
  options.noRegParams = 0;
  /* Default code and data locations — the Pokémon Mini common-area memory map.
     The cartridge header is a separate ABS area pinned at 0x2100 (in crt0); _CODE
     holds the code and is pinned just AFTER the 208-byte header at 0x21D0 (the
     conventional PM entry; the reset trampoline bjumps to __start here). _DATA is
     near RAM at 0x1000 (RAM spans 0x1000-0x1FFF). Pinned via {z80bases} -> -b. */
  options.code_loc = 0x21D0;
  options.allow_undoc_inst = false;

  options.data_loc = 0x1000;

  options.out_fmt = 'i';        /* Default output format is ihx */
}

#if 0
/* Mangling format:
    _fun_policy_params
    where:
      policy is the function policy
      params is the parameter format

   policy format:
    rsp
    where:
      r is 'r' for reentrant, 's' for static functions
      s is 'c' for callee saves, 'r' for caller saves
      f is 'f' for profiling on, 'x' for profiling off
    examples:
      rr - reentrant, caller saves
   params format:
    A combination of register short names and s to signify stack variables.
    examples:
      bds - first two args appear in BC and DE, the rest on the stack
      s - all arguments are on the stack.
*/
static const char *
_mangleSupportFunctionName (const char *original)
{
  struct dbuf_s dbuf;

  if (strstr (original, "longlong"))
    return (original);

  dbuf_init (&dbuf, 128);
  dbuf_printf (&dbuf, "%s_rr%s_%s", original, options.profile ? "f" : "x", options.noRegParams ? "s" : "bds"    /* MB: but the library only has hds variants ??? */
    );

  return dbuf_detach_c_str (&dbuf);
}
#endif

static const char *
_getRegName (const struct reg_info *reg)
{
  if (reg)
    {
      return reg->name;
    }
  /*  assert (0); */
  return "err";
}

static int
_getRegByName (const char *name)
{
  if (!strcmp (name, "a"))
    return 0;
  if (!strcmp (name, "c"))
    return 1;
  if (!strcmp (name, "b"))
    return 2;
  if (!strcmp (name, "e"))
    return 3;
  if (!strcmp (name, "d"))
    return 4;
  if (!strcmp (name, "l"))
    return 5;
  if (!strcmp (name, "h"))
    return 6;
  if (!strcmp (name, "iyl"))
    return 7;
  if (!strcmp (name, "iyh"))
    return 8;
  return -1;
}

static void
_s1c88_genAssemblerStart (FILE * of)
{
  if (!options.noOptsdccInAsm)
    {
      tfprintf (of, "\t!optsdcc -m%s", port->target);
      fprintf (of, " sdcccall(%d)", options.sdcccall);
      fprintf (of, "\n");
    }
}

static bool
_hasNativeMulFor (iCode *ic, sym_link *left, sym_link *right)
{
  sym_link *test = NULL;
  int result_size = IS_SYMOP (IC_RESULT(ic)) ? getSize (OP_SYM_TYPE (IC_RESULT(ic))) : 4;

  if (IS_BITINT (OP_SYM_TYPE (IC_RESULT(ic))) && SPEC_BITINTWIDTH (OP_SYM_TYPE (IC_RESULT(ic))) % 8)
    return false;

  /* S1C88 native DIV (CE D9, MODEL1/3 — present on the Pokémon Mini core):
     unsigned HL / A -> quotient L, remainder H. Claim an unsigned dividend
     of up to 16 bits against an unsigned 8-bit divisor: 8 / 8 is a single
     DIV (quotient and remainder always fit; V never set), 16 / 8 is the
     two-DIV schoolbook chain (both partial quotients provably fit, since
     the running remainder is < the divisor <= 255). A zero divisor raises
     the hardware zero-division exception (C UB). Signed divisions and
     16-bit divisors stay support calls. (Note: u16 / u8var promotes the
     divisor to unsigned int in C, so the variable-divisor 16 / 8 case
     only claims when the middle end narrows it back — in practice the
     16-bit dividend claims are literal divisors, e.g. /10.) */
  if (ic->op == '/' || ic->op == '%')
    {
      if (!IS_INTEGRAL (left) || getSize (left) > 2)
        return false;
      if (IS_UNSIGNED (left))
        {
          if (IS_CHAR (right) && IS_UNSIGNED (right) && getSize (right) == 1)
            return true;
          if (IS_LITERAL (right))
            {
              unsigned long val = ulFromVal (valFromType (right));
              return (val > 0 && val <= 255);
            }
        }
      /* signed 8 / 8: branchless negate-fixup around the unsigned DIV
         (sep mask abs + mask-applied result; ~17-26 bytes inline), so
         keep the __divschar/__modschar support calls when optimizing
         for code size. Negative literal divisors are rare and stay
         support calls too. */
      else if (!optimize.codeSize && IS_CHAR (left))
        {
          if (IS_CHAR (right) && !IS_UNSIGNED (right) && getSize (right) == 1)
            return true;
          if (IS_LITERAL (right))
            {
              double dval = floatFromVal (valFromType (right));
              return (dval >= 1 && dval <= 127);
            }
        }
      return false;
    }

  if (ic->op != '*')
    return(false);

  if (IS_LITERAL (left))
    test = left;
  else if (IS_LITERAL (right))
    test = right;
  /* 8x8 unsigned multiplication code is shorter than
     call overhead for the multiplication routine. */
  else if (IS_CHAR (right) && IS_UNSIGNED (right) && IS_CHAR (left) && IS_UNSIGNED (left))
    return(true);
  /* Same for any multiplication with 8 bit result. */
  else if (result_size == 1)
    return(true);
  else
    return(false);

  if (getSize (test) <= 2)
    return(true);

  return(false);
}

/* Indicate which extended bit operations this port supports */
static bool
hasExtBitOp (int op, sym_link *left, int right)
{
  switch (op)
    {
    case GETABIT:
    case GETBYTE:
    case GETWORD:
      return (true);
    case ROT:
      {
        unsigned int lbits = bitsForType (left);
        if (lbits % 8)
          return (false);
        if (lbits == 8)
          return (true);
        if (right % lbits  == 1 || right % lbits == lbits - 1)
          return (true);
        if ((getSize (left) <= 2 || getSize (left) == 4) && lbits == right * 2)
          return (true);
      }
      return (false);
    }
  return (false);
}

/* Indicate the expense of an access to an output storage class */
static int
oclsExpense (struct memmap *oclass)
{
  if (IN_FARSPACE (oclass))
    return 1;

  return 0;
}


//#define LINKCMD "sdld{port} -nf {dstfilename}"
/*
#define LINKCMD \
    "sdld{port} -n -c -- {z80bases} -m -j" \
    " {z80libspec}" \
    " {z80extralibfiles} {z80extralibpaths}" \
    " {z80outputtypeflag} \"{linkdstfilename}\"" \
    " {z80crt0}" \
    " \"{dstfilename}{objext}\"" \
    " {z80extraobj}"
*/

static const char *_s1c88LinkCmd[] = {
  "sdldz80", "-nf", "$1", "$L", NULL
};

/* $3 is replaced by assembler.debug_opts resp. port->assembler.plain_opts */
static const char *_s1c88AsmCmd[] = {
  "sdas88", "$l", "$3", "$2", "$1.asm", NULL
};

static const char *const _crt[] = { "crt0.rel", NULL, };
static const char *const _libs_s1c88[] = { "s1c88", NULL, };

/* Globals */
PORT s1c88_port =
{
  TARGET_ID_S1C88,
  "s1c88",
  "Epson S1C88",                /* Target name */
  NULL,                         /* Processor name */
  {
    glue,
    FALSE,
    NO_MODEL,
    NO_MODEL,
    NULL,                       /* model == target */
  },
  {                             /* Assembler */
    _s1c88AsmCmd,
    NULL,
    "-plosgffwy",               /* Options with debug */
    "-plosgffw",                /* Options without debug */
    0,
    ".asm"
  },
  {                             /* Linker */
    _s1c88LinkCmd,                //NULL,
    NULL,                       //LINKCMD,
    NULL,
    ".rel",
    1,                          /* needLinkerScript */
    _crt,                       /* crt */
    _libs_s1c88,                  /* libs */
  },
  {                             /* Peephole optimizer */
    _s1c88_defaultRules,
    s1c88instructionSize,
    0,
    0,
    0,
    s1c88notUsed,
    s1c88canAssign,
    s1c88notUsedFrom,
    s1c88symmParmStack,
    s1c88canJoinRegs,
    s1c88canSplitReg,
  },
  /* Sizes: char, short, int, long, long long, near ptr, far ptr, gptr, func ptr, banked func ptr, bit, float, BitInt (in bits) */
  /* far ptr = 3: 24-bit linear data address (EP page : 16-bit offset) — S1C88 data paging is linear
     (physical = EP*65536 + HL), so byte 2 is simply (addr >> 16) and SDCCglue's stock 3-byte
     initializer emission is natively correct. gptr stays 2 (== near; no runtime-tagged pointers).
     func ptr = 3: a banked code pointer (lo, hi, bank) — code symbols link as (bank<<16)|logic,
     so &f's third byte (sym>>16) IS the bank (the #9 XL3 reloc machinery); the PCALL dispatch is
     `ld nb,<bank>` + `call (__sdcc_fptr)` (the call's CB<-NB latch switches the bank, the 3-byte
     max-mode frame restores it on return). */
  { 1, 2, 2, 4, 8, 2, 3, 2, 3, 3, 1, 4, 64 },
  /* tags for generic pointers */
  { 0x00, 0x40, 0x60, 0x80 },   /* far, near, xstack, code */
  {
    "XSEG",
    "STACK",
    "CODE",
    "DATA",
    NULL,                       /* idata */
    NULL,                       /* pdata */
    "FAR",                      /* xdata = the __far data space (EP-paged, beyond the 64K near window) */
    NULL,                       /* bit */
    "RSEG (ABS)",
    "GSINIT",                   /* static initialization */
    NULL,                       /* overlay */
    "GSFINAL",
    "HOME",
    NULL,                       /* xidata */
    NULL,                       /* xinit */
    NULL,                       /* const_name */
    "CABS (ABS)",               /* cabs_name */
    "DABS (ABS)",               /* xabs_name */
    NULL,                       /* iabs_name */
    "INITIALIZED",              /* name of segment for initialized variables */
    "INITIALIZER",              /* name of segment for copies of initialized variables in code space */
    NULL,
    NULL,
    1,                          /* CODE  is read-only */
    false,                      // unqualified pointers cannot point to __sfr.
    1                           /* No fancy alignments supported. */
  },
  { NULL, NULL },
  1,                            /* ABI revision */
  /* stack: direction, bank_overhead, isr_overhead, call_overhead 5 =
     3-byte CB:PC return frame (S1C88 MAXIMUM mode — every call pushes CB
     and RET pops it; PokeMini-verified) + 2-byte saved IX frame pointer.
     (The inherited value 4 assumed 2-byte minimum-mode frames — wrong for the
     Pokémon Mini, whose 2MB banked ROM requires max mode.) */
  { -1, 0, 0, 5, 0, 3, 0 },
  { 
    -1,                         /* shifts never use support routines */
    false,                      /* int x int -> long: use full __mullong, NOT a __mul*int2*long
                                   widening routine. Those ship only as per-port hand asm (they
                                   conflict with a C definition — SDCC pre-declares them), and the
                                   s1c88 port doesn't provide them; __mullong is already in the lib.
                                   (Re-enable + add asm widening routines for code size — TODO #15.) */
    false,                      /* do not use support routine for unsigned long x unsigned char -> unsigned long long multiplication */
  },
  { s1c88_emitDebuggerSymbol },
  {
    256,                        /* maxCount */
    3,                          /* sizeofElement */
    {6, 7, 8},                  /* sizeofMatchJump[] - Assumes operand allocated to registers */
    {6, 9, 15},                 /* sizeofRangeCompare[] - Assumes operand allocated to registers*/
    1,                          /* sizeofSubtract - Assumes use of a singel inc or dec */
    9,                          /* sizeofDispatch - Assumes operand allocated to register e or c*/
  },
  "_",
  _s1c88_init,
  _parseOptions,
  _s1c88_options,
  NULL,
  _finaliseOptions,
  _setDefaultOptions,
  s1c88_assignRegisters,
  _getRegName,
  _getRegByName,
  NULL,
  _keywords,
  _s1c88_genAssemblerStart,
  NULL,                         /* no genAssemblerEnd */
  0,                            /* no local IVT generation code */
  0,                            /* no genXINIT code */
  NULL,                         /* genInitStartup */
  _reset_regparm,
  _reg_parm,
  _process_pragma,
  NULL,
  _hasNativeMulFor,
  hasExtBitOp,                  /* hasExtBitOp */
  oclsExpense,                  /* oclsExpense */
  TRUE,
  TRUE,                         /* little endian */
  0,                            /* leave lt */
  0,                            /* leave gt */
  1,                            /* transform <= to ! > */
  1,                            /* transform >= to ! < */
  1,                            /* transform != to !(a == b) */
  0,                            /* leave == */
  FALSE,                        /* Array initializer support. */
  0,                            /* no CSE cost estimation yet */
  _s1c88_builtins,                /* builtin functions */
  GPOINTER,                     /* treat unqualified pointers as "generic" pointers */
  1,                            /* reset labelKey to 1 */
  1,                            /* globals & local statics allowed */
  4,                            /* num_regs: S1C88 byte GPRs A,B,L,H (tree-decomposition allocator in SDCCralloc.hpp) */
  PORT_MAGIC
};

