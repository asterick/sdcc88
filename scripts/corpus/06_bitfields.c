/* bitfield load/store, packing */
struct flags {
  unsigned a : 1;
  unsigned b : 3;
  unsigned c : 4;
  unsigned d : 8;
};
struct flags gf;

unsigned geta(struct flags *f) { return f->a; }
unsigned getb(struct flags *f) { return f->b; }
unsigned getc(struct flags *f) { return f->c; }
void seta(struct flags *f, unsigned v) { f->a = v; }
void setb(struct flags *f, unsigned v) { f->b = v; }
void setc(struct flags *f, unsigned v) { f->c = v; }
void setd(struct flags *f, unsigned v) { f->d = v; }

void setall(struct flags *f) { f->a = 1; f->b = 5; f->c = 9; f->d = 200; }
unsigned sumbf(struct flags *f) { return f->a + f->b + f->c + f->d; }
