/* builtin cluster: genBuiltInMemset / Strcpy / Strncpy / Strchr (native byte
   loops — HL=src/scan, IY=dst, B or borrowed-IX counter) + the s5 memcpy.
   These were a corpus blind spot: the z80 originals emitted ldi/ldir, DE, and
   jp PO/PE parity branches. */

char buf[600];
char src[600];

/* --- memset: straight-line (n <= 4), literal fill --- */
void ms_tiny (void) { __builtin_memset (buf, 0x55, 3); }

/* --- memset: B-counter loop, literal fill (ld (hl), #nn) --- */
void ms_loop (void) { __builtin_memset (buf, 0, 40); }

/* --- memset: variable fill value (through A) --- */
void ms_var (int c) { __builtin_memset (buf, c, 7); }

/* --- memset: wide (> 255, borrowed-IX counter) --- */
void ms_wide (void) { __builtin_memset (buf, 0xaa, 600); }

/* --- memset: fill value parked in a live register across the call --- */
char ms_keep (char c)
{
  __builtin_memset (buf, c, 10);
  return c;                      /* c must survive the counter/fill regs */
}

/* --- strcpy: result unused --- */
void sc_plain (void) { __builtin_strcpy (buf, src); }

/* --- strcpy: result used (returned original dst from the saved IY) --- */
char *sc_ret (void) { return __builtin_strcpy (buf, src); }

/* --- strncpy: B counter + zero-pad tail --- */
void sn_loop (void) { __builtin_strncpy (buf, src, 10); }

/* --- strncpy: wide (> 255, borrowed-IX counter) --- */
void sn_wide (void) { __builtin_strncpy (buf, src, 300); }

/* --- strchr: literal char (cp a,#nn) , result used --- */
char *sr_lit (void) { return __builtin_strchr (src, 'x'); }

/* --- strchr: literal NUL (must return the terminator pointer) --- */
char *sr_nul (void) { return __builtin_strchr (src, 0); }

/* --- strchr: variable char (through B, cp a,b) --- */
char *sr_var (int c) { return __builtin_strchr (src, c); }

/* --- memcpy regression (s5): the shared HL/IY setup was deduped --- */
void mc_small (void) { __builtin_memcpy (buf, src, 16); }
void mc_wide (void) { __builtin_memcpy (buf, src, 300); }
