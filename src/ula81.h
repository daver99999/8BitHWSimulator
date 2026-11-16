/* ula81.h - the Sinclair ZX81 / Timex Sinclair 1000 logic array (IC1, "Sinclair Computer Logic": Ferranti
 * 2C158E, 2C184E or 2C210E), modeled at its 40 pins. See ula81.c. */
#ifndef ULA81_H
#define ULA81_H
#include "sim.h"
#include "parts.h"

#define ULA81_CYCLES_PER_LINE   207     /* CPU clock cycles in a scan line (3.25 MHz / 207 = 15.7 kHz) */
#define ULA81_PIXELS_PER_LINE   414     /* two pixels (6.5 MHz) per CPU clock cycle */
#define ULA81_LINES             262     /* lines of the picture kept (the US machine's 60 Hz frame) */

typedef struct {
    uint8_t phase;                  /* half period of the 6.5 MHz oscillator inside a CPU clock period (0..3) */
    bool cpuclk;                    /* the CPU clock the ULA makes (3.25 MHz) */
    uint16_t cnt;                   /* cycles since the line counter was reset, 0..206 */
    uint8_t line;                   /* the 3 bit line counter: the scan line inside a character row */
    bool nmi_en, vsync, hsync;      /* the NMI generator, and the sync generators */
    /* The video fetch of one opcode fetch (M1) cycle with A15 high. */
    uint8_t vf;                     /* 0 idle, 1 forcing a NOP on the bus, 2 driving the ROM address, 3 done */
    bool fetched;                   /* this M1 cycle was looked at */
    uint8_t code;                   /* the character code latched */
    /* What the pins did one step ago, to find the cycles. */
    bool m1_prev, wr_prev, rd_prev, ack_prev;
    /* The picture. */
    uint8_t shift, shift_n;         /* the video shift register (bit 7 first) and the pixels left in it */
    bool inverse;
    uint8_t half_pix;               /* steps since the last pixel was put out */
    bool ink;                       /* the level of the picture signal: a pixel that is lit (black ink) */
    int fy;                         /* the line of the picture (-1 before the first line after a vertical sync) */
    uint32_t frames;
    bool running;                   /* the oscillator X1 oscillates */
    uint32_t *fb;                   /* ULA81_PIXELS_PER_LINE x ULA81_LINES */
} ula81_state_t;

extern const part_type_t PT_ULA81;
void ula81_step(sim_t *s, part_t *p);       /* one half period of the 6.5 MHz oscillator */
#endif
