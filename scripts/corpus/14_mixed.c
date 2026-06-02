/* mixed realistic code: switch, nested loops, local arrays, casts */
unsigned char state;

int fsm(unsigned char ev) {
  switch (ev) {
    case 0: state = 1; return 10;
    case 1: state = 2; return 20;
    case 2: state = 0; return 30;
    case 5: state = 5; return 50;
    default: return -1;
  }
}

int matrix(void) {
  int m[4];
  int i, j, s = 0;
  for (i = 0; i < 4; i++) m[i] = i;
  for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) s += m[i] * m[j];
  return s;
}

unsigned crc(unsigned char *p, int n) {
  unsigned c = 0xffff;
  while (n--) { c ^= *p++; c = (c >> 1) ^ (c & 1 ? 0xa001 : 0); }
  return c;
}

long accumulate(int *p, int n) {
  long sum = 0;
  while (n--) sum += *p++;
  return sum;
}
