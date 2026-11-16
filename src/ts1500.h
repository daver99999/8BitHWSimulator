/* ts1500.h - the Timex Sinclair 1500: netlist (ts1500_board_data.c), the gate array U5 (a 68 pin custom chip that does
 * the ZX81 ULA's job, plus the DRAM control; logic from ula81.c) and the machine (ts1500.c). */
#ifndef TS1500_H
#define TS1500_H
#include "board.h"
#include "chips_common.h"
#include "z80.h"
#include "kbd.h"
#include "ula81.h"

#define T5_FREQ             3250000     /* CPU clock, Hz: 6.5 MHz / 2 */
#define T5_STEPS_PER_CYCLE  4           /* half periods of the 6.5 MHz oscillator */
#define T5_CYCLES_PER_FRAME (207 * 262)

typedef struct ts1500 {
    board_t board;              /* first: a ts1500_t is a board_t */
    z80_t cpu;
    uint64_t cpu_pins;
    int p_cpu, p_ula, p_rom, p_dram[8];
    int pin_clk, clk;
    bool in_reset;
    int32_t cache_addr, cache_data;
    uint8_t rom[8192];
    bool rom_loaded;
    kbd_t kbd;
    uint16_t key_matrix[5];
    int pin_row[8], pin_kb[5];  /* KB2 (the half rows A8-A15) and KB1 (KBD0-KBD4) */
    int pin_reset_net;
    uint64_t reset_until;
} ts1500_t;

static inline ts1500_t *T5(board_t *b) { return (ts1500_t *)b; }
extern const machine_t machine_ts1500;
static inline bool board_is_ts1500(const board_t *b) { return b->m == &machine_ts1500; }

/* The byte at a RAM address ($4000-$7FFF) in the eight 4116 DRAMs (for tests). */
uint8_t ts1500_ram_peek(board_t *b, uint16_t addr);
#endif
