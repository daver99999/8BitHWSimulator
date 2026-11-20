/* coco_chips.c - the chips of the Tandy Color Computer 2 that the other machines do not have: the MC6809E (a pin level
 * wrapper around chips/m6809.h), the MC6883 / SN74LS785 SAM (memory map, clock and DRAM timing, the video address
 * counter) and the MC6847T1 video chip (which also is the transceiver between the CPU's data bus and the memories').
 *
 * Timing. A CPU cycle is 16 periods of the 14.31818 MHz crystal, that is 32 steps of the simulation (one step is half
 * a period). The SAM numbers the steps of a cycle 0..31 and puts out E, Q, /RAS, /CAS, the multiplexed address and /WE
 * for the two memory accesses of the cycle: the CPU's in the second half (E high) and the video chip's in the first
 * (E low). The machine (coco2.c) advances the SAM's step counter once per step. */
#include "parts.h"
#include "../chips/m6809.h"
#include "coco_chips.h"
#include <string.h>

/* The 14.31818 MHz crystal X1: a clock source on pin 2 (the input of the SAM's oscillator, pin 5). */
static void xtal_coco_eval(sim_t *s, part_t *p) { pin_out(s, p, 2, DRV_BIT(s->clock_level)); }
const part_type_t PT_XTAL_COCO = { "14.31818 XTAL", "P1:A P2:O P3:P", xtal_coco_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* MC6809E                                                                   */
/* ------------------------------------------------------------------------ */

enum { C_NMI = 2, C_IRQ = 3, C_FIRQ = 4, C_BS = 5, C_BA = 6, C_A0 = 8, C_D7 = 24, C_RW = 32, C_BUSY = 33, C_E = 34,
       C_Q = 35, C_AVMA = 36, C_RESET = 37, C_LIC = 38, C_TSC = 39, C_HALT = 40 };
static const int c_addr[16] = { 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
static const int c_data[8] = { 31, 30, 29, 28, 27, 26, 25, 24 };       /* D0..D7 */

typedef struct {
    m6809_t cpu;
    uint8_t e;              /* E at the last step */
    uint8_t latch;          /* the data bus, as it was while E was high */
    int32_t cache_a, cache_d;
} cpu6809_t;

static void cpu6809_eval(sim_t *s, part_t *p) {
    cpu6809_t *c = p->state;
    const m6809_t *m = &c->cpu;
    bool tsc = pin_in(s, p, C_TSC);
    part_drive_bus_cached(s, p, c_addr, 16, m->addr, !tsc, &c->cache_a);
    pin_out(s, p, C_RW, tsc ? DRV_Z : DRV_BIT(m->rw));
    pin_out(s, p, C_BA, DRV_BIT(m->ba));
    pin_out(s, p, C_BS, DRV_BIT(m->bs));
    pin_out(s, p, C_AVMA, DRV_BIT(m->avma));
    pin_out(s, p, C_LIC, DRV_BIT(m->lic));
    pin_out(s, p, C_BUSY, DRV_BIT(m->bs && m->ba));
    /* a write puts its data on the bus while E is high */
    part_drive_bus_cached(s, p, c_data, 8, m->dout, c->e && !m->rw && !tsc && !m->ba, &c->cache_d);
}

/* After every step: while E is high the data bus is kept (the memories let go of it when E falls); at the step in
 * which E falls the cycle ends and the CPU takes what the bus carried. */
static void cpu6809_commit(sim_t *s, part_t *p) {
    cpu6809_t *c = p->state;
    int e = pin_in(s, p, C_E);
    if (e) c->latch = (uint8_t)part_read_bus(s, p, c_data, 8);
    if (c->e && !e) {
        m6809_cycle(&c->cpu, c->latch, pin_in(s, p, C_NMI), pin_in(s, p, C_FIRQ), pin_in(s, p, C_IRQ),
                    pin_in(s, p, C_RESET), pin_in(s, p, C_HALT));
        sim_mark_dirty(s, (int)(p - s->parts));
    }
    if (e != c->e) { c->e = (uint8_t)e; sim_mark_dirty(s, (int)(p - s->parts)); }
}

static void cpu6809_reset(sim_t *s, part_t *p) {
    (void)s;
    cpu6809_t *c = p->state;
    memset(c, 0, sizeof(*c));
    m6809_init(&c->cpu);
}

const part_type_t PT_6809E = { "MC6809E",
    "VSS:P /NMI:T /IRQ:T /FIRQ:T BS:O BA:O VCC:P A0:O A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O A10:O A11:O A12:O "
    "A13:O A14:O A15:O D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B R/W:O BUSY:O E:M Q:m AVMA:O /RESET:T LIC:O TSC:T /HALT:T",
    cpu6809_eval, cpu6809_commit, cpu6809_reset, sizeof(cpu6809_t), TECH_NMOS };

const m6809_t *coco_cpu_core(sim_t *s, int part) {
    cpu6809_t *c = s->parts[part].state;
    return &c->cpu;
}

/* ------------------------------------------------------------------------ */
/* MC6883 / SN74LS785 SAM                                                    */
/* ------------------------------------------------------------------------ */

/* Pins as the CoCo 2 board wires them. */
enum { S_OSCIN = 5, S_DA0 = 8, S_HS = 9, S_W = 10, S_CAS = 11, S_RAS = 12, S_Q = 13, S_E = 14, S_RW = 15, S_S2 = 25,
       S_S1 = 26, S_S0 = 27, S_Z0 = 28 };
static const int s_addr[16] = { 16, 17, 18, 19, 21, 22, 23, 24, 4, 3, 2, 1, 39, 38, 37, 36 };    /* A0..A15 */
static const int s_z[8] = { 28, 29, 30, 31, 32, 33, 34, 35 };                                       /* Z0..Z7 (MA0..7) */

/* The control bits, in the order of the registers at $FFC0..$FFDF (bit i is cleared by $FFC0+2i, set by $FFC1+2i). */
enum { SB_V0 = 0, SB_V1, SB_V2, SB_F0, SB_F1, SB_F2, SB_F3, SB_F4, SB_F5, SB_F6, SB_P1, SB_R0, SB_R1, SB_M0, SB_M1, SB_TY };

typedef struct {
    uint16_t bits;                  /* the control bits */
    uint8_t phase;                  /* the step of the cycle, 0..31 */
    uint8_t osc;                    /* the oscillator pin at the last step */
    uint8_t da0, hs;                /* the inputs at the last step */
    uint16_t counter;               /* the video address counter */
    uint8_t xdiv2, xdiv3, ydiv2, ydiv3, ydiv4, b4_in, b5_in;
    uint16_t vaddr;                 /* the video address of this cycle (counter, taken at step 2) */
    int32_t cache_s, cache_z;
} sam_t;

/* Which of the eight chip selects an address gives (S2 S1 S0 of the datasheet). */
static unsigned sam_select(const sam_t *c, unsigned a) {
    bool ty = (c->bits >> SB_TY) & 1;
    if (a < 0x8000) return 0;
    if (a < 0xFF00) {
        if (ty) return 0;
        return a < 0xA000 ? 1 : a < 0xC000 ? 2 : 3;
    }
    if (a < 0xFF20) return 4;
    if (a < 0xFF40) return 5;
    if (a < 0xFF60) return 6;
    if (a < 0xFFE0) return 7;
    return 2;                       /* the vectors are in the BASIC ROM */
}

/* The address a memory access of the CPU gives the RAM: the size of the RAM (M1 M0) limits it, and in the 64K mode P1
 * says which half of it the lower 32K of the map is. */
static unsigned sam_ram_addr(const sam_t *c, unsigned a) {
    unsigned m = (c->bits >> SB_M0) & 3;
    if (m == 0) return a & 0x0FFF;
    if (m == 1) return a & 0x3FFF;
    if (!((c->bits >> SB_TY) & 1) && a < 0x8000) return (a & 0x7FFF) | ((unsigned)((c->bits >> SB_P1) & 1) << 15);
    return a;
}

static unsigned sam_vid_addr(const sam_t *c) {
    unsigned m = (c->bits >> SB_M0) & 3;
    return c->counter & (m == 0 ? 0x0FFFu : m == 1 ? 0x3FFFu : 0xFFFFu);
}

/* --- the video address counter ------------------------------------------------------------------------------------
 * A chain of toggle stages: B0..B3 count the video chip's DA0 edges, B4 and B5 count what the X and Y dividers pass
 * on, and the mode (V2 V1 V0) says how a divider is used: the X divider (by 3 or 2) turns the 16 bytes of a graphics
 * row in B4's clock into rows of the display's size; the Y divider (by 12, 3 or 2) repeats a row of bytes for the
 * 12, 3 or 2 scan lines that show it. Every horizontal sync clears the low bits, so the line is read again. The dividers
 * run whatever the mode is; what changes is which output feeds the next stage. */
static bool sam_xout(const sam_t *c) {
    switch (c->bits & 7) {
    case 1: return c->xdiv3 >> 1;
    case 3: return c->xdiv2;
    default: return (c->counter >> 3) & 1;
    }
}
static bool sam_yout(const sam_t *c) {
    switch (c->bits & 7) {
    case 0: return c->ydiv4 >> 1;
    case 2: return c->ydiv3 >> 1;
    case 4: return c->ydiv2;
    default: return (c->counter >> 4) & 1;
    }
}
static void sam_set_b5(sam_t *c, bool level) {
    bool fall = c->b5_in && !level;
    c->b5_in = level;
    if (fall) c->counter = (uint16_t)(c->counter + 0x20);
}
static void sam_b4_falls(sam_t *c) {       /* the counter's bit 4 fell: the Y dividers count it */
    c->ydiv2 ^= 1;
    bool before = c->ydiv3 >> 1;
    c->ydiv3 = (uint8_t)((c->ydiv3 + 1) % 3);
    if (before && !(c->ydiv3 >> 1)) c->ydiv4 = (uint8_t)((c->ydiv4 + 1) & 3);
    sam_set_b5(c, sam_yout(c));
}
static void sam_set_b4(sam_t *c, bool level) {
    bool fall = c->b4_in && !level;
    c->b4_in = level;
    if (fall) {
        c->counter ^= 0x10;
        if (!((c->counter >> 4) & 1)) sam_b4_falls(c);      /* B4 itself fell: the Y dividers count it */
        else sam_set_b5(c, sam_yout(c));
    }
}
static void sam_b3_falls(sam_t *c, bool counted) {
    if (counted) {
        c->xdiv2 ^= 1;
        c->xdiv3 = (uint8_t)((c->xdiv3 + 1) % 3);
    }
    sam_set_b4(c, sam_xout(c));
}

/* The video chip toggled DA0: B0 follows it, and B0..B3 count. */
static void sam_da0_edge(sam_t *c, bool da0) {
    if (da0 == (bool)(c->counter & 1)) return;
    uint16_t old = c->counter;
    c->counter = (uint16_t)((c->counter & ~0xF) | ((c->counter + 1) & 0xF));
    if ((old ^ c->counter) & 8) sam_b3_falls(c, !((c->counter >> 3) & 1));
}

/* A horizontal sync: the low bits of the counter are cleared (B1..B3 in the modes where X divides, B1..B4 where it
 * doesn't), and the clearing carries into the next stage like any other fall would. */
static void sam_hsync(sam_t *c) {
    switch (c->bits & 7) {
    case 7: break;                          /* DMA: nothing */
    case 1: case 3: case 5: {
        bool fall = (c->counter >> 3) & 1;
        c->counter &= (uint16_t)~0x0F;
        sam_b3_falls(c, fall);
        break;
    }
    default: {
        bool fall = (c->counter >> 4) & 1;
        c->counter &= (uint16_t)~0x1F;
        c->b4_in = false;
        if (fall) sam_b4_falls(c); else sam_set_b5(c, sam_yout(c));
        break;
    }
    }
}

/* The vertical reset: the counter takes the display offset (F6..F0 are address bits 15..9) and the dividers clear. */
static void sam_vreset(sam_t *c) {
    c->counter = (uint16_t)(((c->bits >> SB_F0) & 0x7F) << 9);
    c->xdiv2 = c->xdiv3 = c->ydiv2 = c->ydiv3 = c->ydiv4 = 0;
    c->b4_in = c->b5_in = 0;
}

/* A mode change reconnects the divider outputs to the next stages, and a fall of the new output counts at once. */
static void sam_mode_changed(sam_t *c, unsigned old_mode) {
    static const uint8_t xd[8] = { 1, 3, 1, 2, 1, 1, 1, 1 }, yd[8] = { 12, 1, 3, 1, 2, 1, 1, 1 };
    unsigned nm = c->bits & 7;
    if (yd[old_mode] != yd[nm]) {
        if ((yd[old_mode] == 12 && yd[nm] == 3) || (yd[old_mode] == 3 && yd[nm] == 12)) sam_set_b5(c, false);
        else if ((yd[old_mode] == 12 && yd[nm] == 2) || (yd[old_mode] == 2 && yd[nm] == 12)) sam_set_b5(c, (c->counter >> 4) & 1);
        sam_set_b5(c, sam_yout(c));
    }
    if (xd[old_mode] != xd[nm]) {
        if ((xd[old_mode] == 2 && xd[nm] == 3) || (xd[old_mode] == 3 && xd[nm] == 2)) sam_set_b4(c, false);
        sam_set_b4(c, sam_xout(c));
    }
}

/* One step of a CPU cycle: which of the memory accesses has its /RAS and /CAS low, and the address the Z lines show. */
static void sam_eval(sim_t *s, part_t *p) {
    sam_t *c = p->state;
    unsigned k = c->phase;
    bool e = k >= 16, q = k >= 8 && k < 24;
    unsigned a = part_read_bus(s, p, s_addr, 16);
    unsigned sel = sam_select(c, a);
    /* the CPU's own access to the RAM happens only when it addresses RAM (the DRAM is not refreshed otherwise: it doesn't forget here) */
    bool cpu_ras = k >= 18 && k < 30 && sel == 0, vid_ras = k >= 4 && k < 14;
    bool cpu_cas = (k >= 22 || k < 2) && sel == 0, vid_cas = k >= 8 && k < 16;
    bool write = !pin_in(s, p, S_RW);
    unsigned z;
    if (k >= 16 && k < 20) z = sam_ram_addr(c, a) & 0xFF;                       /* CPU: row */
    else if (k >= 20 || k < 2) z = (sam_ram_addr(c, a) >> 8) & 0xFF;            /* CPU: column */
    else if (k < 6) z = c->vaddr & 0xFF;                                        /* video: row */
    else z = (c->vaddr >> 8) & 0xFF;                                            /* video: column */
    pin_out(s, p, S_E, DRV_BIT(e));
    pin_out(s, p, S_Q, DRV_BIT(q));
    pin_out(s, p, S_RAS, DRV_BIT(!(cpu_ras || vid_ras)));
    pin_out(s, p, S_CAS, DRV_BIT(!(cpu_cas || vid_cas)));
    pin_out(s, p, S_W, DRV_BIT(!((k >= 16 || k < 2) && write && sel == 0)));
    part_drive_bus_cached(s, p, s_z, 8, z, true, &c->cache_z);
    pin_out(s, p, S_S0, DRV_BIT(sel & 1));
    pin_out(s, p, S_S1, DRV_BIT((sel >> 1) & 1));
    pin_out(s, p, S_S2, DRV_BIT((sel >> 2) & 1));
}

static void sam_commit(sim_t *s, part_t *p) {
    sam_t *c = p->state;
    int part = (int)(p - s->parts);
    /* the last step of the CPU's access: a write to $FFC0..$FFDF sets or clears a control bit */
    if (c->phase == 31 && !pin_in(s, p, S_RW)) {
        unsigned a = part_read_bus(s, p, s_addr, 16);
        if (a >= 0xFFC0 && a < 0xFFE0) {
            unsigned i = (a - 0xFFC0) >> 1, set = a & 1;
            uint16_t old = c->bits;
            c->bits = (uint16_t)(set ? (old | (1u << i)) : (old & ~(1u << i)));
            if ((old ^ c->bits) & 7) sam_mode_changed(c, old & 7);
        }
    }
    bool da0 = pin_in(s, p, S_DA0), hs = pin_in(s, p, S_HS);
    if (da0 != (bool)c->da0) { c->da0 = da0; sam_da0_edge(c, da0); }
    if (hs != (bool)c->hs) {
        c->hs = hs;
        if (!hs) {                          /* /HS falls: with DA0 low (no line was fetched) the frame starts over */
            if (!da0) sam_vreset(c); else sam_hsync(c);
        }
    }
    int osc = pin_in(s, p, S_OSCIN);
    if (osc != c->osc) {
        c->osc = (uint8_t)osc;
        c->phase = (uint8_t)((c->phase + 1) & 31);
        if (c->phase == 2) c->vaddr = (uint16_t)sam_vid_addr(c);
        sim_mark_dirty(s, part);
    }
}

static void sam_reset(sim_t *s, part_t *p) {
    (void)s;
    sam_t *c = p->state;
    memset(c, 0, sizeof(*c));
}

const part_type_t PT_SAM = { "SN74LS785 SAM",
    "A11:M A10:M A9:M A8:M OSCIN:m OSCOUT:A VCC:P DA0:m /HS:m /W:O /CAS:O /RAS:O Q:O E:O R/W:M A0:M A1:M A2:M A3:M "
    "VSS:P A4:M A5:M A6:M A7:M S2:O S1:O S0:O Z0:O Z1:O Z2:O Z3:O Z4:O Z5:O Z6:O Z7:O A15:M A14:M A13:M A12:M VDD:P",
    sam_eval, sam_commit, sam_reset, sizeof(sam_t), TECH_NMOS };

unsigned coco_sam_bits(sim_t *s, int part) { sam_t *c = s->parts[part].state; return c->bits; }
unsigned coco_sam_counter(sim_t *s, int part) { sam_t *c = s->parts[part].state; return c->counter; }

/* ------------------------------------------------------------------------ */
/* MC6847T1 video display generator                                          */
/* ------------------------------------------------------------------------ */

/* The character generator of the 6847 (64 characters of 8 x 12 dots) and the colours of its outputs, as measured on real
 * chips, are from Andre Weissflog's chips project (mc6847.h, zlib/libpng license, Copyright (c) 2018 Andre Weissflog:
 * https://github.com/floooh/chips), where the font was taken from MAME's dump of the chip. This copy is altered: it is
 * used by the pin level model below and not by the original code. */
static const uint8_t vdg_font[64 * 12] = {
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x1A, 0x2A, 0x2A, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x12, 0x12, 0x1C, 0x12, 0x12, 0x3C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x12, 0x12, 0x12, 0x12, 0x12, 0x3C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3E, 0x20, 0x20, 0x38, 0x20, 0x20, 0x3E, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3E, 0x20, 0x20, 0x38, 0x20, 0x20, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1E, 0x20, 0x20, 0x26, 0x22, 0x22, 0x1E, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x22, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3E, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x36, 0x2A, 0x2A, 0x22, 0x22, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x32, 0x2A, 0x26, 0x22, 0x22, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3E, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x2A, 0x24, 0x1A, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x28, 0x24, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x10, 0x08, 0x04, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x2A, 0x2A, 0x36, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3E, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x20, 0x20, 0x10, 0x08, 0x04, 0x02, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x1C, 0x2A, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x3E, 0x10, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x14, 0x14, 0x36, 0x00, 0x36, 0x14, 0x14, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x1E, 0x20, 0x1C, 0x02, 0x3C, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x28, 0x28, 0x10, 0x2A, 0x24, 0x1A, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x18, 0x24, 0x24, 0x24, 0x24, 0x24, 0x18, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x1C, 0x20, 0x20, 0x3E, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x04, 0x02, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x0C, 0x14, 0x3E, 0x04, 0x04, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3E, 0x20, 0x3C, 0x02, 0x02, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x3E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x18, 0x24, 0x04, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
};

/* Colour numbers: the eight colours of the graphics modes, then the alphanumeric ones and black. */
enum { VC_GREEN, VC_YELLOW, VC_BLUE, VC_RED, VC_BUFF, VC_CYAN, VC_MAGENTA, VC_ORANGE, VC_ALN_FG_G, VC_ALN_BG_G, VC_ALN_FG_O,
       VC_ALN_BG_O, VC_BLACK, VC_NUM };
#define VRGB(r, g, b) (0xFF000000u | (unsigned)(((r) * 4 / 3) > 255 ? 255 : ((r) * 4 / 3)) | \
                       ((unsigned)(((g) * 4 / 3) > 255 ? 255 : ((g) * 4 / 3)) << 8) | \
                       ((unsigned)(((b) * 4 / 3) > 255 ? 255 : ((b) * 4 / 3)) << 16))
static const uint32_t vdg_colors[VC_NUM] = {
    VRGB(19, 146, 11), VRGB(155, 150, 10), VRGB(2, 22, 175), VRGB(155, 22, 7), VRGB(141, 150, 154), VRGB(15, 143, 155),
    VRGB(139, 39, 155), VRGB(140, 31, 11), VRGB(19, 146, 11), 0xFF002400u | 0x00000000u, VRGB(140, 31, 11), 0xFF220E00u, 0xFF111111u,
};

/* Pins of the CoCo 2's 6847T1. */
enum { V_WE = 12, V_RAS = 13, V_DA0 = 22, V_BUSEN = 24, V_GM2 = 27, V_GM1 = 29, V_GM0 = 30, V_AG = 35, V_FS = 37, V_HS = 38,
       V_CSS = 39 };
static const int v_dd[8] = { 8, 7, 6, 5, 4, 3, 2, 40 };            /* DD0..DD7: the memories' data bus */
static const int v_d[8] = { 23, 21, 20, 19, 18, 16, 15, 14 };       /* D0..D7: the CPU's */

/* A line is 227.5 colour clocks of 8 steps; a frame 262 lines. */
#define V_LINE_STEPS 1820
#define V_LINES 262
#define V_FETCH_AT 256              /* the step of the line in which the line's bytes start to be fetched */
#define V_FIRST_LINE 38             /* the first line of the picture (192 of them), lines 14..37 and 230..253 are the border */
#define V_BOTTOM 230
#define V_FB_TOP 14

typedef struct {
    uint32_t fb[COCO_FB_W * COCO_FB_H];
    uint32_t frames;
    uint16_t h, line;
    uint8_t ras, da0, hs, fs;
    uint8_t fetching, idx, nb, dummy;
    uint8_t ag, css, gm0, gm1, gm2;     /* the mode at the start of the line */
    uint8_t chr[32];
    int32_t cache_dd, cache_d;
    uint8_t clk, took;                  /* the plain 6847: the CLK pin's level, the sample of this cycle is taken */
    const uint8_t *sync;                /* the plain 6847: the SAM's step of the cycle (see vdg47_commit) */
    uint8_t lcount, lpr;                /* the MC-10's 6847 (it makes its own addresses): the line of the row, lines in a row */
    uint16_t abase, addr;               /* the start of the row, the address on DA0..DA11 */
    int32_t cache_da;
} vdg_t;

static void vdg_draw_line(vdg_t *v, int line) {
    uint32_t *dst = &v->fb[(line - V_FB_TOP) * COCO_FB_W];
    uint32_t border = v->ag ? vdg_colors[v->css ? VC_BUFF : VC_GREEN] : vdg_colors[VC_BLACK];
    for (int x = 0; x < 32; x++) dst[x] = dst[x + 288] = border;
    uint32_t *o = dst + 32;
    int y = line - V_FIRST_LINE;
    if (v->ag) {
        unsigned sub = (unsigned)(v->gm2 << 1 | v->gm1);
        if (v->gm0) {                       /* a resolution mode: one bit a dot (or two) */
            int dots = sub < 3 ? 2 : 1;
            uint32_t fg = vdg_colors[v->css ? VC_BUFF : VC_GREEN];
            for (int i = 0; i < v->nb; i++)
                for (int b = 7; b >= 0; b--) {
                    uint32_t c = (v->chr[i] >> b) & 1 ? fg : vdg_colors[VC_BLACK];
                    for (int d = 0; d < dots; d++) *o++ = c;
                }
        } else {                            /* a colour mode: two bits a dot */
            int dots = sub == 0 ? 4 : 2;
            for (int i = 0; i < v->nb; i++)
                for (int b = 6; b >= 0; b -= 2) {
                    uint32_t c = vdg_colors[((v->chr[i] >> b) & 3) + (v->css ? 4 : 0)];
                    for (int d = 0; d < dots; d++) *o++ = c;
                }
        }
    } else {
        int cy = y % 12;
        int sh4 = (1 - cy / 6) * 2, sh6 = (2 - cy / 4) * 2;
        uint32_t fg = vdg_colors[v->css ? VC_ALN_FG_O : VC_ALN_FG_G], bg = vdg_colors[v->css ? VC_ALN_BG_O : VC_ALN_BG_G];
        for (int i = 0; i < 32; i++) {
            uint8_t ch = v->chr[i];
            if (ch & 0x80) {                /* semigraphics: 2x2 blocks (4 colours bits) or 2x3 (INT/EXT high) */
                unsigned m, col;
                if (v->gm0) { m = (ch >> sh6) & 3; col = ((ch >> 6) & 3) + (v->css ? 4u : 0u); }
                else { m = (ch >> sh4) & 3; col = (ch >> 4) & 7; }
                for (int p = 1; p >= 0; p--) {
                    uint32_t c = (m >> p) & 1 ? vdg_colors[col] : vdg_colors[VC_BLACK];
                    for (int d = 0; d < 4; d++) *o++ = c;
                }
            } else {
                uint8_t m = vdg_font[(ch & 0x3F) * 12 + cy];
                if (ch & 0x40) m = (uint8_t)~m;
                for (int b = 7; b >= 0; b--) *o++ = (m >> b) & 1 ? fg : bg;
            }
        }
    }
}

static void vdg_border_line(vdg_t *v, int line, bool ag, bool css) {
    uint32_t c = ag ? vdg_colors[css ? VC_BUFF : VC_GREEN] : vdg_colors[VC_BLACK];
    uint32_t *dst = &v->fb[(line - V_FB_TOP) * COCO_FB_W];
    for (int x = 0; x < COCO_FB_W; x++) dst[x] = c;
}

/* The transceiver between the CPU's data bus and the memories': while /BUS_ENABLE is low the CPU's cycle goes through, one
 * way or the other as /WE says; the DD bus is what the video chip itself reads in its own cycles. */
static void vdg_eval(sim_t *s, part_t *p) {
    vdg_t *v = p->state;
    bool en = !pin_in(s, p, V_BUSEN), wr = !pin_in(s, p, V_WE);
    if (en && wr) {
        part_drive_bus_cached(s, p, v_dd, 8, part_read_bus(s, p, v_d, 8), true, &v->cache_dd);
        part_drive_bus_cached(s, p, v_d, 8, 0, false, &v->cache_d);
    } else if (en) {
        part_drive_bus_cached(s, p, v_d, 8, part_read_bus(s, p, v_dd, 8), true, &v->cache_d);
        part_drive_bus_cached(s, p, v_dd, 8, 0, false, &v->cache_dd);
    } else {
        part_drive_bus_cached(s, p, v_dd, 8, 0, false, &v->cache_dd);
        part_drive_bus_cached(s, p, v_d, 8, 0, false, &v->cache_d);
    }
    pin_out(s, p, V_DA0, DRV_BIT(v->da0));
    pin_out(s, p, V_FS, DRV_BIT(!v->fs));
    pin_out(s, p, V_HS, DRV_BIT(!v->hs));
}

static void vdg_commit(sim_t *s, part_t *p) {
    vdg_t *v = p->state;
    int part = (int)(p - s->parts);
    bool dirty = false;
    /* the video access: the memories' /RAS rises while the data is still on the DD bus, outside of the CPU's cycles */
    int ras = pin_in(s, p, V_RAS);
    if (ras && !v->ras && pin_in(s, p, V_BUSEN) && v->fetching) {
        uint8_t byte = (uint8_t)part_read_bus(s, p, v_dd, 8);
        v->chr[v->idx] = byte;
        v->idx++;
        if (v->idx >= v->nb) {
            v->fetching = 0;
            vdg_draw_line(v, v->line);
        } else {
            v->da0 = v->idx & 1;
            dirty = true;
        }
    }
    v->ras = (uint8_t)ras;
    /* the position in the frame */
    v->h++;
    if (v->h >= V_LINE_STEPS) {
        v->h = 0;
        v->line++;
        if (v->line >= V_LINES) {
            v->line = 0;
            v->fs = 0;
            v->frames++;
            dirty = true;
        }
        if (v->line == V_BOTTOM) { v->fs = 1; dirty = true; }
        if ((v->line >= V_FB_TOP && v->line < V_FIRST_LINE) || (v->line >= V_BOTTOM && v->line < V_BOTTOM + 24))
            vdg_border_line(v, v->line, pin_in(s, p, V_AG), pin_in(s, p, V_CSS));
    }
    if (v->h == 80) { v->hs = 1; dirty = true; }
    else if (v->h == 208) { v->hs = 0; dirty = true; }
    else if (v->h == 220 && v->da0) { v->da0 = 0; dirty = true; }
    if (v->h == V_FETCH_AT && v->line >= V_FIRST_LINE && v->line < V_BOTTOM) {
        v->ag = (uint8_t)pin_in(s, p, V_AG);
        v->css = (uint8_t)pin_in(s, p, V_CSS);
        v->gm0 = (uint8_t)pin_in(s, p, V_GM0);
        v->gm1 = (uint8_t)pin_in(s, p, V_GM1);
        v->gm2 = (uint8_t)pin_in(s, p, V_GM2);
        unsigned sub = (unsigned)(v->gm2 << 1 | v->gm1);
        v->nb = 32;
        if (v->ag && (v->gm0 ? sub < 3 : sub == 0)) v->nb = 16;
        v->idx = 0;
        v->fetching = 1;
        v->da0 = 0;
        dirty = true;
    }
    if (dirty) sim_mark_dirty(s, part);
}

static void vdg_reset(sim_t *s, part_t *p) {
    (void)s;
    vdg_t *v = p->state;
    const uint8_t *sync = v->sync;          /* set by the machine at its creation */
    memset(v, 0, sizeof(*v));
    v->sync = sync;
    for (int i = 0; i < COCO_FB_W * COCO_FB_H; i++) v->fb[i] = 0xFF000000u;
}

const part_type_t PT_6847T1 = { "MC6847T1",
    "VSS:P DD6:X DD5:X DD4:X DD3:X DD2:X DD1:X DD0:X CHB:A Y:A PHA:A /WE:M /RAS:m D7:X D6:X D5:X VCC:P D4:X D3:X D2:X "
    "D1:X DA0:O D0:X /BUSEN:M /BURST:A NC:A GM2:m A:A GM1:m GM0:m VSS:P NC:A CLK:A NC:A A/G:m NC:A /FS:O /HS:O CSS:m DD7:X",
    vdg_eval, vdg_commit, vdg_reset, sizeof(vdg_t), TECH_NMOS };

const uint32_t *coco_vdg_screen(sim_t *s, int part) { vdg_t *v = s->parts[part].state; return v->fb; }
uint32_t coco_vdg_frames(sim_t *s, int part) { vdg_t *v = s->parts[part].state; return v->frames; }

/* For tests: the line the video chip drew last and the bytes it fetched for it. */
int coco_vdg_last_line(sim_t *s, int part, uint8_t *chr, int *nb) {
    vdg_t *v = s->parts[part].state;
    memcpy(chr, v->chr, sizeof(v->chr));
    *nb = v->nb;
    return v->line;
}

/* ------------------------------------------------------------------------ */
/* MC6883 SAM (the Dragon 64's): as the 74LS785, with the VCLK output on pin 7 */
/* ------------------------------------------------------------------------ */

/* The 6883 divides its oscillator by 4 for the video chip: VCLK is 8 steps a period (the board gates it for the 6847). */
static void sam6883_eval(sim_t *s, part_t *p) {
    sam_eval(s, p);
    const sam_t *c = p->state;
    pin_out(s, p, 7, DRV_BIT((c->phase >> 2) & 1));
}
const part_type_t PT_SAM6883 = { "MC6883 SAM",
    "A11:M A10:M A9:M A8:M OSCIN:m OSCOUT:A VCLK:O DA0:m /HS:m /W:O /CAS:O /RAS:O Q:O E:O R/W:M A0:M A1:M A2:M A3:M "
    "VSS:P A4:M A5:M A6:M A7:M S2:O S1:O S0:O Z0:O Z1:O Z2:O Z3:O Z4:O Z5:O Z6:O Z7:O A15:M A14:M A13:M A12:M VDD:P",
    sam6883_eval, sam_commit, sam_reset, sizeof(sam_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* MC6847 video display generator (the Dragon's)                             */
/* ------------------------------------------------------------------------ */

/* The plain 6847 has no bus transceiver: it reads the memories' data from the DD bus by itself and counts its own CLK pin, which
 * the Dragon's board gates and stretches to get the PAL line and field. The chip reads DD at a fixed point after its address
 * output DA0 changes; where that point is within the SAM's cycle is not something the chip knows by pins (it comes from the
 * counting of its clock), so the machine tells the model the SAM's step (coco_vdg_sync): the data is taken while the latch
 * that holds the RAM's byte (the board's 74LS273) is valid. */
enum { V47_DA0 = 22, V47_GM2 = 27, V47_GM1 = 29, V47_GM0 = 30, V47_CLK = 33, V47_AG = 35, V47_FS = 37, V47_HS = 38, V47_CSS = 39 };
static const int v47_dd[8] = { 3, 4, 5, 6, 7, 8, 2, 40 };           /* DD0..DD7 as the Dragon wires them */
#define V47_LINE_CLKS 228
#define V47_SAMPLE_STEP 20

static void vdg47_eval(sim_t *s, part_t *p) {
    vdg_t *v = p->state;
    pin_out(s, p, V47_DA0, DRV_BIT(v->da0));
    pin_out(s, p, V47_FS, DRV_BIT(!v->fs));
    pin_out(s, p, V47_HS, DRV_BIT(!v->hs));
}

static void vdg47_commit(sim_t *s, part_t *p) {
    vdg_t *v = p->state;
    int part = (int)(p - s->parts);
    bool dirty = false;
    if (v->sync) {
        if (*v->sync != V47_SAMPLE_STEP) v->took = 0;
        else if (!v->took) {
            v->took = 1;
            if (v->fetching) {
                v->chr[v->idx++] = (uint8_t)part_read_bus(s, p, v47_dd, 8);
                if (v->idx >= v->nb) { v->fetching = 0; vdg_draw_line(v, v->line); }
                else v->da0 = v->idx & 1;
                dirty = true;
            }
        }
    }
    int clk = pin_in(s, p, V47_CLK);
    if (clk && !v->clk) {
        v->h++;
        if (v->h >= V47_LINE_CLKS) {
            v->h = 0;
            v->line++;
            if (v->line >= V_LINES) { v->line = 0; v->fs = 0; v->frames++; dirty = true; }
            if (v->line == V_BOTTOM) { v->fs = 1; dirty = true; }
            if ((v->line >= V_FB_TOP && v->line < V_FIRST_LINE) || (v->line >= V_BOTTOM && v->line < V_BOTTOM + 24))
                vdg_border_line(v, v->line, pin_in(s, p, V47_AG), pin_in(s, p, V47_CSS));
        }
        if (v->h == 10) { v->hs = 1; dirty = true; }
        else if (v->h == 26) { v->hs = 0; dirty = true; }
        else if (v->h == 28 && v->da0) { v->da0 = 0; dirty = true; }
        if (v->h == 32 && v->line >= V_FIRST_LINE && v->line < V_BOTTOM) {
            v->ag = (uint8_t)pin_in(s, p, V47_AG);
            v->css = (uint8_t)pin_in(s, p, V47_CSS);
            v->gm0 = (uint8_t)pin_in(s, p, V47_GM0);
            v->gm1 = (uint8_t)pin_in(s, p, V47_GM1);
            v->gm2 = (uint8_t)pin_in(s, p, V47_GM2);
            unsigned sub = (unsigned)(v->gm2 << 1 | v->gm1);
            v->nb = 32;
            if (v->ag && (v->gm0 ? sub < 3 : sub == 0)) v->nb = 16;
            v->idx = 0;
            v->fetching = 1;
            v->da0 = 0;
            dirty = true;
        }
    }
    v->clk = (uint8_t)clk;
    if (dirty) sim_mark_dirty(s, part);
}

const part_type_t PT_6847 = { "MC6847",
    "VSS:P DD6:M DD0:M DD1:M DD2:M DD3:M DD4:M DD5:M CHB:A oB:A oA:A /MS:m DA5:A DA6:A DA7:A DA8:A VCC:P DA9:A DA10:A "
    "DA11:A DA12:A DA0:O DA1:A DA2:A DA3:A DA4:A GM2:m Y:A GM1:m GM0:m INT/EXT:m INV:m CLK:M A/S:m A/G:m /RP:m /FS:O /HS:O "
    "CSS:m DD7:M",
    vdg47_eval, vdg47_commit, vdg_reset, sizeof(vdg_t), TECH_NMOS };

/* The MC-10's 6847: no SAM and no 74LS273 here, the chip makes the video RAM's addresses itself (DA0..DA11, which it lets go
 * of while /MS is low, the CPU's access) and reads the byte from the RAM bus a few clocks later. A row of the picture is
 * repeated for 12 lines of text, 3, 2 or 1 line(s) of graphics (by the mode); the address counter starts at 0 each frame. */
static const int v10_da[12] = { 22, 23, 24, 25, 26, 13, 14, 15, 16, 18, 19, 20 };
enum { V10_MS = 12 };
/* The board's /HS circuit (74LS74 + 74LS76) locks the 6847's clock so that /HS falls in the CPU's write window, in the first
 * half of an E cycle; the address then goes out a whole number of cells after that in the E low half and the byte is read half a
 * cell later, still in the E low half, where the CPU leaves the RAM bus alone. */
#define V10_FETCH_AT 34

static void vdg10_eval(sim_t *s, part_t *p) {
    vdg_t *v = p->state;
    part_drive_bus_cached(s, p, v10_da, 12, v->addr, pin_in(s, p, V10_MS), &v->cache_da);
    pin_out(s, p, V47_FS, DRV_BIT(!v->fs));
    pin_out(s, p, V47_HS, DRV_BIT(!v->hs));
}

static void vdg10_commit(sim_t *s, part_t *p) {
    vdg_t *v = p->state;
    int part = (int)(p - s->parts);
    bool dirty = false;
    int clk = pin_in(s, p, V47_CLK);
    if (clk && !v->clk) {
        v->h++;
        if (v->h >= V47_LINE_CLKS) {
            v->h = 0;
            v->line++;
            if (v->line >= V_LINES) { v->line = 0; v->fs = 0; v->frames++; dirty = true; }
            if (v->line == V_BOTTOM) { v->fs = 1; dirty = true; }
            if ((v->line >= V_FB_TOP && v->line < V_FIRST_LINE) || (v->line >= V_BOTTOM && v->line < V_BOTTOM + 24))
                vdg_border_line(v, v->line, pin_in(s, p, V47_AG), pin_in(s, p, V47_CSS));
        }
        if (v->h == 10) { v->hs = 1; dirty = true; }
        else if (v->h == 26) { v->hs = 0; dirty = true; }
        if (v->h == V10_FETCH_AT && v->line >= V_FIRST_LINE && v->line < V_BOTTOM) {
            v->ag = (uint8_t)pin_in(s, p, V47_AG);
            v->css = (uint8_t)pin_in(s, p, V47_CSS);
            v->gm0 = (uint8_t)pin_in(s, p, V47_GM0);
            v->gm1 = (uint8_t)pin_in(s, p, V47_GM1);
            v->gm2 = (uint8_t)pin_in(s, p, V47_GM2);
            unsigned sub = (unsigned)(v->gm2 << 1 | v->gm1);
            v->nb = 32;
            if (v->ag && (v->gm0 ? sub < 3 : sub == 0)) v->nb = 16;
            static const uint8_t lines_per_row[8] = { 3, 3, 3, 2, 2, 1, 1, 1 };      /* G1C G1R G2C G2R G3C G3R G6C G6R */
            v->lpr = v->ag ? lines_per_row[v->gm2 << 2 | v->gm1 << 1 | v->gm0] : 12;
            if (v->line == V_FIRST_LINE) { v->abase = 0; v->lcount = 0; }
            v->idx = 0;
            v->fetching = 1;
            dirty = true;
        }
        if (v->fetching && v->h >= V10_FETCH_AT) {
            unsigned cell = v->nb == 32 ? 4u : 8u, rel = (unsigned)(v->h - V10_FETCH_AT), k = rel / cell, ph = rel % cell;
            if (k < v->nb && ph == 0) { v->addr = (uint16_t)((v->abase + k) & 0xFFF); dirty = true; }
            else if (k < v->nb && ph == cell / 2) {
                v->chr[k] = (uint8_t)part_read_bus(s, p, v47_dd, 8);
                if (k + 1 == v->nb) {
                    v->fetching = 0;
                    vdg_draw_line(v, v->line);
                    if (++v->lcount >= v->lpr) { v->lcount = 0; v->abase = (uint16_t)(v->abase + v->nb); }
                    dirty = true;
                }
            }
        }
    }
    v->clk = (uint8_t)clk;
    if (dirty) sim_mark_dirty(s, part);
}

const part_type_t PT_6847_MC10 = { "MC6847",
    "VSS:P DD6:M DD0:M DD1:M DD2:M DD3:M DD4:M DD5:M CHB:A oB:A oA:A /MS:T DA5:O DA6:O DA7:O DA8:O VCC:P DA9:O DA10:O DA11:O "
    "DA12:O DA0:O DA1:O DA2:O DA3:O DA4:O GM2:m Y:A GM1:m GM0:m INT/EXT:m INV:m CLK:M A/S:m A/G:m /RP:m /FS:O /HS:O "
    "CSS:m DD7:M",
    vdg10_eval, vdg10_commit, vdg_reset, sizeof(vdg_t), TECH_NMOS };

void coco_vdg_sync(sim_t *s, int vdg, int sam) {
    vdg_t *v = s->parts[vdg].state;
    sam_t *c = s->parts[sam].state;
    v->sync = &c->phase;
}

/* ------------------------------------------------------------------------ */
/* 74LS273, 74LS393 and 74LS390 (the Dragon's RAM data latch and its PAL timing counters)     */
/* ------------------------------------------------------------------------ */

typedef struct { uint8_t q, clk; } ls273_t;
static const int l273_d[8] = { 3, 4, 7, 8, 13, 14, 17, 18 }, l273_q[8] = { 2, 5, 6, 9, 12, 15, 16, 19 };
static void ls273_eval(sim_t *s, part_t *p) {
    const ls273_t *t = p->state;
    for (int i = 0; i < 8; i++) pin_out(s, p, l273_q[i], DRV_BIT((t->q >> i) & 1));
}
static void ls273_commit(sim_t *s, part_t *p) {
    ls273_t *t = p->state;
    int clk = pin_in(s, p, 11);
    uint8_t q = t->q;
    if (!pin_in(s, p, 1)) q = 0;
    else if (clk && !t->clk) q = (uint8_t)part_read_bus(s, p, l273_d, 8);
    t->clk = (uint8_t)clk;
    if (q != t->q) { t->q = q; sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void ls273_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls273_t)); }
const part_type_t PT_74LS273 = { "74LS273",
    "/MR:T Q0:O D0:T D1:T Q1:O Q2:O D2:T D3:T Q3:O GND:P CLK:T Q4:O D4:T D5:T Q5:O Q6:O D6:T D7:T Q7:O VCC:P",
    ls273_eval, ls273_commit, ls273_reset, sizeof(ls273_t), TECH_TTL };

/* 74LS393: two 4 bit ripple counters, counting on the falling edge of CP, MR clears. */
typedef struct { uint8_t n[2], cp[2]; } ls393_t;
static void ls393_eval(sim_t *s, part_t *p) {
    const ls393_t *t = p->state;
    static const int q[2][4] = { { 3, 4, 5, 6 }, { 11, 10, 9, 8 } };
    for (int h = 0; h < 2; h++)
        for (int i = 0; i < 4; i++) pin_out(s, p, q[h][i], DRV_BIT((t->n[h] >> i) & 1));
}
static void ls393_commit(sim_t *s, part_t *p) {
    ls393_t *t = p->state;
    static const int cp[2] = { 1, 13 }, mr[2] = { 2, 12 };
    bool ch = false;
    for (int h = 0; h < 2; h++) {
        int c = pin_in(s, p, cp[h]);
        uint8_t n = t->n[h];
        if (pin_in(s, p, mr[h])) n = 0;
        else if (!c && t->cp[h]) n = (uint8_t)((n + 1) & 15);
        t->cp[h] = (uint8_t)c;
        if (n != t->n[h]) { t->n[h] = n; ch = true; }
    }
    if (ch) sim_mark_dirty(s, (int)(p - s->parts));
}
static void ls393_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls393_t)); }
const part_type_t PT_74LS393 = { "74LS393",
    "1CP:T 1MR:T 1Q0:O 1Q1:O 1Q2:O 1Q3:O GND:P 2Q3:O 2Q2:O 2Q1:O 2Q0:O 2MR:T 2CP:T VCC:P",
    ls393_eval, ls393_commit, ls393_reset, sizeof(ls393_t), TECH_TTL };

