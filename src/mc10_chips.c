/* mc10_chips.c - the chips of the Tandy MC-10 that the other machines do not have: the MC6803 (a pin level wrapper around
 * chips/m6803.h with the multiplexed address/data bus), the 74LS155 decoder, the 74LS367 bus drivers, the 74LS76 flip-flops,
 * the MC14503 keyboard buffer, the TMS2564 ROM and the clock source that stands for the MC1372 video chip's oscillator.
 *
 * Timing. The 6803 divides its EXTAL input (3.579545 MHz) by 4 to get E: a cycle is 4 periods of EXTAL, that is 8 steps of the
 * simulation (a step is half a period). The part counts the edges of EXTAL (h = 0..7): the address goes out on AD0..AD7 with AS
 * high in the first 4 half periods, then E is high for 4 and the data is on the pins; the cycle ends when E falls. */
#include "parts.h"
#include "../chips/m6803.h"
#include "mc10_chips.h"
#include <string.h>

/* ------------------------------------------------------------------------ */
/* MC6803                                                                    */
/* ------------------------------------------------------------------------ */

enum { C3_EXTAL = 3, C3_NMI = 4, C3_IRQ = 5, C3_RESET = 6, C3_RW = 38, C3_AS = 39, C3_E = 40 };
static const int c3_p1[8] = { 13, 14, 15, 16, 17, 18, 19, 20 };      /* P10..P17 */
static const int c3_p2[5] = { 8, 9, 10, 11, 12 };                    /* P20..P24 */
static const int c3_hi[8] = { 29, 28, 27, 26, 25, 24, 23, 22 };      /* A8..A15 */
static const int c3_ad[8] = { 37, 36, 35, 34, 33, 32, 31, 30 };      /* AD0..AD7 */

typedef struct {
    m6803_t cpu;
    uint8_t h, extal, latch;
    int32_t cache_hi, cache_ad;
} cpu6803_t;

static void cpu6803_eval(sim_t *s, part_t *p) {
    cpu6803_t *c = p->state;
    const m6803_t *m = &c->cpu;
    bool e = c->h >= 4;
    pin_out(s, p, C3_E, DRV_BIT(e));
    pin_out(s, p, C3_AS, DRV_BIT(c->h <= 2));
    pin_out(s, p, C3_RW, DRV_BIT(m->rw));
    part_drive_bus_cached(s, p, c3_hi, 8, (unsigned)(m->addr >> 8), true, &c->cache_hi);
    if (!e) part_drive_bus_cached(s, p, c3_ad, 8, m->addr & 0xFFu, true, &c->cache_ad);
    else part_drive_bus_cached(s, p, c3_ad, 8, m->dout, m->ext && !m->rw, &c->cache_ad);
    for (int i = 0; i < 8; i++) pin_out(s, p, c3_p1[i], ((m->ddr1 >> i) & 1) ? DRV_BIT((m->p1 >> i) & 1) : DRV_Z);
    for (int i = 0; i < 5; i++) pin_out(s, p, c3_p2[i], ((m->ddr2 >> i) & 1) ? DRV_BIT((m->p2 >> i) & 1) : DRV_Z);
}

static void cpu6803_commit(sim_t *s, part_t *p) {
    cpu6803_t *c = p->state;
    int part = (int)(p - s->parts);
    if (c->h >= 4) c->latch = (uint8_t)part_read_bus(s, p, c3_ad, 8) ;
    int x = pin_in(s, p, C3_EXTAL);
    if (x != c->extal) {
        c->extal = (uint8_t)x;
        c->h = (uint8_t)((c->h + 1) & 7);
        if (c->h == 0) {                                    /* E fell: the cycle ends */
            uint8_t p1 = 0, p2 = 0;
            for (int i = 0; i < 8; i++) p1 = (uint8_t)(p1 | pin_in(s, p, c3_p1[i]) << i);
            for (int i = 0; i < 5; i++) p2 = (uint8_t)(p2 | pin_in(s, p, c3_p2[i]) << i);
            m6803_cycle(&c->cpu, c->latch, p1, p2, pin_in(s, p, C3_NMI), pin_in(s, p, C3_IRQ), pin_in(s, p, C3_RESET));
        }
        sim_mark_dirty(s, part);
    }
}

