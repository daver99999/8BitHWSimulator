/* ts1500.c - the Timex Sinclair 1500, board with the 68 pin gate array U5.
 *
 * THE NETLIST IS TRANSCRIBED FROM THE SCHEMATIC "TS-1500 Rev 1.0, redrawn 2015" (Dick Berry), which prints every pin number of
 * U5. Ten of them (/M1 2, /NMI 3, /IORQ 4, /RD 5, /WR 6, /MREQ 7, /HALT 8, /WAIT 9, /RESET 10, /RFSH 16) were also checked with
 * continuity readings on a real board, and agree. See docs/TS1500.md and tools/ts1500-netlist-src/sheet01.py.
 *
 * WHAT U5 IS. Not a Ferranti ULA but a custom 68 pin NCR gate array. From the schematic it is the ULA's job (CPU clock from
 * the 6.5 MHz oscillator, ROM select, the video, the NMI line counter, the keyboard and tape port, the WAIT line that
 * the TS1000 makes with a transistor) plus the control of 8 4116 DRAMs: /RAS, /CAS, /WE and the multiplexed address
 * RA0-RA7, and /DEN and /AEN, which switch the data transceiver U6 (74LS245) and the address buffer U3 (74LS244).
 * Its logic is the ULA's (ula81.c, with its own pin table); what is U5's own is the DRAM timing. Like the ULA, it is NOT a
 * copy of the gates but of the documented behaviour, checked by what matters: the ROM boots and the keys work.
 *
 * HOW TIME MOVES: as the TS1000's (ts1000.c): U5 advances every half period of the 6.5 MHz oscillator, the board
 * settles, the Z80 sees an edge of its clock if U5 made one, the memories see their strobes, the board settles again.
 *
 * ASSUMED where the schematic does not say: which U5 pins are inputs or outputs (RAMRM, RAMCS, pin 18), the keyboard pull-ups
 * (inside U5 here), which of pins 57 and 58 is on the Z80's CLK (both carry the clock here), the DRAM refresh (RAS only cycles are not made: the DRAM
 * model keeps its data), and the character bitmap path in a video fetch (read from the ROM's outputs here).
 * NOT modeled: the power supply, the modulator and the video signal levels, the tape circuits, the expansion pack. */
#include "ts1500.h"
#include "sokol/sokol_app.h"   /* key codes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

/* ------------------------------------------------------------------------ */
/* The chips                                                                 */
/* ------------------------------------------------------------------------ */

static const int t5_addr_pins[16] = { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5 };
static const int t5_data_pins[8] = { 14, 15, 12, 8, 7, 9, 10, 13 };
enum { T5Z_CLK = 6, T5Z_INT = 16, T5Z_NMI = 17, T5Z_HALT = 18, T5Z_MREQ = 19, T5Z_IORQ = 20, T5Z_RD = 21, T5Z_WR = 22,
       T5Z_BUSAK = 23, T5Z_WAIT = 24, T5Z_BUSRQ = 25, T5Z_RESET = 26, T5Z_M1 = 27, T5Z_RFSH = 28 };

static void t5_cpu_eval(sim_t *s, part_t *p) {
    ts1500_t *z = T5(p->user);
    uint64_t pins = z->cpu_pins;
    bool aoe = (pins & Z80_AOE) != 0;
    part_drive_bus_cached(s, p, t5_addr_pins, 16, Z80_GET_ADDR(pins), aoe, &z->cache_addr);
    pin_out(s, p, T5Z_MREQ, aoe ? DRV_BIT(!(pins & Z80_MREQ)) : DRV_Z);
    pin_out(s, p, T5Z_IORQ, aoe ? DRV_BIT(!(pins & Z80_IORQ)) : DRV_Z);
    pin_out(s, p, T5Z_RD, aoe ? DRV_BIT(!(pins & Z80_RD)) : DRV_Z);
    pin_out(s, p, T5Z_WR, aoe ? DRV_BIT(!(pins & Z80_WR)) : DRV_Z);
    pin_out(s, p, T5Z_M1, DRV_BIT(!(pins & Z80_M1)));
    pin_out(s, p, T5Z_RFSH, DRV_BIT(!(pins & Z80_RFSH)));
    pin_out(s, p, T5Z_HALT, DRV_BIT(!(pins & Z80_HALT)));
    pin_out(s, p, T5Z_BUSAK, DRV_BIT(!(pins & Z80_BUSAK)));
    part_drive_bus_cached(s, p, t5_data_pins, 8, Z80_GET_DATA(pins), (pins & Z80_DOE) != 0, &z->cache_data);
}
static const part_type_t PT_T5Z80 = { "Z80A",
    "A11:O A12:O A13:O A14:O A15:O CLK:m D4:B D3:B D5:B D6:B VCC:P D2:B D7:B D0:B D1:B /INT:m /NMI:m "
    "/HALT:O /MREQ:O /IORQ:O /RD:O /WR:O /BUSAK:O /WAIT:m /BUSRQ:m /RESET:m /M1:O /RFSH:O GND:P A0:O "
    "A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O A10:O", t5_cpu_eval, 0, 0, 0, TECH_NMOS };

