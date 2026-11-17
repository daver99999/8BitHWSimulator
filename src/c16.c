/* c16.c - the Commodore 16.
 *
 * The TED (U1) makes the system clock out of the 14.31818 MHz crystal Y1 (a source on CLKIN, PT_XTAL14;
 * the oscillator transistors Q1 and Q2 aren't simulated): phase 0 on pin 12 runs the 7501 CPU (U2) and
 * the PLA (U16), and the TED makes the DRAM strobes (/RAS, /CAS, MUX), the ROM selects (/CS0, /CS1) and
 * the CPU's bus control (AEC, BA) too. Nothing in this file drives a bus cycle: a step of the board is
 * half a period of the crystal (35 ns), 32 are a cycle (0.895 MHz) and the chips watch their own clocks.
 *
 * The memory: two 4416s (U5, U6: 16K x 4 each, the low and the high nibble) behind the address
 * multiplexers U7 and U8; BASIC (U3) and the KERNAL (U4) 23128 ROMs, selected through the ROM latch
 * (U15, $FDD0-$FDDF) and the 74LS139 U14. The keyboard is an 8 x 8 matrix between the 6529B U13 at
 * $FD30 (the rows) and the TED's K lines (the columns, latched by writing $FF08). */
#include "c16.h"
#include "sokol/sokol_app.h"   /* key codes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#define GLUE(ref, type, label, what) { ref, type, label, ROLE_GLUE, NULL, what }

static const part_def_t c16_part_defs[] = {
    { "U2", &PT_7501, "CPU", ROLE_CHIP, "7501", "7501 CPU, 0.89 MHz: phase 0 comes from the TED; AEC and BA (RDY) from the TED stop it while the TED uses the bus" },
    { "U1", &PT_7360, "TED", ROLE_CHIP, "7360 TED\nvideo + sound\n$FF00-$FF1F",
      "7360 TED: video, sound, timers, the DRAM and ROM strobes, the system clock and the keyboard latch" },
    { "U13", &PT_6529B, "KEYPORT", ROLE_CHIP, "6529B\n$FD30", "6529B single port: the keyboard rows (the TED's K lines are the columns)" },
    { "U3", &PT_C16ROM, "BASIC", ROLE_ROM, "23128 ROM\nBASIC 3.5\n$8000-$BFFF", "23128 BASIC 3.5 ROM (318006-01), $8000-$BFFF" },
    { "U4", &PT_C16ROM, "KERNAL", ROLE_ROM, "23128 ROM\nKERNAL\n$C000-$FFFF", "23128 KERNAL ROM (318005-04), $C000-$FFFF" },
    { "U5", &PT_4416, "RAM", ROLE_RAM, "4416\n16K x 4\nbits 0-3", "4416 dynamic RAM, 16K x 4: bits 0-3 of $0000-$3FFF" },
    { "U6", &PT_4416, "RAM", ROLE_RAM, "4416\n16K x 4\nbits 4-7", "4416 dynamic RAM, 16K x 4: bits 4-7 of $0000-$3FFF" },
    { "Y1", &PT_XTAL14, "14.31818", ROLE_CLOCK, "crystal", "14.31818 MHz crystal (modelled as a clock source on the TED's CLKIN)" },
    { "U10", &PT_LM555, "NE555", ROLE_CLOCK, "reset", "NE555 power-on reset timer: holds /RESET low for a while (through the 7406 U9)" },
    GLUE("U16", &PT_7700, "7700 PLA", "7700-010 PLA: decodes $FD00-$FDDF (the cassette, the keyboard port, the ROM latch) and makes phase 2"),
    GLUE("U15", &PT_74LS175, "74LS175", "74LS175: the ROM latch, written by any write to $FDD0-$FDDF (the low address bits)"),
    GLUE("U14", &PT_74LS139, "74LS139", "74LS139: turns the TED's /CS0 and /CS1 and the ROM latch into the ROM and cartridge selects"),
    GLUE("U12", &PT_74LS02, "74LS02", "74LS02: the ROM latch clock and the high ROM select"),
    GLUE("U11", &PT_74LS125, "74LS125", "74LS125: the joystick selects from D1 and D2, and the cassette switch to D2"),
    GLUE("U9", &PT_7406, "7406", "7406: open collector buffers: /RESET, the serial bus, the cassette motor"),
    GLUE("U7", &PT_74LS257, "74LS257", "74LS257: DRAM address multiplexer for A0-A3 (row) and A8-A10 (column)"),
    GLUE("U8", &PT_74LS257, "74LS257", "74LS257: DRAM address multiplexer for A4-A7 (row) and A11-A13 (column)"),
};

