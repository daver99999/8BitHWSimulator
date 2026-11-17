/* vic20.c - the Commodore VIC-20.
 *
 * The 6560 (UB7) makes the system clock: the 14.31818 MHz crystal Y1
 * (a source on its pin 1, PT_XTAL14) goes through the gates of UB9, which
 * make the two phases Ø1IN and Ø2IN for the 6560 (pins 39 and 38). The 6560
 * divides them by 14 and puts out phase 1 on pin 35; the 7406 UB4 inverts it
 * into the 6502's phase 0, and the 6502 (UE10, the PET's model) runs on that.
 * The 6560 owns the video bus (VA0-VA13, BD0-BD7, VD8-VD11) during phase 1,
 * the CPU during phase 2; the 74LS245s UD8/UE8 (addresses) and UF8 (data)
 * switch between the CPU's buses and the video bus. Nothing in this file
 * drives a bus cycle: a step of the board is half a period of the crystal
 * (35 ns), 28 are a cycle (1.0227 MHz); the cores watch their own clocks.
 *
 * The keyboard is an 8 x 8 matrix on the second 6522 (UAB1): the columns are
 * port B, the rows port A. */
#include "vic20.h"
#include "pet.h"
#include "sokol/sokol_app.h"   /* key codes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

/* ------------------------------------------------------------------------ */
/* The chips                                                                 */
/* ------------------------------------------------------------------------ */

#define GLUE(ref, type, label, what) { ref, type, label, ROLE_GLUE, NULL, what }

/* The chips both boards have. */
#define VIC20_COMMON_PARTS \
    { "UE10", &PT_PET6502, "CPU", ROLE_CHIP, "6502A", "6502A CPU, 1 MHz: phase 0 comes from the 6560 through the 7406 UB4" }, \
    { "UB7", &PT_6560, "VIC", ROLE_CHIP, "6560\nvideo + sound\n$9000-$900F", \
      "6560 video interface chip (NTSC): the video, the sound, and the system clock; owns the video bus in phase 1" }, \
    { "UAB1", &PT_VIC6522, "VIA2", ROLE_CHIP, "6522\n$9120", "6522 VIA 2: keyboard (port B columns, port A rows), joystick right, cassette" }, \
    { "UAB3", &PT_VIC6522, "VIA1", ROLE_CHIP, "6522\n$9110", "6522 VIA 1: user port, serial bus, joystick, cassette; RESTORE is its CA1 (the NMI)" }, \
    { "UE11", &PT_2364, "BASIC", ROLE_ROM, "2364 ROM\n$C000-$DFFF", "2364 BASIC ROM, $C000-$DFFF" }, \
    { "UE12", &PT_2364, "KERNAL", ROLE_ROM, "2364 ROM\n$E000-$FFFF", "2364 KERNAL ROM, $E000-$FFFF" }, \
    { "UD7", &PT_2332L, "CHAR", ROLE_ROM, "2332 ROM\n$8000-$8FFF", "2332 character ROM, $8000-$8FFF (the 6560 reads it at $0000)" }, \
    { "UE1", &PT_2114, "COLOR", ROLE_VRAM, "2114\ncolor RAM\n$9400-$97FF", "2114 color RAM, 1K x 4 at $9400-$97FF, read by the 6560 for every character" }, \
    { "Y1", &PT_XTAL14, "14.31818", ROLE_CLOCK, "crystal", "14.31818 MHz crystal (modelled as a clock source)" }, \
    { "UB9", &PT_74LS02, "74LS02", ROLE_CLOCK, "oscillator\n+ phases", "74LS02: the crystal oscillator and the two clock phases of the 6560" }, \
    { "UB6", &PT_LM555, "NE555", ROLE_CLOCK, "reset", "NE555 power-on reset timer: holds /RES low for a while (through the 7406 UB4)" }, \
    GLUE("UD8", &PT_74LS245, "74LS245", "74LS245: CPU address A0-A7 to the video bus, enabled in phase 2"), \
    GLUE("UE8", &PT_74LS245, "74LS245", "74LS245: CPU address A8-A12, BLK4 and COLOR to the video bus, enabled in phase 2"), \
    GLUE("UF8", &PT_74LS245, "74LS245", "74LS245: CPU data bus to the buffered data bus"), \
    GLUE("UC5", &PT_74LS138, "74LS138", "74LS138: the eight 8K blocks of the CPU address space (BLK0-BLK7)"), \
    GLUE("UC6", &PT_74LS138, "74LS138", "74LS138: the I/O decode inside BLK4 (VIAs, color RAM, I/O 2 and 3)"), \
    GLUE("UC4", &PT_74LS138, "74LS138", "74LS138: the RAM selects, from the video bus addresses"), \
    GLUE("UC2", &PT_74LS04, "74LS04", "74LS04: inverters (BLK0, the R/W for the 6560, COLOR, the system clock)"), \
    GLUE("UC3", &PT_74LS02, "74LS02", "74LS02: the system clocks SO1 and SO2, the 6560's R/W, the RAM enable"), \
    GLUE("UD9", &PT_74LS133, "74LS133", "74LS133: 13 input NAND: low when no expansion block is selected, in phase 2"), \
    GLUE("UD1", &PT_4066, "4066", "4066: connects the color RAM data to the data bus in phase 2"), \
    GLUE("UB4", &PT_7406, "7406", "7406: open collector buffers: phase 0, /RES, the serial bus")

