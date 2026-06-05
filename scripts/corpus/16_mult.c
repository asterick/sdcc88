/* multiplication cluster: genMult literal paths (BA/B CSD chains, byte loops,
   byte-granular A/B saves) + genMultOneChar variable 8x8 (native MLT) */

/* --- literal: 16-bit add_in_hl loop (add hl,ba) --- */
int mul_lit3(int a) { return a * 3; }
int mul_lit10(int a) { return a * 10; }
int mul_lit100(int a) { return a * 100; }
unsigned int mul_lit6u(unsigned int a) { return a * 6; }

/* --- literal: byte CSD accumulator loop (add/sub a,b) --- */
char mul_litc5(char c) { return (char)(c * 5); }
char mul_litc7(char c) { return (char)(c * 7); }              /* CSD: 8c - c */
unsigned char mul_litc10(unsigned char c) { return (unsigned char)(c * 10); }

/* --- literal: live byte regs across the multiply (push a / push b) --- */
char mul_lit_liveb(char c, char d) { return (char)(c * 3 + d); }
int mul_lit_livea(char a, int b) { return b * 3 + a; }
int mul_lit_pressure(int a, int b, int c) { return a * 3 + b * 5 + c; }

/* --- literal: widened signed/unsigned char operand --- */
int mul_lit_sc(signed char c) { return c * 10; }
unsigned int mul_lit_uc(unsigned char c) { return c * 6u; }

/* --- literal: store to global (result genMove paths) --- */
int gmi;
char gmc;
void mul_lit_store(int a) { gmi = a * 3; }
void mul_lit_storec(char c) { gmc = (char)(c * 5); }

/* --- variable 8x8 -> MLT (hl <- l * a) --- */
char mul_cc(char a, char b) { return (char)(a * b); }
int mul_cc16(char a, char b) { return a * b; }
unsigned int mul_ucuc(unsigned char a, unsigned char b) { return a * b; }
char mul_cg(char x) { return (char)(x * gmc); }
char mul_cc_keep(char a, char b, char c) { return (char)((char)(b * c) + a); }

/* --- variable 16x16 / 32x32 -> support calls (bcall __mulint/__mullong) --- */
int mul_ii(int a, int b) { return a * b; }
long mul_ll(long a, long b) { return a * b; }
