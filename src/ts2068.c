/* ts2068.c - the Timex Sinclair TS2068, board 335-850001 (the netlist is from the schematic
 * 335-870500, rev 03; the board marked 03D has an extra patch board that is not in it).
 *
 * HOW TIME MOVES
 * As the Spectrum's (zx48.c): the SCLD (U3, scld.c) is the board clock. Every half period of
 * its 14.112 MHz oscillator it advances, the board settles, the Z80 sees an edge of its clock if
 * there was one (the SCLD makes it), the memories see their strobes (a commit) and the board
 * settles again. 8 half periods make a T state (3.528 MHz).
 *
 * WHAT IS MODELED: the CPU, SCLD, the home ROM (16K), the EXROM (8K), the 16K display RAM and
 * the 32K upper RAM (4416 DRAMs), U9/U15 (74LS245), U5 (74LS244), U10/U11 (74LS157), the AY-3-8912
 * (U14), the keyboard (J9, CR6-CR13) and the joysticks' lines. NOT modeled: the power supply, the
 * video encoder and RF modulator, the tape and sound output circuits (the sound is not played yet). */
#include "ts2068.h"
#include "sokol/sokol_app.h"   /* key codes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

/* ------------------------------------------------------------------------ */
/* The chips                                                                 */
/* ------------------------------------------------------------------------ */

static const int tz_addr_pins[16] = { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5 };
static const int tz_data_pins[8] = { 14, 15, 12, 8, 7, 9, 10, 13 };
enum { Z_CLK = 6, Z_INT = 16, Z_NMI = 17, Z_HALT = 18, Z_MREQ = 19, Z_IORQ = 20, Z_RD = 21, Z_WR = 22,
       Z_BUSAK = 23, Z_WAIT = 24, Z_BUSRQ = 25, Z_RESET = 26, Z_M1 = 27, Z_RFSH = 28 };

static void tz_cpu_eval(sim_t *s, part_t *p) {
    ts2068_t *z = TS(p->user);
    uint64_t pins = z->cpu_pins;
    bool aoe = (pins & Z80_AOE) != 0;
    part_drive_bus_cached(s, p, tz_addr_pins, 16, Z80_GET_ADDR(pins), aoe, &z->cache_addr);
    pin_out(s, p, Z_MREQ, aoe ? DRV_BIT(!(pins & Z80_MREQ)) : DRV_Z);
    pin_out(s, p, Z_IORQ, aoe ? DRV_BIT(!(pins & Z80_IORQ)) : DRV_Z);
    pin_out(s, p, Z_RD, aoe ? DRV_BIT(!(pins & Z80_RD)) : DRV_Z);
    pin_out(s, p, Z_WR, aoe ? DRV_BIT(!(pins & Z80_WR)) : DRV_Z);
    pin_out(s, p, Z_M1, DRV_BIT(!(pins & Z80_M1)));
    pin_out(s, p, Z_RFSH, DRV_BIT(!(pins & Z80_RFSH)));
    pin_out(s, p, Z_HALT, DRV_BIT(!(pins & Z80_HALT)));
    pin_out(s, p, Z_BUSAK, DRV_BIT(!(pins & Z80_BUSAK)));
    part_drive_bus_cached(s, p, tz_data_pins, 8, Z80_GET_DATA(pins), (pins & Z80_DOE) != 0, &z->cache_data);
}
static const part_type_t PT_TZ80 = { "Z80A",
    "A11:O A12:O A13:O A14:O A15:O CLK:m D4:B D3:B D5:B D6:B VCC:P D2:B D7:B D0:B D1:B /INT:m /NMI:m "
    "/HALT:O /MREQ:O /IORQ:O /RD:O /WR:O /BUSAK:O /WAIT:m /BUSRQ:m /RESET:m /M1:O /RFSH:O GND:P A0:O "
    "A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O A10:O", tz_cpu_eval, 0, 0, 0, TECH_NMOS };

