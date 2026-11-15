#pragma once
/*#
    # m6545.h

    Rockwell/MOS 6545 CRT Controller (CRTC), the display controller of the
    Commodore PET 4000/8000 series (UB13 on the 8032080 board).

    Written for the PET additions to c64-hw-sim. m6545_tick() is called once per
    character clock (CLK, 1 MHz on the PET). The CPU bus access (CS, RS, RW, data)
    is sampled in the same call, which matches the PET where the character
    clock and the bus clock (phi2, the E pin) run at the same rate.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C file to create the implementation.

    ## Pins

    - D0..D7, RW (1 = CPU read), CS (set by the caller when selected in this
      cycle), RS (0 = address/status register, 1 = data register), RES
    - LPEN        light pen input (rising edge latches the display address)
    - MA0..MA13   refresh / display memory address outputs
    - RA0..RA4    raster (scan line) address outputs
    - DE, HSYNC, VSYNC, CURSOR   outputs (active high)

    ## Registers

    R0 horizontal total-1, R1 horizontal displayed, R2 hsync position, R3 sync
    widths (bits 0-3 horizontal, bits 4-7 vertical, 0 = 16), R4 vertical total-1,
    R5 vertical total adjust, R6 vertical displayed, R7 vsync position, R8 mode,
    R9 max scan line-1, R10/R11 cursor start/end (R10 bits 5-6 blink mode),
    R12/R13 start address, R14/R15 cursor address, R16/R17 light pen (read
    only), R18/R19 update address, R31 update strobe (dummy).

    ## Status register (RS=0 read)

    bit 7 update ready (always 1), bit 6 light pen register full (cleared by
    reading R16 or R17), bit 5 vertical blanking.

    ## Not emulated

    - interlace modes, display / cursor skew bits of R8, the update strobe
      memory cycle (R18/R19/R31 are stored and read back)

    ## zlib/libpng license (same as the rest of the chips directory)
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M6545_PIN_MA0   (0)     // MA0..MA13 = bits 0..13
#define M6545_PIN_D0    (16)
#define M6545_PIN_RW    (24)
#define M6545_PIN_RES   (25)
#define M6545_PIN_CS    (40)
#define M6545_PIN_RS    (41)
#define M6545_PIN_LPEN  (42)
#define M6545_PIN_RA0   (48)    // RA0..RA4 = bits 48..52
#define M6545_PIN_DE    (53)
#define M6545_PIN_HSYNC (54)
#define M6545_PIN_VSYNC (55)
#define M6545_PIN_CURSOR (56)

#define M6545_RW        (1ULL<<M6545_PIN_RW)
#define M6545_RES       (1ULL<<M6545_PIN_RES)
#define M6545_CS        (1ULL<<M6545_PIN_CS)
#define M6545_RS        (1ULL<<M6545_PIN_RS)
#define M6545_LPEN      (1ULL<<M6545_PIN_LPEN)
#define M6545_MA_PINS   (0x3FFFULL)
#define M6545_RA_PINS   (0x1FULL<<M6545_PIN_RA0)
#define M6545_DE        (1ULL<<M6545_PIN_DE)
#define M6545_HSYNC     (1ULL<<M6545_PIN_HSYNC)
#define M6545_VSYNC     (1ULL<<M6545_PIN_VSYNC)
#define M6545_CURSOR    (1ULL<<M6545_PIN_CURSOR)

#define M6545_GET_DATA(p)   ((uint8_t)((p)>>M6545_PIN_D0))
#define M6545_SET_DATA(p,d) {p=((p)&~(0xFFULL<<M6545_PIN_D0))|(((uint64_t)(d)&0xFF)<<M6545_PIN_D0);}
#define M6545_GET_MA(p)     ((uint16_t)((p)&M6545_MA_PINS))
#define M6545_GET_RA(p)     ((uint8_t)(((p)>>M6545_PIN_RA0)&0x1F))

#define M6545_NUM_REGS (32)

typedef struct {
    uint8_t reg[M6545_NUM_REGS];
    uint8_t addr;                   // address register (5 bits)
    // counters
    uint8_t hc;                     // horizontal character counter
    uint8_t rc;                     // raster (scan line) counter
    uint8_t vc;                     // character row counter
    uint8_t adj;                    // vertical adjust lines done
    bool in_adjust;
    uint16_t line_start;            // MA at the start of the current character row
    uint16_t frame_start;           // R12/R13 latched at the frame start
    uint8_t hs_cnt, vs_cnt;
    bool hsync, vsync, de, vdisp, cursor;
    uint16_t ma;
    uint32_t frame;                 // frame counter (cursor blink)
    // light pen
    bool lpen_prev, lpen_full;
    // status
    bool in_reset;
} m6545_t;

void m6545_init(m6545_t* c);
void m6545_reset(m6545_t* c);
uint64_t m6545_tick(m6545_t* c, uint64_t pins);

#ifdef __cplusplus
} // extern "C"
#endif

/*--- IMPLEMENTATION ---------------------------------------------------------*/
#ifdef CHIPS_IMPL

