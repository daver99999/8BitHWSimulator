/* coco2.h - the Tandy / Radio Shack Color Computer 2 (26-3134B / 3136B board): netlist (coco2_board_data.c, from the KiCad PCB
 * of the coco_motherboards project, read by tools/kicad_pcb2c.py), part models (coco_chips.c: the 6809E, the SAM and the 6847T1;
 * the PIAs of pet_chips.c) and the machine that moves them (coco2.c). */
#ifndef COCO2_H
#define COCO2_H
#include "board.h"
#include "parts.h"
#include "kbd.h"

#define CC_STEPS_PER_CYCLE  32          /* half periods of the 14.31818 MHz crystal in a CPU cycle (the SAM divides by 16) */
#define CC_FREQ             894886      /* 14318180 / 16 */
#define CC_CYCLES_PER_FRAME 14934       /* 262 lines of 56.875 cycles */

typedef struct coco2 {
    board_t board;                  /* first: a coco2_t is a board_t */
    int p_cpu, p_sam, p_vdg, p_pia0, p_pia1, p_rom, p_xtal;
    int pin_row[7], pin_col[8];     /* the keyboard: PIA0's PA0..PA6 and PB0..PB7 */
    int pin_reset_cap;              /* the reset capacitor's pin: held low at power on */
    uint8_t rom[16384];             /* IC8: Extended BASIC ($8000) then Color BASIC ($A000) */
    unsigned rom_mask;              /* bit 0: Extended BASIC, bit 1: Color BASIC */
    kbd_t kbd;
    uint8_t key_matrix[7];          /* per row, the columns that are pressed */
    uint32_t reset_cycles;          /* the power-on reset is held until sim.cycle gets here */
} coco2_t;

static inline coco2_t *CC2(board_t *b) { return (coco2_t *)b; }
extern const machine_t machine_coco2;
static inline bool board_is_coco2(const board_t *b) { return b->m == &machine_coco2; }

/* The byte at a RAM address as the 4164s hold it (for tests and --text). */
uint8_t coco2_ram_peek(board_t *b, uint16_t addr);
void coco2_run_steps(board_t *b, int steps);
#endif
