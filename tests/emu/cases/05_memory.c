/* 05_memory — pointers, arrays, structs, bitfields, and the native byte-loop
 * block copies that replaced ldir (s5/s14). */
#include "emu.h"

int garr[8] = {10, 20, 30, 40, 50, 60, 70, 80};
char gstr[] = "s1c88";
unsigned char dst[8];

struct flags {
    unsigned a : 1;
    unsigned b : 3;
    unsigned c : 4;
    unsigned wide : 10;
};

volatile struct flags fl;

struct node {
    int val;
    struct node *next;
};

struct node n2 = {22, 0};
struct node n1 = {11, &n2};

int main(void)
{
    unsigned char i;
    int *ip;
    int acc = 0;

    /* array indexing, runtime subscript */
    for (i = 0; i < 8; i++)
        acc += garr[i];
    CHECK(acc == 360);
    CHECK(garr[3] == 40);

    /* pointer walk + writeback */
    ip = garr;
    ip += 2;
    CHECK(*ip == 30);
    *ip = -30;
    CHECK(garr[2] == -30);

    /* global<->global block copy (the byte loop, not ldir) */
    for (i = 0; i < 6; i++)
        dst[i] = (unsigned char)gstr[i];
    CHECK(dst[0] == 's');
    CHECK(dst[4] == '8');
    CHECK(dst[5] == 0);

    /* linked struct deref chains */
    CHECK(n1.next->val == 22);
    n1.next->val = 33;
    CHECK(n2.val == 33);

    /* bitfields (and/or masks — no RES/SET on S1C88) */
    fl.a = 1;
    fl.b = 5;
    fl.c = 9;
    fl.wide = 777;
    CHECK(fl.a == 1);
    CHECK(fl.b == 5);
    CHECK(fl.c == 9);
    CHECK(fl.wide == 777);
    fl.b = 0;
    CHECK(fl.a == 1 && fl.b == 0 && fl.c == 9);

    EMU_DONE();
}
