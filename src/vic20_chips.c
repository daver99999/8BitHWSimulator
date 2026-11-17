/* vic20_chips.c - the chips of the Commodore VIC-20 that the other machines
 * don't have: the 6560 video chip (UB7), the 74LS245 bus buffers, the 74LS133
 * (UD9) and the crystal. The 6502 and the 6522s are the PET's (pet_chips.c).
 *
 * The 6560 (NTSC; the 6561 is the PAL twin) makes the VIC-20's clock: the
 * two phases of the 14.31818 MHz crystal come in on its pins 39 and 38 (made
 * by the gates of UB9), it divides them by 14 and puts out phase 1 of the
 * system clock on pin 35. During phase 1 the VIC owns the video bus: it puts
 * an address on A0-A13 and reads D0-D11 at the end of the phase (character
 * codes and colors from the video matrix, then the dots of the character).
 * During phase 2 the buses are the CPU's, and the VIC answers the CPU if it
 * is addressing its 16 registers ($9000-$900F: VA13 low, VA12 high, VA11-VA4
 * low). Everything else (RAM, ROMs, color RAM) is on the board.
 *
 * A step of the board is half a period of the crystal: 28 steps are one
 * cycle (1.0227 MHz), 65 cycles a line, 261 lines a frame (60.3 Hz).
 * The picture is drawn straight into the screen buffer as the data is read:
 * the analog video output (pins 2 and 3) and the sound (pin 19) are not
 * simulated. */
#include "parts.h"
#include <string.h>

/* ------------------------------------------------------------------------ */
/* 74LS245, 74LS133, the crystal                                             */
/* ------------------------------------------------------------------------ */

/* 74LS245: DIR high sends A to B, low B to A; /E high isolates. */
static void ls245_eval(sim_t *s, part_t *p) {
    static const int a[8] = { 2, 3, 4, 5, 6, 7, 8, 9 }, bb[8] = { 18, 17, 16, 15, 14, 13, 12, 11 };
    bool en = !pin_in(s, p, 19), dir = pin_in(s, p, 1);
    if (!en) {
        part_drive_bus(s, p, a, 8, 0, false);
        part_drive_bus(s, p, bb, 8, 0, false);
    } else if (dir) {
        part_drive_bus(s, p, a, 8, 0, false);
        part_drive_bus(s, p, bb, 8, part_read_bus(s, p, a, 8), true);
    } else {
        part_drive_bus(s, p, bb, 8, 0, false);
        part_drive_bus(s, p, a, 8, part_read_bus(s, p, bb, 8), true);
    }
}
const part_type_t PT_74LS245 = { "74LS245",
    "DIR:T A1:X A2:X A3:X A4:X A5:X A6:X A7:X A8:X GND:P B8:X B7:X B6:X B5:X B4:X B3:X B2:X B1:X /E:T VCC:P",
    ls245_eval, 0, 0, 0, TECH_TTL };

/* 74LS133: 13 input NAND. Pins: A B C D E F G GND Y H I J K L M VCC */
static void ls133_eval(sim_t *s, part_t *p) {
    static const int in[13] = { 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15 };
    bool all = true;
    for (int i = 0; i < 13; i++) all = all && pin_in(s, p, in[i]);
    pin_out(s, p, 9, DRV_BIT(!all));
}
const part_type_t PT_74LS133 = { "74LS133",
    "A:T B:T C:T D:T E:T F:T G:T GND:P Y:O H:T I:T J:T K:T L:T M:T VCC:P", ls133_eval, 0, 0, 0, TECH_TTL };

/* The 14.31818 MHz crystal Y1: a clock source on pin 1, the input of the
 * oscillator gate UB9 (the feedback through the other pin is not simulated). */
