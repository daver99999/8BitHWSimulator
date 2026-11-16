/* ts2068.h - the Timex Sinclair TS2068 board 335-850001 (schematic 335-870500, Technical
 * Manual): netlist (ts2068_board_data.c), the SCLD (scld.c) and the machine (ts2068.c). */
#ifndef TS2068_H
#define TS2068_H
#include "board.h"
#include "chips_common.h"
#include "z80.h"
#include "kbd.h"
#include "scld.h"
#include "ay8912.h"

#define TS_FREQ             3528000     /* CPU clock, Hz: 14.112 MHz / 4 */
#define TS_STEPS_PER_CYCLE  8           /* half periods of the 14.112 MHz oscillator */
#define TS_CYCLES_PER_FRAME (224 * 262)

typedef struct ts2068 {
    board_t board;              /* first: a ts2068_t is a board_t */
    z80_t cpu;
    uint64_t cpu_pins;
    int p_cpu, p_scld, p_ay, p_rom0, p_rom1;
    int pin_clk, clk;
    bool in_reset;
    int32_t cache_addr, cache_data;
    uint64_t changes_seen;
    uint8_t rom_home[16384], rom_ex[8192];
    unsigned rom_mask;          /* bit 0: home ROM loaded, bit 1: EXROM */
    kbd_t kbd;
    uint16_t key_matrix[5];
    int pin_row[8], pin_kb[5];  /* J9: the key rows (A8-A15) and KB0-KB4 */
    int pin_reset_net;
    uint64_t reset_until;
} ts2068_t;

static inline ts2068_t *TS(board_t *b) { return (ts2068_t *)b; }
extern const machine_t machine_ts2068;
static inline bool board_is_ts2068(const board_t *b) { return b->m == &machine_ts2068; }

/* The byte at a RAM address ($4000-$FFFF) as the DRAMs hold it (for tests). */
uint8_t ts2068_ram_peek(board_t *b, uint16_t addr);
/* Run single half periods of the 14.112 MHz oscillator (71 ns): for tracing. */
void ts2068_run_steps(board_t *b, int steps);
#endif