static const conn_def_t c16_conn_defs[] = {
    { "CN1", "EXPANSION", "1=GND 2=+5V 3=+5V 4=/IRQ 5=R/W 6=C1HIGH 7=C2LOW 8=C2HIGH 9=/CS1 10=/CS0 11=/CAS 12=MUX 13=BA 14=? "
                          "15=? 16=? 17=? 18=? 19=? 20=? 21=D0 22=AEC 23=EXT_AUDIO 24=PHI2 25=GND A=GND B=C1LOW C=/RESET "
                          "D=/RAS E=PHI0 F=A15 H=A14 J=A13 K=A12 L=A11 M=A10 N=A9 P=A8 R=A7 S=A6 T=A5 U=A4 V=A3 W=A2 X=A1 Y=A0" },
    { "CN2", "KEYBOARD", "1=ROW5 3=COL7 4=GND 6=ROW7 7=COL4 8=ROW1 9=COL5 10=COL6 11=ROW3 12=ROW2 13=ROW4 14=COL2 15=COL1 "
                         "16=ROW6 17=COL3 18=COL0 19=ROW0" },
    { "CN3", "CASSETTE", "1=GND 2=+5V 3=MOTOR 4=READ 5=WRITE 6=SENSE 7=GND" },
    { "CN4", "JOYSTICK 1", "1=UP 2=DOWN 3=LEFT 4=RIGHT 5=+5V 6=FIRE 7=GND 8=SELECT" },
    { "CN5", "JOYSTICK 2", "1=UP 2=DOWN 3=LEFT 4=RIGHT 5=+5V 6=FIRE 7=GND 8=SELECT" },
    { "CN6", "AUDIO/VIDEO", "1=LUM 2=GND 3=AUDIO_OUT 4=VIDEO 5=AUDIO_IN 6=COLOR 7=? 8=+5V" },
    { "CN7", "SERIAL", "1=SRQ 2=GND 3=ATN 4=CLK 5=DATA 6=RESET" },
};

static int c16_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int c16_kind_of(const char *ref) {
    if (ref[0] == 'U') return PART_IC;
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    if (!strncmp(ref, "RP", 2)) return PART_RESISTOR;
    if (!strncmp(ref, "FB", 2) || ref[0] == 'L' || ref[0] == 'F' || !strncmp(ref, "JP", 2)) return PART_LINK;
    if (!strncmp(ref, "CN", 2) && ref[2] >= '0' && ref[2] <= '9') return PART_CONNECTOR;
    return PART_OTHER;
}

/* ------------------------------------------------------------------------ */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------ */

bool c16_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n) {
    c16_t *z = C16(b);
    bool basic = !strcmp(ref, "U3");
    if (!basic && strcmp(ref, "U4")) return false;
    if (n != 16384) return false;
    memcpy(basic ? z->basic : z->kernal, data, n);
    z->rom_mask |= basic ? 1 : 2;
    return true;
}

static const char *c16_rom_ref(const char *path) {
    static const struct { const char *key, *ref; bool own_folder; } map[] = {
        { "318006", "U3", false }, { "318005", "U4", false }, { "basic", "U3", true }, { "kernal", "U4", true },
    };
    const char *name = board_basename(path);
    bool own = strstr(path, "c16/") || strstr(path, "c16\\");
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (map[i].own_folder && !own) continue;
        size_t n = strlen(map[i].key);
        for (const char *s = name; *s; s++)
            if (!strncasecmp(s, map[i].key, n)) return map[i].ref;
    }
    return NULL;
}

static bool c16_fit_rom(board_t *b, const char *path, char *msg, int msglen) {
    const char *ref = c16_rom_ref(path);
    if (!ref) { snprintf(msg, (size_t)msglen, "%s: don't know which ROM this is", path); return false; }
    uint8_t buf[16385];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(buf, 1, sizeof(buf), f) : 0;
    if (f) fclose(f);
    if (!c16_set_rom(b, ref, buf, n)) {
        snprintf(msg, (size_t)msglen, "%s: %zu bytes don't fit socket %s", path, n, ref);
        return false;
    }
    snprintf(msg, (size_t)msglen, "%s fitted in %s", board_basename(path), ref);
    return true;
}