/* 74LS390: two decade counters, each a divide by 2 (CP0 -> Q0) and a divide by 5 (CP1 -> Q1 Q2 Q3), falling edge. */
typedef struct { uint8_t two[2], five[2], cp0[2], cp1[2]; } ls390_t;
static void ls390_eval(sim_t *s, part_t *p) {
    const ls390_t *t = p->state;
    static const int q[2][4] = { { 3, 5, 6, 7 }, { 13, 11, 10, 9 } };
    for (int h = 0; h < 2; h++) {
        pin_out(s, p, q[h][0], DRV_BIT(t->two[h]));
        for (int i = 0; i < 3; i++) pin_out(s, p, q[h][i + 1], DRV_BIT((t->five[h] >> i) & 1));
    }
}
static void ls390_commit(sim_t *s, part_t *p) {
    ls390_t *t = p->state;
    static const int cp0[2] = { 1, 15 }, cp1[2] = { 4, 12 }, mr[2] = { 2, 14 };
    bool ch = false;
    for (int h = 0; h < 2; h++) {
        int c0 = pin_in(s, p, cp0[h]), c1 = pin_in(s, p, cp1[h]);
        uint8_t two = t->two[h], five = t->five[h];
        if (pin_in(s, p, mr[h])) two = five = 0;
        else {
            if (!c0 && t->cp0[h]) two ^= 1;
            if (!c1 && t->cp1[h]) five = five >= 4 ? 0 : (uint8_t)(five + 1);
        }
        t->cp0[h] = (uint8_t)c0;
        t->cp1[h] = (uint8_t)c1;
        if (two != t->two[h] || five != t->five[h]) { t->two[h] = two; t->five[h] = five; ch = true; }
    }
    if (ch) sim_mark_dirty(s, (int)(p - s->parts));
}
static void ls390_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls390_t)); }
const part_type_t PT_74LS390 = { "74LS390",
    "1CP0:T 1MR:T 1Q0:O 1CP1:T 1Q1:O 1Q2:O 1Q3:O GND:P 2Q3:O 2Q2:O 2Q1:O 2CP1:T 2Q0:O 2MR:T 2CP0:T VCC:P",
    ls390_eval, ls390_commit, ls390_reset, sizeof(ls390_t), TECH_TTL };

