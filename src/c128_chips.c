/* c128_chips.c - the chips of the Commodore 128 that no other machine has: the 8722 MMU and the 8721 PLA.
 *
 * The 8721 is the Verilog of tools/8721-pla.v (a read-out of the chip, converted by tools/gen_8721.py): its
 * outputs are active high in the file, and active low on the pins, except CASENB, CLRBNK and DIR.
 *
 * The 8722 sits between the processors and the memory: it relocates pages 0 and 1, adds the common RAM, picks
 * the RAM bank (/CAS0, /CAS1) and tells the PLA which ROM bank, or the I/O area, is wanted for the address
 * (MS0-MS3). In Z80 mode it shifts the Z80's address up by 4K, so that the BIOS ROM (the KERNAL's $D000-$DFFF)
 * appears at $0000 and the RAM follows it. Its registers are at $D500-$D50B (when the I/O area is on) and the
 * load configuration registers at $FF00-$FF04. */
#include "parts.h"
#include "chips_common.h"
#include <string.h>

#include "c128_pla.inc"
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------ */
/* 8721 PLA (U11)                                                            */
/* ------------------------------------------------------------------------ */

typedef struct { uint8_t dwe, casenb; } pla8721_t;

static void pla8721_run(sim_t *s, part_t *p, pla8721_out_t *o, int *dwe, int *casenb) {
    const pla8721_t *t = p->state;
    pla8721_in_t in;
    in.a15 = pin_in(s, p, 1); in.a14 = pin_in(s, p, 2); in.a13 = pin_in(s, p, 3);
    in.a12 = pin_in(s, p, 4); in.a11 = pin_in(s, p, 5); in.a10 = pin_in(s, p, 6);
    in.vicfix = pin_in(s, p, 7); in.dmaack = pin_in(s, p, 8); in.aec = pin_in(s, p, 9); in.rw = pin_in(s, p, 10);
    in.game = pin_in(s, p, 11); in.exrom = pin_in(s, p, 12); in.z80en = pin_in(s, p, 13); in.z80io = pin_in(s, p, 14);
    in.ms3 = pin_in(s, p, 15); in.ms2 = pin_in(s, p, 16); in.ms1 = pin_in(s, p, 17); in.ms0 = pin_in(s, p, 18);
    in.vma4 = pin_in(s, p, 19); in.vma5 = pin_in(s, p, 20); in.ba = pin_in(s, p, 21);
    in.loram = pin_in(s, p, 22); in.hiram = pin_in(s, p, 23); in.charen = pin_in(s, p, 25);
    in.va14 = pin_in(s, p, 26); in.rom_256 = pin_in(s, p, 27); in.clk = pin_in(s, p, 47);
    int dwe_d, casenb_d, latch;
    pla8721_calc(&in, o, &dwe_d, &casenb_d, &latch);
    *dwe = in.clk ? dwe_d : t->dwe;                  /* transparent latches */
    *casenb = latch ? casenb_d : t->casenb;
}

static void pla8721_eval(sim_t *s, part_t *p) {
    pla8721_out_t o;
    int dwe, casenb;
    pla8721_run(s, p, &o, &dwe, &casenb);
    pin_out(s, p, 28, DRV_BIT(!o.sden));
    pin_out(s, p, 29, DRV_BIT(o.dir));
    pin_out(s, p, 30, DRV_BIT(!o.roml));
    pin_out(s, p, 31, DRV_BIT(!o.romh));
    pin_out(s, p, 32, DRV_BIT(o.clrbnk));
    pin_out(s, p, 33, DRV_BIT(!o.from));
    pin_out(s, p, 34, DRV_BIT(!o.rom4));
    pin_out(s, p, 35, DRV_BIT(!o.rom3));
    pin_out(s, p, 36, DRV_BIT(!o.rom2));
    pin_out(s, p, 37, DRV_BIT(!o.rom1));
    pin_out(s, p, 38, DRV_BIT(!o.iocs));
    pin_out(s, p, 40, DRV_BIT(!dwe));
    pin_out(s, p, 41, DRV_BIT(casenb));
    pin_out(s, p, 42, DRV_BIT(!o.vic));
    pin_out(s, p, 43, DRV_BIT(!o.ioacc));
    pin_out(s, p, 44, DRV_BIT(!o.gwe));
    pin_out(s, p, 45, DRV_BIT(!o.colram));
    pin_out(s, p, 46, DRV_BIT(!o.charom));
}

static void pla8721_commit(sim_t *s, part_t *p) {
    pla8721_t *t = p->state;
    pla8721_out_t o;
    int dwe, casenb;
    pla8721_run(s, p, &o, &dwe, &casenb);
    if (dwe != t->dwe || casenb != t->casenb) {
        t->dwe = (uint8_t)dwe;
        t->casenb = (uint8_t)casenb;
        sim_mark_dirty(s, (int)(p - s->parts));
    }
}

static void pla8721_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(pla8721_t)); }

const part_type_t PT_8721 = { "8721 PLA",
    "A15:M A14:M A13:M A12:M A11:M A10:M VICFIX:M DMAACK:M AEC:M R/W:M GAME:M EXROM:M /Z80EN:M Z80IO:M "
    "MS3:M MS2:M MS1:M MS0:M MA4:M MA5:M BA:M LORAM:M HIRAM:M GND:P CHAREN:M VA14:M 128/256:M "
    "/SDEN:O DIR:O /ROML:O /ROMH:O CLRBNK:O /FROM1:O /ROM4:O /ROM3:O /ROM2:O /ROM1:O /IOCS:O NC:X /DWE:O CASENB:O "
    "/VIC:O /IOCC:O /GWE:O /COLORAM:O /CHAROM:O CLK:M VCC:P",
    pla8721_eval, pla8721_commit, pla8721_reset, sizeof(pla8721_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 74LS74 dual D flip-flop (U56): the stub in parts.c does nothing            */
/* ------------------------------------------------------------------------ */

typedef struct { uint8_t q[2], ck[2]; } ff74_t;
static const struct { int clr, d, ck, pr, q, qn; } ff74_pins[2] = { { 1, 2, 3, 4, 5, 6 }, { 13, 12, 11, 10, 9, 8 } };

static void ff74_apply(sim_t *s, part_t *p, int i, int *q, int *qn) {
    ff74_t *t = p->state;
    int clr = pin_in(s, p, ff74_pins[i].clr), pr = pin_in(s, p, ff74_pins[i].pr);
    *q = t->q[i];
    if (!clr && !pr) { *q = 1; *qn = 1; return; }
    if (!clr) *q = 0;
    else if (!pr) *q = 1;
    *qn = !*q;
}
static void ff74_eval(sim_t *s, part_t *p) {
    for (int i = 0; i < 2; i++) {
        int q, qn;
        ff74_apply(s, p, i, &q, &qn);
        pin_out(s, p, ff74_pins[i].q, DRV_BIT(q));
        pin_out(s, p, ff74_pins[i].qn, DRV_BIT(qn));
    }
}
static void ff74_commit(sim_t *s, part_t *p) {
    ff74_t *t = p->state;
    bool changed = false;
    for (int i = 0; i < 2; i++) {
        int q, qn;
        ff74_apply(s, p, i, &q, &qn);
        int ck = pin_in(s, p, ff74_pins[i].ck);
        if (ck && !t->ck[i] && pin_in(s, p, ff74_pins[i].clr) && pin_in(s, p, ff74_pins[i].pr))
            q = pin_in(s, p, ff74_pins[i].d);
        t->ck[i] = (uint8_t)ck;
        if (q != t->q[i]) { t->q[i] = (uint8_t)q; changed = true; }
    }
    if (changed) sim_mark_dirty(s, (int)(p - s->parts));
}
static void ff74_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ff74_t)); }
const part_type_t PT_74LS74_128 = { "74LS74",
    "/1CLR:m 1D:m 1CK:m /1PR:m 1Q:O /1Q:O GND:P /2Q:O 2Q:O /2PR:m 2CK:m 2D:m /2CLR:m VCC:P",
    ff74_eval, ff74_commit, ff74_reset, sizeof(ff74_t), TECH_TTL };

