/* struct member access, struct copy/assign, return-by-value */
struct point { int x; int y; };
struct big { char name[8]; int a; long b; };

int getx(struct point *p) { return p->x; }
void setx(struct point *p, int v) { p->x = v; }
int sumxy(struct point *p) { return p->x + p->y; }

struct point gp;
void copyp(struct point *d, struct point *s) { *d = *s; }
struct point makep(int x, int y) { struct point p; p.x = x; p.y = y; return p; }
struct point dupp(struct point *s) { return *s; }
struct point readg(void) { return gp; }

struct big gb;
void copyb(struct big *d) { *d = gb; }
long bget(struct big *p) { return p->b; }

struct point passp(struct point p) { p.x++; return p; }
