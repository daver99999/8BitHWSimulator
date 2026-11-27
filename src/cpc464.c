/* cpc464.c - the Amstrad CPC 464 main board (Z70100).
 *
 * HOW TIME MOVES
 * One simulator cycle is one period of the 16 MHz crystal, two steps (half periods) each. The oscillator (IC125, X101) is not simulated
 * as a circuit: IC125 pin 8, the gate array's XTAL input, is driven by the machine. The gate array (IC116, gate array model below) does
 * everything else from that clock, 16 ticks to the microsecond:
 *     ticks 0-3   video byte 0 (RAS, CAS, the address multiplexers switched to the 6845's address, CCLK low)
 *     ticks 4-7   video byte 1 (CCLK high)
 *     ticks 8-15  the Z80's RAM access (multiplexers on the CPU's address). READY, which holds the Z80 and the 74LS373 (IC114) latch, is
 *                 released from tick 8 to 13: every Z80 memory or I/O cycle is stretched to a whole number of microseconds.
 * The Z80 clock is 4 ticks (CLK high on ticks 0-1 of each 4, produced by Q103 as the inverse of the gate array's PHI pin), the AY gets
 * the 1 MHz /CPU ADDR, the 6845 the 1 MHz CCLK. The exact tick of each strobe inside the microsecond is this model's own: the real chip's
 * is not documented well enough to copy, so scope timings of RAS and CAS are approximate. The logic (which cycle uses RAM or ROM, the
 * registers written by OUT &7Fxx, the interrupt counter, the video) is as the CPC works.
 *
 * WHAT IS MODELED: Z80A, gate array, ROM (32K, lower 16K OS at $0000, upper 16K BASIC at $C000), the 8255, the AY-3-8912, the 6845
 * (the simulator's 6545 core), the multiplexers and buffers, the eight 4164s, the keyboard matrix and the reset circuit. NOT modeled: the
 * tape and printer circuits' analog parts, the sound output (it is not played), the monitor's analog levels (the picture is drawn from
 * the gate array's colours), the joystick, the expansion port's devices. The purpose is to troubleshoot the board, not to load programs. */
#include "cpc464.h"
#include "coco_chips.h"
#include "sokol/sokol_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define CPC_RES_STEPS 800000        /* 50 ms of the 16 MHz clock */

/* ------------------------------------------------------------------------ */
/* Z80A (IC111)                                                              */
/* ------------------------------------------------------------------------ */

static const int cz_addr_pins[16] = { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5 };
static const int cz_data_pins[8] = { 14, 15, 12, 8, 7, 9, 10, 13 };
enum { Z_CLK = 6, Z_INT = 16, Z_NMI = 17, Z_HALT = 18, Z_MREQ = 19, Z_IORQ = 20, Z_RD = 21, Z_WR = 22,
       Z_BUSAK = 23, Z_WAIT = 24, Z_BUSRQ = 25, Z_RESET = 26, Z_M1 = 27, Z_RFSH = 28 };

static void cz_eval(sim_t *s, part_t *p) {
    cpc464_t *z = CPC(p->user);
    uint64_t pins = z->cpu_pins;
    bool aoe = (pins & Z80_AOE) != 0;
    part_drive_bus_cached(s, p, cz_addr_pins, 16, Z80_GET_ADDR(pins), aoe, &z->cache_addr);
    pin_out(s, p, Z_MREQ, aoe ? DRV_BIT(!(pins & Z80_MREQ)) : DRV_Z);
    pin_out(s, p, Z_IORQ, aoe ? DRV_BIT(!(pins & Z80_IORQ)) : DRV_Z);
    pin_out(s, p, Z_RD, aoe ? DRV_BIT(!(pins & Z80_RD)) : DRV_Z);
    pin_out(s, p, Z_WR, aoe ? DRV_BIT(!(pins & Z80_WR)) : DRV_Z);
    pin_out(s, p, Z_M1, DRV_BIT(!(pins & Z80_M1)));
    pin_out(s, p, Z_RFSH, DRV_BIT(!(pins & Z80_RFSH)));
    pin_out(s, p, Z_HALT, DRV_BIT(!(pins & Z80_HALT)));
    pin_out(s, p, Z_BUSAK, DRV_BIT(!(pins & Z80_BUSAK)));
    part_drive_bus_cached(s, p, cz_data_pins, 8, Z80_GET_DATA(pins), (pins & Z80_DOE) != 0, &z->cache_data);
}
static const part_type_t PT_CZ80 = { "Z80A",
    "A11:O A12:O A13:O A14:O A15:O CLK:m D4:B D3:B D5:B D6:B VCC:P D2:B D7:B D0:B D1:B /INT:m /NMI:m "
    "/HALT:O /MREQ:O /IORQ:O /RD:O /WR:O /BUSAK:O /WAIT:m /BUSRQ:m /RESET:m /M1:O /RFSH:O GND:P A0:O "
    "A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O A10:O", cz_eval, 0, 0, 0, TECH_NMOS };

static void cz_edge(board_t *b, int clk) {
    cpc464_t *z = CPC(b);
    sim_t *s = &b->sim;
    part_t *p = &s->parts[z->p_cpu];
    if (p->dead) return;
    uint64_t pins = clk ? Z80_CLK : 0;
    Z80_SET_DATA(pins, (uint64_t)part_read_bus(s, p, cz_data_pins, 8));
    if (!pin_in(s, p, Z_INT)) pins |= Z80_INT;
    if (!pin_in(s, p, Z_NMI)) pins |= Z80_NMI;
    if (!pin_in(s, p, Z_WAIT)) pins |= Z80_WAIT;
    if (!pin_in(s, p, Z_RESET)) pins |= Z80_RES;
    if (!pin_in(s, p, Z_BUSRQ)) pins |= Z80_BUSRQ;
    z->in_reset = (pins & Z80_RES) != 0;
    uint64_t out = z80_tick(&z->cpu, pins);
    if (out != z->cpu_pins) { z->cpu_pins = out; sim_mark_dirty(s, z->p_cpu); }
}

/* ------------------------------------------------------------------------ */
/* ROM IC103 (32K, 27256 pin-out): /CS = ROMEN, /OE = ROMDIS                  */
/* ------------------------------------------------------------------------ */