/* ------------------------------------------------------------------------ */
/* 8722 MMU (U7)                                                             */
/* ------------------------------------------------------------------------ */

typedef struct {
    uint8_t cr, pcr[4], mcr, rcr, p0l, p0h, p1l, p1h;
    uint8_t phi0, was_reset, pend_v;
    int16_t pend_reg;
    int32_t cache_d;
} mmu8722_t;

enum { MMU_RES = 2, MMU_AEC = 16, MMU_MUX = 17, MMU_A47 = 22, MMU_A67 = 23, MMU_RW = 32, MMU_PHI0 = 33,
       MMU_Z80EN = 43, MMU_FSDIR = 44, MMU_GAME = 45, MMU_EXROM = 46, MMU_MS3 = 47, MMU_4080 = 48 };
static const int mmu_a0[4] = { 18, 19, 20, 21 };
static const int mmu_hi[8] = { 24, 25, 26, 27, 28, 29, 30, 31 };      /* A8-A15 */
static const int mmu_d[8] = { 35, 36, 37, 38, 39, 40, 41, 42 };
static const int mmu_ta[8] = { 10, 9, 8, 7, 6, 5, 4, 3 };             /* TA8-TA15 */

/* What the MMU puts out for the address: the translated high byte, the RAM block, and the ROM bank bits. */
typedef struct { uint8_t ta, block, ms; bool io; } mmu_map_t;

static void mmu_map(const mmu8722_t *t, unsigned hi, bool aec, mmu_map_t *m) {
    bool mode64 = (t->mcr & 0x40) != 0, z80 = !(t->mcr & 1);
    unsigned addr = hi << 8;
    m->io = false;
    m->ms = 3;
    if (mode64) {                                   /* C64 mode: no banking, the PLA does the work */
        m->ta = (uint8_t)hi;
        m->block = 0;
        return;
    }
    if (z80) {                                      /* the Z80 has the start-up ROM at $0000 and the RAM where the 8502 sees it */
        m->ta = (uint8_t)(hi < 0x10 ? hi + 0x10 : hi);   /* the BIOS ROM is read 4K up, the RAM where it is */
        m->block = (t->cr >> 6) & 3;
        if (hi < 0x10) m->ms = 0;                   /* the start-up code stays visible while the Z80 runs */
    } else {
        if (hi == 0) { m->ta = t->p0l; m->block = t->p0h & 0x0F; }
        else if (hi == 1) { m->ta = t->p1l; m->block = t->p1h & 0x0F; }
        else {
            m->ta = (uint8_t)hi;
            m->block = (t->cr >> 6) & 3;
            static const unsigned sizes[4] = { 0x400, 0x1000, 0x2000, 0x4000 };
            unsigned size = sizes[t->rcr & 3];
            if (((t->rcr & 4) && addr < size) || ((t->rcr & 8) && addr >= 0x10000 - size)) m->block = 0;
            unsigned field;
            if (hi < 0x40) m->ms = 3;
            else if (hi < 0x80) m->ms = (t->cr & 2) ? 3 : 0;
            else if (hi < 0xC0) {
                field = (t->cr >> 2) & 3;
                m->ms = (uint8_t)(((field & 1) << 1) | ((field >> 1) & 1));
            } else {
                field = (t->cr >> 4) & 3;
                m->ms = (uint8_t)(((field & 1) << 1) | ((field >> 1) & 1));
            }
        }
        if (hi < 2) m->ms = 3;
    }
    if (!aec) m->block = (t->rcr >> 4) & 3;         /* the VIC's turn: its bank */
}

static unsigned mmu_hi_of(sim_t *s, part_t *p) {
    unsigned v = 0;
    for (int i = 0; i < 8; i++) v |= (unsigned)pin_in(s, p, mmu_hi[i]) << i;
    return v;
}

/* The register the address selects: 0-11 for $D500-$D50B, 0x100+n for the write only $FF00-$FF04, else -1. */
static int mmu_reg(sim_t *s, part_t *p, const mmu8722_t *t) {
    if (pin_in(s, p, MMU_A47) || pin_in(s, p, MMU_A67)) return -1;      /* A4-A7 must be 0 */
    unsigned lo = 0, hi = mmu_hi_of(s, p);
    for (int i = 0; i < 4; i++) lo |= (unsigned)pin_in(s, p, mmu_a0[i]) << i;
    if (hi == 0xD5 && lo <= 0x0B && (!(t->cr & 1) || !(t->mcr & 1))) return (int)lo;   /* the Z80 reaches them by I/O */
    if (hi == 0xFF && lo <= 4) return 0x100 + (int)lo;
    return -1;
}

static void mmu_eval(sim_t *s, part_t *p) {
    mmu8722_t *t = p->state;
    mmu_map_t m;
    bool aec = pin_in(s, p, MMU_AEC) != 0;
    mmu_map(t, mmu_hi_of(s, p), aec, &m);
    for (int i = 0; i < 8; i++) pin_out(s, p, mmu_ta[i], DRV_BIT((m.ta >> i) & 1));
    pin_out(s, p, 12, DRV_BIT(m.block != 0));
    pin_out(s, p, 11, DRV_BIT(m.block != 1));
    bool mode64 = (t->mcr & 0x40) != 0;
    pin_out(s, p, 15, DRV_BIT(m.ms & 1));
    pin_out(s, p, 14, DRV_BIT((m.ms >> 1) & 1));
    pin_out(s, p, 13, DRV_BIT(t->cr & 1));
    pin_out(s, p, MMU_MS3, DRV_BIT(!mode64));
    pin_out(s, p, MMU_Z80EN, DRV_BIT(t->mcr & 1));
    pin_out(s, p, MMU_FSDIR, DRV_BIT(!(t->mcr & 8)));
    pin_out(s, p, MMU_GAME, (t->mcr & 0x10) ? DRV_PULLUP : DRV_LO);
    pin_out(s, p, MMU_EXROM, (t->mcr & 0x20) ? DRV_PULLUP : DRV_LO);
    /* a read of a register */
    int reg = mmu_reg(s, p, t);
    bool rd = ((reg >= 0 && reg < 12) || reg == 0x100) && pin_in(s, p, MMU_RW) && aec;   /* $FF00 reads back the CR */
    unsigned v = 0xFF;
    if (rd) switch (reg) {
        case 0: case 0x100: v = t->cr; break;
        case 1: case 2: case 3: case 4: v = t->pcr[reg - 1]; break;
        case 5: v = (unsigned)((pin_in(s, p, MMU_4080) << 7) | ((t->mcr & 0x40)) | (pin_in(s, p, MMU_EXROM) << 5) |
                               (pin_in(s, p, MMU_GAME) << 4) | (t->mcr & 8) | 6 | (t->mcr & 1)); break;
        case 6: v = t->rcr; break;
        case 7: v = t->p0l; break;
        case 8: v = t->p0h; break;
        case 9: v = t->p1l; break;
        case 10: v = t->p1h; break;
        case 11: v = 0x20; break;                   /* two 64K banks, version 0 */
    }
    part_drive_bus_cached(s, p, mmu_d, 8, v, rd, &t->cache_d);
}

