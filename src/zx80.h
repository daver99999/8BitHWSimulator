/* zx80.h - the Sinclair ZX80: netlist (zx80_board_data.c), its own chips (zx80_parts.c) and the machine (zx80.c). */
#ifndef ZX80_H
#define ZX80_H
#include "board.h"
#include "chips_common.h"
#include "z80.h"
#include "kbd.h"
#include "ula81.h"      /* the picture is the same size as the ZX81's */

#define Z8_FREQ             3250000     /* CPU clock, Hz: 6.5 MHz / 2 */
#define Z8_STEPS_PER_CYCLE  4           /* half periods of the 6.5 MHz oscillator */
#define Z8_CYCLES_PER_FRAME (207 * 262)

typedef struct zx80 {
    board_t board;              /* first: a zx80_t is a board_t */
    z80_t cpu;
    uint64_t cpu_pins;
    int p_cpu, p_rom, p_ram, p_osc, p_diff, p_x1;
    int pin_clk, clk, pin_phi, phi;
    int pin_video, pin_sync;
    bool in_reset;
    int32_t cache_addr, cache_data;
    uint8_t rom80[4096], rom81[8192];
    bool have80, have81;
    int rom_sel;                /* option "rom": 0 = the ZX80's own 4K ROM, 1 = the ZX81's 8K ROM */
    kbd_t kbd;
    uint16_t key_matrix[5];
    int pin_row[8], pin_kb[5];  /* KB2 (the half rows A8-A15) and KB1 (KBD0-KBD4) */
    int pin_reset_net;
    uint64_t reset_until;
    /* the video signal, sampled every step */
    int vx, vy, vsync_len, vsync_prev;
} zx80_t;

static inline zx80_t *Z8(board_t *b) { return (zx80_t *)b; }
extern const machine_t machine_zx80;
static inline bool board_is_zx80(const board_t *b) { return b->m == &machine_zx80; }

/* The byte at a RAM address ($4000-$7FFF, 1K mirrored) (for tests). */
uint8_t zx80_ram_peek(board_t *b, uint16_t addr);
#endif
