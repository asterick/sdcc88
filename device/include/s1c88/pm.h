/*-------------------------------------------------------------------------
   pm.h — Pokémon Mini (Epson S1C88) hardware register map for sdcc -ms1c88.

   Epson-datasheet / official-SDK register names. Addresses cross-checked
   against the minimon emulator core (third_party/minimon-core) and the
   EPSON/TASKING c88-pokemini header. All MMIO lives in the 0x2000-0x20FF
   register page; every register is `volatile`.

   Usage:  #include <pm.h>
-------------------------------------------------------------------------*/
#ifndef _PM_H
#define _PM_H

#define _BV(bit)          ((unsigned char)(1u << (bit)))
#define REG_BASE          0x2000

#define _SFR8(off)        (*(volatile unsigned char *)(REG_BASE + (off)))
#define _SFR16(off)       (*(volatile unsigned int  *)(REG_BASE + (off)))

/* ---- System / power ---------------------------------------------------- */
#define SYS_CTRL1         _SFR8(0x00)
#define SYS_CTRL2         _SFR8(0x01)
#define SYS_CTRL3         _SFR8(0x02)
#define SYS_BATT          _SFR8(0x10)   /* battery / low-power status        */

/* ---- Seconds (real-time) timer ----------------------------------------- */
#define SEC_CTRL          _SFR8(0x08)
#define SEC_CNT           _SFR16(0x09)  /* 24-bit; LO/MID/HI below           */
#define SEC_CNT_LO        _SFR8(0x09)
#define SEC_CNT_MID       _SFR8(0x0A)
#define SEC_CNT_HI        _SFR8(0x0B)

/* ---- Timer prescaler / oscillator select (per timer) ------------------- */
#define TMR1_SCALE        _SFR8(0x18)
#define TMR1_OSC          _SFR8(0x19)
#define TMR2_SCALE        _SFR8(0x1A)
#define TMR2_OSC          _SFR8(0x1B)
#define TMR3_SCALE        _SFR8(0x1C)
#define TMR3_OSC          _SFR8(0x1D)

/* ---- Programmable timers 1/2/3 (16-bit; _L/_H halves) ------------------- */
#define TMR1_CTRL         _SFR16(0x30)
#define TMR1_CTRL_L       _SFR8(0x30)
#define TMR1_CTRL_H       _SFR8(0x31)
#define TMR1_PRE          _SFR16(0x32)  /* preset / reload                   */
#define TMR1_PRE_L        _SFR8(0x32)
#define TMR1_PRE_H        _SFR8(0x33)
#define TMR1_PVT          _SFR16(0x34)  /* pivot (for hi/lo split mode)      */
#define TMR1_PVT_L        _SFR8(0x34)
#define TMR1_PVT_H        _SFR8(0x35)
#define TMR1_CNT          _SFR16(0x36)  /* live count                        */
#define TMR1_CNT_L        _SFR8(0x36)
#define TMR1_CNT_H        _SFR8(0x37)

#define TMR2_CTRL         _SFR16(0x38)
#define TMR2_CTRL_L       _SFR8(0x38)
#define TMR2_CTRL_H       _SFR8(0x39)
#define TMR2_PRE          _SFR16(0x3A)
#define TMR2_PRE_L        _SFR8(0x3A)
#define TMR2_PRE_H        _SFR8(0x3B)
#define TMR2_PVT          _SFR16(0x3C)
#define TMR2_PVT_L        _SFR8(0x3C)
#define TMR2_PVT_H        _SFR8(0x3D)
#define TMR2_CNT          _SFR16(0x3E)
#define TMR2_CNT_L        _SFR8(0x3E)
#define TMR2_CNT_H        _SFR8(0x3F)

#define TMR3_CTRL         _SFR16(0x48)
#define TMR3_CTRL_L       _SFR8(0x48)
#define TMR3_CTRL_H       _SFR8(0x49)
#define TMR3_PRE          _SFR16(0x4A)
#define TMR3_PRE_L        _SFR8(0x4A)
#define TMR3_PRE_H        _SFR8(0x4B)
#define TMR3_PVT          _SFR16(0x4C)
#define TMR3_PVT_L        _SFR8(0x4C)
#define TMR3_PVT_H        _SFR8(0x4D)
#define TMR3_CNT          _SFR16(0x4E)
#define TMR3_CNT_L        _SFR8(0x4E)
#define TMR3_CNT_H        _SFR8(0x4F)

