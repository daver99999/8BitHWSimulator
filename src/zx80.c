/* zx80.c - the Sinclair ZX80 (1980): a Z80A and discrete TTL, no ULA.
 *
 * THE NETLIST (zx80_board_data.c) is transcribed from the "ZX80 circuit diagram": see tools/zx80-netlist-src/sheet01.py.
 *
 * HOW TIME MOVES: the 6.5 MHz oscillator (IC20, the 74LS86 with the crystal X1) is the board clock. Every half period
 * the machine flips it, the board settles, the Z80 sees an edge of its clock if there was one (IC18b divides by two,
 * IC15 buffers it), the memories and flip-flops see their strobes (a commit) and the board settles again. 4 half periods
 * make a CPU clock cycle (3.25 MHz). The crystal itself is not simulated: the machine drives the amplifier's output.
 * C11 (47 pF, R25) makes a short pulse on every rising edge of phi: the machine makes it as a pulse of one commit.
 *
 * TWO ROMS: the ZX80's own 4K ROM (8332, /CS1 = /ROMCS, CS2 = A12), or the 8K ZX81 ROM (2364, the same pins, but pin 21
 * is A12): option "rom" (zx80 or zx81). Neither is shipped: roms/zx80/ (4096 bytes) and roms/ts1000/ (8192 bytes).
 * The ZX81 ROM runs here in FAST mode only: the board has no NMI generator.
 *
 * NOT modeled: the power supply, the modulator, the EAR / MIC circuits, the edge connector, the 16K RAM pack. */
#include "zx80.h"
#include "sokol/sokol_app.h"   /* key codes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

/* ------------------------------------------------------------------------ */
/* The Z80                                                                   */
/* ------------------------------------------------------------------------ */

static const int z8_addr_pins[16] = { 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5 };
static const int z8_data_pins[8] = { 14, 15, 12, 8, 7, 9, 10, 13 };
enum { Z8Z_CLK = 6, Z8Z_INT = 16, Z8Z_NMI = 17, Z8Z_HALT = 18, Z8Z_MREQ = 19, Z8Z_IORQ = 20, Z8Z_RD = 21, Z8Z_WR = 22,
       Z8Z_BUSAK = 23, Z8Z_WAIT = 24, Z8Z_BUSRQ = 25, Z8Z_RESET = 26, Z8Z_M1 = 27, Z8Z_RFSH = 28 };

static void z8_cpu_eval(sim_t *s, part_t *p) {
    zx80_t *z = Z8(p->user);
    uint64_t pins = z->cpu_pins;
    bool aoe = (pins & Z80_AOE) != 0;
    part_drive_bus_cached(s, p, z8_addr_pins, 16, Z80_GET_ADDR(pins), aoe, &z->cache_addr);
    pin_out(s, p, Z8Z_MREQ, aoe ? DRV_BIT(!(pins & Z80_MREQ)) : DRV_Z);
    pin_out(s, p, Z8Z_IORQ, aoe ? DRV_BIT(!(pins & Z80_IORQ)) : DRV_Z);
    pin_out(s, p, Z8Z_RD, aoe ? DRV_BIT(!(pins & Z80_RD)) : DRV_Z);
    pin_out(s, p, Z8Z_WR, aoe ? DRV_BIT(!(pins & Z80_WR)) : DRV_Z);
    pin_out(s, p, Z8Z_M1, DRV_BIT(!(pins & Z80_M1)));
    pin_out(s, p, Z8Z_RFSH, DRV_BIT(!(pins & Z80_RFSH)));
    pin_out(s, p, Z8Z_HALT, DRV_BIT(!(pins & Z80_HALT)));
    pin_out(s, p, Z8Z_BUSAK, DRV_BIT(!(pins & Z80_BUSAK)));
    part_drive_bus_cached(s, p, z8_data_pins, 8, Z80_GET_DATA(pins), (pins & Z80_DOE) != 0, &z->cache_data);
}
static const part_type_t PT_Z8Z80 = { "Z80A",
    "A11:O A12:O A13:O A14:O A15:O CLK:m D4:B D3:B D5:B D6:B VCC:P D2:B D7:B D0:B D1:B /INT:m /NMI:m "
    "/HALT:O /MREQ:O /IORQ:O /RD:O /WR:O /BUSAK:O /WAIT:m /BUSRQ:m /RESET:m /M1:O /RFSH:O GND:P A0:O "
    "A1:O A2:O A3:O A4:O A5:O A6:O A7:O A8:O A9:O A10:O", z8_cpu_eval, 0, 0, 0, TECH_NMOS };

