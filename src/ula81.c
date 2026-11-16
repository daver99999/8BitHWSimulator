/* ula81.c - the Sinclair ZX81 / Timex Sinclair 1000 ULA (IC1), at its pins.
 *
 * WHERE THIS COMES FROM. There is no netlist of this chip's die. What is written here is its documented
 * behaviour, as described by Andrew Rea's replacement notes (oldcomputer.info), Tom Harte's CLK wiki, Wilf
 * Rigter's notes on the WAIT circuit and the ROM listing's own comments, and it is checked by what matters:
 * the ROM boots, the picture is a picture and the keys work. It is NOT a copy of the gates, and it does not
 * reproduce the exact analog levels of the video signal. What it does:
 *
 * - makes the CPU clock: a 6.5 MHz oscillator (X1, not simulated: it always oscillates) divided by two,
 *   put out on /CLK (pin 14), which the transistor TR2 inverts for the Z80;
 * - decodes the memory: /ROMCS = /MREQ or A14, /RAMCS = /MREQ or not A14 (the board puts 680 ohm
 *   resistors after them, so a RAM pack can override them);
 * - counts 207 CPU clock cycles per line: the horizontal sync is on from cycle 16 to 32 of it, and when the
 *   NMI generator is on (OUT to a port with A0 low; off with A1 low) /NMI goes low with it. The Z80's
 *   interrupt acknowledge restarts the count (the ROM's display routine is a chain of interrupts, one per
 *   scan line, which also advance the 3 bit line counter), any OUT resets the line counter and ends the
 *   vertical sync, which an IN from a port with A0 low starts (while the NMI generator is off);
 * - is the port: IN with A0 low reads D0-D4 = KBD0-KBD4 (the keyboard columns), D5 = 1, D6 = UK/US, D7 = tape;
 * - makes the picture. When the Z80 fetches an opcode (M1) with A15 high and not halted, the ULA latches the
 *   byte (a character of the display file, mirrored at $8000) and, if its bit 6 is clear, pulls the data bus
 *   low (its outputs are open collector, the RAM and ROM are behind 470 ohm resistors) so the CPU executes
 *   a NOP. During the refresh half of the cycle it puts the line counter on A0'-A2' and the character on
 *   A3'-A8': the character ROM (I = $1E, $1E00) answers with the bitmap row, which the ULA loads into a shift
 *   register, put out at 6.5 MHz: ink (bit 1) is black on a white picture, bit 7 of the code inverts it.
 *
 * ASSUMED: the cycle on which the line counter restarts after an interrupt acknowledge (the first cycle),
 * the cycles of the sync pulse (16..31, after Tom Harte), the cycle of the pixels (the picture is drawn as
 * the shift register would put it out, one cycle after the load). The TV/TAPE pin drives a logic level: sync
 * and ink low, paper high. The tape output (the sync generator drives it) is not simulated. */
#include "ula81.h"
#include <string.h>

/* The pins of the 40 pin ULA of the ZX81 / TS1000; other chips with the same logic (the TS1500's gate array) pass their own. */
static const ula81_pins_t ula81_default_pins = {
    .a7p = 1, .a8p = 2, .a2p = 3, .a1p = 4, .a0p = 5, .rd = 6, .iorq = 7, .wr = 8, .mreq = 9, .m1 = 10, .a14 = 11,
    .ramcs = 12, .romcs = 13, .clk = 14, .nmi = 15, .tv = 16, .halt = 17, .a15 = 18, .tape = 20, .ukus = 22,
    .data = { 32, 30, 28, 26, 24, 23, 21, 19 }, .kbd = { 33, 31, 29, 27, 25 }, .alt = { 5, 4, 3, 36, 37, 38, 39, 1, 2 },
};

#define WHITE 0xFFFFFFFFu
#define BLACK 0xFF000000u

bool ula81_hsync_cycle(const ula81_state_t *u) { return !u->vsync && u->cnt >= 16 && u->cnt < 32; }

