/* c128.h - the Commodore 128: schematic 310378 rev 7A (netlist c128_board_data.c), the 8502 and Z80A CPUs, the
 * 8722 MMU, the 8721 PLA, the 8564 VIC-IIe and the rest of the board (c128.c, c128_chips.c). */
#ifndef C128_H
#define C128_H
#include "board.h"
#include "chips_common.h"
#include "kbd.h"
#include "c128_drive.h"

#define C128_FREQ            985248     /* the system clock: sim.cycle counts these (the VIC core is the PAL 6569) */
#define C128_STEPS_PER_CYCLE 16         /* half periods of the dot clock */
#define C128_CYCLES_PER_FRAME (63 * 312)
#define C128_VIC_W           392        /* the picture of the 8564 */
#define C128_VIC_H           272
#define C128_SCREEN_W        784        /* the monitor's: the 8564's picture twice as big, or the VDC's 640 x 200 lines doubled */
#define C128_SCREEN_H        544

typedef struct c128 {
    board_t board;                      /* first: a c128_t is a board_t */
    int p_cn6, p_cpu, p_z80, p_vic, p_mmu, p_cia[2], p_sid, p_vdc, p_osc, p_y2, p_cn5;
    uint8_t rom32[32768], basic_lo[16384], basic_hi[16384], chr[8192];
    unsigned rom_mask;                  /* 1 system ROM, 2 BASIC lo, 4 BASIC hi, 8 characters */
    uint32_t raster[C128_VIC_W * C128_VIC_H];
    uint8_t cbm_hold;                   /* the C= key is held for the Z80's start-up code, to make it start C64 mode */
    uint8_t mon80;                      /* the monitor is on the RGBI output (the VDC, 80 columns), not the composite one */
    kbd_t kbd;
    uint16_t key_matrix[12];            /* per line: the PB bits closed; lines 0-7 are PA bits, 8-10 the K lines, 11 RESTORE */
    uint8_t caps, key4080;              /* the two latching keys */
    uint8_t joy[2];                     /* closed switches: bit 0 up, 1 down, 2 left, 3 right, 4 fire */
    int pin_col[8], pin_row[8], pin_k[3], pin_restore, pin_caps, pin_4080, pin_joy[2][5];
    int pin_por, pin_tod[2], net_gnd;
} c128_t;

static inline c128_t *C128(board_t *b) { return (c128_t *)b; }
extern const machine_t machine_c128;
static inline bool board_is_c128(const board_t *b) { return b->m == &machine_c128; }
bool c128_disk_insert(board_t *b, const char *path, char *msg, int msglen);
void c128_disk_eject(board_t *b);
bool c128_disk_active(board_t *b);      /* the drive is working on the bus: for its light */
void c128_set_c64(board_t *b, bool on);
bool c128_is_c64(board_t *b);
void c128_set_monitor80(board_t *b, bool on);
const char *c128_key_legend(int line, int col);
#endif