static const part_def_t vic20b_part_defs[] = {
    VIC20_COMMON_PARTS,
    { "U15", &PT_6116, "RAM", ROLE_RAM, "6116\n2K x 8\n$1000-$17FF", "6116 static RAM, 2K x 8 at $1000-$17FF" },
    { "U14", &PT_6116, "RAM", ROLE_RAM, "6116\n2K x 8\n$1800-$1FFF", "6116 static RAM, 2K x 8 at $1800-$1FFF" },
    { "UD2", &PT_2114, "RAM", ROLE_RAM, "2114\n$0000-$03FF\nbits 0-3", "2114 static RAM, 1K x 4, bits 0-3 of $0000-$03FF" },
    { "UE2", &PT_2114, "RAM", ROLE_RAM, "2114\n$0000-$03FF\nbits 4-7", "2114 static RAM, 1K x 4, bits 4-7 of $0000-$03FF" },
    GLUE("U13", &PT_74LS08, "74LS08", "74LS08: ANDs the RAM selects of the two 6116s"),
};

/* The early board: eleven 2114s (the color RAM UE1 is in the common parts) instead of the 6116s. */
#define VIC20A_RAM(d, e, lo, hi) \
    { d, &PT_2114, "RAM", ROLE_RAM, "2114\n" lo "-" hi "\nbits 0-3", "2114 static RAM, 1K x 4, bits 0-3 of " lo "-" hi }, \
    { e, &PT_2114, "RAM", ROLE_RAM, "2114\n" lo "-" hi "\nbits 4-7", "2114 static RAM, 1K x 4, bits 4-7 of " lo "-" hi }
static const part_def_t vic20a_part_defs[] = {
    VIC20_COMMON_PARTS,
    VIC20A_RAM("UD2", "UE2", "$0000", "$03FF"),
    VIC20A_RAM("UD3", "UE3", "$1000", "$13FF"),
    VIC20A_RAM("UD4", "UE4", "$1400", "$17FF"),
    VIC20A_RAM("UD5", "UE5", "$1800", "$1BFF"),
    VIC20A_RAM("UD6", "UE6", "$1C00", "$1FFF"),
};

/* The connectors: what each pin is for (reference, name, "PAD=FUNCTION"). */
static const conn_def_t vic20_conn_defs[] = {
    { "P1", "USER PORT", "1=GND 2=+5V 3=/RES 4=JOY0 5=JOY1 6=JOY2 7=LITE_PEN 8=CASS_SWITCH 9=ATN_IN 10=9VAC 11=9VAC 12=GND "
                         "A=GND B=CB1 C=PB0 D=PB1 E=PB2 F=PB3 H=PB4 J=PB5 K=PB6 L=PB7 M=CB2 N=GND" },
    { "P5", "CONTROLLER", "1=JOY0_UP 2=JOY1_DOWN 3=JOY2_LEFT 4=JOY3_RIGHT 5=POT_Y 6=LITE_PEN_FIRE 7=+5V 8=GND 9=POT_X" },
    { "P10", "KEYBOARD", "1=GND 2=+5V 3=RESTORE 4=+5V 5=COL7 6=COL6 7=COL5 8=COL4 9=COL3 10=COL2 11=COL1 12=COL0 "
                         "13=ROW7 14=ROW6 15=ROW5 16=ROW4 17=ROW3 18=ROW2 19=ROW1 20=ROW0" },
};

