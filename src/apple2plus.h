/* apple2plus.h - the Apple II+ board (rev 0 motherboard, schematic from the Apple II Reference Manual, 1979, redrawn in KiCad
 * by baldengineer's "bit-preserve"): netlist (apple2plus_board_data.c, traced by tools/apple2_sch2net.py), part models
 * (apple2_chips.c, the 6502 of the PET in pet_chips.c) and the clock that moves them (apple2plus.c). */
#ifndef APPLE2PLUS_H
#define APPLE2PLUS_H
#include "board.h"
#include "chips_common.h"
#include "parts.h"

#define A2_FREQ             1020484     /* CPU clock, Hz (14.31818 MHz / 14; every 65th cycle is a little longer) */
#define A2_STEPS_PER_CYCLE  28          /* half periods of the 14.31818 MHz crystal */
#define A2_CYCLES_PER_FRAME 17030       /* 262 lines of 65 cycles */
#define A2_SCREEN_W         580         /* the picture: 560 dots (40 characters of 7) and a little border */
#define A2_SCREEN_H         384         /* 192 lines, each shown twice */
#define A2_LINE_SAMPLES     912         /* the dots (14.31818 MHz samples) of a line, with some room */
#define A2_LINES            270

typedef struct apple2plus {
    board_t board;              /* first: an apple2plus_t is a board_t */
    int p_cpu, p_xtal, p_kbd, p_chr;
    int pin_xtal, pin_osc;      /* the crystal's pin 1 and the oscillator input of the dot clock gate (the transistors between are not simulated) */
    int pin_video, pin_sync;    /* VIDEO_DATA and the composite sync */
    int pin_12v, pin_m5v, pin_m12v;
    uint8_t rom[6][2048];       /* the ROMs D0, D8, E0, E8, F0, F8 */
    uint8_t chars[2048];
    unsigned rom_mask;          /* bit i: rom i is there, bit 6: the character generator */
    /* The beam: the dots (one per cycle of the crystal) of a line since its sync pulse, the line, and what the sync did. */
    uint8_t raw[A2_LINES][A2_LINE_SAMPLES];
    int beam_x, beam_y, sync_prev, sync_low, sync_low_last, vs_seen;
    uint32_t step_in_cycle;
    /* The keys typed, and the strobe of the one the keyboard connector has. */
    uint8_t keyq[512]; int keyq_n;
    uint64_t strobe_off, next_key;
    int shift_down, ctrl_down;
} apple2plus_t;

static inline apple2plus_t *A2(board_t *b) { return (apple2plus_t *)b; }
extern const machine_t machine_apple2plus;
static inline bool board_is_apple2plus(const board_t *b) { return b->m == &machine_apple2plus; }

/* The byte at a RAM address as the 4116s hold it (for tests and --text). */
uint8_t apple2plus_ram_peek(board_t *b, uint16_t addr);
void apple2plus_run_steps(board_t *b, int steps);
void apple2_kbc_set(part_t *p, int data, int strobe, int reset);
#endif
