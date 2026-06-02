/* swaps, register shuffles, in-place ops that stress the allocator */
int gi1, gi2;
long gl1, gl2;

void swap_i(void) { int t = gi1; gi1 = gi2; gi2 = t; }
void swap_l(void) { long t = gl1; gl1 = gl2; gl2 = t; }
void swap_args(int *a, int *b) { int t = *a; *a = *b; *b = t; }

int rot3(int a, int b, int c) { return (a << 1) ^ (b << 2) ^ (c << 3); }
long combine(int hi, int lo) { return ((long)hi << 16) | (unsigned)lo; }
int hilo(long x) { return (int)(x >> 16) + (int)x; }

void multi(int a, int b, int c, int d) {
  gi1 = a + b; gi2 = c + d; gl1 = (long)a * b; gl2 = (long)c * d;
}