static const int cr_addr_pins[15] = { 10, 9, 8, 7, 6, 5, 4, 3, 25, 24, 21, 23, 2, 26, 27 };
static const int cr_data_pins[8] = { 11, 12, 13, 15, 16, 17, 18, 19 };
static void cr_eval(sim_t *s, part_t *p) {
    rom_state_t *st = p->state;
    bool selected = !pin_in(s, p, 20) && !pin_in(s, p, 22);
    unsigned a = selected ? part_read_bus(s, p, cr_addr_pins, 15) : 0;
    part_drive_bus_cached(s, p, cr_data_pins, 8, st->data ? st->data[a] : 0xFF, selected, &st->out_cache);
}
static const part_type_t PT_CPC_ROM = { "27256 ROM",
    "NC:O A12:M A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O D7:O "
    "/CS:M A10:M /OE:M A11:M A9:M A8:M A13:M A14:M VCC:P", cr_eval, 0, 0, sizeof(rom_state_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* AY-3-8912 (IC102): BDIR/BC1 (BC2 and A8 are high): 01 read, 10 write, 11 latch address                   */
/* ------------------------------------------------------------------------ */

typedef struct { ay8912_t ay; uint8_t clk_prev, mode, data; } ay_state_t;
static const int ay_data_pins[8] = { 28, 27, 26, 25, 24, 23, 22, 21 };
static const int ay_ioa_pins[8] = { 14, 13, 12, 11, 10, 9, 8, 7 };
static int ay_mode(sim_t *s, part_t *p) {
    if (!pin_in(s, p, 17) || !pin_in(s, p, 19)) return AY_BUS_NONE;
    int bdir = pin_in(s, p, 18), bc1 = pin_in(s, p, 20);
    return bdir ? (bc1 ? AY_BUS_ADDR : AY_BUS_WRITE) : (bc1 ? AY_BUS_READ : AY_BUS_NONE);
}
static void ay_eval(sim_t *s, part_t *p) {
    ay_state_t *st = p->state;
    bool rd = ay_mode(s, p) == AY_BUS_READ;
    part_drive_bus(s, p, ay_data_pins, 8, rd ? ay8912_bus(&st->ay, AY_BUS_READ, 0) : 0, rd);
    bool out = (st->ay.reg[7] & 0x40) != 0;
    for (int i = 0; i < 8; i++)
        pin_out(s, p, ay_ioa_pins[i], out ? DRV_BIT((st->ay.reg[14] >> i) & 1) : DRV_PULLUP);
}
static void ay_commit(sim_t *s, part_t *p) {
    ay_state_t *st = p->state;
    int mode = ay_mode(s, p);
    if (mode != AY_BUS_NONE && mode != AY_BUS_READ) {
        st->mode = (uint8_t)mode;
        st->data = (uint8_t)part_read_bus(s, p, ay_data_pins, 8);
    } else if (st->mode) {
        ay8912_bus(&st->ay, st->mode, st->data);
        st->mode = 0;
        sim_mark_dirty(s, (int)(p - s->parts));
    }
    int ck = pin_in(s, p, 15);
    if (ck && !st->clk_prev) ay8912_tick(&st->ay);
    st->clk_prev = (uint8_t)ck;
    if (!pin_in(s, p, 16)) { ay8912_reset(&st->ay); st->mode = 0; }
    uint8_t in = (uint8_t)part_read_bus(s, p, ay_ioa_pins, 8);
    if (in != st->ay.port_a_in) { st->ay.port_a_in = in; sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void ay_reset(sim_t *s, part_t *p) {
    (void)s;
    ay_state_t *st = p->state;
    memset(st, 0, sizeof(*st));
    ay8912_reset(&st->ay);
    st->ay.port_a_in = 0xFF;
}
static const part_type_t PT_CPC_AY = { "AY-3-8912",
    "C:O TEST1:T VCC:P B:O A:O GND:P IOA7:X IOA6:X IOA5:X IOA4:X IOA3:X IOA2:X IOA1:X IOA0:X CLOCK:T RESET:T "
    "A8:T BDIR:T BC2:T BC1:T D7:X D6:X D5:X D4:X D3:X D2:X D1:X D0:X",
    ay_eval, ay_commit, ay_reset, sizeof(ay_state_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 8255 PPI (IC107), mode 0 and bit set / reset                              */
/* ------------------------------------------------------------------------ */

typedef struct { uint8_t pa, pb, pc, ctrl, pending, waddr, wdata; } ppi_state_t;
static const int pp_a[8] = { 4, 3, 2, 1, 40, 39, 38, 37 }, pp_b[8] = { 18, 19, 20, 21, 22, 23, 24, 25 };
static const int pp_cl[4] = { 14, 15, 16, 17 }, pp_ch[4] = { 13, 12, 11, 10 };
static const int pp_d[8] = { 34, 33, 32, 31, 30, 29, 28, 27 };
enum { PP_RD = 5, PP_CS = 6, PP_A1 = 8, PP_A0 = 9, PP_RESET = 35, PP_WR = 36 };

static uint8_t pp_pc_pins(sim_t *s, part_t *p) {
    return (uint8_t)(part_read_bus(s, p, pp_cl, 4) | (part_read_bus(s, p, pp_ch, 4) << 4));
}
static void ppi_eval(sim_t *s, part_t *p) {
    ppi_state_t *st = p->state;
    bool a_out = !(st->ctrl & 0x10), b_out = !(st->ctrl & 0x02), cu_out = !(st->ctrl & 0x08), cl_out = !(st->ctrl & 0x01);
    for (int i = 0; i < 8; i++) {
        pin_out(s, p, pp_a[i], a_out ? DRV_BIT((st->pa >> i) & 1) : DRV_Z);
        pin_out(s, p, pp_b[i], b_out ? DRV_BIT((st->pb >> i) & 1) : DRV_Z);
    }
    for (int i = 0; i < 4; i++) {
        pin_out(s, p, pp_cl[i], cl_out ? DRV_BIT((st->pc >> i) & 1) : DRV_Z);
        pin_out(s, p, pp_ch[i], cu_out ? DRV_BIT((st->pc >> (4 + i)) & 1) : DRV_Z);
    }
    bool rd = !pin_in(s, p, PP_CS) && !pin_in(s, p, PP_RD);
    unsigned v = 0xFF;
    if (rd) {
        int addr = (pin_in(s, p, PP_A1) << 1) | pin_in(s, p, PP_A0);
        if (addr == 0) v = a_out ? st->pa : (unsigned)part_read_bus(s, p, pp_a, 8);
        else if (addr == 1) v = b_out ? st->pb : (unsigned)part_read_bus(s, p, pp_b, 8);
        else if (addr == 2) {
            unsigned pins = pp_pc_pins(s, p);
            v = (cl_out ? (st->pc & 0x0F) : (pins & 0x0F)) | (cu_out ? (st->pc & 0xF0) : (pins & 0xF0));
        }
    }
    part_drive_bus(s, p, pp_d, 8, v, rd);
}
static void ppi_commit(sim_t *s, part_t *p) {
    ppi_state_t *st = p->state;
    if (p->dead) return;
    bool dirty = false;
    if (pin_in(s, p, PP_RESET)) {
        if (st->ctrl != 0x9B || st->pa || st->pb || st->pc) { st->ctrl = 0x9B; st->pa = st->pb = st->pc = 0; dirty = true; }
        st->pending = 0;
    } else {
        bool w = !pin_in(s, p, PP_CS) && !pin_in(s, p, PP_WR);
        if (w) {
            st->pending = 1;
            st->waddr = (uint8_t)((pin_in(s, p, PP_A1) << 1) | pin_in(s, p, PP_A0));
            st->wdata = (uint8_t)part_read_bus(s, p, pp_d, 8);
        } else if (st->pending) {
            st->pending = 0;
            switch (st->waddr) {
            case 0: st->pa = st->wdata; break;
            case 1: st->pb = st->wdata; break;
            case 2: st->pc = st->wdata; break;
            default:
                if (st->wdata & 0x80) { st->ctrl = st->wdata; st->pa = st->pb = st->pc = 0; }
                else {
                    int bit = (st->wdata >> 1) & 7;
                    if (st->wdata & 1) st->pc |= (uint8_t)(1 << bit); else st->pc &= (uint8_t)~(1 << bit);
                }
            }
            dirty = true;
        }
    }
    if (dirty) sim_mark_dirty(s, (int)(p - s->parts));
}
static void ppi_reset(sim_t *s, part_t *p) {
    (void)s;
    ppi_state_t *st = p->state;
    memset(st, 0, sizeof(*st));
    st->ctrl = 0x9B;
}
static const part_type_t PT_CPC_PPI = { "8255",
    "PA3:X PA2:X PA1:X PA0:X /RD:T /CS:T GND:P A1:T A0:T PC7:X PC6:X PC5:X PC4:X PC0:X PC1:X PC2:X PC3:X "
    "PB0:X PB1:X PB2:X PB3:X PB4:X PB5:X PB6:X PB7:X VCC:P D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B RESET:T "
    "/WR:T PA7:X PA6:X PA5:X PA4:X", ppi_eval, ppi_commit, ppi_reset, sizeof(ppi_state_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* IC125 ZN7400E: three NAND gates, and the XTAL output (gate 3, pin 8), driven by the machine                */
/* ------------------------------------------------------------------------ */

static void osc_eval(sim_t *s, part_t *p) {
    cpc464_t *z = CPC(p->user);
    pin_out(s, p, 3, DRV_BIT(!(pin_in(s, p, 1) && pin_in(s, p, 2))));
    pin_out(s, p, 6, DRV_BIT(!(pin_in(s, p, 4) && pin_in(s, p, 5))));
    pin_out(s, p, 11, DRV_BIT(!(pin_in(s, p, 12) && pin_in(s, p, 13))));
    pin_out(s, p, 8, DRV_BIT(z->xtal));
}
static const part_type_t PT_CPC_OSC = { "ZN7400E",
    "1A:T 1B:T 1Y:O 2A:T 2B:T 2Y:O GND:P 3Y:O 3A:T 3B:T 4Y:O 4A:T 4B:T VCC:P", osc_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* The gate array (IC116, Amstrad 40007)                                      */
/* ------------------------------------------------------------------------ */

enum { GA_CPUADDR = 1, GA_READY = 2, GA_CAS = 3, GA_N244EN = 4, GA_MWE = 5, GA_CASADDR = 6, GA_RAS = 7, GA_XTAL = 8, GA_INT = 10,
       GA_SYNC = 11, GA_ROMEN = 12, GA_RAMRD = 13, GA_HSYNC = 14, GA_VSYNC = 15, GA_IORQ = 16, GA_M1 = 17, GA_MREQ = 18, GA_RD = 19,
       GA_A15 = 20, GA_A14 = 21, GA_DISPEN = 23, GA_B = 32, GA_G = 34, GA_R = 36, GA_RESET = 37, GA_PHI = 39, GA_CCLK = 40 };
static const int ga_d_pins[8] = { 24, 25, 26, 27, 28, 29, 30, 31 };

/* The 32 hardware colours: red, green, blue levels 0, 1, 2 (0, 50%, 100%) */
static const uint8_t hw_rgb[32][3] = {
    { 1, 1, 1 }, { 1, 1, 1 }, { 0, 2, 1 }, { 2, 2, 1 }, { 0, 0, 1 }, { 2, 0, 1 }, { 0, 1, 1 }, { 2, 1, 1 },
    { 2, 0, 1 }, { 2, 2, 1 }, { 2, 2, 0 }, { 2, 2, 2 }, { 2, 0, 0 }, { 2, 0, 2 }, { 2, 1, 0 }, { 2, 1, 2 },
    { 0, 0, 1 }, { 0, 2, 1 }, { 0, 2, 0 }, { 0, 2, 2 }, { 0, 0, 0 }, { 0, 0, 2 }, { 0, 1, 0 }, { 0, 1, 2 },
    { 1, 0, 1 }, { 1, 2, 1 }, { 1, 2, 0 }, { 1, 2, 2 }, { 1, 0, 0 }, { 1, 0, 2 }, { 1, 1, 0 }, { 1, 1, 2 } };

typedef struct {
    uint8_t t;                  /* tick in the microsecond, 0-15 */
    uint8_t xtal_prev;
    /* registered outputs */
    uint8_t cpuaddr, cclk, phi, ras, cas, casaddr, mwe, ready, sync;
    uint8_t cpu_ram, cpu_wr;    /* this microsecond's Z80 slot does a RAM cycle / a write */
    /* registers */
    uint8_t pen, ink[17], rmr;  /* rmr: bits 0-1 mode, 2 lower ROM off, 3 upper ROM off */
    /* interrupts */
    uint8_t ic, vs_wait, int_pending;
    uint8_t hs_prev, vs_prev;
    /* video */
    uint8_t vb0, vb1, db0, db1, disp_sample, disp_cur;
    int lt, ly;                 /* ticks since HSYNC began, lines since VSYNC began */
    uint8_t hs_on, vs_on;
    uint8_t out_r, out_g, out_b;
    uint32_t *fb;
    int32_t cache_dummy;
} ga_state_t;

static bool ga_low(sim_t *s, part_t *p, int pin) { return !pin_in(s, p, pin); }

static bool ga_rom_sel(sim_t *s, part_t *p, const ga_state_t *g) {
    int a15 = pin_in(s, p, GA_A15), a14 = pin_in(s, p, GA_A14);
    if (!a15 && !a14) return !(g->rmr & 4);
    if (a15 && a14) return !(g->rmr & 8);
    return false;
}

static void ga_eval(sim_t *s, part_t *p) {
    ga_state_t *g = p->state;
    pin_out(s, p, GA_CPUADDR, DRV_BIT(g->cpuaddr));
    pin_out(s, p, GA_CCLK, DRV_BIT(g->cclk));
    pin_out(s, p, GA_PHI, DRV_BIT(g->phi));
    pin_out(s, p, GA_RAS, DRV_BIT(g->ras));
    pin_out(s, p, GA_CAS, DRV_BIT(g->cas));
    pin_out(s, p, GA_CASADDR, DRV_BIT(g->casaddr));
    pin_out(s, p, GA_MWE, DRV_BIT(g->mwe));
    pin_out(s, p, GA_READY, g->ready ? DRV_Z : DRV_LO);                  /* open collector */
    pin_out(s, p, GA_INT, g->int_pending ? DRV_LO : DRV_Z);              /* open collector */
    pin_out(s, p, GA_SYNC, DRV_BIT(g->sync));
    static const drive_t lvl[3] = { DRV_LO, DRV_Z, DRV_HI };
    pin_out(s, p, GA_R, lvl[g->out_r]);
    pin_out(s, p, GA_G, lvl[g->out_g]);
    pin_out(s, p, GA_B, lvl[g->out_b]);
    /* combinational: the ROM and RAM read strobes, and the buffer that lets the CPU's data reach the gate array on a register write */
    bool mreq = ga_low(s, p, GA_MREQ), iorq = ga_low(s, p, GA_IORQ), rd = ga_low(s, p, GA_RD), m1 = ga_low(s, p, GA_M1);
    bool rom = ga_rom_sel(s, p, g);
    bool memrd = mreq && rd && !iorq;
    pin_out(s, p, GA_ROMEN, DRV_BIT(!(memrd && rom)));
    pin_out(s, p, GA_RAMRD, DRV_BIT(!(memrd && !rom)));
    bool regwr = iorq && !rd && !m1 && !pin_in(s, p, GA_A15) && pin_in(s, p, GA_A14);
    pin_out(s, p, GA_N244EN, DRV_BIT(!regwr));
}

static void ga_write_reg(ga_state_t *g, uint8_t d) {
    switch (d >> 6) {
    case 0: g->pen = (d & 0x10) ? 16 : (d & 15); break;
    case 1: g->ink[g->pen] = d & 31; break;
    case 2:
        g->rmr = d & 15;
        if (d & 0x10) { g->ic = 0; g->int_pending = 0; }
        break;
    default: break;                 /* RAM banking: not on the 464 */
    }
}

/* One tick of the 16 MHz clock (the crystal's rising edge). */
static void ga_tick(sim_t *s, part_t *p) {
    ga_state_t *g = p->state;
    if (p->dead) return;
    uint8_t t = g->t = (uint8_t)((g->t + 1) & 15);
    bool reset = ga_low(s, p, GA_RESET);
    bool mreq = ga_low(s, p, GA_MREQ), iorq = ga_low(s, p, GA_IORQ), rd = ga_low(s, p, GA_RD), m1 = ga_low(s, p, GA_M1);
    int hs = pin_in(s, p, GA_HSYNC), vs = pin_in(s, p, GA_VSYNC);

    if (reset) {
        g->rmr = 0; g->pen = 0; g->ic = 0; g->int_pending = 0; g->vs_wait = 0;
    }
    /* what the previous tick's strobes brought: video bytes, and a register write */
    if (t == 3) g->vb0 = (uint8_t)part_read_bus(s, p, ga_d_pins, 8);
    if (t == 7) g->vb1 = (uint8_t)part_read_bus(s, p, ga_d_pins, 8);
    if (t == 13 && iorq && !rd && !m1 && !pin_in(s, p, GA_A15) && pin_in(s, p, GA_A14))
        ga_write_reg(g, (uint8_t)part_read_bus(s, p, ga_d_pins, 8));
    if (iorq && m1) { g->int_pending = 0; g->ic &= 0x1F; }          /* interrupt acknowledge */
    /* the interrupt counter: HSYNC falling edge */
    if (g->hs_prev && !hs) {
        g->ic = (uint8_t)((g->ic + 1) & 63);
        if (g->ic == 52) { g->ic = 0; g->int_pending = 1; }
        if (g->vs_wait && --g->vs_wait == 0) {
            if (g->ic >= 32) g->int_pending = 1;
            g->ic = 0;
        }
        g->ly++;
    }
    if (!g->hs_prev && hs) g->lt = 0;
    if (!g->vs_prev && vs) { g->vs_wait = 2; g->ly = 0; }
    g->hs_prev = (uint8_t)hs;
    g->vs_prev = (uint8_t)vs;
    g->hs_on = (uint8_t)hs; g->vs_on = (uint8_t)vs;
    g->lt++;
    /* the strobes of this tick */
    g->cpuaddr = t < 8;
    g->cclk = t >= 4 && t < 12;
    g->phi = (t & 3) >= 2;
    g->ready = t >= 8 && t <= 13;
    if (t < 8) {
        int sub = t & 3;
        g->ras = !(sub < 3);
        g->casaddr = sub >= 1;
        g->cas = !(sub == 1 || sub == 2);
        g->mwe = 1;
    } else {
        if (t == 8) {
            bool rdh = !rd;
            g->cpu_ram = mreq && !iorq && (rdh || !ga_rom_sel(s, p, g));
            g->cpu_wr = rdh;
        }
        if (g->cpu_ram && t <= 14) {
            g->ras = 0;
            g->casaddr = t >= 9;
            int c0 = g->cpu_wr ? 11 : 9;
            g->cas = !(t >= c0);
            g->mwe = g->cpu_wr ? 0 : 1;
        } else {
            g->ras = 1; g->cas = 1; g->casaddr = 0; g->mwe = 1;
        }
    }
    if (t == 0) { g->db0 = g->vb0; g->db1 = g->vb1; g->disp_cur = g->disp_sample; g->disp_sample = (uint8_t)pin_in(s, p, GA_DISPEN); }
    g->sync = !(g->hs_on || g->vs_on);
    /* the picture: one pixel of the 16 MHz clock per tick */
    int xp = g->lt - 14 * 16, yp = g->ly - 36;
    int ink;
    if (g->disp_cur) {
        uint8_t byte = (t < 8) ? g->db0 : g->db1;
        int k = t & 7, pen;
        switch (g->rmr & 3) {
        case 0: {
            int px = k >> 2;
            pen = px == 0 ? (((byte >> 7) & 1) | (((byte >> 3) & 1) << 1) | (((byte >> 5) & 1) << 2) | (((byte >> 1) & 1) << 3))
                          : (((byte >> 6) & 1) | (((byte >> 2) & 1) << 1) | (((byte >> 4) & 1) << 2) | (((byte >> 0) & 1) << 3));
            break; }
        case 1: { int px = k >> 1; pen = ((byte >> (7 - px)) & 1) | (((byte >> (3 - px)) & 1) << 1); break; }
        default: pen = (byte >> (7 - k)) & 1; break;
        }
        ink = g->ink[pen];
    } else ink = g->ink[16];
    g->out_r = hw_rgb[ink][0]; g->out_g = hw_rgb[ink][1]; g->out_b = hw_rgb[ink][2];
    if (g->hs_on || g->vs_on) g->out_r = g->out_g = g->out_b = 0;       /* blanking */
    if (g->fb && xp >= 0 && xp < CPC_SCREEN_W && yp >= 0 && yp < CPC_SCREEN_H) {
        static const uint8_t lv[3] = { 0, 0x80, 0xFF };
        g->fb[yp * CPC_SCREEN_W + xp] = 0xFF000000u | ((uint32_t)lv[g->out_b] << 16) | ((uint32_t)lv[g->out_g] << 8) | lv[g->out_r];
    }
    sim_mark_dirty(s, (int)(p - s->parts));
}

static void ga_reset(sim_t *s, part_t *p) {
    (void)s;
    ga_state_t *g = p->state;
    uint32_t *fb = g->fb;
    memset(g, 0, sizeof(*g));
    g->fb = fb;
    g->ras = g->cas = g->mwe = 1;
    g->ready = 0;
    for (int i = 0; i < 17; i++) g->ink[i] = 20;
}
static const part_type_t PT_CPC_GA = { "Gate array",
    "/CPUADDR:O READY:O /CAS:O /244EN:O /MWE:O /CASADDR:O /RAS:O XTAL:m VCC2:P /INT:O /SYNC:O /ROMEN:O /RAMRD:O HSYNC:m VSYNC:m "
    "/IORQ:M /M1:M /MREQ:M /RD:M A15:M A14:M VCC1:P DISPEN:m D0:B D1:B D2:B D3:B D4:B D5:B D6:B D7:B B:O VCC2:P G:O GND:P R:O "
    "/RESET:m VCC1:P PHI:O CCLK:O", ga_eval, 0, ga_reset, sizeof(ga_state_t), TECH_NMOS };

/* The gate array's registers, for reports. */
void cpc464_ga_status(board_t *b, char *buf, int len) {
    const ga_state_t *g = b->sim.parts[CPC(b)->p_ga].state;
    int n = snprintf(buf, (size_t)len, "GA pen=%d rmr=%02X ic=%d int=%d ly=%d inks:", g->pen, g->rmr, g->ic, g->int_pending, g->ly);
    for (int i = 0; i < 17 && n < len - 4; i++) n += snprintf(buf + n, (size_t)(len - n), " %d", g->ink[i]);
}

/* ------------------------------------------------------------------------ */
/* Chips of the board                                                        */
/* ------------------------------------------------------------------------ */

static const part_def_t cpc_part_defs[] = {
    { "IC111", &PT_CZ80, "CPU", ROLE_CHIP, "Z80A", "Z80A CPU, 4 MHz (stretched to whole microseconds by the gate array)" },
    { "IC116", &PT_CPC_GA, "GATE ARRAY", ROLE_CHIP, "40007\ngate array", "Amstrad gate array: video, CPU clock and wait states, RAM strobes, ROM and RAM select, interrupt timer" },
    { "IC103", &PT_CPC_ROM, "ROM", ROLE_ROM, "40009\n32K ROM", "OS ROM $0000-$3FFF and BASIC ROM $C000-$FFFF, 32K (the first half is the lower ROM, the second the upper)" },
    { "IC107", &PT_CPC_PPI, "8255", ROLE_CHIP, "8255 PPI", "8255: keyboard row select, PSG data and control, vsync, tape and printer signals" },
    { "IC102", &PT_CPC_AY, "AY", ROLE_CHIP, "AY-3-8912\nsound", "General Instrument AY-3-8912 sound chip; its port A reads the keyboard and joystick" },
    { "IC108", &PT_PET6545, "6845", ROLE_CHIP, "6845\nCRTC", "HD6845 CRT controller (the simulator's 6545 core)" },
    { "IC101", &PT_74LS145, "74LS145", ROLE_GLUE, "keyboard\nrow decoder", "BCD to decimal decoder: selects the keyboard row (the 8255's port C bits 0-3)" },
    { "IC104", &PT_74LS153, "74LS153", ROLE_GLUE, "RAM address\nmux", "RAM address multiplexer (CPU address / video address, row / column)" },
    { "IC105", &PT_74LS153, "74LS153", ROLE_GLUE, "RAM address\nmux", "RAM address multiplexer (CPU address / video address, row / column)" },
    { "IC109", &PT_74LS153, "74LS153", ROLE_GLUE, "RAM address\nmux", "RAM address multiplexer (CPU address / video address, row / column)" },
    { "IC113", &PT_74LS153, "74LS153", ROLE_GLUE, "RAM address\nmux", "RAM address multiplexer (CPU address / video address, row / column)" },
    { "IC106", &PT_74LS273, "74LS273", ROLE_GLUE, "printer\nlatch", "Printer data latch (written by OUT &EFxx); bit 7 is the strobe" },
    { "IC110", &PT_74LS00, "74LS132", ROLE_GLUE, "reset and\nI/O gates", "Schmitt NAND gates: the reset (the simulator uses plain NAND thresholds), the 6845's E and the printer strobe" },
    { "IC112", &PT_74LS32, "74LS32", ROLE_GLUE, "OR gates", "OR gates: /IORD, /IOWR, the printer latch clock and the RAM read enable" },
    { "IC114", &PT_74LS373, "74LS373", ROLE_GLUE, "RAM data\nlatch", "RAM data to the CPU: transparent while READY is high, holds while it is low" },
    { "IC115", &PT_74LS244, "74LS244", ROLE_GLUE, "data buffer", "CPU data onto the RAM data lines (for writes to the gate array's registers)" },
    { "IC125", &PT_CPC_OSC, "ZN7400E", ROLE_CLOCK, "16 MHz\noscillator", "The 16 MHz oscillator with X101 (not simulated as a circuit: the machine drives XTAL)" },
#define DRAM(ref, bit) { ref, &PT_4164, "DRAM", ROLE_RAM, "4164\nbit " #bit, "4164 DRAM, 64K x 1: bit " #bit " of the 64K RAM" }
    DRAM("IC117", 3), DRAM("IC118", 2), DRAM("IC119", 1), DRAM("IC120", 0), DRAM("IC121", 4), DRAM("IC122", 5), DRAM("IC123", 6), DRAM("IC124", 7),
};

static const conn_def_t cpc_conn_defs[] = {
    { "CP001", "TAPE", "1=+5V_(A) 2=GND_(B) 3=+5V_(C) 4=WRITE_DATA_(D) 5=READ_DATA_(E) 6=NC_(F) 7=SOUND_(G) 8=MOTOR_(H)" },
    { "CP002", "KEYBOARD", "1=IOA7 2=Y10 3=IOA0 4=IOA1 5=IOA2 6=IOA3 7=IOA4 8=IOA5 9=IOA6 10=IOA7 11=Y9 12=Y8 13=Y7 14=Y6 15=Y5 16=Y4 17=Y3 18=Y2 19=Y1" },
    { "J101", "VIDEO", "1=RED 2=GREEN 3=BLUE 4=SYNC 5=GND 6=LUM" },
    { "J102", "JOYSTICK", "1=UP 2=DOWN 3=LEFT 4=RIGHT 5=SPARE 6=FIRE2 7=FIRE1 8=COMMON 9=COM2" },
    { "J103", "STEREO SOUND", "1=GND 2=RIGHT 3=LEFT" },
    { "J104", "POWER", "1=+5V 2=SWITCH 3=GND" },
    { "PRN", "PRINTER", "1=STROBE 2=D0 3=D1 4=D2 5=D3 6=D4 7=D5 8=D6 9=D7_(GND) 11=BUSY" },
    { "EXPC", "EXPANSION", "1=SOUND 2=GND 3=A15 4=A14 5=A13 6=A12 7=A11 8=A10 9=A9 10=A8 11=A7 12=A6 13=A5 14=A4 15=A3 16=A2 17=A1 18=A0 "
                           "19=D7 20=D6 21=D5 22=D4 23=D3 24=D2 25=D1 26=D0 27=+5V 28=MREQ 29=M1 30=RFSH 31=IORQ 32=RD 33=WR 34=HALT "
                           "35=INT 36=NMI 37=BUSRQ 38=BUSAK 39=READY 40=BUS_RESET 41=RESET 42=ROMEN 43=ROMDIS 44=RAMRD 45=RAMDIS "
                           "46=CURSOR 47=L.PEN 48=EXP 49=GND 50=CLK" },
};

static int cpc_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int cpc_kind_of(const char *ref) {
    if (ref[0] == 'I' && ref[1] == 'C') return PART_IC;
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    if (ref[0] == 'J' || !strncmp(ref, "CP", 2) || !strcmp(ref, "PRN") || !strcmp(ref, "EXPC")) return PART_CONNECTOR;
    return PART_OTHER;
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static part_t *cpc_find(board_t *b, const char *ref) {
    int i = sim_find_part(&b->sim, ref);
    return i >= 0 ? &b->sim.parts[i] : NULL;
}
static int cpc_pin_of(board_t *b, const char *ref, int number) {
    part_t *p = cpc_find(b, ref);
    return p ? part_pin(p, number) : -1;
}

static void cpc_make_element(board_t *b, const char *ref, int pin_a, int pin_b, int type) {
    part_t *p = cpc_find(b, ref);
    if (!p || part_pin(p, pin_a) < 0 || part_pin(p, pin_b) < 0) return;
    resistor_state_t *st = p->state;
    if (!st) { st = calloc(1, sizeof(*st)); p->state = st; p->state_size = sizeof(*st); }
    p->kind = PART_RESISTOR;
    st->nel = 1;
    st->el[0] = (element_t){ part_pin(p, pin_a), part_pin(p, pin_b), EL_ANALOG };
    element_set_type(&b->sim, p, 0, type);
}

/* Key codes: the character it types, or these */
enum { K_UP = 0x81, K_RIGHT, K_DOWN, K_LEFT, K_CLR, K_COPY, K_CAPS, K_SHIFT, K_CTRL, K_F0 = 0x90 };

static void cpc_init_keymap(cpc464_t *z) {
    kbd_init(&z->kbd, 1);
    kbd_register_modifier(&z->kbd, 0, 2, 5);     /* shift */
    kbd_register_modifier(&z->kbd, 1, 2, 7);     /* control */
    /* matrix[row][bit]: the key at that cell, 0 for none. Rows 0-9 are the 74LS145's outputs, bits 0-7 the AY's port A lines. */
    static const int mat[10][8] = {
        { K_UP, K_RIGHT, K_DOWN, K_F0 + 9, K_F0 + 6, K_F0 + 3, 0x0E, K_F0 + 10 },       /* 0x0E: ENTER (keypad), K_F0+10: keypad . */
        { K_LEFT, K_COPY, K_F0 + 7, K_F0 + 8, K_F0 + 5, K_F0 + 1, K_F0 + 2, K_F0 },
        { K_CLR, '[', 0x0D, ']', K_F0 + 4, K_SHIFT, '\\', K_CTRL },
        { '^', '-', '@', 'p', ';', ':', '/', '.' },
        { '0', '9', 'o', 'i', 'l', 'k', 'm', ',' },
        { '8', '7', 'u', 'y', 'h', 'j', 'n', ' ' },
        { '6', '5', 'r', 't', 'g', 'f', 'b', 'v' },
        { '4', '3', 'e', 'w', 's', 'd', 'c', 'x' },
        { '1', '2', 0x1B, 'q', 0x09, 'a', K_CAPS, 'z' },
        { 0, 0, 0, 0, 0, 0, 0, 0x7F } };
    for (int r = 0; r < 10; r++)
        for (int bit = 0; bit < 8; bit++) {
            int k = mat[r][bit];
            if (!k) continue;
            kbd_register_key(&z->kbd, k, r, bit, 0);
            if (k >= 'a' && k <= 'z') kbd_register_key(&z->kbd, k - 32, r, bit, 1);       /* shifted: the capital */
        }
    /* shifted symbols: a key's shifted character, on its cell */
    static const struct { char sh; int row, bit; } shifted[] = {
        { '!', 8, 0 }, { '"', 8, 1 }, { '#', 7, 1 }, { '$', 7, 0 }, { '%', 6, 1 }, { '&', 6, 0 }, { '\'', 5, 1 }, { '(', 5, 0 }, { ')', 4, 1 },
        { '_', 4, 0 }, { '=', 3, 1 }, { '|', 3, 2 }, { '{', 2, 1 }, { '}', 2, 3 }, { '+', 3, 4 }, { '*', 3, 5 }, { '?', 3, 6 },
        { '>', 3, 7 }, { '<', 4, 7 }, { '`', 2, 6 } };
    for (size_t i = 0; i < sizeof(shifted) / sizeof(shifted[0]); i++) kbd_register_key(&z->kbd, shifted[i].sh, shifted[i].row, shifted[i].bit, 1);
}

/* ROM: roms/cpc464/ (or roms, or .): any .rom/.bin of 32768 bytes (not shipped: it is Amstrad's) */
static void cpc_find_roms(board_t *b) {
    cpc464_t *z = CPC(b);
    static const char *const dirs[] = { "roms/cpc464", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]) && !z->rom_ok; d++) {
        char path[1024];
        static const char *const names[] = { "cpc464.rom", "cpc464.bin", "cpc464-os.rom", "os464.rom" };
        for (size_t n = 0; n < sizeof(names) / sizeof(names[0]) && !z->rom_ok; n++) {
            snprintf(path, sizeof(path), "%s/%s", dirs[d], names[n]);
            FILE *f = fopen(path, "rb");
            if (!f) continue;
            uint8_t buf[32769];
            size_t got = fread(buf, 1, sizeof(buf), f);
            fclose(f);
            if (got == sizeof(z->rom)) { memcpy(z->rom, buf, got); z->rom_ok = true; }
        }
    }
}

static void cpc_create(board_t *b) {
    cpc464_t *z = CPC(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "IC111");
    z->p_ga = sim_find_part(s, "IC116");
    z->p_crtc = sim_find_part(s, "IC108");
    z->p_ppi = sim_find_part(s, "IC107");
    z->p_ay = sim_find_part(s, "IC102");
    z->p_rom = sim_find_part(s, "IC103");
    z->p_osc = sim_find_part(s, "IC125");
    z->pin_clk = part_pin(&s->parts[z->p_cpu], Z_CLK);
    for (int i = 0; i < s->nparts; i++) s->parts[i].user = b;
    memset(z->rom, 0xFF, sizeof(z->rom));
    z->rom_ok = false;
    cpc_find_roms(b);
    ((rom_state_t *)s->parts[z->p_rom].state)->data = z->rom;
    ((ga_state_t *)s->parts[z->p_ga].state)->fb = b->screen;
    /* transistors: Q103 inverts the gate array's PHI into the Z80's clock (base pin 3, collector pin 2); Q101 is the motor switch */
    cpc_make_element(b, "Q103", 3, 2, EL_NPN);
    cpc_make_element(b, "Q101", 3, 2, EL_NPN);
    /* the keyboard: a pressed key joins its row (a 74LS145 output) to its column (an AY port A line) */
    static const int rowpin[10] = { 19, 18, 17, 16, 15, 14, 13, 12, 11, 2 };
    for (int r = 0; r < 10; r++) z->pin_row[r] = cpc_pin_of(b, "CP002", rowpin[r]);
    for (int c = 0; c < 8; c++) z->pin_col[c] = cpc_pin_of(b, "CP002", 3 + c);
    cpc_init_keymap(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    z->pin_rc = cpc_pin_of(b, "C102", 1);                   /* the reset RC node, held low at power on */
    b->pin_reset = cpc_pin_of(b, "IC110", 2);               /* the RESET button pulls BUS RESET to ground */
    b->pin_reset_gnd = cpc_pin_of(b, "IC110", 7);
    char name[16];
    int bit = 0;
    static const struct { const char *fmt; int n; } buses[] = { { "A%d", 16 }, { "D%d", 8 }, { "RD%d", 8 }, { "RAM_A%d", 8 } };
    for (size_t k = 0; k < sizeof(buses) / sizeof(buses[0]); k++)
        for (int i = 0; i < buses[k].n; i++) {
            snprintf(name, sizeof(name), buses[k].fmt, i);
            sim_set_bus_bit(s, sim_find_net(s, name), bit++);
        }
}

static void cpc_power_on(board_t *b) {
    cpc464_t *z = CPC(b);
    z->cpu_pins = z80_init(&z->cpu);
    z->in_reset = true;
    z->clk = 0;
    z->xtal = 0;
    z->reset_until = CPC_RES_STEPS;
    memset(b->screen, 0, (size_t)CPC_SCREEN_W * CPC_SCREEN_H * sizeof(uint32_t));
}

static void cpc_joins(board_t *b) {
    cpc464_t *z = CPC(b);
    sim_t *s = &b->sim;
    for (int l = 0; l < 8; l++)
        for (int r = 0; r < 10; r++) {
            if (!((z->key_matrix[l] >> r) & 1)) continue;
            int rp = z->pin_row[r], cp = z->pin_col[l];
            if (rp < 0 || cp < 0 || s->pins[rp].fault == PF_OPEN || s->pins[cp].fault == PF_OPEN) continue;
            sim_join(s, s->pins[rp].net, s->pins[cp].net);
        }
}

static void cpc_sync_keys(board_t *b) {
    cpc464_t *z = CPC(b);
    bool changed = false;
    for (int l = 0; l < 8; l++) {
        uint16_t m = z->kbd.scanout_column_masks[l];
        if (m != z->key_matrix[l]) { z->key_matrix[l] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}
static void cpc_key_down(board_t *b, int key) { kbd_key_down(&CPC(b)->kbd, key); cpc_sync_keys(b); }
static void cpc_key_up(board_t *b, int key) { kbd_key_up(&CPC(b)->kbd, key); cpc_sync_keys(b); }
static void cpc_frame_update(board_t *b, uint32_t us) { kbd_update(&CPC(b)->kbd, us); cpc_sync_keys(b); }

static int cpc_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return 0x0D;
    case SAPP_KEYCODE_KP_ENTER: return 0x0E;
    case SAPP_KEYCODE_BACKSPACE: case SAPP_KEYCODE_DELETE: return 0x7F;
    case SAPP_KEYCODE_ESCAPE: return 0x1B;
    case SAPP_KEYCODE_TAB: return 0x09;
    case SAPP_KEYCODE_LEFT: return K_LEFT;
    case SAPP_KEYCODE_RIGHT: return K_RIGHT;
    case SAPP_KEYCODE_UP: return K_UP;
    case SAPP_KEYCODE_DOWN: return K_DOWN;
    case SAPP_KEYCODE_LEFT_SHIFT: case SAPP_KEYCODE_RIGHT_SHIFT: return K_SHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL: case SAPP_KEYCODE_RIGHT_CONTROL: return K_CTRL;
    case SAPP_KEYCODE_CAPS_LOCK: return K_CAPS;
    case SAPP_KEYCODE_HOME: return K_CLR;
    case SAPP_KEYCODE_END: return K_COPY;
    case SAPP_KEYCODE_F1: return K_F0 + 1; case SAPP_KEYCODE_F2: return K_F0 + 2; case SAPP_KEYCODE_F3: return K_F0 + 3;
    case SAPP_KEYCODE_F4: return K_F0 + 4; case SAPP_KEYCODE_F5: return K_F0 + 5; case SAPP_KEYCODE_F6: return K_F0 + 6;
    case SAPP_KEYCODE_F7: return K_F0 + 7; case SAPP_KEYCODE_F8: return K_F0 + 8; case SAPP_KEYCODE_F9: return K_F0 + 9;
    case SAPP_KEYCODE_F10: return K_F0;
    default: return 0;
    }
}
static int cpc_host_char(int c) { return c; }

static void cpc_monitor(board_t *b, uint32_t *out) {
    part_t *ga = &b->sim.parts[CPC(b)->p_ga];
    b->no_signal = ga->dead;
    for (int i = 0; i < CPC_SCREEN_W * CPC_SCREEN_H; i++) {
        uint32_t c = b->screen[i];
        if (ga->dead) {
            b->snow_seed = b->snow_seed * 1103515245u + 12345u;
            uint32_t g = ((b->snow_seed >> 24) & 0xFF) * 3 / 4;
            c = 0xFF000000u | (g << 16) | (g << 8) | g;
        }
        out[i] = c;
    }
}

/* ------------------------------------------------------------------------ */
/* Time                                                                      */
/* ------------------------------------------------------------------------ */

static void cpc_step(board_t *b) {
    cpc464_t *z = CPC(b);
    sim_t *s = &b->sim;
    uint8_t x = (uint8_t)(!z->xtal);
    z->xtal = x;
    sim_mark_dirty(s, z->p_osc);
    if (x) ga_tick(s, &s->parts[z->p_ga]);
    sim_settle(s);
    int clk = sim_read(s, z->pin_clk);
    if (clk != z->clk) {
        z->clk = clk;
        cz_edge(b, clk);
        sim_settle(s);
    }
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void cpc_run(board_t *b, int cycles) {
    cpc464_t *z = CPC(b);
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        sim_drive(s, z->pin_rc, s->cycle < z->reset_until ? DRV_LO : DRV_Z);
        for (int k = 0; k < CPC_STEPS_PER_CYCLE; k++) cpc_step(b);
    }
}

void cpc464_run_steps(board_t *b, int steps) {
    for (int i = 0; i < steps; i++) cpc_step(b);
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static void cpc_destroy(board_t *b) { (void)b; }

static const char *cpc_cpu_status(board_t *b) {
    const cpc464_t *z = CPC(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!z->rom_ok) return "no ROM (put cpc464.rom, 32768 bytes, in roms/cpc464)";
    return z->in_reset ? "in reset" : "running";
}

static void cpc_cpu_regs(board_t *b, char *buf, int len) {
    const z80_t *c = &CPC(b)->cpu;
    snprintf(buf, (size_t)len, "PC=%04X SP=%04X AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X",
             c->pc, c->sp, c->af, c->bc, c->de, c->hl, c->ix, c->iy);
}

static const file_kind_t *cpc_file_kind(const char *path, char *info, int len, bool *bad) {
    (void)path; (void)info; (void)len; (void)bad;
    return NULL;
}
static bool cpc_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)path; (void)run;
    snprintf(msg, (size_t)msglen, "the CPC 464 doesn't load programs: it is a board to troubleshoot");
    return false;
}

static void cpc_print_text(board_t *b) {
    (void)b;
    printf("(no text dump for the CPC 464: use --screenshot)\n");
}

const machine_t machine_cpc464 = {
    .id = "cpc464",
    .name = "Amstrad CPC 464",
    .issue = "464",
    .board_name = "CPC 464 Z70100 (netlist from the circuit diagram, checked by continuity)",
    .size = sizeof(cpc464_t),
    .data = &bd_cpc464,
    .sim = { .rail_of = cpc_rail_of, .kind_of = cpc_kind_of, .cycles_per_us = 16.0 },
    .cpu_hz = CPC_FREQ,
    .cycles_per_frame = CPC_CYCLES_PER_FRAME,
    .steps_per_cycle = CPC_STEPS_PER_CYCLE,
    .screen_w = CPC_SCREEN_W, .screen_h = CPC_SCREEN_H,
    .ready_cycles = 3 * (uint64_t)CPC_FREQ,
    .parts = cpc_part_defs, .nparts = sizeof(cpc_part_defs) / sizeof(cpc_part_defs[0]),
    .conns = cpc_conn_defs, .nconns = sizeof(cpc_conn_defs) / sizeof(cpc_conn_defs[0]),
    .link_resistors = "R146 R147 R148 R149 R150 R151 R152 R153 R138 R139 R141 R113",
    .crystals = "X101",
    .diodes = "", .jumpers = "", .supplies = "",
    .keys_help = "Keys: type; Left Shift = SHIFT, Ctrl = CONTROL, Esc = ESC, Backspace = DEL, Home = CLR, End = COPY, F1-F9, F10 = F0",
    .reset_help = "Hold to press RESET: pulls BUS RESET (IC110 pin 2, expansion pin 40) to GND",
    .create = cpc_create,
    .destroy = cpc_destroy,
    .power_on = cpc_power_on,
    .run = cpc_run,
    .joins = cpc_joins,
    .key_down = cpc_key_down,
    .key_up = cpc_key_up,
    .frame_update = cpc_frame_update,
    .host_key = cpc_host_key,
    .host_char = cpc_host_char,
    .monitor = cpc_monitor,
    .files_title = "Open a file",
    .file_kind = cpc_file_kind,
    .load = cpc_load,
    .cpu_status = cpc_cpu_status,
    .cpu_regs = cpc_cpu_regs,
    .print_text = cpc_print_text,
};