/* At start: the ROMs found in roms/c16, roms or the current folder. */
static void c16_find_roms(board_t *b) {
    static const char *const dirs[] = { "roms/c16", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir))) {
            size_t l = strlen(e->d_name);
            if (l < 5 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom"))) continue;
            char path[1024], msg[256];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], e->d_name);
            const char *ref = c16_rom_ref(path);
            if (!ref) continue;
            if (C16(b)->rom_mask & (!strcmp(ref, "U3") ? 1u : 2u)) continue;
            c16_fit_rom(b, path, msg, sizeof(msg));
        }
        closedir(dir);
    }
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static int c16_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

static void c16_keys_register(c16_t *z);

static void c16_create(board_t *b) {
    c16_t *z = C16(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "U2");
    z->p_ted = sim_find_part(s, "U1");
    z->p_xtal = sim_find_part(s, "Y1");
    z->p_ram[0] = sim_find_part(s, "U5");
    z->p_ram[1] = sim_find_part(s, "U6");
    memset(z->basic, 0xFF, sizeof(z->basic));
    memset(z->kernal, 0xFF, sizeof(z->kernal));
    z->rom_mask = 0;
    int idx;
    if ((idx = sim_find_part(s, "U3")) >= 0) ((rom_state_t *)s->parts[idx].state)->data = z->basic;
    if ((idx = sim_find_part(s, "U4")) >= 0) ((rom_state_t *)s->parts[idx].state)->data = z->kernal;
    c16_find_roms(b);
    if (z->p_ted >= 0) ted_set_screen(&s->parts[z->p_ted], z->raster);
    memset(z->raster, 0, sizeof(z->raster));
    static const int row_pin[8] = { 19, 8, 12, 11, 13, 1, 16, 6 }, col_pin[8] = { 18, 15, 14, 17, 7, 9, 10, 3 };
    for (int i = 0; i < 8; i++) {
        z->pin_row[i] = c16_pin_of(b, "CN2", row_pin[i]);
        z->pin_col[i] = c16_pin_of(b, "CN2", col_pin[i]);
    }
    b->pin_reset = c16_pin_of(b, "U2", 40);             /* 7501 /RES to VSS, as an external reset switch would */
    b->pin_reset_gnd = c16_pin_of(b, "U2", 20);
    c16_keys_register(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
}

static void c16_power_on(board_t *b) {
    b->sim.clock_level = 0;
    memset(C16(b)->raster, 0, sizeof(C16(b)->raster));
}

static void c16_joins(board_t *b) {
    sim_t *s = &b->sim;
    c16_t *z = C16(b);
    /* A closed key joins its row (a 6529 port bit) to its column (a TED K line), unless a pin is open. */
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++) {
            if (!((z->key_matrix[row] >> col) & 1)) continue;
            int pr = z->pin_row[row], pc = z->pin_col[col];
            if (pr < 0 || pc < 0 || s->pins[pr].fault == PF_OPEN || s->pins[pc].fault == PF_OPEN) continue;
            sim_join(s, s->pins[pr].net, s->pins[pc].net);
        }
}

/* One half period of the crystal. */
static void c16_step(board_t *b) {
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, C16(b)->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void c16_run(board_t *b, int cycles) {
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        for (int k = 0; k < C16_STEPS_PER_CYCLE; k++) c16_step(b);
    }
}

uint32_t c16_cpu_cycles(board_t *b) { return c16_cpu_cycles_of(&b->sim.parts[C16(b)->p_cpu]); }

/* RAM: the cell of an address in the 4416s. The row is A0-A7 on MA0-MA7; the column A8-A13 on MA1-MA6,
 * with MA0 and MA7 tied high (the 74LS257s' unused inputs). U5 holds the low nibble. */
static int c16_cell(uint16_t addr) { return ((addr & 0xFF) << 8) | 0x81 | (((addr >> 8) & 0x3F) << 1); }

void c16_ram_poke(board_t *b, uint16_t addr, uint8_t v) {
    c16_t *z = C16(b);
    int cell = c16_cell(addr);
    if (z->p_ram[0] >= 0) ((dram4_state_t *)b->sim.parts[z->p_ram[0]].state)->cells[cell] = v & 15;
    if (z->p_ram[1] >= 0) ((dram4_state_t *)b->sim.parts[z->p_ram[1]].state)->cells[cell] = v >> 4;
}

