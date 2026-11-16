/* scld.c - the Timex SCLD (U3) of the TS2068, at its pins.
 *
 * WHERE THIS COMES FROM. Unlike the Spectrum's 6C001 there is no netlist
 * of this chip's die: what is written here is its documented behaviour
 * (Timex Sinclair 2068 Technical Manual, 2.1.8 "Control Logic" and Table
 * 2.1.8-1 "SCLD I/O pin function definitions", 2.1.10, 2.1.13 "Port Map"),
 * and a few things the manual doesn't say, which are assumptions, marked
 * ASSUMED below. The model is checked by what matters: the ROMs boot and
 * the screen is right. It is NOT a copy of the gates, and it does not
 * reproduce the exact contention pattern of a real TS2068.
 *
 * From the manual: a 14.112 MHz crystal, /4 for the 3.528 MHz CPU clock,
 * stopped high while the CPU and the display want the same RAM; the line
 * is 14.112 MHz / 896 (15.75 kHz), 262 lines, 60.1145 Hz; the display
 * reads 32 cells of a line, one RAS with two CAS (bitmap, then attribute)
 * every 1.134 us (4 T states) at 7.056 MHz pixels; INT during the vertical
 * blanking if port FF bit 6 is 0; the ports FF (display), FE (keyboard,
 * tape, border, beep), F4 (the horizontal select register), F5 and F6
 * (the AY-3-8912); the home bank enables: ROMCS (A15 A14 = 00), CASI
 * (01, the display RAM), CAS2 (10), CAS3 (11), unless the horizontal
 * select register selects the chunk (A15-A13) in the dock (ROSCS, port FF
 * bit 7 = 0) or the extension ROM (EXROM, bit 7 = 1) and BE is high.
 *
 * ASSUMED (not in the manual): the position of the INT in the frame
 * (start of the vertical blanking, 32 T states long); which of the 8
 * T state slots the display fetch uses (see 'busy'): the display RAM is
 * busy for the first half of every 4 T state cell and the CPU is held
 * (clock high) so that its RAM cycle fits in the other half; the tick at
 * which a cell appears on the screen (16 ticks after the start of the line
 * as on the Spectrum); the RAM cycle timing of a CPU access (MUX goes to
 * the column 2 half periods after /RAS, CAS 1 half period later).
 *
 * Memory map of the display RAM (the 16K U6/U7, CPU addresses 4000-7FFF):
 * a 4416 has 8 row and 6 column address bits: the row is the low 8 bits
 * of the 14 bit address, the column the high 6. */
#include "scld.h"
#include "ula.h"        /* ula_color() */
#include <string.h>

enum {
    P_A0 = 1, P_A7 = 8, P_A13 = 9, P_PHIBUF = 10, P_EXROM = 11, P_BE = 12, P_OSCO = 13, P_OSCI = 14, P_TAPEIN = 15,
    P_SPKR = 16, P_BC1 = 17, P_BDIR = 18, P_PHIC = 19, P_TS = 20, P_MUX = 21, P_MA7 = 22, P_ROMCS = 27,
    P_ROSCS = 32, P_Y = 33, P_U = 34, P_V = 35, P_RASI = 36, P_DRAMWE = 37, P_ATR = 38, P_CAS3 = 39, P_CAS2 = 40,
    P_CASI = 41, P_CLK = 42, P_A14 = 44, P_A15 = 45, P_BLUE = 46, P_GREEN = 47, P_RED = 48, P_KB0 = 49,
    P_RDDIR = 54, P_RD = 63, P_IORQ = 64, P_WR = 65, P_MREQ = 66, P_INT = 67, P_RFSH = 68
};
static const int addr_pins[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };                 /* A0-A7 */
static const int data_pins[8] = { 57, 55, 56, 58, 61, 59, 60, 62 };        /* D0-D7 (the RAM side of U9) */
static const int ma_pins[8] = { 31, 29, 25, 23, 24, 30, 28, 22 };          /* MA0-MA7 */
static const int kb_pins[5] = { 49, 50, 51, 52, 53 };