static void tz_cpu_edge(board_t *b, int clk) {
    ts2068_t *z = TS(b);
    sim_t *s = &b->sim;
    part_t *p = &s->parts[z->p_cpu];
    if (p->dead) return;
    uint64_t pins = clk ? Z80_CLK : 0;
    Z80_SET_DATA(pins, (uint64_t)part_read_bus(s, p, tz_data_pins, 8));
    if (!pin_in(s, p, Z_INT)) pins |= Z80_INT;
    if (!pin_in(s, p, Z_NMI)) pins |= Z80_NMI;
    if (!pin_in(s, p, Z_WAIT)) pins |= Z80_WAIT;
    if (!pin_in(s, p, Z_RESET)) pins |= Z80_RES;
    if (!pin_in(s, p, Z_BUSRQ)) pins |= Z80_BUSRQ;
    z->in_reset = (pins & Z80_RES) != 0;
    uint64_t out = z80_tick(&z->cpu, pins);
    if (out != z->cpu_pins) { z->cpu_pins = out; sim_mark_dirty(s, z->p_cpu); }
}

/* The ROMs (28 pin footprint; U20 is the 8K 2364 on the same pins as the 16K 23128 but A13). */
static const int tr_addr_pins[14] = { 10, 9, 8, 7, 6, 5, 4, 3, 25, 24, 21, 23, 2, 26 };
static const int tr_data_pins[8] = { 11, 12, 13, 15, 16, 17, 18, 19 };
static void tr_eval(sim_t *s, part_t *p, int abits) {
    rom_state_t *st = p->state;
    bool selected = !pin_in(s, p, 20) && !pin_in(s, p, 22);     /* /CS and /OE */
    unsigned a = selected ? part_read_bus(s, p, tr_addr_pins, abits) : 0;
    part_drive_bus_cached(s, p, tr_data_pins, 8, st->data ? st->data[a] : 0xFF, selected, &st->out_cache);
}
static void tr16_eval(sim_t *s, part_t *p) { tr_eval(s, p, 14); }
static void tr8_eval(sim_t *s, part_t *p) { tr_eval(s, p, 13); }
#define TS_ROM_PINS "NC A12:M A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:O D1:O D2:O GND:P D3:O D4:O D5:O D6:O " \
                    "D7:O /CS:M A10:M /OE:M A11:M A9:M A8:M A13:M PGM:M VCC:P"
static const part_type_t PT_TS_ROM16 = { "23128 ROM", TS_ROM_PINS, tr16_eval, 0, 0, sizeof(rom_state_t), TECH_NMOS };
static const part_type_t PT_TS_ROM8 = { "2364 ROM", TS_ROM_PINS, tr8_eval, 0, 0, sizeof(rom_state_t), TECH_NMOS };

