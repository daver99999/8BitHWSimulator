/* mc10.c - the Tandy MC-10.
 *
 * The board is small: the MC6803 runs on a 3.579545 MHz crystal (the MC1372's oscillator, which stands for the video modulator
 * here) and divides it by 4 for E. The 6847 reads the 4K of RAM itself in the time the CPU leaves it, so the CPU's address and
 * data go to the RAM through the 74LS367 and 74LS245 only while the 74LS155 selects the RAM (/MS), and the 6847 lets go of its
 * address pins then. A 74LS74 and a 74LS76 make a window for the CPU's writes and stretch the 6847's clock at the horizontal sync.
 * Nothing in this file drives a bus cycle: a step is half a period of the crystal, 8 steps are a CPU cycle. The keyboard matrix
 * is wired through the nets. */
#include "mc10.h"
#include "coco_chips.h"
#include "mc10_chips.h"
#include "sokol/sokol_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const part_def_t mc_part_defs[] = {
#include "mc10_parts.inc"
};

static const conn_def_t mc_conn_defs[] = {
    { "J4", "KEYBOARD COLUMNS", "1=KB0 2=KB1 4=KB2 5=KB3 6=KB4 7=KB5 8=SHIFT" },
    { "J5", "KEYBOARD ROWS", "1=KR0 2=KR1 3=KR2 4=KR3 5=KR4 6=KR5 7=KR6 8=KR7" },
};

static int mc_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int mc_kind_of(const char *ref) {
    if (!strcmp(ref, "J4") || !strcmp(ref, "J5") || !strcmp(ref, "E.")) return PART_CONNECTOR;
    if (ref[0] == 'U' || !strcmp(ref, "IC2")) return PART_IC;
    if (ref[0] == 'R' && ((ref[1] >= '0' && ref[1] <= '9') || ref[1] == 'N')) return PART_RESISTOR;
    return PART_OTHER;
}

static bool read_file(const char *path, uint8_t *buf, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(buf, 1, n, f);
    fclose(f);
    return got == n;
}

/* roms/mc10: mc10.rom, the 8K of BASIC. */
static void mc_find_roms(board_t *b) {
    mc10_t *z = MC(b);
    static const char *const dirs[] = { "roms/mc10", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]) && !z->rom_found; d++) {
        char path[256];
        snprintf(path, sizeof(path), "%s/mc10.rom", dirs[d]);
        z->rom_found = read_file(path, z->rom, sizeof(z->rom));
    }
}

static int mc_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

static void mc_keys_register(mc10_t *z);

static void mc_create(board_t *b) {
    mc10_t *z = MC(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "U1");
    z->p_vdg = sim_find_part(s, "U11");
    z->p_rom = sim_find_part(s, "U3");
    z->p_ram[0] = sim_find_part(s, "U9");
    z->p_ram[1] = sim_find_part(s, "U10");
    z->p_xtal = sim_find_part(s, "IC2");
    memset(z->rom, 0xFF, sizeof(z->rom));
    z->rom_found = false;
    if (z->p_rom >= 0) ((rom_state_t *)s->parts[z->p_rom].state)->data = z->rom;
    mc_find_roms(b);
    static const int col_pad[7] = { 1, 2, 4, 5, 6, 7, 8 };
    for (int i = 0; i < 8; i++) z->pin_row[i] = mc_pin_of(b, "J5", 1 + i);
    for (int i = 0; i < 7; i++) z->pin_col[i] = mc_pin_of(b, "J4", col_pad[i]);
    z->pin_rst = mc_pin_of(b, "S1", 2);
    b->pin_reset = z->pin_rst;                      /* the RESET switch: the reset net to GND */
    b->pin_reset_gnd = mc_pin_of(b, "S1", 1);
    mc_keys_register(z);
}

static void mc_power_on(board_t *b) {
    mc10_t *z = MC(b);
    sim_t *s = &b->sim;
    s->clock_level = 0;
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    mc_keys_register(z);
    memset(b->screen, 0, (size_t)COCO_FB_W * COCO_FB_H * sizeof(uint32_t));
    /* C8 holds the reset net low for a while after the power comes on (R24 pulls it up afterwards). */
    z->reset_cycles = (uint32_t)s->cycle + 4000;
    if (z->pin_rst >= 0) sim_drive(s, z->pin_rst, DRV_LO);
}

/* A closed key joins its row (KR, a 6803 port 1 pin) to its column (KB). */
static void mc_joins(board_t *b) {
    mc10_t *z = MC(b);
    sim_t *s = &b->sim;
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 7; col++) {
            if (!((z->key_matrix[row] >> col) & 1)) continue;
            int pr = z->pin_row[row], pc = z->pin_col[col];
            if (pr < 0 || pc < 0 || s->pins[pr].fault == PF_OPEN || s->pins[pc].fault == PF_OPEN) continue;
            sim_join(s, s->pins[pr].net, s->pins[pc].net);
        }
}

