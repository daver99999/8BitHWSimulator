/* ts1000.h - the Timex Sinclair 1000 (the American ZX81), board Issue 3: netlist (ts1000_board_data.c), the ULA
 * (ula81.c) and the machine (ts1000.c). */
#ifndef TS1000_H
#define TS1000_H
#include "board.h"
#include "chips_common.h"
#include "z80.h"
#include "kbd.h"
#include "ula81.h"

#define T1_FREQ             3250000     /* CPU clock, Hz: 6.5 MHz / 2 */
#define T1_STEPS_PER_CYCLE  4           /* half periods of the 6.5 MHz oscillator */
#define T1_CYCLES_PER_FRAME (207 * 262)

typedef struct ts1000 {
    board_t board;              /* first: a ts1000_t is a board_t */
    z80_t cpu;
    uint64_t cpu_pins;
    int p_cpu, p_ula, p_rom, p_ram;
    int pin_clk, clk;
    bool in_reset;
    int32_t cache_addr, cache_data;
    uint8_t rom[8192];
    bool rom_loaded;
    bool ram1k;                 /* option "ram": the ZX81's 1K instead of the TS1000's 2K */
    kbd_t kbd;
    uint16_t key_matrix[5];
    int pin_row[8], pin_kb[5];  /* KB2 (the half rows A8-A15) and KB1 (KBD0-KBD4) */
    int pin_reset_net;
    uint64_t reset_until;
    uint32_t seen_frames;       /* the ULA's frame count when last looked at, and the cycle it last changed (the monitor's sync) */
    uint64_t last_frame_cycle;
} ts1000_t;

static inline ts1000_t *T1(board_t *b) { return (ts1000_t *)b; }
extern const machine_t machine_ts1000, machine_ts1000i1;     /* the Issue 3 and the Issue 1 */
static inline bool board_is_ts1000(const board_t *b) { return b->m == &machine_ts1000 || b->m == &machine_ts1000i1; }

/* The byte at a RAM address ($4000-$7FFF, mirrored) in the 2K RAM chip (for tests). */
uint8_t ts1000_ram_peek(board_t *b, uint16_t addr);
#endif