static void t5_cpu_edge(board_t *b, int clk) {
    ts1500_t *z = T5(b);
    sim_t *s = &b->sim;
    part_t *p = &s->parts[z->p_cpu];
    if (p->dead) return;
    uint64_t pins = clk ? Z80_CLK : 0;
    Z80_SET_DATA(pins, (uint64_t)part_read_bus(s, p, t5_data_pins, 8));
    if (!pin_in(s, p, T5Z_INT)) pins |= Z80_INT;
    if (!pin_in(s, p, T5Z_NMI)) pins |= Z80_NMI;
    if (!pin_in(s, p, T5Z_WAIT)) pins |= Z80_WAIT;
    if (!pin_in(s, p, T5Z_RESET)) pins |= Z80_RES;
    if (!pin_in(s, p, T5Z_BUSRQ)) pins |= Z80_BUSRQ;
    z->in_reset = (pins & Z80_RES) != 0;
    uint64_t out = z80_tick(&z->cpu, pins);
    if (out != z->cpu_pins) { z->cpu_pins = out; sim_mark_dirty(s, z->p_cpu); }
}


/* The ROM (U2, 2364) is the shared 2364 part (PT_2364). Its data pins (the primed bus D0'-D7'): */
static const int t5r_data_pins[8] = { 9, 10, 11, 13, 14, 15, 16, 17 };

/* The 4116 DRAMs (U8-U15, 16K x 1): the shared 4116 with the -5V and +12V pins unchecked (those supplies are not modeled). */
static void t5_dram_eval(sim_t *s, part_t *p) { PT_4116.eval(s, p); }
static void t5_dram_commit(sim_t *s, part_t *p) { PT_4116.commit(s, p); }
static void t5_dram_reset(sim_t *s, part_t *p) { PT_4116.reset(s, p); }
static const part_type_t PT_T5DRAM = { "4116",
    "VBB:X D:m /WE:m /RAS:m A0:m A2:m A1:m VDD:X VCC:P A5:m A4:m A3:m A6:m Q:O /CAS:m VSS:P",
    t5_dram_eval, t5_dram_commit, t5_dram_reset, sizeof(dram_state_t), TECH_NMOS };

/* 74LS245 (U6): the B side (pins 11-18) is the CPU bus, the A side (pins 2-9) the memories' bus D0'-D7'. DIR (on /WR) high sends A to B (memories to CPU, a read), low B to A; /G high isolates. */
static void t5_245_eval(sim_t *s, part_t *p) {
    static const int a[8] = { 2, 3, 4, 5, 6, 7, 8, 9 }, bb[8] = { 18, 17, 16, 15, 14, 13, 12, 11 };
    bool en = !pin_in(s, p, 19), dir = pin_in(s, p, 1);
    if (!en) {
        part_drive_bus(s, p, a, 8, 0, false);
        part_drive_bus(s, p, bb, 8, 0, false);
    } else if (dir) {
        part_drive_bus(s, p, a, 8, 0, false);
        part_drive_bus(s, p, bb, 8, part_read_bus(s, p, a, 8), true);
    } else {
        part_drive_bus(s, p, bb, 8, 0, false);
        part_drive_bus(s, p, a, 8, part_read_bus(s, p, bb, 8), true);
    }
}
static const part_type_t PT_T5245 = { "74LS245",
    "DIR:T A1:X A2:X A3:X A4:X A5:X A6:X A7:X A8:X GND:P B8:X B7:X B6:X B5:X B4:X B3:X B2:X B1:X /G:T VCC:P",
    t5_245_eval, 0, 0, 0, TECH_TTL };

