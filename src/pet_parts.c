/* pet_parts.c - pin-level models of the chips of the Commodore PET
 * 8032080 board that the C64 and Spectrum boards don't use: more TTL glue,
 * the flip-flops and counters of the 16 MHz timing chain, the character
 * ROM, the IEEE-488 transceivers and the power-up reset timer. The big
 * chips (6502, 6520, 6522, 6545) are wrapped in pet.c around the cores in
 * chips/.
 *
 * Sequential parts follow the same rules as the others (see sim.h): eval()
 * only drives the outputs from the part's state and its asynchronous
 * inputs, commit() samples the clock after the board settled. A step of the
 * board is half a period of the 16 MHz crystal, so a ripple counter's second
 * stage changes one step (31 ns) after the first, about what a real
 * 74LS177 does. */
#include "parts.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Combinational TTL                                                         */
/* ------------------------------------------------------------------------ */

/* 74LS02: quad 2-input NOR. Pins: 1Y 1A 1B 2Y 2A 2B GND 3A 3B 3Y 4A 4B 4Y */
static void ls02_eval(sim_t *s, part_t *p) {
    static const int g[4][3] = { {2, 3, 1}, {5, 6, 4}, {8, 9, 10}, {11, 12, 13} };
    for (int i = 0; i < 4; i++)
        pin_out(s, p, g[i][2], DRV_BIT(!(pin_in(s, p, g[i][0]) || pin_in(s, p, g[i][1]))));
}
const part_type_t PT_74LS02 = { "74LS02",
    "1Y:O 1A:T 1B:T 2Y:O 2A:T 2B:T GND:P 3A:T 3B:T 3Y:O 4A:T 4B:T 4Y:O VCC:P",
    ls02_eval, 0, 0, 0, TECH_TTL };

/* 74LS10: triple 3-input NAND. Pins: 1A 1B 2A 2B 2C 2Y GND 3Y 3A 3B 3C 1Y 1C */
static void ls10_eval(sim_t *s, part_t *p) {
    static const int g[3][4] = { {1, 2, 13, 12}, {3, 4, 5, 6}, {9, 10, 11, 8} };
    for (int i = 0; i < 3; i++)
        pin_out(s, p, g[i][3], DRV_BIT(!(pin_in(s, p, g[i][0]) && pin_in(s, p, g[i][1]) && pin_in(s, p, g[i][2]))));
}
const part_type_t PT_74LS10 = { "74LS10",
    "1A:T 1B:T 2A:T 2B:T 2C:T 2Y:O GND:P 3Y:O 3A:T 3B:T 3C:T 1Y:O 1C:T VCC:P",
    ls10_eval, 0, 0, 0, TECH_TTL };

/* 74LS20: dual 4-input NAND. Pins: 1A 1B NC 1C 1D 1Y GND 2Y 2A 2B NC 2C 2D */
static void ls20_eval(sim_t *s, part_t *p) {
    static const int g[2][5] = { {1, 2, 4, 5, 6}, {9, 10, 12, 13, 8} };
    for (int i = 0; i < 2; i++)
        pin_out(s, p, g[i][4], DRV_BIT(!(pin_in(s, p, g[i][0]) && pin_in(s, p, g[i][1]) &&
                                          pin_in(s, p, g[i][2]) && pin_in(s, p, g[i][3]))));
}
const part_type_t PT_74LS20 = { "74LS20",
    "1A:T 1B:T NC:O 1C:T 1D:T 1Y:O GND:P 2Y:O 2A:T 2B:T NC:O 2C:T 2D:T VCC:P",
    ls20_eval, 0, 0, 0, TECH_TTL };

/* 7425: dual 4-input NOR with strobe: the output is high when the strobe is
 * low. Pins: 1A 1B 1STB 1C 1D 1Y GND 2Y 2A 2B 2STB 2C 2D */
