/* apple2_chips.c - the chips of the Apple II+ board that the other machines do not have: pin level models of the
 * TTL parts that make its clock, video and decoding, the 2K ROMs, the character generator, the keyboard
 * connector (the key encoder), the game connector's timers and the transistors that are logic here. */
#include "parts.h"
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Gates                                                                     */
/* ------------------------------------------------------------------------ */

/* 74LS11: three 3-input AND gates. */
static void ls11_eval(sim_t *s, part_t *p) {
    static const int g[3][4] = { { 1, 2, 13, 12 }, { 3, 4, 5, 6 }, { 9, 10, 11, 8 } };
    for (int i = 0; i < 3; i++)
        pin_out(s, p, g[i][3], DRV_BIT(pin_in(s, p, g[i][0]) && pin_in(s, p, g[i][1]) && pin_in(s, p, g[i][2])));
}
const part_type_t PT_74LS11 = { "74LS11",
    "1A:T 1B:T 2A:T 2B:T 2C:T 2Y:O GND:P 3Y:O 3A:T 3B:T 3C:T 1Y:O 1C:T VCC:P", ls11_eval, 0, 0, 0, TECH_TTL };

/* 74LS51: two AND-OR-INVERT gates: Y1 = !(1A 1B 1C + 1D 1E 1F), Y2 = !(2A 2B + 2C 2D). The first is pins 1, 13, 12 and 9, 10, 11 (output 8),
 * the second 2, 3 and 4, 5 (output 6), as the Apple II+ schematic draws it. */