/* ------------------------------------------------------------------------ */
/* U5, the gate array                                                        */
/* ------------------------------------------------------------------------ */
/* Pin numbers: the schematic's (see the header). The address and data pins are not in runs, so they are tables. */
enum {
    G_KBD0 = 1, G_M1 = 2, G_NMI = 3, G_IORQ = 4, G_RD = 5, G_WR = 6, G_MREQ = 7, G_HALT = 8, G_WAIT = 9, G_RESET = 10,
    G_RAMRM = 11, G_DEN = 12, G_AEN = 13, G_ROMCS = 15, G_RFSH = 16, G_A8P = 48, G_VCC = 52, G_TV = 53, G_CHSEL = 54,
    G_OSCOUT = 55, G_OSCIN = 56, G_CPUCLK = 57, G_SYSCLK = 58, G_TAPEIN = 59, G_GND = 60, G_CAS = 62, G_RAS = 63, G_WRD = 64
};
static const int t5_ga_a0p[9] = { 46, 51, 19, 24, 28, 32, 36, 40, 48 };            /* A0'..A7' (to U3's outputs / the ROM), A8 */
static const int t5_ga_ahi[7] = { 49, 22, 26, 30, 34, 38, 42 };                    /* A9..A15 */
static const int t5_ga_ra[8] = { 44, 50, 23, 27, 31, 35, 39, 43 };                 /* RA0..RA7 */
static const int t5_ga_d[8] __attribute__((unused)) = { 25, 29, 33, 37, 41, 47, 20, 17 };                  /* D0..D7 */
static const int t5_ga_kbd[5] = { 1, 68, 67, 66, 65 };                             /* KBD0..KBD4 */
static uint8_t t5_rom_bitmap(sim_t *s);
static const ula81_pins_t t5_ga_pins = {
    .a0p = 46, .a1p = 51, .a2p = 19, .a7p = 40, .a8p = G_A8P,
    .rd = G_RD, .iorq = G_IORQ, .wr = G_WR, .mreq = G_MREQ, .m1 = G_M1, .a14 = 38, .a15 = 42,
    .ramcs = G_RAMRM, .romcs = G_ROMCS, .clk = G_CPUCLK, .clk_true = true, .nmi = G_NMI, .tv = G_TV, .halt = G_HALT,
    .tape = G_TAPEIN, .ukus = 0, .push_pull = true, .bitmap = t5_rom_bitmap,
    .data = { 25, 29, 33, 37, 41, 47, 20, 17 }, .kbd = { 1, 68, 67, 66, 65 },
    .alt = { 46, 51, 19, 24, 28, 32, 36, 40, 48 },
};
typedef struct {
    ula81_state_t ula;      /* first: the ULA logic runs on this */
    bool ras, cas;          /* the DRAM strobes */
    uint8_t acc;            /* steps the memory cycle has been active */
    bool chg;
} t5_ga_t;

static void t5_ga_eval(sim_t *s, part_t *p) {
    t5_ga_t *g = p->state;
    ula81_state_t *u = &g->ula;
    ula81_eval(s, p);
    bool mreq = !pin_in(s, p, G_MREQ), wr = !pin_in(s, p, G_WR);
    pin_out(s, p, G_SYSCLK, DRV_BIT(u->cpuclk));                                     /* SYS Clk (the edge connector); pin 57 is driven by the ULA logic */
    pin_out(s, p, G_RESET, DRV_PULLUP);                                              /* the power-on RC (C23) pulls /RESET up: INFERRED */
    pin_out(s, p, G_CHSEL, DRV_LO);
    for (int i = 0; i < 5; i++) pin_out(s, p, t5_ga_kbd[i], DRV_PULLUP);           /* the keyboard columns */
    /* /WAIT (the TS1000 makes it with TR1): low when the NMI line is active and the CPU is not halted. */
    bool nmi_on = u->nmi_en && ula81_hsync_cycle(u);
    pin_out(s, p, G_WAIT, DRV_BIT(!(nmi_on && pin_in(s, p, G_HALT))));
    /* The data transceiver is off while U5 forces a NOP on the bus, while the ROM answers U5's character address, and for I/O cycles; the address buffer is off while U5
     * drives the character address (A0'-A8') in the refresh half of a video fetch. */
    pin_out(s, p, G_DEN, DRV_BIT(!(mreq && u->vf != 1 && u->vf != 2)));
    pin_out(s, p, G_AEN, DRV_BIT(u->vf == 2));
    /* The DRAM: /WE follows /WR, the address is the row (A0-A6) until /CAS falls, then the column (A7-A13). */
    pin_out(s, p, G_RAS, DRV_BIT(!g->ras));
    pin_out(s, p, G_CAS, DRV_BIT(!g->cas));
    pin_out(s, p, G_WRD, DRV_BIT(!wr));
    unsigned a = 0;
    for (int i = 0; i < 9; i++) if (pin_in(s, p, t5_ga_a0p[i])) a |= 1u << i;
    for (int i = 0; i < 5; i++) if (pin_in(s, p, t5_ga_ahi[i])) a |= 1u << (9 + i);
    unsigned ra = g->cas ? (a >> 7) & 0x7F : a & 0x7F;
    for (int i = 0; i < 7; i++) pin_out(s, p, t5_ga_ra[i], DRV_BIT((ra >> i) & 1));
    pin_out(s, p, t5_ga_ra[7], DRV_LO);
}

