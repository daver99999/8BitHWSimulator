/* zx80_parts.c - the chips of the Sinclair ZX80 that no other machine has: the 74LS93 line counter (IC21), the 74LS165
 * video shift register (IC9), the 74LS365 keyboard buffer (IC10), the ROM socket (IC2: the 4K 8332 or an 8K 2364), the
 * crystal oscillator's 74LS86 (IC20) and the 47 pF differentiator C11. */
#include "parts.h"

/* 74LS93 (IC21): only the B counter is used (CKB = SYNC, QB QC QD = the line counter), it counts on the falling edge. */
typedef struct { uint8_t count, ck; } z8_ls93_t;
static void z8_ls93_eval(sim_t *s, part_t *p) {
    z8_ls93_t *st = p->state;
    if (pin_in(s, p, 2) && pin_in(s, p, 3)) st->count = 0;
    pin_out(s, p, 9, DRV_BIT(st->count & 1));        /* QB */
    pin_out(s, p, 8, DRV_BIT(st->count & 2));        /* QC */
    pin_out(s, p, 11, DRV_BIT(st->count & 4));       /* QD */
    pin_out(s, p, 12, DRV_LO);                       /* QA (CKA is not used) */
}
static void z8_ls93_commit(sim_t *s, part_t *p) {
    z8_ls93_t *st = p->state;
    int ck = pin_in(s, p, 1);
    bool reset = pin_in(s, p, 2) && pin_in(s, p, 3);
    uint8_t old = st->count;
    if (reset) st->count = 0;
    else if (st->ck && !ck) st->count = (uint8_t)((st->count + 1) & 7);
    st->ck = (uint8_t)ck;
    if (st->count != old) sim_mark_dirty(s, (int)(p - s->parts));
}
static void z8_ls93_reset(sim_t *s, part_t *p) { (void)s; z8_ls93_t *st = p->state; st->count = 0; st->ck = 1; }
const part_type_t PT_ZX_LS93 = { "74LS93",
    "CKB:T R01:T R02:T NC:T VCC:P NC:T NC:T QC:O QB:O GND:P QD:O QA:O NC:T CKA:T",
    z8_ls93_eval, z8_ls93_commit, z8_ls93_reset, sizeof(z8_ls93_t), TECH_TTL };

/* 74LS165 (IC9): parallel load while SH/LD is low, shift towards QH (H is the first bit out) on the rising clock. */
typedef struct { uint8_t sr, ck; } z8_ls165_t;
static const int z8_165_in[8] = { 11, 12, 13, 14, 3, 4, 5, 6 };      /* A..H */
static void z8_ls165_eval(sim_t *s, part_t *p) {
    z8_ls165_t *st = p->state;
    if (!pin_in(s, p, 1)) st->sr = (uint8_t)part_read_bus(s, p, z8_165_in, 8);
    pin_out(s, p, 9, DRV_BIT(st->sr & 0x80));
    pin_out(s, p, 7, DRV_BIT(!(st->sr & 0x80)));
}
static void z8_ls165_commit(sim_t *s, part_t *p) {
    z8_ls165_t *st = p->state;
    int ck = pin_in(s, p, 2);
    uint8_t old = st->sr;
    if (!pin_in(s, p, 1)) st->sr = (uint8_t)part_read_bus(s, p, z8_165_in, 8);
    else if (ck && !st->ck && !pin_in(s, p, 15)) st->sr = (uint8_t)((st->sr << 1) | pin_in(s, p, 10));
    st->ck = (uint8_t)ck;
    if (st->sr != old) sim_mark_dirty(s, (int)(p - s->parts));
}
static void z8_ls165_reset(sim_t *s, part_t *p) { (void)s; z8_ls165_t *st = p->state; st->sr = 0; st->ck = 0; }
const part_type_t PT_ZX_LS165 = { "74LS165",
    "SH/LD:T CK:T E:T F:T G:T H:T /QH:O GND:P QH:O SER:T A:T B:T C:T D:T CKINH:T VCC:P",
    z8_ls165_eval, z8_ls165_commit, z8_ls165_reset, sizeof(z8_ls165_t), TECH_TTL };

