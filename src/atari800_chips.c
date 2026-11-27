/* atari800_chips.c - the chips of the Atari 800 that the other machines do not have: the three custom chips (ANTIC, GTIA, POKEY) as
 * behavioural models of what the real chips do at their pins, the mask ROMs of the personality board and the small chips of the boards.
 *
 * Timing. The board has one master clock, the 3.579545 MHz crystal (OSC). Every step of the simulation is half a period of it. GTIA
 * divides OSC by two and puts the result on F0 (1.79 MHz): the CPU board makes PHI2 (= F0), PHI1, the 6502's phase 0 and the halt
 * synchroniser from it. A cycle of the machine is two periods of OSC = four steps; ANTIC and GTIA each count the periods of OSC from power
 * on (they are never out of step: that is what the real chips are made sure of by the video signal) and a line is 228 periods = 114
 * cycles, a frame 262 lines. ANTIC takes the bus for a cycle by pulling HALT low in the cycle before; the CPU board's flip-flop Z302 turns
 * that into the stopped 6502 clock and the disabled address buffers. A scan line of the video is made by ANTIC (the playfield, as AN0-AN2
 * codes on every step: half a colour clock) and GTIA (players, missiles, priorities, colours, collisions; luminance and chroma on its pins and
 * the picture in the machine's frame buffer).
 * The AN code is: 0 background, 2 hi-res pixel off (colour PF2), 3 hi-res pixel on (PF2 hue, PF1 luminance), 4 to 7 playfield 0 to 3.
 *
 * Registers are the real ones (the Atari 400/800 Hardware Manual); what is not modelled: ANTIC horizontal / vertical fine scrolling, the
 * light pen, GTIA's graphics modes 9 to 11, POKEY's high-pass filters, the serial port (SEROUT / SERIN) and the analog side of the pots. */
#include "atari800.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ------------------------------------------------------------------------ */
/* Small chips                                                               */
/* ------------------------------------------------------------------------ */

static void xtal358_eval(sim_t *s, part_t *p) { pin_out(s, p, 1, DRV_BIT(s->clock_level)); }
const part_type_t PT_XTAL358 = { "3.579545 XTAL", "P1:O P2:O", xtal358_eval, 0, 0, 0, TECH_TTL };

/* The two 20 pin address buffers of the CPU board (Z303, Z304): eight non-inverting buffers, enabled while both /G pins (1, 19) are low. */
static const int buf_in[8] = { 3, 5, 7, 9, 12, 14, 16, 18 }, buf_out[8] = { 17, 15, 13, 11, 8, 6, 4, 2 };
static void buf_eval(sim_t *s, part_t *p) {
    bool en = !pin_in(s, p, 1) && !pin_in(s, p, 19);
    for (int i = 0; i < 8; i++) pin_out(s, p, buf_out[i], en ? DRV_BIT(pin_in(s, p, buf_in[i])) : DRV_Z);
}
const part_type_t PT_ATARI_BUF = { "bus buffer",
    "/G1:T Y7:O A0:T Y6:O A1:T Y5:O A2:T Y4:O A3:T GND:P Y3:O A4:T Y2:O A5:T Y1:O A6:T Y0:O A7:T /G2:T VCC:P",
    buf_eval, 0, 0, 0, TECH_TTL };
/* (the pins are named by the signal they carry: A0 is input 3 and output 17, which is Y0; the list above names outputs Y7..Y0 in pin order) */

/* 74LS03: quad open collector NAND (it can only pull low). */
static const int q03[4][3] = { {1, 2, 3}, {4, 5, 6}, {9, 10, 8}, {12, 13, 11} };
static void ls03_eval(sim_t *s, part_t *p) {
    for (int i = 0; i < 4; i++)
        pin_out(s, p, q03[i][2], pin_in(s, p, q03[i][0]) && pin_in(s, p, q03[i][1]) ? DRV_LO : DRV_Z);
}
const part_type_t PT_74LS03 = { "74LS03", "1A:T 1B:T 1Y:O 2A:T 2B:T 2Y:O GND:P 3Y:O 3A:T 3B:T 4Y:O 4A:T 4B:T VCC:P",
    ls03_eval, 0, 0, 0, TECH_TTL_OC };

/* 74LS42: BCD to decimal decoder, active low outputs. Pins: Y0 Y1 Y2 Y3 Y4 Y5 Y6 GND Y7 Y8 Y9 D C B A VCC */
static void ls42_eval(sim_t *s, part_t *p) {
    static const int y[10] = { 1, 2, 3, 4, 5, 6, 7, 9, 10, 11 };
    int n = pin_in(s, p, 15) | pin_in(s, p, 14) << 1 | pin_in(s, p, 13) << 2 | pin_in(s, p, 12) << 3;
    for (int i = 0; i < 10; i++) pin_out(s, p, y[i], DRV_BIT(n != i));
}
const part_type_t PT_74LS42 = { "74LS42", "Y0:O Y1:O Y2:O Y3:O Y4:O Y5:O Y6:O GND:P Y7:O Y8:O Y9:O D:T C:T B:T A:T VCC:P",
    ls42_eval, 0, 0, 0, TECH_TTL };

/* 74LS158: quad 2-to-1 multiplexer with INVERTING outputs. S low selects the A inputs. /STROBE high: outputs low (inverted zero = high: the
 * 158 gives 1 with the strobe high). Pins: S 1A 1B 1Y 2A 2B 2Y GND 3Y 3B 3A 4Y 4B 4A /G VCC */
static void ls158_eval(sim_t *s, part_t *p) {
    static const int sec[4][3] = { {2, 3, 4}, {5, 6, 7}, {11, 10, 9}, {14, 13, 12} };
    bool en = !pin_in(s, p, 15);
    int sel = pin_in(s, p, 1);
    for (int i = 0; i < 4; i++)
        pin_out(s, p, sec[i][2], DRV_BIT(!(en && pin_in(s, p, sel ? sec[i][1] : sec[i][0]))));
}
const part_type_t PT_74LS158 = { "74LS158", "S:T 1A:T 1B:T 1Y:O 2A:T 2B:T 2Y:O GND:P 3Y:O 3B:T 3A:T 4Y:O 4B:T 4A:T /G:T VCC:P",
    ls158_eval, 0, 0, 0, TECH_TTL };

/* CD4051: eight channel analog multiplexer / switch. The selected channel connects X to COM: each side gets what drives the other (as the
 * 4066 does). INH high: all off. Pins: X4 X6 COM X7 X5 INH VEE VSS C B A X3 X0 X1 X2 VDD */
