/* parts.h - part types: pin descriptions and simple pin-level models for
 * the glue logic and memories of the C64 board. The big chips (6510, VIC-II,
 * CIAs, SID) are wrapped in board.c around the existing emulator cores. */
#ifndef PARTS_H
#define PARTS_H

#include "sim.h"
#include <stddef.h>

typedef struct {
    const char *name;       /* label drawn on the chip, e.g. "74LS08" */
    /* Pins in order 1..N as "NAME:K", K = kind:
     *   T  TTL input, re-evaluate on change (floating reads 1)
     *   M  MOS input, re-evaluate on change (floating reads the charge)
     *   m  MOS input sampled only at clock edges / by the chip core
     *   O  output (or analog pin not simulated)
     *   B  bidirectional MOS pin (data bus), sampled by the core
 *   X  bidirectional pin that re-evaluates the part when it changes
     *   P  power pin (if open, the chip is dead)
 *   S  analog switch terminal (4066): passes any drive through */
    const char *pins;
    part_fn eval, commit, reset;
    size_t state_size;
    int tech;               /* TECH_* (default NMOS) */
} part_type_t;

extern const part_type_t PT_6510, PT_6569, PT_6526, PT_6581;
extern const part_type_t PT_82S100, PT_2364, PT_2332, PT_2114, PT_4164;
extern const part_type_t PT_74LS08, PT_7406, PT_74LS139, PT_74LS257, PT_74LS258;
extern const part_type_t PT_74LS373, PT_4066, PT_556, PT_74LS629;
extern const part_type_t PT_74LS74, PT_74LS193, PT_MC4044;
extern const part_type_t PT_4416, PT_4116, PT_4532, PT_UPD23128, PT_HN613128, PT_74LS157, PT_74LS32, PT_74LS00, PT_LM1889;
/* The other chips for the 4532's place (zx48.c: the upper RAM option). */
extern const part_type_t PT_4532_4, PT_3732L, PT_3732H, PT_4164_ZX;
/* The Issue 6A's glue: a hex inverter, and the ZX8401 (its 15
 * multiplexer sections: output, A and B pins; see parts.c). */
extern const part_type_t PT_74LS04, PT_ZX8401;
#define ZX8401_NMUX 15
extern const int zx8401_mux[ZX8401_NMUX][3];
/* The C64 ASSY 250469's: its RAMs, BASIC and KERNAL ROM, a Schmitt
 * trigger, the clock generator, the HMOS 6510, VIC-II and SID, and the
 * Super PLA (see parts.c). */
extern const part_type_t PT_41464, PT_27128, PT_74LS14, PT_8701, PT_8500, PT_8565, PT_8580, PT_SUPERPLA;

/* The Commodore PET 8032080's: TTL glue, the 16 MHz timing chain, the
 * character ROM, the IEEE-488 transceivers, the oscillator and the
 * power-up reset (pet_parts.c). */
extern const part_type_t PT_74LS02, PT_74LS10, PT_74LS20, PT_7425, PT_74LS86, PT_74S04, PT_74S04_OSC, PT_7417;
extern const part_type_t PT_74LS138, PT_74LS145, PT_74154, PT_74LS244, PT_MC3446;
extern const part_type_t PT_ZX_LS93, PT_ZX_LS165, PT_ZX_LS365, PT_ZX_ROM, PT_ZX_OSC86, PT_ZX_DIFF;   /* zx80_parts.c */
extern const part_type_t PT_74LS74R, PT_74S74, PT_74177, PT_74LS164, PT_74166;
extern const part_type_t PT_2316, PT_XTAL16, PT_LM555, PT_2114_PET;
/* pet_chips.c: the PET's 6502, 6520 (x2), 6522 and 6545 around the cores in chips/ */
extern const part_type_t PT_PET6502, PT_PET6520, PT_PET6522, PT_VIC6522, PT_PET6545;
uint32_t pet_cpu_cycles_of(const part_t *p);
void pet_cpu_regs_of(const part_t *p, char *buf, int len);
uint8_t pet_crtc_reg_of(const part_t *p, int r);

