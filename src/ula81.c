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

enum {
    P_A7P = 1, P_A8P = 2, P_A2P = 3, P_A1P = 4, P_A0P = 5, P_RD = 6, P_IORQ = 7, P_WR = 8, P_MREQ = 9, P_M1 = 10,
    P_A14 = 11, P_RAMCS = 12, P_ROMCS = 13, P_CLK = 14, P_NMI = 15, P_TV = 16, P_HALT = 17, P_A15 = 18, P_D7 = 19,
    P_TAPE = 20, P_UKUS = 22, P_KBD4 = 25, P_KBD3 = 27, P_KBD2 = 29, P_KBD1 = 31, P_KBD0 = 33, P_OSC = 35
};
static const int data_pins[8] = { 32, 30, 28, 26, 24, 23, 21, 19 };         /* D0-D7 */
static const int kbd_pins[5] = { 33, 31, 29, 27, 25 };                      /* KBD0-KBD4 */
static const int alt_pins[9] = { 5, 4, 3, 36, 37, 38, 39, 1, 2 };           /* A0'-A8' */

#define WHITE 0xFFFFFFFFu
#define BLACK 0xFF000000u

static bool hsync_cycle(const ula81_state_t *u) { return !u->vsync && u->cnt >= 16 && u->cnt < 32; }

static void ula81_eval(sim_t *s, part_t *p) {
    ula81_state_t *u = p->state;
    bool mreq = !pin_in(s, p, P_MREQ), a14 = pin_in(s, p, P_A14);
    bool rd = !pin_in(s, p, P_RD), iorq = !pin_in(s, p, P_IORQ), m1 = !pin_in(s, p, P_M1);
    pin_out(s, p, P_ROMCS, DRV_BIT(!(mreq && !a14)));
    pin_out(s, p, P_RAMCS, DRV_BIT(!(mreq && a14)));
    pin_out(s, p, P_CLK, DRV_BIT(!u->cpuclk));
    pin_out(s, p, P_NMI, DRV_BIT(!(u->nmi_en && hsync_cycle(u))));
    pin_out(s, p, P_TV, DRV_BIT(!(u->vsync || hsync_cycle(u)) && !u->ink));
    /* The data bus: open collector outputs, they only pull low. */
    unsigned low = 0;
    bool drive = false;
    if (u->vf == 1 && m1 && rd) { low = 0xFF; drive = true; }                       /* a NOP for the CPU */
    else if (iorq && rd && !m1 && !pin_in(s, p, P_A0P)) {                           /* IN from port FE (A0 low) */
        unsigned v = 0x20;
        for (int i = 0; i < 5; i++) if (pin_in(s, p, kbd_pins[i])) v |= 1u << i;
        if (pin_in(s, p, P_UKUS)) v |= 0x40;
        if (pin_in(s, p, P_TAPE)) v |= 0x80;
        low = ~v & 0xFF;
        drive = true;
    }
    for (int i = 0; i < 8; i++)
        pin_out(s, p, data_pins[i], drive && ((low >> i) & 1) ? DRV_LO : DRV_Z);
    /* The alternate address lines, driven during the refresh of a video fetch. */
    unsigned alt = (unsigned)(u->line & 7) | (unsigned)(u->code & 0x3F) << 3;
    for (int i = 0; i < 9; i++)
        pin_out(s, p, alt_pins[i], u->vf == 2 ? DRV_BIT((alt >> i) & 1) : DRV_Z);
}

static void put_pixel(ula81_state_t *u, bool lit) {
    u->ink = lit;
    /* the picture is shifted so that the 256 pixel display sits in the middle of the 414 wide frame */
    int x = ((int)u->cnt * 2 + (u->phase >> 1) + ULA81_PIXELS_PER_LINE - 66) % ULA81_PIXELS_PER_LINE;
    if (!u->fb || u->fy < 0 || u->fy >= ULA81_LINES || x >= ULA81_PIXELS_PER_LINE) return;
    u->fb[u->fy * ULA81_PIXELS_PER_LINE + x] = lit ? BLACK : WHITE;
}

void ula81_step(sim_t *s, part_t *p) {
    ula81_state_t *u = p->state;
    if (p->dead || !u->running) return;
    uint8_t old_vf = u->vf;
    bool old_nmi = u->nmi_en && hsync_cycle(u), old_ink = u->ink, old_clk = u->cpuclk;
    u->phase = (u->phase + 1) & 3;
    u->cpuclk = u->phase < 2;
    bool m1 = !pin_in(s, p, P_M1), mreq = !pin_in(s, p, P_MREQ), rd = !pin_in(s, p, P_RD);
    bool wr = !pin_in(s, p, P_WR), iorq = !pin_in(s, p, P_IORQ), a15 = pin_in(s, p, P_A15);
    bool halted = !pin_in(s, p, P_HALT);

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
    bool a0 = pin_in(s, p, P_A0P), a1 = pin_in(s, p, P_A1P);
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
    if (m1 && mreq && rd && !iorq && a15 && !halted && !u->fetched) {
        u->fetched = true;
        unsigned code = part_read_bus(s, p, data_pins, 8);
        if (!(code & 0x40)) { u->vf = 1; u->code = (uint8_t)(code & 0xBF); }
    } else if (u->vf == 1 && !(m1 && rd)) {
        u->vf = 2;                                                      /* refresh: the ROM address goes out */
    } else if (u->vf == 2 && u->phase == 0) {
        u->shift = (uint8_t)part_read_bus(s, p, data_pins, 8);          /* the bitmap row */
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
        put_pixel(u, lit);
    } else if (!u->shift_n) {
        u->ink = false;
    }

    bool nmi = u->nmi_en && hsync_cycle(u);
    if (old_vf != u->vf || nmi != old_nmi || old_ink != u->ink || old_clk != u->cpuclk || u->phase == 0)
        sim_mark_dirty(s, (int)(p - s->parts));
}

static void ula81_reset(sim_t *s, part_t *p) {
    (void)s;
    ula81_state_t *u = p->state;
    uint32_t *fb = u->fb;
    bool running = u->running;
    memset(u, 0, sizeof(*u));
    u->fb = fb; u->running = running;
    u->cpuclk = true;
    u->fy = -1;
}

const part_type_t PT_ULA81 = { "ULA 2C184E",
    "A7P:B A8P:B A2P:B A1P:B A0P:B /RD:T /IORQ:T /WR:T /MREQ:T /M1:T A14:T /RAMCS:O /ROMCS:O /CLK:O /NMI:O "
    "TV/TAPE:O /HALT:T A15:T D7:B TAPEIN:T D6:B UK/US:T D5:B D4:B KBD4:T D3:B KBD3:T D2:B KBD2:T D1:B KBD1:T "
    "D0:B KBD0:T GND:P OSC:X A3P:B A4P:B A5P:B A6P:B VCC:P",
    ula81_eval, 0, ula81_reset, sizeof(ula81_state_t), TECH_TTL };
