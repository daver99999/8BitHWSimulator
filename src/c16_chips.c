/* c16_chips.c - the chips of the Commodore 16 that the other machines don't have: the 7360 TED (video,
 * sound, timers, the DRAM and ROM strobes and the system clock), the 7501 CPU, the 6529B keyboard
 * port, the 7700 PLA (the I/O decode), the 74LS175 (the ROM latch), the 74LS125 and the 23128 ROMs
 * with the C16's chip select levels.
 *
 * The TED (U1) takes the 14.31818 MHz crystal on CLKIN (pin 14) and makes everything else of the
 * machine's timing out of it. One CPU cycle is 16 periods of the crystal (0.895 MHz) and the TED
 * divides it into four accesses of the DRAM, 4 periods each:
 *
 *      periods  0-7 (phi0 high)   the CPU's access (8 periods), or, on a line where the TED reads the
 *                                 characters (a "bad line"), two accesses of the TED: the colour
 *                                 attribute and the character code (AEC low, the CPU is stopped by BA)
 *      periods  8-11 (phi0 low)   the TED reads the dots of a character (or the bitmap)
 *      periods 12-15              a refresh cycle (RAS only), a few of them on every line
 *
 * A DRAM access: the address is on the bus (the TED's or the CPU's), MUX high is the row; /RAS falls,
 * MUX goes low (the column), /CAS falls, and the data is read before /RAS and /CAS rise again. The
 * TED holds /CAS off for the CPU's accesses to ROM ($8000 and up when the ROM is on), to its own
 * registers and to the I/O area, and drives /CS0 (the BASIC ROM, $8000-$BFFF) or /CS1 (the KERNAL,
 * $C000-$FFFF, but not $FD00-$FEFF) when the CPU reads ROM. The ROM latch and the 74LS139 turn those
 * into the chip selects of the ROMs and of the cartridges.
 *
 * The picture is drawn straight into the screen buffer as the dots are read: the analog video
 * (COLOR, LUM) is not simulated. The sound (SND) is a simple square wave of the two voices. */
#include "parts.h"
#include "chips_common.h"
#include "m6502.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define C16_CYCLES_LINE 57
#define C16_LINES       262
#define C16_X0          6       /* the part of the raster the monitor shows: from this cycle, */
#define C16_WIN_CYCLES  48      /* this many cycles (8 dots each), */
#define C16_Y0          238     /* from this line (wrapping around the end of the frame), */
#define C16_WIN_LINES   252     /* this many lines */
#define C16_D0          10      /* the first cycle of the 40 character display window */

/* ------------------------------------------------------------------------ */
/* 74LS175, 74LS125                                                          */
/* ------------------------------------------------------------------------ */

typedef struct { uint8_t q[4], clk; } ls175_t;
static void ls175_eval(sim_t *s, part_t *p) {
    const ls175_t *t = p->state;
    static const int q[4] = { 2, 7, 10, 15 }, nq[4] = { 3, 6, 11, 14 };      /* the real chip: Q4 is pin 15, /Q4 pin 14 */
    for (int i = 0; i < 4; i++) {
        pin_out(s, p, q[i], DRV_BIT(t->q[i]));
        pin_out(s, p, nq[i], DRV_BIT(!t->q[i]));
    }
}
static void ls175_commit(sim_t *s, part_t *p) {
    ls175_t *t = p->state;
    static const int d[4] = { 4, 5, 12, 13 };
    int clk = pin_in(s, p, 9);
    uint8_t q[4];
    memcpy(q, t->q, 4);
    if (!pin_in(s, p, 1)) memset(q, 0, 4);
    else if (clk && !t->clk)
        for (int i = 0; i < 4; i++) q[i] = (uint8_t)pin_in(s, p, d[i]);
    t->clk = (uint8_t)clk;
    if (memcmp(q, t->q, 4)) { memcpy(t->q, q, 4); sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void ls175_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls175_t)); }
const part_type_t PT_74LS175 = { "74LS175",
    "/CLR:m Q1:O /Q1:O D1:m D2:m /Q2:O Q2:O GND:P CLK:m Q3:O /Q3:O D3:m D4:m /Q4:O Q4:O VCC:P",
    ls175_eval, ls175_commit, ls175_reset, sizeof(ls175_t), TECH_TTL };

/* 74LS125: four buffers with a low active enable. */
static void ls125_eval(sim_t *s, part_t *p) {
    static const int g[4][3] = { { 1, 2, 3 }, { 4, 5, 6 }, { 10, 9, 8 }, { 13, 12, 11 } };    /* /G, A, Y */
    for (int i = 0; i < 4; i++)
        pin_out(s, p, g[i][2], pin_in(s, p, g[i][0]) ? DRV_Z : DRV_BIT(pin_in(s, p, g[i][1])));
}
const part_type_t PT_74LS125 = { "74LS125",
    "/1G:T 1A:T 1Y:O /2G:T 2A:T 2Y:O GND:P 3Y:O 3A:T /3G:T 4Y:O 4A:T /4G:T VCC:P", ls125_eval, 0, 0, 0, TECH_TTL };


/* ------------------------------------------------------------------------ */
/* 74LS27: three 3-input NOR gates                                           */
/* ------------------------------------------------------------------------ */