static int vic20_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int vic20_kind_of(const char *ref) {
    if (ref[0] == 'U') return PART_IC;
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    if (!strncmp(ref, "FB", 2) || ref[0] == 'L' || ref[0] == 'F' || !strncmp(ref, "JP", 2)) return PART_LINK;
    if ((ref[0] == 'P' || !strncmp(ref, "CN", 2)) && ref[1] >= '0' && ref[1] <= '9') return PART_CONNECTOR;
    return PART_OTHER;
}

/* ------------------------------------------------------------------------ */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------ */

bool vic20_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n) {
    vic20_t *z = VIC20(b);
    if (!strcmp(ref, "UD7")) {
        if (n != 4096) return false;
        memcpy(z->chars, data, n);
        z->rom_mask |= 4;
        return true;
    }
    bool basic = !strcmp(ref, "UE11");
    if (!basic && strcmp(ref, "UE12")) return false;
    /* A dump of a KERNAL may be one byte short: the last byte, the high byte of the IRQ vector, is $FF. */
    if (n != 8192 && !(n == 8191 && !basic)) return false;
    uint8_t *rom = basic ? z->basic : z->kernal;
    memset(rom, 0xFF, 8192);
    memcpy(rom, data, n);
    z->rom_mask |= basic ? 1 : 2;
    return true;
}

static const char *vic20_rom_ref(const char *path) {
    static const struct { const char *key, *ref; } map[] = {
        { "901486-01", "UE11" }, { "901486-04", "UE12" }, { "901486-06", "UE12" }, { "901486-07", "UE12" },
        { "901460-03", "UD7" }, { "901460-02", "UD7" }, { "basic", "UE11" }, { "kernal", "UE12" }, { "characters", "UD7" },
    };
    const char *name = board_basename(path);
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        size_t n = strlen(map[i].key);
        for (const char *s = name; *s; s++)
            if (!strncasecmp(s, map[i].key, n)) return map[i].ref;
    }
    return NULL;
}

static bool vic20_fit_rom(board_t *b, const char *path, char *msg, int msglen) {
    const char *ref = vic20_rom_ref(path);
    if (!ref) { snprintf(msg, (size_t)msglen, "%s: don't know which ROM this is", path); return false; }
    uint8_t buf[8193];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(buf, 1, sizeof(buf), f) : 0;
    if (f) fclose(f);
    if (!vic20_set_rom(b, ref, buf, n)) {
        snprintf(msg, (size_t)msglen, "%s: %zu bytes don't fit socket %s", path, n, ref);
        return false;
    }
    snprintf(msg, (size_t)msglen, "%s fitted in %s", board_basename(path), ref);
    return true;
}

/* At start: the ROMs found in roms/vic20, roms or the current folder. */
static void vic20_find_roms(board_t *b) {
    static const char *const dirs[] = { "roms/vic20", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir))) {
            size_t l = strlen(e->d_name);
            if (l < 5 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom"))) continue;
            char path[1024], msg[256];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], e->d_name);
            const char *ref = vic20_rom_ref(path);
            if (!ref) continue;
            unsigned bit = !strcmp(ref, "UE11") ? 1 : !strcmp(ref, "UE12") ? 2 : 4;
            if (VIC20(b)->rom_mask & bit) continue;     /* an earlier folder had it */
            vic20_fit_rom(b, path, msg, sizeof(msg));
        }
        closedir(dir);
    }
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static int vic20_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

static void vic20_keys_register(vic20_t *z);