void ula81_eval(sim_t *s, part_t *p) {
    ula81_state_t *u = p->state;
    const ula81_pins_t *pm = u->pm ? u->pm : &ula81_default_pins;
    bool mreq = !pin_in(s, p, pm->mreq), a14 = pin_in(s, p, pm->a14);
    bool rd = !pin_in(s, p, pm->rd), iorq = !pin_in(s, p, pm->iorq), m1 = !pin_in(s, p, pm->m1);
    pin_out(s, p, pm->romcs, DRV_BIT(!(mreq && !a14)));
    pin_out(s, p, pm->ramcs, DRV_BIT(!(mreq && a14)));
    if (pm->clk) pin_out(s, p, pm->clk, DRV_BIT(pm->clk_true ? u->cpuclk : !u->cpuclk));
    pin_out(s, p, pm->nmi, DRV_BIT(!(u->nmi_en && ula81_hsync_cycle(u))));
    pin_out(s, p, pm->tv, DRV_BIT(!(u->vsync || ula81_hsync_cycle(u)) && !u->ink));
    /* The data bus: open collector outputs, they only pull low. */
    unsigned low = 0;
    bool drive = false;
    if (u->vf == 1 && m1 && rd) { low = 0xFF; drive = true; }                       /* a NOP for the CPU */
    else if (iorq && rd && !m1 && !pin_in(s, p, pm->a0p)) {                           /* IN from port FE (A0 low) */
        unsigned v = 0x20;
        for (int i = 0; i < 5; i++) if (pin_in(s, p, pm->kbd[i])) v |= 1u << i;
        if (pm->ukus && pin_in(s, p, pm->ukus)) v |= 0x40;
        if (pin_in(s, p, pm->tape)) v |= 0x80;
        low = ~v & 0xFF;
        drive = true;
    }
    for (int i = 0; i < 8; i++)
        pin_out(s, p, pm->data[i], !drive ? DRV_Z : pm->push_pull ? DRV_BIT(!((low >> i) & 1)) : ((low >> i) & 1) ? DRV_LO : DRV_Z);
    /* The alternate address lines, driven during the refresh of a video fetch. */
    unsigned alt = (unsigned)(u->line & 7) | (unsigned)(u->code & 0x3F) << 3;
    for (int i = 0; i < 9; i++)
        pin_out(s, p, pm->alt[i], u->vf == 2 ? DRV_BIT((alt >> i) & 1) : DRV_Z);
}

static void ula81_put_pixel(ula81_state_t *u, bool lit) {
    u->ink = lit;
    /* the picture is shifted so that the 256 pixel display sits in the middle of the 414 wide frame */
    int x = ((int)u->cnt * 2 + (u->phase >> 1) + ULA81_PIXELS_PER_LINE - 66) % ULA81_PIXELS_PER_LINE;
    if (!u->fb || u->fy < 0 || u->fy >= ULA81_LINES || x >= ULA81_PIXELS_PER_LINE) return;
    u->fb[u->fy * ULA81_PIXELS_PER_LINE + x] = lit ? BLACK : WHITE;
}

