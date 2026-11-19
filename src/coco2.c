/* coco2.c - the Tandy / Radio Shack Color Computer 2.
 *
 * The board makes its own timing: the SAM divides the 14.31818 MHz crystal X1 into the 6809E's E and Q clocks (16 periods a
 * cycle) and the RAM's /RAS, /CAS and multiplexed address, and the 6847T1 makes the picture while the SAM's counter feeds it the
 * bytes. Nothing in this file drives a bus cycle: a step is half a period of the crystal, 32 steps are a CPU cycle.
 * The keyboard matrix is wired through the nets: a pressed key joins its row (a PA pin of PIA 0) to its column (a PB pin). */
#include "coco2.h"
#include "coco_chips.h"
#include "sokol/sokol_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const part_def_t cc2_part_defs[] = {
#include "coco2_parts.inc"
};

static const conn_def_t cc2_conn_defs[] = {
    { "CN2", "KEYBOARD", "1=PA0 2=PA1 3=PA2 4=PA3 5=PA4 6=PA5 7=PA6 8=NC 9=PB0 10=PB1 11=PB2 12=PB3 13=PB4 14=PB5 15=PB6 16=PB7" },
};

static int cc2_rail_of(const char *name) {
    if (!strcmp(name, "GNDS")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

/* The two analog chips (the sound/joystick multiplexer and the cassette/serial interface) and the RAM sockets of the 16K
 * option are not simulated; the solder jumpers of the ROM are wires as set for the 16K ROM, except J2 (see cc2_joins). */
static int cc2_kind_of(const char *ref) {
    static const char *const links[] = { "J1", "J3", "J4", "J5" };
    for (size_t i = 0; i < sizeof(links) / sizeof(links[0]); i++)
        if (!strcmp(ref, links[i])) return PART_LINK;
    if (!strcmp(ref, "IC6") || !strcmp(ref, "IC7") || !strcmp(ref, "IC10") || !strcmp(ref, "IC11")) return PART_OTHER;
    if (!strncmp(ref, "IC", 2) || !strcmp(ref, "X1")) return PART_IC;
    if (!strncmp(ref, "FB", 2)) return PART_LINK;
    if (!strncmp(ref, "CN", 2) || !strncmp(ref, "JK", 2)) return PART_CONNECTOR;
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    return PART_OTHER;
}

/* ------------------------------------------------------------------------ */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------ */

static bool read_file(const char *path, uint8_t *buf, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(buf, 1, n, f);
    fclose(f);
    return got == n;
}

/* roms/coco2: extbas11.rom (Extended BASIC 1.1, 8K, $8000) and bas12.rom (Color BASIC 1.2, 8K, $A000). */
static void cc2_find_roms(board_t *b) {
    coco2_t *z = CC2(b);
    static const char *const dirs[] = { "roms/coco2", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        char path[512];
        if (!(z->rom_mask & 1)) {
            snprintf(path, sizeof(path), "%s/extbas11.rom", dirs[d]);
            if (read_file(path, z->rom, 8192)) z->rom_mask |= 1;
        }
        if (!(z->rom_mask & 2)) {
            snprintf(path, sizeof(path), "%s/bas12.rom", dirs[d]);
            if (read_file(path, z->rom + 8192, 8192)) z->rom_mask |= 2;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static void cc2_keys_register(coco2_t *z);

static int cc2_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

static void cc2_create(board_t *b) {
    coco2_t *z = CC2(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "IC1");
    z->p_sam = sim_find_part(s, "IC2");
    z->p_vdg = sim_find_part(s, "IC3");
    z->p_pia1 = sim_find_part(s, "IC4");
    z->p_pia0 = sim_find_part(s, "IC5");
    z->p_rom = sim_find_part(s, "IC8");
    z->p_xtal = sim_find_part(s, "X1");
    memset(z->rom, 0xFF, sizeof(z->rom));
    z->rom_mask = 0;
    if (z->p_rom >= 0) ((rom_state_t *)s->parts[z->p_rom].state)->data = z->rom;
    cc2_find_roms(b);
    for (int i = 0; i < 7; i++) z->pin_row[i] = cc2_pin_of(b, "IC5", 2 + i);
    for (int i = 0; i < 8; i++) z->pin_col[i] = cc2_pin_of(b, "IC5", 10 + i);
    z->pin_reset_cap = cc2_pin_of(b, "C8", 1);
    b->pin_reset = cc2_pin_of(b, "IC1", 37);        /* the RESET switch: /RESET to GND */
    b->pin_reset_gnd = cc2_pin_of(b, "IC1", 1);
    cc2_keys_register(z);
}

static void cc2_power_on(board_t *b) {
    coco2_t *z = CC2(b);
    sim_t *s = &b->sim;
    s->clock_level = 0;
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    cc2_keys_register(z);
    memset(b->screen, 0, (size_t)320 * 240 * sizeof(uint32_t));
    /* The reset capacitor (C8) and its 100K pull-up make /RESET low for a while after the power comes on. */
    z->reset_cycles = (uint32_t)s->cycle + 3000;
    if (z->pin_reset_cap >= 0) sim_drive(s, z->pin_reset_cap, DRV_LO);
}

/* J2 of the ROM jumpers is bridged between its pins 2 and 3 (A12 to the ROM's A12 through the 16K/8K selection). */
static void cc2_joins(board_t *b) {
    coco2_t *z = CC2(b);
    sim_t *s = &b->sim;
    int j2b = cc2_pin_of(b, "J2", 2), j2c = cc2_pin_of(b, "J2", 3);
    if (j2b >= 0 && j2c >= 0 && s->pins[j2b].net >= 0 && s->pins[j2c].net >= 0) sim_join(s, s->pins[j2b].net, s->pins[j2c].net);
    /* A closed key joins its row to its column (unless a pin is open). */
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 8; col++) {
            if (!((z->key_matrix[row] >> col) & 1)) continue;
            int pr = z->pin_row[row], pc = z->pin_col[col];
            if (pr < 0 || pc < 0 || s->pins[pr].fault == PF_OPEN || s->pins[pc].fault == PF_OPEN) continue;
            sim_join(s, s->pins[pr].net, s->pins[pc].net);
        }
}

/* ------------------------------------------------------------------------ */
/* Keyboard                                                                  */
/* ------------------------------------------------------------------------ */

/* The matrix: the rows are PIA 0's PA0..PA6, the columns PB0..PB7. */
static const char *const cc2_keys[7][8] = {
    { "@", "A", "B", "C", "D", "E", "F", "G" },
    { "H", "I", "J", "K", "L", "M", "N", "O" },
    { "P", "Q", "R", "S", "T", "U", "V", "W" },
    { "X", "Y", "Z", "UP", "DOWN", "LEFT", "RIGHT", "SPACE" },
    { "0", "1", "2", "3", "4", "5", "6", "7" },
    { "8", "9", ":", ";", ",", "-", ".", "/" },
    { "ENTER", "CLEAR", "BREAK", "", "", "", "", "SHIFT" },
};
enum { VK_ENTER = 13, VK_CLEAR = 0x81, VK_BREAK, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_SHIFT };

static void cc2_keys_register(coco2_t *z) {
    kbd_init(&z->kbd, 2);
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 8; col++) {
            const char *n = cc2_keys[row][col];
            if (!n[0]) continue;
            int key = 0;
            if (n[1] == 0) key = (unsigned char)n[0];
            else if (!strcmp(n, "ENTER")) key = VK_ENTER;
            else if (!strcmp(n, "CLEAR")) key = VK_CLEAR;
            else if (!strcmp(n, "BREAK")) key = VK_BREAK;
            else if (!strcmp(n, "UP")) key = VK_UP;
            else if (!strcmp(n, "DOWN")) key = VK_DOWN;
            else if (!strcmp(n, "LEFT")) key = VK_LEFT;
            else if (!strcmp(n, "RIGHT")) key = VK_RIGHT;
            else if (!strcmp(n, "SPACE")) key = ' ';
            else if (!strcmp(n, "SHIFT")) { kbd_register_modifier(&z->kbd, 0, col, row); key = VK_SHIFT; }
            if (!key) continue;
            kbd_register_key(&z->kbd, key, col, row, 0);
            if (key >= 'A' && key <= 'Z') kbd_register_key(&z->kbd, key + 32, col, row, 1);     /* shifted: lower case */
            if (n[1] == 0) {                /* the shifted legends of the CoCo */
                static const char *const from = "1234567:;,-./", *const to = "!\"#$%&'*+<=>?";
                const char *q = strchr(from, n[0]);
                if (q && n[0] != '\0') kbd_register_key(&z->kbd, (unsigned char)to[q - from], col, row, 1);
            }
            if (n[0] == '8') kbd_register_key(&z->kbd, '(', col, row, 1);
            if (n[0] == '9') kbd_register_key(&z->kbd, ')', col, row, 1);
        }
}

static void cc2_sync_keys(board_t *b) {
    coco2_t *z = CC2(b);
    bool changed = false;
    for (int row = 0; row < 7; row++) {
        uint8_t m = (uint8_t)z->kbd.scanout_column_masks[row];
        if (m != z->key_matrix[row]) { z->key_matrix[row] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

static void cc2_key_down(board_t *b, int key) { kbd_key_down(&CC2(b)->kbd, key); cc2_sync_keys(b); }
static void cc2_key_up(board_t *b, int key) { kbd_key_up(&CC2(b)->kbd, key); cc2_sync_keys(b); }
static void cc2_frame_update(board_t *b, uint32_t us) { kbd_update(&CC2(b)->kbd, us); cc2_sync_keys(b); }

static int cc2_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return VK_ENTER;
    case SAPP_KEYCODE_BACKSPACE: case SAPP_KEYCODE_LEFT: return VK_LEFT;
    case SAPP_KEYCODE_RIGHT: return VK_RIGHT;
    case SAPP_KEYCODE_UP: return VK_UP;
    case SAPP_KEYCODE_DOWN: return VK_DOWN;
    case SAPP_KEYCODE_HOME: return VK_CLEAR;
    case SAPP_KEYCODE_ESCAPE: return VK_BREAK;
    case SAPP_KEYCODE_RIGHT_SHIFT: return VK_SHIFT;
    default: return 0;
    }
}

/* The CoCo types capitals without Shift (the VDG's lower case is the inverse of the capitals). */
static int cc2_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

/* ------------------------------------------------------------------------ */
/* Running                                                                   */
/* ------------------------------------------------------------------------ */

static void cc2_step(board_t *b) {
    coco2_t *z = CC2(b);
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, z->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void cc2_cycle_start(board_t *b) {
    coco2_t *z = CC2(b);
    sim_t *s = &b->sim;
    s->cycle++;
    if (z->reset_cycles && s->cycle >= z->reset_cycles) {
        z->reset_cycles = 0;
        if (z->pin_reset_cap >= 0) sim_drive(s, z->pin_reset_cap, DRV_Z);
    }
}

void coco2_run_steps(board_t *b, int steps) {
    static uint32_t phase;
    for (int i = 0; i < steps; i++) {
        if (phase == 0) cc2_cycle_start(b);
        cc2_step(b);
        if (++phase == CC_STEPS_PER_CYCLE) phase = 0;
    }
}

static void cc2_run(board_t *b, int cycles) {
    for (int i = 0; i < cycles; i++) {
        cc2_cycle_start(b);
        for (int k = 0; k < CC_STEPS_PER_CYCLE; k++) cc2_step(b);
    }
}

/* The picture is the 6847's, as it drew it: nothing to decode. */
static void cc2_monitor(board_t *b, uint32_t *out) {
    const uint32_t *fb = coco_vdg_screen(&b->sim, CC2(b)->p_vdg);
    memcpy(out, fb, (size_t)COCO_FB_W * COCO_FB_H * sizeof(uint32_t));
}

/* ------------------------------------------------------------------------ */
/* RAM                                                                       */
/* ------------------------------------------------------------------------ */

/* The SAM puts A0..A7 on the RAM's address pins for the row and A8..A15 for the column. */
static int cc2_cell(uint16_t a) { return (a & 0xFF) << 8 | (a >> 8); }

uint8_t coco2_ram_peek(board_t *b, uint16_t addr) {
    uint8_t v = 0;
    char ref[8];
    for (int bit = 0; bit < 8; bit++) {
        snprintf(ref, sizeof(ref), "IC%d", 14 + bit);
        int p = sim_find_part(&b->sim, ref);
        if (p >= 0) v |= (uint8_t)(((dram_state_t *)b->sim.parts[p].state)->cells[cc2_cell(addr)] << bit);
    }
    return v;
}

static int cc2_ram_cell(board_t *b, int part, uint16_t addr) {
    const char *ref = b->sim.parts[part].ref;
    if (strncmp(ref, "IC", 2)) return -1;
    int n = atoi(ref + 2);
    return n >= 14 && n <= 21 ? cc2_cell(addr) : -1;
}

/* The screen as the text of the VDG's character set: the 32x16 screen at $0400. */
static void cc2_print_text(board_t *b) {
    for (int y = 0; y < 16; y++) {
        char line[33];
        for (int x = 0; x < 32; x++) {
            int c = coco2_ram_peek(b, (uint16_t)(0x400 + y * 32 + x)) & 0x3F;
            line[x] = (char)(c < 0x20 ? c + 0x40 : c);
        }
        line[32] = 0;
        printf("%s\n", line);
    }
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static const file_kind_t *cc2_file_kind(const char *path, char *info, int len, bool *bad) {
    return board_state_file_kind(path, info, len, bad);
}
static bool cc2_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)run;
    snprintf(msg, (size_t)msglen, "%s: the Color Computer 2 takes no files yet (no cassette or cartridge built)", path);
    return false;
}
static void cc2_destroy(board_t *b) { (void)b; }
static const char *cc2_cpu_status(board_t *b) {
    const coco2_t *z = CC2(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if ((z->rom_mask & 3) != 3) return "no ROMs (put them in roms/coco2)";
    return "running";
}
static void cc2_cpu_regs(board_t *b, char *buf, int len) {
    const m6809_t *c = coco_cpu_core(&b->sim, CC2(b)->p_cpu);
    snprintf(buf, (size_t)len, "PC=%04X A=%02X B=%02X X=%04X Y=%04X U=%04X S=%04X DP=%02X CC=%02X",
             c->pc, c->a, c->b, c->x, c->y, c->u, c->s, c->dp, c->cc);
}

const machine_t machine_coco2 = {
    .id = "coco2",
    .name = "Color Computer 2",
    .board_name = "CoCo 2 26-3134B/3136B",
    .issue = NULL,
    .size = sizeof(coco2_t),
    .data = &bd_coco2,
    .sim = { .rail_of = cc2_rail_of, .kind_of = cc2_kind_of, .cycles_per_us = 0.894886 },
    .cpu_hz = CC_FREQ,
    .cycles_per_frame = CC_CYCLES_PER_FRAME,
    .steps_per_cycle = CC_STEPS_PER_CYCLE,
    .screen_w = COCO_FB_W, .screen_h = COCO_FB_H,
    .ready_cycles = 2 * CC_FREQ,
    .parts = cc2_part_defs, .nparts = sizeof(cc2_part_defs) / sizeof(cc2_part_defs[0]),
    .conns = cc2_conn_defs, .nconns = sizeof(cc2_conn_defs) / sizeof(cc2_conn_defs[0]),
    .link_resistors = "R4 R11 R12 R29 R30 R31",
    .crystals = "X1",
    .diodes = "D", .jumpers = "J JP", .supplies = "",
    .keys_help = "Keys: type (capitals without Shift); Esc = BREAK, Home = CLEAR, Backspace or Left = left arrow, arrow keys",
    .reset_help = "Hold to press RESET: shorts the 6809 /RESET (IC1 pin 37) to GND",
    .create = cc2_create,
    .destroy = cc2_destroy,
    .power_on = cc2_power_on,
    .run = cc2_run,
    .joins = cc2_joins,
    .key_down = cc2_key_down,
    .key_up = cc2_key_up,
    .frame_update = cc2_frame_update,
    .host_key = cc2_host_key,
    .host_char = cc2_host_char,
    .monitor = cc2_monitor,
    .files_title = "Open a save state",
    .file_kind = cc2_file_kind,
    .load = cc2_load,
    .cpu_status = cc2_cpu_status,
    .cpu_regs = cc2_cpu_regs,
    .print_text = cc2_print_text,
    .ram_cell = cc2_ram_cell,
};
