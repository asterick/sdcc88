/* varargs */
typedef char *va_list;

int sum_va(int n, ...) {
  va_list ap = (va_list)(&n + 1);
  int total = 0;
  while (n--) { total += *(int *)ap; ap += 2; }
  return total;
}

extern int printf_like(const char *fmt, ...);
void use_va(void) { printf_like("x", 1, 2, 3); }
