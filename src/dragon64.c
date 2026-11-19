/* dragon64.c - the Dragon 64 (issue 6 board; the Tano Dragon was an NTSC Dragon 64).
 *
 * As the Color Computer 2 the board makes its own timing: the 6883 SAM divides the 14.218 MHz crystal XL3 into the 6809E's E and
 * Q clocks (16 periods a cycle) and the RAM's /RAS, /CAS and multiplexed address. The 6847 draws the picture from the RAM's
 * bytes, which a 74LS273 holds for it, and its CLK is the SAM's VCLK, gated by the counters and gates of the PAL timing (they
 * stretch the 262 line field of the 6847 to the 312 lines of 50 Hz). Nothing in this file drives a bus cycle: a step is half a
 * period of the crystal, 32 steps are a CPU cycle. The keyboard matrix is wired through the nets. */
#include "dragon64.h"
#include "coco_chips.h"
#include "sokol/sokol_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const part_def_t dg_part_defs[] = {
#include "dragon64_parts.inc"
};

static const conn_def_t dg_conn_defs[] = {
    { "PL1", "KEYBOARD", "1=PA0 2=PA1 3=GND 4=PA2 5=PA3 6=PA4 7=PA5 8=PA6 9=PB0 10=PB1 11=PB2 12=PB3 13=PB4 14=PB5 15=PB6 16=PB7" },
};

static int dg_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

/* The analog chips (the sound and joystick multiplexer and comparators, the PAL colour encoder, the colour phase monostable)
 * are not simulated; the links LK1 (Z7), LK3 (/MS to +5V) and LK4 (the horizontal sync through the latch) are as the board is set
 * up, pins 1 and 2 joined; the 22 ohm resistors in the SAM's RAM signals are wires (series resistors of the board). */