void m6545_init(m6545_t* c) {
    for (int i = 0; i < M6545_NUM_REGS; i++) c->reg[i] = 0;
    c->addr = 0;
    m6545_reset(c);
}

void m6545_reset(m6545_t* c) {
    // reset only holds the counters; the registers keep their values
    c->hc = c->rc = c->vc = c->adj = 0;
    c->in_adjust = false;
    c->frame_start = (uint16_t)(((c->reg[12] & 0x3F) << 8) | c->reg[13]);
    c->line_start = c->frame_start;
    c->hs_cnt = c->vs_cnt = 0;
    c->hsync = c->vsync = c->de = c->cursor = false;
    c->vdisp = true;
    c->ma = c->frame_start;
    c->frame = 0;
    c->lpen_prev = false;
    c->lpen_full = false;
    c->in_reset = true;
}

static uint8_t m6545_reg_mask(int r) {
    // bits that exist in each register
    static const uint8_t m[32] = {
        0xFF,0xFF,0xFF,0xFF, 0x7F,0x1F,0x7F,0x7F, 0xFF,0x1F,0x7F,0x1F, 0x3F,0xFF,0x3F,0xFF,
        0x3F,0xFF,0x3F,0xFF, 0,0,0,0, 0,0,0,0, 0,0,0,0xFF
    };
    return m[r & 31];
}

static inline int m6545_hsw(const m6545_t* c) { int w = c->reg[3] & 0x0F; return w ? w : 16; }
static inline int m6545_vsw(const m6545_t* c) { int w = (c->reg[3] >> 4) & 0x0F; return w ? w : 16; }