static void t5_ga_reset(sim_t *s, part_t *p) {
    t5_ga_t *g = p->state;
    uint32_t *fb = g->ula.fb;
    bool running = g->ula.running;
    memset(g, 0, sizeof(*g));
    g->ula.fb = fb; g->ula.running = running; g->ula.pm = &t5_ga_pins;
    g->ula.cpuclk = true;
    g->ula.fy = -1;
    (void)s; (void)p;
}

/* One half period of the oscillator: the ULA logic, then the memory strobes. */
static void t5_ga_step(sim_t *s, part_t *p) {
    t5_ga_t *g = p->state;
    if (p->dead) return;
    bool mreq = !pin_in(s, p, G_MREQ), rd = !pin_in(s, p, G_RD), wr = !pin_in(s, p, G_WR);
    bool a14 = pin_in(s, p, t5_ga_ahi[5]), rfsh = !pin_in(s, p, G_RFSH);
    bool ras = mreq && a14 && !rfsh;
    g->acc = ras && (rd || wr) ? (uint8_t)(g->acc < 3 ? g->acc + 1 : 3) : 0;
    bool cas = ras && g->acc >= 3;      /* /RAS first, then the column: the data of a write are valid by then */
    g->ula.hold_fetch = !g->cas;        /* a video fetch reads the character from the DRAM: wait until /CAS has been low for a step */
    ula81_step(s, p);
    if (ras != g->ras || cas != g->cas) { g->ras = ras; g->cas = cas; sim_mark_dirty(s, (int)(p - s->parts)); }
}

static const part_type_t PT_T5GA = { "U5 gate array",
    "KBD0:B M1:T /NMI:O IORQ:T RD:T WR:T MREQ:T HALT:T /WAIT:O RESET:B RAMRM:O /DEN:O /AEN:O RAMCS:X /ROMCS:O RFSH:T D7:B CONT:X "
    "A2P:B D6:B HZ:X A10:T RA2:O A3P:B D0:B A11:T RA3:O A4P:B D1:B A12:T RA4:O A5P:B D2:B A13:T RA5:O A6P:B D3:B A14:T RA6:O "
    "A7P:B D4:B A15:T RA7:O RA0:O NC45:X A0P:B D5:B A8P:B A9:T RA1:O A1P:B +5V:P TV/TAPE:O CHSEL:O OSCOUT:X OSCIN:X "
    "CPUCLK:O SYSCLK:O TAPEIN:T GND:P NC61:X /CAS:O /RAS:O WRD:O KBD4:B KBD3:B KBD2:B KBD1:B",
    t5_ga_eval, 0, t5_ga_reset, sizeof(t5_ga_t), TECH_TTL };

static const part_def_t t5_part_defs[] = {
    { "U5", &PT_T5GA, "U5", ROLE_CHIP, "U5\nGATE ARRAY", "Custom 68 pin gate array (NCR): the ULA's job (clock, ROM select, video, sync, NMI, keyboard, tape port, WAIT) and the DRAM control (/RAS, /CAS, /WE, RA0-RA7) and the buffer controls /DEN, /AEN. Pin numbers from the schematic" },
    { "U7", &PT_T5Z80, "CPU", ROLE_CHIP, "Z80A", "Z80A CPU, 3.25 MHz" },
    { "U2", &PT_2364, "ROM", ROLE_ROM, "8K ROM\n$0000-$1FFF", "ROM: BASIC and the character set, 8K (the TS1500's own: TS1510 cartridge support, fixes)" },
    { "U3", &PT_74LS244, "74LS244", ROLE_GLUE, "buffer", "Address buffer: CPU A0-A7 to the ROM (A0'-A7'); off (/AEN high) while U5 drives the character address in a video fetch" },
    { "U6", &PT_T5245, "74LS245", ROLE_GLUE, "buffer", "Data transceiver between the CPU bus and the memories' bus (D0'-D7'); /G is U5's /DEN, DIR is /WR" },
    { "U10", &PT_T5DRAM, "DRAM", ROLE_VRAM, "4116\nD0", "16K x 1 dynamic RAM, data bit 0 ($4000-$7FFF)" },
    { "U9", &PT_T5DRAM, "DRAM", ROLE_VRAM, "4116\nD1", "16K x 1 dynamic RAM, data bit 1" },
    { "U8", &PT_T5DRAM, "DRAM", ROLE_VRAM, "4116\nD2", "16K x 1 dynamic RAM, data bit 2" },
    { "U11", &PT_T5DRAM, "DRAM", ROLE_VRAM, "4116\nD3", "16K x 1 dynamic RAM, data bit 3" },
    { "U12", &PT_T5DRAM, "DRAM", ROLE_VRAM, "4116\nD4", "16K x 1 dynamic RAM, data bit 4" },
    { "U13", &PT_T5DRAM, "DRAM", ROLE_VRAM, "4116\nD5", "16K x 1 dynamic RAM, data bit 5" },
    { "U14", &PT_T5DRAM, "DRAM", ROLE_VRAM, "4116\nD6", "16K x 1 dynamic RAM, data bit 6" },
    { "U15", &PT_T5DRAM, "DRAM", ROLE_VRAM, "4116\nD7", "16K x 1 dynamic RAM, data bit 7" },
};