static void cd4051_eval(sim_t *s, part_t *p) {
    static const int x[8] = { 13, 14, 15, 12, 1, 5, 2, 4 };
    int com = part_pin(p, 3);
    int sel = pin_in(s, p, 11) | pin_in(s, p, 10) << 1 | pin_in(s, p, 9) << 2;
    bool inh = pin_in(s, p, 6) != 0;
    for (int i = 0; i < 8; i++) {
        int xp = part_pin(p, x[i]);
        if (xp < 0 || com < 0) continue;
        if (!inh && i == sel) sim_drive(s, xp, sim_drive_others(s, com));
        else sim_drive(s, xp, DRV_Z);
    }
    if (!inh && com >= 0 && part_pin(p, x[sel]) >= 0) sim_drive(s, com, sim_drive_others(s, part_pin(p, x[sel])));
    else if (com >= 0) sim_drive(s, com, DRV_Z);
}
const part_type_t PT_CD4051 = { "CD4051", "X4:S X6:S COM:S X7:S X5:S INH:M VEE:P VSS:P C:M B:M A:M X3:S X0:S X1:S X2:S VDD:P",
    cd4051_eval, 0, 0, 0, TECH_CMOS };

/* CD4050: hex non-inverting buffer. Pins: VDD 1Y 1A 2Y 2A 3Y 3A VSS 4A 4Y 5A 5Y NC 6A 6Y NC */
static void cd4050_eval(sim_t *s, part_t *p) {
    static const int g[6][2] = { {3, 2}, {5, 4}, {7, 6}, {9, 10}, {11, 12}, {14, 15} };
    for (int i = 0; i < 6; i++) pin_out(s, p, g[i][1], DRV_BIT(pin_in(s, p, g[i][0])));
}
const part_type_t PT_CD4050 = { "CD4050", "VDD:P AY:O A:M BY:O B:M CY:O C:M VSS:P D:M DY:O E:M EY:O NC:O F:M FY:O NC:O",
    cd4050_eval, 0, 0, 0, TECH_CMOS };

/* ------------------------------------------------------------------------ */
/* A plug-in cartridge: an 8K ROM at $A000-$BFFF (the left slot, BASIC). It drives RD5 high, which takes the RAM's $A000 select away. */
/* ------------------------------------------------------------------------ */