static void ls51_eval(sim_t *s, part_t *p) {
    bool a = pin_in(s, p, 1) && pin_in(s, p, 13) && pin_in(s, p, 12);
    bool b = pin_in(s, p, 9) && pin_in(s, p, 10) && pin_in(s, p, 11);
    pin_out(s, p, 8, DRV_BIT(!(a || b)));
    pin_out(s, p, 6, DRV_BIT(!((pin_in(s, p, 2) && pin_in(s, p, 3)) || (pin_in(s, p, 4) && pin_in(s, p, 5)))));
}
const part_type_t PT_74LS51 = { "74LS51",
    "1A:T 2A:T 2B:T 2C:T 2D:T 2Y:O GND:P 1Y:O 1D:T 1E:T 1F:T 1C:T 1B:T VCC:P", ls51_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* Multiplexers, adder                                                       */
/* ------------------------------------------------------------------------ */

/* 74LS153: two 4-to-1 multiplexers with common select S0 (14), S1 (2); a high /E makes the output low. */
static void ls153_eval(sim_t *s, part_t *p) {
    int sel = pin_in(s, p, 14) | (pin_in(s, p, 2) << 1);
    static const int a[4] = { 6, 5, 4, 3 }, b[4] = { 10, 11, 12, 13 };
    pin_out(s, p, 7, DRV_BIT(!pin_in(s, p, 1) && pin_in(s, p, a[sel])));
    pin_out(s, p, 9, DRV_BIT(!pin_in(s, p, 15) && pin_in(s, p, b[sel])));
}
const part_type_t PT_74LS153 = { "74LS153",
    "/1E:T S1:T 1I3:T 1I2:T 1I1:T 1I0:T 1Z:O GND:P 2Z:O 2I0:T 2I1:T 2I2:T 2I3:T S0:T /2E:T VCC:P", ls153_eval, 0, 0, 0, TECH_TTL };

/* 74LS151: 8-to-1 multiplexer with a low active enable; Z (5) and /Z (6). 74LS251: the same with three-state outputs
 * (pin 7 is the output enable, low active). */
static const int mux8_in[8] = { 4, 3, 2, 1, 15, 14, 13, 12 };
static void ls151_eval(sim_t *s, part_t *p) {
    int sel = pin_in(s, p, 11) | (pin_in(s, p, 10) << 1) | (pin_in(s, p, 9) << 2);
    bool z = !pin_in(s, p, 7) && pin_in(s, p, mux8_in[sel]);
    pin_out(s, p, 5, DRV_BIT(z));
    pin_out(s, p, 6, DRV_BIT(!z));
}
static void ls251_eval(sim_t *s, part_t *p) {
    int sel = pin_in(s, p, 11) | (pin_in(s, p, 10) << 1) | (pin_in(s, p, 9) << 2);
    bool en = !pin_in(s, p, 7), z = pin_in(s, p, mux8_in[sel]);
    pin_out(s, p, 5, en ? DRV_BIT(z) : DRV_Z);
    pin_out(s, p, 6, en ? DRV_BIT(!z) : DRV_Z);
}
#define MUX8_PINS(en) "I3:T I2:T I1:T I0:T Z:O /Z:O " en ":T GND:P S2:T S1:T S0:T I7:T I6:T I5:T I4:T VCC:P"
const part_type_t PT_74LS151 = { "74LS151", MUX8_PINS("/E"), ls151_eval, 0, 0, 0, TECH_TTL };
const part_type_t PT_74LS251 = { "74LS251", MUX8_PINS("/OE"), ls251_eval, 0, 0, 0, TECH_TTL };

/* 74LS283: 4 bit adder. */
static void ls283_eval(sim_t *s, part_t *p) {
    static const int a[4] = { 5, 3, 14, 12 }, b[4] = { 6, 2, 15, 11 }, sum[4] = { 4, 1, 13, 10 };
    unsigned x = 0, y = 0;
    for (int i = 0; i < 4; i++) { x |= (unsigned)pin_in(s, p, a[i]) << i; y |= (unsigned)pin_in(s, p, b[i]) << i; }
    unsigned r = x + y + (unsigned)pin_in(s, p, 7);
    for (int i = 0; i < 4; i++) pin_out(s, p, sum[i], DRV_BIT((r >> i) & 1));
    pin_out(s, p, 9, DRV_BIT((r >> 4) & 1));
}
const part_type_t PT_74LS283 = { "74LS283",
    "S2:O B2:T A2:T S1:O A1:T B1:T C0:T GND:P C4:O S4:O B4:T A4:T S3:O A3:T B3:T VCC:P", ls283_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* Registers and counters                                                    */
/* ------------------------------------------------------------------------ */

/* 74LS174: six D flip-flops with a common clock (9) and clear (1). */
typedef struct { uint8_t q[6], ck; } ls174_t;
static const int ls174_d[6] = { 3, 4, 6, 11, 13, 14 }, ls174_q[6] = { 2, 5, 7, 10, 12, 15 };
static void ls174_eval(sim_t *s, part_t *p) {
    const ls174_t *t = p->state;
    for (int i = 0; i < 6; i++) pin_out(s, p, ls174_q[i], DRV_BIT(t->q[i] && pin_in(s, p, 1)));
}
static void ls174_commit(sim_t *s, part_t *p) {
    ls174_t *t = p->state;
    uint8_t q[6];
    memcpy(q, t->q, 6);
    int ck = pin_in(s, p, 9);
    if (!pin_in(s, p, 1)) memset(q, 0, 6);
    else if (ck && !t->ck) for (int i = 0; i < 6; i++) q[i] = (uint8_t)pin_in(s, p, ls174_d[i]);
    t->ck = (uint8_t)ck;
    if (memcmp(q, t->q, 6)) { memcpy(t->q, q, 6); sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void ls174_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls174_t)); }
const part_type_t PT_74LS174 = { "74LS174",
    "/CLR:T 1Q:O 1D:T 2D:T 2Q:O 3D:T 3Q:O GND:P CLK:m 4Q:O 4D:T 5Q:O 5D:T 6D:T 6Q:O VCC:P",
    ls174_eval, ls174_commit, ls174_reset, sizeof(ls174_t), TECH_TTL };

/* 74LS161: synchronous 4 bit counter, asynchronous clear (1), synchronous load (9, low). TC (15) is CET and Q = 15. */
typedef struct { uint8_t q, ck; } ls161_t;
static void ls161_eval(sim_t *s, part_t *p) {
    const ls161_t *t = p->state;
    uint8_t q = pin_in(s, p, 1) ? t->q : 0;
    pin_out(s, p, 14, DRV_BIT(q & 1));
    pin_out(s, p, 13, DRV_BIT((q >> 1) & 1));
    pin_out(s, p, 12, DRV_BIT((q >> 2) & 1));
    pin_out(s, p, 11, DRV_BIT((q >> 3) & 1));
    pin_out(s, p, 15, DRV_BIT(q == 15 && pin_in(s, p, 10)));
}
static void ls161_commit(sim_t *s, part_t *p) {
    ls161_t *t = p->state;
    uint8_t q = t->q;
    int ck = pin_in(s, p, 2);
    if (!pin_in(s, p, 1)) q = 0;
    else if (ck && !t->ck) {
        if (!pin_in(s, p, 9)) q = (uint8_t)(pin_in(s, p, 3) | (pin_in(s, p, 4) << 1) | (pin_in(s, p, 5) << 2) | (pin_in(s, p, 6) << 3));
        else if (pin_in(s, p, 7) && pin_in(s, p, 10)) q = (uint8_t)((q + 1) & 15);
    }
    t->ck = (uint8_t)ck;
    if (q != t->q) { t->q = q; sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void ls161_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls161_t)); }