static void ttl7425_eval(sim_t *s, part_t *p) {
    static const int g[2][6] = { {1, 2, 4, 5, 3, 6}, {9, 10, 12, 13, 11, 8} };
    for (int i = 0; i < 2; i++) {
        bool any = pin_in(s, p, g[i][0]) || pin_in(s, p, g[i][1]) || pin_in(s, p, g[i][2]) || pin_in(s, p, g[i][3]);
        bool stb = pin_in(s, p, g[i][4]);
        pin_out(s, p, g[i][5], DRV_BIT(!(stb && any)));
    }
}
const part_type_t PT_7425 = { "7425",
    "1A:T 1B:T 1STB:T 1C:T 1D:T 1Y:O GND:P 2Y:O 2A:T 2B:T 2STB:T 2C:T 2D:T VCC:P",
    ttl7425_eval, 0, 0, 0, TECH_TTL };

/* 74LS86: quad XOR. Pins as the 74LS00. */
static void ls86_eval(sim_t *s, part_t *p) {
    static const int g[4][3] = { {1, 2, 3}, {4, 5, 6}, {9, 10, 8}, {12, 13, 11} };
    for (int i = 0; i < 4; i++)
        pin_out(s, p, g[i][2], DRV_BIT(pin_in(s, p, g[i][0]) != pin_in(s, p, g[i][1])));
}
const part_type_t PT_74LS86 = { "74LS86",
    "1A:T 1B:T 1Y:O 2A:T 2B:T 2Y:O GND:P 3Y:O 3A:T 3B:T 4Y:O 4A:T 4B:T VCC:P",
    ls86_eval, 0, 0, 0, TECH_TTL };

/* 74S04: hex inverter (same logic as the 74LS04). */
static void s04_eval(sim_t *s, part_t *p) {
    static const int g[6][2] = { {1, 2}, {3, 4}, {5, 6}, {9, 8}, {11, 10}, {13, 12} };
    for (int i = 0; i < 6; i++) pin_out(s, p, g[i][1], DRV_BIT(!pin_in(s, p, g[i][0])));
}
const part_type_t PT_74S04 = { "74S04",
    "1A:T 1Y:O 2A:T 2Y:O 3A:T 3Y:O GND:P 4Y:O 4A:T 5Y:O 5A:T 6Y:O 6A:T VCC:P",
    s04_eval, 0, 0, 0, TECH_TTL };

/* The oscillator's own inverters (UE4 gates 1 and 2, which make the crystal
 * oscillate) are not simulated: their outputs are left alone and the crystal
 * part (PT_XTAL16) puts the 16 MHz on the net. */
static void s04_osc_eval(sim_t *s, part_t *p) {
    static const int g[4][2] = { {5, 6}, {9, 8}, {11, 10}, {13, 12} };
    for (int i = 0; i < 4; i++) pin_out(s, p, g[i][1], DRV_BIT(!pin_in(s, p, g[i][0])));
}
const part_type_t PT_74S04_OSC = { "74S04",
    "1A:A 1Y:A 2A:A 2Y:A 3A:T 3Y:O GND:P 4Y:O 4A:T 5Y:O 5A:T 6Y:O 6A:T VCC:P",
    s04_osc_eval, 0, 0, 0, TECH_TTL };

/* 7417: hex buffer with open collector outputs (low when the input is low). */
static void ttl7417_eval(sim_t *s, part_t *p) {
    static const int g[6][2] = { {1, 2}, {3, 4}, {5, 6}, {9, 8}, {11, 10}, {13, 12} };
    for (int i = 0; i < 6; i++)
        pin_out(s, p, g[i][1], pin_in(s, p, g[i][0]) ? DRV_Z : DRV_LO);
}
const part_type_t PT_7417 = { "7417",
    "1A:T 1Y:O 2A:T 2Y:O 3A:T 3Y:O GND:P 4Y:O 4A:T 5Y:O 5A:T 6Y:O 6A:T VCC:P",
    ttl7417_eval, 0, 0, 0, TECH_TTL_OC };

/* 74LS138: 3-to-8 decoder, active low outputs.
 * Pins: A B C /G2A /G2B G1 Y7 GND Y6 Y5 Y4 Y3 Y2 Y1 Y0 */