/* The VIC-20 (vic20_chips.c, parts.c): the 6560 video chip, 74LS245, 74LS133, the
 * 14.31818 MHz crystal, the character ROM and the 6116 RAM of the 250403. */
/* The Commodore 16 (c16_chips.c): the TED, the 7501, the 6529B, the 7700 PLA and the small chips. */
extern const part_type_t PT_7360, PT_7501, PT_6529B, PT_7700, PT_74LS175, PT_74LS125, PT_C16ROM, PT_74LS27, PT_6551, PT_8721, PT_8722;
extern const part_type_t PT_74LS74_128, PT_7407, PT_C128ROM, PT_C128CHR, PT_8502, PT_Z80A, PT_6526_128, PT_6581_128, PT_8563, PT_8564;
void c128_rom_set(part_t *p, const uint8_t *data, unsigned size);
void c128_vic_set_screen(part_t *p, uint32_t *fb, int w, int h);
bool c128_vic_fast(const part_t *p);
uint8_t c128_vic_reg_of(const part_t *p, int r);
uint32_t c128_cpu_cycles_of(const part_t *p);
void c128_cpu_regs_of(const part_t *p, char *buf, int len);
int c128_cpu_sync_of(const part_t *p, unsigned *addr);
void c128_z80_regs_of(const part_t *p, char *buf, int len);
void ted_set_screen(part_t *p, uint32_t *screen);
uint8_t ted_reg_of(const part_t *p, int r);
uint32_t ted_frame_of(const part_t *p);
bool ted_rom_on(const part_t *p);
void ted_status_of(const part_t *p, char *buf, int len);
uint32_t c16_cpu_cycles_of(const part_t *p);
void c16_cpu_regs_of(const part_t *p, char *buf, int len);

extern const part_type_t PT_6560, PT_74LS245, PT_74LS133, PT_XTAL14, PT_2332L, PT_6116;
/* The raster picture the 6560 draws (VIC_W x VIC_H pixels) goes to SCREEN. */
#define VIC_W 224
#define VIC_H 232
void vic_set_screen(part_t *p, uint32_t *screen);
uint8_t vic_reg_of(const part_t *p, int r);
uint32_t vic_frame_of(const part_t *p);

/* A 64K x 1 DRAM, whole or half good (a 4164, 4532, 3732). */
bool part_is_dram64(const part_t *p);

/* A memory chip that can take memory faults (MF_*): its address lines and
 * data bits, and whether it is a DRAM. 0 if it isn't a memory chip. */
int part_memory(const part_t *p, int *data_bits, bool *dram);

/* 82S100 PLA: its logic as a table of product terms (see parts.c). The
 * input word has bit PI_x set when that input pin is high; the outputs are
 * pin levels, bit PO_x. Pin names as in the schematic (/LORAM, /CAS...). */
enum { PI_A13, PI_A14, PI_A15, PI_VA14, PI_CHAREN, PI_HIRAM, PI_LORAM, PI_CAS,
       PI_VA12, PI_VA13, PI_GAME, PI_EXROM, PI_RW, PI_AEC, PI_BA, PI_A12, PLA_NIN };
enum { PO_ROMH, PO_ROML, PO_IO, PO_GRW, PO_CHAROM, PO_KERNAL, PO_BASIC, PO_CASRAM, PLA_NOUT };
typedef struct {
    uint16_t mask, value;   /* true when (inputs & mask) == value */
    uint8_t outputs;        /* bit PO_x set: the term feeds that output */
} pla_term_t;
extern const pla_term_t pla_terms[];
extern const int pla_nterms;
extern const int pla_in_pin[PLA_NIN], pla_out_pin[PLA_NOUT];
unsigned pla_read_inputs(sim_t *s, part_t *p);
unsigned pla_logic(unsigned inputs);
/* The 250469's Super PLA, modeled with the same table: its pin for each
 * input (PI_AEC is /AEC inside, from its AEC pin) and output (0: the
 * output stays inside, /IO feeds its I/O decoder and GR/W nothing), and
 * the input word as pla_read_inputs() makes it. */