static void mmu_reset_regs(mmu8722_t *t) {
    memset(t, 0, sizeof(*t));
    t->mcr = 0x30;                                  /* the Z80 first, 128 mode, GAME and EXROM released */
    t->pend_reg = -1;
    t->p1l = 1;
}

static void mmu_commit(sim_t *s, part_t *p) {
    mmu8722_t *t = p->state;
    bool changed = false;
    if (!pin_in(s, p, MMU_RES)) {
        if (!t->was_reset) { int32_t c = t->cache_d; mmu_reset_regs(t); t->cache_d = c; t->was_reset = 1; changed = true; }
        t->phi0 = (uint8_t)pin_in(s, p, MMU_PHI0);
        if (changed) sim_mark_dirty(s, (int)(p - s->parts));
        return;
    }
    t->was_reset = 0;
    int phi0 = pin_in(s, p, MMU_PHI0);
    /* While phi0 is high, remember what a write to the MMU would be: when phi0 falls, R/W, AEC and the address
     * have already moved on, so a write is judged on what was there just before. */
    if (phi0) {
        bool wr = !pin_in(s, p, MMU_RW) && pin_in(s, p, MMU_AEC);
        t->pend_reg = wr ? (int16_t)mmu_reg(s, p, t) : -1;
        t->pend_v = (uint8_t)part_read_bus(s, p, mmu_d, 8);
    }
    if (!phi0 && t->phi0 && t->pend_reg >= 0) {                                    /* end of a write cycle */
        int reg = t->pend_reg;
        uint8_t v = t->pend_v;
        t->pend_reg = -1;
        if (reg >= 0x100) {
            if (reg == 0x100) t->cr = v; else t->cr = t->pcr[reg - 0x101];
            changed = true;
        } else {
            switch (reg) {
            case 0: t->cr = v; break;
            case 1: case 2: case 3: case 4: t->pcr[reg - 1] = v; break;
            case 5: t->mcr = v & 0x79; if (getenv("C128DBG")) fprintf(stderr,"MCR=%02X cyc=%u\n", v, (unsigned)s->cycle); break;
            case 6: t->rcr = v; break;
            case 7: t->p0l = v; break;
            case 8: t->p0h = v & 0x0F; break;
            case 9: t->p1l = v; break;
            case 10: t->p1h = v & 0x0F; break;
            default: break;
            }
            changed = true;
        }
    }
    t->phi0 = (uint8_t)phi0;
    if (changed) sim_mark_dirty(s, (int)(p - s->parts));
}

static void mmu_reset(sim_t *s, part_t *p) { (void)s; mmu_reset_regs(p->state); }