/* /INT (pin 16) is the A6 line: A6 low in a refresh cycle ends the HALT of a display line. */
static void z8_cpu_edge(board_t *b, int clk) {
    zx80_t *z = Z8(b);
    sim_t *s = &b->sim;
    part_t *p = &s->parts[z->p_cpu];
    if (p->dead) return;
    uint64_t pins = clk ? Z80_CLK : 0;
    Z80_SET_DATA(pins, (uint64_t)part_read_bus(s, p, z8_data_pins, 8));
    if (!pin_in(s, p, Z8Z_INT)) pins |= Z80_INT;
    if (!pin_in(s, p, Z8Z_NMI)) pins |= Z80_NMI;
    if (!pin_in(s, p, Z8Z_WAIT)) pins |= Z80_WAIT;
    if (!pin_in(s, p, Z8Z_RESET)) pins |= Z80_RES;
    if (!pin_in(s, p, Z8Z_BUSRQ)) pins |= Z80_BUSRQ;
    z->in_reset = (pins & Z80_RES) != 0;
    uint64_t out = z80_tick(&z->cpu, pins);
    if (out != z->cpu_pins) { z->cpu_pins = out; sim_mark_dirty(s, z->p_cpu); }
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static const part_def_t z8_part_defs[] = {
    { "IC1", &PT_Z8Z80, "CPU", ROLE_CHIP, "Z80A", "Z80A CPU, 3.25 MHz" },
    { "IC2", &PT_ZX_ROM, "ROM", ROLE_ROM, "ROM\n$0000", "ROM: the ZX80's 4K (8332) or, as an option, the ZX81's 8K (2364). Pin 21 is /CS2 (4K) or A12 (8K)" },
    { "IC3", &PT_2114, "RAM", ROLE_VRAM, "2114\n$4000-$43FF", "Static RAM 1K x 4: D0-D3 of the program, the variables and the display file" },
    { "IC4", &PT_2114, "RAM", ROLE_VRAM, "2114\n$4000-$43FF", "Static RAM 1K x 4: D4-D7" },
    { "IC5", &PT_74LS373, "LATCH", ROLE_GLUE, "74LS373", "Character latch: holds the character code the CPU just read from the display file, for the 8 lines of the character" },
    { "IC6", &PT_74LS157, "MUX", ROLE_GLUE, "74LS157", "Address multiplexer: the CPU's A8, A14, A15 (/RFSH high) or the character latch (refresh)" },
    { "IC7", &PT_74LS157, "MUX", ROLE_GLUE, "74LS157", "Address multiplexer: A4-A7 of the ROM, from the CPU or the character latch" },
    { "IC8", &PT_74LS157, "MUX", ROLE_GLUE, "74LS157", "Address multiplexer: A0-A3 of the ROM, from the CPU or the line counter and the latch" },
    { "IC9", &PT_ZX_LS165, "SHIFT", ROLE_GLUE, "74LS165", "Video shift register: loaded with the character's bits, shifted out at 6.5 MHz" },
    { "IC10", &PT_ZX_LS365, "KBD", ROLE_GLUE, "74LS365", "Keyboard buffer: puts the keys (and EAR on D7) on the data bus when /KBD is low" },
    { "IC11", &PT_74LS00, "GLUE", ROLE_GLUE, "74LS00", "Gates: the keyboard / vertical sync latch" },
    { "IC12", &PT_74LS00, "GLUE", ROLE_GLUE, "74LS00", "Gates: the inverse video latch, the RAM and ROM selects" },
    { "IC13", &PT_74LS04, "GLUE", ROLE_GLUE, "74LS04", "Inverters" },
    { "IC14", &PT_7406, "NOP", ROLE_GLUE, "74LS05", "Open collector inverters: pull the data bus low (a NOP) while the display is made" },
    { "IC15", &PT_7406, "NOP", ROLE_GLUE, "74LS05", "Open collector inverters: D6, D7 to the NOP node, the Z80 clock, /M1" },
    { "IC16", &PT_74LS10, "GLUE", ROLE_GLUE, "74LS10", "3-input NANDs: the shift register load, the latch enable" },
    { "IC17", &PT_74LS32, "GLUE", ROLE_GLUE, "74LS32", "OR gates: the keyboard strobe, the NOP node" },
    { "IC18", &PT_74LS74R, "SYNC", ROLE_GLUE, "74LS74", "Flip-flops: IC18a the sync start, IC18b divides the 6.5 MHz by two for the Z80 clock (phi)" },
    { "IC19", &PT_74LS74R, "SYNC", ROLE_GLUE, "74LS74", "Flip-flops: the SYNC output" },
    { "IC20", &PT_ZX_OSC86, "XTAL", ROLE_CLOCK, "74LS86", "XORs: the 6.5 MHz crystal oscillator, the video output" },
    { "IC21", &PT_ZX_LS93, "LINES", ROLE_GLUE, "74LS93", "Line counter: the 8 lines of a character row (counts the SYNC pulses)" },
    { "C11", &PT_ZX_DIFF, "C11", ROLE_GLUE, "47p", "Differentiator: a pulse on every rising edge of phi (with R25)" },
};

static const conn_def_t z8_conn_defs[] = {
    { "KB1", "Keyboard (columns)", "1=KBD0 2=KBD1 3=KBD2 4=KBD3 5=KBD4" },
    { "KB2", "Keyboard (half rows)", "1=A8 2=A9 3=A10 4=A11 5=A12 6=A13 7=A14 8=A15" },
};

static int z8_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int z8_kind_of(const char *ref) {
    if (!strcmp(ref, "X1") || !strncmp(ref, "KB", 2)) return PART_CONNECTOR;
    if (!strncmp(ref, "IC", 2) || !strcmp(ref, "C11")) return PART_IC;
    if (ref[0] == 'R' && (ref[1] >= '0' && ref[1] <= '9')) return PART_RESISTOR;
    return PART_OTHER;      /* the diodes, C9 (made elements in z8_create) */
}

static part_t *z8_find(board_t *b, const char *ref) {
    int i = sim_find_part(&b->sim, ref);
    return i >= 0 ? &b->sim.parts[i] : NULL;
}
static int z8_pin_of(board_t *b, const char *ref, int number) {
    part_t *p = z8_find(b, ref);
    return p ? part_pin(p, number) : -1;
}

static void z8_make_element(board_t *b, const char *ref, int pin_a, int pin_b, int type) {
    part_t *p = z8_find(b, ref);
    if (!p || part_pin(p, pin_a) < 0 || part_pin(p, pin_b) < 0) return;
    resistor_state_t *st = p->state;
    if (!st) { st = calloc(1, sizeof(*st)); p->state = st; p->state_size = sizeof(*st); }
    p->kind = PART_RESISTOR;
    st->nel = 1;
    st->el[0] = (element_t){ part_pin(p, pin_a), part_pin(p, pin_b), EL_ANALOG };
    element_set_type(&b->sim, p, 0, type);
}

bool zx80_kbd81;     /* option "keyboard": the ZX81's keyboard on this board (the Keyboard window follows it) */

static void z8_init_keymap(zx80_t *z) {
    /* The ZX80's matrix is the ZX81's (column = address line A8+n, line = KBD0..KBD4) but its shifted keys are not: SHIFT+Y is ",
     * +U $, +I (, +O ), +P *, +H ** (^), +J -, +K +, +L =, +Z :, +X ;, +C ?, +V /, +M >, +N <, +. ,; SHIFT+1..4 are the keywords
     * NOT AND THEN TO, +5..8 the cursors, +9 HOME, +0 RUBOUT, +NEWLINE is EDIT. */
    kbd_init(&z->kbd, 1);
    kbd_register_modifier(&z->kbd, 0, 0, 0);     /* shift */
    const char *keymap =
        " zxcv" "asdfg" "qwert" "12345" "09876" "poiuy" " lkjh" " .mnb"
        " ZXCV" "ASDFG" "QWERT" "     " "     " "POIUY" " LKJH" " .MNB"
        " :;?/" "     " "     " "     " "     " "*)($\"" " =+-^" " ,><  ";
    if (z->kbd81)      /* the ZX81's: SHIFT+P is ", Y has none, B is *, H has none */
        keymap = " zxcv" "asdfg" "qwert" "12345" "09876" "poiuy" " lkjh" " .mnb"
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
    if (z->kbd81) {
        kbd_register_key(&z->kbd, 0x07, 3, 0, 1);    /* edit: shift + 1 */
    } else {
        kbd_register_key(&z->kbd, 0x05, 4, 1, 1);    /* home: shift + 9 */
        kbd_register_key(&z->kbd, 0x07, 6, 0, 1);    /* edit: shift + newline */
    }
}

/* The ROM images: a 4096 byte one is the ZX80's, an 8192 byte one the ZX81's; in roms/zx80, roms/ts1000, roms or . */
static void z8_find_roms(board_t *b) {
    zx80_t *z = Z8(b);
    static const char *const dirs[] = { "roms/zx80", "roms/ts1000", "roms", "." };
    z->have80 = z->have81 = false;
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]) && !(z->have80 && z->have81); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir)) && !(z->have80 && z->have81)) {
            size_t l = strlen(e->d_name);
            if (l < 5 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom"))) continue;
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], e->d_name);
            FILE *f = fopen(path, "rb");
            if (!f) continue;
            uint8_t buf[8193];
            size_t n = fread(buf, 1, sizeof(buf), f);
            fclose(f);
            if (n == sizeof(z->rom80) && !z->have80) { memcpy(z->rom80, buf, n); z->have80 = true; }
            else if (n == sizeof(z->rom81) && !z->have81) { memcpy(z->rom81, buf, n); z->have81 = true; }
        }
        closedir(dir);
    }
}