static int dg_kind_of(const char *ref) {
    static const char *const links[] = { "LK1", "LK3", "LK4" }, *const other[] = { "IC2", "IC3", "IC4", "IC6", "IC17", "IC28", "IC29", "IC31" };
    for (size_t i = 0; i < sizeof(links) / sizeof(links[0]); i++)
        if (!strcmp(ref, links[i])) return PART_LINK;
    for (size_t i = 0; i < sizeof(other) / sizeof(other[0]); i++)
        if (!strcmp(ref, other[i])) return PART_OTHER;
    if (!strncmp(ref, "IC", 2) || !strcmp(ref, "XL3")) return PART_IC;
    if (!strncmp(ref, "PL", 2)) return PART_CONNECTOR;
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

/* roms/dragon64: d64_1.rom (IC34, /ROM0) and d64_2.rom (IC37, /ROM1), 16K each. */
static void dg_find_roms(board_t *b) {
    dragon64_t *z = DG(b);
    static const char *const dirs[] = { "roms/dragon64", "roms", "." };
    static const char *const names[2] = { "d64_1.rom", "d64_2.rom" };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++)
        for (int i = 0; i < 2; i++) {
            char path[512];
            if (z->rom_mask & (1u << i)) continue;
            snprintf(path, sizeof(path), "%s/%s", dirs[d], names[i]);
            if (read_file(path, z->rom[i], 16384)) z->rom_mask |= 1u << i;
        }
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static void dg_keys_register(dragon64_t *z);

static int dg_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

static void dg_create(board_t *b) {
    dragon64_t *z = DG(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "IC38");
    z->p_sam = sim_find_part(s, "IC39");
    z->p_vdg = sim_find_part(s, "IC10");
    z->p_pia1 = sim_find_part(s, "IC8");
    z->p_pia0 = sim_find_part(s, "IC5");
    z->p_rom0 = sim_find_part(s, "IC34");
    z->p_rom1 = sim_find_part(s, "IC37");
    z->p_xtal = sim_find_part(s, "XL3");
    memset(z->rom, 0xFF, sizeof(z->rom));
    z->rom_mask = 0;
    if (z->p_rom0 >= 0) ((rom_state_t *)s->parts[z->p_rom0].state)->data = z->rom[0];
    if (z->p_rom1 >= 0) ((rom_state_t *)s->parts[z->p_rom1].state)->data = z->rom[1];
    if (z->p_vdg >= 0 && z->p_sam >= 0) coco_vdg_sync(s, z->p_vdg, z->p_sam);
    dg_find_roms(b);
    for (int i = 0; i < 7; i++) z->pin_row[i] = dg_pin_of(b, "IC5", 2 + i);
    for (int i = 0; i < 8; i++) z->pin_col[i] = dg_pin_of(b, "IC5", 10 + i);
    z->pin_reset_cap = dg_pin_of(b, "C55", 1);
    b->pin_reset = dg_pin_of(b, "IC38", 37);        /* the RESET switch: /RESET to GND */
    b->pin_reset_gnd = dg_pin_of(b, "IC38", 1);
    dg_keys_register(z);
}

static void dg_power_on(board_t *b) {
    dragon64_t *z = DG(b);
    sim_t *s = &b->sim;
    s->clock_level = 0;
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    dg_keys_register(z);
    memset(b->screen, 0, (size_t)320 * 240 * sizeof(uint32_t));
    /* The reset capacitor (C55) holds /RESET low for a while after the power comes on. */
    z->reset_cycles = (uint32_t)s->cycle + 3000;
    if (z->pin_reset_cap >= 0) sim_drive(s, z->pin_reset_cap, DRV_LO);
}

/* A closed key joins its row (a PA pin of PIA 0) to its column (a PB pin). */
static void dg_joins(board_t *b) {
    dragon64_t *z = DG(b);
    sim_t *s = &b->sim;
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

/* The matrix: the rows are PIA 0's PA0..PA6, the columns PB0..PB7 (found by pressing every crossing in the simulation). */
static const char *const dg_keys[7][8] = {
    { "0", "1", "2", "3", "4", "5", "6", "7" },
    { "8", "9", ":", ";", ",", "-", ".", "/" },
    { "@", "A", "B", "C", "D", "E", "F", "G" },
    { "H", "I", "J", "K", "L", "M", "N", "O" },
    { "P", "Q", "R", "S", "T", "U", "V", "W" },
    { "X", "Y", "Z", "UP", "DOWN", "LEFT", "RIGHT", "SPACE" },
    { "ENTER", "CLEAR", "BREAK", "", "", "", "", "SHIFT" },
};
enum { VK_ENTER = 13, VK_CLEAR = 0x81, VK_BREAK, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_SHIFT };

static void dg_keys_register(dragon64_t *z) {
    kbd_init(&z->kbd, 2);
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 8; col++) {
            const char *n = dg_keys[row][col];
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
            if (n[1] == 0) {                /* the shifted legends */
                static const char *const from = "1234567:;,-./", *const to = "!\"#$%&'*+<=>?";
                const char *q = strchr(from, n[0]);
                if (q && n[0] != '\0') kbd_register_key(&z->kbd, (unsigned char)to[q - from], col, row, 1);
            }
            if (n[0] == '8') kbd_register_key(&z->kbd, '(', col, row, 1);
            if (n[0] == '9') kbd_register_key(&z->kbd, ')', col, row, 1);
        }
}

static void dg_sync_keys(board_t *b) {
    dragon64_t *z = DG(b);
    bool changed = false;
    for (int row = 0; row < 7; row++) {
        uint8_t m = (uint8_t)z->kbd.scanout_column_masks[row];
        if (m != z->key_matrix[row]) { z->key_matrix[row] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

static void dg_key_down(board_t *b, int key) { kbd_key_down(&DG(b)->kbd, key); dg_sync_keys(b); }
static void dg_key_up(board_t *b, int key) { kbd_key_up(&DG(b)->kbd, key); dg_sync_keys(b); }
static void dg_frame_update(board_t *b, uint32_t us) { kbd_update(&DG(b)->kbd, us); dg_sync_keys(b); }

static int dg_host_key(int keycode) {
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

/* The Dragon types capitals without Shift (the VDG's lower case is the inverse of the capitals). */
static int dg_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

/* ------------------------------------------------------------------------ */
/* Running                                                                   */
/* ------------------------------------------------------------------------ */

static void dg_step(board_t *b) {
    dragon64_t *z = DG(b);
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, z->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void dg_cycle_start(board_t *b) {
    dragon64_t *z = DG(b);
    sim_t *s = &b->sim;
    s->cycle++;
    if (z->reset_cycles && s->cycle >= z->reset_cycles) {
        z->reset_cycles = 0;
        if (z->pin_reset_cap >= 0) sim_drive(s, z->pin_reset_cap, DRV_PULLUP);       /* R17 and D11 pull /RESET up */
    }
}

void dragon64_run_steps(board_t *b, int steps) {
    static uint32_t phase;
    for (int i = 0; i < steps; i++) {
        if (phase == 0) dg_cycle_start(b);
        dg_step(b);
        if (++phase == CC_STEPS_PER_CYCLE) phase = 0;
    }
}

static void dg_run(board_t *b, int cycles) {
    for (int i = 0; i < cycles; i++) {
        dg_cycle_start(b);
        for (int k = 0; k < CC_STEPS_PER_CYCLE; k++) dg_step(b);
    }
}

/* The picture is the 6847's, as it drew it: nothing to decode. */
static void dg_monitor(board_t *b, uint32_t *out) {
    const uint32_t *fb = coco_vdg_screen(&b->sim, DG(b)->p_vdg);
    memcpy(out, fb, (size_t)COCO_FB_W * COCO_FB_H * sizeof(uint32_t));
}

/* ------------------------------------------------------------------------ */
/* RAM                                                                       */
/* ------------------------------------------------------------------------ */

/* The SAM puts A0..A7 on the RAM's address pins for the row and A8..A15 for the column. The board wires its Z1 and Z2 to the
 * pins that are A2 and A1 of the 4164 (the chip does not care, a row or column is just a name), and the model numbers the cells
 * by the chip's own address bits. */
static int dg_perm(int v) { return (v & ~6) | ((v & 2) << 1) | ((v & 4) >> 1); }
static int dg_cell(uint16_t a) { return dg_perm(a & 0xFF) << 8 | dg_perm(a >> 8); }

static const char *const dg_rams[8] = { "IC15", "IC16", "IC21", "IC22", "IC26", "IC27", "IC32", "IC33" };

/* The bit of the data bus a RAM chip holds: the /Dn net on its D pin (pin 2). */
static int dg_ram_bit(board_t *b, const char *ref) {
    int p = sim_find_part(&b->sim, ref);
    if (p < 0) return -1;
    int pin = part_pin(&b->sim.parts[p], 2);
    int net = pin < 0 ? -1 : b->sim.pins[pin].net;
    const char *name = net < 0 ? "" : b->sim.nets[net].name;
    const char *d = strrchr(name, 'D');
    return d && d[1] >= '0' && d[1] <= '7' && !d[2] ? d[1] - '0' : -1;
}

uint8_t dragon64_ram_peek(board_t *b, uint16_t addr) {
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) {
        int bit = dg_ram_bit(b, dg_rams[i]);
        int p = sim_find_part(&b->sim, dg_rams[i]);
        if (bit >= 0 && p >= 0) v |= (uint8_t)(((dram_state_t *)b->sim.parts[p].state)->cells[dg_cell(addr)] << bit);
    }
    return v;
}

static int dg_ram_cell(board_t *b, int part, uint16_t addr) {
    const char *ref = b->sim.parts[part].ref;
    for (int i = 0; i < 8; i++)
        if (!strcmp(ref, dg_rams[i])) return dg_cell(addr);
    (void)b;
    return -1;
}

/* The screen as the text of the VDG's character set: the 32x16 screen at $0400. */
static void dg_print_text(board_t *b) {
    for (int y = 0; y < 16; y++) {
        char line[33];
        for (int x = 0; x < 32; x++) {
            int c = dragon64_ram_peek(b, (uint16_t)(0x400 + y * 32 + x)) & 0x3F;
            line[x] = (char)(c < 0x20 ? c + 0x40 : c);
        }
        line[32] = 0;
        printf("%s\n", line);
    }
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static const file_kind_t *dg_file_kind(const char *path, char *info, int len, bool *bad) {
    return board_state_file_kind(path, info, len, bad);
}
static bool dg_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)run;
    snprintf(msg, (size_t)msglen, "%s: the Dragon 64 takes no files yet (no cassette or cartridge built)", path);
    return false;
}
static void dg_destroy(board_t *b) { (void)b; }
static const char *dg_cpu_status(board_t *b) {
    const dragon64_t *z = DG(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if ((z->rom_mask & 3) != 3) return "no ROMs (put them in roms/dragon64)";
    return "running";
}
static void dg_cpu_regs(board_t *b, char *buf, int len) {
    const m6809_t *c = coco_cpu_core(&b->sim, DG(b)->p_cpu);
    snprintf(buf, (size_t)len, "PC=%04X A=%02X B=%02X X=%04X Y=%04X U=%04X S=%04X DP=%02X CC=%02X",
             c->pc, c->a, c->b, c->x, c->y, c->u, c->s, c->dp, c->cc);
}

const machine_t machine_dragon64 = {
    .id = "dragon64",
    .name = "Dragon 64",
    .board_name = "Dragon 64 issue 6",
    .issue = "Dragon 64",
    .size = sizeof(dragon64_t),
    .data = &bd_dragon64,
    .sim = { .rail_of = dg_rail_of, .kind_of = dg_kind_of, .cycles_per_us = 0.888625 },
    .cpu_hz = CC_FREQ,
    .cycles_per_frame = CC_CYCLES_PER_FRAME,
    .steps_per_cycle = CC_STEPS_PER_CYCLE,
    .screen_w = COCO_FB_W, .screen_h = COCO_FB_H,
    .ready_cycles = 2 * CC_FREQ,
    .parts = dg_part_defs, .nparts = sizeof(dg_part_defs) / sizeof(dg_part_defs[0]),
    .conns = dg_conn_defs, .nconns = sizeof(dg_conn_defs) / sizeof(dg_conn_defs[0]),
    .link_resistors = "R73 R74 R75 R76 R77 R78 R79 R80 R81 R82 R83",
    .crystals = "XL3",
    .diodes = "D", .jumpers = "J JP", .supplies = "",
    .keys_help = "Keys: type (capitals without Shift); Esc = BREAK, Home = CLEAR, Backspace or Left = left arrow, arrow keys",
    .reset_help = "Hold to press RESET: shorts the 6809 /RESET (IC38 pin 37) to GND",
    .create = dg_create,
    .destroy = dg_destroy,
    .power_on = dg_power_on,
    .run = dg_run,
    .joins = dg_joins,
    .key_down = dg_key_down,
    .key_up = dg_key_up,
    .frame_update = dg_frame_update,
    .host_key = dg_host_key,
    .host_char = dg_host_char,
    .monitor = dg_monitor,
    .files_title = "Open a save state",
    .file_kind = dg_file_kind,
    .load = dg_load,
    .cpu_status = dg_cpu_status,
    .cpu_regs = dg_cpu_regs,
    .print_text = dg_print_text,
    .ram_cell = dg_ram_cell,
};
