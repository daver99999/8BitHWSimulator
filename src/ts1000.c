/* ts1000.c - the Timex Sinclair 1000, the American ZX81, board Issue 3.
 *
 * THE NETLIST IS RECONSTRUCTED, not transcribed from a schematic of this board: see tools/ts1000-netlist-src/sheet01.py
 * for what it is made from and what is guessed.
 *
 * HOW TIME MOVES
 * As the Spectrum's (zx48.c) and the TS2068's: the ULA (IC1, ula81.c) is the board clock. Every half period of its
 * 6.5 MHz oscillator it advances, the board settles, the Z80 sees an edge of its clock if there was one (the ULA makes
 * it: /CLK, inverted by TR2), the memories see their strobes (a commit) and the board settles again. 4 half periods
 * make a CPU clock cycle (3.25 MHz).
 *
 * WHAT IS MODELED: the Z80A (IC3), the ULA (IC1), the ROM (IC2, 8K), the RAM (IC4, 2K), the buffer resistors, TR1
 * (the /WAIT circuit), TR2 (the clock inverter), the keyboard (KB1, KB2, D1-D8) and the expansion edge connector J1.
 * NOT modeled: the power supply, the modulator and the video signal levels, the tape circuits, the 16K RAM pack. */
#include "ts1000.h"
#include "sokol/sokol_app.h"   /* key codes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

/* ------------------------------------------------------------------------ */
/* The chips                                                                 */
/* ------------------------------------------------------------------------ */

static const int t1_addr_pins[16] = { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5 };
static const int t1_data_pins[8] = { 14, 15, 12, 8, 7, 9, 10, 13 };
enum { T1Z_CLK = 6, T1Z_INT = 16, T1Z_NMI = 17, T1Z_HALT = 18, T1Z_MREQ = 19, T1Z_IORQ = 20, T1Z_RD = 21, T1Z_WR = 22,
       T1Z_BUSAK = 23, T1Z_WAIT = 24, T1Z_BUSRQ = 25, T1Z_RESET = 26, T1Z_M1 = 27, T1Z_RFSH = 28 };

static void t1_cpu_eval(sim_t *s, part_t *p) {
    ts1000_t *z = T1(p->user);
    uint64_t pins = z->cpu_pins;
    bool aoe = (pins & Z80_AOE) != 0;
    part_drive_bus_cached(s, p, t1_addr_pins, 16, Z80_GET_ADDR(pins), aoe, &z->cache_addr);
    pin_out(s, p, T1Z_MREQ, aoe ? DRV_BIT(!(pins & Z80_MREQ)) : DRV_Z);
    pin_out(s, p, T1Z_IORQ, aoe ? DRV_BIT(!(pins & Z80_IORQ)) : DRV_Z);
    pin_out(s, p, T1Z_RD, aoe ? DRV_BIT(!(pins & Z80_RD)) : DRV_Z);
    pin_out(s, p, T1Z_WR, aoe ? DRV_BIT(!(pins & Z80_WR)) : DRV_Z);
    pin_out(s, p, T1Z_M1, DRV_BIT(!(pins & Z80_M1)));
    pin_out(s, p, T1Z_RFSH, DRV_BIT(!(pins & Z80_RFSH)));
    pin_out(s, p, T1Z_HALT, DRV_BIT(!(pins & Z80_HALT)));
    pin_out(s, p, T1Z_BUSAK, DRV_BIT(!(pins & Z80_BUSAK)));
    part_drive_bus_cached(s, p, t1_data_pins, 8, Z80_GET_DATA(pins), (pins & Z80_DOE) != 0, &z->cache_data);
}
static const part_type_t PT_T1Z80 = { "Z80A",
    "A11:O A12:O A13:O A14:O A15:O CLK:m D4:B D3:B D5:B D6:B VCC:P D2:B D7:B D0:B D1:B /INT:m /NMI:m "
    "/HALT:O /MREQ:O /IORQ:O /RD:O /WR:O /BUSAK:O /WAIT:m /BUSRQ:m /RESET:m /M1:O /RFSH:O GND:P A0:O "
    "A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O A10:O", t1_cpu_eval, 0, 0, 0, TECH_NMOS };