typedef struct { const uint8_t *data; int32_t out_cache; int size; } z8_rom_view_t;   /* as zx80_parts.c's z8_rom_t */

static void z8_apply_rom(board_t *b) {
    zx80_t *z = Z8(b);
    z8_rom_view_t *st = b->sim.parts[z->p_rom].state;
    if (z->rom_sel == 1 && !z->have81) z->rom_sel = 0;
    if (z->rom_sel == 0 && !z->have80 && z->have81) z->rom_sel = 1;
    st->data = z->rom_sel ? z->rom81 : (z->have80 ? z->rom80 : NULL);
    st->size = z->rom_sel ? 8192 : 4096;
    if (!z->have80 && !z->have81) st->data = NULL;
    st->out_cache = -1;
    sim_mark_dirty(&b->sim, z->p_rom);
}

static void z8_create(board_t *b) {
    zx80_t *z = Z8(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "IC1");
    z->p_rom = sim_find_part(s, "IC2");
    z->p_ram = sim_find_part(s, "IC3");
    z->p_osc = sim_find_part(s, "IC20");
    z->p_diff = sim_find_part(s, "C11");
    z->p_x1 = sim_find_part(s, "X1");
    z->pin_clk = part_pin(&s->parts[z->p_cpu], Z8Z_CLK);
    z->pin_phi = z8_pin_of(b, "IC18", 9);
    z->pin_video = z8_pin_of(b, "IC20", 6);
    z->pin_sync = z8_pin_of(b, "IC19", 5);
    z8_find_roms(b);
    z->rom_sel = z->have80 ? 0 : 1;
    z8_apply_rom(b);
    /* R4-R11 buffer the data bus between the CPU and the memory (let each side drive its half): series elements. */
    for (int i = 4; i <= 11; i++) { char ref[8]; snprintf(ref, sizeof(ref), "R%d", i); part_t *p = z8_find(b, ref);
        if (p && p->state && ((resistor_state_t *)p->state)->nel) element_set_type(s, p, 0, EL_SERIES); }
    static const char *const ser[] = { "R19", "R24", "R30", "R32" };   /* RAM select, LOADI-LOADD, the video mixer */
    for (int i = 0; i < 4; i++) { part_t *p = z8_find(b, ser[i]);
        if (p && p->state && ((resistor_state_t *)p->state)->nel) element_set_type(s, p, 0, EL_SERIES); }
    z8_make_element(b, "C9", 1, 2, EL_LINK);            /* 2 phi reaches the shift register's clock through C9 */
    /* The keyboard diodes: the half row (pin 1) is the anode, the address line (pin 2) the cathode. */
    for (int i = 3; i <= 11; i++) { char ref[8]; snprintf(ref, sizeof(ref), "D%d", i); z8_make_element(b, ref, 1, 2, EL_DIODE); }
    for (int c = 0; c < 8; c++) z->pin_row[c] = z8_pin_of(b, "KB2", 1 + c);
    for (int l = 0; l < 5; l++) z->pin_kb[l] = z8_pin_of(b, "KB1", 1 + l);
    z->kbd81 = zx80_kbd81;
    z8_init_keymap(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    z->pin_reset_net = z8_pin_of(b, "IC1", 26);          /* /RESET: held low at power on (R21, C10 pull it up and make the delay) */
    b->pin_reset = z8_pin_of(b, "IC1", 26);
    b->pin_reset_gnd = z8_pin_of(b, "IC1", 29);
    char name[16];
    int bit = 0;
    static const struct { const char *fmt; int n; } buses[] = { { "A%d", 16 }, { "D%d", 8 }, { "D%dP", 8 }, { "A%dP", 9 } };
    for (size_t k = 0; k < sizeof(buses) / sizeof(buses[0]); k++)
        for (int i = 0; i < buses[k].n; i++) {
            snprintf(name, sizeof(name), buses[k].fmt, i);
            sim_set_bus_bit(s, sim_find_net(s, name), bit++);
        }
}

static void z8_power_on(board_t *b) {
    zx80_t *z = Z8(b);
    z->cpu_pins = z80_init(&z->cpu);
    z->in_reset = true;
    z->clk = 0;
    z->phi = 0;
    z->reset_until = Z8_FREQ / 20;      /* 50 ms */
    z->vx = z->vy = z->vsync_len = z->vsync_prev = 0;
    z8_apply_rom(b);
    for (int i = 0; i < ULA81_PIXELS_PER_LINE * ULA81_LINES; i++) b->screen[i] = 0xFF000000u;
}

static void z8_joins(board_t *b) {
    zx80_t *z = Z8(b);
    sim_t *s = &b->sim;
    for (int l = 0; l < 5; l++)
        for (int c = 0; c < 8; c++) {
            if (!((z->key_matrix[l] >> c) & 1)) continue;
            int r = z->pin_row[c], k = z->pin_kb[l];
            if (r < 0 || k < 0 || s->pins[r].fault == PF_OPEN || s->pins[k].fault == PF_OPEN) continue;
            sim_join(s, s->pins[r].net, s->pins[k].net);
        }
}

static void z8_sync_keys(board_t *b) {
    zx80_t *z = Z8(b);
    bool changed = false;
    for (int l = 0; l < 5; l++) {
        uint16_t m = z->kbd.scanout_column_masks[l];
        if (m != z->key_matrix[l]) { z->key_matrix[l] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}
static void z8_key_down(board_t *b, int key) { kbd_key_down(&Z8(b)->kbd, key); z8_sync_keys(b); }
static void z8_key_up(board_t *b, int key) { kbd_key_up(&Z8(b)->kbd, key); z8_sync_keys(b); }
static void z8_frame_update(board_t *b, uint32_t us) { kbd_update(&Z8(b)->kbd, us); z8_sync_keys(b); }

static int z8_host_key(int keycode) {
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
static int z8_host_char(int c) { return c; }

static void z8_monitor(board_t *b, uint32_t *out) {
    b->no_signal = b->sim.parts[Z8(b)->p_cpu].dead;
    memcpy(out, b->screen, sizeof(uint32_t) * ULA81_PIXELS_PER_LINE * ULA81_LINES);
}

/* ------------------------------------------------------------------------ */
/* Time                                                                      */
/* ------------------------------------------------------------------------ */

/* The picture: the composite signal is SYNC (low = sync tip) and VIDEO (high = white). A sync pulse that is long is the
 * vertical sync (the line counter starts over); the end of every sync pulse starts a line. Two steps make a pixel. */
static void z8_video(board_t *b) {
    zx80_t *z = Z8(b);
    sim_t *s = &b->sim;
    int sync = !sim_read(s, z->pin_sync);              /* 1 while the sync pulse lasts */
    if (sync) {
        z->vsync_len++;
    } else {
        if (z->vsync_prev) {                       /* the end of a sync pulse */
            if (z->vsync_len > 1000) z->vy = 0; else z->vy++;
            z->vx = 0;
        }
        z->vsync_len = 0;
        int x = z->vx++ / 2 + 20;
        if (z->vy >= 0 && z->vy < ULA81_LINES && x < ULA81_PIXELS_PER_LINE)
            b->screen[z->vy * ULA81_PIXELS_PER_LINE + x] = sim_read(s, z->pin_video) ? 0xFFFFFFFFu : 0xFF000000u;
    }
    z->vsync_prev = sync;
}

void (*zx80_step_hook)(board_t *b);   /* debugging */
static void z8_step(board_t *b) {
    zx80_t *z = Z8(b);
    sim_t *s = &b->sim;
    part_t *x1 = z->p_x1 >= 0 ? &s->parts[z->p_x1] : NULL;
    if (!(x1 && (x1->dead || x1->removed))) {       /* the crystal oscillates */
        ((uint8_t *)s->parts[z->p_osc].state)[0] ^= 1;
        sim_mark_dirty(s, z->p_osc);
    }
    sim_settle(s);
    int clk = sim_read(s, z->pin_clk);
    if (clk != z->clk) {
        z->clk = clk;
        z8_cpu_edge(b, clk);
        sim_settle(s);
    }
    sim_commit(s);
    sim_settle(s);
    int phi = sim_read(s, z->pin_phi);
    if (phi && !z->phi) {                           /* C11: a pulse on the rising edge of phi */
        uint8_t *pulse = s->parts[z->p_diff].state;
        *pulse = 1;
        sim_mark_dirty(s, z->p_diff);
        sim_settle(s);
        sim_commit(s);
        sim_settle(s);
        *pulse = 0;
        sim_mark_dirty(s, z->p_diff);
        sim_settle(s);
    }
    z->phi = phi;
    z8_video(b);
    if (zx80_step_hook) zx80_step_hook(b);
    board_sample(b);
}

static void z8_run(board_t *b, int cycles) {
    zx80_t *z = Z8(b);
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        sim_drive(s, z->pin_reset_net, s->cycle < z->reset_until ? DRV_LO : DRV_Z);
        for (int k = 0; k < Z8_STEPS_PER_CYCLE; k++) z8_step(b);
    }
}

uint8_t zx80_ram_peek(board_t *b, uint16_t addr) {
    /* Two 2114s (1K x 4). Their address and data pins are wired in a scrambled order (it does not matter to the CPU):
     * part address bits 0-2 are A2 A1 A0, bits 3-9 are A3-A9; part data bits 0-3 are IC3: D7 D1 D0 D2, IC4: D4 D3 D5 D6. */
    zx80_t *z = Z8(b);
    int pa = z->p_ram, pb = sim_find_part(&b->sim, "IC4");
    const sram_state_t *r3 = b->sim.parts[pa].state, *r4 = b->sim.parts[pb].state;
    unsigned a10 = addr & 0x3FF;
    unsigned idx = ((a10 >> 2) & 1) | (((a10 >> 1) & 1) << 1) | ((a10 & 1) << 2) | (a10 & 0x3F8);
    unsigned n3 = r3->cells[idx] & 15, n4 = r4->cells[idx] & 15;
    static const int d3[4] = { 7, 1, 0, 2 }, d4[4] = { 4, 3, 5, 6 };
    unsigned v = 0;
    for (int i = 0; i < 4; i++) { v |= ((n3 >> i) & 1u) << d3[i]; v |= ((n4 >> i) & 1u) << d4[i]; }
    return (uint8_t)v;
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static void z8_destroy(board_t *b) { (void)b; }

static const char *z8_cpu_status(board_t *b) {
    const zx80_t *z = Z8(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!z->have80 && !z->have81) return "no ROM (put the 4K ZX80 ROM in roms/zx80 and/or the 8K ZX81 ROM in roms/ts1000)";
    return z->in_reset ? "in reset" : "running";
}

static void z8_cpu_regs(board_t *b, char *buf, int len) {
    const z80_t *c = &Z8(b)->cpu;
    snprintf(buf, (size_t)len, "PC=%04X SP=%04X AF=%04X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X",
             c->pc, c->sp, c->af, c->bc, c->de, c->hl, c->ix, c->iy);
}

static const file_kind_t *z8_file_kind(const char *path, char *info, int len, bool *bad) {
    (void)path; (void)info; (void)len; (void)bad;
    return NULL;
}
static bool z8_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)path; (void)run;
    snprintf(msg, (size_t)msglen, "the ZX80 can't load files yet");
    return false;
}

/* The screen as text: the display file is at D_FILE ($4003 on the ZX80, $400C on the ZX81), 0x76 ends a line. */
static void z8_print_text(board_t *b) {
    int at = Z8(b)->rom_sel ? 0x400C : 0x4003;
    uint16_t dfile = (uint16_t)(zx80_ram_peek(b, (uint16_t)at) | zx80_ram_peek(b, (uint16_t)(at + 1)) << 8);
    int col = 0, rows = 0;
    for (int n = 0; n < 1100 && rows < 24; n++) {
        uint8_t c = zx80_ram_peek(b, (uint16_t)(dfile + n));
        if (c == 0x76) { if (n) putchar('\n'); col = 0; rows++; continue; }
        const char *sym = "\"#$:?()><=+-*/;,.";
        char ch = c == 0 ? ' ' : c >= 0x1C && c <= 0x25 ? (char)('0' + c - 0x1C) : c >= 0x26 && c <= 0x3F ? (char)('A' + c - 0x26) :
                  c >= 0x0B && c <= 0x1B ? sym[c - 0x0B] : '?';
        putchar(ch);
        if (++col > 32) break;
    }
    putchar('\n');
}

/* Option: which ROM is fitted. */
static const char *const z8_rom_values[] = { "ZX80 4K", "ZX81 8K" };
static const char *const z8_rom_ids[] = { "zx80", "zx81" };
static const char *const z8_kbd_values[] = { "ZX80", "ZX81" };
static const char *const z8_kbd_ids[] = { "zx80", "zx81" };
static const machine_option_t z8_options[] = {
    { "rom", "ROM", "The ZX80's own 4K ROM (8332), or the ZX81's 8K ROM (2364) in its place (FAST mode only: there is no NMI generator)",
      z8_rom_values, z8_rom_ids, 2 },
    { "keyboard", "Keyboard", "The ZX80's keyboard (same matrix as the ZX81's, different shifted keys and legends) or the ZX81's",
      z8_kbd_values, z8_kbd_ids, 2 },
};
static int z8_get_option(board_t *b, int option) { return option == 0 ? Z8(b)->rom_sel : Z8(b)->kbd81; }
static void z8_set_option(board_t *b, int option, int value) {
    zx80_t *z = Z8(b);
    if (option == 1) {
        z->kbd81 = value != 0;
        zx80_kbd81 = z->kbd81;
        z8_init_keymap(z);
        memset(z->key_matrix, 0, sizeof(z->key_matrix));
        board_rebuild(b);
        return;
    }
    if (value == 1 && !z->have81) return;
    if (value == 0 && !z->have80) return;
    z->rom_sel = value;
    board_config_changed(b);
    board_rebuild(b);
    board_power_on(b);
}

const machine_t machine_zx80 = {
    .id = "zx80",
    .name = "Sinclair ZX80",
    .issue = "ZX80",
    .board_name = "ZX80 (schematic)",
    .size = sizeof(zx80_t),
    .data = &bd_zx80,
    .sim = { .rail_of = z8_rail_of, .kind_of = z8_kind_of, .cycles_per_us = 3.25 },
    .cpu_hz = Z8_FREQ,
    .cycles_per_frame = Z8_CYCLES_PER_FRAME,
    .steps_per_cycle = Z8_STEPS_PER_CYCLE,
    .screen_w = ULA81_PIXELS_PER_LINE, .screen_h = ULA81_LINES,
    .ready_cycles = 2 * Z8_FREQ,
    .parts = z8_part_defs, .nparts = sizeof(z8_part_defs) / sizeof(z8_part_defs[0]),
    .conns = z8_conn_defs, .nconns = sizeof(z8_conn_defs) / sizeof(z8_conn_defs[0]),
    .link_resistors = "", .crystals = "",
    .diodes = "D", .jumpers = "", .supplies = "",
    .keys_help = "Keys: type; Left Shift = SHIFT, Enter = NEWLINE, Backspace = RUBOUT (= DELETE), arrows = cursor, Esc = BREAK",
    .reset_help = "Hold to press RESET: shorts the Z80 /RESET (IC1 pin 26) to GND",
    .create = z8_create,
    .destroy = z8_destroy,
    .power_on = z8_power_on,
    .run = z8_run,
    .joins = z8_joins,
    .key_down = z8_key_down,
    .key_up = z8_key_up,
    .frame_update = z8_frame_update,
    .host_key = z8_host_key,
    .host_char = z8_host_char,
    .monitor = z8_monitor,
    .files_title = "Open a program",
    .file_kind = z8_file_kind,
    .load = z8_load,
    .cpu_status = z8_cpu_status,
    .cpu_regs = z8_cpu_regs,
    .print_text = z8_print_text,
    .options = z8_options, .noptions = 2,
    .get_option = z8_get_option,
    .set_option = z8_set_option,
};