const part_type_t PT_74LS161 = { "74LS161",
    "/MR:T CP:m D0:T D1:T D2:T D3:T CEP:T GND:P /PE:T CET:T Q3:O Q2:O Q1:O Q0:O TC:O VCC:P",
    ls161_eval, ls161_commit, ls161_reset, sizeof(ls161_t), TECH_TTL };

/* 74LS194: 4 bit bidirectional universal shift register. S1 S0: 00 hold, 01 right (DSR in, Q0 to Q1...), 10 left, 11 load. */
typedef struct { uint8_t q, ck; } ls194_t;
static void ls194_eval(sim_t *s, part_t *p) {
    const ls194_t *t = p->state;
    uint8_t q = pin_in(s, p, 1) ? t->q : 0;
    pin_out(s, p, 15, DRV_BIT(q & 1));
    pin_out(s, p, 14, DRV_BIT((q >> 1) & 1));
    pin_out(s, p, 13, DRV_BIT((q >> 2) & 1));
    pin_out(s, p, 12, DRV_BIT((q >> 3) & 1));
}
static void ls194_commit(sim_t *s, part_t *p) {
    ls194_t *t = p->state;
    uint8_t q = t->q;
    int ck = pin_in(s, p, 11);
    if (!pin_in(s, p, 1)) q = 0;
    else if (ck && !t->ck) {
        switch (pin_in(s, p, 9) | (pin_in(s, p, 10) << 1)) {
        case 1: q = (uint8_t)(((q << 1) | pin_in(s, p, 2)) & 15); break;
        case 2: q = (uint8_t)((q >> 1) | (pin_in(s, p, 7) << 3)); break;
        case 3: q = (uint8_t)(pin_in(s, p, 3) | (pin_in(s, p, 4) << 1) | (pin_in(s, p, 5) << 2) | (pin_in(s, p, 6) << 3)); break;
        default: break;
        }
    }
    t->ck = (uint8_t)ck;
    if (q != t->q) { t->q = q; sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void ls194_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls194_t)); }
const part_type_t PT_74LS194 = { "74LS194",
    "/MR:T DSR:T P0:T P1:T P2:T P3:T DSL:T GND:P S0:T S1:T CP:m Q3:O Q2:O Q1:O Q0:O VCC:P",
    ls194_eval, ls194_commit, ls194_reset, sizeof(ls194_t), TECH_TTL };