static void t1_cpu_edge(board_t *b, int clk) {
    ts1000_t *z = T1(b);
    sim_t *s = &b->sim;
    part_t *p = &s->parts[z->p_cpu];
    if (p->dead) return;
    uint64_t pins = clk ? Z80_CLK : 0;
    Z80_SET_DATA(pins, (uint64_t)part_read_bus(s, p, t1_data_pins, 8));
    if (!pin_in(s, p, T1Z_INT)) pins |= Z80_INT;
    if (!pin_in(s, p, T1Z_NMI)) pins |= Z80_NMI;
    if (!pin_in(s, p, T1Z_WAIT)) pins |= Z80_WAIT;
    if (!pin_in(s, p, T1Z_RESET)) pins |= Z80_RES;
    if (!pin_in(s, p, T1Z_BUSRQ)) pins |= Z80_BUSRQ;
    z->in_reset = (pins & Z80_RES) != 0;
    uint64_t out = z80_tick(&z->cpu, pins);
    if (out != z->cpu_pins) { z->cpu_pins = out; sim_mark_dirty(s, z->p_cpu); }
}


/* The ROM (IC2, 2364) is the shared 2364 part (PT_2364); the RAM below shares its data pins. */
static const int t1r_data_pins[8] = { 9, 10, 11, 13, 14, 15, 16, 17 };

/* The RAM: 2K x 8 static (6116 pins: 18 /CS, 20 /OE, 21 /WE). */
typedef struct { uint8_t cells[2048]; int32_t out_cache; unsigned mask; } t1_ram_t;   /* mask: 2047 (2K) or 1023 (1K: A10 is not wired) */
static const int t1m_addr_pins[11] = { 8, 7, 6, 5, 4, 3, 2, 1, 23, 22, 19 };
static void t1_ram_eval(sim_t *s, part_t *p) {
    t1_ram_t *st = p->state;
    bool read = !pin_in(s, p, 18) && !pin_in(s, p, 20) && pin_in(s, p, 21);
    unsigned a = read ? part_read_bus(s, p, t1m_addr_pins, 11) & st->mask : 0;
    part_drive_bus_cached(s, p, t1r_data_pins, 8, st->cells[a], read, &st->out_cache);
}
static void t1_ram_commit(sim_t *s, part_t *p) {
    t1_ram_t *st = p->state;
    if (!pin_in(s, p, 18) && !pin_in(s, p, 21))
        st->cells[part_read_bus(s, p, t1m_addr_pins, 11) & st->mask] = (uint8_t)part_read_bus(s, p, t1r_data_pins, 8);
}
static void t1_ram_reset(sim_t *s, part_t *p) {
    (void)s;
    t1_ram_t *st = p->state;
    if (!st->mask) st->mask = 2047;
    for (int i = 0; i < 2048; i++) st->cells[i] = (uint8_t)((i * 7 + (i >> 4) + 0x5A) & 0xFF);
}
static const part_type_t PT_T1RAM = { "6116 SRAM",
    "A7:M A6:M A5:M A4:M A3:M A2:M A1:M A0:M D0:B D1:B D2:B GND:P D3:B D4:B D5:B D6:B D7:B /CS:M A10:M /OE:M /WE:M "
    "A9:M A8:M VCC:P", t1_ram_eval, t1_ram_commit, t1_ram_reset, sizeof(t1_ram_t), TECH_NMOS };

/* TR1: pulls /WAIT (collector) low when its base (/HALT) is high and its emitter (/NMI) is low. */
static void t1_tr1_eval(sim_t *s, part_t *p) {
    bool on = pin_in(s, p, 1) && !pin_in(s, p, 3);
    pin_out(s, p, 2, on ? DRV_LO : DRV_Z);
}
static const part_type_t PT_T1TR1 = { "ZTX313", "B:M C:O E:M", t1_tr1_eval, 0, 0, 0, TECH_TTL };