/* ------------------------------------------------------------------------ */
/* Keyboard                                                                  */
/* ------------------------------------------------------------------------ */

/* The matrix: the rows are KR0..KR7, the columns KB0..KB5 and the SHIFT line. */
static const char *const mc_keys[8][7] = {
    { "@", "H", "P", "X", "0", "8", "CONTROL" },
    { "A", "I", "Q", "Y", "1", "9", "" },
    { "B", "J", "R", "Z", "2", ":", "BREAK" },
    { "C", "K", "S", "", "3", ";", "" },
    { "D", "L", "T", "", "4", ",", "" },
    { "E", "M", "U", "", "5", "-", "" },
    { "F", "N", "V", "ENTER", "6", ".", "" },
    { "G", "O", "W", "SPACE", "7", "/", "SHIFT" },
};
enum { VK_ENTER = 13, VK_BREAK = 0x81, VK_CONTROL, VK_SHIFT };

static void mc_keys_register(mc10_t *z) {
    kbd_init(&z->kbd, 2);
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 7; col++) {
            const char *n = mc_keys[row][col];
            if (!n[0]) continue;
            int key = 0;
            if (n[1] == 0) key = (unsigned char)n[0];
            else if (!strcmp(n, "ENTER")) key = VK_ENTER;
            else if (!strcmp(n, "BREAK")) key = VK_BREAK;
            else if (!strcmp(n, "CONTROL")) key = VK_CONTROL;
            else if (!strcmp(n, "SPACE")) key = ' ';
            else if (!strcmp(n, "SHIFT")) { kbd_register_modifier(&z->kbd, 0, col, row); key = VK_SHIFT; }
            if (!key) continue;
            kbd_register_key(&z->kbd, key, col, row, 0);
            if (n[1] == 0) {                /* the shifted legends */
                static const char *const from = "123456789:;,-./", *const to = "!\"#$%&'()*+<=>?";
                const char *q = strchr(from, n[0]);
                if (q && n[0] != '\0') kbd_register_key(&z->kbd, (unsigned char)to[q - from], col, row, 1);
            }
        }
}

