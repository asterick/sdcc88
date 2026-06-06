/* division cluster: genDivMod native DIV (CE D9, unsigned HL/A -> L quot, H rem)
   - 8/8 single DIV, 16/8 two-DIV schoolbook chain - plus the unclaimed shapes
   that must stay support calls (signed, 16-bit divisor, 32-bit) */

/* --- unsigned 8/8: single DIV, quotient (L) and remainder (H) --- */
unsigned char div_uu(unsigned char a, unsigned char b) { return a / b; }
unsigned char mod_uu(unsigned char a, unsigned char b) { return a % b; }
unsigned int div_uu16(unsigned char a, unsigned char b) { return a / b; }   /* widened result: H<-0 + genMove */

/* --- unsigned 8/lit: divisor staged as an immediate --- */
unsigned char div_u10(unsigned char a) { return a / 10; }
unsigned char mod_u10(unsigned char a) { return a % 10; }
unsigned char div_lit200(unsigned char x) { return 200 / x; }               /* literal dividend */

/* --- live regs across the divide (byte-granular push a; B survives DIV) --- */
unsigned char div_keep(unsigned char a, unsigned char b, unsigned char c) { return a / b + c; }
unsigned char div_chain(unsigned char a, unsigned char b, unsigned char c) { return a / b / c; }

/* --- both results of one division (two DIVs; B holds an operand across) --- */
unsigned char gdq, gdr;
void divmod_uu(unsigned char a, unsigned char b) { gdq = a / b; gdr = a % b; }

/* --- memory / pointer operands (staging order + requiresHL bounce) --- */
unsigned char gdu, gdv;
unsigned char div_gg(void) { return gdu / gdv; }
void mod_gg(void) { gdr = gdu % gdv; }
unsigned char div_pp(unsigned char *p, unsigned char *q) { return *p / *q; }

/* --- quotient feeding a compare --- */
unsigned char div_cond(unsigned char a, unsigned char b) { if (a / b > 3) return 1; return 0; }

/* --- unsigned 16/lit: the two-DIV chain (qhi push/pop only for wide '/') --- */
unsigned int div16_10(unsigned int x) { return x / 10; }
unsigned char mod16_10(unsigned int x) { return x % 10; }                   /* no qhi push */
unsigned int mod16_w(unsigned int x) { return x % 100; }
unsigned int gdx;
unsigned int div16_g(void) { return gdx / 100; }
unsigned int div16_keepb(unsigned int x, unsigned char c) { return x / 10 + c; }  /* save_b around the chain */

/* --- binary-to-decimal: the chain's workhorse shape --- */
void dec3(unsigned int v, unsigned char *out) {
  out[2] = (unsigned char)(v % 10); v /= 10;
  out[1] = (unsigned char)(v % 10); v /= 10;
  out[0] = (unsigned char)(v % 10);
}

/* --- signed 8/8: branchless sep-mask negate-fixup around DIV (not under --opt-code-size) --- */
signed char div_ss(signed char a, signed char b) { return a / b; }
signed char mod_ss(signed char a, signed char b) { return a % b; }
int div_ss16(signed char a, signed char b) { return a / b; }                /* sep-extended wide result */
signed char div_ss_keep(signed char a, signed char b, signed char c) { return a / b + c; }
signed char gsq, gsr;
void divmod_ss(signed char a, signed char b) { gsq = a / b; gsr = a % b; } /* save_b across the first cluster */

/* --- unclaimed: support calls (bcall __divsint/__divuint/__divulong/__moduint;
       NB signed literal divisors widen to int in the middle end, so sc/10 stays __divsint) --- */
signed char div_s10(signed char a) { return a / 10; }
int div_ii(int a, int b) { return a / b; }
unsigned int div_uu16v(unsigned int a, unsigned int b) { return a / b; }    /* 16-bit divisor */
unsigned int mod_uu16v(unsigned int a, unsigned int b) { return a % b; }
unsigned long div_ull(unsigned long a, unsigned long b) { return a / b; }