static void cpu6803_reset(sim_t *s, part_t *p) {
    (void)s;
    cpu6803_t *c = p->state;
    memset(c, 0, sizeof(*c));
    m6803_init(&c->cpu);
}

const part_type_t PT_6803 = { "MC6803",
    "VSS:P XTAL:A EXTAL:M /NMI:T /IRQ:T /RESET:T VCC:P P20:X P21:X P22:X P23:X P24:X P10:X P11:X P12:X P13:X P14:X P15:X P16:X P17:X "
    "STBY:m A15:O A14:O A13:O A12:O A11:O A10:O A9:O A8:O D7:X D6:X D5:X D4:X D3:X D2:X D1:X D0:X R/W:O AS:O E:O",
    cpu6803_eval, cpu6803_commit, cpu6803_reset, sizeof(cpu6803_t), TECH_NMOS };

const m6803_t *mc10_cpu_core(sim_t *s, int part) {
    cpu6803_t *c = s->parts[part].state;
    return &c->cpu;
}

/* ------------------------------------------------------------------------ */
/* The clock: the MC1372's oscillator (IC2 pin 1) with the 3.579545 MHz crystal. One step is half a period. */
/* ------------------------------------------------------------------------ */

static void xtal_mc10_eval(sim_t *s, part_t *p) { pin_out(s, p, 1, DRV_BIT(s->clock_level)); }
const part_type_t PT_XTAL_MC10 = { "MC1372",
    "CLK:O OSC:A N3:A GND:P CB:A CREF:A CA:A CHROMA:A LUMA:A CIN:A VCC:P RF:A TANK1:A TANK2:A", xtal_mc10_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* 74LS155: two 2 to 4 decoders sharing the select A, B. Section 1 is enabled by 1C high and /1G low, section 2 by /2C low and /2G low. */
/* ------------------------------------------------------------------------ */

static void ls155_eval(sim_t *s, part_t *p) {
    static const int y1[4] = { 7, 6, 5, 4 }, y2[4] = { 9, 10, 11, 12 };
    unsigned sel = (unsigned)(pin_in(s, p, 3) << 1 | pin_in(s, p, 13));
    bool en1 = pin_in(s, p, 1) && !pin_in(s, p, 2);
    bool en2 = !pin_in(s, p, 15) && !pin_in(s, p, 14);
    for (unsigned n = 0; n < 4; n++) {
        pin_out(s, p, y1[n], DRV_BIT(!(en1 && sel == n)));
        pin_out(s, p, y2[n], DRV_BIT(!(en2 && sel == n)));
    }
}
const part_type_t PT_74LS155 = { "74LS155",
    "1C:T /1G:T B:T 1Y3:O 1Y2:O 1Y1:O 1Y0:O GND:P 2Y0:O 2Y1:O 2Y2:O 2Y3:O A:T /2G:T /2C:T VCC:P", ls155_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* 74LS367: hex bus driver, 4 gates enabled by /1G and 2 by /2G                */
/* ------------------------------------------------------------------------ */

static void ls367_eval(sim_t *s, part_t *p) {
    static const int a1[4] = { 2, 4, 6, 10 }, y1[4] = { 3, 5, 7, 9 }, a2[2] = { 12, 14 }, y2[2] = { 11, 13 };
    bool g1 = !pin_in(s, p, 1), g2 = !pin_in(s, p, 15);
    for (int i = 0; i < 4; i++) pin_out(s, p, y1[i], g1 ? DRV_BIT(pin_in(s, p, a1[i])) : DRV_Z);
    for (int i = 0; i < 2; i++) pin_out(s, p, y2[i], g2 ? DRV_BIT(pin_in(s, p, a2[i])) : DRV_Z);
}
const part_type_t PT_74LS367 = { "74LS367",
    "/1G:T 1A1:T 1Y1:O 1A2:T 1Y2:O 1A3:T 1Y3:O GND:P 1Y4:O 1A4:T 2Y1:O 2A1:T 2Y2:O 2A2:T /2G:T VCC:P", ls367_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* 74LS76: two JK flip-flops, falling edge, with /PRE and /CLR                  */
/* ------------------------------------------------------------------------ */

typedef struct { uint8_t q[2], clk[2]; } ls76_t;
static const int ls76_pin[2][7] = { { 1, 2, 3, 4, 16, 15, 14 }, { 6, 7, 8, 9, 12, 11, 10 } };   /* CLK /PRE /CLR J K Q /Q */

static void ls76_eval(sim_t *s, part_t *p) {
    const ls76_t *t = p->state;
    for (int f = 0; f < 2; f++) {
        bool pre = !pin_in(s, p, ls76_pin[f][1]), clr = !pin_in(s, p, ls76_pin[f][2]);
        bool q = pre ? true : clr ? false : t->q[f];
        bool nq = (pre && clr) ? true : !q;
        pin_out(s, p, ls76_pin[f][5], DRV_BIT(q));
        pin_out(s, p, ls76_pin[f][6], DRV_BIT(nq));
    }
}
static void ls76_commit(sim_t *s, part_t *p) {
    ls76_t *t = p->state;
    bool changed = false;
    for (int f = 0; f < 2; f++) {
        bool pre = !pin_in(s, p, ls76_pin[f][1]), clr = !pin_in(s, p, ls76_pin[f][2]);
        int clk = pin_in(s, p, ls76_pin[f][0]);
        uint8_t q = t->q[f];
        if (!clk && t->clk[f] && !pre && !clr) {
            int j = pin_in(s, p, ls76_pin[f][3]), k = pin_in(s, p, ls76_pin[f][4]);
            q = (uint8_t)(j ? (k ? !q : 1) : (k ? 0 : q));
        }
        if (pre) q = 1;
        else if (clr) q = 0;
        t->clk[f] = (uint8_t)clk;
        if (q != t->q[f]) { t->q[f] = q; changed = true; }
    }
    if (changed) sim_mark_dirty(s, (int)(p - s->parts));
}
static void ls76_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls76_t)); }
const part_type_t PT_74LS76 = { "74LS76",
    "1CLK:T /1PRE:T /1CLR:T 1J:T VCC:P 2CLK:T /2PRE:T /2CLR:T 2J:T 2/Q:O 2Q:O 2K:T GND:P 1/Q:O 1Q:O 1K:T",
    ls76_eval, ls76_commit, ls76_reset, sizeof(ls76_t), TECH_TTL };