static void ls27_eval(sim_t *s, part_t *p) {
    static const int g[3][4] = { { 1, 2, 13, 12 }, { 3, 4, 5, 6 }, { 9, 10, 11, 8 } };
    for (int i = 0; i < 3; i++)
        pin_out(s, p, g[i][3], DRV_BIT(!(pin_in(s, p, g[i][0]) || pin_in(s, p, g[i][1]) || pin_in(s, p, g[i][2]))));
}
const part_type_t PT_74LS27 = { "74LS27",
    "1A:T 1B:T 2A:T 2B:T 2C:T 2Y:O GND:P 3Y:O 3A:T 3B:T 3C:T 1Y:O 1C:T VCC:P", ls27_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* 6551A ACIA at $FD00-$FD03 (the Plus/4's user port)                         */
/* ------------------------------------------------------------------------ */

/* Only the registers are modelled: the data register, the status (the transmitter is always empty, DCD and
 * DSR show their pins), the command register (it drives /DTR and /RTS) and the control register. Nothing is
 * sent or received: TxD stays high and RxD is ignored. The Plus/4 wires D0-D7 to pins 18-25. */
static const int acia_d[8] = { 18, 19, 20, 21, 22, 23, 24, 25 };
typedef struct { uint8_t cmd, ctl, data; int32_t cache_d; } acia_t;
static bool acia_selected(sim_t *s, part_t *p) { return pin_in(s, p, 2) && !pin_in(s, p, 3) && pin_in(s, p, 27); }
static void acia_eval(sim_t *s, part_t *p) {
    acia_t *t = p->state;
    int rs = pin_in(s, p, 13) | (pin_in(s, p, 14) << 1);
    bool rd = acia_selected(s, p) && pin_in(s, p, 28);
    uint8_t v = rs == 0 ? 0 : rs == 1 ? (uint8_t)(0x10 | (pin_in(s, p, 17) << 6) | (pin_in(s, p, 16) << 5)) : rs == 2 ? t->cmd : t->ctl;
    part_drive_bus_cached(s, p, acia_d, 8, rd ? v : 0, rd, &t->cache_d);
    pin_out(s, p, 10, DRV_HI);                                  /* TxD idles high */
    pin_out(s, p, 11, (t->cmd & 1) ? DRV_LO : DRV_HI);         /* /DTR */
    pin_out(s, p, 8, (t->cmd & 0x0C) ? DRV_LO : DRV_HI);       /* /RTS */
    pin_out(s, p, 26, DRV_Z);                                   /* /IRQ: open drain, never asserted */
    pin_out(s, p, 5, DRV_Z);
    pin_out(s, p, 7, DRV_Z);
}
static void acia_commit(sim_t *s, part_t *p) {
    acia_t *t = p->state;
    uint8_t cmd = t->cmd, ctl = t->ctl;
    if (!pin_in(s, p, 4)) { cmd = 0; ctl = 0; }
    else if (acia_selected(s, p) && !pin_in(s, p, 28)) {
        int rs = pin_in(s, p, 13) | (pin_in(s, p, 14) << 1);
        uint8_t v = (uint8_t)part_read_bus(s, p, acia_d, 8);
        if (rs == 0) t->data = v;
        else if (rs == 1) cmd &= 0xE0;                          /* a write to the status register is a programmed reset */
        else if (rs == 2) cmd = v;
        else ctl = v;
    }
    if (cmd != t->cmd || ctl != t->ctl) { t->cmd = cmd; t->ctl = ctl; sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void acia_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(acia_t)); }
const part_type_t PT_6551 = { "6551A",
    "GND:P CS0:M /CS1:M /RES:M RxC:O XTAL1:m XTAL2:O /RTS:O /CTS:M TxD:O /DTR:O RxD:M RS0:M RS1:M VCC:P /DCD:M /DSR:M "
    "D0:B D1:B D2:B D3:B D4:B D5:B D6:B D7:B /IRQ:O PHI2:M R/W:M",
    acia_eval, acia_commit, acia_reset, sizeof(acia_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 23128 ROM (the C16's: /CS1 low, CS2 and CS3 high)                         */
/* ------------------------------------------------------------------------ */

static const int c16rom_a[14] = { 10, 9, 8, 7, 6, 5, 4, 3, 25, 24, 21, 23, 2, 26 };
static const int c16rom_d[8] = { 11, 12, 13, 15, 16, 17, 18, 19 };
static void c16rom_eval(sim_t *s, part_t *p) {
    rom_state_t *st = p->state;
    bool sel = !pin_in(s, p, 22) && pin_in(s, p, 20) && pin_in(s, p, 27);
    unsigned a = sel ? part_read_bus(s, p, c16rom_a, 14) : 0;
    part_drive_bus_cached(s, p, c16rom_d, 8, st->data ? st->data[a] : 0xFF, sel, &st->out_cache);
}
const part_type_t PT_C16ROM = { "23128 ROM",
    "NC:O A12:M A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O D7:O "
    "CS2:M A10:M /CS1:M A11:M A9:M A8:M A13:M CS3:M VCC:P", c16rom_eval, 0, 0, sizeof(rom_state_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 6529B: the keyboard port at $FD30                                         */
/* ------------------------------------------------------------------------ */

static const int p6529_d[8] = { 18, 17, 16, 15, 14, 13, 12, 11 };
typedef struct { uint8_t latch; int32_t cache_d; } p6529_t;
static unsigned p6529_pins(sim_t *s, part_t *p) {
    unsigned v = 0;
    for (int i = 0; i < 8; i++) v |= (unsigned)pin_in(s, p, 2 + i) << i;
    return v;
}
static void p6529_eval(sim_t *s, part_t *p) {
    p6529_t *t = p->state;
    for (int i = 0; i < 8; i++) pin_out(s, p, 2 + i, (t->latch >> i) & 1 ? DRV_PULLUP : DRV_LO);
    bool rd = !pin_in(s, p, 19) && pin_in(s, p, 1);
    part_drive_bus_cached(s, p, p6529_d, 8, rd ? p6529_pins(s, p) : 0, rd, &t->cache_d);
}
static void p6529_commit(sim_t *s, part_t *p) {
    p6529_t *t = p->state;
    if (!pin_in(s, p, 19) && !pin_in(s, p, 1)) {
        uint8_t v = (uint8_t)part_read_bus(s, p, p6529_d, 8);
        if (v != t->latch) { t->latch = v; sim_mark_dirty(s, (int)(p - s->parts)); }
    }
}
static void p6529_reset(sim_t *s, part_t *p) { (void)s; p6529_t *t = p->state; t->latch = 0xFF; t->cache_d = 0; }
const part_type_t PT_6529B = { "6529B",
    "R/W:M P0:X P1:X P2:X P3:X P4:X P5:X P6:X P7:X GND:P D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B /CS:M VCC:P",
    p6529_eval, p6529_commit, p6529_reset, sizeof(p6529_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 7700-010 PLA (251641-02)                                                  */
/* ------------------------------------------------------------------------ */

/* The truth table of the PLA (Commodore's own listing). Inputs I0-I15 of a product term: '0', '1' or
 * 'X' (ignored); outputs F0-F7: the level the term gives an output, 'X' for no change. An output that no
 * term sets has the level of the default column.
 *   inputs:  I0 F7, I1 phi0, I2 A15, I3 A4, I4 A5, I5 A6, I6 A7, I7 A12, I8 MUX, I9 A14, I10 A8, I11 A9,
 *            I12 A13, I13 A11, I14 A10, I15 /RAS
 *   outputs: F0 (not connected), F1 phi2, F2 /USER, F3 /6551, F4 ADDR CLK, F5 /KEYPORT, F6 KERNAL,
 *            F7 (wired back to I0: a latch that holds while phi0 is high and /RAS low) */
typedef struct { const char *in, *out; } pla7700_term_t;
static const char pla7700_default[] = "10111100";
static const pla7700_term_t pla7700_terms[] = {
    { "X1100001X110111X", "XXX0XXXX" },     /* $FD00-$FD0F: /6551 */
    { "X1101001X1101110", "0XXXXXXX" },     /* $FD20-$FD2F: F0 */
    { "XX1XXXX1X100111X", "XXXXXX1X" },     /* $FC00-$FCFF: KERNAL */
    { "11111001X1101110", "X1XXX0X1" },     /* $FD30-$FD3F: /KEYPORT */
    { "11110111X1101110", "X1XX0XX1" },     /* $FDD0-$FDDF: ADDR CLK */
    { "11110001X1101110", "X10XXXX1" },     /* $FD10-$FD1F: /USER (the cassette) */
    { "XXXXXXXX1XXXXXXX", "XXXXXXX1" },     /* MUX high: set the latch */
    { "11XXXXXXXXXXXXX0", "X1XXXXX1" },     /* the latch holds while phi0 high and /RAS low: phi2 */
};
static const int pla7700_ipin[16] = { 9, 8, 7, 6, 5, 4, 3, 2, 27, 26, 25, 24, 23, 22, 21, 20 };
static const int pla7700_opin[8] = { 18, 17, 16, 15, 13, 12, 11, 10 };

static void pla7700_eval(sim_t *s, part_t *p) {
    unsigned in = 0;
    for (int i = 0; i < 16; i++) in |= (unsigned)pin_in(s, p, pla7700_ipin[i]) << i;
    char out[9];
    memcpy(out, pla7700_default, 9);
    for (size_t t = 0; t < sizeof(pla7700_terms) / sizeof(pla7700_terms[0]); t++) {
        bool hit = true;
        for (int i = 0; i < 16 && hit; i++) {
            char c = pla7700_terms[t].in[i];
            if (c != 'X' && (c - '0') != (int)((in >> i) & 1)) hit = false;
        }
        if (!hit) continue;
        for (int o = 0; o < 8; o++)
            if (pla7700_terms[t].out[o] != 'X') out[o] = pla7700_terms[t].out[o];
    }
    bool en = !pin_in(s, p, 19);
    for (int o = 0; o < 8; o++) pin_out(s, p, pla7700_opin[o], en ? DRV_BIT(out[o] == '1') : DRV_Z);
}
const part_type_t PT_7700 = { "7700-010 PLA",
    "FE:O I7:T I6:T I5:T I4:T I3:T I2:T I1:T I0:T F7:O F6:O F5:O F4:O GND:P F3:O F2:O F1:O F0:O /CE:T "
    "I15:T I14:T I13:T I12:T I11:T I10:T I9:T I8:T VCC:P",
    pla7700_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* 7501 CPU (U2)                                                             */
/* ------------------------------------------------------------------------ */

/* A 6502 with the 6510's I/O port (P0-P4, P6, P7 have pins), an /IRQ and no /NMI. It runs on phase 0
 * from the TED (pin 1): the core does one cycle when phi0 falls, with the data the bus had at the
 * end of the high phase. Its address, R/W and data pins are in high impedance while AEC is low. */
enum { K_PHI0 = 1, K_RDY = 2, K_IRQ = 3, K_AEC = 4, K_GATE = 23, K_RW = 39, K_RES = 40 };
static const int c16cpu_a[16] = { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22 };
static const int c16cpu_d[8] = { 38, 37, 36, 35, 34, 33, 32, 31 };
static const int c16cpu_port[8] = { 30, 29, 28, 27, 26, 0, 25, 24 };       /* P0..P7: P5 has no pin */

typedef struct {
    m6502_t core;
    uint64_t pins;
    sim_t *s;                   /* for the port callbacks */
    part_t *part;
    uint8_t phi0, in_reset, latched;
    uint32_t cycles;
    int32_t cache_a, cache_d, cache_p;
} c16_cpu_t;

static uint8_t c16cpu_port_in(void *user) {
    c16_cpu_t *c = user;
    uint8_t v = 0;
    for (int i = 0; i < 8; i++)
        if (c16cpu_port[i]) v |= (uint8_t)(pin_in(c->s, c->part, c16cpu_port[i]) << i);
    return v;
}
static void c16cpu_port_out(uint8_t data, void *user) { (void)data; (void)user; }

static void c16cpu_eval(sim_t *s, part_t *p) {
    c16_cpu_t *c = p->state;
    int32_t pk = (c->core.io_ddr << 8) | c->core.io_out;
    if (c->cache_p != pk + 1) {
        c->cache_p = pk + 1;
        for (int i = 0; i < 8; i++)
            if (c16cpu_port[i]) pin_out(s, p, c16cpu_port[i], (c->core.io_ddr >> i) & 1 ? DRV_BIT((c->core.io_out >> i) & 1) : DRV_Z);
    }
    bool bus = pin_in(s, p, K_AEC) != 0;
    uint16_t addr = M6502_GET_ADDR(c->pins);
    bool write = !(c->pins & M6502_RW);
    part_drive_bus_cached(s, p, c16cpu_a, 16, addr, bus, &c->cache_a);
    pin_out(s, p, K_RW, bus ? DRV_BIT(!write) : DRV_Z);
    bool phi0 = pin_in(s, p, K_PHI0) != 0;
    part_drive_bus_cached(s, p, c16cpu_d, 8, M6502_GET_DATA(c->pins), bus && write && phi0 && addr > 1, &c->cache_d);
}

static void c16cpu_commit(sim_t *s, part_t *p) {
    c16_cpu_t *c = p->state;
    if (p->dead) return;
    int phi0 = pin_in(s, p, K_PHI0);
    if (phi0) c->latched = (uint8_t)part_read_bus(s, p, c16cpu_d, 8);
    if (!phi0 && c->phi0) {
        if (!pin_in(s, p, K_RES)) {
            c->in_reset = 1;
        } else {
            c->s = s;
            c->part = p;
            c->core.user_data = c;
            if (c->in_reset) {
                c->in_reset = 0;
                c->pins |= M6502_RES;
                c->core.io_ddr = 0;
                c->core.io_out = 0;
            }
            uint64_t pins = c->pins & ~(M6502_IRQ | M6502_NMI | M6502_RDY);
            if (!pin_in(s, p, K_IRQ)) pins |= M6502_IRQ;
            if (!pin_in(s, p, K_RDY)) pins |= M6502_RDY;
            if (pins & M6502_RW) M6502_SET_DATA(pins, c->latched);
            if (M6510_CHECK_IO(pins)) pins = m6510_iorq(&c->core, pins);
            c->pins = m6502_tick(&c->core, pins);
            c->cycles++;
            sim_mark_dirty(s, (int)(p - s->parts));
        }
    }
    c->phi0 = (uint8_t)phi0;
}

static void c16cpu_reset(sim_t *s, part_t *p) {
    (void)s;
    c16_cpu_t *c = p->state;
    memset(c, 0, sizeof(*c));
    c->pins = m6502_init(&c->core, &(m6502_desc_t){
        .m6510_in_cb = c16cpu_port_in, .m6510_out_cb = c16cpu_port_out, .m6510_user_data = c,
        .m6510_io_pullup = 0, .m6510_io_floating = 0 });
    c->in_reset = 1;
}

const part_type_t PT_7501 = { "7501",
    "PHI0:M RDY:m /IRQ:m AEC:M VCC:P A0:O A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O A10:O A11:O A12:O A13:O "
    "VSS:P A14:O A15:O GATE:m P7:B P6:B P4:B P3:B P2:B P1:B P0:B D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B "
    "R/W:O /RES:m",
    c16cpu_eval, c16cpu_commit, c16cpu_reset, sizeof(c16_cpu_t), TECH_NMOS };

uint32_t c16_cpu_cycles_of(const part_t *p) { return ((const c16_cpu_t *)p->state)->cycles; }
void c16_cpu_regs_of(const part_t *p, char *buf, int len) {
    const c16_cpu_t *c = p->state;
    snprintf(buf, (size_t)len, "PC=%04X A=%02X X=%02X Y=%02X S=%02X P=%02X", c->core.PC, c->core.A, c->core.X,
             c->core.Y, c->core.S, c->core.P);
}

/* ------------------------------------------------------------------------ */
/* 7360 TED (U1)                                                             */
/* ------------------------------------------------------------------------ */

enum { TP_CS0 = 5, TP_CS1 = 6, TP_RW = 7, TP_IRQ = 8, TP_MUX = 9, TP_RAS = 10, TP_CAS = 11, TP_PHI0 = 12,
       TP_CLKIN = 14, TP_SND = 33, TP_BA = 34, TP_AEC = 35 };
static const int ted_a[16] = { 3, 2, 1, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36 };
static const int ted_d[8] = { 25, 26, 27, 28, 29, 30, 31, 32 };
static const int ted_k[8] = { 15, 16, 17, 18, 19, 20, 21, 22 };

enum { SUB_NONE, SUB_ATTR, SUB_CODE, SUB_GFX, SUB_REFRESH, SUB_CPU };

typedef struct {
    uint8_t reg[32];
    uint8_t clk, ph;                /* CLKIN at the last step; the crystal period of the cycle (0-15) */
    uint8_t cyc;                    /* the cycle of the line (0-56) */
    uint16_t line;                  /* the raster line */
    uint32_t frame;
    /* what the pins show */
    uint16_t addr;
    uint8_t addr_en, rw_drive, ras, cas, mux, phi0, aec, ba, cs0, cs1, dout_en, dout, irq_out, snd;
    /* the access in progress */
    uint8_t dma;                    /* this cycle's first half is the TED's (character DMA) */
    uint8_t sub, sub_cas, sub_cs;   /* kind, strobes CAS, selects a ROM (0 none, 1 /CS0, 2 /CS1) */
    uint8_t cpu_rw, cpu_reg, cpu_write_reg;
    uint8_t cpu_ram;
    uint16_t cpu_addr;
    uint8_t gfx_k;                  /* the character the dots are being read for */
    uint8_t gfx_ok;
    /* the registers' own state */
    uint16_t t1, t2, t3, t1_rel, t2_rel, t3_rel;
    uint8_t t1_run, t2_run, t3_run;
    uint8_t irq_status, klatch, rom_on;
    /* video */
    uint8_t badline, prev_bad, row_active, rc;
    uint8_t attr[40], code[40], nattr[40], ncode[40];
    uint8_t dma_row, next_row, disp_row;
    uint8_t refresh;
    uint32_t sndcnt1, sndcnt2;
    int32_t cache_a, cache_d;
    uint32_t *screen;               /* C16_SCREEN_W x C16_SCREEN_H, set by the machine */
} ted_t;

/* The 128 colors (16 hues, 8 luminances): hue 0 is black, hue 1 the grays, the rest by an angle. */
static uint32_t ted_pal[128];
static bool ted_pal_ready;
static void ted_make_palette(void) {
    static const int angle[16] = { 0, 0, 103, 283, 53, 241, 347, 167, 123, 148, 195, 83, 265, 323, 13, 213 };
    for (int h = 0; h < 16; h++)
        for (int l = 0; l < 8; l++) {
            double y = h == 0 ? 0.0 : (l + 0.4) / 7.6, u = 0, v = 0;
            if (h >= 2) { u = 0.32 * cos(angle[h] * 3.14159265 / 180.0); v = 0.32 * sin(angle[h] * 3.14159265 / 180.0); }
            double r = y + 1.14 * v, g = y - 0.395 * u - 0.581 * v, b = y + 2.032 * u;
            r = r < 0 ? 0 : r > 1 ? 1 : r;
            g = g < 0 ? 0 : g > 1 ? 1 : g;
            b = b < 0 ? 0 : b > 1 ? 1 : b;
            ted_pal[h * 8 + l] = 0xFF000000u | (uint32_t)(b * 255) << 16 | (uint32_t)(g * 255) << 8 | (uint32_t)(r * 255);
        }
    ted_pal_ready = true;
}
static inline uint32_t ted_color(unsigned c) { return ted_pal[(c & 15) * 8 + ((c >> 4) & 7)]; }

static uint64_t ted_sig(const ted_t *t) {
    return (uint64_t)t->ras | (uint64_t)t->cas << 1 | (uint64_t)t->mux << 2 | (uint64_t)t->phi0 << 3 |
           (uint64_t)t->aec << 4 | (uint64_t)t->ba << 5 | (uint64_t)t->cs0 << 6 | (uint64_t)t->cs1 << 7 |
           (uint64_t)t->addr << 8 | (uint64_t)t->addr_en << 24 | (uint64_t)t->rw_drive << 25 |
           (uint64_t)t->dout_en << 26 | (uint64_t)t->dout << 27 | (uint64_t)t->irq_out << 35 | (uint64_t)t->snd << 36;
}

static void ted_eval(sim_t *s, part_t *p) {
    ted_t *t = p->state;
    pin_out(s, p, TP_PHI0, DRV_BIT(t->phi0));
    pin_out(s, p, TP_RAS, DRV_BIT(t->ras));
    pin_out(s, p, TP_CAS, DRV_BIT(t->cas));
    pin_out(s, p, TP_MUX, DRV_BIT(t->mux));
    pin_out(s, p, TP_CS0, DRV_BIT(t->cs0));
    pin_out(s, p, TP_CS1, DRV_BIT(t->cs1));
    pin_out(s, p, TP_AEC, DRV_BIT(t->aec));
    pin_out(s, p, TP_BA, DRV_BIT(t->ba));
    pin_out(s, p, TP_IRQ, t->irq_out ? DRV_LO : DRV_Z);
    pin_out(s, p, TP_SND, DRV_BIT(t->snd));
    pin_out(s, p, TP_RW, t->rw_drive ? DRV_HI : DRV_Z);
    part_drive_bus_cached(s, p, ted_a, 16, t->addr, t->addr_en, &t->cache_a);
    part_drive_bus_cached(s, p, ted_d, 8, t->dout, t->dout_en, &t->cache_d);
    for (int i = 0; i < 8; i++) pin_out(s, p, ted_k[i], DRV_PULLUP);
}

static uint8_t ted_read_reg(ted_t *t, int r) {
    bool irq = (t->irq_status & t->reg[0x0A] & 0x5E) != 0;
    switch (r) {
    case 0x00: return (uint8_t)t->t1;
    case 0x01: return (uint8_t)(t->t1 >> 8);
    case 0x02: return (uint8_t)t->t2;
    case 0x03: return (uint8_t)(t->t2 >> 8);
    case 0x04: return (uint8_t)t->t3;
    case 0x05: return (uint8_t)(t->t3 >> 8);
    case 0x08: return t->klatch;
    case 0x09: return (uint8_t)((t->irq_status & 0x5E) | (irq ? 0x80 : 0) | 0x21);
    case 0x0A: return (uint8_t)(t->reg[0x0A] | 0xA0);
    case 0x0C: return (uint8_t)(t->reg[0x0C] | 0xFC);
    case 0x10: return (uint8_t)(t->reg[0x10] | 0xFC);
    case 0x12: return (uint8_t)(t->reg[0x12] | 0xC0);
    case 0x13: return (uint8_t)((t->reg[0x13] & 0xFE) | (t->rom_on ? 1 : 0));
    case 0x14: return (uint8_t)(t->reg[0x14] | 0x07);
    case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: return (uint8_t)(t->reg[r] | 0x80);
    case 0x1A: return (uint8_t)(t->reg[0x1A] | 0xFC);
    case 0x1C: return (uint8_t)(((t->line >> 8) & 1) | 0xFE);
    case 0x1D: return (uint8_t)t->line;
    case 0x1E: return (uint8_t)(t->cyc * 4);
    case 0x1F: return (uint8_t)(t->rc | (((t->frame >> 4) & 15) << 3) | 0x80);
    default: return t->reg[r];
    }
}

static void ted_write_reg(sim_t *s, part_t *p, ted_t *t, int r, uint8_t v) {
    switch (r) {
    case 0x00: t->t1_rel = (uint16_t)((t->t1_rel & 0xFF00) | v); break;
    case 0x01: t->t1_rel = (uint16_t)((t->t1_rel & 0x00FF) | v << 8); t->t1 = t->t1_rel; t->t1_run = 1; break;
    case 0x02: t->t2_rel = (uint16_t)((t->t2_rel & 0xFF00) | v); break;
    case 0x03: t->t2_rel = (uint16_t)((t->t2_rel & 0x00FF) | v << 8); t->t2 = t->t2_rel; t->t2_run = 1; break;
    case 0x04: t->t3_rel = (uint16_t)((t->t3_rel & 0xFF00) | v); break;
    case 0x05: t->t3_rel = (uint16_t)((t->t3_rel & 0x00FF) | v << 8); t->t3 = t->t3_rel; t->t3_run = 1; break;
    case 0x08: {                                        /* latch the keyboard: K0-K7 */
        unsigned k = 0;
        for (int i = 0; i < 8; i++) k |= (unsigned)pin_in(s, p, ted_k[i]) << i;
        t->klatch = (uint8_t)k;
        break;
    }
    case 0x09: t->irq_status &= (uint8_t)~(v & 0x5E); break;
    case 0x1C: case 0x1D: case 0x1E: break;
    default: break;
    }
    t->reg[r] = v;
}

/* Is the CPU's read of this address answered by a ROM? 1: /CS0 (BASIC side), 2: /CS1 (KERNAL side). */
static int ted_rom_of(const ted_t *t, unsigned a) {
    if (a >= 0xFD00 && a < 0xFF00) return 0;            /* I/O */
    if (a >= 0xFF00 && a < 0xFF20) return 0;            /* the TED */
    if (!t->rom_on) return 0;                           /* ROM off: all RAM, $FC00-$FCFF too (BASIC keeps its strings there) */
    if (a >= 0xFC00 && a < 0xFD00) return 2;            /* the KERNAL's tail: the PLA's KERN output keeps the KERNAL, not a function ROM, on it */
    if (a >= 0x8000 && a < 0xC000) return 1;
    if (a >= 0xC000) return 2;
    return 0;
}

static void ted_plot(ted_t *t, int cyc, int line, const uint32_t px[8]) {
    int y = (line - C16_Y0 + C16_LINES) % C16_LINES, x = (cyc - C16_X0) * 8;
    if (!t->screen || y >= C16_WIN_LINES || x < 0 || x >= C16_WIN_CYCLES * 8) return;
    for (int i = 0; i < 8; i++) t->screen[y * (C16_WIN_CYCLES * 8) + x + i] = px[i];
}

static bool ted_vwin(const ted_t *t, int line) {
    if (!(t->reg[0x06] & 0x10)) return false;
    int top = (t->reg[0x06] & 8) ? 4 : 8, bot = (t->reg[0x06] & 8) ? 203 : 199;
    return line >= top && line <= bot;
}

/* The 8 dots of a character cell: the border or the background of the window, or the picture. */
static void ted_fill_cell(ted_t *t) {
    uint32_t px[8];
    int c = t->cyc;
    bool inside = ted_vwin(t, t->line) && c >= C16_D0 && c < C16_D0 + 40;
    uint32_t col = ted_color(inside ? t->reg[0x15] : t->reg[0x19]);
    for (int i = 0; i < 8; i++) px[i] = col;
    ted_plot(t, c, t->line, px);
}

static void ted_draw_char(ted_t *t, int k, unsigned g) {
    uint32_t px[8];
    uint8_t code = t->code[k], attr = t->attr[k];
    bool ecm = t->reg[0x06] & 0x40, bmm = t->reg[0x06] & 0x20, mcm = t->reg[0x07] & 0x10;
    bool flash = (t->frame >> 4) & 1;
    if (!bmm && !mcm) {
        uint32_t fg = ted_color(attr), bg = ted_color(t->reg[0x15 + (ecm ? code >> 6 : 0)]);
        bool inv = !ecm && (code & 0x80);
        unsigned cur = ((t->reg[0x0C] & 3) << 8) | t->reg[0x0D];
        if ((attr & 0x80) && flash) inv = !inv;
        if (cur == (unsigned)(t->disp_row * 40 + k) && flash) inv = !inv;
        for (int i = 0; i < 8; i++) px[i] = (((g >> (7 - i)) & 1) != 0) != inv ? fg : bg;
    } else if (!bmm) {
        for (int i = 0; i < 4; i++) {
            int c = (g >> (6 - 2 * i)) & 3;
            unsigned col = c == 0 ? t->reg[0x15] : c == 1 ? t->reg[0x16] : c == 2 ? t->reg[0x17] : attr;
            px[2 * i] = px[2 * i + 1] = ted_color(col);
        }
    } else if (!mcm) {
        for (int i = 0; i < 8; i++) px[i] = ted_color(((g >> (7 - i)) & 1) ? attr : code);
    } else {
        for (int i = 0; i < 4; i++) {
            int c = (g >> (6 - 2 * i)) & 3;
            unsigned col = c == 0 ? t->reg[0x15] : c == 1 ? code : c == 2 ? attr : t->reg[0x16];
            px[2 * i] = px[2 * i + 1] = ted_color(col);
        }
    }
    ted_plot(t, C16_D0 + k, t->line, px);
}

static void ted_irq_update(ted_t *t) { t->irq_out = (t->irq_status & t->reg[0x0A] & 0x5E) != 0; }

/* A new line: the badline, the rows. */
static void ted_new_line(ted_t *t) {
    int line = t->line, yscroll = t->reg[0x06] & 7;
    if (line == 0) { t->next_row = 0; t->row_active = 0; t->frame++; }
    t->badline = (t->reg[0x06] & 0x10) && line >= 3 && line <= 202 && (line & 7) == yscroll;
    if (t->prev_bad) {
        memcpy(t->attr, t->nattr, 40);
        memcpy(t->code, t->ncode, 40);
        t->row_active = 1;
        t->rc = 0;
        t->disp_row = t->dma_row;
    } else if (t->row_active && ++t->rc == 8) {
        t->row_active = 0;
    }
    t->prev_bad = t->badline;
}

static void ted_sub_off(ted_t *t) {
    t->ras = t->cas = t->mux = t->cs0 = t->cs1 = 1;
    t->sub = SUB_NONE;
}

/* An access of the TED's own, 4 periods: the address now, /RAS, the column, /CAS follow. */
static void ted_sub_start(ted_t *t, int kind, unsigned addr, bool cas, int cs) {
    t->sub = (uint8_t)kind;
    t->sub_cas = cas;
    t->sub_cs = (uint8_t)cs;
    t->addr = (uint16_t)addr;
    t->addr_en = 1;
    t->rw_drive = 1;
    t->mux = 1;
}

static void ted_sub_wave(ted_t *t, int o) {
    if (t->sub == SUB_NONE) return;
    if (o == 1) t->ras = 0;
    if (o == 2) {
        t->mux = 0;
        if (t->sub_cs == 1) t->cs0 = 0;
        if (t->sub_cs == 2) t->cs1 = 0;
    }
    if (o == 3 && t->sub_cas) t->cas = 0;
}

static void ted_cycle_start(sim_t *s, part_t *p, ted_t *t) {
    (void)s; (void)p;
    if (++t->cyc == C16_CYCLES_LINE) {
        t->cyc = 0;
        if (++t->line == C16_LINES) t->line = 0;
        ted_new_line(t);
    }
    int c = t->cyc;
    /* timers (one step per cycle) */
    if (t->t1_run) {
        if (t->t1 == 0) { t->t1 = t->t1_rel; t->irq_status |= 0x08; } else t->t1--;
    }
    if (t->t2_run) {
        if (t->t2 == 0) { t->t2 = 0xFFFF; t->irq_status |= 0x10; } else t->t2--;
    }
    if (t->t3_run) {
        if (t->t3 == 0) { t->t3 = 0xFFFF; t->irq_status |= 0x40; } else t->t3--;
    }
    if (c == 1 && t->line == (unsigned)(((t->reg[0x0A] & 1) << 8) | t->reg[0x0B])) t->irq_status |= 0x02;
    ted_irq_update(t);
    if (c == 0) t->refresh = 5;
    /* character DMA: cycles 9-48 of a bad line, with BA 3 cycles earlier */
    t->dma = t->badline && c >= C16_D0 - 1 && c < C16_D0 + 39;
    t->ba = !(t->badline && c >= C16_D0 - 4 && c < C16_D0 + 39);
    t->phi0 = 1;
    t->aec = !t->dma;
    ted_fill_cell(t);
    ted_sub_off(t);
    t->cpu_reg = t->cpu_write_reg = t->cpu_ram = 0;
    t->dout_en = 0;
    t->rw_drive = 0;
    t->addr_en = 0;
    if (t->dma) {
        unsigned base = (unsigned)(t->reg[0x14] & 0xF8) << 8;
        if (c == C16_D0 - 1) t->dma_row = t->next_row;
        ted_sub_start(t, SUB_ATTR, base + (unsigned)t->dma_row * 40 + (unsigned)(c - (C16_D0 - 1)), true, 0);
    } else {
        t->sub = SUB_CPU;
        t->mux = 1;
    }
}

/* The CPU's access: what is the address, what answers. Called when /RAS is low and the address settled. */
static void ted_cpu_decode(sim_t *s, part_t *p, ted_t *t) {
    unsigned a = part_read_bus(s, p, ted_a, 16);
    bool rw = pin_in(s, p, TP_RW) != 0;
    t->cpu_addr = (uint16_t)a;
    t->cpu_rw = rw;
    bool reg = a >= 0xFF00 && a < 0xFF20, io = a >= 0xFD00 && a < 0xFF00;
    t->cpu_ram = 0;
    if (reg) {
        if (rw) { t->dout = ted_read_reg(t, (int)(a & 0x1F)); t->dout_en = 1; }
        else t->cpu_write_reg = 1;
        return;
    }
    if (a == 0xFF3E || a == 0xFF3F) { if (!rw) t->cpu_write_reg = 2; }
    if (io) return;
    int rom = rw ? ted_rom_of(t, a) : 0;
    if (rom == 1) t->cs0 = 0;
    else if (rom == 2) t->cs1 = 0;
    t->cpu_ram = rom == 0 && !(a == 0xFF3E || a == 0xFF3F);
}

static void ted_gfx_start(ted_t *t) {
    int c = t->cyc, k = c - C16_D0;
    t->gfx_ok = 0;
    if (!t->row_active || !ted_vwin(t, t->line) || k < 0 || k >= 40) return;
    bool bmm = t->reg[0x06] & 0x20, ecm = t->reg[0x06] & 0x40;
    unsigned addr;
    if (bmm) addr = ((unsigned)(t->reg[0x12] & 0x38) << 10) + ((unsigned)t->disp_row * 40 + (unsigned)k) * 8 + t->rc;
    else addr = ((unsigned)(t->reg[0x13] & 0xFC) << 8) + (unsigned)(t->code[k] & (ecm ? 0x3F : 0x7F)) * 8 + t->rc;
    bool rom = (t->reg[0x12] & 4) && addr >= 0x8000;
    int cs = rom ? (addr >= 0xC000 ? 2 : 1) : 0;
    t->gfx_k = (uint8_t)k;
    t->gfx_ok = 1;
    ted_sub_start(t, SUB_GFX, addr, !rom, cs);
}

static void ted_clock(sim_t *s, part_t *p, ted_t *t) {
    uint64_t before = ted_sig(t);
    unsigned ph = (t->ph + 1) & 15;
    t->ph = (uint8_t)ph;
    if (ph == 0) {
        ted_cycle_start(s, p, t);
    } else if (ph == 4 && t->dma) {                     /* the attribute is read: now the character code */
        t->nattr[t->cyc - (C16_D0 - 1)] = (uint8_t)part_read_bus(s, p, ted_d, 8);
        ted_sub_off(t);
        ted_sub_start(t, SUB_CODE, ((unsigned)(t->reg[0x14] & 0xF8) << 8) + 0x400 + (unsigned)t->dma_row * 40 +
                      (unsigned)(t->cyc - (C16_D0 - 1)), true, 0);
    } else if (ph == 8) {
        /* the end of the CPU's (or the TED's DMA) half: the data is on the bus for the last time */
        if (t->dma) {
            t->ncode[t->cyc - (C16_D0 - 1)] = (uint8_t)part_read_bus(s, p, ted_d, 8);
            if (t->cyc == C16_D0 + 38) t->next_row++;
        } else if (t->cpu_write_reg == 1) {
            ted_write_reg(s, p, t, (int)(t->cpu_addr & 0x1F), (uint8_t)part_read_bus(s, p, ted_d, 8));
            ted_irq_update(t);
        } else if (t->cpu_write_reg == 2) {
            t->rom_on = (t->cpu_addr == 0xFF3E);
        }
        ted_sub_off(t);
        t->phi0 = 0;
        t->aec = 0;
        t->dout_en = 0;
        t->rw_drive = 1;
        t->addr_en = 1;
        t->addr = 0xFFFF;
        ted_gfx_start(t);
        if (t->sub == SUB_NONE) t->rw_drive = 1;
    } else if (ph == 12) {
        if (t->gfx_ok) {
            unsigned g = part_read_bus(s, p, ted_d, 8);
            ted_draw_char(t, t->gfx_k, g);
            t->gfx_ok = 0;
        }
        ted_sub_off(t);
        t->addr = 0xFFFF;
        if (t->refresh && t->cyc >= 1 && t->cyc <= 5) {
            t->refresh--;
            ted_sub_start(t, SUB_REFRESH, (unsigned)(t->cyc * 37 + t->line) & 0xFF, false, 0);
        }
    }
    /* waveform of the current access */
    if (t->sub == SUB_CPU) {
        if (ph == 1) t->ras = 0;
        else if (ph == 2) ted_cpu_decode(s, p, t);
        else if (ph == 3) t->mux = 0;
        else if (ph == 4 && t->cpu_ram) t->cas = 0;
    } else {
        ted_sub_wave(t, (int)(ph & 3));
    }
    if (ted_sig(t) != before) sim_mark_dirty(s, (int)(p - s->parts));
}

static void ted_commit(sim_t *s, part_t *p) {
    ted_t *t = p->state;
    if (p->dead) return;
    int clk = pin_in(s, p, TP_CLKIN);
    if (clk && !t->clk) ted_clock(s, p, t);
    t->clk = (uint8_t)clk;
}

static void ted_reset(sim_t *s, part_t *p) {
    (void)s;
    ted_t *t = p->state;
    uint32_t *screen = t->screen;
    memset(t, 0, sizeof(*t));
    t->screen = screen;
    if (!ted_pal_ready) ted_make_palette();
    t->ph = 15;
    t->cyc = C16_CYCLES_LINE - 1;
    t->line = C16_LINES - 1;
    t->ras = t->cas = t->mux = t->cs0 = t->cs1 = t->ba = 1;
    t->rom_on = 1;
    t->reg[0x06] = 0x1B;
    t->reg[0x07] = 0x08;
    t->reg[0x15] = 0x71;
    t->reg[0x19] = 0xEE;
}

const part_type_t PT_7360 = { "7360 TED",
    "A2:B A1:B A0:B VDD:P /CS0:O /CS1:O R/W:B /IRQ:O MUX:O /RAS:O /CAS:O PHI0:O COLOR:O CLKIN:M "
    "K0:B K1:B K2:B K3:B K4:B K5:B K6:B K7:B LUM:O VSS:P D0:B D1:B D2:B D3:B D4:B D5:B D6:B D7:B SND:O BA:O AEC:O "
    "A15:B A14:B A13:B A12:B A11:B A10:B A9:B A8:B A7:B A6:B A5:B A4:B A3:B",
    ted_eval, ted_commit, ted_reset, sizeof(ted_t), TECH_NMOS };

void ted_set_screen(part_t *p, uint32_t *screen) { ((ted_t *)p->state)->screen = screen; }
uint8_t ted_reg_of(const part_t *p, int r) { return ((const ted_t *)p->state)->reg[r & 31]; }
uint32_t ted_frame_of(const part_t *p) { return ((const ted_t *)p->state)->frame; }
bool ted_rom_on(const part_t *p) { return ((const ted_t *)p->state)->rom_on != 0; }

/* The TED's state in a line, for reports. */
void ted_status_of(const part_t *p, char *buf, int len) {
    const ted_t *t = p->state;
    snprintf(buf, (size_t)len, "line=%u cyc=%u irq=%02X mask=%02X cmp=%02X t1=%04X/%04X rom=%d ff06=%02X ff07=%02X ff12=%02X ff13=%02X ff14=%02X",
             t->line, t->cyc, t->irq_status, t->reg[0x0A], t->reg[0x0B], t->t1, t->t1_rel, t->rom_on, t->reg[6], t->reg[7],
             t->reg[0x12], t->reg[0x13], t->reg[0x14]);
}
