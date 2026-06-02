/* deep stack usage, many locals/args -> spills, stack-word peeks, frame addressing */
extern int sink(int);

int many_locals(int a, int b, int c) {
  int l0 = a + 1, l1 = b + 2, l2 = c + 3;
  int l3 = l0 * l1, l4 = l1 * l2, l5 = l2 * l0;
  int l6 = l3 + l4 + l5;
  int l7 = sink(l6) + l0 + l1 + l2 + l3 + l4 + l5;
  return l7;
}

int stacked_args(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

long stacked_long(long a, long b, long c) {
  return a + b + c;
}

int recurse(int n) { if (n <= 1) return 1; return n * recurse(n - 1); }

int local_array_addr(int i) {
  int tmp[8];
  int k;
  for (k = 0; k < 8; k++) tmp[k] = sink(k);
  return tmp[i & 7];
}