/* 74LS245: DIR high sends A to B, low B to A; /E high isolates. */
static void ls245_eval(sim_t *s, part_t *p) {
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
static const part_type_t PT_TS_245 = { "74LS245",
    "DIR:T A1:X A2:X A3:X A4:X A5:X A6:X A7:X A8:X GND:P B8:X B7:X B6:X B5:X B4:X B3:X B2:X B1:X /E:T VCC:P",
    ls245_eval, 0, 0, 0, TECH_TTL };

/* AY-3-8912 (U14). BDIR/BC1 (BC2 and A8 are high): 01 read, 10 write, 11 latch address. A write
 * is done when the bus cycle ends (the data are valid then). Clocked by PHIC (1.764 MHz). */
typedef struct { ay8912_t ay; uint8_t clk_prev, mode, data; } ay_state_t;
static const int ay_data_pins[8] = { 28, 27, 26, 25, 24, 23, 22, 21 };
static const int ay_ioa_pins[8] = { 14, 13, 12, 11, 10, 9, 8, 7 };
static int ay_mode(sim_t *s, part_t *p) {
    if (!pin_in(s, p, 17) || !pin_in(s, p, 19)) return AY_BUS_NONE;
    int bdir = pin_in(s, p, 18), bc1 = pin_in(s, p, 20);
    return bdir ? (bc1 ? AY_BUS_ADDR : AY_BUS_WRITE) : (bc1 ? AY_BUS_READ : AY_BUS_NONE);
}
static void ay_eval(sim_t *s, part_t *p) {
    ay_state_t *st = p->state;
    bool rd = ay_mode(s, p) == AY_BUS_READ;
    part_drive_bus(s, p, ay_data_pins, 8, rd ? ay8912_bus(&st->ay, AY_BUS_READ, 0) : 0, rd);
    /* Port A: an output drives its pins, an input is pulled up (and read in ay_commit). */
    bool out = (st->ay.reg[7] & 0x40) != 0;
    for (int i = 0; i < 8; i++)
        pin_out(s, p, ay_ioa_pins[i], out ? DRV_BIT((st->ay.reg[14] >> i) & 1) : DRV_PULLUP);
}
static void ay_commit(sim_t *s, part_t *p) {
    ay_state_t *st = p->state;
    int mode = ay_mode(s, p);
    if (mode != AY_BUS_NONE && mode != AY_BUS_READ) {
        st->mode = (uint8_t)mode;
        st->data = (uint8_t)part_read_bus(s, p, ay_data_pins, 8);
    } else if (st->mode) {
        ay8912_bus(&st->ay, st->mode, st->data);
        st->mode = 0;
        sim_mark_dirty(s, (int)(p - s->parts));
    }
    int ck = pin_in(s, p, 15);
    if (ck && !st->clk_prev) ay8912_tick(&st->ay);
    st->clk_prev = (uint8_t)ck;
    if (!pin_in(s, p, 16)) { ay8912_reset(&st->ay); st->mode = 0; }
    uint8_t in = (uint8_t)part_read_bus(s, p, ay_ioa_pins, 8);
    if (in != st->ay.port_a_in) { st->ay.port_a_in = in; sim_mark_dirty(s, (int)(p - s->parts)); }
}
static void ay_reset(sim_t *s, part_t *p) {
    (void)s;
    ay_state_t *st = p->state;
    memset(st, 0, sizeof(*st));
    ay8912_reset(&st->ay);
    st->ay.port_a_in = 0xFF;
}
static const part_type_t PT_TS_AY = { "AY-3-8912",
    "C:O TEST1:T VCC:P B:O A:O GND:P IOA7:X IOA6:X IOA5:X IOA4:X IOA3:X IOA2:X IOA1:X IOA0:X CLOCK:T RESET:T "
    "A8:T BDIR:T BC2:T BC1:T D7:X D6:X D5:X D4:X D3:X D2:X D1:X D0:X",
    ay_eval, ay_commit, ay_reset, sizeof(ay_state_t), TECH_NMOS };

#define RAM_CHIP(ref, label, bits, range) { ref, &PT_4416, label, ROLE_RAM, "4416\n16Kx4\n" range, \
    "4416 DRAM, 16K x 4 bit: " bits " of " range }