static void vic20_create(board_t *b) {
    vic20_t *z = VIC20(b);
    sim_t *s = &b->sim;
    z->v = b->m->variant;
    z->p_cpu = sim_find_part(s, "UE10");
    z->p_via1 = sim_find_part(s, "UAB3");
    z->p_via2 = sim_find_part(s, "UAB1");
    z->p_vic = sim_find_part(s, "UB7");
    z->p_xtal = sim_find_part(s, "Y1");
    memset(z->basic, 0xFF, sizeof(z->basic));
    memset(z->kernal, 0xFF, sizeof(z->kernal));
    memset(z->chars, 0, sizeof(z->chars));
    z->rom_mask = 0;
    int idx;
    if ((idx = sim_find_part(s, "UE11")) >= 0) ((rom_state_t *)s->parts[idx].state)->data = z->basic;
    if ((idx = sim_find_part(s, "UE12")) >= 0) ((rom_state_t *)s->parts[idx].state)->data = z->kernal;
    if ((idx = sim_find_part(s, "UD7")) >= 0) ((rom_state_t *)s->parts[idx].state)->data = z->chars;
    vic20_find_roms(b);
    if (z->p_vic >= 0) vic_set_screen(&s->parts[z->p_vic], z->raster);
    memset(z->raster, 0, sizeof(z->raster));
    for (int i = 0; i < 8; i++) {
        z->pin_row[i] = vic20_pin_of(b, "P10", 20 - i);     /* ROW0..7 = VIA 2 port A */
        z->pin_col[i] = vic20_pin_of(b, "P10", 12 - i);     /* COL0..7 = port B */
    }
    z->pin_restore = vic20_pin_of(b, "P10", 3);
    z->pin_restore_gnd = vic20_pin_of(b, "P10", 1);
    b->pin_reset = vic20_pin_of(b, "UE10", 40);             /* 6502 /RES to VSS, as an external reset switch would */
    b->pin_reset_gnd = vic20_pin_of(b, "UE10", 21);
    vic20_keys_register(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    z->restore = false;
}

static void vic20_power_on(board_t *b) {
    vic20_t *z = VIC20(b);
    b->sim.clock_level = 0;
    z->step_in_cycle = 0;
    memset(z->raster, 0, sizeof(z->raster));
}

static void vic20_joins(board_t *b) {
    sim_t *s = &b->sim;
    vic20_t *z = VIC20(b);
    /* A closed key joins its row to its column (unless a pin is open). */
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++) {
            if (!((z->key_matrix[row] >> col) & 1)) continue;
            int pr = z->pin_row[row], pc = z->pin_col[col];
            if (pr < 0 || pc < 0 || s->pins[pr].fault == PF_OPEN || s->pins[pc].fault == PF_OPEN) continue;
            sim_join(s, s->pins[pr].net, s->pins[pc].net);
        }
    if (z->restore && z->pin_restore >= 0 && z->pin_restore_gnd >= 0 &&
        s->pins[z->pin_restore].fault != PF_OPEN && s->pins[z->pin_restore_gnd].fault != PF_OPEN)
        sim_join(s, s->pins[z->pin_restore].net, s->pins[z->pin_restore_gnd].net);
}

/* One half period of the crystal. */
static void vic20_step(board_t *b) {
    vic20_t *z = VIC20(b);
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, z->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void vic20_run(board_t *b, int cycles) {
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        for (int k = 0; k < VIC20_STEPS_PER_CYCLE; k++) vic20_step(b);
    }
}

uint32_t vic20_cpu_cycles(board_t *b) { return pet_cpu_cycles_of(&b->sim.parts[VIC20(b)->p_cpu]); }

/* RAM: the table of the variant; the color RAM is a 2114 at $9400. */
static uint8_t *vic20_cells(board_t *b, const char *ref) {
    int i = ref ? sim_find_part(&b->sim, ref) : -1;
    return i < 0 ? NULL : (uint8_t *)b->sim.parts[i].state;     /* both RAM states start with their cells */
}

/* The 2114 model keeps its bit 0 on pin 14 (I/O 4): the cells hold a nibble in the reverse order of the pins. */
static uint8_t rev4(uint8_t v) { return (uint8_t)((v & 1) << 3 | (v & 2) << 1 | (v & 4) >> 1 | (v & 8) >> 3); }

void vic20_ram_poke(board_t *b, uint16_t addr, uint8_t v) {
    vic20_t *z = VIC20(b);
    if (addr >= 0x9400 && addr < 0x9800) {
        uint8_t *c = vic20_cells(b, z->v->color);
        if (c) c[addr & 0x3FF] = rev4(v & 15);
        return;
    }
    for (int i = 0; i < z->v->nram; i++) {
        const vic20_ram_t *r = &z->v->ram[i];
        if (addr < r->base || addr >= r->base + r->len) continue;
        int off = addr - r->base;
        if (r->hi) {
            uint8_t *lo = vic20_cells(b, r->lo), *hi = vic20_cells(b, r->hi);
            if (lo) lo[off] = rev4(v & 15);
            if (hi) hi[off] = rev4(v >> 4);
        } else {
            uint8_t *c = vic20_cells(b, r->lo);
            if (c) c[off] = v;
        }
        return;
    }
}

