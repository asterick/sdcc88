/* comparisons across widths/signedness/senses, both ifx and bool result */
int si;
int lt_i(int a, int b) { return a < b; }
int gt_i(int a, int b) { return a > b; }
int le_i(int a, int b) { return a <= b; }
int ge_i(int a, int b) { return a >= b; }
int eq_i(int a, int b) { return a == b; }
int ne_i(int a, int b) { return a != b; }
int lt_u(unsigned a, unsigned b) { return a < b; }
int gt_u(unsigned a, unsigned b) { return a > b; }
int lt_l(long a, long b) { return a < b; }
int eq_l(long a, long b) { return a == b; }
int lt_c(signed char a, signed char b) { return a < b; }

int litcmp(int a) { if (a < 100) return 1; if (a > -50) return 2; return 3; }
int lcmp_lit(long a) { if (a < 1000L) return 1; return 0; }

void branchy(int a, int b) {
  if (a < b) { si = 1; } else if (a == b) { si = 2; } else { si = 3; }
}