static const conn_def_t t5_conn_defs[] = {
    { "P1", "Edge connector", "1=D7' 2=+5V 3=/RAMRM 4=+9V 7=D0' 8=GND 9=D1' 10=GND 11=D2' 12=SYS_CLK 13=D6' 14=A0 15=D5' 16=A1 17=D3' 18=A2 "
                              "19=D4' 20=A3 21=/INT 22=A15 23=/NMI 24=A14 25=/HALT 26=A13 27=/MREQ 28=A12 29=/IORQ 30=A11 31=/RD "
                              "32=A10 33=/WR 34=A9 35=/BUSAK 36=A8 37=/WAIT 38=A7 39=/BUSRQ 40=A6 41=/RESET 42=A5 43=/M1 44=A4 "
                              "45=/RFSH 46=/ROMCS" },
    { "J5", "KBD COLS", "1=KBD0 2=KBD1 3=KBD2 4=KBD3 5=KBD4" },
    { "J6", "KBD ROWS", "1=A8 2=A9 3=A10 4=A11 5=A12 6=A13 7=A14 8=A15" },
    { "J2", "MIC jack", "1=MIC 2=sleeve" },
    { "J1", "EAR jack", "1=EAR 2=GND" },
    { "MOD", "Modulator", "1=video 2=CH_SEL 3=GND 4=+5V" },
    { "J3", "9V input", "1=+9V 2=GND" },
};

static int t5_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int t5_kind_of(const char *ref) {
    if (!strcmp(ref, "X1") || !strcmp(ref, "MOD") || !strcmp(ref, "P1") || (ref[0] == 'J' && ref[1] >= '0' && ref[1] <= '9')) return PART_CONNECTOR;
    if (ref[0] == 'U') return PART_IC;
    if (ref[0] == 'R' && (ref[1] >= '0' && ref[1] <= '9')) return PART_RESISTOR;
    return PART_OTHER;      /* the diodes (made elements in t5_create) */
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static part_t *t5_find(board_t *b, const char *ref) {
    int i = sim_find_part(&b->sim, ref);
    return i >= 0 ? &b->sim.parts[i] : NULL;
}
static int t5_pin_of(board_t *b, const char *ref, int number) {
    part_t *p = t5_find(b, ref);
    return p ? part_pin(p, number) : -1;
}

/* Turn a part into one active element: pin_a, pin_b as the element's two sides. */
static void t5_make_element(board_t *b, const char *ref, int pin_a, int pin_b, int type) {
    part_t *p = t5_find(b, ref);
    if (!p || part_pin(p, pin_a) < 0 || part_pin(p, pin_b) < 0) return;
    resistor_state_t *st = p->state;
    if (!st) { st = calloc(1, sizeof(*st)); p->state = st; p->state_size = sizeof(*st); }
    p->kind = PART_RESISTOR;
    st->nel = 1;
    st->el[0] = (element_t){ part_pin(p, pin_a), part_pin(p, pin_b), EL_ANALOG };
    element_set_type(&b->sim, p, 0, type);
}

static void t5_init_keymap(ts1500_t *z) {
    /* The matrix of the Spectrum: column = address line A8+n, line = KBD0..KBD4. The ZX81's shifted symbols
     * are on the keys the ROM's table says (the 'K-SHIFT' table at $00A5). */
    kbd_init(&z->kbd, 1);
    kbd_register_modifier(&z->kbd, 0, 0, 0);     /* shift */
    const char *keymap =
        " zxcv" "asdfg" "qwert" "12345" "09876" "poiuy" " lkjh" " .mnb"
        " ZXCV" "ASDFG" "QWERT" "     " "     " "POIUY" " LKJH" " .MNB"
        " :;?/" "     " "     " "     " "     " "\")($ " " =+- " " ,><*";
    for (int layer = 0; layer < 3; layer++)
        for (int column = 0; column < 8; column++)
            for (int line = 0; line < 5; line++) {
                int c = keymap[layer * 40 + column * 5 + line];
                if (c != ' ') kbd_register_key(&z->kbd, c, column, line, layer == 2 ? 1 : 0);
            }
    kbd_register_key(&z->kbd, ' ', 7, 0, 0);
    kbd_register_key(&z->kbd, 0x03, 7, 0, 0);    /* break is the space key */
    kbd_register_key(&z->kbd, 0x0E, 0, 0, 0);    /* shift */
    kbd_register_key(&z->kbd, 0x0D, 6, 0, 0);    /* newline */
    kbd_register_key(&z->kbd, 0x0C, 4, 0, 1);    /* rubout: shift + 0 */
    kbd_register_key(&z->kbd, 0x08, 3, 4, 1);    /* cursor left: shift + 5 */
    kbd_register_key(&z->kbd, 0x0A, 4, 4, 1);    /* down: shift + 6 */
    kbd_register_key(&z->kbd, 0x0B, 4, 3, 1);    /* up: shift + 7 */
    kbd_register_key(&z->kbd, 0x09, 4, 2, 1);    /* right: shift + 8 */
    kbd_register_key(&z->kbd, 0x07, 3, 0, 1);    /* edit: shift + 1 */
}

/* ROM file: roms/ts1500 (or roms, or .), any .rom/.bin/.u2 file of 8192 bytes (the TS1500 ROM). Not shipped. */
static void t5_find_roms(board_t *b) {
    ts1500_t *z = T5(b);
    static const char *const dirs[] = { "roms/ts1500", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]) && !z->rom_loaded; d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while (!z->rom_loaded && (e = readdir(dir))) {
            size_t l = strlen(e->d_name);
            if (l < 4 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom") && strcasecmp(e->d_name + l - 3, ".u2"))) continue;
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], e->d_name);
            FILE *f = fopen(path, "rb");
            if (!f) continue;
            uint8_t buf[8193];
            size_t n = fread(buf, 1, sizeof(buf), f);
            fclose(f);
            if (n == sizeof(z->rom)) { memcpy(z->rom, buf, n); z->rom_loaded = true; }
        }
        closedir(dir);
    }
}

