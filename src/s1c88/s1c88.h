/** @file z80/z80.h
    Common definitions for the the z80-related ports.
*/
#include "common.h"
#include "ralloc.h"
#include "gen.h"
#include "peep.h"
#include "support.h"

typedef enum
  {
    SUB_Z80,
    SUB_Z180,
    SUB_R2K,
    SUB_R2KA,
    SUB_R3KA,
    SUB_SM83,
    SUB_TLCS90,
    SUB_EZ80_Z80,
    SUB_Z80N,
    SUB_R800
  }
Z80_SUB_PORT;

typedef struct
  {
    Z80_SUB_PORT sub;
    int calleeSavesBC;
    int port_mode;
    int port_back;
    int reserveIY;
    int noOmitFramePtr;
    int legacyBanking;
    int nmosZ80;
  }
Z80_OPTS;

extern Z80_OPTS s1c88_opts;

/* sdcc88 is a SINGLE-VARIANT port: main.c fixes s1c88_opts.sub = SUB_Z80 once and
   nothing ever changes it (the other -mz80 variants are not built). So the
   sub-port predicates are compile-time constants — IS_Z80 always true, every
   other variant always false. Making them literal lets the compiler dead-code-
   eliminate the ~hundreds of unreachable z80-variant branches (SM83/Rabbit/
   TLCS90/eZ80/Z80N/R800 code, much of which carries z80-only DE/BC idioms with
   no S1C88 analog) instead of compiling them in. Verified byte-identical
   codegen across the corpus — these branches never executed. */
#define IS_Z80 1
#define IS_Z180 0
#define IS_R2K 0
#define IS_R2KA 0
#define IS_R3KA 0
#define IS_RAB 0
#define IS_SM83 0
#define IS_TLCS90 0
#define IS_EZ80_Z80 0
#define IS_Z80N 0
#define IS_R800 0
#define HAS_IYL_INST (options.allow_undoc_inst)

#define IY_RESERVED (s1c88_opts.reserveIY)

#define OPTRALLOC_IY !(IY_RESERVED || IS_SM83)

enum
  {
    ACCUSE_A = 1,
    ACCUSE_SCRATCH,
    ACCUSE_IY
  };

