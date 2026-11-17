/* pet_chips.c - the big chips of the Commodore PET 8032080 board as parts:
 * the 6502 CPU (UB14), the two 6520 PIAs (UB12 keyboard/cassette, UB16
 * IEEE-488), the 6522 VIA (UB15) and the 6545 CRT controller (UB13), each
 * wrapped around the core in chips/. Unlike the C64 (c64.c), nothing here
 * drives the bus cycle: every chip watches its own clock pin on the board
 * and does its work in commit(), when that clock changes level.
 *
 * Bus cycle of the PIAs, the VIA and the CRTC (E or phi2 is the clock):
 *   - while the clock is high the selects, R/W, the register number and
 *     the data bus are sampled at every step (the last sample is what
 *     the chip sees: the CPU lets go of the data bus as phi2 falls);
 *   - a read is answered while the clock is high, from a copy of the core
 *     (reading has side effects, so the real access is done later);
 *   - when the clock falls the core runs one cycle with the access: a
 *     write, the side effects of a read, or no access.
 * The CPU core runs one cycle when phi0 falls: it takes the data sampled
 * at the end of the previous phi2 and puts out the next address. */
#include "parts.h"
#include "chips_common.h"
#include "m6502.h"
#include "m6520.h"
#include "m6522.h"
#include "m6545.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int bus_d[8] = { 33, 32, 31, 30, 29, 28, 27, 26 };     /* D0..D7 pins (all five chips) */

/* ------------------------------------------------------------------------ */
/* 6502 (UB14)                                                               */
/* ------------------------------------------------------------------------ */

enum { C_RDY = 2, C_PHI1 = 3, C_IRQ = 4, C_NMI = 6, C_SYNC = 7, C_RW = 34, C_PHI0 = 37, C_SO = 38, C_PHI2 = 39,
       C_RES = 40 };
static const int cpu_a[16] = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 24, 25 };

typedef struct {
    m6502_t core;
    uint64_t pins;              /* the core's pins after its last cycle */
    uint8_t phi0, in_reset;
    uint8_t latched;            /* data bus at the end of phi2 */
    uint32_t cycles;            /* cycles run since power on (for tests) */
    int32_t cache_a, cache_d;
} pet_cpu_t;

static void cpu_eval(sim_t *s, part_t *p) {
    pet_cpu_t *c = p->state;
    bool phi0 = pin_in(s, p, C_PHI0) != 0;
    pin_out(s, p, C_PHI2, DRV_BIT(phi0));
    pin_out(s, p, C_PHI1, DRV_BIT(!phi0));
    part_drive_bus_cached(s, p, cpu_a, 16, M6502_GET_ADDR(c->pins), true, &c->cache_a);
    pin_out(s, p, C_RW, DRV_BIT((c->pins & M6502_RW) != 0));
    pin_out(s, p, C_SYNC, DRV_BIT((c->pins & M6502_SYNC) != 0));
    bool write = !(c->pins & M6502_RW);
    part_drive_bus_cached(s, p, bus_d, 8, M6502_GET_DATA(c->pins), write && phi0, &c->cache_d);
}

static void cpu_commit(sim_t *s, part_t *p) {
    pet_cpu_t *c = p->state;
    if (p->dead) return;
    int phi0 = pin_in(s, p, C_PHI0);
    if (phi0) c->latched = (uint8_t)part_read_bus(s, p, bus_d, 8);
    if (!phi0 && c->phi0) {
        if (!pin_in(s, p, C_RES)) {
            c->in_reset = 1;
        } else {
            if (c->in_reset) { c->in_reset = 0; c->pins |= M6502_RES; }
            uint64_t pins = c->pins & ~(M6502_IRQ | M6502_NMI | M6502_RDY);
            if (!pin_in(s, p, C_IRQ)) pins |= M6502_IRQ;
            if (!pin_in(s, p, C_NMI)) pins |= M6502_NMI;
            if (!pin_in(s, p, C_RDY)) pins |= M6502_RDY;
            if (pins & M6502_RW) M6502_SET_DATA(pins, c->latched);
            c->pins = m6502_tick(&c->core, pins);
            c->cycles++;
            sim_mark_dirty(s, (int)(p - s->parts));
        }
    }
    c->phi0 = (uint8_t)phi0;
}