static const part_def_t ts_part_defs[] = {
    { "U3", &PT_SCLD, "SCLD", ROLE_CHIP, "SCLD\nvideo, I/O,\nbanking",
      "Timex SCLD: video, CPU clock, memory banking and contention, DRAM control, ports F4, FE, FF and the AY bus" },
    { "U19", &PT_TZ80, "CPU", ROLE_CHIP, "Z80A", "Z80A CPU, 3.528 MHz" },
    { "U16", &PT_TS_ROM16, "ROM", ROLE_ROM, "23128\n$0000-$3FFF", "Home ROM (BASIC), 16K" },
    { "U20", &PT_TS_ROM8, "EXROM", ROLE_ROM, "2364\nEXROM 8K", "Extension ROM (EXROM), 8K: the 64 column mode and the extra BASIC" },
    { "U6", &PT_4416, "DRAM", ROLE_VRAM, "4416\nD0-D3\n$4000-$7FFF", "4416 DRAM, display RAM, bits 0-3 of $4000-$7FFF" },
    { "U7", &PT_4416, "DRAM", ROLE_VRAM, "4416\nD4-D7\n$4000-$7FFF", "4416 DRAM, display RAM, bits 4-7 of $4000-$7FFF" },
    { "U12", &PT_4416, "DRAM", ROLE_RAM, "4416\nD0-D3\n$8000-$BFFF", "4416 DRAM, upper RAM, bits 0-3 of $8000-$BFFF" },
    { "U13", &PT_4416, "DRAM", ROLE_RAM, "4416\nD4-D7\n$8000-$BFFF", "4416 DRAM, upper RAM, bits 4-7 of $8000-$BFFF" },
    { "U17", &PT_4416, "DRAM", ROLE_RAM, "4416\nD0-D3\n$C000-$FFFF", "4416 DRAM, upper RAM, bits 0-3 of $C000-$FFFF" },
    { "U18", &PT_4416, "DRAM", ROLE_RAM, "4416\nD4-D7\n$C000-$FFFF", "4416 DRAM, upper RAM, bits 4-7 of $C000-$FFFF" },
    { "U9", &PT_TS_245, "BUF", ROLE_GLUE, "74LS245", "Data bus buffer between the CPU and the RAM side (the SCLD drives the direction)" },
    { "U15", &PT_TS_245, "BUF", ROLE_GLUE, "74LS245", "Buffer from the CPU to the dock (J4): control lines and A13-A15" },
    { "U5", &PT_74LS244, "BUF", ROLE_GLUE, "74LS244", "Address buffer onto the DRAM address bus MA0-MA7" },
    { "U10", &PT_74LS157, "MUX", ROLE_GLUE, "74LS157", "Upper RAM address multiplexer (row/column)" },
    { "U11", &PT_74LS157, "MUX", ROLE_GLUE, "74LS157", "Upper RAM address multiplexer (row/column)" },
    { "U21", &PT_74LS00, "NAND", ROLE_GLUE, "74LS00", "Spare gates (the patch board of the 03D replaces what was here)" },
    { "U14", &PT_TS_AY, "AY", ROLE_CHIP, "AY-3-8912\nsound", "General Instrument AY-3-8912 sound chip with the joystick port" },
};

static const conn_def_t ts_conn_defs[] = {
    { "J4", "Cartridge dock", "1=A14 2=+5V 3=A12 4=A13 5=D0 6=D7 7=D1 8=A0 9=D2 10=A1 11=D6 12=A2 13=D5 14=A3 15=D3 16=A15 "
                              "17=D4 18=/MREQ 19=/IORQ 20=A7 21=/RD 22=/M1 23=/WR 24=A8 25=A7 26=A9 27=A6 28=A10 29=A5 "
                              "30=A11 31=A4 32=/RFSH 33=/BE 34=/EXROM 35=/ROSCS 36=GND" },
    { "J5", "Joystick 1", "1=UP 2=DOWN 3=LEFT 4=RIGHT 6=FIRE 7=+5V 8=STROBE 9=GND" },
    { "J6", "Joystick 2", "1=UP 2=DOWN 3=LEFT 4=RIGHT 6=FIRE 7=+5V 8=STROBE 9=GND" },
    { "J9", "Keyboard", "1=GND 2=KB0 3=KB1 4=KB2 5=KB3 6=KB4 7=A11 8=A10 9=A9 10=A12 11=A13 12=A8 13=A14 14=A15" },
};

