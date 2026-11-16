/* scld.h - the Timex SCLD (Standard Cell Logic Device, U3) of the TS2068,
 * modeled at its pins. See scld.c. */
#ifndef SCLD_H
#define SCLD_H
#include "sim.h"
#include "parts.h"

#define SCLD_TICKS_PER_LINE 448     /* pixel clock (7.056 MHz) ticks: 14.112 MHz / 896 = 15.75 kHz */
#define SCLD_LINES          262     /* 60.1145 Hz */
#define SCLD_SCREEN_W       704     /* 2 x (48 + 256 + 48): the hi-res mode has 512 pixels in 256 ticks */
#define SCLD_SCREEN_H       240     /* 24 + 192 + 24 */
#define SCLD_CELL_HS        32      /* half periods of the 14.112 MHz oscillator in a cell fetch (8 ticks) */

typedef struct {
    uint16_t c, v;                  /* pixel counter 0..447, line counter 0..261 */
    uint8_t half;                   /* half period of the oscillator inside a tick (0..3) */
    uint32_t frames;
    uint8_t cphase;                 /* half periods inside the CPU clock period (8) */
    bool cpuclk, held;              /* the CPU clock, and whether contention is holding it high */
    uint8_t aphase;                 /* the AY clock (1.764 MHz) */
    bool ayclk;
    /* Registers. */
    uint8_t ff, hsr, border;        /* ports FF, F4 and the border of FE */
    bool mic, speaker;
    /* The display fetch of the current cell. */
    bool busy;                      /* the display is using the display RAM */
    bool f_ras, f_cas;              /* its strobes */
    uint8_t f_addr;                 /* what it puts on MA0-MA7 */
    bool f_ae;                      /* it drives MA */
    uint8_t line_a[32], line_b[32]; /* bitmap and attribute (or the second file's byte) of every cell */
    uint8_t data_a, data_b;
    /* CPU cycles. */
    int cpu_hs;                     /* half periods since /MREQ went low (-1: none) */
    int cras_hs;                    /* half periods since the CPU's /RASI began (display RAM; -1: none) */
    uint8_t gap;                    /* half periods since the display's /RASI last was low (precharge), up to 8 */
    bool mreq_prev;
    uint8_t refresh_bit;            /* the 8th row bit of the refresh address */
    uint8_t rfsh_prev;
    bool irq;
    bool running;                   /* there is an oscillator */
    bool ear_in;                    /* what comes into the EAR socket (set by the machine) */
    uint32_t *fb;                   /* SCLD_SCREEN_W x SCLD_SCREEN_H */
    uint8_t pix;                    /* colour of the last pixel */
} scld_state_t;

extern const part_type_t PT_SCLD;
void scld_step(sim_t *s, part_t *p);       /* one half period of the 14.112 MHz oscillator */
#endif
