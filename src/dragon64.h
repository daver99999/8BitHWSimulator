/* dragon64.h - the Dragon 64 (issue 6 board; the Tano Dragon was an NTSC Dragon 64): netlist (dragon64_board_data.c, from the
 * KiCad PCB of the Dragon schematics project, read by tools/kicad_pcb2c.py), part models (coco_chips.c: the 6809E, the 6883 SAM,
 * the 6847 and the PAL timing counters; the PIAs of pet_chips.c, the ACIA of c16_chips.c) and the machine that moves them
 * (dragon64.c). */
#ifndef DRAGON64_H
#define DRAGON64_H
#include "board.h"
#include "parts.h"
#include "kbd.h"

#define CC_STEPS_PER_CYCLE  32          /* half periods of the 14.218 MHz crystal in a CPU cycle (the SAM divides by 16) */
#define CC_FREQ             888625      /* 14218000 / 16 */
#define CC_CYCLES_PER_FRAME 17772       /* 50 Hz */

typedef struct dragon64 {
    board_t board;                  /* first: a dragon64_t is a board_t */
    int p_cpu, p_sam, p_vdg, p_pia0, p_pia1, p_rom0, p_rom1, p_xtal;
    int pin_row[7], pin_col[8];     /* the keyboard: PIA0's PA0..PA6 and PB0..PB7 */
    int pin_reset_cap;              /* the reset capacitor's pin: held low at power on */
    uint8_t rom[2][16384];          /* IC34 (/ROM0) and IC37 (/ROM1): d64_1.rom and d64_2.rom */
    unsigned rom_mask;              /* bit 0: d64_1.rom found, bit 1: d64_2.rom */
    kbd_t kbd;
    uint8_t key_matrix[7];          /* per row, the columns that are pressed */
    uint32_t reset_cycles;          /* the power-on reset is held until sim.cycle gets here */
} dragon64_t;

static inline dragon64_t *DG(board_t *b) { return (dragon64_t *)b; }
extern const machine_t machine_dragon64;
static inline bool board_is_dragon64(const board_t *b) { return b->m == &machine_dragon64; }

/* The byte at a RAM address as the 4164s hold it (for tests and --text). */
uint8_t dragon64_ram_peek(board_t *b, uint16_t addr);
void dragon64_run_steps(board_t *b, int steps);
#endif