uint8_t vic20_ram_peek(board_t *b, uint16_t addr) {
    vic20_t *z = VIC20(b);
    if (addr >= 0x9400 && addr < 0x9800) {
        uint8_t *c = vic20_cells(b, z->v->color);
        return c ? rev4(c[addr & 0x3FF] & 15) : 0;
    }
    for (int i = 0; i < z->v->nram; i++) {
        const vic20_ram_t *r = &z->v->ram[i];
        if (addr < r->base || addr >= r->base + r->len) continue;
        int off = addr - r->base;
        if (r->hi) {
            uint8_t *lo = vic20_cells(b, r->lo), *hi = vic20_cells(b, r->hi);
            return (uint8_t)((lo ? rev4(lo[off] & 15) : 0) | (hi ? rev4(hi[off] & 15) << 4 : 0));
        }
        uint8_t *c = vic20_cells(b, r->lo);
        return c ? c[off] : 0;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* The keyboard                                                              */
/* ------------------------------------------------------------------------ */

/* The keys of the matrix, as the KERNAL's decode table has them ($EC5E): per column
 * (VIA 2 port B), the keys of the rows 0-7 (port A). A single character is the key's
 * own cap. */
#define VK_RETURN 0x0D
#define VK_DEL    0x14
#define VK_RIGHT  0x1D
#define VK_DOWN   0x11
#define VK_HOME   0x13
#define VK_STOP   0x03
#define VK_RSHIFT 0x16
#define VK_CBM    0x02
#define VK_CTRL   0x04
#define VK_F1     0x85
static const char *const vic20_keys[8][8] = {
    { "1", "3", "5", "7", "9", "+", "POUND", "DEL" },
    { "LARROW", "W", "R", "Y", "I", "P", "*", "RETURN" },
    { "CTRL", "A", "D", "G", "J", "L", ";", "RIGHT" },
    { "STOP", "LSHIFT", "X", "V", "N", ",", "/", "DOWN" },
    { "SPACE", "Z", "C", "B", "M", ".", "RSHIFT", "F1" },
    { "CBM", "S", "F", "H", "K", ":", "=", "F3" },
    { "Q", "E", "T", "U", "O", "@", "UPARROW", "F5" },
    { "2", "4", "6", "8", "0", "-", "HOME", "F7" },
};

const char *vic20_key_legend(int col, int row) { return vic20_keys[col & 7][row & 7]; }

static void vic20_keys_register(vic20_t *z) {
    kbd_init(&z->kbd, 2);
    for (int col = 0; col < 8; col++)
        for (int row = 0; row < 8; row++) {
            const char *n = vic20_keys[col][row];
            if (!strcmp(n, "LSHIFT")) { kbd_register_modifier(&z->kbd, 0, col, row); continue; }
            int key = 0, shifted = 0;
            if (n[1] == 0) key = (unsigned char)n[0];
            else if (!strcmp(n, "RETURN")) key = VK_RETURN;
            else if (!strcmp(n, "DEL")) { key = VK_DEL; }
            else if (!strcmp(n, "RIGHT")) { key = VK_RIGHT; shifted = 0x9D; }
            else if (!strcmp(n, "DOWN")) { key = VK_DOWN; shifted = 0x91; }
            else if (!strcmp(n, "HOME")) { key = VK_HOME; shifted = 0x93; }
            else if (!strcmp(n, "STOP")) key = VK_STOP;
            else if (!strcmp(n, "RSHIFT")) key = VK_RSHIFT;
            else if (!strcmp(n, "CBM")) key = VK_CBM;
            else if (!strcmp(n, "CTRL")) key = VK_CTRL;
            else if (!strcmp(n, "SPACE")) key = ' ';
            else if (!strcmp(n, "POUND")) key = 0x5C;
            else if (!strcmp(n, "LARROW")) key = 0x5F;
            else if (!strcmp(n, "UPARROW")) key = 0x5E;
            else if (n[0] == 'F' && n[1] >= '1' && n[1] <= '7' && !n[2]) {     /* F1 F3 F5 F7; shifted F2 F4 F6 F8 */
                key = VK_F1 + (n[1] - '1') / 2;
                shifted = 0x89 + (n[1] - '1') / 2;
            }
            if (!key) continue;
            kbd_register_key(&z->kbd, key, col, row, 0);
            if (key >= 'A' && key <= 'Z') kbd_register_key(&z->kbd, key + 32, col, row, 1);     /* shifted: lower case */
            if (shifted) kbd_register_key(&z->kbd, shifted, col, row, 1);
            if (n[1] == 0) {        /* shifted legends of the VIC-20 */
                static const char *const from = "123456789,./:;", *const to = "!\"#$%&'()<>?[]";
                const char *q = strchr(from, n[0]);
                if (q) kbd_register_key(&z->kbd, (unsigned char)to[q - from], col, row, 1);
            }
        }
}

static void vic20_sync_keys(board_t *b) {
    vic20_t *z = VIC20(b);
    bool changed = false;
    for (int row = 0; row < 8; row++) {
        uint16_t m = z->kbd.scanout_column_masks[row];
        if (m != z->key_matrix[row]) { z->key_matrix[row] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

#define VK_RESTORE 0x7F
static void vic20_key_down(board_t *b, int key) {
    if (key == VK_RESTORE) { VIC20(b)->restore = true; board_rebuild(b); return; }
    kbd_key_down(&VIC20(b)->kbd, key);
    vic20_sync_keys(b);
}
static void vic20_key_up(board_t *b, int key) {
    if (key == VK_RESTORE) { VIC20(b)->restore = false; board_rebuild(b); return; }
    kbd_key_up(&VIC20(b)->kbd, key);
    vic20_sync_keys(b);
}
static void vic20_frame_update(board_t *b, uint32_t us) { kbd_update(&VIC20(b)->kbd, us); vic20_sync_keys(b); }

static int vic20_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return VK_RETURN;
    case SAPP_KEYCODE_BACKSPACE: return VK_DEL;
    case SAPP_KEYCODE_RIGHT: return VK_RIGHT;
    case SAPP_KEYCODE_DOWN: return VK_DOWN;
    case SAPP_KEYCODE_HOME: return VK_HOME;
    case SAPP_KEYCODE_ESCAPE: return VK_STOP;
    case SAPP_KEYCODE_RIGHT_SHIFT: return VK_RSHIFT;
    case SAPP_KEYCODE_PAGE_UP: return VK_RESTORE;
    case SAPP_KEYCODE_F1: return VK_F1;
    case SAPP_KEYCODE_F3: return VK_F1 + 1;
    case SAPP_KEYCODE_F5: return VK_F1 + 2;
    case SAPP_KEYCODE_F7: return VK_F1 + 3;
    case SAPP_KEYCODE_F2: return 0x89;
    case SAPP_KEYCODE_F4: return 0x8A;
    case SAPP_KEYCODE_F6: return 0x8B;
    case SAPP_KEYCODE_F8: return 0x8C;
    case SAPP_KEYCODE_LEFT: return 0x9D;
    case SAPP_KEYCODE_UP: return 0x91;
    case SAPP_KEYCODE_DELETE: return 0x93;      /* shifted HOME: CLR */
    default: return 0;
    }
}

/* Like the C64, the VIC-20 types capitals without shift. */
static int vic20_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

/* What a monitor would show: the raster of the 6560, every dot twice as wide. */
static void vic20_monitor(board_t *b, uint32_t *out) {
    const vic20_t *z = VIC20(b);
    for (int y = 0; y < 232; y++)
        for (int x = 0; x < 224; x++) out[y * VIC20_SCREEN_W + 2 * x] = out[y * VIC20_SCREEN_W + 2 * x + 1] = z->raster[y * 224 + x];
}

/* ------------------------------------------------------------------------ */
/* Files                                                                     */
/* ------------------------------------------------------------------------ */

static const file_kind_t vic20_rom_kind = { "ROM image", "Fit in its socket", false, ROLE_ROM };
static const file_kind_t vic20_prg = { "program", "Load and RUN", true, -1 };

static const file_kind_t *vic20_file_kind(const char *path, char *info, int len, bool *bad) {
    const file_kind_t *state = board_state_file_kind(path, info, len, bad);
    if (state) return state;
    size_t pl = strlen(path);
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        const char *ref = vic20_rom_ref(path);
        if (!ref) return NULL;
        snprintf(info, (size_t)len, "goes in socket %s", ref);
        return &vic20_rom_kind;
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
    else if (addr == 0x0401 || addr == 0x1201) snprintf(info, (size_t)len, "BASIC program for a VIC-20 with more RAM ($%04X)", addr);
    else snprintf(info, (size_t)len, "machine code at $%04X", addr);
    return &vic20_prg;
}

static bool vic20_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    size_t pl = strlen(path);
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        if (!vic20_fit_rom(b, path, msg, msglen)) return false;
        board_config_changed(b);
        board_power_on(b);          /* the machine starts over with the new ROM */
        return true;
    }
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(msg, (size_t)msglen, "can't open %s", path); return false; }
    uint8_t *buf = malloc(65536 + 2);
    size_t n = buf ? fread(buf, 1, 65536 + 2, f) : 0;
    fclose(f);
    if (n < 3) { free(buf); snprintf(msg, (size_t)msglen, "%s is too short", path); return false; }
    unsigned start = buf[0] | buf[1] << 8, end = start + (unsigned)(n - 2);
    bool in_ram = (start >= 0x0200 && end <= 0x0400) || (start >= 0x1000 && end <= 0x2000);
    if (!in_ram) {
        free(buf);
        snprintf(msg, (size_t)msglen, "$%04X-$%04X doesn't fit the VIC-20's RAM ($0200-$03FF, $1000-$1FFF: no expansion)", start, end);
        return false;
    }
    for (size_t i = 2; i < n; i++) vic20_ram_poke(b, (uint16_t)(start + i - 2), buf[i]);
    free(buf);
    bool basic = start == 0x1001;
    if (basic) {    /* end of the program = start of the variables, arrays, end of arrays ($2D-$32) */
        for (int p = 0x2D; p <= 0x31; p += 2) {
            vic20_ram_poke(b, (uint16_t)p, end & 0xFF);
            vic20_ram_poke(b, (uint16_t)(p + 1), end >> 8);
        }
    }
    if (run) {      /* type RUN (or SYS) <return> through the keyboard buffer ($0277, its count at $C6) */
        char cmd[16];
        snprintf(cmd, sizeof(cmd), basic ? "RUN\r" : "SYS%u\r", start);
        int len = (int)strlen(cmd);
        for (int i = 0; i < len; i++) vic20_ram_poke(b, (uint16_t)(0x0277 + i), (uint8_t)cmd[i]);
        vic20_ram_poke(b, 0xC6, (uint8_t)len);
    }
    snprintf(msg, (size_t)msglen, "loaded %s at $%04X-$%04X", path, start, end - 1);
    return true;
}

