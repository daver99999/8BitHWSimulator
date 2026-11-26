/* atari800.h - the Atari 800 (motherboard CO14175 Rev A with its CPU board, personality board and 16K RAM boards). The netlist is typed
 * in from the Atari 400/800 Field Service Manual (atari800_board_data.c, by tools/atari800_netlist2c.py); the custom chips (ANTIC, GTIA,
 * POKEY) are behavioural models in atari800_chips.c, the clock that moves them is in atari800.c. */
#ifndef ATARI800_H
#define ATARI800_H
#include "board.h"
#include "parts.h"
#include "kbd.h"

#define AT_FREQ             1789773
#define AT_STEPS_PER_CYCLE  4
#define AT_CYCLES_PER_FRAME 29868          /* 114 x 262 */
#define AT_SCREEN_W         336
#define AT_SCREEN_H         240
#define AT_OSC_PER_LINE     228
#define AT_LINES            262

typedef struct atari800 {
    board_t board;
    int p_xtal, p_cpu, p_gtia, p_antic, p_pokey, p_pia, p_rc;
    int pin_rcnode, pin_12v, pin_m5v, pin_osc;
    uint8_t os[10240];                      /* A402 (2K) + A403 (4K) + A401 (4K) = $D800-$FFFF */
    uint8_t basic[8192];                    /* the BASIC cartridge */
    unsigned rom_mask;                      /* bit 0 OS, 1 BASIC */
    bool basic_in;                          /* the BASIC cartridge is plugged into the left slot */
    uint64_t reset_until;
    uint32_t step_in_cycle;
    int keyq_n;
    kbd_t kbd;
    uint8_t km[12];                         /* per matrix line, the columns that are pressed (lines 8-11: shift, START, SELECT, OPTION) */
} atari800_t;

static inline atari800_t *AT(board_t *b) { return (atari800_t *)b; }
extern const machine_t machine_atari800;
void atari800_run_steps(board_t *b, int steps);
static inline bool board_is_atari800(const board_t *b) { return b->m == &machine_atari800; }

extern const part_type_t PT_ATARI_GTIA, PT_ATARI_ANTIC, PT_ATARI_POKEY, PT_ATARI_ROM, PT_ATARI_CART, PT_ATARI_BUF, PT_XTAL358;
extern const part_type_t PT_74LS03, PT_74LS42, PT_74LS158, PT_CD4051, PT_CD4050;
/* A mask ROM: its image, size (2048 or 4096) and the active level of its chip selects (pins 18 (2K only), 20, 21). */
void atari_cart_set(part_t *p, const uint8_t *data);
void atari_rom_set(part_t *p, const uint8_t *data, unsigned size, int cs18, int cs20, int cs21);
void atari_gtia_set_screen(part_t *p, uint32_t *fb);
uint8_t atari_gtia_reg_of(const part_t *p, int r);
uint8_t atari_antic_reg_of(const part_t *p, int r);
void atari_antic_status_of(const part_t *p, char *buf, int len);
void atari_pokey_keys_status(const part_t *p, char *buf, int len);
#endif