/* ------------------------------------------------------------------------ */
/* 74LS74: two D flip-flops with preset and clear (the Dragon's)             */
/* ------------------------------------------------------------------------ */

typedef struct { uint8_t q[2], ck[2]; } ls74_t;
static const int ls74_pin[2][6] = { { 1, 2, 3, 4, 5, 6 }, { 13, 12, 11, 10, 9, 8 } };       /* /CLR D CK /PR Q /Q */
static void ls74_eval(sim_t *s, part_t *p) {
    const ls74_t *t = p->state;
    for (int h = 0; h < 2; h++) {
        pin_out(s, p, ls74_pin[h][4], DRV_BIT(t->q[h]));
        pin_out(s, p, ls74_pin[h][5], DRV_BIT(!t->q[h]));
    }
}
static void ls74_commit(sim_t *s, part_t *p) {
    ls74_t *t = p->state;
    bool ch = false;
    for (int h = 0; h < 2; h++) {
        int clr = pin_in(s, p, ls74_pin[h][0]), d = pin_in(s, p, ls74_pin[h][1]), ck = pin_in(s, p, ls74_pin[h][2]),
            pr = pin_in(s, p, ls74_pin[h][3]);
        uint8_t q = t->q[h];
        if (!pr) q = 1;                     /* both low: this chip gives both outputs high; the board never does it */
        else if (!clr) q = 0;
        else if (ck && !t->ck[h]) q = (uint8_t)d;
        t->ck[h] = (uint8_t)ck;
        if (q != t->q[h]) { t->q[h] = q; ch = true; }
    }
    if (ch) sim_mark_dirty(s, (int)(p - s->parts));
}
static void ls74_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls74_t)); }
const part_type_t PT_74LS74F = { "74LS74",
    "/1CLR:T 1D:T 1CK:T /1PR:T 1Q:O /1Q:O GND:P /2Q:O 2Q:O /2PR:T 2CK:T 2D:T /2CLR:T VCC:P",
    ls74_eval, ls74_commit, ls74_reset, sizeof(ls74_t), TECH_TTL };