uint8_t c16_ram_peek(board_t *b, uint16_t addr) {
    c16_t *z = C16(b);
    int cell = c16_cell(addr);
    uint8_t lo = z->p_ram[0] >= 0 ? ((dram4_state_t *)b->sim.parts[z->p_ram[0]].state)->cells[cell] & 15 : 0;
    uint8_t hi = z->p_ram[1] >= 0 ? ((dram4_state_t *)b->sim.parts[z->p_ram[1]].state)->cells[cell] & 15 : 0;
    return (uint8_t)(lo | hi << 4);
}

/* ------------------------------------------------------------------------ */
/* The keyboard                                                              */
/* ------------------------------------------------------------------------ */

#define CK_RETURN 0x0D
#define CK_DEL    0x14
#define CK_HOME   0x13
#define CK_STOP   0x03
#define CK_CBM    0x02
#define CK_CTRL   0x04
#define CK_ESC    0x1B
#define CK_HELP   0x08
#define CK_F1     0x85
#define CK_RIGHT  0x1D
#define CK_DOWN   0x11
#define CK_LEFT   0x9D
#define CK_UP     0x91

/* The matrix: per row (6529 port bit, written to $FD30), the keys of the columns (TED K0-K7, read from
 * $FF08 after a write to it). A single character is the key's own cap. */
static const char *const c16_keys[8][8] = {
    { "DEL", "RETURN", "POUND", "HELP", "F1", "F2", "F3", "@" },
    { "3", "W", "A", "4", "Z", "S", "E", "LSHIFT" },
    { "5", "R", "D", "6", "C", "F", "T", "X" },
    { "7", "Y", "G", "8", "B", "H", "U", "V" },
    { "9", "I", "J", "0", "M", "K", "O", "N" },
    { "DOWN", "P", "L", "UP", ".", ":", "-", "," },
    { "LEFT", "*", ";", "RIGHT", "ESC", "=", "+", "/" },
    { "1", "HOME", "CTRL", "2", "SPACE", "CBM", "Q", "STOP" },
};

const char *c16_key_legend(int row, int col) { return c16_keys[row & 7][col & 7]; }

static void c16_keys_register(c16_t *z) {
    kbd_init(&z->kbd, 2);
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++) {
            const char *n = c16_keys[row][col];
            if (!strcmp(n, "LSHIFT")) { kbd_register_modifier(&z->kbd, 0, col, row); continue; }
            int key = 0;
            if (n[1] == 0) key = (unsigned char)n[0];
            else if (!strcmp(n, "RETURN")) key = CK_RETURN;
            else if (!strcmp(n, "DEL")) key = CK_DEL;
            else if (!strcmp(n, "HOME")) key = CK_HOME;
            else if (!strcmp(n, "STOP")) key = CK_STOP;
            else if (!strcmp(n, "CBM")) key = CK_CBM;
            else if (!strcmp(n, "CTRL")) key = CK_CTRL;
            else if (!strcmp(n, "ESC")) key = CK_ESC;
            else if (!strcmp(n, "HELP")) key = CK_HELP;
            else if (!strcmp(n, "SPACE")) key = ' ';
            else if (!strcmp(n, "POUND")) key = '~';
            else if (!strcmp(n, "RIGHT")) key = CK_RIGHT;
            else if (!strcmp(n, "DOWN")) key = CK_DOWN;
            else if (!strcmp(n, "LEFT")) key = CK_LEFT;
            else if (!strcmp(n, "UP")) key = CK_UP;
            else if (n[0] == 'F' && n[1] >= '1' && n[1] <= '3' && !n[2]) key = CK_F1 + (n[1] - '1');
            if (!key) continue;
            kbd_register_key(&z->kbd, key, col, row, 0);
            if (key >= 'A' && key <= 'Z') kbd_register_key(&z->kbd, key + 32, col, row, 1);     /* shifted: lower case */
            if (n[1] == 0) {
                static const char *const from = "123456789,./:;", *const to = "!\"#$%&'()<>?[]";
                const char *q = strchr(from, n[0]);
                if (q) kbd_register_key(&z->kbd, (unsigned char)to[q - from], col, row, 1);
            }
        }
}