static void xtal14_eval(sim_t *s, part_t *p) { pin_out(s, p, 1, DRV_BIT(s->clock_level)); }
const part_type_t PT_XTAL14 = { "14.31818 XTAL", "P1:O P2:O", xtal14_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* 6560                                                                      */
/* ------------------------------------------------------------------------ */

enum { V_RW = 4, V_D11 = 5, V_D8 = 8, V_D7 = 9, V_D0 = 16, V_A0 = 21, V_PHI1 = 35, V_DOT2 = 38, V_DOT1 = 39 };
static const int vic_a[14] = { 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34 };
static const int vic_d[8] = { 16, 15, 14, 13, 12, 11, 10, 9 };      /* D0..D7 */
static const int vic_vd[4] = { 8, 7, 6, 5 };                        /* D8..D11 */

#define VIC_PERIODS 14      /* crystal periods in a cycle */
#define VIC_CYCLES  65      /* cycles in a line */
#define VIC_LINES   261
#define WIN_X0      5       /* the part of the raster the monitor shows: from this cycle, */
#define WIN_CYCLES  56      /* this many cycles (4 dots each), */
#define WIN_Y0      26      /* from this line, */
#define WIN_LINES   232     /* this many lines */

typedef struct {
    uint8_t reg[16];
    uint8_t clk;                /* pin 39 at the last step */
    uint8_t period;             /* 0..13: the crystal period of the cycle */
    uint8_t phi1;               /* phase 1 (the VIC's) */
    uint8_t fetch;              /* the read of this cycle: 0 none, 1 video matrix, 2 character */
    uint8_t sampled, code, color, col;
    uint8_t sub;                /* dot line of the character */
    uint8_t out_en, out_data;   /* a register is being read */
    uint16_t addr;              /* address on the bus in phase 1 */
    uint16_t cycle, line;
    int32_t cache_a, cache_d;
    uint32_t frame;
    uint32_t *screen;           /* VIC_W x VIC_H, set by the machine */
} vic_t;

#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (r))
static const uint32_t vic_pal[16] = {
    RGB(0x00, 0x00, 0x00), RGB(0xFF, 0xFF, 0xFF), RGB(0x78, 0x29, 0x22), RGB(0x87, 0xD6, 0xDD),
    RGB(0xAA, 0x5F, 0xB6), RGB(0x55, 0xA0, 0x49), RGB(0x40, 0x31, 0x8D), RGB(0xBF, 0xCE, 0x72),
    RGB(0xAA, 0x74, 0x49), RGB(0xEA, 0xB4, 0x89), RGB(0xB8, 0x69, 0x62), RGB(0xC7, 0xFF, 0xFF),
    RGB(0xEA, 0x9F, 0xF6), RGB(0x94, 0xE0, 0x89), RGB(0x80, 0x71, 0xCC), RGB(0xFF, 0xFF, 0xB0),
};

/* The registers at a read (the 6560's data sheet): the raster is readable,
 * the light pen and the paddles are not connected. */
static uint8_t vic_read_reg(const vic_t *v, int r) {
    switch (r) {
    case 3: return (uint8_t)((v->reg[3] & 0x7F) | ((v->line & 1) << 7));
    case 4: return (uint8_t)(v->line >> 1);
    case 6: case 7: return 0;
    case 8: case 9: return 0xFF;
    default: return v->reg[r];
    }
}

static void vic_plot(vic_t *v, int cycle_x, int line, const uint32_t px[8]) {
    int y = line - WIN_Y0;
    if (!v->screen || y < 0 || y >= WIN_LINES) return;
    int x = (cycle_x - WIN_X0) * 4;
    for (int i = 0; i < 8; i++)
        if (x + i >= 0 && x + i < VIC_W) v->screen[y * VIC_W + x + i] = px[i];
}

/* The character fetched in the last two cycles: its 8 dots. */
static void vic_draw_char(vic_t *v, int k, uint8_t pat) {
    uint32_t px[8];
    uint8_t aux = v->reg[14] >> 4, bg = v->reg[15] >> 4, border = v->reg[15] & 7;
    bool normal = (v->reg[15] & 8) != 0;
    int fg = v->color & 7;
    if (!normal) {                                  /* reverse: the dots set show the background */
        for (int i = 0; i < 8; i++) px[i] = vic_pal[(pat >> (7 - i)) & 1 ? bg : fg];
    } else if (v->color & 8) {                      /* multicolor: pairs of dots */
        for (int i = 0; i < 4; i++) {
            int c = (pat >> (6 - 2 * i)) & 3;
            int col = c == 0 ? bg : c == 1 ? border : c == 2 ? fg : aux;
            px[2 * i] = px[2 * i + 1] = vic_pal[col];
        }
    } else {
        for (int i = 0; i < 8; i++) px[i] = vic_pal[(pat >> (7 - i)) & 1 ? fg : bg];
    }
    vic_plot(v, (v->reg[0] & 0x7F) + 4 + 2 * k + 2, v->line, px);
}

/* The start of a cycle: where we are in the raster, what to read. */
static void vic_begin_cycle(vic_t *v) {
    if (++v->cycle == VIC_CYCLES) {
        v->cycle = 0;
        if (++v->line == VIC_LINES) { v->line = 0; v->frame++; }
    }
    if (v->cycle == 0) {            /* a new line: the border color everywhere, the characters come on top */
        int y = v->line - WIN_Y0;
        if (v->screen && y >= 0 && y < WIN_LINES)
            for (int x = 0; x < VIC_W; x++) v->screen[y * VIC_W + x] = vic_pal[v->reg[15] & 7];
    }
    int cols = v->reg[2] & 0x7F, rows = (v->reg[3] >> 1) & 0x3F, h = (v->reg[3] & 1) ? 16 : 8;
    int y0 = v->reg[1] * 2, rel = v->cycle - ((v->reg[0] & 0x7F) + 4);
    v->fetch = 0;
    v->addr = 0x3FFF;               /* idle */
    if (v->line >= y0 && v->line < y0 + rows * h && rel >= 0 && rel < cols * 2) {
        int row = (v->line - y0) / h, k = rel >> 1;
        v->col = (uint8_t)k;
        v->sub = (uint8_t)((v->line - y0) % h);
        if (!(rel & 1)) {
            v->fetch = 1;
            v->addr = (uint16_t)((((v->reg[5] & 0xF0) << 6) | ((v->reg[2] & 0x80) << 2)) + row * cols + k) & 0x3FFF;
        } else {
            v->fetch = 2;
            v->addr = (uint16_t)(((v->reg[5] & 0x0F) << 10) + v->code * h + v->sub) & 0x3FFF;
        }
    }
}

static void vic_eval(sim_t *s, part_t *p) {
    vic_t *v = p->state;
    pin_out(s, p, V_PHI1, DRV_BIT(v->phi1));
    part_drive_bus_cached(s, p, vic_a, 14, v->addr, v->phi1, &v->cache_a);
    part_drive_bus_cached(s, p, vic_d, 8, v->out_data, v->out_en && !v->phi1, &v->cache_d);
}

static void vic_commit(sim_t *s, part_t *p) {
    vic_t *v = p->state;
    if (p->dead) return;
    int part = (int)(p - s->parts), clk = pin_in(s, p, V_DOT1);
    bool dirty = false;
    if (clk && !v->clk) {                           /* a crystal period */
        v->period = (uint8_t)((v->period + 1) % VIC_PERIODS);
        if (v->period == 0) { vic_begin_cycle(v); v->phi1 = 1; dirty = true; }
        else if (v->period == 7) { v->phi1 = 0; dirty = true; }
    }
    if (v->phi1) {
        /* the end of phase 1 (the second half of the last period): read what the memory answers */
        if (v->period == 6 && !clk && v->fetch) {
            unsigned d = part_read_bus(s, p, vic_d, 8) | part_read_bus(s, p, vic_vd, 4) << 8;
            if (v->fetch == 1) { v->code = (uint8_t)d; v->color = (uint8_t)(d >> 8); }
            else vic_draw_char(v, v->col, (uint8_t)d);
            v->fetch = 0;
        }
    } else {
        /* phase 2: is the CPU talking to a register? */
        unsigned a = part_read_bus(s, p, vic_a, 14);
        bool sel = !(a & 0x2000) && (a & 0x1000) && !(a & 0x0FF0);
        bool rw = pin_in(s, p, V_RW);
        uint8_t en = (uint8_t)(sel && rw), data = en ? vic_read_reg(v, (int)(a & 15)) : 0;
        if (en != v->out_en || data != v->out_data) { v->out_en = en; v->out_data = data; dirty = true; }
        if (sel && !rw && v->period == 13 && !clk)  /* the write, at the end of phase 2 */
            v->reg[a & 15] = (uint8_t)part_read_bus(s, p, vic_d, 8);
    }
    v->clk = (uint8_t)clk;
    if (dirty) sim_mark_dirty(s, part);
}

static void vic_reset(sim_t *s, part_t *p) {
    (void)s;
    vic_t *v = p->state;
    uint32_t *screen = v->screen;
    memset(v, 0, sizeof(*v));
    v->screen = screen;
    v->line = VIC_LINES - 1;
    v->cycle = VIC_CYCLES - 1;
}

const part_type_t PT_6560 = { "6560",
    "NC:O CHROMA:O LUMA:O R/W:m D11:m D10:m D9:m D8:m D7:B D6:B D5:B D4:B D3:B D2:B D1:B D0:B POTX:O POTY:O "
    "SOUND:O VSS:P A0:B A1:B A2:B A3:B A4:B A5:B A6:B A7:B A8:B A9:B A10:B A11:B A12:B A13:B PHI1:O NC:O "
    "LPEN:m DOT2:M DOT1:M VCC:P",
    vic_eval, vic_commit, vic_reset, sizeof(vic_t), TECH_NMOS };

void vic_set_screen(part_t *p, uint32_t *screen) { ((vic_t *)p->state)->screen = screen; }
uint8_t vic_reg_of(const part_t *p, int r) { return ((const vic_t *)p->state)->reg[r & 15]; }
uint32_t vic_frame_of(const part_t *p) { return ((const vic_t *)p->state)->frame; }