/* The addresses of the display RAM (offsets in the 16K) of cell k of pixel line v. */
static unsigned bitmap_addr(unsigned v, unsigned k) {
    return ((v & 0xC0) << 5) | ((v & 7) << 8) | ((v & 0x38) << 2) | k;
}
static unsigned attr_addr(unsigned v, unsigned k) { return 0x1800 | ((v & 0xF8) << 2) | k; }

static int vmode(const scld_state_t *u) { return u->ff & 7; }     /* 0, 1 normal files, 2 hi-colour, 6 hi-res */

static bool on_screen_line(const scld_state_t *u) { return u->v < 192; }

/* What the CPU is doing, from the pins. */
typedef struct {
    bool mreq, iorq, rd, wr, rfsh, a14, a15, be;
    unsigned a13, port;
} cpu_in_t;

static void read_cpu(sim_t *s, part_t *p, cpu_in_t *c) {
    c->mreq = !pin_in(s, p, P_MREQ);
    c->iorq = !pin_in(s, p, P_IORQ);
    c->rd = !pin_in(s, p, P_RD);
    c->wr = !pin_in(s, p, P_WR);
    c->rfsh = !pin_in(s, p, P_RFSH);
    c->a14 = pin_in(s, p, P_A14);
    c->a15 = pin_in(s, p, P_A15);
    c->a13 = (unsigned)pin_in(s, p, P_A13);
    c->be = pin_in(s, p, P_BE);
    c->port = part_read_bus(s, p, addr_pins, 8);
}

/* Which chunk-selected bank the access goes to: 0 home, 1 dock (ROSCS), 2 extension ROM (EXROM). */
static int bank_of(const scld_state_t *u, const cpu_in_t *c) {
    unsigned chunk = (c->a15 << 2) | (c->a14 << 1) | c->a13;
    if (!((u->hsr >> chunk) & 1)) return 0;
    return (u->ff & 0x80) ? 2 : 1;
}

