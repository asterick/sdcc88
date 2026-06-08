/** @file s1c88.h
    Common definitions for the Epson S1C88 port (derived from the z80 port).
*/
#include "common.h"
#include "ralloc.h"
#include "gen.h"
#include "peep.h"
#include "support.h"

typedef struct
  {
    int calleeSavesBC;
    int reserveIY;
    int noOmitFramePtr;
    int legacyBanking;
  }
S1C88_OPTS;

extern S1C88_OPTS s1c88_opts;

/* sdcc88 is a SINGLE-VARIANT port: the inherited multi-variant sub-port
   predicate machinery is GONE — every variant-gated branch was
   constant-folded away (task #7a).
   HAS_IYL_INST gated the eZ80 byte-addressable index-register instructions
   (`ld iyl,…` etc.); the S1C88 IX/IY are NOT byte-addressable, so it is
   hardcoded 0. (It was previously tied to `--allow-undoc-inst`, which would
   have switched on invalid IX/IY-byte codegen — a latent footgun.) IYL_IDX/
   IYH_IDX remain as ASMOP_IY's byte ordinals; only the eZ80 *instructions*
   are gone. */
#define HAS_IYL_INST 0

#define IY_RESERVED (s1c88_opts.reserveIY)

#define OPTRALLOC_IY !(IY_RESERVED)

enum
  {
    ACCUSE_A = 1,
    ACCUSE_SCRATCH,
    ACCUSE_IY
  };

