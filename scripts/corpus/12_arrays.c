/* arrays, loops, memcpy-style copies */
int arr[16];
char buf[32];

int sum_arr(void) { int i, s = 0; for (i = 0; i < 16; i++) s += arr[i]; return s; }
void fill(int v) { int i; for (i = 0; i < 16; i++) arr[i] = v; }
void clear_buf(void) { int i; for (i = 0; i < 32; i++) buf[i] = 0; }

void mcpy(char *d, char *s, int n) { while (n--) *d++ = *s++; }
void mcpy16(char *d, char *s) { __builtin_memcpy(d, s, 16); }
void mcpyn(char *d, char *s, unsigned n) { __builtin_memcpy(d, s, n); }

int find(int *p, int n, int key) {
  int i;
  for (i = 0; i < n; i++) if (p[i] == key) return i;
  return -1;
}