static const part_def_t t1_part_defs[] = {
    { "IC1", &PT_ULA81, "ULA", ROLE_CHIP, "ULA\n2C210E", "Sinclair logic array (Ferranti ULA): clock, chip selects, video, sync, NMI generator, keyboard and tape port" },
    { "IC3", &PT_T1Z80, "CPU", ROLE_CHIP, "Z80A", "Z80A CPU, 3.25 MHz" },
    { "IC2", &PT_2364, "ROM", ROLE_ROM, "2364\n$0000-$1FFF", "ROM: BASIC and the character set, 8K" },
    { "IC4", &PT_T1RAM, "RAM", ROLE_VRAM, "2K x 8\n$4000-$47FF", "Static RAM, 2K: the program, the variables and the display file" },
    { "TR1", &PT_T1TR1, "ZTX313", ROLE_GLUE, "TR1\nWAIT / NMI", "TR1 (ZTX313): makes /WAIT from /HALT (through R1) and /NMI (emitter); R17 pulls /WAIT up" },
};

static const conn_def_t t1_conn_defs[] = {
    { "J1", "Edge connector", "1=D7 2=+5V 3=/RAMCS 4=+9V 7=D0 8=GND 9=D1 10=GND 11=D2 12=CLK 13=D6 14=A0 15=D5 16=A1 17=D3 18=A2 "
                              "19=D4 20=A3 21=/INT 22=A15 23=/NMI 24=A14 25=/HALT 26=A13 27=/MREQ 28=A12 29=/IORQ 30=A11 31=/RD "
                              "32=A10 33=/WR 34=A9 35=/BUSAK 36=A8 37=/WAIT 38=A7 39=/BUSRQ 40=A6 41=/RESET 42=A5 43=/M1 44=A4 "
                              "45=/RFSH 46=/ROMCS" },
    { "KB1", "Keyboard (columns)", "1=KBD0 2=KBD1 3=KBD2 4=KBD3 5=KBD4" },
    { "KB2", "Keyboard (half rows)", "1=A8 2=A9 3=A10 4=A11 5=A12 6=A13 7=A14 8=A15" },
    { "MICJ", "MIC jack", "1=MIC 2=GND" },
    { "EARJ", "EAR jack", "1=EAR 2=GND" },
    { "MOD", "Modulator", "1=UK2_video 2=USA3 3=GND 4=+5V" },
    { "DC", "9V input", "1=+9V 2=GND" },
};