/* 74LS365 (IC10): six buffers, three-state outputs, enabled while both /G1 and /G2 are low. */
static void z8_ls365_eval(sim_t *s, part_t *p) {
    static const int a[6] = { 2, 4, 6, 10, 12, 14 }, y[6] = { 3, 5, 7, 9, 11, 13 };
    bool en = !pin_in(s, p, 1) && !pin_in(s, p, 15);
    for (int i = 0; i < 6; i++) pin_out(s, p, y[i], en ? DRV_BIT(pin_in(s, p, a[i])) : DRV_Z);
}
const part_type_t PT_ZX_LS365 = { "74LS365",
    "/G1:T A1:T Y1:O A2:T Y2:O A3:T Y3:O GND:P Y4:O A4:T Y5:O A5:T Y6:O A6:T /G2:T VCC:P",
    z8_ls365_eval, 0, 0, 0, TECH_TTL };

/* The ROM socket (IC2). The 8332 (4K) is selected by /CS1 low and CS2 (the A12 line) low; an 8K ROM (the ZX81's 2364) has
 * A12 on that pin. 'size' is that of the image fitted. */
typedef struct { const uint8_t *data; int32_t out_cache; int size; } z8_rom_t;
static const int z8_rom_addr[12] = { 8, 7, 6, 5, 4, 3, 2, 1, 23, 22, 19, 18 };
static const int z8_rom_data[8] = { 9, 10, 11, 13, 14, 15, 16, 17 };
static void z8_rom_eval(sim_t *s, part_t *p) {
    z8_rom_t *st = p->state;
    bool a12 = pin_in(s, p, 21);
    bool sel = !pin_in(s, p, 20) && (st->size > 4096 || !a12);
    unsigned v = 0xFF;
    if (sel && st->data) {
        unsigned addr = part_read_bus(s, p, z8_rom_addr, 12) | (st->size > 4096 && a12 ? 0x1000u : 0);
        v = st->data[addr % (unsigned)st->size];
    }
    part_drive_bus_cached(s, p, z8_rom_data, 8, v, sel, &st->out_cache);
}
const part_type_t PT_ZX_ROM = { "8332 / 2364 ROM",
    "A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O "
    "D7:O A11:M A10:M /CS1:M CS2/A12:M A9:M A8:M VCC:P", z8_rom_eval, 0, 0, sizeof(z8_rom_t), TECH_NMOS };

/* IC20, the 74LS86 that is also the crystal oscillator: gate D (pins 12, 13, 11) is the amplifier; here it is driven by the
 * machine (state 'clk'), the other three gates are XORs. */
typedef struct { uint8_t clk; } z8_osc_t;
static void z8_osc_eval(sim_t *s, part_t *p) {
    static const int g[3][3] = { {1, 2, 3}, {4, 5, 6}, {9, 10, 8} };
    z8_osc_t *st = p->state;
    for (int i = 0; i < 3; i++)
        pin_out(s, p, g[i][2], DRV_BIT(pin_in(s, p, g[i][0]) != pin_in(s, p, g[i][1])));
    pin_out(s, p, 11, DRV_BIT(st->clk));
}
const part_type_t PT_ZX_OSC86 = { "74LS86 (oscillator)",
    "1A:T 1B:T 1Y:O 2A:T 2B:T 2Y:O GND:P 3Y:O 3A:T 3B:T 4Y:O 4A:T 4B:T VCC:P",
    z8_osc_eval, 0, 0, sizeof(z8_osc_t), TECH_TTL };

/* C11 (47 pF) and R25: a rising edge on phi makes a short high pulse; the machine sets 'pulse' for that. */
typedef struct { uint8_t pulse; } z8_pulse_t;
static void z8_pulse_eval(sim_t *s, part_t *p) {
    z8_pulse_t *st = p->state;
    pin_out(s, p, 2, st->pulse ? DRV_HI : DRV_Z);
}
const part_type_t PT_ZX_DIFF = { "47p", "A:T B:O", z8_pulse_eval, 0, 0, sizeof(z8_pulse_t), TECH_TTL };