static void c16_sync_keys(board_t *b) {
    c16_t *z = C16(b);
    bool changed = false;
    for (int row = 0; row < 8; row++) {
        uint16_t m = z->kbd.scanout_column_masks[row];
        if (m != z->key_matrix[row]) { z->key_matrix[row] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

static void c16_key_down(board_t *b, int key) { kbd_key_down(&C16(b)->kbd, key); c16_sync_keys(b); }
static void c16_key_up(board_t *b, int key) { kbd_key_up(&C16(b)->kbd, key); c16_sync_keys(b); }
static void c16_frame_update(board_t *b, uint32_t us) { kbd_update(&C16(b)->kbd, us); c16_sync_keys(b); }

static int c16_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return CK_RETURN;
    case SAPP_KEYCODE_BACKSPACE: return CK_DEL;
    case SAPP_KEYCODE_RIGHT: return CK_RIGHT;
    case SAPP_KEYCODE_LEFT: return CK_LEFT;
    case SAPP_KEYCODE_DOWN: return CK_DOWN;
    case SAPP_KEYCODE_UP: return CK_UP;
    case SAPP_KEYCODE_HOME: return CK_HOME;
    case SAPP_KEYCODE_ESCAPE: return CK_STOP;
    case SAPP_KEYCODE_INSERT: return CK_ESC;
    case SAPP_KEYCODE_TAB: return CK_CTRL;
    case SAPP_KEYCODE_LEFT_ALT: return CK_CBM;
    case SAPP_KEYCODE_F1: return CK_F1;
    case SAPP_KEYCODE_F2: return CK_F1 + 1;
    case SAPP_KEYCODE_F3: return CK_F1 + 2;
    case SAPP_KEYCODE_F4: return CK_HELP;
    default: return 0;
    }
}

/* Like the C64, the C16 types capitals without shift. */
static int c16_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static void c16_monitor(board_t *b, uint32_t *out) {
    memcpy(out, C16(b)->raster, sizeof(C16(b)->raster));
}

/* ------------------------------------------------------------------------ */
/* Files                                                                     */
/* ------------------------------------------------------------------------ */

static const file_kind_t c16_rom_kind = { "ROM image", "Fit in its socket", false, ROLE_ROM };
static const file_kind_t c16_prg = { "program", "Load and RUN", true, -1 };

static const file_kind_t *c16_file_kind(const char *path, char *info, int len, bool *bad) {
    const file_kind_t *state = board_state_file_kind(path, info, len, bad);
    if (state) return state;
    size_t pl = strlen(path);
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        const char *ref = c16_rom_ref(path);
        if (!ref) return NULL;
        snprintf(info, (size_t)len, "goes in socket %s", ref);
        return &c16_rom_kind;
    }
    if (pl < 5 || strcasecmp(path + pl - 4, ".prg")) return NULL;
    FILE *f = fopen(path, "rb");
    uint8_t h[4] = {0};
    size_t n = f ? fread(h, 1, sizeof(h), f) : 0;
    if (f) fclose(f);
    unsigned addr = n >= 2 ? h[0] | h[1] << 8 : 0;
    *bad = n < 3;
    if (n < 3) snprintf(info, (size_t)len, "too short for a program");
    else if (addr == 0x1001) snprintf(info, (size_t)len, "BASIC program");
    else snprintf(info, (size_t)len, "machine code at $%04X", addr);
    return &c16_prg;
}

static bool c16_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    size_t pl = strlen(path);
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        if (!c16_fit_rom(b, path, msg, msglen)) return false;
        board_config_changed(b);
        board_power_on(b);
        return true;
    }
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(msg, (size_t)msglen, "can't open %s", path); return false; }
    uint8_t *buf = malloc(65536 + 2);
    size_t n = buf ? fread(buf, 1, 65536 + 2, f) : 0;
    fclose(f);
    if (n < 3) { free(buf); snprintf(msg, (size_t)msglen, "%s is too short", path); return false; }
    unsigned start = buf[0] | buf[1] << 8, end = start + (unsigned)(n - 2);
    if (start < 0x0400 || end > 0x4000) {
        free(buf);
        snprintf(msg, (size_t)msglen, "$%04X-$%04X doesn't fit the C16's 16K of RAM ($0400-$3FFF)", start, end);
        return false;
    }
    for (size_t i = 2; i < n; i++) c16_ram_poke(b, (uint16_t)(start + i - 2), buf[i]);
    free(buf);
    bool basic = start == 0x1001;
    if (basic) {    /* end of the program = start of the variables, arrays, end of arrays ($2D-$32) */
        for (int p = 0x2D; p <= 0x31; p += 2) {
            c16_ram_poke(b, (uint16_t)p, end & 0xFF);
            c16_ram_poke(b, (uint16_t)(p + 1), end >> 8);
        }
    }
    if (run) {      /* type RUN (or SYS) <return> through the keyboard buffer ($0527, its count at $EF) */
        char cmd[16];
        snprintf(cmd, sizeof(cmd), basic ? "RUN\r" : "SYS%u\r", start);
        int len = (int)strlen(cmd);
        for (int i = 0; i < len; i++) c16_ram_poke(b, (uint16_t)(0x0527 + i), (uint8_t)cmd[i]);
        c16_ram_poke(b, 0xEF, (uint8_t)len);
    }
    snprintf(msg, (size_t)msglen, "loaded %s at $%04X-$%04X", path, start, end - 1);
    return true;
}