void ula81_step(sim_t *s, part_t *p) {
    ula81_state_t *u = p->state;
    const ula81_pins_t *pm = u->pm ? u->pm : &ula81_default_pins;
    if (p->dead || !u->running) return;
    uint8_t old_vf = u->vf;
    bool old_nmi = u->nmi_en && ula81_hsync_cycle(u), old_ink = u->ink, old_clk = u->cpuclk;
    u->phase = (u->phase + 1) & 3;
    u->cpuclk = u->phase < 2;
    bool m1 = !pin_in(s, p, pm->m1), mreq = !pin_in(s, p, pm->mreq), rd = !pin_in(s, p, pm->rd);
    bool wr = !pin_in(s, p, pm->wr), iorq = !pin_in(s, p, pm->iorq), a15 = pin_in(s, p, pm->a15);
    bool halted = !pin_in(s, p, pm->halt);

    /* The line counter and the horizontal sync, once per CPU clock cycle. */
    if (u->phase == 0) {
        if (u->vsync) u->cnt = 0;
        else if (++u->cnt >= ULA81_CYCLES_PER_LINE) u->cnt = 0;
        if (u->cnt == 16 && !u->vsync) {
            u->fy++;
            for (int x = 0; u->fb && u->fy >= 0 && u->fy < ULA81_LINES && x < ULA81_PIXELS_PER_LINE; x++)
                u->fb[u->fy * ULA81_PIXELS_PER_LINE + x] = WHITE;       /* a fresh line: paper */
        }
    }

    /* Interrupt acknowledge (M1 and IORQ): the line count starts over, the line counter advances. */
    bool ack = m1 && iorq;
    if (ack && !u->ack_prev) {
        u->cnt = 0;
        u->line = (u->line + 1) & 7;
    }
    u->ack_prev = ack;

    /* Ports. */
    bool a0 = pin_in(s, p, pm->a0p), a1 = pin_in(s, p, pm->a1p);
    if (iorq && !m1 && rd && !a0 && !u->nmi_en && !u->vsync) {          /* IN with A0 low: the vertical sync starts */
        u->vsync = true;
        u->cnt = 0;
    }
    if (iorq && !m1 && wr && !u->wr_prev) {                             /* any OUT */
        if (u->vsync) { u->vsync = false; u->cnt = 0; u->fy = -1; u->frames++; }
        u->line = 0;
    }
    if (iorq && !m1 && wr) {
        if (!a0) u->nmi_en = true;
        if (!a1) u->nmi_en = false;
    }
    u->wr_prev = iorq && !m1 && wr;

    /* The video fetch. */
    if (m1 && !u->m1_prev) { u->fetched = false; u->vf = 0; }
    u->m1_prev = m1;
    if (m1 && mreq && rd && !iorq && a15 && !halted && !u->fetched && !u->hold_fetch) {
        u->fetched = true;
        unsigned code = part_read_bus(s, p, pm->data, 8);
        if (!(code & 0x40)) { u->vf = 1; u->code = (uint8_t)(code & 0xBF); }
    } else if (u->vf == 1 && !(m1 && rd)) {
        u->vf = 2;                                                      /* refresh: the ROM address goes out */
    } else if (u->vf == 2 && u->phase == 0) {
        u->shift = pm->bitmap ? pm->bitmap(s) : (uint8_t)part_read_bus(s, p, pm->data, 8);   /* the bitmap row */
        u->inverse = (u->code & 0x80) != 0;
        u->shift_n = 8;
        u->half_pix = 0;
        u->vf = 3;
    }
    u->rd_prev = rd;

    /* The shift register: a pixel every second half period. */
    if (u->shift_n && (u->half_pix++ & 1) == 0) {
        bool lit = ((u->shift & 0x80) != 0) != u->inverse;
        u->shift <<= 1;
        u->shift_n--;
        ula81_put_pixel(u, lit);
    } else if (!u->shift_n) {
        u->ink = false;
    }

    bool nmi = u->nmi_en && ula81_hsync_cycle(u);
    if (old_vf != u->vf || nmi != old_nmi || old_ink != u->ink || old_clk != u->cpuclk || u->phase == 0)
        sim_mark_dirty(s, (int)(p - s->parts));
}

static void ula81_reset(sim_t *s, part_t *p) {
    (void)s;
    ula81_state_t *u = p->state;
    uint32_t *fb = u->fb;
    bool running = u->running;
    const ula81_pins_t *pm = u->pm;
    memset(u, 0, sizeof(*u));
    u->fb = fb; u->running = running; u->pm = pm;
    u->cpuclk = true;
    u->fy = -1;
}

const part_type_t PT_ULA81 = { "ULA 2C184E",
    "A7P:B A8P:B A2P:B A1P:B A0P:B /RD:T /IORQ:T /WR:T /MREQ:T /M1:T A14:T /RAMCS:O /ROMCS:O /CLK:O /NMI:O "
    "TV/TAPE:O /HALT:T A15:T D7:B TAPEIN:T D6:B UK/US:T D5:B D4:B KBD4:T D3:B KBD3:T D2:B KBD2:T D1:B KBD1:T "
    "D0:B KBD0:T GND:P OSC:X A3P:B A4P:B A5P:B A6P:B VCC:P",
    ula81_eval, 0, ula81_reset, sizeof(ula81_state_t), TECH_TTL };
