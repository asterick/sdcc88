/* function pointers: direct, struct member, table-indexed */
typedef int (*fp_t)(int);
typedef void (*vp_t)(void);

int via_ptr(fp_t f, int x) { return f(x); }
void via_void(vp_t f) { f(); }

struct vtable { int (*op)(int, int); void (*reset)(void); };
int call_member(struct vtable *v, int a, int b) { return v->op(a, b); }
void reset_member(struct vtable *v) { v->reset(); }

fp_t table[4];
int dispatch(int i, int x) { return table[i](x); }

int tail_ptr(fp_t f, int x) { return f(x); }