static void vic20_destroy(board_t *b) { (void)b; }

static const char *vic20_cpu_status(board_t *b) {
    const vic20_t *z = VIC20(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!(z->rom_mask & 2)) return "no KERNAL ROM (put the VIC-20 ROMs in roms/vic20)";
    if (!(z->rom_mask & 1)) return "no BASIC ROM (put the VIC-20 ROMs in roms/vic20)";
    if (!(z->rom_mask & 4)) return "no character ROM (put the VIC-20 ROMs in roms/vic20)";
    return "running";
}

static void vic20_cpu_regs(board_t *b, char *buf, int len) {
    pet_cpu_regs_of(&b->sim.parts[VIC20(b)->p_cpu], buf, len);
}

static void vic20_print_text(board_t *b) {
    /* The screen as the video matrix holds it (screen codes to ASCII). */
    vic20_t *z = VIC20(b);
    const part_t *vic = &b->sim.parts[z->p_vic];
    int cols = vic_reg_of(vic, 2) & 0x7F, rows = (vic_reg_of(vic, 3) >> 1) & 0x3F;
    unsigned base = (unsigned)(((vic_reg_of(vic, 5) & 0xF0) << 6) | ((vic_reg_of(vic, 2) & 0x80) << 2)) & 0x1FFF;
    if (cols > 40) cols = 40;
    for (int y = 0; y < rows; y++) {
        char line[48];
        for (int x = 0; x < cols; x++) {
            int c = vic20_ram_peek(b, (uint16_t)(base + y * cols + x)) & 0x7F;
            line[x] = c == 0 ? '@' : c < 27 ? (char)('A' + c - 1) : c >= 32 && c < 64 ? (char)c : '.';
        }
        line[cols] = 0;
        printf("%s\n", line);
    }
}

