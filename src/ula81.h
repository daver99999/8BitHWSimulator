/* ula81.h - the Sinclair ZX81 / Timex Sinclair 1000 logic array (IC1, "Sinclair Computer Logic": Ferranti
 * 2C158E, 2C184E or 2C210E), modeled at its 40 pins. See ula81.c. */
#ifndef ULA81_H
#define ULA81_H
#include "sim.h"
#include "parts.h"

#define ULA81_CYCLES_PER_LINE   207     /* CPU clock cycles in a scan line (3.25 MHz / 207 = 15.7 kHz) */
#define ULA81_PIXELS_PER_LINE   414     /* two pixels (6.5 MHz) per CPU clock cycle */
#define ULA81_LINES             262     /* lines of the picture kept (the US machine's 60 Hz frame) */

/* The pins the logic uses (1-based; 0 = the chip has no such pin). */
typedef struct {
    int a7p, a8p, a2p, a1p, a0p, rd, iorq, wr, mreq, m1, a14, ramcs, romcs, clk, nmi, tv, halt, a15, tape, ukus, osc;
    int data[8], kbd[5], alt[9];
    bool clk_true;                  /* the clock pin carries the CPU clock itself (the 40 pin ULA's is inverted by TR2) */
    bool push_pull;                 /* the data pins drive both levels (the ULA's are open collector) */
    uint8_t (*bitmap)(sim_t *s);    /* where the character bitmap row is read from during a video fetch (NULL: the data pins) */
} ula81_pins_t;

typedef struct {
    const ula81_pins_t *pm;         /* NULL: the 40 pin ULA's */
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
    bool hold_fetch;                /* the memory has not answered yet: do not look at the data bus (the TS1500's DRAMs) */
    uint32_t *fb;                   /* ULA81_PIXELS_PER_LINE x ULA81_LINES */
} ula81_state_t;

extern const part_type_t PT_ULA81;
void ula81_step(sim_t *s, part_t *p);
void ula81_eval(sim_t *s, part_t *p);       /* the outputs the logic drives (the chip adds its own) */
bool ula81_hsync_cycle(const ula81_state_t *u);       /* one half period of the 6.5 MHz oscillator */
#endif