/* ------------------------------------------------------------------------ */
/* MC14503B: hex non-inverting three-state buffer (the keyboard columns onto the data bus) */
/* ------------------------------------------------------------------------ */

static void mc14503_eval(sim_t *s, part_t *p) {
    static const int in[6] = { 2, 4, 6, 10, 12, 14 }, out[6] = { 3, 5, 7, 9, 11, 13 };
    bool en = !pin_in(s, p, 1) && !pin_in(s, p, 15);
    for (int i = 0; i < 6; i++) pin_out(s, p, out[i], en ? DRV_BIT(pin_in(s, p, in[i])) : DRV_Z);
}
const part_type_t PT_MC14503 = { "MC14503B",
    "/DISA:T IN1:T OUT1:O IN2:T OUT2:O IN3:T OUT3:O VSS:P OUT4:O IN4:T OUT5:O IN5:T OUT6:O IN6:T /DISB:T VDD:P",
    mc14503_eval, 0, 0, 0, TECH_CMOS };

/* ------------------------------------------------------------------------ */
/* TMS2564: 8K x 8 ROM in a 28 pin case (the MC-10's BASIC), selected by pin 22 */
/* ------------------------------------------------------------------------ */

static const int t2564_a[13] = { 10, 9, 8, 7, 6, 5, 4, 3, 25, 24, 21, 20, 23 };
static const int t2564_d[8] = { 11, 12, 13, 15, 16, 17, 18, 19 };

static void t2564_eval(sim_t *s, part_t *p) {
    rom_state_t *st = p->state;
    bool selected = !pin_in(s, p, 22);
    unsigned addr = selected ? part_read_bus(s, p, t2564_a, 13) : 0;
    part_drive_bus_cached(s, p, t2564_d, 8, st->data ? st->data[addr] : 0xFFu, selected, &st->out_cache);
}
const part_type_t PT_TMS2564 = { "TMS2564 ROM",
    "VPP:P /CS1:m A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O D7:O A11:M A10:M /E:M "
    "A12:M A9:M A8:M VCC:P CS2:m VCC2:P",
    t2564_eval, 0, 0, sizeof(rom_state_t), TECH_NMOS };