/* 74LS195: 4 bit parallel access shift register: /PE low loads P0-P3, high shifts right; the first stage is a JK (J, /K). */
static void ls195_eval(sim_t *s, part_t *p) {
    const ls194_t *t = p->state;
    uint8_t q = pin_in(s, p, 1) ? t->q : 0;
    pin_out(s, p, 15, DRV_BIT(q & 1));
    pin_out(s, p, 14, DRV_BIT((q >> 1) & 1));
    pin_out(s, p, 13, DRV_BIT((q >> 2) & 1));
    pin_out(s, p, 12, DRV_BIT((q >> 3) & 1));
    pin_out(s, p, 11, DRV_BIT(!((q >> 3) & 1)));
}
static void ls195_commit(sim_t *s, part_t *p) {
    ls194_t *t = p->state;
    uint8_t q = t->q;
    int ck = pin_in(s, p, 10);
    if (!pin_in(s, p, 1)) q = 0;
    else if (ck && !t->ck) {
        if (!pin_in(s, p, 9)) q = (uint8_t)(pin_in(s, p, 4) | (pin_in(s, p, 5) << 1) | (pin_in(s, p, 6) << 2) | (pin_in(s, p, 7) << 3));
        else {
            int q0 = q & 1, j = pin_in(s, p, 2), nk = pin_in(s, p, 3);
            q = (uint8_t)(((q << 1) & 14) | ((j && !q0) || (nk && q0)));
        }
    }
    t->ck = (uint8_t)ck;
    if (q != t->q) { t->q = q; sim_mark_dirty(s, (int)(p - s->parts)); }
}
const part_type_t PT_74LS195 = { "74LS195",
    "/MR:T J:T /K:T P0:T P1:T P2:T P3:T GND:P /PE:T CP:m /Q3:O Q3:O Q2:O Q1:O Q0:O VCC:P",
    ls195_eval, ls195_commit, ls194_reset, sizeof(ls194_t), TECH_TTL };

/* 74LS259: 8 bit addressable latch. /E low: the addressed bit follows D; /E high: hold; Clr low clears (with /E low the others stay clear). */
typedef struct { uint8_t q; } ls259_t;
static void ls259_eval(sim_t *s, part_t *p) {
    static const int q[8] = { 4, 5, 6, 7, 9, 10, 11, 12 };
    const ls259_t *t = p->state;
    for (int i = 0; i < 8; i++) pin_out(s, p, q[i], DRV_BIT((t->q >> i) & 1));
}
static void ls259_commit(sim_t *s, part_t *p) {
    ls259_t *t = p->state;
    uint8_t q = t->q;
    int a = pin_in(s, p, 1) | (pin_in(s, p, 2) << 1) | (pin_in(s, p, 3) << 2), e = !pin_in(s, p, 14), d = pin_in(s, p, 13);
    if (!pin_in(s, p, 15)) { q = 0; if (e && d) q = (uint8_t)(1 << a); }
    else if (e) q = (uint8_t)((q & ~(1 << a)) | (d << a));
    if (q != t->q) { t->q = q; sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void ls259_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(ls259_t)); }
const part_type_t PT_74LS259 = { "74LS259",
    "A0:T A1:T A2:T Q0:O Q1:O Q2:O Q3:O GND:P Q4:O Q5:O Q6:O Q7:O D:T /E:T /CLR:T VCC:P",
    ls259_eval, ls259_commit, ls259_reset, sizeof(ls259_t), TECH_TTL };

/* ------------------------------------------------------------------------ */
/* Buffers                                                                   */
/* ------------------------------------------------------------------------ */

/* 8T97: six three-state buffers; a high DIS1 (1) or DIS2 (15) turns them off (the Apple ties both to /DMA's inverse). */
static void t97_eval(sim_t *s, part_t *p) {
    static const int io[6][2] = { { 2, 3 }, { 4, 5 }, { 6, 7 }, { 10, 9 }, { 12, 11 }, { 14, 13 } };
    bool off = pin_in(s, p, 1) || pin_in(s, p, 15);
    for (int i = 0; i < 6; i++) pin_out(s, p, io[i][1], off ? DRV_Z : DRV_BIT(pin_in(s, p, io[i][0])));
}
const part_type_t PT_8T97 = { "8T97",
    "DIS1:T I1:T Y1:O I2:T Y2:O I3:T Y3:O GND:P Y4:O I4:T Y5:O I5:T Y6:O I6:T DIS2:T VCC:P", t97_eval, 0, 0, 0, TECH_TTL };