static void scld_eval(sim_t *s, part_t *p) {
    scld_state_t *u = p->state;
    cpu_in_t c;
    read_cpu(s, p, &c);
    bool home_mem = c.mreq && c.be && bank_of(u, &c) == 0;
    bool disp_cpu = home_mem && !c.a15 && c.a14;                    /* the CPU is on the display RAM */
    bool upper_cpu = home_mem && c.a15;
    int bank = c.mreq && c.be ? bank_of(u, &c) : 0;
    /* Chip selects. */
    pin_out(s, p, P_ROMCS, DRV_BIT(!(home_mem && !c.a15 && !c.a14)));
    pin_out(s, p, P_ROSCS, DRV_BIT(!(c.mreq && c.be && bank == 1)));
    pin_out(s, p, P_EXROM, DRV_BIT(!(c.mreq && c.be && bank == 2)));
    /* The CPU's RAM cycle: row, then column. */
    bool cyc = (disp_cpu && !u->busy) || upper_cpu;
    bool col = u->cpu_hs >= 2;
    bool cas_ok = u->cpu_hs >= 3 && (c.rd || c.wr);
    bool refresh = c.mreq && c.rfsh && !c.iorq;
    bool disp_refresh = refresh && !c.a15 && !u->busy;              /* ASSUMED: it refreshes the display RAM too */
    bool cpu_ras = (disp_cpu && !u->busy && !c.rfsh) || disp_refresh;
    pin_out(s, p, P_RASI, DRV_BIT(!(u->f_ras || cpu_ras)));
    pin_out(s, p, P_CASI, DRV_BIT(!(u->f_cas || (disp_cpu && !u->busy && cas_ok && !c.rfsh))));
    pin_out(s, p, P_CAS2, DRV_BIT(!(upper_cpu && !c.a14 && cas_ok && !c.rfsh)));
    pin_out(s, p, P_CAS3, DRV_BIT(!(upper_cpu && c.a14 && cas_ok && !c.rfsh)));
    pin_out(s, p, P_DRAMWE, DRV_BIT(!(disp_cpu && c.wr && !c.rfsh)));
    pin_out(s, p, P_MUX, DRV_BIT(cyc && col));
    /* ATR: the 8th row address bit of the upper RAM: A7 in a cycle, the refresh counter in a refresh. */
    pin_out(s, p, P_ATR, DRV_BIT(c.rfsh ? u->refresh_bit : (c.port >> 7) & 1));
    /* MA0-MA7: the display's while it fetches, a refresh address, else tri-state (U5 puts the CPU's). */
    if (u->f_ae) part_drive_bus(s, p, ma_pins, 8, u->f_addr, true);
    else if (disp_refresh) part_drive_bus(s, p, ma_pins, 8, (c.port & 0x7F) | (unsigned)u->refresh_bit << 7, true);
    else part_drive_bus(s, p, ma_pins, 8, 0, false);
    /* The I/O ports. */
    unsigned port = c.port;
    bool io = c.iorq && (c.rd || c.wr);
    bool ay = io && (port == 0xF5 || port == 0xF6);
    bool port_rd = io && c.rd && !ay && (port == 0xFE || port == 0xFF || port == 0xF4 || port == 0xF5);
    bool port_any = io && !ay && (port == 0xFE || port == 0xFF || port == 0xF4);
    bool tri = disp_cpu && !u->busy;                                  /* U9 and U5 on */
    pin_out(s, p, P_TS, DRV_BIT(!(tri || port_any || (io && port == 0xF5 && c.rd))));
    pin_out(s, p, P_RDDIR, DRV_BIT(pin_in(s, p, P_RD)));
    /* AY: F5 write latches the register (BDIR=BC1=1), F6 write writes (BDIR=1), F6 read reads (BC1=1). */
    bool bdir = io && c.wr && (port == 0xF5 || port == 0xF6);
    bool bc1 = io && ((c.wr && port == 0xF5) || (c.rd && port == 0xF6));
    pin_out(s, p, P_BDIR, DRV_BIT(bdir));
    pin_out(s, p, P_BC1, DRV_BIT(bc1));
    pin_out(s, p, P_PHIC, DRV_BIT(u->ayclk));
    pin_out(s, p, P_CLK, DRV_BIT(u->cpuclk));
    pin_out(s, p, P_PHIBUF, DRV_BIT(u->cpuclk));
    pin_out(s, p, P_INT, u->irq ? DRV_LO : DRV_Z);                    /* open drain, pulled up on the board */
    pin_out(s, p, P_SPKR, DRV_BIT(u->speaker || u->mic));
    /* Colour outputs for an RGB monitor (the colour index of the last pixel). */
    pin_out(s, p, P_BLUE, DRV_BIT(u->pix & 1));
    pin_out(s, p, P_RED, DRV_BIT((u->pix >> 1) & 1));
    pin_out(s, p, P_GREEN, DRV_BIT((u->pix >> 2) & 1));
    /* Data to the CPU through U9: the SCLD's own ports (FE keyboard and tape, FF, F4, F5 = 255). */
    uint8_t out = 0xFF;
    bool drive = false;
    if (port_rd) {
        drive = true;
        if (port == 0xFE) {
            out = 0;                                                  /* D5, D7: "not used (set to 0)" */
            for (int i = 0; i < 5; i++) if (pin_in(s, p, kb_pins[i])) out |= (uint8_t)(1 << i);
            if (u->ear_in || pin_in(s, p, P_TAPEIN)) out |= 0x40;
        } else if (port == 0xFF) out = u->ff;
        else if (port == 0xF4) out = u->hsr;
        else out = 0xFF;
    }
    part_drive_bus(s, p, data_pins, 8, out, drive);
}