static void ls138_eval(sim_t *s, part_t *p) {
    static const int y[8] = { 15, 14, 13, 12, 11, 10, 9, 7 };
    bool en = pin_in(s, p, 6) && !pin_in(s, p, 4) && !pin_in(s, p, 5);
    int sel = pin_in(s, p, 1) | (pin_in(s, p, 2) << 1) | (pin_in(s, p, 3) << 2);
    for (int i = 0; i < 8; i++) pin_out(s, p, y[i], DRV_BIT(!(en && sel == i)));
}
const part_type_t PT_74LS138 = { "74LS138",
    "A:T B:T C:T /G2A:T /G2B:T G1:T Y7:O GND:P Y6:O Y5:O Y4:O Y3:O Y2:O Y1:O Y0:O VCC:P",
    ls138_eval, 0, 0, 0, TECH_TTL };

/* 74LS145: BCD to decimal decoder with open collector outputs (the PET's
 * keyboard row driver). Only the selected output pulls low; for a code above
 * 9 all outputs are off.
 * Pins: Y0 Y1 Y2 Y3 Y4 Y5 Y6 GND Y7 Y8 Y9 D C B A */
static void ls145_eval(sim_t *s, part_t *p) {
    static const int y[10] = { 1, 2, 3, 4, 5, 6, 7, 9, 10, 11 };
    int code = pin_in(s, p, 15) | (pin_in(s, p, 14) << 1) | (pin_in(s, p, 13) << 2) | (pin_in(s, p, 12) << 3);
    for (int i = 0; i < 10; i++) pin_out(s, p, y[i], code == i ? DRV_LO : DRV_Z);
}
const part_type_t PT_74LS145 = { "74LS145",
    "Y0:O Y1:O Y2:O Y3:O Y4:O Y5:O Y6:O GND:P Y7:O Y8:O Y9:O D:T C:T B:T A:T VCC:P",
    ls145_eval, 0, 0, 0, TECH_TTL_OC };

/* 74154: 4-to-16 decoder, active low outputs, two active low enables.
 * Pins: Y0..Y7 (1-8) Y8 Y9 Y10 (9-11) GND Y11..Y15 (13-17) /G1 /G2 D C B A */
static void ttl74154_eval(sim_t *s, part_t *p) {
    static const int y[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17 };
    bool en = !pin_in(s, p, 18) && !pin_in(s, p, 19);
    int sel = pin_in(s, p, 23) | (pin_in(s, p, 22) << 1) | (pin_in(s, p, 21) << 2) | (pin_in(s, p, 20) << 3);
    for (int i = 0; i < 16; i++) pin_out(s, p, y[i], DRV_BIT(!(en && sel == i)));
}
const part_type_t PT_74154 = { "74154",
    "Y0:O Y1:O Y2:O Y3:O Y4:O Y5:O Y6:O Y7:O Y8:O Y9:O Y10:O GND:P Y11:O Y12:O Y13:O Y14:O Y15:O "
    "/G1:T /G2:T D:T C:T B:T A:T VCC:P",
    ttl74154_eval, 0, 0, 0, TECH_TTL };

/* 74LS244: two groups of four non-inverting tri-state buffers, outputs
 * enabled by a low /G. Pins: 1G 1A1 2Y4 1A2 2Y3 1A3 2Y2 1A4 2Y1 GND 2A1 1Y4 2A2 1Y3 2A3 1Y2 2A4 1Y1 2G */
static void ls244_eval(sim_t *s, part_t *p) {
    /* { enable, input, output } */
    static const int b[8][3] = {
        {1, 2, 18}, {1, 4, 16}, {1, 6, 14}, {1, 8, 12},
        {19, 11, 9}, {19, 13, 7}, {19, 15, 5}, {19, 17, 3} };
    for (int i = 0; i < 8; i++) {
        bool en = !pin_in(s, p, b[i][0]);
        pin_out(s, p, b[i][2], en ? DRV_BIT(pin_in(s, p, b[i][1])) : DRV_Z);
    }
}
const part_type_t PT_74LS244 = { "74LS244",
    "/1G:T 1A1:T 2Y4:O 1A2:T 2Y3:O 1A3:T 2Y2:O 1A4:T 2Y1:O GND:P 2A1:T 1Y4:O 2A2:T 1Y3:O 2A3:T 1Y2:O "
    "2A4:T 1Y1:O /2G:T VCC:P", ls244_eval, 0, 0, 0, TECH_TTL };