static int ts_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int ts_kind_of(const char *ref) {
    if (ref[0] == 'U') return PART_IC;
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    if (ref[0] == 'J' || !strcmp(ref, "Y1")) return PART_CONNECTOR;
    return PART_OTHER;      /* the diodes (made elements in ts_create), the strobe transistors */
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static part_t *ts_find(board_t *b, const char *ref) {
    int i = sim_find_part(&b->sim, ref);
    return i >= 0 ? &b->sim.parts[i] : NULL;
}
static int ts_pin_of(board_t *b, const char *ref, int number) {
    part_t *p = ts_find(b, ref);
    return p ? part_pin(p, number) : -1;
}

static void ts_make_diode(board_t *b, const char *ref, int anode, int cathode) {
    part_t *p = ts_find(b, ref);
    if (!p || part_pin(p, anode) < 0 || part_pin(p, cathode) < 0) return;
    resistor_state_t *st = p->state;
    if (!st) { st = calloc(1, sizeof(*st)); p->state = st; p->state_size = sizeof(*st); }
    p->kind = PART_RESISTOR;
    st->nel = 1;
    st->el[0] = (element_t){ part_pin(p, anode), part_pin(p, cathode), EL_ANALOG };
    element_set_type(&b->sim, p, 0, EL_DIODE);
}

static void ts_init_keymap(ts2068_t *z) {
    /* The same 8 x 5 matrix as the Spectrum's: column = address line A8+n, line = KB0..KB4. */
    kbd_init(&z->kbd, 1);
    kbd_register_modifier(&z->kbd, 0, 0, 0);     /* caps shift */
    kbd_register_modifier(&z->kbd, 1, 7, 1);     /* symbol shift */
    const char *keymap =
        " zxcv" "asdfg" "qwert" "12345" "09876" "poiuy" " lkjh" "  mnb"
        " ZXCV" "ASDFG" "QWERT" "     " "     " "POIUY" " LKJH" "  MNB"
        " : ?/" "     " "   <>" "!@#$%" "_)('&" "\";   " " =+-^" "  .,*";
    for (int layer = 0; layer < 3; layer++)
        for (int column = 0; column < 8; column++)
            for (int line = 0; line < 5; line++) {
                int c = keymap[layer * 40 + column * 5 + line];
                if (c != ' ') kbd_register_key(&z->kbd, c, column, line, layer > 0 ? 1 << (layer - 1) : 0);
            }
    kbd_register_key(&z->kbd, ' ', 7, 0, 0);
    kbd_register_key(&z->kbd, 0x0F, 7, 1, 0);    /* symbol shift */
    kbd_register_key(&z->kbd, 0x0E, 0, 0, 0);    /* caps shift */
    kbd_register_key(&z->kbd, 0x08, 3, 4, 1);    /* cursor left: caps shift + 5 */
    kbd_register_key(&z->kbd, 0x0A, 4, 4, 1);    /* down: caps shift + 6 */
    kbd_register_key(&z->kbd, 0x0B, 4, 3, 1);    /* up: caps shift + 7 */
    kbd_register_key(&z->kbd, 0x09, 4, 2, 1);    /* right: caps shift + 8 */
    kbd_register_key(&z->kbd, 0x07, 3, 0, 1);    /* edit: caps shift + 1 */
    kbd_register_key(&z->kbd, 0x0C, 4, 0, 1);    /* delete: caps shift + 0 */
    kbd_register_key(&z->kbd, 0x03, 7, 0, 1);    /* break: caps shift + space */
    kbd_register_key(&z->kbd, 0x0D, 6, 0, 0);    /* enter */
}

/* ROM files: roms/ts2068 (or roms, or .), ts2068-0.rom (home, 16K) and ts2068-1.rom (EXROM, 8K),
 * or any .rom/.bin file of those sizes. Not shipped: they are Timex's. */
static void ts_find_roms(board_t *b) {
    ts2068_t *z = TS(b);
    static const char *const dirs[] = { "roms/ts2068", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir))) {
            size_t l = strlen(e->d_name);
            if (l < 5 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom"))) continue;
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], e->d_name);
            FILE *f = fopen(path, "rb");
            if (!f) continue;
            uint8_t buf[16385];
            size_t n = fread(buf, 1, sizeof(buf), f);
            fclose(f);
            if (n == sizeof(z->rom_home) && !(z->rom_mask & 1)) { memcpy(z->rom_home, buf, n); z->rom_mask |= 1; }
            else if (n == sizeof(z->rom_ex) && !(z->rom_mask & 2)) { memcpy(z->rom_ex, buf, n); z->rom_mask |= 2; }
        }
        closedir(dir);
    }
}