static void put_pixel(scld_state_t *u) {
    unsigned px = u->c >= 16 && u->c < 272 ? u->c - 16 : 9999;
    bool in = u->v < 192 && px < 256;
    uint8_t pix[2] = { u->border & 7, u->border & 7 };
    if (in) {
        unsigned k = px >> 3, bit = px & 7;
        int m = vmode(u);
        uint8_t a = u->line_b[k], b = u->line_a[k];
        if (m == 6) {                                                 /* 512 x 192: the first file's byte, then the second's */
            uint8_t ink = (u->ff >> 3) & 7, paper = 7 - ink;
            uint8_t byte = bit < 4 ? u->line_a[k] : u->line_b[k];
            unsigned q = (bit & 3) * 2;
            pix[0] = ((byte >> (7 - q)) & 1) ? ink : paper;
            pix[1] = ((byte >> (6 - q)) & 1) ? ink : paper;
        } else {
            bool inkb = (b >> (7 - bit)) & 1;
            if ((a & 0x80) && ((u->frames >> 4) & 1)) inkb = !inkb;  /* FLASH */
            uint8_t col = (uint8_t)((inkb ? (a & 7) : ((a >> 3) & 7)) | ((a & 0x40) >> 3));
            pix[0] = pix[1] = col;
        }
    }
    u->pix = pix[0];
    int xt = ((int)u->c - 16 + 48 + SCLD_TICKS_PER_LINE) % SCLD_TICKS_PER_LINE;
    int y = u->v < 216 ? (int)u->v + 24 : (int)u->v - 238;
    if (!u->fb || xt >= 352 || y < 0 || y >= SCLD_SCREEN_H) return;
    for (int h = 0; h < 2; h++) {
        uint8_t col = pix[h];
        u->fb[y * SCLD_SCREEN_W + xt * 2 + h] = ula_color(col & 7, col & 8);
    }
}

void scld_step(sim_t *s, part_t *p) {
    scld_state_t *u = p->state;
    if (p->dead || !u->running) return;
    cpu_in_t c;
    read_cpu(s, p, &c);
    /* Time. */
    u->half = (u->half + 1) & 3;
    if (u->half == 0) {
        if (++u->c == SCLD_TICKS_PER_LINE) {
            u->c = 0;
            if (++u->v == SCLD_LINES) { u->v = 0; u->frames++; }
        }
        put_pixel(u);
    }
    /* The display fetch: cell k is fetched in 32 half periods, busy for the first 16 (ASSUMED),
     * which start at half period 4 of the cell (so that the idle 16 begin at a CPU clock fall). */
    unsigned ticks = u->c, hs = u->half;
    bool f_ras = false, f_cas = false, f_ae = false, busy = false;
    uint8_t f_addr = u->f_addr;
    int cellhs = (int)((ticks - 8) * 4 + hs) ;                        /* half periods since the first cell started (cell 0 at tick 8) */
    if (on_screen_line(u) && cellhs >= 0 && cellhs < 32 * SCLD_CELL_HS) {
        unsigned k = (unsigned)cellhs / SCLD_CELL_HS, t = ((unsigned)cellhs % SCLD_CELL_HS + SCLD_CELL_HS - 4) % SCLD_CELL_HS;
        if (t == 31 || t < 16) busy = true;
        /* addresses of this cell */
        unsigned bm = bitmap_addr(u->v, k), at = attr_addr(u->v, k);
        int m = vmode(u);
        unsigned A = bm, B = at;
        if (m == 1) { A = bm + 0x2000; B = at + 0x2000; }
        else if (m == 2 || m == 6) { A = bm; B = bm + 0x2000; }
        unsigned row = A & 0xFF, colA = A >> 8, colB = B >> 8;
        if (t < 16) {
            f_ae = true;
            f_ras = t >= 2;                       /* row at t 0..1, /RAS falls at t 2 */
            f_cas = (t >= 5 && t <= 8) || (t >= 10 && t <= 13);
            f_addr = (uint8_t)(t < 5 ? row : t < 10 ? colA : colB);
            if (t == 8) u->data_a = (uint8_t)part_read_bus(s, p, data_pins, 8);
            if (t == 13) { u->data_b = (uint8_t)part_read_bus(s, p, data_pins, 8); u->line_a[k] = u->data_a; u->line_b[k] = u->data_b; }
        }
    }
    /* Ports. */
    bool io = c.iorq && c.wr;
    if (io) {
        uint8_t d = (uint8_t)part_read_bus(s, p, data_pins, 8);
        if (c.port == 0xFE) {
            bool was = u->speaker || u->mic;
            u->border = d & 7; u->mic = (d >> 3) & 1; u->speaker = (d >> 4) & 1;
            if (was != (u->speaker || u->mic)) sim_mark_dirty(s, (int)(p - s->parts));
        } else if (c.port == 0xFF) u->ff = d;
        else if (c.port == 0xF4) u->hsr = d;
    }
    /* The refresh counter: the 8th bit changes every time the Z80's 7 bit counter wraps. */
    if (c.mreq && c.rfsh && !u->rfsh_prev && (c.port & 0x7F) == 0x7F) u->refresh_bit ^= 1;
    u->rfsh_prev = c.mreq && c.rfsh;
    /* CPU RAM cycle timing. */
    int cpu_hs = u->cpu_hs;
    if (c.mreq) cpu_hs = u->mreq_prev ? cpu_hs + 1 : 0; else cpu_hs = -1;
    u->mreq_prev = c.mreq;
    /* The CPU clock: high 4 half periods, low 4; held high (ASSUMED rule, see the top) while the display
     * busy/idle phase wouldn't let a RAM cycle that starts at the next fall fit. */
    bool held = false;
    uint8_t cphase = u->cphase;
    bool wants = !c.a15 && c.a14;
    if (cphase == 3 && wants && on_screen_line(u)) {
        int next = cellhs + 1;                       /* the clock would fall in the next half period */
        if (next >= 0 && next < 32 * SCLD_CELL_HS) {
            unsigned t = ((unsigned)next % SCLD_CELL_HS + SCLD_CELL_HS - 4) % SCLD_CELL_HS;
            if (t != 16) held = true;                /* the idle half starts at t = 16 */
        } else if (next >= 32 * SCLD_CELL_HS && next < 32 * SCLD_CELL_HS + 32) {
            held = false;
        }
    }
    if (!held) { cphase = (uint8_t)((cphase + 1) & 7); }
    bool cpuclk = cphase < 4;
    u->aphase = (uint8_t)((u->aphase + 1) & 15);
    bool ayclk = u->aphase < 8;
    /* /INT: 32 T states at the start of the vertical blanking (ASSUMED). */
    bool irq = !(u->ff & 0x40) && u->v == 216 && (u->c < 64);
    if (cpuclk != u->cpuclk || held != u->held || f_ras != u->f_ras || f_cas != u->f_cas || f_ae != u->f_ae ||
        f_addr != u->f_addr || busy != u->busy || irq != u->irq || ayclk != u->ayclk || cpu_hs != u->cpu_hs) {
        u->cpuclk = cpuclk; u->held = held; u->f_ras = f_ras; u->f_cas = f_cas; u->f_ae = f_ae;
        u->f_addr = f_addr; u->busy = busy; u->irq = irq; u->ayclk = ayclk; u->cpu_hs = cpu_hs;
        sim_mark_dirty(s, (int)(p - s->parts));
    }
    u->cphase = cphase;
}