/* ---- 256 Hz timer ------------------------------------------------------ */
#define TMR256_CTRL       _SFR8(0x40)
#define TMR256_CNT        _SFR8(0x41)

/* ---- Input / I/O ------------------------------------------------------- */
#define KEY_PAD           _SFR8(0x52)   /* keys: 0=pressed (active low)      */
#define CART_BUS          _SFR8(0x53)
#define IO_DIR            _SFR8(0x60)   /* GPIO direction                    */
#define IO_DATA           _SFR8(0x61)   /* GPIO data                         */

#define KEY_A             _BV(0)
#define KEY_B             _BV(1)
#define KEY_C             _BV(2)
#define KEY_UP            _BV(3)
#define KEY_DOWN          _BV(4)
#define KEY_LEFT          _BV(5)
#define KEY_RIGHT         _BV(6)
#define KEY_POWER         _BV(7)

/* ---- Interrupt controller (priority / enable / active) ----------------- */
#define IRQ_PRI1          _SFR8(0x20)
#define IRQ_PRI2          _SFR8(0x21)
#define IRQ_PRI3          _SFR8(0x22)
#define IRQ_ENA1          _SFR8(0x23)
#define IRQ_ENA2          _SFR8(0x24)
#define IRQ_ENA3          _SFR8(0x25)
#define IRQ_ENA4          _SFR8(0x26)
#define IRQ_ACT1          _SFR8(0x27)   /* write 1 to acknowledge            */
#define IRQ_ACT2          _SFR8(0x28)
#define IRQ_ACT3          _SFR8(0x29)
#define IRQ_ACT4          _SFR8(0x2A)

/* Priority group fields (2 bits each, value 0..3). */
#define PRI1_PRC(x)       (((x) & 3) << 6)
#define PRI1_TIM2(x)      (((x) & 3) << 4)
#define PRI1_TIM1(x)      (((x) & 3) << 2)
#define PRI1_TIM3(x)      (((x) & 3) << 0)
#define PRI2_TIM256(x)    (((x) & 3) << 6)
#define PRI2_CART(x)      (((x) & 3) << 4)
#define PRI2_KEY(x)       (((x) & 3) << 2)
#define PRI3_IRSHOCK(x)   (((x) & 3) << 0)

/* IRQ_ENA1 / IRQ_ACT1 flags */
#define IRQ1_PRC_COMPLETE _BV(7)
#define IRQ1_DIV_OVF      _BV(6)
#define IRQ1_TIM2_HI_UF   _BV(5)
#define IRQ1_TIM2_LO_UF   _BV(4)
#define IRQ1_TIM1_HI_UF   _BV(3)
#define IRQ1_TIM1_LO_UF   _BV(2)
#define IRQ1_TIM3_HI_UF   _BV(1)
#define IRQ1_TIM3_PIVOT   _BV(0)
/* IRQ_ENA2 / IRQ_ACT2 flags */
#define IRQ2_32HZ         _BV(5)
#define IRQ2_8HZ          _BV(4)
#define IRQ2_2HZ          _BV(3)
#define IRQ2_1HZ          _BV(2)
#define IRQ2_CART_EJECT   _BV(1)
#define IRQ2_CART         _BV(0)
/* IRQ_ENA3 / IRQ_ACT3 flags */
#define IRQ3_KEYPOWER     _BV(7)
#define IRQ3_KEYRIGHT     _BV(6)
#define IRQ3_KEYLEFT      _BV(5)
#define IRQ3_KEYDOWN      _BV(4)
#define IRQ3_KEYUP        _BV(3)
#define IRQ3_KEYC         _BV(2)
#define IRQ3_KEYB         _BV(1)
#define IRQ3_KEYA         _BV(0)
/* IRQ_ENA4 / IRQ_ACT4 flags */
#define IRQ4_IR_RECV      _BV(7)
#define IRQ4_SHOCK        _BV(6)

/* Hardware interrupt VECTOR numbers — the handler address is read from
   physical (2 * vector). These index the crt0 cartridge vector table
   (slot N at 0x2102 + 6*N). See device/lib/s1c88/crt0.s. */