static int t5_ga_bitmap_part = -1;
static sim_t *t5_ga_bitmap_sim;
/* The character bitmap row of a video fetch: the ROM's outputs (D0'-D7'). */
static uint8_t t5_rom_bitmap(sim_t *s) {
    (void)s;
    sim_t *ss = t5_ga_bitmap_sim;
    uint8_t v = ss && t5_ga_bitmap_part >= 0 ? (uint8_t)part_read_bus(ss, &ss->parts[t5_ga_bitmap_part], t5r_data_pins, 8) : 0xFF;
    return v;
}

static void t5_create(board_t *b) {
    ts1500_t *z = T5(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "U7");
    z->p_ula = sim_find_part(s, "U5");
    z->p_rom = sim_find_part(s, "U2");
    static const char *const dram_refs[8] = { "U10", "U9", "U8", "U11", "U12", "U13", "U14", "U15" };      /* D0'..D7' */
    for (int i = 0; i < 8; i++) z->p_dram[i] = sim_find_part(s, dram_refs[i]);
    z->pin_clk = part_pin(&s->parts[z->p_cpu], T5Z_CLK);
    memset(z->rom, 0xFF, sizeof(z->rom));
    z->rom_loaded = false;
    t5_find_roms(b);
    ((rom_state_t *)s->parts[z->p_rom].state)->data = z->rom;
    t5_ga_t *g = s->parts[z->p_ula].state;
    g->ula.pm = &t5_ga_pins;
    g->ula.fb = b->screen;
    g->ula.running = true;                               /* X1 is not simulated: it always oscillates */
    t5_ga_bitmap_part = z->p_rom;
    t5_ga_bitmap_sim = s;
    /* R28 (1K) is in the ROM's A8 line, a series resistor: the gate array and the CPU side drive their own halves. R25 (680) is in
     * the /ROMCS line. */
    static const char *const ser[] = { "R28", "R25" };
    for (int i = 0; i < 2; i++) { part_t *p = t5_find(b, ser[i]);
        if (p && p->state && ((resistor_state_t *)p->state)->nel) element_set_type(s, p, 0, EL_SERIES); }
    /* The keyboard diodes D10-D17: the half row (pin 1) is the anode, the address line (pin 2) the cathode. */
    for (int i = 10; i <= 17; i++) { char ref[8]; snprintf(ref, sizeof(ref), "D%d", i); t5_make_element(b, ref, 1, 2, EL_DIODE); }
    for (int c = 0; c < 8; c++) z->pin_row[c] = t5_pin_of(b, "J6", 1 + c);      /* J6 pins 1..8: the half rows A8..A15 */
    for (int l = 0; l < 5; l++) z->pin_kb[l] = t5_pin_of(b, "J5", 1 + l);
    t5_init_keymap(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    z->pin_reset_net = t5_pin_of(b, "U7", 26);           /* /RESET: held low at power on (the RC of the real board is not drawn) */
    b->pin_reset = t5_pin_of(b, "U7", 26);
    b->pin_reset_gnd = t5_pin_of(b, "U7", 29);
    char name[16];
    int bit = 0;
    static const struct { const char *fmt; int n; } buses[] = { { "A%d", 16 }, { "D%d", 8 }, { "D%dP", 8 }, { "A%dP", 9 } };
    for (size_t k = 0; k < sizeof(buses) / sizeof(buses[0]); k++)
        for (int i = 0; i < buses[k].n; i++) {
            snprintf(name, sizeof(name), buses[k].fmt, i);
            sim_set_bus_bit(s, sim_find_net(s, name), bit++);
        }
}

static void t5_power_on(board_t *b) {
    ts1500_t *z = T5(b);
    z->cpu_pins = z80_init(&z->cpu);
    z->in_reset = true;
    z->clk = 0;
    z->reset_until = T5_FREQ / 20;      /* 50 ms */
    for (int i = 0; i < ULA81_PIXELS_PER_LINE * ULA81_LINES; i++) b->screen[i] = 0xFF000000u;
}

static void t5_joins(board_t *b) {
    ts1500_t *z = T5(b);
    sim_t *s = &b->sim;
    for (int l = 0; l < 5; l++)
        for (int c = 0; c < 8; c++) {
            if (!((z->key_matrix[l] >> c) & 1)) continue;
            int r = z->pin_row[c], k = z->pin_kb[l];
            if (r < 0 || k < 0 || s->pins[r].fault == PF_OPEN || s->pins[k].fault == PF_OPEN) continue;
            sim_join(s, s->pins[r].net, s->pins[k].net);
        }
}

static void t5_sync_keys(board_t *b) {
    ts1500_t *z = T5(b);
    bool changed = false;
    for (int l = 0; l < 5; l++) {
        uint16_t m = z->kbd.scanout_column_masks[l];
        if (m != z->key_matrix[l]) { z->key_matrix[l] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}
static void t5_key_down(board_t *b, int key) { kbd_key_down(&T5(b)->kbd, key); t5_sync_keys(b); }
static void t5_key_up(board_t *b, int key) { kbd_key_up(&T5(b)->kbd, key); t5_sync_keys(b); }
static void t5_frame_update(board_t *b, uint32_t us) { kbd_update(&T5(b)->kbd, us); t5_sync_keys(b); }

static int t5_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return 0x0D;
    case SAPP_KEYCODE_BACKSPACE: return 0x0C;
    case SAPP_KEYCODE_LEFT: return 0x08;
    case SAPP_KEYCODE_RIGHT: return 0x09;
    case SAPP_KEYCODE_UP: return 0x0B;
    case SAPP_KEYCODE_DOWN: return 0x0A;
    case SAPP_KEYCODE_ESCAPE: return 0x03;
    case SAPP_KEYCODE_LEFT_SHIFT: return 0x0E;
    case SAPP_KEYCODE_TAB: return 0x07;
    default: return 0;
    }
}
static int t5_host_char(int c) { return c; }

static void t5_monitor(board_t *b, uint32_t *out) {
    part_t *ula = &b->sim.parts[T5(b)->p_ula];
    b->no_signal = ula->dead;
    for (int i = 0; i < ULA81_PIXELS_PER_LINE * ULA81_LINES; i++) {
        uint32_t c = b->screen[i];
        if (ula->dead) {
            b->snow_seed = b->snow_seed * 1103515245u + 12345u;
            uint32_t g = ((b->snow_seed >> 24) & 0xFF) * 3 / 4;
            c = 0xFF000000u | (g << 16) | (g << 8) | g;
        }
        out[i] = c;
    }
}

/* ------------------------------------------------------------------------ */
/* Time                                                                      */
/* ------------------------------------------------------------------------ */

static void t5_step(board_t *b) {
    ts1500_t *z = T5(b);
    sim_t *s = &b->sim;
    t5_ga_step(s, &s->parts[z->p_ula]);
    sim_settle(s);
    int clk = sim_read(s, z->pin_clk);
    if (clk != z->clk) {
        z->clk = clk;
        t5_cpu_edge(b, clk);
        sim_settle(s);
    }
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void t5_run(board_t *b, int cycles) {
    ts1500_t *z = T5(b);
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        sim_drive(s, z->pin_reset_net, s->cycle < z->reset_until ? DRV_LO : DRV_Z);
        for (int k = 0; k < T5_STEPS_PER_CYCLE; k++) t5_step(b);
    }
}

uint8_t ts1500_ram_peek(board_t *b, uint16_t addr) {
    unsigned a = addr & 0x3FFF, cell = ((a & 0x7F) << 8) | ((a >> 7) & 0x7F);      /* row A0-A6, column A7-A13 */
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) {
        const dram_state_t *st = b->sim.parts[T5(b)->p_dram[i]].state;
        v |= (uint8_t)((st->cells[cell] & 1) << i);
    }
    return v;
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static void t5_destroy(board_t *b) { (void)b; }

static const char *t5_cpu_status(board_t *b) {
    const ts1500_t *z = T5(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!z->rom_loaded) return "no ROM (put the 8K TS1500 ROM in roms/ts1500)";
    return z->in_reset ? "in reset" : "running";
}

static void t5_cpu_regs(board_t *b, char *buf, int len) {
    const z80_t *c = &T5(b)->cpu;
    snprintf(buf, (size_t)len, "PC=%04X SP=%04X AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X",
             c->pc, c->sp, c->af, c->bc, c->de, c->hl, c->ix, c->iy);
}

static const file_kind_t *t5_file_kind(const char *path, char *info, int len, bool *bad) {
    (void)path; (void)info; (void)len; (void)bad;
    return NULL;
}
static bool t5_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)path; (void)run;
    snprintf(msg, (size_t)msglen, "the TS1500 can't load files yet");
    return false;
}