static void scld_reset(sim_t *s, part_t *p) {
    (void)s;
    scld_state_t *u = p->state;
    uint32_t *fb = u->fb;
    bool ear = u->ear_in, running = u->running;
    memset(u, 0, sizeof(*u));
    u->fb = fb; u->ear_in = ear; u->running = running;
    u->cpuclk = true; u->cpu_hs = -1;
}

const part_type_t PT_SCLD = { "SCLD",
    "A0:T A1:T A2:T A3:T A4:T A5:T A6:T A7:T A13:T PHI:O /EXROM:O /BE:T OSCOUT:X OSCIN:m TAPEIN:X "
    "SPKR:O BC1:O BDIR:O PHIC:O /TS:O MUX:O MA7:O MA3:O MA4:O MA2:O GND:P /ROMCS:O MA6:O MA1:O MA5:O MA0:O "
    "/ROSCS:O Y:O U:O V:O /RASI:O /DRAMWE:O ATR:O /CAS3:O /CAS2:O /CASI:O CLK:O VCC:P A14:T A15:T BLUE:O "
    "GREEN:O RED:O KB0:T KB1:T KB2:T KB3:T KB4:T /RD*:O D1:X D2:X D0:X D3:X D5:X D6:X D4:X D7:X /RD:T "
    "/IORQ:T /WR:T /MREQ:T /INT:O /RFSH:T",
    scld_eval, 0, scld_reset, sizeof(scld_state_t), TECH_TTL };