static void mc_sync_keys(board_t *b) {
    mc10_t *z = MC(b);
    bool changed = false;
    for (int row = 0; row < 8; row++) {
        uint8_t m = (uint8_t)z->kbd.scanout_column_masks[row];
        if (m != z->key_matrix[row]) { z->key_matrix[row] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

static void mc_key_down(board_t *b, int key) { kbd_key_down(&MC(b)->kbd, key); mc_sync_keys(b); }
static void mc_key_up(board_t *b, int key) { kbd_key_up(&MC(b)->kbd, key); mc_sync_keys(b); }
static void mc_frame_update(board_t *b, uint32_t us) { kbd_update(&MC(b)->kbd, us); mc_sync_keys(b); }

static int mc_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return VK_ENTER;
    case SAPP_KEYCODE_ESCAPE: return VK_BREAK;
    case SAPP_KEYCODE_RIGHT_SHIFT: return VK_SHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL: case SAPP_KEYCODE_RIGHT_CONTROL: return VK_CONTROL;
    default: return 0;
    }
}

/* The MC-10 types capitals without Shift (Shift and a letter makes one of the graphics blocks). */
static int mc_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

/* ------------------------------------------------------------------------ */
/* Running                                                                   */
/* ------------------------------------------------------------------------ */

static void mc_step(board_t *b) {
    mc10_t *z = MC(b);
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, z->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void mc_cycle_start(board_t *b) {
    mc10_t *z = MC(b);
    sim_t *s = &b->sim;
    s->cycle++;
    if (z->reset_cycles && s->cycle >= z->reset_cycles) {
        z->reset_cycles = 0;
        if (z->pin_rst >= 0) sim_drive(s, z->pin_rst, DRV_Z);       /* R24 pulls the reset net up */
    }
}

void mc10_run_steps(board_t *b, int steps) {
    static uint32_t phase;
    for (int i = 0; i < steps; i++) {
        if (phase == 0) mc_cycle_start(b);
        mc_step(b);
        if (++phase == MC10_STEPS_PER_CYCLE) phase = 0;
    }
}

static void mc_run(board_t *b, int cycles) {
    for (int i = 0; i < cycles; i++) {
        mc_cycle_start(b);
        for (int k = 0; k < MC10_STEPS_PER_CYCLE; k++) mc_step(b);
    }
}

/* The picture is the 6847's, as it drew it: nothing to decode. */
static void mc_monitor(board_t *b, uint32_t *out) {
    const uint32_t *fb = coco_vdg_screen(&b->sim, MC(b)->p_vdg);
    memcpy(out, fb, (size_t)COCO_FB_W * COCO_FB_H * sizeof(uint32_t));
}

/* ------------------------------------------------------------------------ */
/* RAM                                                                       */
/* ------------------------------------------------------------------------ */

typedef struct { uint8_t cells[2048]; int32_t out_cache; } mc_sram_t;      /* the 6116 model's state (parts.c) */

/* The CPU's $4000-$4FFF: A11 low is U9 (its /CS is VA11), high is U10. */
uint8_t mc10_ram_peek(board_t *b, uint16_t addr) {
    mc10_t *z = MC(b);
    int p = z->p_ram[(addr >> 11) & 1];
    if (p < 0) return 0;
    return ((mc_sram_t *)b->sim.parts[p].state)->cells[addr & 0x7FF];
}

static int mc_ram_cell(board_t *b, int part, uint16_t addr) {
    mc10_t *z = MC(b);
    if (addr < 0x4000 || addr > 0x4FFF) return -1;
    if (part == z->p_ram[(addr >> 11) & 1]) return addr & 0x7FF;
    return -1;
}

/* The screen as the text of the VDG's character set: the 32x16 screen at $4000. */
static void mc_print_text(board_t *b) {
    for (int y = 0; y < 16; y++) {
        char line[33];
        for (int x = 0; x < 32; x++) {
            int raw = mc10_ram_peek(b, (uint16_t)(0x4000 + y * 32 + x)), c = raw & 0x3F;
            line[x] = (raw & 0x80) ? '#' : (char)(c < 0x20 ? c + 0x40 : c);
        }
        line[32] = 0;
        printf("%s\n", line);
    }
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static const file_kind_t *mc_file_kind(const char *path, char *info, int len, bool *bad) {
    return board_state_file_kind(path, info, len, bad);
}
static bool mc_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)run;
    snprintf(msg, (size_t)msglen, "%s: the MC-10 takes no files yet (no cassette or cartridge built)", path);
    return false;
}
static void mc_destroy(board_t *b) { (void)b; }
static const char *mc_cpu_status(board_t *b) {
    const mc10_t *z = MC(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!z->rom_found) return "no ROM (put mc10.rom in roms/mc10)";
    return "running";
}
static void mc_cpu_regs(board_t *b, char *buf, int len) {
    const m6803_t *c = mc10_cpu_core(&b->sim, MC(b)->p_cpu);
    snprintf(buf, (size_t)len, "PC=%04X A=%02X B=%02X X=%04X S=%04X CC=%02X", c->pc, c->a, c->b, c->x, c->s, c->cc);
}

const machine_t machine_mc10 = {
    .id = "mc10",
    .name = "MC-10",
    .board_name = "MC-10",
    .issue = "MC-10",
    .size = sizeof(mc10_t),
    .data = &bd_mc10,
    .sim = { .rail_of = mc_rail_of, .kind_of = mc_kind_of, .cycles_per_us = 0.894886 },
    .cpu_hz = MC10_FREQ,
    .cycles_per_frame = MC10_CYCLES_PER_FRAME,
    .steps_per_cycle = MC10_STEPS_PER_CYCLE,
    .screen_w = COCO_FB_W, .screen_h = COCO_FB_H,
    .ready_cycles = 2 * MC10_FREQ,
    .parts = mc_part_defs, .nparts = sizeof(mc_part_defs) / sizeof(mc_part_defs[0]),
    .conns = mc_conn_defs, .nconns = sizeof(mc_conn_defs) / sizeof(mc_conn_defs[0]),
    .link_resistors = "",
    .crystals = "X101",
    .diodes = "D", .jumpers = "", .supplies = "",
    .keys_help = "Keys: type (capitals without Shift); Esc = BREAK",
    .reset_help = "Hold to press RESET: shorts the reset net (S1) to GND",
    .create = mc_create,
    .destroy = mc_destroy,
    .power_on = mc_power_on,
    .run = mc_run,
    .joins = mc_joins,
    .key_down = mc_key_down,
    .key_up = mc_key_up,
    .frame_update = mc_frame_update,
    .host_key = mc_host_key,
    .host_char = mc_host_char,
    .monitor = mc_monitor,
    .files_title = "Open a save state",
    .file_kind = mc_file_kind,
    .load = mc_load,
    .cpu_status = mc_cpu_status,
    .cpu_regs = mc_cpu_regs,
    .print_text = mc_print_text,
    .ram_cell = mc_ram_cell,
};
