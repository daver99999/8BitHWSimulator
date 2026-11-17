/* plus4.h - the Commodore Plus/4 (schematic 310164, netlist plus4_board_data.c): the C16's TED, 8501 CPU and
 * PLA (c16_chips.c) with 64K of 4164s, four ROMs, a 6551 and a second 6529B (plus4.c). */
#ifndef PLUS4_H
#define PLUS4_H
#include "board.h"
#include "chips_common.h"
#include "kbd.h"

#define PLUS4_FREQ            894886      /* the CPU clock (14.31818 MHz / 16): sim.cycle counts these */
#define PLUS4_STEPS_PER_CYCLE 32          /* half periods of the crystal */
#define PLUS4_CYCLES_PER_FRAME (57 * 262)
#define PLUS4_SCREEN_W        384         /* 48 cycles of 8 dots */
#define PLUS4_SCREEN_H        252

typedef struct plus4 {
    board_t board;                      /* first: a plus4_t is a board_t */
    int p_cpu, p_ted, p_xtal, p_ram[8];
    uint8_t rom[4][16384];              /* U23 BASIC, U24 KERNAL, U25 function low, U26 function high */
    unsigned rom_mask;                  /* 1 BASIC, 2 KERNAL, 4 function low, 8 function high */
    uint32_t raster[PLUS4_SCREEN_W * PLUS4_SCREEN_H];
    kbd_t kbd;
    uint16_t key_matrix[8];             /* per row (6529 port bit): the columns (TED K lines) closed */
    int pin_row[8], pin_col[8];
    uint8_t joy[2];                     /* closed switches of the joysticks: bit 0 up, 1 down, 2 left, 3 right, 4 fire */
    int pin_joy[2][5], pin_joy_sel[2];
} plus4_t;

static inline plus4_t *PLUS4(board_t *b) { return (plus4_t *)b; }
extern const machine_t machine_plus4;
static inline bool board_is_plus4(const board_t *b) { return b->m == &machine_plus4; }

/* Put a ROM image in (ref U23 BASIC, U24 KERNAL, U25 function low, U26 function high). False if the size is wrong. */
bool plus4_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n);
/* RAM, as the eight 4164s hold it ($0000-$FFFF). */
void plus4_ram_poke(board_t *b, uint16_t addr, uint8_t v);
uint8_t plus4_ram_peek(board_t *b, uint16_t addr);
uint32_t plus4_cpu_cycles(board_t *b);
/* The key at a place of the matrix (row = 6529 port bit, column = TED K line), for the keymap. */
const char *plus4_key_legend(int row, int col);
#endif
