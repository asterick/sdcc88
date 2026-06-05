/** @file z80/z80.h
    Common definitions for the the z80-related ports.
*/
#include "common.h"
#include "ralloc.h"
#include "gen.h"
#include "peep.h"
#include "support.h"

typedef struct
  {
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

/* sdcc88 is a SINGLE-VARIANT port: the z80 sub-port predicate machinery
   (IS_Z80/IS_SM83/IS_RAB/...) is GONE — every variant-gated branch was
   constant-folded away (task #7a). HAS_IYL_INST survives only until the
   IYL/IYH_IDX removal (#7c): the S1C88 IX/IY are not byte-addressable. */
#define HAS_IYL_INST (options.allow_undoc_inst)

#define IY_RESERVED (s1c88_opts.reserveIY)

#define OPTRALLOC_IY !(IY_RESERVED)

enum
  {
    ACCUSE_A = 1,
    ACCUSE_SCRATCH,
    ACCUSE_IY
  };