static int t1_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int t1_kind_of(const char *ref) {
    if (!strcmp(ref, "X1") || !strcmp(ref, "MICJ") || !strcmp(ref, "EARJ") || !strcmp(ref, "MOD") || !strcmp(ref, "DC") ||
        !strncmp(ref, "KB", 2) || ref[0] == 'J') return PART_CONNECTOR;
    if (!strncmp(ref, "IC", 2) || !strcmp(ref, "TR1")) return PART_IC;
    if (ref[0] == 'R' && (ref[1] >= '0' && ref[1] <= '9')) return PART_RESISTOR;
    return PART_OTHER;      /* the diodes and TR2 (made elements in t1_create) */
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static part_t *t1_find(board_t *b, const char *ref) {
    int i = sim_find_part(&b->sim, ref);
    return i >= 0 ? &b->sim.parts[i] : NULL;
}
static int t1_pin_of(board_t *b, const char *ref, int number) {
    part_t *p = t1_find(b, ref);
    return p ? part_pin(p, number) : -1;
}

/* Turn a part into one active element: pin_a, pin_b as the element's two sides. */
static void t1_make_element(board_t *b, const char *ref, int pin_a, int pin_b, int type) {
    part_t *p = t1_find(b, ref);
    if (!p || part_pin(p, pin_a) < 0 || part_pin(p, pin_b) < 0) return;
    resistor_state_t *st = p->state;
    if (!st) { st = calloc(1, sizeof(*st)); p->state = st; p->state_size = sizeof(*st); }
    p->kind = PART_RESISTOR;
    st->nel = 1;
    st->el[0] = (element_t){ part_pin(p, pin_a), part_pin(p, pin_b), EL_ANALOG };
    element_set_type(&b->sim, p, 0, type);
}

static void t1_init_keymap(ts1000_t *z) {
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

/* ROM file: roms/ts1000 (or roms, or .), any .rom/.bin file of 8192 bytes (the ZX81 ROM). Not shipped. */
static void t1_find_roms(board_t *b) {
    ts1000_t *z = T1(b);
    static const char *const dirs[] = { "roms/ts1000", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]) && !z->rom_loaded; d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while (!z->rom_loaded && (e = readdir(dir))) {
            size_t l = strlen(e->d_name);
            if (l < 5 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom"))) continue;
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

static void t1_create(board_t *b) {
    ts1000_t *z = T1(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "IC3");
    z->p_ula = sim_find_part(s, "IC1");
    z->p_rom = sim_find_part(s, "IC2");
    z->p_ram = sim_find_part(s, "IC4");
    z->pin_clk = part_pin(&s->parts[z->p_cpu], T1Z_CLK);
    memset(z->rom, 0xFF, sizeof(z->rom));
    z->rom_loaded = false;
    t1_find_roms(b);
    ((rom_state_t *)s->parts[z->p_rom].state)->data = z->rom;
    ((ula81_state_t *)s->parts[z->p_ula].state)->fb = b->screen;
    ((ula81_state_t *)s->parts[z->p_ula].state)->running = true;     /* X1 is not simulated: it always oscillates */
    /* Resistors that do logic: the buffers (470 ohm on the data bus, 1K on the ROM address lines, 680 ohm on the chip
     * selects) let each side drive its own half of the bus; R6 and R1 are base resistors, wires here. */
    for (int i = 7; i <= 14; i++) { char ref[8]; snprintf(ref, sizeof(ref), "R%d", i); part_t *p = t1_find(b, ref);
        if (p && p->state && ((resistor_state_t *)p->state)->nel) element_set_type(s, p, 0, EL_SERIES); }
    for (int i = 18; i <= 26; i++) { char ref[8]; snprintf(ref, sizeof(ref), "R%d", i); part_t *p = t1_find(b, ref);
        if (p && p->state && ((resistor_state_t *)p->state)->nel) element_set_type(s, p, 0, EL_SERIES); }
    static const char *const ser[] = { "R2", "R28" };
    for (int i = 0; i < 2; i++) { part_t *p = t1_find(b, ser[i]);
        if (p && p->state && ((resistor_state_t *)p->state)->nel) element_set_type(s, p, 0, EL_SERIES); }
    static const char *const links[] = { "R6", "R1" };
    for (int i = 0; i < 2; i++) { part_t *p = t1_find(b, links[i]);
        if (p && p->state) ((resistor_state_t *)p->state)->el[0].type = EL_LINK; }
    /* The keyboard diodes: the half row (pin 1) is the anode, the address line (pin 2) the cathode. */
    for (int i = 1; i <= 8; i++) { char ref[8]; snprintf(ref, sizeof(ref), "D%d", i); t1_make_element(b, ref, 1, 2, EL_DIODE); }
    t1_make_element(b, "TR2", 1, 2, EL_NPN);            /* the clock inverter: base to collector */
    for (int c = 0; c < 8; c++) z->pin_row[c] = t1_pin_of(b, "KB2", 1 + c);
    for (int l = 0; l < 5; l++) z->pin_kb[l] = t1_pin_of(b, "KB1", 1 + l);
    t1_init_keymap(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    z->pin_reset_net = t1_pin_of(b, "IC3", 26);          /* /RESET: held low at power on (the RC of the real board is not drawn) */
    b->pin_reset = t1_pin_of(b, "IC3", 26);
    b->pin_reset_gnd = t1_pin_of(b, "IC3", 29);
    char name[16];
    int bit = 0;
    static const struct { const char *fmt; int n; } buses[] = { { "A%d", 16 }, { "D%d", 8 }, { "D%dP", 8 }, { "A%dP", 9 } };
    for (size_t k = 0; k < sizeof(buses) / sizeof(buses[0]); k++)
        for (int i = 0; i < buses[k].n; i++) {
            snprintf(name, sizeof(name), buses[k].fmt, i);
            sim_set_bus_bit(s, sim_find_net(s, name), bit++);
        }
}

static void t1_power_on(board_t *b) {
    ts1000_t *z = T1(b);
    ((t1_ram_t *)b->sim.parts[z->p_ram].state)->mask = z->ram1k ? 1023 : 2047;
    z->cpu_pins = z80_init(&z->cpu);
    z->in_reset = true;
    z->clk = 0;
    z->reset_until = T1_FREQ / 20;      /* 50 ms */
    for (int i = 0; i < ULA81_PIXELS_PER_LINE * ULA81_LINES; i++) b->screen[i] = 0xFF000000u;
}

static void t1_joins(board_t *b) {
    ts1000_t *z = T1(b);
    sim_t *s = &b->sim;
    for (int l = 0; l < 5; l++)
        for (int c = 0; c < 8; c++) {
            if (!((z->key_matrix[l] >> c) & 1)) continue;
            int r = z->pin_row[c], k = z->pin_kb[l];
            if (r < 0 || k < 0 || s->pins[r].fault == PF_OPEN || s->pins[k].fault == PF_OPEN) continue;
            sim_join(s, s->pins[r].net, s->pins[k].net);
        }
}

static void t1_sync_keys(board_t *b) {
    ts1000_t *z = T1(b);
    bool changed = false;
    for (int l = 0; l < 5; l++) {
        uint16_t m = z->kbd.scanout_column_masks[l];
        if (m != z->key_matrix[l]) { z->key_matrix[l] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}
static void t1_key_down(board_t *b, int key) { kbd_key_down(&T1(b)->kbd, key); t1_sync_keys(b); }
static void t1_key_up(board_t *b, int key) { kbd_key_up(&T1(b)->kbd, key); t1_sync_keys(b); }
static void t1_frame_update(board_t *b, uint32_t us) { kbd_update(&T1(b)->kbd, us); t1_sync_keys(b); }

static int t1_host_key(int keycode) {
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
static int t1_host_char(int c) { return c; }

static void t1_monitor(board_t *b, uint32_t *out) {
    part_t *ula = &b->sim.parts[T1(b)->p_ula];
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

static void t1_step(board_t *b) {
    ts1000_t *z = T1(b);
    sim_t *s = &b->sim;
    ula81_step(s, &s->parts[z->p_ula]);
    sim_settle(s);
    int clk = sim_read(s, z->pin_clk);
    if (clk != z->clk) {
        z->clk = clk;
        t1_cpu_edge(b, clk);
        sim_settle(s);
    }
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void t1_run(board_t *b, int cycles) {
    ts1000_t *z = T1(b);
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        sim_drive(s, z->pin_reset_net, s->cycle < z->reset_until ? DRV_LO : DRV_Z);
        for (int k = 0; k < T1_STEPS_PER_CYCLE; k++) t1_step(b);
    }
}

uint8_t ts1000_ram_peek(board_t *b, uint16_t addr) {
    t1_ram_t *st = b->sim.parts[T1(b)->p_ram].state;
    return st->cells[addr & st->mask & 0x7FF];
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static void t1_destroy(board_t *b) { (void)b; }

static const char *t1_cpu_status(board_t *b) {
    const ts1000_t *z = T1(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!z->rom_loaded) return "no ROM (put the 8K ZX81 ROM in roms/ts1000)";
    return z->in_reset ? "in reset" : "running";
}

static void t1_cpu_regs(board_t *b, char *buf, int len) {
    const z80_t *c = &T1(b)->cpu;
    snprintf(buf, (size_t)len, "PC=%04X SP=%04X AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X",
             c->pc, c->sp, c->af, c->bc, c->de, c->hl, c->ix, c->iy);
}

static const file_kind_t *t1_file_kind(const char *path, char *info, int len, bool *bad) {
    (void)path; (void)info; (void)len; (void)bad;
    return NULL;
}
static bool t1_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)path; (void)run;
    snprintf(msg, (size_t)msglen, "the TS1000 can't load files yet");
    return false;
}

/* The screen as text: the display file is at D_FILE (system variable at $400C), 33 bytes a line. */
static void t1_print_text(board_t *b) {
    uint16_t dfile = (uint16_t)(ts1000_ram_peek(b, 0x400C) | ts1000_ram_peek(b, 0x400D) << 8);
    int col = 0, rows = 0;
    for (uint16_t a = dfile; rows < 24 && a < 0x4800 + 0x4000; a++) {
        uint8_t c = ts1000_ram_peek(b, a);
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

/* Option: how much RAM. The ZX81 had 1K (the same board, A10 of the RAM not wired: the 1K shows again at $4400); the TS1000 has 2K. */
static const char *const t1_ram_values[] = { "2K (TS1000)", "1K (ZX81)" };
static const char *const t1_ram_ids[] = { "2k", "1k" };
static const machine_option_t t1_options[] = {
    { "ram", "RAM", "The TS1000's 2K of RAM, or the ZX81's 1K (the RAM chip's A10 is left out, so the 1K repeats at $4400)",
      t1_ram_values, t1_ram_ids, 2 },
};
static int t1_get_option(board_t *b, int option) { (void)option; return T1(b)->ram1k; }
static void t1_set_option(board_t *b, int option, int value) {
    (void)option;
    T1(b)->ram1k = value != 0;
    board_config_changed(b);
    board_rebuild(b);
    board_power_on(b);
}

const machine_t machine_ts1000 = {
    .id = "ts1000",
    .name = "Timex Sinclair 1000",
    .issue = "TS1000/ZX81",
    .board_name = "TS1000 Issue 3 (reconstructed netlist)",
    .size = sizeof(ts1000_t),
    .data = &bd_ts1000,
    .sim = { .rail_of = t1_rail_of, .kind_of = t1_kind_of, .cycles_per_us = 3.25 },
    .cpu_hz = T1_FREQ,
    .cycles_per_frame = T1_CYCLES_PER_FRAME,
    .steps_per_cycle = T1_STEPS_PER_CYCLE,
    .screen_w = ULA81_PIXELS_PER_LINE, .screen_h = ULA81_LINES,
    .ready_cycles = 2 * T1_FREQ,
    .parts = t1_part_defs, .nparts = sizeof(t1_part_defs) / sizeof(t1_part_defs[0]),
    .conns = t1_conn_defs, .nconns = sizeof(t1_conn_defs) / sizeof(t1_conn_defs[0]),
    .link_resistors = "R6 R1", .crystals = "X1",
    .diodes = "D", .jumpers = "", .supplies = "",
    .keys_help = "Keys: type; Left Shift = SHIFT, Enter = NEWLINE, Backspace = RUBOUT (= DELETE), arrows = cursor, Esc = BREAK",
    .reset_help = "Hold to press RESET: shorts the Z80 /RESET (IC3 pin 26) to GND",
    .options = t1_options, .noptions = 1,
    .get_option = t1_get_option,
    .set_option = t1_set_option,
    .create = t1_create,
    .destroy = t1_destroy,
    .power_on = t1_power_on,
    .run = t1_run,
    .joins = t1_joins,
    .key_down = t1_key_down,
    .key_up = t1_key_up,
    .frame_update = t1_frame_update,
    .host_key = t1_host_key,
    .host_char = t1_host_char,
    .monitor = t1_monitor,
    .files_title = "Open a program",
    .file_kind = t1_file_kind,
    .load = t1_load,
    .cpu_status = t1_cpu_status,
    .cpu_regs = t1_cpu_regs,
    .print_text = t1_print_text,
};
