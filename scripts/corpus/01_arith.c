/* integer/long/char arithmetic, shifts, mixed widths */
unsigned char uc; signed char sc;
unsigned int ui; int si;
unsigned long ul; long sl;

int add_i(int a, int b) { return a + b; }
int sub_i(int a, int b) { return a - b; }
long add_l(long a, long b) { return a + b; }
long sub_l(long a, long b) { return a - b; }
int mul_i(int a, int b) { return a * b; }
int div_i(int a, int b) { return a / b; }
int mod_i(int a, int b) { return a % b; }
unsigned div_u(unsigned a, unsigned b) { return a / b; }
long mul_l(long a, long b) { return a * b; }

int shl_i(int a, int n) { return a << n; }
int shr_i(int a, int n) { return a >> n; }
unsigned shr_u(unsigned a, int n) { return a >> n; }
long shl_l(long a, int n) { return a << n; }
char shl_c(char a, int n) { return a << n; }

void acc(void) { ui = ui + 3; si = si - 7; ul += 9; sl -= 11; uc++; sc--; }