/* MC3446: four IEEE-488 bus transceivers with termination on the bus pins.
 * The driver inverts (a high input pulls the bus low) and is enabled by a
 * low enable; the receiver inverts too (a low bus reads high). Per channel:
 * receiver output, driver input, bus pin; pins 4 and 12 are the enables.
 *   A: 1 3 2   B: 15 13 14   C: 7 5 6   D: 9 11 10 */
static void mc3446_eval(sim_t *s, part_t *p) {
    static const int ch[4][4] = { {1, 3, 2, 4}, {15, 13, 14, 12}, {7, 5, 6, 4}, {9, 11, 10, 12} };
    for (int i = 0; i < 4; i++) {
        bool drv = !pin_in(s, p, ch[i][3]) && pin_in(s, p, ch[i][1]);
        pin_out(s, p, ch[i][2], drv ? DRV_LO : DRV_PULLUP);
        pin_out(s, p, ch[i][0], DRV_BIT(!pin_in(s, p, ch[i][2])));
    }
}
const part_type_t PT_MC3446 = { "MC3446",
    "R1:O D1:T B1:X EN:T D3:T B3:X R3:O GND:P R4:O D4:T B4:X EN2:T D2:T B2:X R2:O VCC:P",
    mc3446_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* Flip-flops, counters, shift registers (16 MHz timing chain)               */
/* ------------------------------------------------------------------------ */

/* 74LS74 / 74S74: dual D flip-flop with asynchronous preset and clear.
 * Both low: both outputs high. Pins: /1CLR 1D 1CK /1PR 1Q /1Q GND /2Q 2Q /2PR 2CK 2D /2CLR */
typedef struct { uint8_t q[2], ck[2]; } ff74_state_t;
static const int ff74_pin[2][7] = { {1, 2, 3, 4, 5, 6, 0}, {13, 12, 11, 10, 9, 8, 0} };  /* clr d ck pr q qn */

static void ff74_apply_async(sim_t *s, part_t *p, ff74_state_t *st) {
    for (int h = 0; h < 2; h++) {
        const int *pn = ff74_pin[h];
        bool clr = !pin_in(s, p, pn[0]), pr = !pin_in(s, p, pn[3]);
        if (clr && !pr) st->q[h] = 0;
        else if (pr && !clr) st->q[h] = 1;
    }
}
static void ff74_eval(sim_t *s, part_t *p) {
    ff74_state_t *st = p->state;
    ff74_apply_async(s, p, st);
    for (int h = 0; h < 2; h++) {
        const int *pn = ff74_pin[h];
        bool clr = !pin_in(s, p, pn[0]), pr = !pin_in(s, p, pn[3]);
        bool q = st->q[h], qn = !st->q[h];
        if (clr && pr) { q = true; qn = true; }
        pin_out(s, p, pn[4], DRV_BIT(q));
        pin_out(s, p, pn[5], DRV_BIT(qn));
    }
}
static void ff74_commit(sim_t *s, part_t *p) {
    ff74_state_t *st = p->state;
    bool changed = false;
    for (int h = 0; h < 2; h++) {
        const int *pn = ff74_pin[h];
        int ck = pin_in(s, p, pn[2]);
        bool clr = !pin_in(s, p, pn[0]), pr = !pin_in(s, p, pn[3]);
        if (ck && !st->ck[h] && !clr && !pr) {
            uint8_t d = (uint8_t)pin_in(s, p, pn[1]);
            if (d != st->q[h]) { st->q[h] = d; changed = true; }
        }
        st->ck[h] = (uint8_t)ck;
    }
    if (changed) sim_mark_dirty(s, (int)(p - s->parts));
}
static void ff74_reset(sim_t *s, part_t *p) {
    (void)s;
    ff74_state_t *st = p->state;
    st->q[0] = st->q[1] = 0;
    st->ck[0] = st->ck[1] = 1;
}
#define FF74_PINS "/1CLR:T 1D:T 1CK:T /1PR:T 1Q:O /1Q:O GND:P /2Q:O 2Q:O /2PR:T 2CK:T 2D:T /2CLR:T VCC:P"
const part_type_t PT_74LS74R = { "74LS74", FF74_PINS, ff74_eval, ff74_commit, ff74_reset, sizeof(ff74_state_t), TECH_TTL };
const part_type_t PT_74S74 = { "74S74", FF74_PINS, ff74_eval, ff74_commit, ff74_reset, sizeof(ff74_state_t), TECH_TTL };

/* 74177: ripple counter, a divide by 2 (CK1 -> QA) and a divide by 8
 * (CK2 -> QB QC QD). Both clock inputs act on the falling edge. An
 * asynchronous /LOAD takes A-D into the outputs; /CLR clears.
 * Pins: /LOAD QC C A QA CK2 GND CK1 QB B D QD /CLR VCC */
typedef struct { uint8_t qa, cnt; uint8_t ck1, ck2; } cnt177_state_t;

static void cnt177_async(sim_t *s, part_t *p, cnt177_state_t *st) {
    if (!pin_in(s, p, 13)) { st->qa = 0; st->cnt = 0; }
    else if (!pin_in(s, p, 1)) {
        st->qa = (uint8_t)pin_in(s, p, 4);
        st->cnt = (uint8_t)(pin_in(s, p, 10) | (pin_in(s, p, 3) << 1) | (pin_in(s, p, 11) << 2));
    }
}
static void cnt177_eval(sim_t *s, part_t *p) {
    cnt177_state_t *st = p->state;
    cnt177_async(s, p, st);
    pin_out(s, p, 5, DRV_BIT(st->qa));
    pin_out(s, p, 9, DRV_BIT(st->cnt & 1));
    pin_out(s, p, 2, DRV_BIT((st->cnt >> 1) & 1));
    pin_out(s, p, 12, DRV_BIT((st->cnt >> 2) & 1));
}
static void cnt177_commit(sim_t *s, part_t *p) {
    cnt177_state_t *st = p->state;
    bool changed = false;
    int ck1 = pin_in(s, p, 8), ck2 = pin_in(s, p, 6);
    bool held = !pin_in(s, p, 13) || !pin_in(s, p, 1);
    if (!held) {
        if (st->ck1 && !ck1) { st->qa ^= 1; changed = true; }
        if (st->ck2 && !ck2) { st->cnt = (uint8_t)((st->cnt + 1) & 7); changed = true; }
    }
    st->ck1 = (uint8_t)ck1; st->ck2 = (uint8_t)ck2;
    if (changed) sim_mark_dirty(s, (int)(p - s->parts));
}
static void cnt177_reset(sim_t *s, part_t *p) {
    (void)s;
    cnt177_state_t *st = p->state;
    st->qa = 0; st->cnt = 0; st->ck1 = st->ck2 = 1;
}
const part_type_t PT_74177 = { "74177",
    "/LOAD:T QC:O C:T A:T QA:O CK2:T GND:P CK1:T QB:O B:T D:T QD:O /CLR:T VCC:P",
    cnt177_eval, cnt177_commit, cnt177_reset, sizeof(cnt177_state_t), TECH_TTL };

/* 74LS164: 8 bit serial in, parallel out shift register, rising edge,
 * asynchronous clear. Pins: A B QA QB QC QD GND CK /CLR QE QF QG QH VCC */
typedef struct { uint8_t q; uint8_t ck; } sr164_state_t;
static void sr164_eval(sim_t *s, part_t *p) {
    static const int q[8] = { 3, 4, 5, 6, 10, 11, 12, 13 };
    sr164_state_t *st = p->state;
    if (!pin_in(s, p, 9)) st->q = 0;
    for (int i = 0; i < 8; i++) pin_out(s, p, q[i], DRV_BIT((st->q >> i) & 1));
}
static void sr164_commit(sim_t *s, part_t *p) {
    sr164_state_t *st = p->state;
    int ck = pin_in(s, p, 8);
    if (ck && !st->ck && pin_in(s, p, 9)) {
        uint8_t in = (uint8_t)(pin_in(s, p, 1) && pin_in(s, p, 2));
        uint8_t n = (uint8_t)(((st->q << 1) | in) & 0xFF);
        if (n != st->q) { st->q = n; sim_mark_dirty(s, (int)(p - s->parts)); }
    }
    st->ck = (uint8_t)ck;
}
static void sr164_reset(sim_t *s, part_t *p) { (void)s; sr164_state_t *st = p->state; st->q = 0; st->ck = 1; }
const part_type_t PT_74LS164 = { "74LS164",
    "A:T B:T QA:O QB:O QC:O QD:O GND:P CK:T /CLR:T QE:O QF:O QG:O QH:O VCC:P",
    sr164_eval, sr164_commit, sr164_reset, sizeof(sr164_state_t), TECH_TTL };

/* 74166: 8 bit parallel load / serial out shift register. On the rising
 * edge of CK (when CK INH is low): SH/LD low loads A-H, high shifts
 * (SER into A, QH is the last stage). Asynchronous /CLR.
 * Pins: SER A B C D CKINH CK GND /CLR E F G QH H SH/LD */
typedef struct { uint8_t q; uint8_t ck; } sr166_state_t;
static void sr166_eval(sim_t *s, part_t *p) {
    sr166_state_t *st = p->state;
    if (!pin_in(s, p, 9)) st->q = 0;
    pin_out(s, p, 13, DRV_BIT((st->q >> 7) & 1));
}
static void sr166_commit(sim_t *s, part_t *p) {
    static const int par[8] = { 2, 3, 4, 5, 10, 11, 12, 14 };
    sr166_state_t *st = p->state;
    int ck = pin_in(s, p, 7);
    if (ck && !st->ck && !pin_in(s, p, 6) && pin_in(s, p, 9)) {
        uint8_t n;
        if (!pin_in(s, p, 15)) {
            n = 0;
            for (int i = 0; i < 8; i++) n |= (uint8_t)(pin_in(s, p, par[i]) << i);
        } else {
            n = (uint8_t)(((st->q << 1) | pin_in(s, p, 1)) & 0xFF);
        }
        if (n != st->q) { st->q = n; sim_mark_dirty(s, (int)(p - s->parts)); }
    }
    st->ck = (uint8_t)ck;
}
static void sr166_reset(sim_t *s, part_t *p) { (void)s; sr166_state_t *st = p->state; st->q = 0; st->ck = 1; }
const part_type_t PT_74166 = { "74166",
    "SER:T A:T B:T C:T D:T CKINH:T CK:T GND:P /CLR:T E:T F:T G:T QH:O H:T SH/LD:T VCC:P",
    sr166_eval, sr166_commit, sr166_reset, sizeof(sr166_state_t), TECH_TTL };

/* ------------------------------------------------------------------------ */
/* Character ROM, crystal, power-up reset                                    */
/* ------------------------------------------------------------------------ */

/* 2316: 2K x 8 ROM with three programmable chip selects. The polarity of the
 * selects is mask programmed and can't be read from the drawing. The board
 * grounds pin 20, drives pin 18 from the CRTC's MA13 (low in normal use) and
 * pin 21 from INIT (high while running), so the only reading in which the
 * character generator (901447-xx) is on in normal use is: pin 20 low, pin 18
 * low, pin 21 high.
 * Pins: A7 A6 A5 A4 A3 A2 A1 A0 D0 D1 D2 GND D3 D4 D5 D6 D7 CS2 A10 CS1 CS3 A9 A8 VCC */
static const int rom2316_addr[11] = { 8, 7, 6, 5, 4, 3, 2, 1, 23, 22, 19 };
static const int rom2316_data[8] = { 9, 10, 11, 13, 14, 15, 16, 17 };
/* The data lags the address by the access time (450 ns, 14 half periods of the
 * 16 MHz clock, for the 2316 in the PET): the video latch is transparent for
 * 4 steps and the dot shift register loads at its end, and relies on the ROM
 * still showing the character of the period before. */
#ifndef ROM2316_ACCESS_STEPS
#define ROM2316_ACCESS_STEPS 14
#endif
typedef struct { rom_state_t base; uint16_t addr_seen; uint8_t wait, shown, sel_seen; } rom2316_state_t;

static bool rom2316_selected(sim_t *s, part_t *p) { return !pin_in(s, p, 20) && !pin_in(s, p, 18) && pin_in(s, p, 21); }

static void rom2316_eval(sim_t *s, part_t *p) {
    rom2316_state_t *st = p->state;
    part_drive_bus_cached(s, p, rom2316_data, 8, st->shown, rom2316_selected(s, p), &st->base.out_cache);
}

static void rom2316_commit(sim_t *s, part_t *p) {
    rom2316_state_t *st = p->state;
    if (!rom2316_selected(s, p)) { st->sel_seen = 0; return; }
    unsigned addr = part_read_bus(s, p, rom2316_addr, 11);
    unsigned v = st->base.data ? st->base.data[addr] : 0xFF;
    if (!st->sel_seen || addr != st->addr_seen) {
        st->sel_seen = 1;
        st->addr_seen = (uint16_t)addr;
        st->wait = ROM2316_ACCESS_STEPS;
    } else if (st->wait) {
        if (--st->wait == 0 && v != st->shown) { st->shown = (uint8_t)v; sim_mark_dirty(s, (int)(p - s->parts)); }
    } else if (v != st->shown) {
        st->shown = (uint8_t)v;
        sim_mark_dirty(s, (int)(p - s->parts));
    }
}

static void rom2316_reset(sim_t *s, part_t *p) {
    (void)s;
    rom2316_state_t *st = p->state;
    st->wait = st->sel_seen = 0;
    st->shown = 0xFF;
}

const part_type_t PT_2316 = { "2316 ROM",
    "A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O D7:O "
    "CS2:M A10:M CS1:M CS3:M A9:M A8:M VCC:P", rom2316_eval, rom2316_commit, rom2316_reset, sizeof(rom2316_state_t), TECH_NMOS };

/* The 16 MHz crystal: drives pin 2 (the oscillator's last node, OSC_C) with
 * the board's clock level, one level per step. The oscillator's own
 * inverters and the crystal's tuning network aren't simulated. */
static void xtal16_eval(sim_t *s, part_t *p) {
    pin_out(s, p, 2, DRV_BIT(s->clock_level));
}
const part_type_t PT_XTAL16 = { "16MHZ XTAL", "P1:O P2:O", xtal16_eval, 0, 0, 0, TECH_TTL };

/* LM555 as the PET's power-up reset: OUT is high (the reset asserted, after
 * the inverters) for about reset_us after power on, then low. */
typedef struct { uint64_t until; } pwr555_state_t;
static void pwr555_eval(sim_t *s, part_t *p) {
    pwr555_state_t *st = p->state;
    pin_out(s, p, 3, DRV_BIT(s->cycle < st->until));
}
static void pwr555_commit(sim_t *s, part_t *p) {
    pwr555_state_t *st = p->state;
    /* the output changes at one moment: wake the part when the time is up */
    if (s->cycle == st->until || s->cycle == st->until + 1) sim_mark_dirty(s, (int)(p - s->parts));
}
#define PET_RESET_US 3000       /* power-up reset length, shorter than the real one */
static void pwr555_reset(sim_t *s, part_t *p) {
    pwr555_state_t *st = p->state;
    st->until = s->cycle + (uint64_t)(PET_RESET_US * s->time_scale);
}
const part_type_t PT_LM555 = { "LM555 RESET",
    "GND:P TRIG:m OUT:O RESET:m CTRL:O THR:m DIS:O VCC:P",
    pwr555_eval, pwr555_commit, pwr555_reset, sizeof(pwr555_state_t), TECH_TTL };
