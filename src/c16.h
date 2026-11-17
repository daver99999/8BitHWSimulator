/* c16.h - the Commodore 16: ASSY 250443 (schematic 251788, netlist c16_board_data.c), the TED video
 * chip, the 7501 CPU and the rest of the board (c16.c, c16_chips.c). */
#ifndef C16_H
#define C16_H
#include "board.h"
#include "chips_common.h"
#include "kbd.h"

#define C16_FREQ            894886      /* the CPU clock (14.31818 MHz / 16): sim.cycle counts these */
#define C16_STEPS_PER_CYCLE 32          /* half periods of the crystal */
#define C16_CYCLES_PER_FRAME (57 * 262)
#define C16_SCREEN_W        384         /* 48 cycles of 8 dots */
#define C16_SCREEN_H        252

typedef struct c16 {
    board_t board;                      /* first: a c16_t is a board_t */
    int p_cpu, p_ted, p_xtal, p_ram[2];
    uint8_t basic[16384], kernal[16384];
    unsigned rom_mask;                  /* 1 BASIC, 2 KERNAL */
    uint32_t raster[C16_SCREEN_W * C16_SCREEN_H];
    kbd_t kbd;
    uint16_t key_matrix[8];             /* per row (6529 port bit): the columns (TED K lines) closed */
    int pin_row[8], pin_col[8];
    uint8_t joy[2];                     /* closed switches of the joysticks: bit 0 up, 1 down, 2 left, 3 right, 4 fire */
    int pin_joy[2][5], pin_joy_sel[2];
} c16_t;

static inline c16_t *C16(board_t *b) { return (c16_t *)b; }
extern const machine_t machine_c16;
static inline bool board_is_c16(const board_t *b) { return b->m == &machine_c16; }

/* Put a ROM image in (ref U3 BASIC, U4 KERNAL). False if the size is wrong. */
bool c16_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n);
/* RAM, as the 4416s hold it ($0000-$3FFF). */
void c16_ram_poke(board_t *b, uint16_t addr, uint8_t v);
uint8_t c16_ram_peek(board_t *b, uint16_t addr);
uint32_t c16_cpu_cycles(board_t *b);
/* The key at a place of the matrix (row = 6529 port bit, column = TED K line), for the keymap. */
const char *c16_key_legend(int row, int col);
#endif
