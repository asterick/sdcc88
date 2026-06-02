/* bitwise / logical ops on char/int/long */
unsigned char and_c(unsigned char a, unsigned char b) { return a & b; }
unsigned char or_c(unsigned char a, unsigned char b) { return a | b; }
unsigned char xor_c(unsigned char a, unsigned char b) { return a ^ b; }
unsigned char not_c(unsigned char a) { return ~a; }
int and_i(int a, int b) { return a & b; }
int or_i(int a, int b) { return a | b; }
int xor_i(int a, int b) { return a ^ b; }
long and_l(long a, long b) { return a & b; }
long or_l(long a, long b) { return a | b; }

int land(int a, int b) { return a && b; }
int lor(int a, int b) { return a || b; }
int lnot(int a) { return !a; }
int neg_i(int a) { return -a; }
long neg_l(long a) { return -a; }

unsigned setbits(unsigned x) { return (x | 0x0f0f) & 0xff00; }