extern const int superpla_in_pin[PLA_NIN], superpla_out_pin[PLA_NOUT];
unsigned superpla_read_inputs(sim_t *s, part_t *p);

/* Attach a type to a part: pin names/flags, functions, state. */
void part_set_type(sim_t *s, int part, const part_type_t *t);
/* Read/drive a group of pins as a binary number (pins[0] is bit 0). */
unsigned part_read_bus(sim_t *s, part_t *p, const int *pins, int n);
void part_drive_bus(sim_t *s, part_t *p, const int *pins, int n, unsigned v, bool enable);
/* Same, but does nothing if value and enable are the same as the last time
 * this cache was used. A pure speed-up: sim_drive() would ignore unchanged
 * drives anyway, one pin at a time. A zeroed cache means "unknown". */
static inline void part_drive_bus_cached(sim_t *s, part_t *p, const int *pins, int n,
                                         unsigned v, bool enable, int32_t *cache) {
    int32_t key = enable ? (int32_t)v + 1 : -1;
    if (*cache == key) return;
    *cache = key;
    part_drive_bus(s, p, pins, n, v, enable);
}

/* ROM state: the image the ROM contains. */
typedef struct { const uint8_t *data; int32_t out_cache; } rom_state_t;
/* DRAM state: one byte per bit cell (simple, not compact). */
typedef struct {
    uint8_t cells[65536];
    uint8_t row, col;
    uint8_t ras_prev, cas_prev;
    uint8_t out_en, out_bit;
} dram_state_t;
/* 64K x 4 DRAM: one nibble per cell. */
typedef struct {
    uint8_t cells[65536];
    uint8_t row, col;
    uint8_t ras_prev, cas_prev;
    uint8_t out_en, out_val;
    int32_t out_cache;
    uint8_t a6_row_bit;     /* the row address bit that is CPU A6 (set by the board) */
} dram4_state_t;
/* Color RAM. */
typedef struct { uint8_t cells[1024]; int32_t out_cache; } sram_state_t;
/* 556: power-on reset and RESTORE debouncer. */
typedef struct {
    uint64_t reset_until;   /* OUT2 high (CPU in reset) until this cycle */
    uint64_t nmi_until;     /* OUT1 high (NMI) until this cycle */
    int restore_net;        /* net of the RESTORE key (seen through C38) */
    uint8_t restore_prev;
    uint64_t last_cycle;    /* time only moves once per cycle */
} timer556_state_t;
/* 74LS629: dot clock and color clock oscillator. */
typedef struct { bool crystal_ok; } vco_state_t;

/* Resistors, links and diodes: what each element does electrically.
 *   PULLUP/PULLDOWN  pin_a to a rail through the resistor
 *   LINK             a wire (a zero ohm resistor, a ferrite bead)
 *   SERIES           a series resistor between two logic nets: each side
 *                    sees what drives the other, weakened (S_RES)
 *   DIODE            pin_a anode, pin_b cathode: a low cathode pulls the
 *                    anode low (the ZX Spectrum keyboard)
 *   DELAY            an RC delay: pin_b (the capacitor) follows pin_a one
 *                    bus step later
 *   NPN              a transistor switch: a high base (pin_a) pulls the
 *                    collector (pin_b) low
 *   FOLLOWER         an emitter follower: a high base (pin_a) drives the
 *                    emitter (pin_b) high
 *   ANALOG           not simulated */
enum { EL_PULLUP, EL_PULLDOWN, EL_LINK, EL_ANALOG, EL_SERIES, EL_DIODE, EL_DELAY, EL_NPN, EL_FOLLOWER };
typedef struct { int pin_a, pin_b, type; } element_t;
typedef struct { element_t el[16]; int nel; uint8_t delayed[16]; } resistor_state_t;
void resistor_eval(sim_t *s, part_t *p);
void resistor_commit(sim_t *s, part_t *p);
/* Make an element one of the active kinds above (sets pin flags, eval and
 * commit of the part). */
void element_set_type(sim_t *s, part_t *p, int index, int type);

#endif
