/* mc10.h - the Tandy / Radio Shack MC-10 (Micro Color Computer): netlist (mc10_board_data.c, from the Eagle schematic, read by
 * tools/mc10_sch2c.py), part models (mc10_chips.c: the 6803, the 74LS155, the 74LS367, the 74LS76, the 14503 and the ROM;
 * coco_chips.c: the 6847 that makes its own addresses) and the machine that moves them (mc10.c). */
#ifndef MC10_H
#define MC10_H
#include "board.h"
#include "parts.h"
#include "kbd.h"

#define MC10_STEPS_PER_CYCLE 8          /* half periods of the 3.579545 MHz crystal in a CPU cycle (the 6803 divides by 4) */
#define MC10_FREQ            894886     /* 3579545 / 4 */
#define MC10_CYCLES_PER_FRAME 14934     /* 262 lines of 228 crystal periods */

typedef struct mc10 {
    board_t board;                  /* first: a mc10_t is a board_t */
    int p_cpu, p_vdg, p_rom, p_xtal, p_ram[2];
    int pin_row[8], pin_col[7];     /* the keyboard: J5 (KR0..KR7) and J4 (KB0..KB5 and SHIFT) */
    int pin_rst;                    /* the reset switch's net: held low at power on */
    uint8_t rom[8192];              /* U3 */
    bool rom_found;
    kbd_t kbd;
    uint8_t key_matrix[8];          /* per row, the columns that are pressed */
    uint32_t reset_cycles;          /* the power-on reset is held until sim.cycle gets here */
} mc10_t;

static inline mc10_t *MC(board_t *b) { return (mc10_t *)b; }
extern const machine_t machine_mc10;
static inline bool board_is_mc10(const board_t *b) { return b->m == &machine_mc10; }

/* The byte at a RAM address ($4000-$4FFF) as the 4016s hold it (for tests and --text). */
uint8_t mc10_ram_peek(board_t *b, uint16_t addr);
void mc10_run_steps(board_t *b, int steps);
#endif