/* The screen as text: the display file is at D_FILE (system variable at $400C), 33 bytes a line. */
static void t5_print_text(board_t *b) {
    uint16_t dfile = (uint16_t)(ts1500_ram_peek(b, 0x400C) | ts1500_ram_peek(b, 0x400D) << 8);
    int col = 0, rows = 0;
    for (uint16_t a = dfile; rows < 24 && a < 0x4800 + 0x4000; a++) {
        uint8_t c = ts1500_ram_peek(b, a);
        if (c == 0x76) { putchar('\n'); col = 0; rows++; continue; }
        static const char *const sym = "\"#$:?()><=+-*/;,.";
        char ch = c == 0 ? ' ' : c >= 0x1C && c <= 0x25 ? (char)('0' + c - 0x1C) : c >= 0x26 && c <= 0x3F ? (char)('A' + c - 0x26) :
                  c >= 0x0B && c <= 0x1B ? ("\"#$:?()><=+-*/;,."[c - 0x0B]) : '?';
        (void)sym;
        putchar(ch);
        if (++col > 32) break;
    }
    putchar('\n');
}

const machine_t machine_ts1500 = {
    .id = "ts1500",
    .name = "Timex Sinclair 1500",
    .issue = "TS1500",
    .board_name = "TS1500 (schematic Rev 1.0, redrawn 2015)",
    .size = sizeof(ts1500_t),
    .data = &bd_ts1500,
    .sim = { .rail_of = t5_rail_of, .kind_of = t5_kind_of, .cycles_per_us = 3.25 },
    .cpu_hz = T5_FREQ,
    .cycles_per_frame = T5_CYCLES_PER_FRAME,
    .steps_per_cycle = T5_STEPS_PER_CYCLE,
    .screen_w = ULA81_PIXELS_PER_LINE, .screen_h = ULA81_LINES,
    .ready_cycles = 2 * T5_FREQ,
    .parts = t5_part_defs, .nparts = sizeof(t5_part_defs) / sizeof(t5_part_defs[0]),
    .conns = t5_conn_defs, .nconns = sizeof(t5_conn_defs) / sizeof(t5_conn_defs[0]),
    .link_resistors = "", .crystals = "X1",
    .diodes = "D", .jumpers = "", .supplies = "",
    .keys_help = "Keys: type; Left Shift = SHIFT, Enter = NEWLINE, Backspace = RUBOUT (= DELETE), arrows = cursor, Esc = BREAK",
    .reset_help = "Hold to press RESET: shorts the Z80 /RESET (U7 pin 26) to GND",
    .create = t5_create,
    .destroy = t5_destroy,
    .power_on = t5_power_on,
    .run = t5_run,
    .joins = t5_joins,
    .key_down = t5_key_down,
    .key_up = t5_key_up,
    .frame_update = t5_frame_update,
    .host_key = t5_host_key,
    .host_char = t5_host_char,
    .monitor = t5_monitor,
    .files_title = "Open a program",
    .file_kind = t5_file_kind,
    .load = t5_load,
    .cpu_status = t5_cpu_status,
    .cpu_regs = t5_cpu_regs,
    .print_text = t5_print_text,
};