static void ts_create(board_t *b) {
    ts2068_t *z = TS(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "U19");
    z->p_scld = sim_find_part(s, "U3");
    z->p_ay = sim_find_part(s, "U14");
    z->p_rom0 = sim_find_part(s, "U16");
    z->p_rom1 = sim_find_part(s, "U20");
    z->pin_clk = part_pin(&s->parts[z->p_cpu], Z_CLK);
    memset(z->rom_home, 0xFF, sizeof(z->rom_home));
    memset(z->rom_ex, 0xFF, sizeof(z->rom_ex));
    z->rom_mask = 0;
    ts_find_roms(b);
    ((rom_state_t *)s->parts[z->p_rom0].state)->data = z->rom_home;
    ((rom_state_t *)s->parts[z->p_rom1].state)->data = z->rom_ex;
    ((scld_state_t *)s->parts[z->p_scld].state)->fb = b->screen;
    ((scld_state_t *)s->parts[z->p_scld].state)->running = true;     /* Y1 is not simulated: it always oscillates */
    /* Keyboard diodes: the key row (pin 1) is the anode, the address line (pin 2) the cathode. */
    for (int i = 6; i <= 13; i++) {
        char ref[8];
        snprintf(ref, sizeof(ref), "CR%d", i);
        ts_make_diode(b, ref, 1, 2);
    }
    /* Joystick diodes: the AY port A line (pin 2) is the anode, the joystick's line (pin 1) the cathode. */
    for (int i = 14; i <= 23; i++) {
        char ref[8];
        snprintf(ref, sizeof(ref), "CR%d", i);
        ts_make_diode(b, ref, 2, 1);
    }
    /* J9: the rows are A8-A15 (pins 12 9 8 7 10 11 13 14), the lines KB0-KB4 are pins 2-6. */
    static const int rows[8] = { 12, 9, 8, 7, 10, 11, 13, 14 };
    for (int c = 0; c < 8; c++) z->pin_row[c] = ts_pin_of(b, "J9", rows[c]);
    for (int l = 0; l < 5; l++) z->pin_kb[l] = ts_pin_of(b, "J9", 2 + l);
    ts_init_keymap(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    z->pin_reset_net = ts_pin_of(b, "U19", 26);         /* /RESET: held low at power on (a reset circuit is not drawn); R43 pulls it up */
    b->pin_reset = ts_pin_of(b, "U19", 26);
    b->pin_reset_gnd = ts_pin_of(b, "U19", 29);
    /* Buses, for the bus bitmap (speed only). */
    char name[16];
    int bit = 0;
    static const struct { const char *fmt; int n; } buses[] = { { "A%d", 16 }, { "D%d", 8 }, { "D%dP", 8 }, { "MA%d", 8 } };
    for (size_t k = 0; k < sizeof(buses) / sizeof(buses[0]); k++)
        for (int i = 0; i < buses[k].n; i++) {
            snprintf(name, sizeof(name), buses[k].fmt, i);
            sim_set_bus_bit(s, sim_find_net(s, name), bit++);
        }
}

static void ts_power_on(board_t *b) {
    ts2068_t *z = TS(b);
    z->cpu_pins = z80_init(&z->cpu);
    z->in_reset = true;
    z->clk = 0;
    z->reset_until = TS_FREQ / 20;      /* 50 ms */
    memset(b->screen, 0, (size_t)SCLD_SCREEN_W * SCLD_SCREEN_H * sizeof(uint32_t));
}

static void ts_joins(board_t *b) {
    ts2068_t *z = TS(b);
    sim_t *s = &b->sim;
    for (int l = 0; l < 5; l++)
        for (int c = 0; c < 8; c++) {
            if (!((z->key_matrix[l] >> c) & 1)) continue;
            int r = z->pin_row[c], k = z->pin_kb[l];
            if (r < 0 || k < 0 || s->pins[r].fault == PF_OPEN || s->pins[k].fault == PF_OPEN) continue;
            sim_join(s, s->pins[r].net, s->pins[k].net);
        }
}

static void ts_sync_keys(board_t *b) {
    ts2068_t *z = TS(b);
    bool changed = false;
    for (int l = 0; l < 5; l++) {
        uint16_t m = z->kbd.scanout_column_masks[l];
        if (m != z->key_matrix[l]) { z->key_matrix[l] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}
static void ts_key_down(board_t *b, int key) { kbd_key_down(&TS(b)->kbd, key); ts_sync_keys(b); }
static void ts_key_up(board_t *b, int key) { kbd_key_up(&TS(b)->kbd, key); ts_sync_keys(b); }
static void ts_frame_update(board_t *b, uint32_t us) { kbd_update(&TS(b)->kbd, us); ts_sync_keys(b); }

static int ts_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return 0x0D;
    case SAPP_KEYCODE_BACKSPACE: return 0x0C;
    case SAPP_KEYCODE_LEFT: return 0x08;
    case SAPP_KEYCODE_RIGHT: return 0x09;
    case SAPP_KEYCODE_UP: return 0x0B;
    case SAPP_KEYCODE_DOWN: return 0x0A;
    case SAPP_KEYCODE_ESCAPE: return 0x03;
    case SAPP_KEYCODE_TAB: return 0x0F;
    case SAPP_KEYCODE_LEFT_SHIFT: return 0x0E;
    default: return 0;
    }
}
static int ts_host_char(int c) { return c; }

static void ts_monitor(board_t *b, uint32_t *out) {
    part_t *scld = &b->sim.parts[TS(b)->p_scld];
    b->no_signal = scld->dead;
    for (int i = 0; i < SCLD_SCREEN_W * SCLD_SCREEN_H; i++) {
        uint32_t c = b->screen[i];
        if (scld->dead) {
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

static void ts_step(board_t *b) {
    ts2068_t *z = TS(b);
    sim_t *s = &b->sim;
    scld_step(s, &s->parts[z->p_scld]);
    sim_settle(s);
    int clk = sim_read(s, z->pin_clk);
    if (clk != z->clk) {
        z->clk = clk;
        tz_cpu_edge(b, clk);
        sim_settle(s);
    }
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void ts_run(board_t *b, int cycles) {
    ts2068_t *z = TS(b);
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        sim_drive(s, z->pin_reset_net, s->cycle < z->reset_until ? DRV_LO : DRV_Z);
        for (int k = 0; k < TS_STEPS_PER_CYCLE; k++) ts_step(b);
    }
}

/* The byte at a RAM address as the 4416s hold it: the row is A0-A6 and ATR (= A7), the column A8-A13 with
 * A6' low and A7' = ATR (as U10/U11 are wired); each chip has four data bits, found by their nets D0'-D7'. */
uint8_t ts2068_ram_peek(board_t *b, uint16_t addr) {
    sim_t *s = &b->sim;
    static const char *const pair[3][2] = { { "U6", "U7" }, { "U12", "U13" }, { "U17", "U18" } };
    int sel = addr < 0x8000 ? 0 : addr < 0xC000 ? 1 : 2;
    if (addr < 0x4000) return 0xFF;
    unsigned cell = ((addr & 0xFFu) << 8) | ((addr >> 8) & 0x3Fu) | (addr & 0x80u);
    static const int dq[4] = { 2, 3, 15, 17 };
    uint8_t v = 0;
    for (int c = 0; c < 2; c++) {
        int idx = sim_find_part(s, pair[sel][c]);
        if (idx < 0) continue;
        part_t *p = &s->parts[idx];
        uint8_t cellv = ((dram4_state_t *)p->state)->cells[cell];
        for (int k = 0; k < 4; k++) {
            int net = s->pins[part_pin(p, dq[k])].net;
            const char *n = net >= 0 ? s->nets[net].name : "";
            if (n[0] == 'D' && n[1] >= '0' && n[1] <= '7') v |= (uint8_t)(((cellv >> k) & 1) << (n[1] - '0'));
        }
    }
    return v;
}

/* Run single half periods of the oscillator, for tracing (board_run() runs whole T states). */
void ts2068_run_steps(board_t *b, int steps) {
    for (int i = 0; i < steps; i++) ts_step(b);
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static void ts_destroy(board_t *b) { (void)b; }

static const char *ts_cpu_status(board_t *b) {
    const ts2068_t *z = TS(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!(z->rom_mask & 1)) return "no home ROM (put ts2068-0.rom in roms/ts2068)";
    if (!(z->rom_mask & 2)) return "no EXROM (put ts2068-1.rom in roms/ts2068)";
    return z->in_reset ? "in reset" : "running";
}

static void ts_cpu_regs(board_t *b, char *buf, int len) {
    const z80_t *c = &TS(b)->cpu;
    snprintf(buf, (size_t)len, "PC=%04X SP=%04X AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X",
             c->pc, c->sp, c->af, c->bc, c->de, c->hl, c->ix, c->iy);
}

static const file_kind_t *ts_file_kind(const char *path, char *info, int len, bool *bad) {
    (void)path; (void)info; (void)len; (void)bad;
    return NULL;
}
static bool ts_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)path; (void)run;
    snprintf(msg, (size_t)msglen, "the TS2068 can't load files yet");
    return false;
}

static void ts_print_text(board_t *b) {
    /* The screen as it is drawn: the 32 column text is not read back from the display RAM yet. */
    (void)b;
    printf("(no text dump for the TS2068 yet: use --screenshot)\n");
}

const machine_t machine_ts2068 = {
    .id = "ts2068",
    .name = "Timex Sinclair TS2068",
    .issue = "TS2068",
    .board_name = "TS2068 335-850001 rev 03 (03D has a patch board, not modeled)",
    .size = sizeof(ts2068_t),
    .data = &bd_ts2068,
    .sim = { .rail_of = ts_rail_of, .kind_of = ts_kind_of, .cycles_per_us = 3.528 },
    .cpu_hz = TS_FREQ,
    .cycles_per_frame = TS_CYCLES_PER_FRAME,
    .steps_per_cycle = TS_STEPS_PER_CYCLE,
    .screen_w = SCLD_SCREEN_W, .screen_h = SCLD_SCREEN_H,
    .ready_cycles = 3 * TS_FREQ,
    .parts = ts_part_defs, .nparts = sizeof(ts_part_defs) / sizeof(ts_part_defs[0]),
    .conns = ts_conn_defs, .nconns = sizeof(ts_conn_defs) / sizeof(ts_conn_defs[0]),
    .link_resistors = "", .crystals = "Y1",
    .diodes = "CR", .jumpers = "", .supplies = "",
    .keys_help = "Keys: type; Tab = symbol shift, Left Shift = caps shift, Esc = BREAK",
    .reset_help = "Hold to press RESET: shorts the Z80 /RESET (U19 pin 26) to GND",
    .create = ts_create,
    .destroy = ts_destroy,
    .power_on = ts_power_on,
    .run = ts_run,
    .joins = ts_joins,
    .key_down = ts_key_down,
    .key_up = ts_key_up,
    .frame_update = ts_frame_update,
    .host_key = ts_host_key,
    .host_char = ts_host_char,
    .monitor = ts_monitor,
    .files_title = "Open a program",
    .file_kind = ts_file_kind,
    .load = ts_load,
    .cpu_status = ts_cpu_status,
    .cpu_regs = ts_cpu_regs,
    .print_text = ts_print_text,
};
