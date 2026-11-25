/* apple2e.h - the Apple IIe (820-0064-B motherboard, unenhanced: 6502, 64K, 80 column slot not fitted). The netlist is traced from
 * the Bit Preserve capture of the 050-0051-D schematic (apple2e_board_data.c, by tools/apple2e_netlist2c.py); the chips that are
 * not in the other machines are in apple2e_chips.c and the clock that moves them in apple2e.c. */
#ifndef APPLE2E_H
#define APPLE2E_H
#include "board.h"
#include "chips_common.h"
#include "parts.h"

#define AE_FREQ             1020484
#define AE_STEPS_PER_CYCLE  28
#define AE_CYCLES_PER_FRAME 17030
#define AE_SCREEN_W         580
#define AE_SCREEN_H         384
#define AE_LINE_SAMPLES     912
#define AE_LINES            270

typedef struct apple2e {
    board_t board;
    int p_cpu, p_xtal, p_kbd, p_kbrom;
    int pin_xtal, pin_osc, pin_video, pin_sync, pin_a10, pin_a9;
    int pin_12v, pin_m12v, pin_m5v, pin_m5c;
    uint8_t rom_cd[8192], rom_ef[8192], kbrom[2048], vrom[4096];
    unsigned rom_mask;          /* bit 0 CD, 1 EF, 2 keyboard, 3 video */
    uint8_t raw[AE_LINES][AE_LINE_SAMPLES];
    int beam_x, beam_y, sync_prev, sync_low, sync_low_last, vs_seen;
    uint32_t step_in_cycle;
    uint8_t keyq[512]; int keyq_n;
    uint64_t strobe_off, akd_off, next_key;
} apple2e_t;

static inline apple2e_t *AE(board_t *b) { return (apple2e_t *)b; }
extern const machine_t machine_apple2e;
static inline bool board_is_apple2e(const board_t *b) { return b->m == &machine_apple2e; }

extern const part_type_t PT_A2E_MMU, PT_A2E_IOU, PT_A2E_HAL, PT_A2E_2764, PT_A2E_2732, PT_A2E_3600, PT_74LS109, PT_74LS374;
void apple2e_kbenc_set(part_t *p, int code, int akd, int strobe);
uint8_t apple2e_ram_peek(board_t *b, uint16_t addr);
void apple2e_run_steps(board_t *b, int steps);
#endif
