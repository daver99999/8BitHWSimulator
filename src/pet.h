/* pet.h - the Commodore PET 4032 / 8032 board (8032080-03 universal dynamic
 * board, logic diagram 8032081 rev E): netlist (pet4032_board_data.c,
 * pet8032_board_data.c), part models (pet_parts.c, pet_chips.c) and the
 * clock that moves them (pet.c). */
#ifndef PET_H
#define PET_H
#include "board.h"
#include "chips_common.h"
#include "kbd.h"

#define PET_FREQ            1000000     /* CPU clock, Hz: sim.cycle counts these */
#define PET_STEPS_PER_CYCLE 32          /* half periods of the 16 MHz crystal */
#define PET_CYCLES_PER_FRAME 16667      /* 60 Hz */
#define PET_SCREEN_W        800         /* the raster: 400 dots a line at 8 MHz (shown twice as wide), */
#define PET_SCREEN_H        400         /* or 800 at 16 MHz; from the start of VSYNC */

typedef struct pet {
    board_t board;              /* first: a pet_t is a board_t */
    int p_cpu, p_pia1, p_pia2, p_via, p_crtc, p_xtal, p_char;
    int pin_12v, pin_m5v, pin_9v;
    bool chiclet;               /* option "keyboard" (4032): the chiclet keyboard (same matrix as the graphics one) */
    bool cols80;                /* the 80 column board (odd display RAM fitted) */
    /* The ROM sockets UD6 ($F000) to UD12 ($9000) and the character ROM. */
    uint8_t rom[7][4096];
    unsigned rom_mask;          /* sockets with an image: bit i = rom_refs[i], bit 7 = UA3 */
    uint8_t chars[2048];
    /* Video: where the beam is, and the last sync levels. */
    int beam_x, beam_y;
    int pin_video, pin_hsync, pin_vsync, pin_shift_clk;
    int vsync_prev, hsync_prev, shift_clk_prev;
    /* The keyboard: J5's rows (the 74145 UC11 drives them low one at a time)
     * and KIN0-7 (PIA1 port B); a closed key joins its row to its column. */
    kbd_t kbd;
    uint16_t key_matrix[10];    /* per row: the columns closed */
    int pin_krow[10], pin_kin[8];
    int ram_part[2][8];         /* the 4116s: [0] the lower 16K ($0000), [1] the upper ($4000), by data bit */
    uint32_t step_in_cycle;     /* pet_run_steps(): steps done in this cycle */
} pet_t;

static inline pet_t *PET(board_t *b) { return (pet_t *)b; }
extern const machine_t machine_pet4032, machine_pet8032;
extern bool pet_chiclet;    /* the 4032's chiclet keyboard is selected (for the Keyboard window) */
static inline bool board_is_pet(const board_t *b) { return b->m == &machine_pet4032 || b->m == &machine_pet8032; }

/* Put a ROM image in a socket by reference (UD6..UD12: 4K each, UA3: the
 * 2K character ROM). Restarts nothing: call board_power_on() afterwards.
 * False if the socket is unknown or the image has the wrong size. */
bool pet_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n);
/* The video RAM as the board holds it (the 2114s UC4/UC5 for the even
 * addresses, UC6/UC7 for the odd ones on the 80 column board): the byte
 * at screen address ADDR (0-2047), for tests and --text. */
uint8_t pet_vram_peek(board_t *b, int addr);
/* Main RAM ($0000-$7FFF) as the 4116s hold it: for the program loader. */
void pet_ram_poke(board_t *b, uint16_t addr, uint8_t v);
uint8_t pet_ram_peek(board_t *b, uint16_t addr);
/* Run single half periods of the crystal (31 ns): for tracing; board_run() runs whole microseconds. */
void pet_run_steps(board_t *b, int steps);
/* CPU cycles run since power on, and the CPU's program counter... */
uint32_t pet_cpu_cycles(board_t *b);

/* The legend of the key at a place of the keyboard matrix as the editor ROM table has it ("Q", "RETURN", "~": no key). */
const char *pet_key_legend(bool cols80, int row, int col);

#endif