#define VIC20_MACHINE \
    .size = sizeof(vic20_t), \
    .sim = { .rail_of = vic20_rail_of, .kind_of = vic20_kind_of, .cycles_per_us = 1.0 }, \
    .cpu_hz = VIC20_FREQ, \
    .cycles_per_frame = VIC20_CYCLES_PER_FRAME, \
    .steps_per_cycle = VIC20_STEPS_PER_CYCLE, \
    .screen_w = VIC20_SCREEN_W, .screen_h = VIC20_SCREEN_H, \
    .ready_cycles = 3 * VIC20_FREQ, \
    .conns = vic20_conn_defs, .nconns = sizeof(vic20_conn_defs) / sizeof(vic20_conn_defs[0]), \
    .link_resistors = "", \
    .crystals = "Y1", \
    .diodes = "CR", .jumpers = "", \
    .supplies = "9VACin 9VACout +5VP", \
    .keys_help = "Keys: type; Esc = RUN/STOP, Home = HOME, Del = CLR, Page Up = RESTORE, arrow keys, F1-F8", \
    .reset_help = "Hold to press RESET: shorts the 6502 /RES (UE10 pin 40) to GND", \
    .create = vic20_create, \
    .destroy = vic20_destroy, \
    .power_on = vic20_power_on, \
    .run = vic20_run, \
    .joins = vic20_joins, \
    .key_down = vic20_key_down, \
    .key_up = vic20_key_up, \
    .frame_update = vic20_frame_update, \
    .host_key = vic20_host_key, \
    .host_char = vic20_host_char, \
    .monitor = vic20_monitor, \
    .files_title = "Open a program or save state", \
    .file_kind = vic20_file_kind, \
    .load = vic20_load, \
    .cpu_status = vic20_cpu_status, \
    .cpu_regs = vic20_cpu_regs, \
    .print_text = vic20_print_text