static void cpu_reset(sim_t *s, part_t *p) {
    (void)s;
    pet_cpu_t *c = p->state;
    memset(c, 0, sizeof(*c));
    c->pins = m6502_init(&c->core, &(m6502_desc_t){ .bcd_disabled = false });
    c->in_reset = 1;
}

const part_type_t PT_PET6502 = { "6502",
    "VSS:P RDY:m PHI1:O /IRQ:m NC:O /NMI:m SYNC:O VCC:P A0:O A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O "
    "A10:O A11:O VSS:P A12:O A13:O A14:O A15:O D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B R/W:O NC:O NC:O "
    "PHI0:M SO:m PHI2:O /RES:m",
    cpu_eval, cpu_commit, cpu_reset, sizeof(pet_cpu_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* Shared bus access state of the PIAs and the VIA                           */
/* ------------------------------------------------------------------------ */

typedef struct {
    uint8_t clk;                /* clock level at the last step */
    uint8_t sel, rw, rs, data;  /* sampled while the clock is high */
    uint8_t out_en, out_data;   /* a read is being answered */
} acc_t;

/* ------------------------------------------------------------------------ */
/* 6520 PIA (UB12, UB16)                                                     */
/* ------------------------------------------------------------------------ */

enum { P_CB1 = 18, P_CB2 = 19, P_RW = 21, P_CS0 = 22, P_CS2 = 23, P_CS1 = 24, P_PHI2 = 25, P_RES = 34, P_RS1 = 35,
       P_RS0 = 36, P_IRQB = 37, P_IRQA = 38, P_CA2 = 39, P_CA1 = 40 };

typedef struct {
    m6520_t core;
    acc_t acc;
    uint8_t irqa, irqb;         /* outputs asserted */
} pet_pia_t;

/* The pin word the core wants: control lines, port pins, the register. */
static uint64_t pia_pins(sim_t *s, part_t *p, const pet_pia_t *c, bool cs, bool rw, uint8_t rs, uint8_t data) {
    uint64_t pins = 0;
    if (cs) pins |= M6520_CS;
    if (rw) pins |= M6520_RW;
    if (!pin_in(s, p, P_RES)) pins |= M6520_RES;
    pins |= (uint64_t)(rs & 3) << M6520_PIN_RS0;
    M6520_SET_DATA(pins, data);
    if (pin_in(s, p, P_CA1)) pins |= M6520_CA1;
    if (pin_in(s, p, P_CA2)) pins |= M6520_CA2;
    if (pin_in(s, p, P_CB1)) pins |= M6520_CB1;
    if (pin_in(s, p, P_CB2)) pins |= M6520_CB2;
    uint8_t pa = 0, pb = 0;
    for (int i = 0; i < 8; i++) {
        pa |= (uint8_t)(pin_in(s, p, 2 + i) << i);
        pb |= (uint8_t)(pin_in(s, p, 10 + i) << i);
    }
    M6520_SET_PAB(pins, pa, pb);
    (void)c;
    return pins;
}

static void pia_eval(sim_t *s, part_t *p) {
    pet_pia_t *c = p->state;
    const m6520_t *m = &c->core;
    for (int i = 0; i < 8; i++) {
        /* port A has internal pull-ups: an undriven PA7 (DIAG) reads high, as the KERNAL expects at reset */
        pin_out(s, p, 2 + i, (m->ddra >> i) & 1 ? DRV_BIT((m->ora >> i) & 1) : DRV_PULLUP);
        pin_out(s, p, 10 + i, (m->ddrb >> i) & 1 ? DRV_BIT((m->orb >> i) & 1) : DRV_Z);
    }
    pin_out(s, p, P_CA2, m6520_ca2_oe(m) ? DRV_BIT(m->ca2_out) : DRV_Z);
    pin_out(s, p, P_CB2, m6520_cb2_oe(m) ? DRV_BIT(m->cb2_out) : DRV_Z);
    pin_out(s, p, P_IRQA, c->irqa ? DRV_LO : DRV_Z);
    pin_out(s, p, P_IRQB, c->irqb ? DRV_LO : DRV_Z);
    part_drive_bus(s, p, bus_d, 8, c->acc.out_data, c->acc.out_en && c->acc.clk);
}

static void pia_commit(sim_t *s, part_t *p) {
    pet_pia_t *c = p->state;
    if (p->dead) return;
    acc_t *a = &c->acc;
    int part = (int)(p - s->parts), clk = pin_in(s, p, P_PHI2);
    bool dirty = false;
    if (clk) {
        a->sel = pin_in(s, p, P_CS0) && pin_in(s, p, P_CS1) && !pin_in(s, p, P_CS2);
        a->rw = (uint8_t)pin_in(s, p, P_RW);
        a->rs = (uint8_t)(pin_in(s, p, P_RS0) | pin_in(s, p, P_RS1) << 1);
        a->data = (uint8_t)part_read_bus(s, p, bus_d, 8);
        bool want = a->sel && a->rw;
        if (want != (bool)a->out_en) {
            a->out_en = want;
            if (want) {         /* answer from a copy: the real read happens when phi2 falls */
                m6520_t tmp = c->core;
                uint64_t pins = m6520_tick(&tmp, pia_pins(s, p, c, true, true, a->rs, 0));
                a->out_data = M6520_GET_DATA(pins);
            }
            dirty = true;
        }
    } else if (a->clk) {
        a->out_en = 0;
        uint64_t pins = m6520_tick(&c->core, pia_pins(s, p, c, a->sel, a->rw, a->rs, a->data));
        uint8_t ia = (pins & M6520_IRQA) != 0, ib = (pins & M6520_IRQB) != 0;
        c->irqa = ia; c->irqb = ib;
        a->sel = 0;
        dirty = true;
    }
    a->clk = (uint8_t)clk;
    if (dirty) sim_mark_dirty(s, part);
}

static void pia_reset(sim_t *s, part_t *p) {
    (void)s;
    pet_pia_t *c = p->state;
    memset(c, 0, sizeof(*c));
    m6520_init(&c->core);
}

#define PIA_PINS "VSS:P PA0:B PA1:B PA2:B PA3:B PA4:B PA5:B PA6:B PA7:B PB0:B PB1:B PB2:B PB3:B PB4:B PB5:B " \
                 "PB6:B PB7:B CB1:m CB2:B VCC:P R/W:m CS0:m /CS2:m CS1:m PHI2:M D7:B D6:B D5:B D4:B D3:B D2:B " \
                 "D1:B D0:B /RES:m RS1:m RS0:m /IRQB:O /IRQA:O CA2:B CA1:m"
const part_type_t PT_PET6520 = { "6520", PIA_PINS, pia_eval, pia_commit, pia_reset, sizeof(pet_pia_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 6522 VIA (UB15)                                                           */
/* ------------------------------------------------------------------------ */

enum { V_CB1 = 18, V_CB2 = 19, V_IRQ = 21, V_RW = 22, V_CS2 = 23, V_CS1 = 24, V_PHI2 = 25, V_RES = 34, V_RS3 = 35,
       V_RS2 = 36, V_RS1 = 37, V_RS0 = 38, V_CA2 = 39, V_CA1 = 40 };

typedef struct {
    m6522_t core;
    acc_t acc;
    uint8_t irq;
} pet_via_t;

static uint64_t via_pins(sim_t *s, part_t *p, bool cs, bool rw, uint8_t rs, uint8_t data) {
    uint64_t pins = 0;
    if (cs) pins |= M6522_CS;
    if (rw) pins |= M6522_RW;
    if (!pin_in(s, p, V_RES)) pins |= M6522_RES;
    pins |= (uint64_t)(rs & 15) << M6522_PIN_RS0;
    pins = (pins & ~(0xFFULL << M6522_PIN_D0)) | ((uint64_t)data << M6522_PIN_D0);
    if (pin_in(s, p, V_CA1)) pins |= M6522_CA1;
    if (pin_in(s, p, V_CA2)) pins |= M6522_CA2;
    if (pin_in(s, p, V_CB1)) pins |= M6522_CB1;
    if (pin_in(s, p, V_CB2)) pins |= M6522_CB2;
    uint8_t pa = 0, pb = 0;
    for (int i = 0; i < 8; i++) {
        pa |= (uint8_t)(pin_in(s, p, 2 + i) << i);
        pb |= (uint8_t)(pin_in(s, p, 10 + i) << i);
    }
    M6522_SET_PAB(pins, pa, pb);
    return pins;
}

static void via_eval(sim_t *s, part_t *p) {
    pet_via_t *c = p->state;
    const m6522_t *m = &c->core;
    uint8_t oea = m6522_pa_oe(m), oeb = m6522_pb_oe(m);
    for (int i = 0; i < 8; i++) {
        pin_out(s, p, 2 + i, (oea >> i) & 1 ? DRV_BIT((m->ora >> i) & 1) : DRV_Z);
        bool pb7 = i == 7 && (m->acr & 0x80);
        pin_out(s, p, 10 + i, (oeb >> i) & 1 ? DRV_BIT(pb7 ? m->pb7_out : (m->orb >> i) & 1) : DRV_Z);
    }
    pin_out(s, p, V_CA2, m6522_ca2_oe(m) ? DRV_BIT(m->ca2_out) : DRV_Z);
    bool shifting_out = (m->acr & 0x10) != 0;
    pin_out(s, p, V_CB2, m6522_cb2_oe(m) ? DRV_BIT(shifting_out ? m->cb2_sr : m->cb2_out) : DRV_Z);
    pin_out(s, p, V_CB1, m6522_cb1_oe(m) ? DRV_BIT(m->cb1_out_level) : DRV_Z);
    pin_out(s, p, V_IRQ, c->irq ? DRV_LO : DRV_Z);
    part_drive_bus(s, p, bus_d, 8, c->acc.out_data, c->acc.out_en && c->acc.clk);
}

static void via_commit(sim_t *s, part_t *p) {
    pet_via_t *c = p->state;
    if (p->dead) return;
    acc_t *a = &c->acc;
    int part = (int)(p - s->parts), clk = pin_in(s, p, V_PHI2);
    bool dirty = false;
    if (clk) {
        a->sel = pin_in(s, p, V_CS1) && !pin_in(s, p, V_CS2);
        a->rw = (uint8_t)pin_in(s, p, V_RW);
        a->rs = (uint8_t)(pin_in(s, p, V_RS0) | pin_in(s, p, V_RS1) << 1 | pin_in(s, p, V_RS2) << 2 |
                          pin_in(s, p, V_RS3) << 3);
        a->data = (uint8_t)part_read_bus(s, p, bus_d, 8);
        bool want = a->sel && a->rw;
        if (want != (bool)a->out_en) {
            a->out_en = want;
            if (want) {
                m6522_t tmp = c->core;
                uint64_t pins = m6522_tick(&tmp, via_pins(s, p, true, true, a->rs, 0));
                a->out_data = (uint8_t)(pins >> M6522_PIN_D0);
            }
            dirty = true;
        }
    } else if (a->clk) {
        a->out_en = 0;
        uint64_t pins = m6522_tick(&c->core, via_pins(s, p, a->sel, a->rw, a->rs, a->data));
        c->irq = (pins & M6522_IRQ) != 0;
        a->sel = 0;
        dirty = true;
    }
    a->clk = (uint8_t)clk;
    if (dirty) sim_mark_dirty(s, part);
}

static void via_reset(sim_t *s, part_t *p) {
    (void)s;
    pet_via_t *c = p->state;
    memset(c, 0, sizeof(*c));
    m6522_init(&c->core);
}

const part_type_t PT_PET6522 = { "6522",
    "VSS:P PA0:B PA1:B PA2:B PA3:B PA4:B PA5:B PA6:B PA7:B PB0:B PB1:B PB2:B PB3:B PB4:B PB5:B PB6:B PB7:B "
    "CB1:B CB2:B VCC:P /IRQ:O R/W:m /CS2:m CS1:m PHI2:M D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B /RES:m "
    "RS3:m RS2:m RS1:m RS0:m CA2:B CA1:m",
    via_eval, via_commit, via_reset, sizeof(pet_via_t), TECH_NMOS };

/* The 6522 of the VIC-20 board: the port pins that are inputs are pulled up by the chip (the keyboard
 * rows are read through them with nothing else on the line). */
static void via_eval_pullup(sim_t *s, part_t *p) {
    via_eval(s, p);
    const m6522_t *m = &((pet_via_t *)p->state)->core;
    uint8_t oea = m6522_pa_oe(m), oeb = m6522_pb_oe(m);
    for (int i = 0; i < 8; i++) {
        if (!((oea >> i) & 1)) pin_out(s, p, 2 + i, DRV_PULLUP);
        if (!((oeb >> i) & 1)) pin_out(s, p, 10 + i, DRV_PULLUP);
    }
}

const part_type_t PT_VIC6522 = { "6522",
    "VSS:P PA0:B PA1:B PA2:B PA3:B PA4:B PA5:B PA6:B PA7:B PB0:B PB1:B PB2:B PB3:B PB4:B PB5:B PB6:B PB7:B "
    "CB1:B CB2:B VCC:P /IRQ:O R/W:m /CS2:m CS1:m PHI2:M D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B /RES:m "
    "RS3:m RS2:m RS1:m RS0:m CA2:B CA1:m",
    via_eval_pullup, via_commit, via_reset, sizeof(pet_via_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 6545 CRT controller (UB13)                                                */
/* ------------------------------------------------------------------------ */

enum { T_RES = 2, T_LPEN = 3, T_DE = 18, T_CURSOR = 19, T_CLK = 21, T_RW = 22, T_E = 23, T_RS = 24, T_CS = 25,
       T_HSYNC = 39, T_VSYNC = 40 };
static const int crtc_ma[14] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };
static const int crtc_ra[5] = { 38, 37, 36, 35, 34 };

typedef struct {
    m6545_t core;
    uint8_t e, clk;
    uint8_t sel, rw, rs, data;      /* sampled while E is high */
    uint8_t pending, p_rw, p_rs, p_data;    /* an access done at the next character clock */
    uint8_t out_en, out_data;
    uint64_t pins;                  /* the outputs as the pins have them */
    uint64_t next_pins;             /* what the last character clock put out: it takes CRTC_TCO steps to get there */
    uint8_t delay;
    int32_t cache_ma, cache_ra;
} pet_crtc_t;

/* A 6545 takes about 150 ns after the falling edge of CLK to change its outputs (the datasheet
 * gives up to 250 ns). With the access time of the display RAM (parts.c, PT_2114_PET) this is
 * what makes the character data arrive after the latch UB3 closed (see there). */
#ifndef CRTC_TCO_STEPS
#define CRTC_TCO_STEPS 3
#endif

static uint64_t crtc_pins(sim_t *s, part_t *p, bool cs, bool rw, uint8_t rs, uint8_t data) {
    uint64_t pins = 0;
    if (cs) pins |= M6545_CS;
    if (rw) pins |= M6545_RW;
    if (rs) pins |= M6545_RS;
    if (!pin_in(s, p, T_RES)) pins |= M6545_RES;
    if (pin_in(s, p, T_LPEN)) pins |= M6545_LPEN;
    M6545_SET_DATA(pins, data);
    return pins;
}

static void crtc_eval(sim_t *s, part_t *p) {
    pet_crtc_t *c = p->state;
    part_drive_bus_cached(s, p, crtc_ma, 14, M6545_GET_MA(c->pins), true, &c->cache_ma);
    part_drive_bus_cached(s, p, crtc_ra, 5, M6545_GET_RA(c->pins), true, &c->cache_ra);
    pin_out(s, p, T_DE, DRV_BIT((c->pins & M6545_DE) != 0));
    pin_out(s, p, T_HSYNC, DRV_BIT((c->pins & M6545_HSYNC) != 0));
    pin_out(s, p, T_VSYNC, DRV_BIT((c->pins & M6545_VSYNC) != 0));
    pin_out(s, p, T_CURSOR, DRV_BIT((c->pins & M6545_CURSOR) != 0));
    part_drive_bus(s, p, bus_d, 8, c->out_data, c->out_en && c->e);
}

static void crtc_commit(sim_t *s, part_t *p) {
    pet_crtc_t *c = p->state;
    if (p->dead) return;
    int part = (int)(p - s->parts), e = pin_in(s, p, T_E), clk = pin_in(s, p, T_CLK);
    bool dirty = false;
    if (e) {
        c->sel = !pin_in(s, p, T_CS);
        c->rw = (uint8_t)pin_in(s, p, T_RW);
        c->rs = (uint8_t)pin_in(s, p, T_RS);
        c->data = (uint8_t)part_read_bus(s, p, bus_d, 8);
        bool want = c->sel && c->rw;
        if (want != (bool)c->out_en) {
            c->out_en = want;
            if (want) {
                m6545_t tmp = c->core;
                uint64_t pins = m6545_tick(&tmp, crtc_pins(s, p, true, true, c->rs, 0));
                c->out_data = M6545_GET_DATA(pins);
            }
            dirty = true;
        }
    } else if (c->e) {
        c->out_en = 0;
        if (c->sel) { c->pending = 1; c->p_rw = c->rw; c->p_rs = c->rs; c->p_data = c->data; }
        c->sel = 0;
        dirty = true;
    }
    c->e = (uint8_t)e;
    if (!clk && c->clk) {       /* the character clock falls: one character */
        uint64_t pins = crtc_pins(s, p, c->pending, c->p_rw, c->p_rs, c->p_data);
        c->next_pins = m6545_tick(&c->core, pins);
        c->delay = CRTC_TCO_STEPS;
        c->pending = 0;
    }
    if (c->delay && --c->delay == 0) { c->pins = c->next_pins; dirty = true; }
    c->clk = (uint8_t)clk;
    if (dirty) sim_mark_dirty(s, part);
}

static void crtc_reset(sim_t *s, part_t *p) {
    (void)s;
    pet_crtc_t *c = p->state;
    memset(c, 0, sizeof(*c));
    m6545_init(&c->core);
}

const part_type_t PT_PET6545 = { "6545",
    "VSS:P /RES:m LPSTB:m MA0:O MA1:O MA2:O MA3:O MA4:O MA5:O MA6:O MA7:O MA8:O MA9:O MA10:O MA11:O MA12:O "
    "MA13:O DE:O CURSOR:O VCC:P CLK:m R/W:m E:M RS:m /CS:m D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B RA4:O "
    "RA3:O RA2:O RA1:O RA0:O HSYNC:O VSYNC:O",
    crtc_eval, crtc_commit, crtc_reset, sizeof(pet_crtc_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* For the machine: what the cores hold                                      */
/* ------------------------------------------------------------------------ */

uint32_t pet_cpu_cycles_of(const part_t *p) { return ((const pet_cpu_t *)p->state)->cycles; }

void pet_cpu_regs_of(const part_t *p, char *buf, int len) {
    const pet_cpu_t *c = p->state;
    snprintf(buf, (size_t)len, "PC=%04X A=%02X X=%02X Y=%02X S=%02X P=%02X", c->core.PC, c->core.A, c->core.X,
             c->core.Y, c->core.S, c->core.P);
}

/* The 6545's registers, for the screen size and the cursor (--text). */
uint8_t pet_crtc_reg_of(const part_t *p, int r) { return ((const pet_crtc_t *)p->state)->core.reg[r & 31]; }
