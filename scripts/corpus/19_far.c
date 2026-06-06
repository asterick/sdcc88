/* 19_far.c - task #9: 3-byte __far pointers (EP:offset, 24-bit linear). */
/* Covers: far objects (const ROM table), 3-byte pointer initializers */
/* (symbol + literal), far deref read/write of char and int, displacement */
/* (p[3]), indexed far table read (24-bit symbolic address arithmetic with */
/* the ((sym) >> 16) page byte), far reads/writes against globals */
/* (EP-toggle walks), literal-address far access, and near pointers */
/* coexisting (still 2 bytes, no tag bytes).  Pre-cpp'd, no includes. */

char __far *p;
char __far *q;
int __far *ip;
const char __far ftbl[8] = {1, 2, 3, 4, 5, 6, 7, 8};
char __far fbuf[4];
char buf[2];
char __far *fp = (char __far *)ftbl;
char *np = buf;
int __far *lp = (int __far *)0x12345;

char g;
int gi;

/* register-result far reads */
char rd1(void) { return *p; }
int rdi(void) { return *ip; }

/* register-value far writes */
void wr1(char c) { *p = c; }
void wri(int v) { *ip = v; }

/* constant displacement (add hl,#imm inside the EP window) */
char rdo(void) { return p[3]; }
void wro(char c) { p[5] = c; }

/* absolute (EP-paged) result/value: the EP-toggle walks */
void glob_rd(void) { g = *p; }
void glob_wr(void) { *p = g; }
void glob_rdi(void) { gi = *ip; }
void glob_wri(void) { *ip = gi; }

/* 24-bit symbolic address arithmetic: page byte = #((sym) >> 16) */
char tbl_rd(unsigned char i) { return ftbl[i]; }
void tbl_wr(unsigned char i, char v) { fbuf[i] = v; }

/* literal far address (page from the literal, not a GP tag) */
void lit_wr(void) { *(char __far *)0x23456 = 7; }
char lit_rd(void) { return *(char __far *)0x23456; }

/* literal value through a far pointer (ld (hl), #nn) */
void lit_val(void) { *p = 42; }
void lit_vali(void) { *ip = 0x1234; }

/* far pointer as argument / assignment plumbing (3-byte moves) */
void setp(char __far *q) { p = q; }
char rdp(char __far *q) { return *q; }
void chain(char __far *q) { p = q; *p = 1; }

/* near pointer still works untouched alongside */
char nrd(void) { return *np; }

/* casts: near->far widens with page 0; far->near truncates */
void cast_n2f(void) { p = (char __far *)np; }
void cast_f2n(void) { np = (char *)p; }

/* far-pointer returns: offset in HL, page in A (Epson HLP) */
char __far *ret_p(void) { return p; }
char __far *ret_tbl(void) { return (char __far *)ftbl; }
char __far *ret_arr(void) { return (char __far *)buf; }
char __far *ret_off(void) { return p + 2; }
char use_ret(void) { return *ret_tbl(); }

/* 24-bit pointer arithmetic: signed offsets must reach the page byte */
void inc_p(void) { p++; }
void dec_p(void) { p--; }
void add_si(int n) { p += n; }
void sub_si(int n) { p -= n; }
void add_ui(unsigned int n) { p += n; }

/* 3-byte compares */
unsigned char eq(void) { return p == q; }
unsigned char ne0(void) { return p != 0; }
unsigned char lt(void) { return p < q; }
int pdiff(void) { return (int)(q - p); }

/* far pointer <-> long */
unsigned long f2l(void) { return (unsigned long)p; }
void l2f(unsigned long v) { p = (char __far *)v; }

/* ISR EP hygiene: the handler saves+zeroes EP (the interrupt may hit inside
   a far-access window), does its own far access, restores EP before rete */
char isr_sink;
void isr_far(void) __interrupt(1) { isr_sink = *p; }