/* RAM of the 250403: 1K at $0000 (two 2114s), 4K at $1000 (two 6116s). */
static const vic20_ram_t vic20b_ram[] = {
    { "UD2", "UE2", 0x0000, 1024 },
    { "U15", NULL, 0x1000, 2048 },
    { "U14", NULL, 0x1800, 2048 },
};
static const vic20_variant_t vic20b_variant = { vic20b_ram, 3, "UE1" };

const machine_t machine_vic20b = {
    VIC20_MACHINE,
    .id = "vic20b",
    .issue = "250403",
    .variant = &vic20b_variant,
    .name = "Commodore VIC-20",
    .board_name = "VIC-20 ASSY 250403 (cost reduced)",
    .data = &bd_vic20b,
    .parts = vic20b_part_defs, .nparts = sizeof(vic20b_part_defs) / sizeof(vic20b_part_defs[0]),
};

/* RAM of the early board (324001): five pairs of 2114s, $0000-$03FF and $1000-$1FFF. */
static const vic20_ram_t vic20a_ram[] = {
    { "UD2", "UE2", 0x0000, 1024 },
    { "UD3", "UE3", 0x1000, 1024 },
    { "UD4", "UE4", 0x1400, 1024 },
    { "UD5", "UE5", 0x1800, 1024 },
    { "UD6", "UE6", 0x1C00, 1024 },
};
static const vic20_variant_t vic20a_variant = { vic20a_ram, 5, "UE1" };

const machine_t machine_vic20a = {
    VIC20_MACHINE,
    .id = "vic20a",
    .issue = "324001",
    .variant = &vic20a_variant,
    .name = "Commodore VIC-20",
    .board_name = "VIC-20 early board (324001, MDK 25HB)",
    .data = &bd_vic20a,
    .parts = vic20a_part_defs, .nparts = sizeof(vic20a_part_defs) / sizeof(vic20a_part_defs[0]),
};
