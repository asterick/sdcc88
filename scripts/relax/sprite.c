/* sprite.c — a self-contained sprite/actor update loop.
 *
 * Models the shape of real Pokémon Mini game code: an actor table, a per-frame
 * update that dispatches through several small handlers, simple AABB collision,
 * and a fixed number of simulated frames. Lots of intra-module calls of varied
 * reach — the point is to give the #14a relaxation analysis realistic branch
 * displacements. Self-contained (no headers); console at 0x1FF8.
 */

#define CONSOLE (*(volatile unsigned char *)0x1FF8)
#define NACT 6

struct actor {
    signed char x, y;
    signed char vx, vy;
    unsigned char kind;
    unsigned char alive;
};

static struct actor act[NACT];

static signed char clampc(signed char v, signed char lo, signed char hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static unsigned char overlap(struct actor *a, struct actor *b)
{
    signed char dx = (signed char)(a->x - b->x);
    signed char dy = (signed char)(a->y - b->y);
    if (dx < 0) dx = (signed char)-dx;
    if (dy < 0) dy = (signed char)-dy;
    return (unsigned char)(dx < 8 && dy < 8);
}

static void move(struct actor *a)
{
    a->x = clampc((signed char)(a->x + a->vx), -96, 96);
    a->y = clampc((signed char)(a->y + a->vy), -64, 64);
    if (a->x == -96 || a->x == 96) a->vx = (signed char)-a->vx;
    if (a->y == -64 || a->y == 64) a->vy = (signed char)-a->vy;
}

static void hurt(struct actor *a, struct actor *b)
{
    if (a->kind == b->kind)
        return;
    if (overlap(a, b)) {
        a->alive = 0;
        b->vx = (signed char)-b->vx;
    }
}

static void spawn(unsigned char i)
{
    struct actor *a = &act[i];
    a->x = (signed char)(i * 13 - 32);
    a->y = (signed char)(i * 7 - 16);
    a->vx = (signed char)((i & 1) ? 3 : -2);
    a->vy = (signed char)((i & 2) ? -1 : 2);
    a->kind = (unsigned char)(i & 1);
    a->alive = 1;
}

static unsigned char step(void)
{
    unsigned char i, j, live = 0;
    for (i = 0; i < NACT; i++) {
        if (!act[i].alive)
            continue;
        move(&act[i]);
        for (j = 0; j < NACT; j++)
            if (i != j && act[j].alive)
                hurt(&act[i], &act[j]);
    }
    for (i = 0; i < NACT; i++)
        live = (unsigned char)(live + act[i].alive);
    return live;
}

static void put_dec(unsigned char v)
{
    if (v >= 10)
        put_dec((unsigned char)(v / 10));
    CONSOLE = (unsigned char)('0' + v % 10);
}

int main(void)
{
    unsigned char f, live;
    for (f = 0; f < NACT; f++)
        spawn(f);
    for (f = 0; f < 40; f++) {
        live = step();
        if (live <= 1)
            break;
    }
    put_dec(live);
    CONSOLE = '\n';
    return 0;
}