#define VEC_RESET             0x00
#define VEC_DIV_ZERO          0x01
#define VEC_WATCHDOG          0x02
#define VEC_PRC_COMPLETE      0x03
#define VEC_DIV_OVF           0x04
#define VEC_TIM2_HI_UF        0x05
#define VEC_TIM2_LO_UF        0x06
#define VEC_TIM1_HI_UF        0x07
#define VEC_TIM1_LO_UF        0x08
#define VEC_TIM3_HI_UF        0x09
#define VEC_TIM3_PIVOT        0x0A
#define VEC_32HZ              0x0B
#define VEC_8HZ               0x0C
#define VEC_2HZ               0x0D
#define VEC_1HZ              0x0E
#define VEC_IR_RECV           0x0F
#define VEC_SHOCK             0x10
#define VEC_CART_EJECT        0x13
#define VEC_CART              0x14
#define VEC_KEYPOWER          0x15
#define VEC_KEYRIGHT          0x16
#define VEC_KEYLEFT           0x17
#define VEC_KEYDOWN           0x18
#define VEC_KEYUP             0x19
#define VEC_KEYC              0x1A
#define VEC_KEYB              0x1B
#define VEC_KEYA              0x1C

/* ---- Audio ------------------------------------------------------------- */
#define AUD_CTRL          _SFR8(0x70)
#define AUD_VOL           _SFR8(0x71)

/* ---- PRC (graphics: map/sprite renderer) ------------------------------- */
#define PRC_MODE          _SFR8(0x80)
#define PRC_RATE          _SFR8(0x81)
#define PRC_MAP_LO        _SFR8(0x82)   /* tile-map base (24-bit phys addr)  */
#define PRC_MAP_MID       _SFR8(0x83)
#define PRC_MAP_HI        _SFR8(0x84)
#define PRC_SCROLL_Y      _SFR8(0x85)
#define PRC_SCROLL_X      _SFR8(0x86)
#define PRC_SPR_LO        _SFR8(0x87)   /* sprite (OAM) base                 */
#define PRC_SPR_MID       _SFR8(0x88)
#define PRC_SPR_HI        _SFR8(0x89)
#define PRC_CONT          _SFR8(0x8A)   /* render counter                    */

/* PRC_MODE bits */
#define MAP_INVERT        _BV(0)
#define MAP_ENABLE        _BV(1)
#define SPRITE_ENABLE     _BV(2)
#define COPY_ENABLE       _BV(3)
#define MAP_12X16         (0 << 4)      /* 96x128 px  */
#define MAP_16X12         (1 << 4)      /* 128x96 px  */
#define MAP_24X8          (2 << 4)      /* 192x64 px  */
#define MAP_24X16         (3 << 4)      /* 192x128 px */

/* PRC_RATE (frame divider of the 72 Hz base) */
#define RATE_24FPS        (0 << 1)
#define RATE_12FPS        (1 << 1)
#define RATE_8FPS         (2 << 1)
#define RATE_6FPS         (3 << 1)
#define RATE_36FPS        (4 << 1)
#define RATE_18FPS        (5 << 1)
#define RATE_9FPS         (7 << 1)

/* ---- LCD controller (Seiko driver, direct register access) ------------- */
#define LCD_CTRL          _SFR8(0xFE)
#define LCD_DATA          _SFR8(0xFF)

/* ---- Video RAM / OAM (near RAM) ---------------------------------------- */
#define TILEMAP           ((volatile unsigned char *)(0x1360))

typedef volatile struct {
    unsigned char x;
    unsigned char y;
    unsigned char tile;
    unsigned char ctrl;
} oam_sprite_t;

#define OAM               ((oam_sprite_t *)(0x1300))
#define OAM_FLIPH         _BV(0)
#define OAM_FLIPV         _BV(1)
#define OAM_INVERT        _BV(2)
#define OAM_ENABLE        _BV(3)

/* ---- CPU clock divider settings (TMRx_OSC / SYS_CTRL) ------------------- */
#define CLK_CPU_2MHZ      0
#define CLK_CPU_500KHZ    1
#define CLK_CPU_125KHZ    2
#define CLK_CPU_62500HZ   3
#define CLK_CPU_31250HZ   4
#define CLK_CPU_15625HZ   5
#define CLK_CPU_3906HZ    6
#define CLK_CPU_976HZ     7

/* Spin until the PRC finishes a frame (and acknowledge it). */
#define wait_vsync()  do {                              \
        IRQ_ACT1 = IRQ1_PRC_COMPLETE;                   \
        while (!(IRQ_ACT1 & IRQ1_PRC_COMPLETE)) { }     \
    } while (0)

#endif /* _PM_H */
