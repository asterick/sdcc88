/* direct calls, many-arg calls, calls returning various widths */
extern int ext_i(int);
extern long ext_l(long, long);
extern char ext_c(char, char, char);
extern void ext_v(int, int, int, int);

int call1(int x) { return ext_i(x) + 1; }
long call2(long a, long b) { return ext_l(a, b); }
char call3(char a) { return ext_c(a, a, a); }
void call4(void) { ext_v(1, 2, 3, 4); }

int chain(int x) { return ext_i(ext_i(ext_i(x))); }
long mixed(int x, long y) { return ext_i(x) + ext_l(y, y); }

int nested(int a, int b, int c, int d, int e) {
  return ext_i(a) + ext_i(b) + ext_i(c) + ext_i(d) + ext_i(e);
}

/* tail call */
int tail(int x) { return ext_i(x); }