const part_type_t PT_8722 = { "8722 MMU",
    "VDD:P /RES:m TA15:O TA14:O TA13:O TA12:O TA11:O TA10:O TA9:O TA8:O /CAS1:O /CAS0:O MS2:O MS1:O MS0:O AEC:M MUX:M "
    "A0:M A1:M A2:M A3:M A5/A4:M A7/A6:M A8:M A9:M A10:M A11:M A12:M A13:M A14:M A15:M R/W:M PHI0:M VSS:P "
    "D0:B D1:B D2:B D3:B D4:B D5:B D6:B D7:B /Z80EN:O /FSDIR:O /GAME:B /EXROM:B MS3:O 40/80:m",
    mmu_eval, mmu_commit, mmu_reset, sizeof(mmu8722_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* Timing: the machine runs C128_STEPS_PER_CYCLE steps (half periods of the dot clock) per 1 MHz system cycle:  */
/* steps 0-7 are phi1 (the VIC's access), steps 8-15 phi2 (the processors'). The chips that act once per cycle   */
/* (CIAs, SID, the VIC's register access) do it at step C128_ACT_STEP of phi2, once the address, the chip       */
/* selects and the write data have settled, and put the data of a read on the bus for the processor to latch.   */
/* ------------------------------------------------------------------------ */
#define C128_ACT_STEP 5            /* commits after the rise of phi2 */

typedef struct { uint8_t prev, high; } phi_t;
/* Feed one commit's level of the clock; true on the commit where the chip acts. */
static bool phi_act(phi_t *f, int level) {
    if (level) f->high = (uint8_t)(f->high < 255 ? f->high + 1 : 255); else f->high = 0;
    f->prev = (uint8_t)level;
    return level && f->high == C128_ACT_STEP;
}

/* ------------------------------------------------------------------------ */
/* 7407: hex buffer with open collector outputs                              */
/* ------------------------------------------------------------------------ */
static void ls07_eval(sim_t *s, part_t *p) {
    static const int g[6][2] = { {1, 2}, {3, 4}, {5, 6}, {9, 8}, {11, 10}, {13, 12} };
    for (int i = 0; i < 6; i++) pin_out(s, p, g[i][1], pin_in(s, p, g[i][0]) ? DRV_Z : DRV_LO);
}
const part_type_t PT_7407 = { "7407",
    "1A:T 1Y:O 2A:T 2Y:O 3A:T 3Y:O GND:P 4Y:O 4A:T 5Y:O 5A:T 6Y:O 6A:T VCC:P", ls07_eval, 0, 0, 0, TECH_TTL_OC };

/* ------------------------------------------------------------------------ */
/* 23128 / 23256 ROM: /CS on pin 20, /OE on pin 22, A14 on pin 27 of the 32K part                         */
/* ------------------------------------------------------------------------ */
typedef struct { const uint8_t *data; int32_t out_cache; unsigned size; } rom128_t;
static const int rom128_a[14] = { 10, 9, 8, 7, 6, 5, 4, 3, 25, 24, 21, 23, 2, 26 };
static const int rom128_d[8] = { 11, 12, 13, 15, 16, 17, 18, 19 };
static void rom128_eval(sim_t *s, part_t *p) {
    rom128_t *st = p->state;
    bool sel = !pin_in(s, p, 20) && !pin_in(s, p, 22);
    unsigned a = sel ? part_read_bus(s, p, rom128_a, 14) : 0;
    if (sel && st->size > 16384 && pin_in(s, p, 27)) a |= 0x4000;
    part_drive_bus_cached(s, p, rom128_d, 8, st->data && a < (st->size ? st->size : 16384) ? st->data[a] : 0xFF, sel,
                          &st->out_cache);
}
const part_type_t PT_C128ROM = { "23128/23256 ROM",
    "NC:m A12:M A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O D7:O "
    "/CS:M A10:M /OE:M A11:M A9:M A8:M A13:M A14:M VCC:P", rom128_eval, 0, 0, sizeof(rom128_t), TECH_NMOS };
void c128_rom_set(part_t *p, const uint8_t *data, unsigned size) {
    rom128_t *st = p->state;
    st->data = data;
    st->size = size;
    st->out_cache = 0;
}

/* ------------------------------------------------------------------------ */
/* 8502 CPU (U6): a 6502 with the 6510's I/O port, P0-P6 on pins 30-24                                    */
/* ------------------------------------------------------------------------ */
#include <stdio.h>
#include "m6502.h"
static const int c502_a[16] = { 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23 };
static const int c502_d[8] = { 38, 37, 36, 35, 34, 33, 32, 31 };
static const int c502_port[7] = { 30, 29, 28, 27, 26, 25, 24 };
typedef struct {
    m6502_t core;
    uint64_t pins;
    sim_t *s;
    part_t *part;
    uint8_t phi0, in_reset, latched, aec_seen;
    uint32_t cycles;
    int32_t cache_a, cache_d, cache_p;
} c502_t;

static uint8_t c502_port_in(void *user) {
    c502_t *c = user;
    uint8_t v = 0;
    for (int i = 0; i < 7; i++) v |= (uint8_t)(pin_in(c->s, c->part, c502_port[i]) << i);
    return v;
}
static void c502_port_out(uint8_t data, void *user) { (void)data; (void)user; }

static void c502_eval(sim_t *s, part_t *p) {
    c502_t *c = p->state;
    int32_t pk = (c->core.io_ddr << 8) | c->core.io_out;
    if (c->cache_p != pk + 1) {
        c->cache_p = pk + 1;
        for (int i = 0; i < 7; i++)
            pin_out(s, p, c502_port[i], (c->core.io_ddr >> i) & 1 ? DRV_BIT((c->core.io_out >> i) & 1) : DRV_Z);
    }
    bool bus = pin_in(s, p, 5) != 0;                       /* AEC */
    uint16_t addr = M6502_GET_ADDR(c->pins);
    bool write = !(c->pins & M6502_RW);
    part_drive_bus_cached(s, p, c502_a, 16, addr, bus, &c->cache_a);
    pin_out(s, p, 39, bus ? DRV_BIT(!write) : DRV_Z);
    bool phi0 = pin_in(s, p, 1) != 0;
    part_drive_bus_cached(s, p, c502_d, 8, M6502_GET_DATA(c->pins), bus && write && phi0 && addr > 1, &c->cache_d);
}

static void c502_commit(sim_t *s, part_t *p) {
    c502_t *c = p->state;
    if (p->dead) return;
    int phi0 = pin_in(s, p, 1);
    c->s = s;
    c->part = p;
    c->core.user_data = c;
    if (phi0) { c->latched = (uint8_t)part_read_bus(s, p, c502_d, 8); c->aec_seen = (uint8_t)pin_in(s, p, 5); }
    if (!phi0 && c->phi0) {                       /* phi0 falls: one cycle of the core */
        if (!pin_in(s, p, 40)) {
            c->in_reset = 1;
        } else {
            if (c->in_reset) {
                c->in_reset = 0;
                c->pins |= M6502_RES;
                c->core.io_ddr = 0;
                c->core.io_out = 0;
            }
            uint64_t pins = c->pins & ~(M6502_IRQ | M6502_NMI | M6502_RDY);
            if (!pin_in(s, p, 3)) pins |= M6502_IRQ;
            if (!pin_in(s, p, 4)) pins |= M6502_NMI;
            if (!pin_in(s, p, 2) || !c->aec_seen) pins |= M6502_RDY;   /* off the bus (AEC low): a read waits */
            if (pins & M6502_RW) M6502_SET_DATA(pins, c->latched);
            if (M6510_CHECK_IO(pins)) pins = m6510_iorq(&c->core, pins);
            c->pins = m6502_tick(&c->core, pins);
            c->cycles++;
            sim_mark_dirty(s, (int)(p - s->parts));
        }
    }
    c->phi0 = (uint8_t)phi0;
}

static void c502_reset(sim_t *s, part_t *p) {
    (void)s;
    c502_t *c = p->state;
    memset(c, 0, sizeof(*c));
    c->pins = m6502_init(&c->core, &(m6502_desc_t){
        .m6510_in_cb = c502_port_in, .m6510_out_cb = c502_port_out, .m6510_user_data = c,
        .m6510_io_pullup = 0, .m6510_io_floating = 0x80 });
    c->in_reset = 1;
}

const part_type_t PT_8502 = { "8502",
    "PHI0:M RDY:m /IRQ:m /NMI:m AEC:M VCC:P A0:O A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O A10:O A11:O A12:O A13:O "
    "GND:P A14:O A15:O P6:B P5:B P4:B P3:B P2:B P1:B P0:B D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B R/W:O /RES:m",
    c502_eval, c502_commit, c502_reset, sizeof(c502_t), TECH_NMOS };
uint32_t c128_cpu_cycles_of(const part_t *p) { return ((const c502_t *)p->state)->cycles; }
int c128_cpu_sync_of(const part_t *p, unsigned *addr) { const c502_t *c = p->state; *addr = M6502_GET_ADDR(c->pins) | ((unsigned)M6502_GET_DATA(c->pins) << 16); return (c->pins & M6502_SYNC) != 0; }
void c128_cpu_regs_of(const part_t *p, char *buf, int len) {
    const c502_t *c = p->state;
    snprintf(buf, (size_t)len, "PC=%04X A=%02X X=%02X Y=%02X S=%02X P=%02X", c->core.PC, c->core.A, c->core.X,
             c->core.Y, c->core.S, c->core.P);
}

/* ------------------------------------------------------------------------ */
/* Z80A (U10): the core runs on every edge of CLK (pin 6)                                                  */
/* ------------------------------------------------------------------------ */
#include "z80.h"
static const int z80_a[16] = { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5 };
static const int z80_d[8] = { 14, 15, 12, 8, 7, 9, 10, 13 };
typedef struct {
    z80_t cpu;
    uint64_t pins;
    uint8_t clk, started;
    uint32_t edges;
    int32_t cache_a, cache_d;
} z80p_t;

static void z80p_eval(sim_t *s, part_t *p) {
    z80p_t *z = p->state;
    bool aoe = (z->pins & Z80_AOE) != 0, doe = (z->pins & Z80_DOE) != 0;
    part_drive_bus_cached(s, p, z80_a, 16, Z80_GET_ADDR(z->pins), aoe, &z->cache_a);
    part_drive_bus_cached(s, p, z80_d, 8, Z80_GET_DATA(z->pins), doe, &z->cache_d);
    pin_out(s, p, 19, aoe ? DRV_BIT(!(z->pins & Z80_MREQ)) : DRV_Z);
    pin_out(s, p, 20, aoe ? DRV_BIT(!(z->pins & Z80_IORQ)) : DRV_Z);
    pin_out(s, p, 21, aoe ? DRV_BIT(!(z->pins & Z80_RD)) : DRV_Z);
    pin_out(s, p, 22, aoe ? DRV_BIT(!(z->pins & Z80_WR)) : DRV_Z);
    pin_out(s, p, 27, DRV_BIT(!(z->pins & Z80_M1)));
    pin_out(s, p, 28, DRV_BIT(!(z->pins & Z80_RFSH)));
    pin_out(s, p, 18, DRV_BIT(!(z->pins & Z80_HALT)));
    pin_out(s, p, 23, DRV_BIT(!(z->pins & Z80_BUSAK)));
}

static void z80p_commit(sim_t *s, part_t *p) {
    z80p_t *z = p->state;
    if (p->dead) return;
    int clk = pin_in(s, p, 6);
    if (clk == z->clk) return;
    z->clk = (uint8_t)clk;
    uint64_t pins = z->pins & Z80_OUTPUT_PIN_MASK;
    if (!(z->pins & Z80_DOE)) {
        uint8_t d = (uint8_t)part_read_bus(s, p, z80_d, 8);
        Z80_SET_DATA(pins, d);
    }
    if (!pin_in(s, p, 16)) pins |= Z80_INT;
    if (!pin_in(s, p, 17)) pins |= Z80_NMI;
    if (!pin_in(s, p, 24)) pins |= Z80_WAIT;
    if (!pin_in(s, p, 25)) pins |= Z80_BUSRQ;
    if (!pin_in(s, p, 26)) pins |= Z80_RES;
    if (clk) pins |= Z80_CLK;
    z->pins = z80_tick(&z->cpu, pins);
    z->edges++;
    sim_mark_dirty(s, (int)(p - s->parts));
}

static void z80p_reset(sim_t *s, part_t *p) {
    (void)s;
    z80p_t *z = p->state;
    memset(z, 0, sizeof(*z));
    z->pins = z80_init(&z->cpu);
}

const part_type_t PT_Z80A = { "Z80A",
    "A11:O A12:O A13:O A14:O A15:O CLK:m D4:B D3:B VCC:P D5:B D6:B D2:B D7:B D0:B D1:B /INT:m /NMI:m /HALT:O /MREQ:O "
    "/IORQ:O /RD:O /WR:O /BUSAK:O /WAIT:m /BUSRQ:m /RESET:m /M1:O /RFSH:O GND:P A0:O A1:O A2:O A3:O A4:O A5:O A6:O "
    "A7:O A8:O A9:O A10:O",
    z80p_eval, z80p_commit, z80p_reset, sizeof(z80p_t), TECH_NMOS };
void c128_z80_regs_of(const part_t *p, char *buf, int len) {
    const z80p_t *z = p->state;
    snprintf(buf, (size_t)len, "Z80 PC=%04X AF=%04X BC=%04X DE=%04X HL=%04X SP=%04X", z->cpu.pc, z->cpu.af, z->cpu.bc,
             z->cpu.de, z->cpu.hl, z->cpu.sp);
}

/* ------------------------------------------------------------------------ */
/* 6526 CIA (U1, U4)                                                         */
/* ------------------------------------------------------------------------ */
#include "m6526.h"
static const int cia_d[8] = { 33, 32, 31, 30, 29, 28, 27, 26 };
static const int cia_rs[4] = { 38, 37, 36, 35 };
typedef struct {
    m6526_t core;
    phi_t phi;
    uint8_t irq, outs, dout_en, dout;
    int32_t cache_pa, cache_pb, cache_d;
} cia128_t;

static void cia128_eval(sim_t *s, part_t *p) {
    cia128_t *t = p->state;
    const m6526_t *c = &t->core;
    uint8_t lows[2] = { (uint8_t)(c->pa.ddr & ~c->pa.reg), (uint8_t)(c->pb.ddr & ~c->pb.reg) };
    int32_t *cache[2] = { &t->cache_pa, &t->cache_pb };
    for (int port = 0; port < 2; port++) {
        if (*cache[port] == lows[port] + 1) continue;
        *cache[port] = lows[port] + 1;
        for (int bit = 0; bit < 8; bit++)
            pin_out(s, p, 2 + port * 8 + bit, (lows[port] >> bit) & 1 ? DRV_LO : DRV_PULLUP);
    }
    pin_out(s, p, 21, t->irq ? DRV_LO : DRV_Z);
    pin_out(s, p, 18, c->pc_low ? DRV_LO : DRV_HI);
    bool sp_out = (c->ta.cr & (1 << 6)) != 0;
    pin_out(s, p, 39, sp_out ? DRV_BIT(c->sp.sp_out) : DRV_Z);
    pin_out(s, p, 40, sp_out ? DRV_BIT(c->sp.cnt_out) : DRV_Z);
    part_drive_bus_cached(s, p, cia_d, 8, t->dout, t->dout_en, &t->cache_d);
}

static void cia128_commit(sim_t *s, part_t *p) {
    cia128_t *t = p->state;
    if (p->dead) return;
    int level = pin_in(s, p, 25);
    bool was_en = t->dout_en;
    if (!level && t->dout_en) { t->dout_en = 0; sim_mark_dirty(s, (int)(p - s->parts)); }
    (void)was_en;
    if (!phi_act(&t->phi, level)) return;
    m6526_t *c = &t->core;
    if (!pin_in(s, p, 34)) {
        m6526_init(c);
        t->irq = 0;
        sim_mark_dirty(s, (int)(p - s->parts));
        return;
    }
    uint64_t pins = 0;
    if (!pin_in(s, p, 23)) {
        pins = M6526_CS | part_read_bus(s, p, cia_rs, 4);
        if (pin_in(s, p, 22)) pins |= M6526_RW;
        M6526_SET_DATA(pins, part_read_bus(s, p, cia_d, 8));
        uint8_t pa = 0, pb = 0;
        for (int bit = 0; bit < 8; bit++) {
            pa |= (uint8_t)(pin_in(s, p, 2 + bit) << bit);
            pb |= (uint8_t)(pin_in(s, p, 10 + bit) << bit);
        }
        M6526_SET_PAB(pins, pa, pb);
    } else {
        M6526_SET_PAB(pins, c->pa.inp, c->pb.inp);
    }
    if (!pin_in(s, p, 24)) pins |= M6526_FLAG;
    if (pin_in(s, p, 19)) pins |= M6526_TOD;
    if (pin_in(s, p, 39)) pins |= M6526_SP;
    if (pin_in(s, p, 40)) pins |= M6526_CNT;
    pins = m6526_tick(c, pins);
    t->irq = (pins & M6526_IRQ) != 0;
    if ((pins & (M6526_CS | M6526_RW)) == (M6526_CS | M6526_RW)) {
        t->dout = M6526_GET_DATA(pins);
        t->dout_en = 1;
    }
    sim_mark_dirty(s, (int)(p - s->parts));
}

static void cia128_reset(sim_t *s, part_t *p) {
    (void)s;
    cia128_t *t = p->state;
    memset(t, 0, sizeof(*t));
    m6526_init(&t->core);
}

const part_type_t PT_6526_128 = { "6526 CIA",
    "GND:P PA0:B PA1:B PA2:B PA3:B PA4:B PA5:B PA6:B PA7:B PB0:B PB1:B PB2:B PB3:B "
    "PB4:B PB5:B PB6:B PB7:B /PC:O TOD:m VCC:P /IRQ:O R/W:m /CS:m /FLAG:m PHI2:m "
    "D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B /RES:m RS3:m RS2:m RS1:m RS0:m SP:O CNT:O",
    cia128_eval, cia128_commit, cia128_reset, sizeof(cia128_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 6581 SID (U5)                                                             */
/* ------------------------------------------------------------------------ */
#include "m6581.h"
static const int sid_a[5] = { 9, 10, 11, 12, 13 };
static const int sid_d[8] = { 15, 16, 17, 18, 19, 20, 21, 22 };
typedef struct {
    m6581_t sid;
    phi_t phi;
    uint8_t dout_en, dout;
    int32_t cache_d;
} sid128_t;

static void sid128_eval(sim_t *s, part_t *p) {
    sid128_t *t = p->state;
    part_drive_bus_cached(s, p, sid_d, 8, t->dout, t->dout_en, &t->cache_d);
}
static void sid128_commit(sim_t *s, part_t *p) {
    sid128_t *t = p->state;
    if (p->dead) return;
    int level = pin_in(s, p, 6);
    if (!level && t->dout_en) { t->dout_en = 0; sim_mark_dirty(s, (int)(p - s->parts)); }
    if (!phi_act(&t->phi, level)) return;
    if (!pin_in(s, p, 5)) { m6581_reset(&t->sid); return; }
    uint64_t pins = 0;
    if (!pin_in(s, p, 8)) {
        pins = M6581_CS | part_read_bus(s, p, sid_a, 5);
        if (pin_in(s, p, 7)) pins |= M6581_RW;
        M6502_SET_DATA(pins, part_read_bus(s, p, sid_d, 8));
    }
    if ((pins & (M6581_CS | M6581_RW)) == (M6581_CS | M6581_RW)) {
        uint8_t reg = pins & M6581_ADDR_MASK;
        if (reg == M6581_POT_X || reg == M6581_POT_Y) t->sid.pot[reg == M6581_POT_Y] = 0xFF;
    }
    pins = m6581_tick(&t->sid, pins);
    if ((pins & (M6581_CS | M6581_RW)) == (M6581_CS | M6581_RW)) {
        t->dout = M6502_GET_DATA(pins);
        t->dout_en = 1;
        sim_mark_dirty(s, (int)(p - s->parts));
    }
}
static void sid128_reset(sim_t *s, part_t *p) {
    (void)s;
    sid128_t *t = p->state;
    memset(t, 0, sizeof(*t));
    m6581_init(&t->sid, &(m6581_desc_t){ .tick_hz = 1022727, .sound_hz = 44100, .magnitude = 1.0f });
}
const part_type_t PT_6581_128 = { "6581 SID",
    "CAP1A:O CAP1B:O CAP2A:O CAP2B:O /RES:m PHI2:m R/W:m /CS:m A0:m A1:m A2:m A3:m A4:m "
    "GND:P D0:B D1:B D2:B D3:B D4:B D5:B D6:B D7:B POTY:m POTX:m VCC:P EXTIN:m AUDIO:O VDD:P",
    sid128_eval, sid128_commit, sid128_reset, sizeof(sid128_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 8563 VDC (U22): the registers and the video RAM; c128_vdc_render() draws its RGBI picture from them  */
/* ------------------------------------------------------------------------ */
static const int vdc_d[8] = { 18, 17, 16, 15, 14, 13, 11, 10 };
typedef struct {
    uint8_t reg[38], idx, last_wr, dout_en, dout;
    uint8_t ram[65536];
    phi_t phi;
    uint32_t frame;
    int32_t cache_d;
} vdc_t;

static uint16_t vdc_addr(const vdc_t *v) { return (uint16_t)(v->reg[18] << 8 | v->reg[19]); }
static void vdc_set_addr(vdc_t *v, uint16_t a) { v->reg[18] = (uint8_t)(a >> 8); v->reg[19] = (uint8_t)a; }

static uint8_t vdc_read(vdc_t *v) {
    if (v->idx == 31) {                                 /* the RAM data port */
        uint16_t a = vdc_addr(v);
        uint8_t r = v->ram[a];
        vdc_set_addr(v, (uint16_t)(a + 1));
        return r;
    }
    if (v->idx == 0x22) return 0;
    return v->idx < 38 ? v->reg[v->idx] : 0xFF;
}

static void vdc_write(vdc_t *v, uint8_t d) {
    if (v->idx >= 38) return;
    if (v->idx == 31) {
        uint16_t a = vdc_addr(v);
        v->ram[a] = d;
        v->last_wr = d;
        vdc_set_addr(v, (uint16_t)(a + 1));
        return;
    }
    v->reg[v->idx] = d;
    if (v->idx == 30) {                                 /* block copy or fill of d + 1 bytes */
        uint16_t dst = vdc_addr(v), src = (uint16_t)(v->reg[32] << 8 | v->reg[33]);
        bool copy = (v->reg[24] & 0x80) != 0;
        for (int i = 0; i <= d; i++) {
            v->ram[(uint16_t)(dst + i)] = copy ? v->ram[(uint16_t)(src + i)] : v->last_wr;
        }
        vdc_set_addr(v, (uint16_t)(dst + d + 1));
        if (copy) { v->reg[32] = (uint8_t)((src + d + 1) >> 8); v->reg[33] = (uint8_t)(src + d + 1); }
    }
}

static void vdc_eval(sim_t *s, part_t *p) {
    vdc_t *v = p->state;
    part_drive_bus_cached(s, p, vdc_d, 8, v->dout, v->dout_en, &v->cache_d);
}
static void vdc_commit(sim_t *s, part_t *p) {
    vdc_t *v = p->state;
    if (p->dead) return;
    int level = pin_in(s, p, 4);
    if (!level && v->dout_en) { v->dout_en = 0; sim_mark_dirty(s, (int)(p - s->parts)); }
    if (!phi_act(&v->phi, level)) return;
    if (pin_in(s, p, 7)) return;                         /* /CS */
    bool a0 = pin_in(s, p, 8) != 0;                      /* /RS: the register select (A0) */
    if (pin_in(s, p, 9)) {                               /* read */
        v->dout = a0 ? vdc_read(v) : (uint8_t)(0x80 | ((s->cycle % (63 * 312) >= 63 * 262) ? 0x20 : 0));   /* ready, vertical blank */
        v->dout_en = 1;
        sim_mark_dirty(s, (int)(p - s->parts));
    } else {
        uint8_t d = (uint8_t)part_read_bus(s, p, vdc_d, 8);
        if (a0) vdc_write(v, d); else v->idx = d & 0x3F;
    }
}

/* The picture of the VDC as a monitor on its RGBI output shows it: drawn from the registers and the video RAM when
 * asked (a frame is 640 x 200 at most, every line twice, in a W x H picture with the border around it).
 * Not done: the smooth scrolling (R24, R25), the interlace modes and the light pen. */
static const uint8_t vdc_rgbi[16][3] = {
    { 0, 0, 0 }, { 85, 85, 85 }, { 0, 0, 170 }, { 85, 85, 255 }, { 0, 170, 0 }, { 85, 255, 85 }, { 0, 170, 170 }, { 85, 255, 255 },
    { 170, 0, 0 }, { 255, 85, 85 }, { 170, 0, 170 }, { 255, 85, 255 }, { 170, 85, 0 }, { 255, 255, 85 }, { 170, 170, 170 }, { 255, 255, 255 } };
static uint32_t vdc_rgb(int c) { return 0xFF000000u | (uint32_t)vdc_rgbi[c & 15][2] << 16 | (uint32_t)vdc_rgbi[c & 15][1] << 8 | vdc_rgbi[c & 15][0]; }

void c128_vdc_render(const sim_t *s, const part_t *p, uint32_t *out, int W, int H) {
    const vdc_t *v = p->state;
    const uint8_t *r = v->reg;
    uint32_t border = vdc_rgb(r[26] & 15);
    for (int i = 0; i < W * H; i++) out[i] = border;
    int cols = r[1], rows = r[6], pitch = (r[22] >> 4) + 1, cw = r[22] & 15, lines = (r[9] & 31) + 1, shown = r[23] & 31;
    if (!cols || !rows || p->dead) return;
    if (cw < 1) cw = 1;
    if (cw > pitch) cw = pitch;
    bool dbl = (r[25] & 0x10) != 0, bitmap = (r[25] & 0x80) != 0, attrs = (r[25] & 0x40) != 0, rev = (r[24] & 0x40) != 0;
    int aw = cols * pitch * (dbl ? 2 : 1), ah = rows * lines;
    if (aw > W) aw = W;
    if (ah > H / 2) ah = H / 2;
    int x0 = (W - aw) / 2, y0 = (H - 2 * ah) / 2;
    uint16_t base = (uint16_t)(r[12] << 8 | r[13]), abase = (uint16_t)(r[20] << 8 | r[21]), cur = (uint16_t)(r[14] << 8 | r[15]);
    uint16_t font = (uint16_t)((r[28] & 0xE0) << 8);
    int inc = r[27], cmode = (r[10] >> 5) & 3, cs = r[10] & 31, ce = r[11] & 31;
    uint64_t frame = s->cycle / (63 * 312);
    bool cur_on = cmode == 0 || (cmode == 2 && (frame & 8)) || (cmode == 3 && (frame & 16));
    bool blink = (frame & 16) != 0;
    int bg0 = r[26] & 15, fg0 = r[26] >> 4;
    for (int y = 0; y < ah; y++) {
        int row = y / lines, l = y % lines;
        uint32_t *dst = out + (y0 + 2 * y) * W + x0;
        for (int c = 0; c < cols; c++) {
            uint8_t bits;
            int fg = fg0, bg = bg0;
            if (bitmap) {
                bits = v->ram[(uint16_t)(base + y * cols + row * inc + c)];
            } else {
                uint16_t cell = (uint16_t)(row * (cols + inc) + c);
                uint8_t ch = v->ram[(uint16_t)(base + cell)], at = 0;
                if (attrs) { at = v->ram[(uint16_t)(abase + cell)]; fg = at & 15; }
                uint16_t def = (uint16_t)(font + ((at & 0x80) ? 0x1000 : 0) + ch * 16 + l);
                bits = l < shown ? v->ram[def] : 0;
                if (attrs && (at & 0x20) && l == (r[29] & 31)) bits = 0xFF;
                bool inv = (attrs && (at & 0x40)) != rev;
                if (attrs && (at & 0x10) && blink) bits = 0;
                if (cur_on && (uint16_t)(base + cell) == cur && l >= cs && l <= ce) inv = !inv;
                if (inv) { int t = fg; fg = bg; bg = t; }
            }
            if (bitmap && rev) { int t = fg; fg = bg; bg = t; }
            for (int x = 0; x < pitch; x++) {
                uint32_t col = vdc_rgb((x < cw && x < 8 && (bits & (0x80 >> x))) ? fg : bg);
                if (dbl) { int px = (c * pitch + x) * 2; if (px + 1 < aw) dst[px] = dst[px + 1] = col; }
                else { int px = c * pitch + x; if (px < aw) dst[px] = col; }
            }
        }
        memcpy(dst + W, dst, (size_t)aw * sizeof(uint32_t));
    }
}
static void vdc_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(vdc_t)); }
const part_type_t PT_8563 = { "8563 VDC",
    "CCLK:m DCLK:m HSYN:O CS:m NC:O NC:O /CS:m /RS:m R/W:m D7:B D6:B VSS:P D5:B D4:B D3:B D2:B D1:B D0:B DISPEN:O "
    "VSYN:O DR/W:O TST:m /RES:m TST2:m LPEN:m DA0:O DA1:O DA2:O DA3:O DA4:O DA5:O DA6:O DA7:O DD0:B DD1:B DD2:B VCC:P "
    "DD3:B DD4:B DD5:B DD6:B DD7:B I:O B:O G:O R:O /RAS:O /CAS:O",
    vdc_eval, vdc_commit, vdc_reset, sizeof(vdc_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 8564 VIC-IIe (U21)                                                        */
/*                                                                           */
/* The 6569 core with the 8564's pins. It makes the system's timing out of the dot clock on pin 30: a 1 MHz   */
/* cycle is 16 half periods ("steps"), steps 0-7 are phi1 (the VIC's access to the DRAM), steps 8-15 phi2 (the */
/* processors'). A DRAM access takes 8 steps: MUX is high (the row on the address lines), /RAS falls, MUX     */
/* falls (the column), /CAS falls, the data is read, /RAS and /CAS rise. The Z80 gets a clock of 2 MHz.        */
/*                                                                           */
/* The core wants its memory data while it ticks, so the cycle is run twice: first on a copy of the state, to */
/* learn the addresses (and BA); then for real at the end of the cycle, with the data the DRAM really gave.   */
/* ------------------------------------------------------------------------ */
#include "m6569.h"
static const int v_sd[8] = { 7, 6, 5, 4, 3, 2, 1, 47 };             /* D0-D7 */
static const int v_col[4] = { 46, 45, 44, 43 };                     /* D8-D11 */
static const int v_ma[6] = { 32, 33, 34, 35, 36, 37 };              /* the multiplexed address, MA0-MA5 */
static const int v_ta[4] = { 40, 41, 42, 31 };                      /* A8-A11 */

typedef struct {
    m6569_t vic;
    uint32_t *fb;
    int fb_w, fb_h;
    uint8_t dot, k, started;
    uint8_t reg2f, reg30, vic_reg;
    uint8_t n, got;                          /* fetches of this cycle, and how many were already read */
    uint16_t faddr[3], fdata[3];
    uint8_t ba_low, irq, refresh;
    uint8_t dout_en, dout;
    uint32_t cycles, frame_cycles;
    int32_t cache_sd, cache_ma, cache_ta, cache_va;
} vic8564_t;

static void vic_nopix(void *fb, int x, int y, uint32_t c) { (void)fb; (void)x; (void)y; (void)c; }
static void vic_setpix(void *fbp, int x, int y, uint32_t c) {
    vic8564_t *v = fbp;
    if (!v->fb || x < 0 || y < 0 || x >= v->fb_w || y >= v->fb_h) return;
    v->fb[y * v->fb_w + x] = c;
}

/* The fetch callback of a dry run: it records the address and hands back the data already known. */
static uint16_t vic_dry_cb(uint16_t addr, void *user) {
    vic8564_t *v = user;
    int i = v->got;
    if (i < 3) v->faddr[i] = addr & 0x3FFF;
    v->got++;
    return i < v->n ? v->fdata[i] : 0;
}
static uint16_t vic_real_cb(uint16_t addr, void *user) {
    vic8564_t *v = user;
    (void)addr;
    int i = v->got++;
    return i < 3 ? v->fdata[i] : 0;
}

/* Run the cycle's fetch part on a copy: fills faddr/n (the first `known` data are in fdata). */
static uint64_t vic_dry(vic8564_t *v, int known) {
    m6569_t copy = v->vic;
    copy.mem.fetch_cb = vic_dry_cb;
    copy.mem.user_data = v;
    copy.crt_set_pixel = vic_nopix;
    uint8_t keep = v->n;
    v->n = (uint8_t)known;
    v->got = 0;
    uint64_t pins = m6569_tick_fetch(&copy, 0);
    uint8_t found = v->got;
    v->n = found;
    (void)keep;
    return pins;
}

static void vic8564_eval(sim_t *s, part_t *p) {
    vic8564_t *v = p->state;
    bool run = v->started && !p->dead;
    unsigned k = v->k, j = k & 7;
    bool phi2 = k >= 8;
    bool own = run && (!phi2 || v->n >= 2);
    pin_out(s, p, 23, run ? DRV_BIT(phi2) : DRV_Z);                             /* 2 MHz: the 8502's phi0 */
    pin_out(s, p, 18, run ? DRV_BIT(phi2) : DRV_Z);                             /* 1 MHz */
    pin_out(s, p, 25, run ? DRV_BIT(((k + 4) & 15) < 8) : DRV_Z);                            /* the Z80's clock */
    pin_out(s, p, 21, run ? DRV_BIT(j < 2 || j == 7) : DRV_Z);                  /* MUX */
    pin_out(s, p, 19, run ? DRV_BIT(!(j >= 1 && j <= 6)) : DRV_Z);              /* /RAS */
    pin_out(s, p, 20, run ? DRV_BIT(!(j >= 3 && j <= 6)) : DRV_Z);              /* /CAS */
    pin_out(s, p, 12, run ? DRV_BIT(!own) : DRV_Z);                             /* AEC */
    pin_out(s, p, 10, run ? DRV_BIT(!v->ba_low) : DRV_Z);                       /* BA */
    pin_out(s, p, 8, run && v->irq ? DRV_LO : DRV_Z);                           /* /IRQ */
    for (int i = 0; i < 3; i++) pin_out(s, p, 26 + i, (v->reg2f >> i) & 1 ? DRV_Z : DRV_LO);   /* K0-K2 */
    uint16_t a = 0x3F00 | v->refresh;
    if (!phi2) a = v->n >= 1 ? v->faddr[0] : a; else if (v->n >= 2) a = v->faddr[1];
    bool col = j >= 2 && j <= 6;
    unsigned mux = col ? (a >> 8) & 63 : a & 63;
    part_drive_bus_cached(s, p, v_ma, 6, mux, own, &v->cache_ma);
    static const int a67[2] = { 38, 39 };
    part_drive_bus_cached(s, p, a67, 2, (a >> 6) & 3, own, &v->cache_va);
    part_drive_bus_cached(s, p, v_ta, 4, (a >> 8) & 15, own, &v->cache_ta);
    part_drive_bus_cached(s, p, v_sd, 8, v->dout, v->dout_en, &v->cache_sd);
}

static void vic8564_commit(sim_t *s, part_t *p) {
    vic8564_t *v = p->state;
    if (p->dead) return;
    int dot = pin_in(s, p, 30);
    if (dot == v->dot && v->started) return;
    v->dot = (uint8_t)dot;
    if (!v->started) { v->started = 1; v->k = 15; }
    unsigned k = (v->k + 1) & 15;
    v->k = (uint8_t)k;
    if (k == 0) {                                          /* a new cycle: what will the core do? */
        v->dout_en = 0;
        v->n = 0;
        uint64_t pins = vic_dry(v, 0);
        v->ba_low = (pins & M6569_BA) != 0;
        v->irq = (pins & M6569_IRQ) != 0;
        if (v->n == 0) v->refresh--;
    } else if (k == 6 && v->n >= 1) {                      /* the first access's data is on the bus */
        v->fdata[0] = (uint16_t)(part_read_bus(s, p, v_sd, 8) | part_read_bus(s, p, v_col, 4) << 8);
        if (v->n >= 2) { uint8_t n = v->n; v->n = 1; vic_dry(v, 1); v->n = n >= 2 ? n : v->n; }   /* the second address */
    } else if (k == 9) {
        v->vic_reg = (uint8_t)part_read_bus(s, p, v_ma, 6);   /* the register address the CPU's cycle put on MA0-MA5 */
    }
    if (k == 14 && v->n >= 2)
        v->fdata[1] = (uint16_t)(part_read_bus(s, p, v_sd, 8) | part_read_bus(s, p, v_col, 4) << 8);
    if ((k == 13 && v->n < 2) || (k == 14 && v->n >= 2)) {      /* the cycle's real tick */
        v->vic.mem.fetch_cb = vic_real_cb;
        v->vic.mem.user_data = v;
        v->vic.crt_set_pixel = vic_setpix;
        v->vic.crt_set_pixel_fb = v;
        v->got = 0;
        m6569_tick_fetch(&v->vic, 0);
        uint64_t pins = 0;
        bool cs = !pin_in(s, p, 13);
        unsigned reg = v->vic_reg & 63;
        if (cs && (reg == 0x2F || reg == 0x30)) {
            if (pin_in(s, p, 14)) { v->dout = reg == 0x2F ? (uint8_t)(v->reg2f | 0xF8) : (uint8_t)(v->reg30 | 0xFC); v->dout_en = 1; }
            else {
                uint8_t d = (uint8_t)part_read_bus(s, p, v_sd, 8);
                if (reg == 0x2F) v->reg2f = d; else v->reg30 = d;
            }
            cs = false;
        }
        if (cs) {
            pins |= M6569_CS | reg;
            if (pin_in(s, p, 14)) pins |= M6569_RW;
            else M6569_SET_DATA(pins, part_read_bus(s, p, v_sd, 8));
        }
        pins = m6569_tick_finish(&v->vic, pins);
        if ((pins & (M6569_CS | M6569_RW)) == (M6569_CS | M6569_RW)) { v->dout = M6569_GET_DATA(pins); v->dout_en = 1; }
        v->cycles++;
    }
    sim_mark_dirty(s, (int)(p - s->parts));
}

static void vic8564_reset(sim_t *s, part_t *p) {
    (void)s;
    vic8564_t *v = p->state;
    uint32_t *fb = v->fb;
    int w = v->fb_w, h = v->fb_h;
    memset(v, 0, sizeof(*v));
    v->fb = fb; v->fb_w = w; v->fb_h = h;
    v->reg2f = 0xFF;
    v->reg30 = 0;
    m6569_init(&v->vic, &(m6569_desc_t){
        .fetch_cb = vic_real_cb, .user_data = v,
        .screen = { .x = 64, .y = 24, .width = w ? w : 392, .height = h ? h : 272 },
        .crt_set_pixel = vic_setpix, .crt_set_pixel_fb = v });
}

void c128_vic_set_screen(part_t *p, uint32_t *fb, int w, int h) {
    vic8564_t *v = p->state;
    v->fb = fb; v->fb_w = w; v->fb_h = h;
}
bool c128_vic_fast(const part_t *p) { return (((const vic8564_t *)p->state)->reg30 & 1) != 0; }
uint8_t c128_vic_reg_of(const part_t *p, int r) { return ((const vic8564_t *)p->state)->vic.reg.regs[(r & 63) < 47 ? (r & 63) : 0]; }

const part_type_t PT_8564 = { "8564 VIC-IIe",
    "D6:B D5:B D4:B D3:B D2:B D1:B D0:B /IRQ:O /LP:m BA:O /DMARQST:m AEC:O /CS:m R/W:m /DMAACK:O CHROMA:O SYNC:O "
    "1MHZ:O /RAS:O /CAS:O MUX:O /IOACC:m 2MHZ:O GND:P Z80PHI:O K0:O K1:O K2:O PHICOL:m PHIIN:m A11:O MA0:B MA1:B MA2:B "
    "MA3:B MA4:B MA5:B A6:O A7:O A8:O A9:O A10:O D11:B D10:B D9:B D8:B D7:B VCC:P",
    vic8564_eval, vic8564_commit, vic8564_reset, sizeof(vic8564_t), TECH_NMOS };

/* 390059-01 character ROM (U18): 8K, 24 pins: A0-A7 on pins 8..1, A8 23, A9 22, A10 19, A11 18, A12 21, /CS 20 */
static const int chr_a[13] = { 8, 7, 6, 5, 4, 3, 2, 1, 23, 22, 19, 18, 21 };
static const int chr_d[8] = { 9, 10, 11, 13, 14, 15, 16, 17 };
static void chr128_eval(sim_t *s, part_t *p) {
    rom128_t *st = p->state;
    bool sel = !pin_in(s, p, 20);
    unsigned a = sel ? part_read_bus(s, p, chr_a, 13) : 0;
    part_drive_bus_cached(s, p, chr_d, 8, st->data && a < st->size ? st->data[a] : 0xFF, sel, &st->out_cache);
}
const part_type_t PT_C128CHR = { "390059-01 ROM",
    "A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O D7:O /CS:M A12:M A10:M A11:M "
    "A9:M A8:M VCC:P", chr128_eval, 0, 0, sizeof(rom128_t), TECH_NMOS };

uint8_t c128_vdc_peek(const part_t *p, uint16_t addr) { return ((const vdc_t *)p->state)->ram[addr]; }
uint8_t c128_vdc_reg_of(const part_t *p, int r) { return ((const vdc_t *)p->state)->reg[r % 38]; }