/* DP8304: octal bus transceiver, A0-A7 on pins 1-8 and B7-B0 on 12-19. CE low enables; T/R high sends A to B (the
 * Apple's CPU data bus is the B side, the board's data bus the A side, and T/R is the CPU's R/W line: a read goes to the CPU). */
static void dp8304_eval(sim_t *s, part_t *p) {
    static const int a[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, b[8] = { 19, 18, 17, 16, 15, 14, 13, 12 };
    bool en = !pin_in(s, p, 9), toa = !pin_in(s, p, 11);
    if (!en) {
        part_drive_bus(s, p, a, 8, 0, false);
        part_drive_bus(s, p, b, 8, 0, false);
    } else if (toa) {
        part_drive_bus(s, p, b, 8, 0, false);
        part_drive_bus(s, p, a, 8, part_read_bus(s, p, b, 8), true);
    } else {
        part_drive_bus(s, p, a, 8, 0, false);
        part_drive_bus(s, p, b, 8, part_read_bus(s, p, a, 8), true);
    }
}
const part_type_t PT_DP8304 = { "DP8304",
    "A0:X A1:X A2:X A3:X A4:X A5:X A6:X A7:X CE:T GND:P T/R:T B7:X B6:X B5:X B4:X B3:X B2:X B1:X B0:X VCC:P",
    dp8304_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------ */

/* The six 2K ROMs of $D000-$FFFF (24 pins as a 2716: A0-A10, D0-D7, CS2 (18, high), /CS1 (20) and /CS3 (21)). */
static const int a2rom_a[11] = { 8, 7, 6, 5, 4, 3, 2, 1, 23, 22, 19 };
static const int a2rom_d[8] = { 9, 10, 11, 13, 14, 15, 16, 17 };
static void a2rom_eval(sim_t *s, part_t *p) {
    rom_state_t *st = p->state;
    bool sel = pin_in(s, p, 18) && !pin_in(s, p, 20) && !pin_in(s, p, 21);
    unsigned a = sel ? part_read_bus(s, p, a2rom_a, 11) : 0;
    part_drive_bus_cached(s, p, a2rom_d, 8, st->data ? st->data[a] : 0xFF, sel, &st->out_cache);
}
const part_type_t PT_A2ROM = { "2316 ROM",
    "A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O D7:O CS2:M /CS1:M A10:M /CS3:M A9:M A8:M VCC:P",
    a2rom_eval, 0, 0, sizeof(rom_state_t), TECH_NMOS };

/* The character generator (2716 pins; /CE 18 and /OE 20 low; pin 21 is not drawn on the schematic). */
static void a2chr_eval(sim_t *s, part_t *p) {
    rom_state_t *st = p->state;
    bool sel = !pin_in(s, p, 18) && !pin_in(s, p, 20);
    unsigned a = sel ? part_read_bus(s, p, a2rom_a, 11) : 0;
    part_drive_bus_cached(s, p, a2rom_d, 8, st->data ? st->data[a] : 0xFF, sel, &st->out_cache);
}
const part_type_t PT_A2CHR = { "2716 CHAR",
    "A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M Q0:O Q1:O Q2:O VDD:P Q3:O Q4:O Q5:O Q6:O Q7:O /CE:M A10:M /OE:M VPP:m A9:M A8:M VCC:P",
    a2chr_eval, 0, 0, sizeof(rom_state_t), TECH_NMOS };

/* ------------------------------------------------------------------------ */
/* Keyboard connector                                                        */
/* ------------------------------------------------------------------------ */

/* The keyboard is not on the board: its connector (A7) takes the encoder's seven data lines and the strobe. The machine
 * puts the key in the state; the pin 2 strobe goes high for a while with the data, which stay until the next key. */
typedef struct { uint8_t data, strobe, reset; } kbc_t;
void apple2_kbc_set(part_t *p, int data, int strobe, int reset) {
    kbc_t *t = p->state;
    if (data >= 0) t->data = (uint8_t)data;
    if (strobe >= 0) t->strobe = (uint8_t)strobe;
    if (reset >= 0) t->reset = (uint8_t)reset;
}
static void kbc_eval(sim_t *s, part_t *p) {
    static const int d[7] = { 12, 13, 10, 11, 6, 5, 7 };      /* KBD_Data0-6 */
    const kbc_t *t = p->state;
    for (int i = 0; i < 7; i++) pin_out(s, p, d[i], DRV_BIT((t->data >> i) & 1));
    pin_out(s, p, 2, DRV_BIT(t->strobe));
    pin_out(s, p, 3, t->reset ? DRV_LO : DRV_Z);               /* the RESET key pulls /RESET low */
    pin_out(s, p, 4, DRV_Z);                                    /* the CTRL and SHIFT pins are not wired on the board */
    pin_out(s, p, 9, DRV_Z);
}
static void kbc_reset(sim_t *s, part_t *p) { (void)s; memset(p->state, 0, sizeof(kbc_t)); }
const part_type_t PT_A2KBD = { "KEYBOARD",
    "+5V:P STROBE:O /RESET:O CTRL:O D5:O D4:O D6:O GND:P SHIFT:O D2:O D3:O D0:O D1:O N14:O -12V:P N16:O",
    kbc_eval, 0, kbc_reset, sizeof(kbc_t), TECH_TTL };

/* ------------------------------------------------------------------------ */
/* Timers, transistors, the cassette comparator                              */
/* ------------------------------------------------------------------------ */

/* NE558 (the game connector's paddle timers): no paddles are plugged in, so the timing capacitors are never discharged and
 * the outputs stay high (PDL returns 255). */
static void ne558_eval(sim_t *s, part_t *p) {
    static const int out[4] = { 1, 8, 9, 16 };
    for (int i = 0; i < 4; i++) pin_out(s, p, out[i], DRV_HI);
}
const part_type_t PT_NE558 = { "NE558",
    "OutA:O TimA:O TrA:T CV:O VCC:P TrB:T TimB:O OutB:O OutC:O TimC:O TrC:T GND:P /RES:T TrD:T TimD:O OutD:O",
    ne558_eval, 0, 0, 0, TECH_TTL };

/* The 555 that makes the flashing of the characters (about 2 Hz: 3.3M and 12K with 0.1 uF): a square wave, 0.23 s each half.
 * Pin 3 is the output. */
#define A2_FLASH_HALF 235000
static void flash555_eval(sim_t *s, part_t *p) { pin_out(s, p, 3, DRV_BIT((s->cycle / A2_FLASH_HALF) & 1)); }
static void flash555_commit(sim_t *s, part_t *p) {
    if (s->cycle % A2_FLASH_HALF == 0) sim_mark_dirty(s, (int)(p - s->parts));
}
const part_type_t PT_A2FLASH = { "555 FLASH",
    "GND:P TRIG:m OUT:O RESET:m CTRL:m THR:m DIS:m VCC:P", flash555_eval, flash555_commit, 0, 0, TECH_TTL };

/* An NPN switch (2N3904: pins 1 E, 2 B, 3 C) that is logic here: the collector is pulled low while the base is high. */
static void npn_eval(sim_t *s, part_t *p) { pin_out(s, p, 3, pin_in(s, p, 2) ? DRV_LO : DRV_Z); }
const part_type_t PT_A2NPN = { "2N3904", "E:O B:T C:O", npn_eval, 0, 0, 0, TECH_TTL };
