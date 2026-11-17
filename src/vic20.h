/* vic20.h - the Commodore VIC-20: the cost reduced board ASSY 250403
 * (schematic 251027, netlist vic20b_board_data.c) and the early board
 * (schematic 324001, vic20a_board_data.c), the 6560 video chip and the
 * clock that moves them (vic20.c, vic20_chips.c). */
#ifndef VIC20_H
#define VIC20_H
#include "board.h"
#include "chips_common.h"
#include "kbd.h"

#define VIC20_FREQ            1022727   /* the system clock (14.31818 MHz / 14): sim.cycle counts these */
#define VIC20_STEPS_PER_CYCLE 28        /* half periods of the crystal */
#define VIC20_CYCLES_PER_FRAME (65 * 261)
#define VIC20_SCREEN_W        448       /* the raster of the 6560, 224 dots shown twice as wide */
#define VIC20_SCREEN_H        232

/* How a board is built: where the RAM is. A RAM is a pair of 2114s (1K x 8:
 * the low nibble chip, the high nibble chip) or one 6116 (2K x 8). */
typedef struct { const char *lo, *hi; uint16_t base, len; } vic20_ram_t;
typedef struct {
    const vic20_ram_t *ram; int nram;
    const char *color;                  /* the color RAM (2114, 1K x 4 at $9400) */
} vic20_variant_t;

typedef struct vic20 {
    board_t board;                      /* first: a vic20_t is a board_t */
    const vic20_variant_t *v;
    int p_cpu, p_via1, p_via2, p_vic, p_xtal;
    uint8_t basic[8192], kernal[8192], chars[4096];
    unsigned rom_mask;                  /* 1 BASIC, 2 KERNAL, 4 characters */
    uint32_t raster[224 * 232];
    kbd_t kbd;
    uint16_t key_matrix[8];             /* per row (VIA 2 port A): the columns closed */
    int pin_row[8], pin_col[8], pin_restore, pin_restore_gnd;
    bool restore;
    uint32_t step_in_cycle;
} vic20_t;

static inline vic20_t *VIC20(board_t *b) { return (vic20_t *)b; }
extern const machine_t machine_vic20a, machine_vic20b;
static inline bool board_is_vic20(const board_t *b) { return b->m == &machine_vic20a || b->m == &machine_vic20b; }

/* Put a ROM image in (ref UE11 BASIC, UE12 KERNAL, UD7 characters). False if the size is wrong. */
bool vic20_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n);
/* RAM, as the chips hold it ($0000-$03FF, $1000-$1FFF; the color RAM at $9400-$97FF). */
void vic20_ram_poke(board_t *b, uint16_t addr, uint8_t v);
uint8_t vic20_ram_peek(board_t *b, uint16_t addr);
uint32_t vic20_cpu_cycles(board_t *b);
/* The key at a place of the matrix (column = VIA 2 port B bit, row = port A bit), for the keyboard map. */
const char *vic20_key_legend(int col, int row);
#endif