static void c16_destroy(board_t *b) { (void)b; }

static const char *c16_cpu_status(board_t *b) {
    const c16_t *z = C16(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!(z->rom_mask & 2)) return "no KERNAL ROM (put the C16 ROMs in roms/c16)";
    if (!(z->rom_mask & 1)) return "no BASIC ROM (put the C16 ROMs in roms/c16)";
    return "running";
}

static void c16_cpu_regs(board_t *b, char *buf, int len) { c16_cpu_regs_of(&b->sim.parts[C16(b)->p_cpu], buf, len); }

static void c16_print_text(board_t *b) {
    /* The screen as the video matrix holds it (screen codes to ASCII). */
    c16_t *z = C16(b);
    const part_t *ted = &b->sim.parts[z->p_ted];
    unsigned base = ((unsigned)(ted_reg_of(ted, 0x14) & 0xF8) << 8) + 0x400;
    for (int y = 0; y < 25; y++) {
        char line[48];
        for (int x = 0; x < 40; x++) {
            int c = c16_ram_peek(b, (uint16_t)((base + (unsigned)(y * 40 + x)) & 0x3FFF)) & 0x7F;
            line[x] = c == 0 ? '@' : c < 27 ? (char)('A' + c - 1) : c >= 32 && c < 64 ? (char)c : '.';
        }
        line[40] = 0;
        printf("%s\n", line);
    }
}

const machine_t machine_c16 = {
    .size = sizeof(c16_t),
    .sim = { .rail_of = c16_rail_of, .kind_of = c16_kind_of, .cycles_per_us = 0.894886 },
    .cpu_hz = C16_FREQ,
    .cycles_per_frame = C16_CYCLES_PER_FRAME,
    .steps_per_cycle = C16_STEPS_PER_CYCLE,
    .screen_w = C16_SCREEN_W, .screen_h = C16_SCREEN_H,
    .ready_cycles = 3 * C16_FREQ,
    .conns = c16_conn_defs, .nconns = sizeof(c16_conn_defs) / sizeof(c16_conn_defs[0]),
    .link_resistors = "RP3 RP4",
    .crystals = "Y1",
    .diodes = "D", .jumpers = "",
    .supplies = "9V_IN 9V_RET 9V_L 9V_SW VIN",
    .keys_help = "Keys: type; Esc = RUN/STOP, Insert = ESC, Tab = CTRL, Left Alt = C=, Home = HOME, Backspace = INST/DEL, arrow keys, F1-F3, F4 = HELP",
    .reset_help = "Hold to press RESET: shorts the 7501 /RES (U2 pin 40) to GND",
    .create = c16_create,
    .destroy = c16_destroy,
    .power_on = c16_power_on,
    .run = c16_run,
    .joins = c16_joins,
    .key_down = c16_key_down,
    .key_up = c16_key_up,
    .frame_update = c16_frame_update,
    .host_key = c16_host_key,
    .host_char = c16_host_char,
    .monitor = c16_monitor,
    .files_title = "Open a program or save state",
    .file_kind = c16_file_kind,
    .load = c16_load,
    .cpu_status = c16_cpu_status,
    .cpu_regs = c16_cpu_regs,
    .print_text = c16_print_text,
    .id = "c16",
    .issue = "251788",
    .name = "Commodore 16",
    .board_name = "C16 ASSY 250443 (PCB 251789)",
    .data = &bd_c16,
    .parts = c16_part_defs, .nparts = sizeof(c16_part_defs) / sizeof(c16_part_defs[0]),
};
