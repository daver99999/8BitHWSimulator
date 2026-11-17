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
    "A15:m A14:m A13:m A12:m A11:m A10:m VICFIX:m DMAACK:m AEC:m R/W:m GAME:m EXROM:m /Z80EN:m Z80IO:m "
    "MS3:m MS2:m MS1:m MS0:m MA4:m MA5:m BA:m LORAM:m HIRAM:m GND:P CHAREN:m VA14:m 128/256:m "
    "/SDEN:O DIR:O /ROML:O /ROMH:O CLRBNK:O /FROM1:O /ROM4:O /ROM3:O /ROM2:O /ROM1:O /IOCS:O NC:X /DWE:O CASENB:O "
    "/VIC:O /IOCC:O /GWE:O /COLORAM:O /CHAROM:O CLK:m VCC:P",
    pla8721_eval, pla8721_commit, pla8721_reset, sizeof(pla8721_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* 8722 MMU (U7)                                                             */
/* ------------------------------------------------------------------------ */

typedef struct {
    uint8_t cr, pcr[4], mcr, rcr, p0l, p0h, p1l, p1h;
    uint8_t phi0, was_reset;
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
    if (z80) {                                      /* the Z80 sees the RAM 4K higher, the BIOS at $0000 */
        m->ta = (uint8_t)(hi + 0x10);
        m->block = (t->cr >> 6) & 3;
        if (hi < 0x10) m->ms = (t->cr & 1) ? 3 : 0;
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
    if (hi == 0xD5 && lo <= 0x0B && !(t->cr & 1)) return (int)lo;
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
    bool rd = reg >= 0 && reg < 12 && pin_in(s, p, MMU_RW) && aec;
    unsigned v = 0xFF;
    if (rd) switch (reg) {
        case 0: v = t->cr; break;
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
    if (!phi0 && t->phi0 && !pin_in(s, p, MMU_RW) && pin_in(s, p, MMU_AEC)) {     /* end of a write cycle */
        int reg = mmu_reg(s, p, t);
        uint8_t v = (uint8_t)part_read_bus(s, p, mmu_d, 8);
        if (reg >= 0x100) {
            if (reg == 0x100) t->cr = v; else t->cr = t->pcr[reg - 0x101];
            changed = true;
        } else if (reg >= 0) {
            switch (reg) {
            case 0: t->cr = v; break;
            case 1: case 2: case 3: case 4: t->pcr[reg - 1] = v; break;
            case 5: t->mcr = v & 0x79; break;
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
    "VDD:P /RES:m TA15:O TA14:O TA13:O TA12:O TA11:O TA10:O TA9:O TA8:O /CAS1:O /CAS0:O MS2:O MS1:O MS0:O AEC:m MUX:m "
    "A0:m A1:m A2:m A3:m A5/A4:m A7/A6:m A8:m A9:m A10:m A11:m A12:m A13:m A14:m A15:m R/W:m PHI0:M VSS:P "
    "D0:B D1:B D2:B D3:B D4:B D5:B D6:B D7:B /Z80EN:O /FSDIR:O /GAME:B /EXROM:B MS3:O 40/80:m",
    mmu_eval, mmu_commit, mmu_reset, sizeof(mmu8722_t), TECH_NMOS };
