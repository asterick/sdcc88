/* pointer deref, pointer arithmetic, pointer-to-pointer */
char *cp; int *ip; long *lp;

char get_c(char *p) { return *p; }
void set_c(char *p, char v) { *p = v; }
int get_i(int *p) { return *p; }
void set_i(int *p, int v) { *p = v; }
long get_l(long *p) { return *p; }
void set_l(long *p, long v) { *p = v; }

int idx_i(int *p, int i) { return p[i]; }
void idx_set(int *p, int i, int v) { p[i] = v; }
char *inc_p(char *p) { return p + 1; }
int diff_p(int *a, int *b) { return a - b; }

int deref2(int **pp) { return **pp; }
void swap_via_ptr(int *a, int *b) { int t = *a; *a = *b; *b = t; }

void walk(char *p, int n) { while (n--) { *p = 0; p++; } }