typedef struct { const uint8_t *data; int32_t cache; } acart_t;
static const int cart_a[13] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
static const int cart_d[8] = { 14, 15, 16, 17, 18, 19, 20, 21 };
void atari_cart_set(part_t *p, const uint8_t *data) { ((acart_t *)p->state)->data = data; }
static void acart_eval(sim_t *s, part_t *p) {
    acart_t *c = p->state;
    pin_out(s, p, 23, c->data ? DRV_HI : DRV_Z);
    if (!c->data) { part_drive_bus(s, p, cart_d, 8, 0, false); return; }
    bool sel = !pin_in(s, p, 22);
    unsigned a = part_read_bus(s, p, cart_a, 13);
    part_drive_bus_cached(s, p, cart_d, 8, sel ? c->data[a] : 0, sel, &c->cache);
}
const part_type_t PT_ATARI_CART = { "cartridge ROM",
    "A0:M A1:M A2:M A3:M A4:M A5:M A6:M A7:M A8:M A9:M A10:M A11:M A12:M D0:O D1:O D2:O D3:O D4:O D5:O D6:O D7:O /CS:M RD5:O NC:O",
    acart_eval, 0, 0, sizeof(acart_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* Mask ROMs of the personality board (A401 "F", A402 "D", A403 "E")         */
/* ------------------------------------------------------------------------ */

typedef struct { const uint8_t *data; uint16_t size; uint8_t cs18, cs20, cs21; int32_t cache; } arom_t;
static const int rom_a[11] = { 8, 7, 6, 5, 4, 3, 2, 1, 23, 22, 19 };       /* A0 .. A10 */
static const int rom_d[8] = { 9, 10, 11, 13, 14, 15, 16, 17 };
void atari_rom_set(part_t *p, const uint8_t *data, unsigned size, int cs18, int cs20, int cs21) {
    arom_t *r = p->state;
    r->data = data; r->size = (uint16_t)size; r->cs18 = (uint8_t)cs18; r->cs20 = (uint8_t)cs20; r->cs21 = (uint8_t)cs21;
}
static void arom_eval(sim_t *s, part_t *p) {
    arom_t *r = p->state;
    if (!r->data) { part_drive_bus(s, p, rom_d, 8, 0, false); return; }
    bool sel = pin_in(s, p, 20) == r->cs20 && pin_in(s, p, 21) == r->cs21;
    unsigned a = part_read_bus(s, p, rom_a, 11);
    if (r->size == 2048) sel = sel && pin_in(s, p, 18) == r->cs18;
    else a |= (unsigned)pin_in(s, p, 18) << 11;
    unsigned v = sel ? r->data[a % r->size] : 0;
    if (sel && p->mfault) {
        unsigned ca = p->mfault == MF_ADDR ? a & ~(1u << p->mf_bit) : a;
        v = r->data[ca % r->size];
        switch (p->mfault) {
        case MF_DATA: v = p->mf_val ? v | 1u << p->mf_bit : v & ~(1u << p->mf_bit); break;
        case MF_FLIP: if (a == p->mf_addr) v ^= p->mf_val; break;
        case MF_WEAK: if (!((a * 2654435761u) >> 26)) v = 0; break;
        default: break;
        }
    }
    part_drive_bus_cached(s, p, rom_d, 8, v, sel, &r->cache);
}
bool atari_rom_is(const part_t *p) { return p->eval == arom_eval; }
const part_type_t PT_ATARI_ROM = { "mask ROM",
    "A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O VSS:P D3:O D4:O D5:O D6:O D7:O CS/A11:M A10:M CS:M CS:M A9:M A8:M VCC:P",
    arom_eval, 0, 0, sizeof(arom_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* Register access shared by the custom chips                                */
/* ------------------------------------------------------------------------ */

/* The chips are clocked by PHI2 like the 6520: while it is high they sample the select, R/W, register number and the data bus and
 * answer a read; the write happens when it falls. Outputs are driven only in eval(), so the order the chips commit in doesn't matter. */
typedef struct { uint8_t clk, sel, rw, ra, data, out_en, out_data; } areg_t;

static const int d_pokey[8] = { 38, 39, 40, 2, 3, 4, 5, 6 };
static const int d_gtia[8] = { 7, 6, 5, 4, 37, 36, 35, 34 };
static const int d_antic[8] = { 30, 31, 32, 33, 40, 39, 38, 37 };
static const int a_antic[16] = { 13, 12, 11, 10, 28, 27, 26, 25, 24, 23, 16, 22, 17, 18, 19, 20 };

/* ------------------------------------------------------------------------ */
/* POKEY (C012294)                                                           */
/* ------------------------------------------------------------------------ */

enum { PK_AUDF1, PK_AUDC1, PK_AUDF2, PK_AUDC2, PK_AUDF3, PK_AUDC3, PK_AUDF4, PK_AUDC4, PK_AUDCTL, PK_STIMER, PK_SKREST, PK_POTGO,
       PK_X0C, PK_SEROUT, PK_IRQEN, PK_SKCTL };

typedef struct {
    areg_t r;
    uint8_t audf[4], audc[4], audctl, skctl, irqen, irqst;
    uint32_t cnt[4];
    uint32_t cyc, lfsr;
    uint8_t aud[4], sdiv;
    /* keyboard */
    uint8_t scan, pass, kbcode, held, held_code, cand, cand_pass, cand_valid, shift;
    uint16_t scan_cyc;
    /* serial output */
    uint8_t sr_busy, sr_bit, sr_data, sr_hold_full, sr_hold, sod;
    uint8_t irq_out, aud_out;
} pokey_t;

static const int pk_k[6] = { 23, 22, 21, 20, 19, 18 };
static const int pk_a[4] = { 36, 35, 34, 33 };

static void pk_irq(pokey_t *k, int bit) { if (k->irqen & bit) k->irqst &= (uint8_t)~bit; }

static void pk_underflow(pokey_t *k, int ch) {
    k->aud[ch] ^= 1;
    if (ch == 0) pk_irq(k, 1);
    else if (ch == 1) pk_irq(k, 2);
    else if (ch == 3) {
        pk_irq(k, 4);
        if ((k->sdiv ^= 1) == 0 && k->sr_busy) {              /* two timer 4 underflows make a bit */
            if (++k->sr_bit == 10) {
                if (k->sr_hold_full) {
                    k->sr_data = k->sr_hold; k->sr_hold_full = 0; k->sr_bit = 0;
                    pk_irq(k, 0x10);
                } else {
                    k->sr_busy = 0;
                    pk_irq(k, 0x08);
                }
            }
        }
    }
}

static void pk_timers(pokey_t *k) {
    bool t64 = k->cyc % 28 == 0, t15 = k->cyc % 114 == 0;
    bool base = (k->audctl & 1) ? t15 : t64;
    bool f1 = k->audctl & 0x40, f3 = k->audctl & 0x20, j12 = k->audctl & 0x10, j34 = k->audctl & 8;
    bool tk[4] = { f1 || base, base, f3 || base, base };
    for (int i = 0; i < 4; i++) {
        if (i == 0 && j12) continue;
        if (i == 2 && j34) continue;
        bool tick = (i == 1 && j12) ? tk[0] : (i == 3 && j34) ? tk[2] : tk[i];
        if (!tick) continue;
        if (k->cnt[i] > 1) { k->cnt[i]--; continue; }
        uint32_t per;
        if (i == 1 && j12) per = (uint32_t)(k->audf[1] << 8 | k->audf[0]) + (f1 ? 7 : 1);
        else if (i == 3 && j34) per = (uint32_t)(k->audf[3] << 8 | k->audf[2]) + (f3 ? 7 : 1);
        else per = (uint32_t)k->audf[i] + (((i == 0 && f1) || (i == 2 && f3)) ? 4 : 1);
        k->cnt[i] = per;
        pk_underflow(k, i);
    }
}

static void pk_reload(pokey_t *k) {
    bool f1 = k->audctl & 0x40, f3 = k->audctl & 0x20, j12 = k->audctl & 0x10, j34 = k->audctl & 8;
    for (int i = 0; i < 4; i++) {
        if (i == 1 && j12) k->cnt[i] = (uint32_t)(k->audf[1] << 8 | k->audf[0]) + (f1 ? 7 : 1);
        else if (i == 3 && j34) k->cnt[i] = (uint32_t)(k->audf[3] << 8 | k->audf[2]) + (f3 ? 7 : 1);
        else k->cnt[i] = (uint32_t)k->audf[i] + (((i == 0 && f1) || (i == 2 && f3)) ? 4 : 1);
    }
}

/* The key scan: K0-K5 count 0..63, one step per 114 cycles (15 kHz). A key is seen by KR1 going low while its code is on the K lines
 * and counts after it has been seen on two passes of the scan. */
static void pk_scan(sim_t *s, part_t *p, pokey_t *k) {
    if (++k->scan_cyc < 100) return;
    int kr1 = pin_in(s, p, 25), kr2 = pin_in(s, p, 16);
    if (k->skctl & 2) {
        bool down = !kr1;
        if (k->held) {
            if (k->scan == k->held_code && !down) k->held = 0;
        } else if (down) {
            if (k->cand_valid && k->cand == k->scan && k->pass != k->cand_pass) {
                k->held = 1; k->held_code = k->scan; k->cand_valid = 0;
                k->kbcode = (uint8_t)(k->scan | (!kr2 ? 0x40 : 0));
                k->shift = !kr2;
                pk_irq(k, 0x40);
            } else if (!k->cand_valid) { k->cand_valid = 1; k->cand = k->scan; k->cand_pass = k->pass; }
        } else if (k->cand_valid && k->cand == k->scan && k->pass != k->cand_pass) k->cand_valid = 0;
    }
}

static uint8_t pk_read(pokey_t *k, int ra) {
    switch (ra) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: return 0xE4;
    case 8: return 0;
    case 9: return k->kbcode;
    case 10: return (uint8_t)(k->lfsr >> 9);
    case 14: return k->irqst;
    case 15: return (uint8_t)(0xFF & ~(k->held ? 4 : 0) & ~(k->held && k->shift ? 8 : 0));
    default: return 0xFF;
    }
}

static void pk_write(pokey_t *k, int ra, uint8_t v) {
    switch (ra) {
    case PK_AUDF1: case PK_AUDF2: case PK_AUDF3: case PK_AUDF4: k->audf[ra >> 1] = v; break;
    case PK_AUDC1: case PK_AUDC2: case PK_AUDC3: case PK_AUDC4: k->audc[ra >> 1] = v; break;
    case PK_AUDCTL: k->audctl = v; break;
    case PK_STIMER: pk_reload(k); break;
    case PK_SEROUT:
        if (!k->sr_busy) { k->sr_busy = 1; k->sr_bit = 0; k->sr_data = v; k->sdiv = 0; pk_irq(k, 0x10); }
        else { k->sr_hold = v; k->sr_hold_full = 1; }
        break;
    case PK_IRQEN: k->irqen = v; k->irqst |= (uint8_t)~v; break;
    case PK_SKCTL:
        k->skctl = v;
        if ((v & 3) == 0) { k->lfsr = 0x1FFFF; k->held = 0; k->cand_valid = 0; k->sr_busy = 0; k->sr_hold_full = 0; }
        break;
    default: break;
    }
}

static void pokey_eval(sim_t *s, part_t *p) {
    pokey_t *k = p->state;
    for (int i = 0; i < 6; i++) pin_out(s, p, pk_k[i], DRV_BIT((k->scan >> i) & 1));
    pin_out(s, p, 29, k->irq_out ? DRV_LO : DRV_Z);
    pin_out(s, p, 28, DRV_BIT(k->sr_busy ? (k->sr_bit == 0 ? 0 : k->sr_bit == 9 ? 1 : (k->sr_data >> (k->sr_bit - 1)) & 1) : 1));
    pin_out(s, p, 37, DRV_BIT(k->aud_out));
    part_drive_bus(s, p, d_pokey, 8, k->r.out_data, k->r.out_en && k->r.clk);
}

static void pokey_commit(sim_t *s, part_t *p) {
    pokey_t *k = p->state;
    if (p->dead) return;
    areg_t *r = &k->r;
    int clk = pin_in(s, p, 7);
    bool dirty = false;
    if (clk) {
        r->sel = !pin_in(s, p, 30) && pin_in(s, p, 31);
        r->rw = (uint8_t)pin_in(s, p, 32);
        r->ra = (uint8_t)part_read_bus(s, p, pk_a, 4);
        r->data = (uint8_t)part_read_bus(s, p, d_pokey, 8);
        bool want = r->sel && r->rw;
        if (want != (bool)r->out_en) {
            r->out_en = want;
            if (want) r->out_data = pk_read(k, r->ra);
            dirty = true;
        }
    } else if (r->clk) {                                    /* PHI2 fell: the cycle ends */
        r->out_en = 0;
        if (r->sel && !r->rw) pk_write(k, r->ra, r->data);
        r->sel = 0;
        k->cyc++;
        pk_timers(k);
        k->lfsr = (k->lfsr >> 1) | ((((k->lfsr ^ (k->lfsr >> 5)) & 1)) << 16);
        pk_scan(s, p, k);
        if (k->scan_cyc >= 114) {
            k->scan_cyc = 0;
            if (++k->scan == 64) { k->scan = 0; k->pass++; }
        }
        uint8_t irq = (uint8_t)(~k->irqst) != 0;
        uint8_t a = 0;
        for (int i = 0; i < 4; i++) a |= (k->aud[i] && (k->audc[i] & 0x0F)) ? 1 : 0;
        if (irq != k->irq_out || a != k->aud_out) { k->irq_out = irq; k->aud_out = a; }
        dirty = true;
    }
    r->clk = (uint8_t)clk;
    if (dirty) sim_mark_dirty(s, (int)(p - s->parts));
}

static void pokey_reset(sim_t *s, part_t *p) {
    (void)s;
    pokey_t *k = p->state;
    memset(k, 0, sizeof(*k));
    k->irqst = 0xFF; k->lfsr = 0x1FFFF; k->kbcode = 0;
    for (int i = 0; i < 4; i++) k->cnt[i] = 1;
}

const part_type_t PT_ATARI_POKEY = { "POKEY",
    "VSS:P D3:B D4:B D5:B D6:B D7:B PHI2:M P6:m P7:m P4:m VSS:P P2:m P3:m P0:m P1:m KR2:m VCC:P K5:O K4:O K3:O K2:O K1:O K0:O "
    "SID:m KR1:m BCLK:m OCLK:m SOD:O /IRQ:O /CS0:m CS1:m R/W:m A3:m A2:m A1:m A0:m AUD:O D0:B D1:B D2:B",
    pokey_eval, pokey_commit, pokey_reset, sizeof(pokey_t), TECH_NMOS };

void atari_pokey_keys_status(const part_t *p, char *buf, int len) {
    const pokey_t *k = p->state;
    snprintf(buf, (size_t)len, "scan %02X kbcode %02X irqst %02X irqen %02X skctl %02X audctl %02X", k->scan, k->kbcode, k->irqst,
             k->irqen, k->skctl, k->audctl);
}

/* ------------------------------------------------------------------------ */
/* GTIA (C014805)                                                            */
/* ------------------------------------------------------------------------ */

enum { G_HPOSP0 = 0, G_HPOSM0 = 4, G_SIZEP0 = 8, G_SIZEM = 12, G_GRAFP0 = 13, G_GRAFM = 17, G_COLPM0 = 18, G_COLPF0 = 22, G_COLBK = 26,
       G_PRIOR = 27, G_VDELAY = 28, G_GRACTL = 29, G_HITCLR = 30, G_CONSOL = 31 };

typedef struct {
    areg_t r;
    uint8_t w[32];
    uint8_t m2pf[4], p2pf[4], m2pl[4], p2pl[4];
    uint8_t osc, f0, halted, lum, hue, csync, spk;
    uint16_t cyc, hp, line;
    uint8_t marker;
    uint32_t *fb;
    uint8_t cs_sel_prev;
} gtia_t;

static const int a_gtia[5] = { 2, 1, 40, 39, 38 };

void atari_gtia_set_screen(part_t *p, uint32_t *fb) { ((gtia_t *)p->state)->fb = fb; }
uint8_t atari_gtia_reg_of(const part_t *p, int r) { return ((const gtia_t *)p->state)->w[r & 31]; }

static uint32_t atari_rgb(uint8_t c) {
    int hue = c >> 4, lum = (c >> 1) & 7;
    float y = 0.06f + (float)lum * 0.132f, u = 0, v = 0;
    if (hue) {
        float th = (190.0f - (float)(hue - 1) * 24.0f) * 0.0174533f;
        u = 0.26f * cosf(th); v = 0.26f * sinf(th);
    }
    float r = y + 1.14f * v, g = y - 0.395f * u - 0.581f * v, b = y + 2.032f * u, f[3] = { r, g, b };
    uint32_t out = 0xFF000000u;
    for (int i = 0; i < 3; i++) {
        float x = f[i] < 0 ? 0 : f[i] > 1 ? 1 : f[i];
        out |= (uint32_t)(x * 255.0f + 0.5f) << (8 * i);
    }
    return out;
}

static uint8_t gtia_read(gtia_t *g, sim_t *s, part_t *p, int ra) {
    if (ra < 4) return g->m2pf[ra];
    if (ra < 8) return g->p2pf[ra - 4];
    if (ra < 12) return g->m2pl[ra - 8];
    if (ra < 16) return g->p2pl[ra - 12];
    if (ra < 20) return (uint8_t)pin_in(s, p, 8 + (ra - 16));
    if (ra == 20) return 0x0F;
    if (ra == 31) return (uint8_t)(pin_in(s, p, 12) | pin_in(s, p, 13) << 1 | pin_in(s, p, 14) << 2 | pin_in(s, p, 15) << 3);
    return 0x0F;
}

static void gtia_write(gtia_t *g, int ra, uint8_t v) {
    if (ra == G_HITCLR) { memset(g->m2pf, 0, 4); memset(g->p2pf, 0, 4); memset(g->m2pl, 0, 4); memset(g->p2pl, 0, 4); return; }
    if (ra == G_CONSOL) { g->spk = (v >> 3) & 1; g->w[ra] = v; return; }
    if (ra >= G_GRAFP0 && ra <= G_GRAFP0 + 3) { g->w[ra] = v; return; }
    g->w[ra] = v;
}

/* The colour at colour clock position hp (half clocks) given ANTIC's code: players, missiles, priority, collisions. */
static uint8_t gtia_colour(gtia_t *g, int an, int hp) {
    int cc = hp >> 1;
    uint8_t pm = 0;
    for (int n = 0; n < 4; n++) {
        int sz = g->w[G_SIZEP0 + n] & 3, mul = sz == 1 ? 2 : sz == 3 ? 4 : 1, rel = cc - g->w[G_HPOSP0 + n];
        if (rel >= 0 && rel < 8 * mul && ((g->w[G_GRAFP0 + n] >> (7 - rel / mul)) & 1)) pm |= (uint8_t)(1 << n);
        int ms = (g->w[G_SIZEM] >> (2 * n)) & 3, mm = ms == 1 ? 2 : ms == 3 ? 4 : 1, mrel = cc - g->w[G_HPOSM0 + n];
        if (mrel >= 0 && mrel < 2 * mm && ((g->w[G_GRAFM] >> (2 * n + 1 - mrel / mm)) & 1)) pm |= (uint8_t)(16 << n);
    }
    int pf = an >= 4 ? an - 4 : an == 3 ? 1 : an == 2 ? 2 : -1;
    if (pf >= 0)
        for (int n = 0; n < 4; n++) {
            if (pm & (1 << n)) g->p2pf[n] |= (uint8_t)(1 << pf);
            if (pm & (16 << n)) g->m2pf[n] |= (uint8_t)(1 << pf);
        }
    if (pm & 0xF)
        for (int n = 0; n < 4; n++) {
            if (pm & (1 << n)) g->p2pl[n] |= (uint8_t)((pm & 0xF) & ~(1 << n));
            if (pm & (16 << n)) g->m2pl[n] |= (uint8_t)(pm & 0xF);
        }
    static const int ord[4][9] = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8 },          /* 0..3 players, 4..7 playfields, 8 background */
        { 0, 1, 4, 5, 6, 7, 2, 3, 8 },
        { 4, 5, 6, 7, 0, 1, 2, 3, 8 },
        { 4, 5, 0, 1, 2, 3, 6, 7, 8 } };
    int pr = g->w[G_PRIOR] & 0xF, which = pr == 2 ? 1 : pr == 4 ? 2 : pr == 8 ? 3 : 0;
    for (int i = 0; i < 9; i++) {
        int it = ord[which][i];
        if (it < 4) {
            if (pm & ((1 << it) | (16 << it))) {
                if ((pm & (16 << it)) && !(pm & (1 << it)) && (g->w[G_PRIOR] & 0x10)) return g->w[G_COLPF0 + 3];
                return g->w[G_COLPM0 + it];
            }
        } else if (it < 8) {
            if (an == it) return g->w[G_COLPF0 + (it - 4)];
            if (it == 5 && an == 3) return (uint8_t)((g->w[G_COLPF0 + 2] & 0xF0) | (g->w[G_COLPF0 + 1] & 0x0F));
            if (it == 6 && an == 2) return g->w[G_COLPF0 + 2];
        } else return an == 1 ? 0 : g->w[G_COLBK];
    }
    return g->w[G_COLBK];
}

static void gtia_eval(sim_t *s, part_t *p) {
    gtia_t *g = p->state;
    pin_out(s, p, 29, DRV_BIT(pin_in(s, p, 28)));   /* measured on the board: GTIA pin 29 is the 3.58 MHz clock */
    pin_out(s, p, 22, DRV_BIT(g->lum & 1)); pin_out(s, p, 23, DRV_BIT((g->lum >> 1) & 1));
    pin_out(s, p, 24, DRV_BIT((g->lum >> 2) & 1)); pin_out(s, p, 31, DRV_BIT((g->lum >> 3) & 1));
    pin_out(s, p, 21, g->hue ? DRV_BIT(g->osc) : DRV_LO);
    pin_out(s, p, 25, DRV_BIT(g->csync));
    for (int i = 0; i < 4; i++) { pin_out(s, p, 8 + i, DRV_PULLUP); pin_out(s, p, 12 + i, DRV_PULLUP); }
    part_drive_bus(s, p, d_gtia, 8, g->r.out_data, g->r.out_en && g->r.clk);
}

static void gtia_commit(sim_t *s, part_t *p) {
    gtia_t *g = p->state;
    if (p->dead) return;
    areg_t *r = &g->r;
    int clk = pin_in(s, p, 30), osc = pin_in(s, p, 28);
    bool dirty = false;
    if (osc && !g->osc) { g->f0 ^= 1; dirty = true; }
    g->osc = (uint8_t)osc;
    if (clk) {
        r->sel = !pin_in(s, p, 32);
        r->rw = (uint8_t)pin_in(s, p, 33);
        r->ra = (uint8_t)part_read_bus(s, p, a_gtia, 5);
        r->data = (uint8_t)part_read_bus(s, p, d_gtia, 8);
        bool want = r->sel && r->rw;
        if (want != (bool)r->out_en) {
            r->out_en = want;
            if (want) r->out_data = gtia_read(g, s, p, r->ra);
            dirty = true;
        }
    } else if (r->clk) {                             /* PHI2 fell: the cycle ends */
        r->out_en = 0;
        if (r->sel && !r->rw) gtia_write(g, r->ra, r->data);
        r->sel = 0;
        if (g->halted) {
            int c = g->cyc;
            if (c == 0) g->w[G_GRAFM] = r->data;
            else if (c >= 4 && c <= 7) g->w[G_GRAFP0 + c - 4] = r->data;
        }
        g->halted = !pin_in(s, p, 26);
        g->cyc = (uint16_t)((g->cyc + 1) % 114);
        dirty = true;
    }
    r->clk = (uint8_t)clk;
    /* One picture sample per step (half a colour clock): ANTIC's code on AN0-AN2 picks the playfield colour. */
    int an = pin_in(s, p, 18) | pin_in(s, p, 19) << 1 | pin_in(s, p, 20) << 2;
    if (an == 1) {
        if (g->marker == 0) { g->hp = 0; g->cyc = 0; }
        if (g->marker < 255) g->marker++;
    } else if (g->marker) {
        if (g->marker >= 4) g->line = 0; else g->line++;
        g->marker = 0;
    }
    if (an == 1) {
        g->csync = 0; g->hp++;
    } else {
        uint8_t c = gtia_colour(g, an, g->hp);
        g->csync = (g->hp >= 20 && g->hp < 56) ? 0 : 1;
        if (g->line >= 8 && g->line < 248 && g->hp >= 88 && g->hp < 424 && g->fb)
            g->fb[(g->line - 8) * AT_SCREEN_W + (g->hp - 88)] = atari_rgb(c);
        uint8_t lum = c & 0x0F, hue = c >> 4;
        if (lum != g->lum || hue != g->hue) { g->lum = lum; g->hue = hue; dirty = true; }
        g->hp++;
    }
    if (dirty) sim_mark_dirty(s, (int)(p - s->parts));
}

static void gtia_reset(sim_t *s, part_t *p) {
    (void)s;
    gtia_t *g = p->state;
    uint32_t *fb = g->fb;
    memset(g, 0, sizeof(*g));
    g->fb = fb;
}

const part_type_t PT_ATARI_GTIA = { "GTIA",
    "A1:m A0:m VSS:P D3:B D2:B D1:B D0:B T0:B T1:B T2:B T3:B S0:B S1:B S2:B S3:B NC:m DEL:O AN0:m AN1:m AN2:m COL:O LUM0:O LUM1:O LUM2:O "
    "CSYNC:O /HALT:m VCC:P OSC:M F0:O PHI2:M LUM3:O /CS:m R/W:m D7:B D6:B D5:B D4:B A4:m A3:m A2:m",
    gtia_eval, gtia_commit, gtia_reset, sizeof(gtia_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* ANTIC (C012296)                                                           */
/* ------------------------------------------------------------------------ */

enum { PL_NONE = 0, PL_M, PL_DL, PL_LMSL, PL_LMSH, PL_P0, PL_P1, PL_P2, PL_P3, PL_S, PL_C, PL_DUMMY };

typedef struct {
    areg_t r;
    uint8_t dmactl, chactl, hscrol, vscrol, pmbase, chbase, nmien, nmist;
    uint16_t dlist, dlptr, memscan, line, addr;
    uint8_t cyc, ph, own, own_kind, halt, refn, wsync, nmi_cnt, rnmi_prev, res_prev, refrow, f0p, f0q;
    uint8_t lines_left, tot_lines, mode, dli, jump, jvb, lo_byte, an;
    uint8_t sidx, cidx, nb, dc, instr_lms;
    uint8_t plan[114], nplan[8], refcyc[114];
    uint8_t linebuf[48], chbuf[48];
} antic_t;

static const uint8_t mode_lines[16] = { 0, 0, 8, 10, 8, 16, 8, 16, 8, 4, 4, 2, 1, 2, 1, 1 };
static const uint8_t mode_bytes[16][3] = {      /* narrow, normal, wide */
    {0,0,0}, {0,0,0}, {32,40,48}, {32,40,48}, {32,40,48}, {32,40,48}, {16,20,24}, {16,20,24},
    {8,10,12}, {8,10,12}, {16,20,24}, {16,20,24}, {16,20,24}, {32,40,48}, {32,40,48}, {32,40,48} };
static const uint8_t mode_bph[16] = { 0, 0, 8, 8, 8, 8, 16, 16, 32, 32, 16, 16, 16, 8, 8, 8 };   /* hires pixels per data byte */
static const uint8_t mode_pw[16] = { 0, 0, 1, 1, 2, 2, 2, 2, 8, 4, 4, 2, 2, 2, 2, 1 };           /* hires pixels per pixel */

static void antic_nmi(antic_t *a, int bit, bool always) {
    a->nmist |= (uint8_t)bit;
    if (always || (a->nmien & bit)) a->nmi_cnt = 12;
}

/* The scan lines of a mode line other than the first get their character data again (bitmaps keep the first line's bytes). */
static void antic_plan_pf(antic_t *a, bool first) {
    int m = a->mode, w = a->dmactl & 3;
    if (m < 2 || !w) return;
    int nb = mode_bytes[m][w - 1], dc = w == 1 ? 32 : w == 2 ? 24 : 16, s0 = dc - 3;
    bool chars = m <= 7;
    a->nb = (uint8_t)nb; a->dc = (uint8_t)dc;
    a->cidx = 0;
    if (first) {
        a->sidx = 0;
        for (int j = 0; j < nb; j++) {
            if (chars) { a->plan[s0 + 2 * j] = PL_S; a->plan[s0 + 2 * j + 1] = PL_C; }
            else a->plan[s0 + j] = PL_S;
        }
    } else if (chars) {
        for (int j = 0; j < nb; j++) a->plan[s0 + j] = PL_C;
    }
}

static void antic_dl_decode(antic_t *a, uint8_t op) {
    a->jump = a->jvb = 0;
    a->dli = op >> 7;
    int m = op & 15;
    a->mode = (uint8_t)m;
    a->instr_lms = 0;
    if (m == 0) { a->tot_lines = a->lines_left = (uint8_t)(((op >> 4) & 7) + 1); a->mode = 0; }
    else if (m == 1) {
        a->jump = 1; a->instr_lms = 1; a->mode = 0;
        if (op & 0x40) { a->jvb = 1; a->tot_lines = a->lines_left = 255; }
        else a->tot_lines = a->lines_left = 1;
    } else {
        a->tot_lines = a->lines_left = mode_lines[m];
        if (op & 0x40) a->instr_lms = 1;
    }
    if (a->instr_lms) { a->plan[2] = PL_LMSL; a->plan[3] = PL_LMSH; }
    else a->plan[2] = PL_DUMMY;
    antic_plan_pf(a, true);
}

static uint16_t antic_pm_base(const antic_t *a, bool single) {
    return (uint16_t)(single ? (a->pmbase & 0xF8) << 8 : (a->pmbase & 0xFC) << 8);
}

/* Fill in the plan of the first cycles of the next line (they must be known before the line starts: ANTIC halts the 6502 a cycle ahead). */
static void antic_early_plan(antic_t *a) {
    int y1 = (a->line + 1) % 262;
    memset(a->nplan, 0, sizeof(a->nplan));
    if (y1 < 8 || y1 >= 248) return;
    bool single = a->dmactl & 0x10;
    bool pm_line = single || !(y1 & 1);
    if ((a->dmactl & 0x0C) && pm_line) a->nplan[0] = PL_M;
    if ((a->dmactl & 0x08) && pm_line) for (int i = 0; i < 4; i++) a->nplan[4 + i] = (uint8_t)(PL_P0 + i);
    if (a->dmactl & 0x20) {
        int ll1 = (y1 == 8) ? 0 : (a->lines_left > 0 ? a->lines_left - 1 : 0);
        if (ll1 == 0) { a->nplan[1] = PL_DL; a->nplan[2] = PL_DUMMY; }
    }
}

static void antic_start_line(antic_t *a) {
    int y = a->line;
    memset(a->plan, 0, sizeof(a->plan));
    memcpy(a->plan, a->nplan, sizeof(a->nplan));
    memset(a->refcyc, 0, sizeof(a->refcyc));
    for (int k = 0; k < 9; k++) a->refcyc[25 + 4 * k] = 1;
    if (y == 8) { a->dlptr = a->dlist; a->lines_left = 0; a->mode = 0; }
    else if (y > 8 && y < 248 && a->lines_left > 0) a->lines_left--;
    if (y >= 8 && y < 248 && (a->dmactl & 0x20)) {
        if (a->lines_left > 0 && a->mode >= 2) antic_plan_pf(a, false);
    } else if (y >= 248) {
        a->mode = 0; a->lines_left = 0;
    }
    if (y == 248) antic_nmi(a, 0x40, false);
}

static uint8_t antic_read(antic_t *a, int ra) {
    switch (ra) {
    case 11: return (uint8_t)(a->line >> 1);
    case 12: case 13: return 0;
    case 15: return (uint8_t)(a->nmist | 0x1F);
    default: return 0xFF;
    }
}

static void antic_write(antic_t *a, int ra, uint8_t v) {
    switch (ra) {
    case 0: a->dmactl = v; break;
    case 1: a->chactl = v; break;
    case 2: a->dlist = (uint16_t)((a->dlist & 0xFF00) | v); break;
    case 3: a->dlist = (uint16_t)((a->dlist & 0x00FF) | v << 8); break;
    case 4: a->hscrol = v; break;
    case 5: a->vscrol = v; break;
    case 7: a->pmbase = v; break;
    case 9: a->chbase = v; break;
    case 10: a->wsync = 1; break;
    case 14: a->nmien = v; break;
    case 15: a->nmist = 0; break;
    default: break;
    }
}

/* The address ANTIC puts on the bus for the cycle that starts. */
static uint16_t antic_addr(antic_t *a, int kind) {
    int y = a->line;
    bool single = a->dmactl & 0x10;
    uint16_t base = antic_pm_base(a, single);
    unsigned off = single ? (unsigned)y : (unsigned)(y >> 1);
    switch (kind) {
    case PL_M: return (uint16_t)(base + (single ? 0x300 : 0x180) + off);
    case PL_P0: case PL_P1: case PL_P2: case PL_P3: {
        int n = kind - PL_P0;
        return (uint16_t)(base + (single ? 0x400 + 0x100 * n : 0x200 + 0x80 * n) + off);
    }
    case PL_DL: case PL_LMSL: case PL_LMSH: return a->dlptr;     /* the pointer is advanced as each byte arrives */
    case PL_S: return (uint16_t)((a->memscan & 0xF000) | ((a->memscan + a->sidx) & 0xFFF));
    case PL_C: {
        int m = a->mode, sl = a->tot_lines - a->lines_left, row = (m == 5 || m == 7) ? sl >> 1 : sl;
        uint8_t code = a->linebuf[a->cidx < 48 ? a->cidx : 0];
        unsigned idx, chb;
        if (m <= 5) { chb = (unsigned)(a->chbase & 0xFC) << 8; idx = code & 0x7F; }
        else { chb = (unsigned)(a->chbase & 0xFE) << 8; idx = code & 0x3F; }
        if (m == 3 && (code & 0x60) == 0x60) row = row >= 2 ? row - 2 : 0;
        if (a->chactl & 4) row = 7 - (row & 7);
        return (uint16_t)(chb + idx * 8 + (row & 7));
    }
    default: return 0;
    }
}

static void antic_done(antic_t *a, int kind, uint8_t d) {
    switch (kind) {
    case PL_DL:
        a->dlptr = (uint16_t)((a->dlptr & 0xFC00) | ((a->dlptr + 1) & 0x3FF));
        antic_dl_decode(a, d);
        break;
    case PL_LMSL:
        a->lo_byte = d;
        a->dlptr = (uint16_t)((a->dlptr & 0xFC00) | ((a->dlptr + 1) & 0x3FF));
        break;
    case PL_LMSH:
        a->dlptr = (uint16_t)((a->dlptr & 0xFC00) | ((a->dlptr + 1) & 0x3FF));
        if (a->jump) a->dlptr = (uint16_t)(d << 8 | a->lo_byte);
        else a->memscan = (uint16_t)(d << 8 | a->lo_byte);
        break;
    case PL_S:
        if (a->sidx < 48) a->linebuf[a->sidx] = d;
        if (++a->sidx == a->nb) a->memscan = (uint16_t)((a->memscan & 0xF000) | ((a->memscan + a->nb) & 0xFFF));
        break;
    case PL_C: {
        uint8_t code = a->linebuf[a->cidx < 48 ? a->cidx : 0];
        int sl = a->tot_lines - a->lines_left;
        if (a->mode == 3 && (code & 0x60) == 0x60 ? sl < 2 : (a->mode == 3 && sl >= 8)) d = 0;
        if (a->cidx < 48) a->chbuf[a->cidx] = d;
        a->cidx++;
        break;
    }
    default: break;
    }
}

/* The code on AN0-AN2 for hires pixel 'pos' of the line (0..455). */
static uint8_t antic_an(const antic_t *a, int pos) {
    int y = a->line;
    if (pos < 2 || (y == 0 && pos < 4)) return 1;
    int m = a->mode, w = a->dmactl & 3;
    if (y < 8 || y >= 248 || !(a->dmactl & 0x20) || m < 2 || !w || a->jvb) return 0;
    int x0 = (w == 1 ? 32 : w == 2 ? 24 : 16) * 4, px = pos - x0;
    int wpx = (w == 1 ? 256 : w == 2 ? 320 : 384);
    if (px < 0 || px >= wpx) return 0;
    int j = px / mode_bph[m], q = px % mode_bph[m], k = q / mode_pw[m];
    if (j >= a->nb) return 0;
    uint8_t code = a->linebuf[j], d = m <= 7 ? a->chbuf[j] : code;
    if (m <= 3) {
        if ((code & 0x80) && (a->chactl & 1)) d = 0;
        if ((code & 0x80) && (a->chactl & 2)) d ^= 0xFF;
    }
    switch (m) {
    case 2: case 3: case 15: return ((d >> (7 - k)) & 1) ? 3 : 2;
    case 6: case 7: return ((d >> (7 - k)) & 1) ? (uint8_t)(4 + (code >> 6)) : 0;
    case 9: case 11: case 12: return ((d >> (7 - k)) & 1) ? 4 : 0;
    case 4: case 5: {
        int v = (d >> (6 - 2 * k)) & 3;
        return v == 0 ? 0 : v == 3 ? ((code & 0x80) ? 7 : 6) : (uint8_t)(3 + v);
    }
    default: {
        int v = (d >> (6 - 2 * k)) & 3;
        return v == 0 ? 0 : (uint8_t)(3 + v);
    }
    }
}

static void antic_eval(sim_t *s, part_t *p) {
    pin_out(s, p, 34, DRV_BIT(((antic_t *)p->state)->f0q));   /* the 1.79 MHz clock: F0 (3.58 MHz) divided by two, to the CPU board */
    antic_t *a = p->state;
    pin_out(s, p, 9, DRV_BIT(!a->halt));
    pin_out(s, p, 4, DRV_PULLUP); pin_out(s, p, 6, DRV_PULLUP);        /* LP and /RNMI: pulled up inside */
    pin_out(s, p, 15, a->wsync ? DRV_LO : DRV_Z);
    pin_out(s, p, 7, DRV_BIT(!a->nmi_cnt));
    pin_out(s, p, 8, DRV_BIT(!a->refn));
    pin_out(s, p, 2, DRV_BIT(a->an & 1)); pin_out(s, p, 3, DRV_BIT((a->an >> 1) & 1)); pin_out(s, p, 5, DRV_BIT((a->an >> 2) & 1));
    part_drive_bus(s, p, a_antic, 16, a->addr, (a->own && a->own_kind != PL_DUMMY) || a->refn);   /* during a refresh cycle ANTIC puts its row counter on A0-A6 */
    part_drive_bus(s, p, d_antic, 8, a->r.out_data, a->r.out_en && a->r.clk);
}

static void antic_commit(sim_t *s, part_t *p) {
    antic_t *a = p->state;
    if (p->dead) return;
    areg_t *r = &a->r;
    int clk = pin_in(s, p, 29), res = pin_in(s, p, 36), rnmi = pin_in(s, p, 6);
    bool dirty = false;
    { int f = pin_in(s, p, 35); if (f && !a->f0p) a->f0q ^= 1; a->f0p = (uint8_t)f; }
    if (!res) {                                     /* RESET: the registers clear, the beam goes on */
        a->dmactl = 0; a->nmien = 0; a->nmist = 0; a->wsync = 0; a->nmi_cnt = 0; a->halt = 0; a->own = 0;
        a->lines_left = 0; a->mode = 0; memset(a->nplan, 0, sizeof(a->nplan));
    }
    if (!rnmi && a->rnmi_prev) antic_nmi(a, 0x20, true);
    a->rnmi_prev = (uint8_t)rnmi;
    bool fell = !clk && r->clk;
    if (clk) {
        r->data = (uint8_t)part_read_bus(s, p, d_antic, 8);
        uint16_t addr = (uint16_t)part_read_bus(s, p, a_antic, 16);
        r->sel = !a->own && (addr >> 8) == 0xD4;
        r->rw = (uint8_t)pin_in(s, p, 14);
        r->ra = (uint8_t)(addr & 15);
        r->data = (uint8_t)part_read_bus(s, p, d_antic, 8);
        bool want = r->sel && r->rw;
        if (want != (bool)r->out_en) {
            r->out_en = want;
            if (want) r->out_data = antic_read(a, r->ra);
            dirty = true;
        }
        if (!r->clk) {                              /* PHI2 rose: ANTIC decides about the cycle after this one */
            int c1 = a->cyc + 1;
            uint8_t nxt = c1 < 114 ? a->plan[c1] : a->nplan[0];
            /* a refresh cycle also halts the 6502: the RAM boards cannot be read while REF is low (9 cycles a line) */
            uint8_t h = nxt != PL_NONE || (c1 < 114 && a->refcyc[c1]);
            if (h != a->halt) { a->halt = h; dirty = true; }
        }
    } else if (r->clk) {                            /* PHI2 fell: the cycle ends, the next one starts */
        r->out_en = 0;
        if (r->sel && !r->rw) antic_write(a, r->ra, r->data);
        r->sel = 0;
        if (a->own) antic_done(a, a->own_kind, r->data);
        if (a->cyc == 104) a->wsync = 0;
        if (a->nmi_cnt) a->nmi_cnt--;
        if (++a->cyc == 114) {
            a->cyc = 0;
            a->line = (uint16_t)((a->line + 1) % 262);
            antic_start_line(a);
        }
        if (a->cyc == 100) antic_early_plan(a);
        if (a->cyc == 7) {
            if (a->dli && a->lines_left == 1 && a->line >= 8 && a->line < 248 && (a->dmactl & 0x20)) antic_nmi(a, 0x80, false);
        }
        a->own_kind = a->plan[a->cyc];
        a->own = a->own_kind != PL_NONE;
        a->addr = a->own ? antic_addr(a, a->own_kind) : 0;
        a->refn = a->refcyc[a->cyc] && !a->own;     /* a DMA fetch in the slot does the refresh with its own row */
        if (a->refn) { a->addr = a->refrow; a->refrow = (uint8_t)((a->refrow + 1) & 127); }
        a->ph = 0;
        dirty = true;
    }
    r->clk = (uint8_t)clk;
    if (!fell && a->ph < 3) a->ph++;
    uint8_t an = antic_an(a, a->cyc * 4 + a->ph);
    if (an != a->an) { a->an = an; dirty = true; }
    if (dirty) sim_mark_dirty(s, (int)(p - s->parts));
}

static void antic_reset(sim_t *s, part_t *p) {
    (void)s;
    antic_t *a = p->state;
    memset(a, 0, sizeof(*a));
    a->nmist = 0;
    a->rnmi_prev = 1;
}

const part_type_t PT_ATARI_ANTIC = { "ANTIC",
    "VSS:P AN0:O AN1:O LP:B AN2:O /RNMI:B /NMI:O /REF:O /HALT:O A3:B A2:B A1:B A0:B R/W:m /RDY:O A10:B A12:B A13:B A14:B A15:B VCC:P "
    "A11:B A9:B A8:B A7:B A6:B A5:B A4:B PHI2:M D0:B D1:B D2:B D3:B PHI0:O F0:M /RES:m D7:B D6:B D5:B D4:B",
    antic_eval, antic_commit, antic_reset, sizeof(antic_t), TECH_NMOS };

uint8_t atari_antic_reg_of(const part_t *p, int r) {
    const antic_t *a = p->state;
    switch (r) { case 0: return a->dmactl; case 1: return a->chactl; case 2: return a->dlist & 255; case 3: return a->dlist >> 8;
    case 7: return a->pmbase; case 9: return a->chbase; case 14: return a->nmien; case 15: return a->nmist; default: return 0; }
}
void atari_antic_status_of(const part_t *p, char *buf, int len) {
    const antic_t *a = p->state;
    snprintf(buf, (size_t)len, "line %d cyc %d dmactl %02X dl %04X ptr %04X mode %X left %d nmien %02X", a->line, a->cyc, a->dmactl,
             a->dlist, a->dlptr, a->mode, a->lines_left, a->nmien);
}
