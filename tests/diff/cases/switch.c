/* switch — switch-statement lowering, differential vs host.
 *
 * The middle end picks the lowering (dense -> jump table via genJumpTab: HL =
 * &table + 2*selector, load the 16-bit target, jp (hl); sparse -> if/else chain),
 * inserts the [min,max] range check, and handles default / fall-through. This
 * sweeps each shape across its WHOLE selector domain — every in-range case, both
 * boundaries, and out-of-range below/above -> default — so the range check, the
 * table index scaling, and the default edge are all exercised.
 *
 * SOUNDNESS: switch compares the selector against same-typed case constants
 * (exact equality / range), identical on host (32-bit int) and target (16-bit
 * int) for values that fit 16 bits; every result is truncated to its emitted
 * width. Selector and case labels share signedness (no mixed-sign compare).
 */
#include "harness.h"

/* dense 0..7 from zero -> straight jump table, no offset subtract */
static u8 dense0(u8 x)
{
    switch (x) {
    case 0: return 0xA0;
    case 1: return 0xA1;
    case 2: return 0xA2;
    case 3: return 0xA3;
    case 4: return 0xA4;
    case 5: return 0xA5;
    case 6: return 0xA6;
    case 7: return 0xA7;
    default: return 0xEE;
    }
}

/* dense but offset 100..104 -> table after a min-subtract + range check */
static u8 dense_off(u8 x)
{
    switch (x) {
    case 100: return 0xB0;
    case 101: return 0xB1;
    case 102: return 0xB2;
    case 103: return 0xB3;
    case 104: return 0xB4;
    default:  return 0xEE;
    }
}

/* sparse -> if/else chain (gaps too wide for a table) */
static u16 sparse(u16 x)
{
    switch (x) {
    case 0:     return 0x1000;
    case 7:     return 0x1007;
    case 64:    return 0x1040;
    case 255:   return 0x10FF;
    case 1000:  return 0x13E8;
    case 0xFFFF:return 0x1FFF;
    default:    return 0xDEAD;
    }
}

/* signed selector incl. negative cases */
static i16 signed_sw(i16 x)
{
    switch (x) {
    case -3: return -300;
    case -1: return -100;
    case 0:  return 0;
    case 2:  return 200;
    case 5:  return 500;
    default: return -9999;
    }
}

/* wide (16-bit) dense block high in the range -> 16-bit index scaling */
static u16 wide_dense(u16 x)
{
    switch (x) {
    case 0x1000: return 0xC000;
    case 0x1001: return 0xC001;
    case 0x1002: return 0xC002;
    case 0x1003: return 0xC003;
    case 0x1004: return 0xC004;
    case 0x1005: return 0xC005;
    default:     return 0x0BAD;
    }
}

/* fall-through: grouped labels + a case that falls into the next */
static u8 fallthrough(u8 x)
{
    u8 acc = 0;
    switch (x) {
    case 1:
    case 2:
    case 3:
        acc += 0x10;       /* 1,2,3 all add 0x10 ... */
        /* fall through */
    case 4:
        acc += 0x04;       /* ... and 1,2,3,4 all add 0x04 */
        break;
    case 5:
        acc = 0x55;
        break;
    default:
        acc = 0xFF;
    }
    return acc;
}

/* no default: out-of-range leaves the value untouched */
static u8 no_default(u8 x)
{
    u8 r = 0x77;
    switch (x) {
    case 10: r = 0x0A; break;
    case 11: r = 0x0B; break;
    case 12: r = 0x0C; break;
    }
    return r;
}

void diff_run(void)
{
    u16 i;

    /* dense0 + dense_off + fallthrough + no_default: sweep 0..130 (covers
       boundaries 0/7, 100/104, the 1..5 group, 10..12, and out-of-range gaps) */
    for (i = 0; i <= 130; i++) {
        EMIT_U8("d0",  dense0((u8)i));
        EMIT_U8("do",  dense_off((u8)i));
        EMIT_U8("ft",  fallthrough((u8)i));
        EMIT_U8("nd",  no_default((u8)i));
    }

    /* sparse + wide_dense: sweep a window that brackets every case + boundaries */
    for (i = 0; i <= 70; i++)
        EMIT_U16("sp", sparse(i));
    EMIT_U16("sp255",  sparse(255));
    EMIT_U16("sp1000", sparse(1000));
    EMIT_U16("spmax",  sparse(0xFFFF));

    for (i = 0x0FFE; i <= 0x1008; i++)
        EMIT_U16("wd", wide_dense(i));

    /* signed selector across -5..5 */
    {
        i16 s;
        for (s = -5; s <= 5; s++)
            EMIT_I16("ss", signed_sw(s));
    }
}
