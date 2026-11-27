/* cpc464.h - the Amstrad CPC 464 main board (Z70100). The netlist is typed in from the CPC 464 circuit diagram and checked by continuity
 * (cpc464_board_data.c, by tools/cpc464_netlist2c.py, docs/CPC464_BEEP_CHECKS.md); the gate array (IC116), the 8255 (IC107), the AY-3-8912
 * (IC102) and the ROM are pin level models in cpc464.c, the Z80, 6845, 74LS parts and 4164 DRAMs are the simulator's own. */
#ifndef CPC464_H
#define CPC464_H
#include "board.h"
#include "chips_common.h"
#include "z80.h"
#include "kbd.h"
#include "ay8912.h"

#define CPC_FREQ             16000000   /* one cycle = one period of the 16 MHz crystal */
#define CPC_STEPS_PER_CYCLE  2          /* half periods */
#define CPC_CYCLES_PER_FRAME (16 * 64 * 312)
#define CPC_SCREEN_W         768        /* 48 microseconds of 16 mode 2 pixels */
#define CPC_SCREEN_H         272

typedef struct cpc464 {
    board_t board;              /* first: a cpc464_t is a board_t */
    z80_t cpu;
    uint64_t cpu_pins;
    int p_cpu, p_ga, p_crtc, p_ppi, p_ay, p_rom, p_osc;
    int pin_clk, clk;
    bool in_reset;
    uint8_t xtal;          /* level of the 16 MHz oscillator (stands in for X101 and the gates around it) */
    uint8_t xtal_seen;     /* the level of the gate array's XTAL pin at the last step */
    int32_t cache_addr, cache_data;
    uint8_t rom[32768];         /* IC103: lower ROM (OS) in the first 16K, upper ROM (BASIC) in the second */
    bool rom_ok;
    kbd_t kbd;
    uint16_t key_matrix[8];     /* per matrix bit (AY port A line), the rows that are pressed */
    int pin_row[10], pin_col[8];
    uint8_t joy;                /* joystick J102 (bit 0 up, 1 down, 2 left, 3 right, 4 fire 2, 5 fire 1): the directions it closes to the common pin */
    int pin_jcom, pin_joy[6];
    int pin_rc;
    uint64_t reset_until;
} cpc464_t;

static inline cpc464_t *CPC(board_t *b) { return (cpc464_t *)b; }
extern const machine_t machine_cpc464;
static inline bool board_is_cpc464(const board_t *b) { return b->m == &machine_cpc464; }
void cpc464_run_steps(board_t *b, int steps);
void cpc464_ga_status(board_t *b, char *buf, int len);
#endif