uint64_t m6545_tick(m6545_t* c, uint64_t pins) {
    if (pins & M6545_RES) {
        m6545_reset(c);
        return pins & ~(M6545_MA_PINS | M6545_RA_PINS | M6545_DE | M6545_HSYNC | M6545_VSYNC | M6545_CURSOR);
    }
    c->in_reset = false;

    // --- bus access
    if (pins & M6545_CS) {
        if (pins & M6545_RW) {
            uint8_t data = 0;
            if (!(pins & M6545_RS)) {
                // status register
                data = 0x80;
                if (c->lpen_full) data |= 0x40;
                if (!c->vdisp) data |= 0x20;
            } else {
                const int r = c->addr & 31;
                if (r >= 12 && r <= 17) {
                    data = c->reg[r];
                    if (r == 16 || r == 17) c->lpen_full = false;
                } else if (r == 31) {
                    data = 0;
                }
            }
            M6545_SET_DATA(pins, data);
        } else {
            const uint8_t data = M6545_GET_DATA(pins);
            if (!(pins & M6545_RS)) {
                c->addr = data & 0x1F;
            } else {
                const int r = c->addr & 31;
                if (r < 16 || r == 18 || r == 19 || r == 31) {
                    c->reg[r] = (uint8_t)(data & m6545_reg_mask(r));
                }
            }
        }
    }

    // --- light pen: a rising edge latches the display address
    const bool lpen = (pins & M6545_LPEN) != 0;
    if (lpen && !c->lpen_prev) {
        c->reg[16] = (uint8_t)((c->ma >> 8) & 0x3F);
        c->reg[17] = (uint8_t)(c->ma & 0xFF);
        c->lpen_full = true;
    }
    c->lpen_prev = lpen;

    // --- outputs for the current character position
    const int r0 = c->reg[0], r1 = c->reg[1], r2 = c->reg[2];
    const int r4 = c->reg[4], r5 = c->reg[5], r6 = c->reg[6], r7 = c->reg[7];
    const int r9 = c->reg[9] & 0x1F;
    const uint16_t ma = (uint16_t)((c->line_start + c->hc) & 0x3FFF);
    c->ma = ma;
    c->vdisp = (c->vc < r6) && !c->in_adjust;
    c->de = c->vdisp && (c->hc < r1);
    if (c->hc == r2) c->hs_cnt = (uint8_t)m6545_hsw(c);
    if (c->hs_cnt) { c->hsync = true; c->hs_cnt--; } else { c->hsync = false; }
    // cursor
    {
        const uint16_t cur = (uint16_t)(((c->reg[14] & 0x3F) << 8) | c->reg[15]);
        const int cs = c->reg[10] & 0x1F, ce = c->reg[11] & 0x1F;
        const int blink = (c->reg[10] >> 5) & 3;
        bool on = c->de && ma == cur && c->rc >= cs && c->rc <= ce;
        if (blink == 1) on = false;
        else if (blink == 2) on = on && (((c->frame >> 4) & 1) == 0);
        else if (blink == 3) on = on && (((c->frame >> 5) & 1) == 0);
        c->cursor = on;
    }
    uint64_t out = pins & ~(M6545_MA_PINS | M6545_RA_PINS | M6545_DE | M6545_HSYNC | M6545_VSYNC | M6545_CURSOR);
    out |= (uint64_t)ma;
    out |= (uint64_t)(c->rc & 0x1F) << M6545_PIN_RA0;
    if (c->de) out |= M6545_DE;
    if (c->hsync) out |= M6545_HSYNC;
    if (c->vsync) out |= M6545_VSYNC;
    if (c->cursor) out |= M6545_CURSOR;

    // --- advance one character
    if (c->hc == r0) {
        c->hc = 0;
        // end of a scan line
        if (c->vs_cnt) {
            c->vs_cnt--;
            if (!c->vs_cnt) c->vsync = false;
        }
        bool new_frame = false;
        if (c->in_adjust) {
            c->adj++;
            c->rc++;
            if (c->adj >= r5) new_frame = true;
        } else if (c->rc == r9) {
            c->rc = 0;
            if (c->vc == r4) {
                if (r5 > 0) { c->in_adjust = true; c->adj = 0; }
                else new_frame = true;
            } else {
                c->vc++;
                c->line_start = (uint16_t)((c->line_start + r1) & 0x3FFF);
            }
        } else {
            c->rc++;
        }
        if (new_frame) {
            c->in_adjust = false;
            c->adj = 0; c->rc = 0; c->vc = 0;
            c->frame++;
            c->frame_start = (uint16_t)(((c->reg[12] & 0x3F) << 8) | c->reg[13]);
            c->line_start = c->frame_start;
        }
        // vertical sync starts on the first scan line of the row R7
        if (!c->in_adjust && c->vc == r7 && c->rc == 0 && !c->vsync) {
            c->vs_cnt = (uint8_t)m6545_vsw(c);
            c->vsync = true;
        }
    } else {
        c->hc++;
    }
    return out;
}

#endif // CHIPS_IMPL
