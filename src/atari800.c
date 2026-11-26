/* atari800.c - the Atari 800 (motherboard CO14175 Rev A with its CPU board, personality board with the 10K OS ROMs, and three 16K RAM boards).
 *
 * The 3.579545 MHz crystal X101 (a clock source on its pin 1, wired to GTIA's OSC pin: the transistor oscillator is not simulated) is
 * the only clock the machine is given. GTIA divides it by two for F0, the CPU board's flip-flops make PHI2 and the halt synchroniser
 * from that, and ANTIC halts the 6502 and takes the bus for display list, screen, character and player/missile DMA. ANTIC, GTIA and
 * POKEY are behavioural models of the real chips (atari800_chips.c). One CPU cycle is four steps (half a crystal period each); a
 * step is also one hires pixel of the picture, which GTIA draws into the frame buffer. The purpose is to troubleshoot the board,
 * not to load programs: there are no disks or cassettes; the OS is in the personality board's mask ROMs (roms/atari800/). */
#include "atari800.h"
#include "sokol/sokol_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const part_def_t at_part_defs[] = {
#include "atari800_parts.inc"
};

static const conn_def_t at_conn_defs[] = {
    { "J115", "POWER / SIO", "1=+5V 17=-5V 18=+5V 21=+12V 15=GND 16=GND 19=GND 22=GND 2=AUDIO" },
    { "J105", "SPEAKER", "1=SPEAKER+ 2=SPEAKER-" },
    { "J106", "KEYBOARD", "9=KEYCOM 6=KB0 1=KB1 4=KB2 7=KB3 2=KB4 8=KB5 3=KB6 5=KB7 16=KC0 15=KC1 12=KC2 17=KC3 10=KC4 14=KC5 11=KC6 13=KC7" },
    { "J114", "CONSOLE SWITCHES", "1=OPTION 2=SELECT 3=START 4=RESET" },
    { "J110", "CONTROLLER 1", "1=UP 2=DOWN 3=LEFT 4=RIGHT 6=TRIGGER 7=+5V 8=GND" },
    { "J111", "CONTROLLER 2", "1=UP 2=DOWN 3=LEFT 4=RIGHT 6=TRIGGER 7=+5V 8=GND" },
    { "J112", "CONTROLLER 3", "1=UP 2=DOWN 3=LEFT 4=RIGHT 6=TRIGGER 7=+5V 8=GND" },
    { "J113", "CONTROLLER 4", "1=UP 2=DOWN 3=LEFT 4=RIGHT 6=TRIGGER 7=+5V 8=GND" },
};

static int at_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int at_kind_of(const char *ref) {
    if (ref[0] == 'J' || !strncmp(ref, "RB_", 3) || !strcmp(ref, "PERSC")) return PART_CONNECTOR;
    if (ref[0] == 'L' && ref[1] >= '0' && ref[1] <= '9') return PART_LINK;          /* the delay line inductors and filters: wires */
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    if (ref[0] == 'C' || ref[0] == 'Q' || ref[0] == 'X') return PART_OTHER;
    return PART_IC;
}

static int at_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

static bool read_file(const char *path, uint8_t *buf, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(buf, 1, n, f);
    fclose(f);
    return got == n;
}

/* roms/atari800/: atarios-revb-ntsc.rom (10K: $D800-$FFFF as the chips D, E and F of the personality board), atari-basic-revc.rom (8K) */
static void at_find_roms(board_t *b) {
    atari800_t *z = AT(b);
    static const char *const dirs[] = { "roms/atari800", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        char path[512];
        if (!(z->rom_mask & 1)) {
            snprintf(path, sizeof(path), "%s/atarios-revb-ntsc.rom", dirs[d]);
            if (read_file(path, z->os, sizeof(z->os))) z->rom_mask |= 1;
        }
        if (!(z->rom_mask & 2)) {
            snprintf(path, sizeof(path), "%s/atari-basic-revc.rom", dirs[d]);
            if (read_file(path, z->basic, sizeof(z->basic))) z->rom_mask |= 2;
        }
    }
}

/* A resistor that is an RC delay of the board (the timing of the RAM boards): from the leg on the net 'from' to the other. */
static void at_delay(board_t *b, const char *ref, const char *from) {
    sim_t *s = &b->sim;
    int pi = sim_find_part(s, ref);
    if (pi < 0) return;
    part_t *p = &s->parts[pi];
    int a = -1;
    for (int k = 0; k < p->npins; k++) {
        int pin = p->first_pin + k;
        if (s->pins[pin].net >= 0 && !strcmp(s->nets[s->pins[pin].net].name, from)) a = pin;
    }
    if (a < 0) return;
    resistor_state_t *st = p->state;
    if (!st) { p->state = st = calloc(1, sizeof(*st)); p->state_size = sizeof(*st); }
    p->kind = PART_RESISTOR;
    st->nel = 1;
    st->el[0] = (element_t){ a, part_pin(p, s->pins[a].number == 1 ? 2 : 1), EL_ANALOG };
    element_set_type(s, p, 0, EL_DELAY);
}

static void at_keys_register(atari800_t *z);

static void at_create(board_t *b) {
    atari800_t *z = AT(b);
    sim_t *s = &b->sim;
    z->p_xtal = sim_find_part(s, "X101");
    z->p_cpu = sim_find_part(s, "A303");
    z->p_gtia = sim_find_part(s, "A301");
    z->p_antic = sim_find_part(s, "A302");
    z->p_pokey = sim_find_part(s, "A101");
    z->p_pia = sim_find_part(s, "A102");
    memset(z->os, 0xFF, sizeof(z->os));
    memset(z->basic, 0xFF, sizeof(z->basic));
    z->rom_mask = 0;
    at_find_roms(b);
    /* A402 is 2K ($D800: S6 low, A12 high, A11 high), A403 4K ($E000: S7 low, A12 low), A401 4K ($F000: S7 low, A12 high). */
    static const struct { const char *ref; size_t off, size; int cs18, cs20, cs21; } roms[3] = {
        { "A402", 0, 2048, 1, 0, 1 }, { "A403", 2048, 4096, 0, 0, 0 }, { "A401", 6144, 4096, 0, 0, 1 } };
    for (int i = 0; i < 3; i++) {
        int idx = sim_find_part(s, roms[i].ref);
        if (idx >= 0) atari_rom_set(&s->parts[idx], z->os + roms[i].off, (unsigned)roms[i].size, roms[i].cs18, roms[i].cs20, roms[i].cs21);
    }
    if (z->p_gtia >= 0 && b->screen) atari_gtia_set_screen(&s->parts[z->p_gtia], b->screen);
    z->pin_rcnode = at_pin_of(b, "C198", 1);
    z->pin_12v = at_pin_of(b, "J115", 21);
    z->pin_m5v = at_pin_of(b, "J115", 17);
    z->pin_osc = at_pin_of(b, "X101", 1);
    at_delay(b, "R403", "PHI2");
    for (int i = 1; i <= 3; i++) {
        char ref[16], net[16];
        snprintf(ref, sizeof(ref), "R501_%d", i);
        snprintf(net, sizeof(net), "RAS_%d", i);
        at_delay(b, ref, net);
    }
    b->pin_reset = at_pin_of(b, "A302", 6);              /* the RESET key pulls ANTIC's /RNMI to ground */
    b->pin_reset_gnd = at_pin_of(b, "A302", 1);
    at_keys_register(z);
}

static void at_power_on(board_t *b) {
    atari800_t *z = AT(b);
    sim_t *s = &b->sim;
    s->clock_level = 0;
    z->keyq_n = 0;
    at_keys_register(z);
    memset(b->screen, 0, (size_t)AT_SCREEN_W * AT_SCREEN_H * sizeof(uint32_t));
    if (z->p_gtia >= 0) atari_gtia_set_screen(&s->parts[z->p_gtia], b->screen);
    sim_drive(s, z->pin_12v, DRIVE(S_SUPPLY, 1));
    sim_drive(s, z->pin_m5v, DRIVE(S_SUPPLY, 0));
    /* The power-on reset: C198 (0.1uF) charges through R156 (470K), and the CMOS buffer A103 keeps /RESET low until it is half way. */
    z->reset_until = (uint64_t)s->cycle + 60000;
    if (z->pin_rcnode >= 0) sim_drive(s, z->pin_rcnode, DRV_LO);
}

/* ------------------------------------------------------------------------ */
/* Keyboard: J106 is a matrix of 8 columns KB0-KB7 (driven by Z103, one low) and 8 rows KC0-KC7 (read through Z104); the key with the
 * code N (the scan count K5..K0) joins KB(N & 7) to KC(N >> 3). Shift joins KEYCOM (KR2) to ground, the console keys short their pin of
 * J114 to ground. Matrix lines 8..11 of the helper are those extra keys. */
/* ------------------------------------------------------------------------ */

enum { VK_RETURN = 13, VK_ESC = 27, VK_TAB = 9, VK_BS = 8, VK_INV = 0x80, VK_CAPS, VK_SHIFT, VK_START, VK_SELECT, VK_OPTION };

static const struct { uint8_t code; const char *legend; } at_keys[] = {
    { 0x00, "Ll" }, { 0x01, "Jj" }, { 0x02, ";:" }, { 0x05, "Kk" }, { 0x06, "+\\" }, { 0x07, "*^" }, { 0x08, "Oo" }, { 0x0A, "Pp" },
    { 0x0B, "Uu" }, { 0x0C, "\r" }, { 0x0D, "Ii" }, { 0x0E, "-_" }, { 0x0F, "=|" }, { 0x10, "Vv" }, { 0x12, "Cc" }, { 0x15, "Bb" },
    { 0x16, "Xx" }, { 0x17, "Zz" }, { 0x18, "4$" }, { 0x1A, "3#" }, { 0x1B, "6&" }, { 0x1C, "\033" }, { 0x1D, "5%" }, { 0x1E, "2\"" },
    { 0x1F, "1!" }, { 0x20, ",[" }, { 0x21, " " }, { 0x22, ".]" }, { 0x23, "Nn" }, { 0x25, "Mm" }, { 0x26, "/?" }, { 0x28, "Rr" },
    { 0x2A, "Ee" }, { 0x2B, "Yy" }, { 0x2C, "\t" }, { 0x2D, "Tt" }, { 0x2E, "Ww" }, { 0x2F, "Qq" }, { 0x30, "9(" }, { 0x32, "0)" },
    { 0x33, "7'" }, { 0x34, "\b" }, { 0x35, "8@" }, { 0x36, "<<" }, { 0x37, ">>" }, { 0x38, "Ff" }, { 0x39, "Hh" }, { 0x3A, "Dd" },
    { 0x3D, "Gg" }, { 0x3E, "Ss" }, { 0x3F, "Aa" } };

static void at_keys_register(atari800_t *z) {
    kbd_init(&z->kbd, 2);
    kbd_register_modifier(&z->kbd, 0, 0, 8);
    for (size_t i = 0; i < sizeof(at_keys) / sizeof(at_keys[0]); i++) {
        int col = at_keys[i].code & 7, row = at_keys[i].code >> 3;
        const char *l = at_keys[i].legend;
        kbd_register_key(&z->kbd, (unsigned char)l[0], col, row, 0);
        if (l[1]) {
            if ((l[0] >= 'A' && l[0] <= 'Z')) kbd_register_key(&z->kbd, (unsigned char)l[1], col, row, 0);
            else kbd_register_key(&z->kbd, (unsigned char)l[1], col, row, 1);
        }
    }
    kbd_register_key(&z->kbd, VK_INV, 7, 4, 0);
    kbd_register_key(&z->kbd, VK_CAPS, 4, 7, 0);
    kbd_register_key(&z->kbd, VK_START, 0, 9, 0);
    kbd_register_key(&z->kbd, VK_SELECT, 0, 10, 0);
    kbd_register_key(&z->kbd, VK_OPTION, 0, 11, 0);
    kbd_register_key(&z->kbd, VK_SHIFT, 0, 8, 0);
}

static const char *const at_kb_pin[8] = { "6", "1", "4", "7", "2", "8", "3", "5" };
static const char *const at_kc_pin[8] = { "16", "15", "12", "17", "10", "14", "11", "13" };

static void at_join_pins(board_t *b, int pa, int pb) {
    sim_t *s = &b->sim;
    if (pa < 0 || pb < 0 || s->pins[pa].net < 0 || s->pins[pb].net < 0) return;
    if (s->pins[pa].fault == PF_OPEN || s->pins[pb].fault == PF_OPEN) return;
    sim_join(s, s->pins[pa].net, s->pins[pb].net);
}

static void at_joins(board_t *b) {
    atari800_t *z = AT(b);
    sim_t *s = &b->sim;
    if (z->pin_osc >= 0 && s->pins[z->pin_osc].net >= 0) {
        int osc = at_pin_of(b, "A301", 28);
        at_join_pins(b, z->pin_osc, osc);
    }
    int gnd = at_pin_of(b, "A302", 1);
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            if ((z->km[row] >> col) & 1)
                at_join_pins(b, board_pad_pin(b, sim_find_part(s, "J106"), at_kc_pin[row]), board_pad_pin(b, sim_find_part(s, "J106"), at_kb_pin[col]));
    if (z->km[8] & 1) at_join_pins(b, board_pad_pin(b, sim_find_part(s, "J106"), "9"), gnd);
    static const char *const cons[3] = { "3", "2", "1" };           /* START, SELECT, OPTION */
    for (int i = 0; i < 3; i++)
        if (z->km[9 + i] & 1) at_join_pins(b, board_pad_pin(b, sim_find_part(s, "J114"), cons[i]), gnd);
}

static void at_sync_keys(board_t *b) {
    atari800_t *z = AT(b);
    bool changed = false;
    for (int i = 0; i < 12; i++) {
        uint8_t m = (uint8_t)z->kbd.scanout_column_masks[i];
        if (m != z->km[i]) { z->km[i] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

static void at_key_down(board_t *b, int key) { kbd_key_down(&AT(b)->kbd, key); at_sync_keys(b); }
static void at_key_up(board_t *b, int key) { kbd_key_up(&AT(b)->kbd, key); at_sync_keys(b); }
static void at_frame_update(board_t *b, uint32_t us) { kbd_update(&AT(b)->kbd, us); at_sync_keys(b); }

static int at_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return VK_RETURN;
    case SAPP_KEYCODE_BACKSPACE: return VK_BS;
    case SAPP_KEYCODE_ESCAPE: return VK_ESC;
    case SAPP_KEYCODE_TAB: return VK_TAB;
    case SAPP_KEYCODE_LEFT_SHIFT: case SAPP_KEYCODE_RIGHT_SHIFT: return VK_SHIFT;
    case SAPP_KEYCODE_F2: return VK_OPTION;
    case SAPP_KEYCODE_F3: return VK_SELECT;
    case SAPP_KEYCODE_F4: return VK_START;
    case SAPP_KEYCODE_F6: return VK_INV;
    case SAPP_KEYCODE_F7: return VK_CAPS;
    default: return 0;
    }
}
static int at_host_char(int c) { return c; }

/* ------------------------------------------------------------------------ */
/* Running                                                                   */
/* ------------------------------------------------------------------------ */

static void at_step(board_t *b) {
    atari800_t *z = AT(b);
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, z->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void at_cycle_start(board_t *b) {
    atari800_t *z = AT(b);
    sim_t *s = &b->sim;
    s->cycle++;
    if (z->reset_until && s->cycle >= z->reset_until) {
        z->reset_until = 0;
        if (z->pin_rcnode >= 0) sim_drive(s, z->pin_rcnode, DRV_Z);
    }
}

static void at_run(board_t *b, int cycles) {
    for (int i = 0; i < cycles; i++) {
        at_cycle_start(b);
        for (int k = 0; k < AT_STEPS_PER_CYCLE; k++) at_step(b);
    }
}

void atari800_run_steps(board_t *b, int steps) {
    atari800_t *z = AT(b);
    for (int i = 0; i < steps; i++) {
        if (z->step_in_cycle == 0) at_cycle_start(b);
        at_step(b);
        if (++z->step_in_cycle == AT_STEPS_PER_CYCLE) z->step_in_cycle = 0;
    }
}

static void at_monitor(board_t *b, uint32_t *out) {
    memcpy(out, b->screen, (size_t)AT_SCREEN_W * AT_SCREEN_H * sizeof(uint32_t));
}

/* ------------------------------------------------------------------------ */
/* Text of the screen (--text) and the CPU                                   */
/* ------------------------------------------------------------------------ */

static void at_print_text(board_t *b) {
    char st[160];
    atari_antic_status_of(&b->sim.parts[AT(b)->p_antic], st, sizeof(st));
    printf("(the Atari 800 screen is graphics; ANTIC: %s)\n", st);
}

static const file_kind_t at_cart_kind = { "cartridge", "Plug into the left slot", true, ROLE_ROM };
static const file_kind_t *at_file_kind(const char *path, char *info, int len, bool *bad) {
    const file_kind_t *state = board_state_file_kind(path, info, len, bad);
    if (state) return state;
    size_t pl = strlen(path);
    if (pl < 5 || (strcasecmp(path + pl - 4, ".rom") && strcasecmp(path + pl - 4, ".bin"))) return NULL;
    FILE *f = fopen(path, "rb");
    long n = -1;
    if (f) { fseek(f, 0, SEEK_END); n = ftell(f); fclose(f); }
    if (n != 8192) return NULL;
    snprintf(info, (size_t)len, "8K cartridge ROM ($A000-$BFFF), plugs into the left slot (J109)");
    return &at_cart_kind;
}
static void at_plug(board_t *b, bool in) {
    atari800_t *z = AT(b);
    int idx = sim_find_part(&b->sim, "CARTL");
    z->basic_in = in;
    if (idx >= 0) atari_cart_set(&b->sim.parts[idx], in ? z->basic : NULL);
}
/* A cartridge is not a program: the 8K ROM goes in the left slot as it would on the real board, and the machine restarts. */
static bool at_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)run;
    atari800_t *z = AT(b);
    if (!read_file(path, z->basic, sizeof(z->basic))) {
        snprintf(msg, (size_t)msglen, "%s: not an 8K cartridge ROM (the Atari 800 takes only cartridges, the board is for troubleshooting)", path);
        return false;
    }
    at_plug(b, true);
    board_power_on(b);
    snprintf(msg, (size_t)msglen, "cartridge plugged into the left slot");
    return true;
}
static const char *at_cart_name(board_t *b) { return AT(b)->basic_in ? "8K cartridge" : NULL; }
static void at_remove_cart(board_t *b) { at_plug(b, false); board_power_on(b); }
static void at_destroy(board_t *b) { (void)b; }
static const char *at_cpu_status(board_t *b) {
    const atari800_t *z = AT(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!(z->rom_mask & 1)) return "no OS ROM (put atarios-revb-ntsc.rom in roms/atari800)";
    return "running";
}
static void at_cpu_regs(board_t *b, char *buf, int len) { pet_cpu_regs_of(&b->sim.parts[AT(b)->p_cpu], buf, len); }

const machine_t machine_atari800 = {
    .id = "atari800",
    .name = "Atari 800",
    .board_name = "Atari 800 CO14175 Rev A",
    .issue = "800",
    .size = sizeof(atari800_t),
    .data = &bd_atari800,
    .sim = { .rail_of = at_rail_of, .kind_of = at_kind_of, .cycles_per_us = 1.7898 },
    .cpu_hz = AT_FREQ,
    .cycles_per_frame = AT_CYCLES_PER_FRAME,
    .steps_per_cycle = AT_STEPS_PER_CYCLE,
    .screen_w = AT_SCREEN_W, .screen_h = AT_SCREEN_H,
    .ready_cycles = 3 * AT_FREQ,
    .parts = at_part_defs, .nparts = sizeof(at_part_defs) / sizeof(at_part_defs[0]),
    .conns = at_conn_defs, .nconns = sizeof(at_conn_defs) / sizeof(at_conn_defs[0]),
    .link_resistors = "R110 R111 R112 R113 R114 R115 R116 R117 R118 R119 R120 R121 R122 R123 R124 R125 R126 R168 R169 R170 R171 R172 R173 "
                      "R174 R175 R176 R177 R178 R184 R185 R186 R187 R404",
    .crystals = "X101",
    .diodes = "CR", .jumpers = "", .supplies = "+12V -5V",
    .keys_help = "Keys: type (letters are capitals unless CAPS is toggled); F2 = OPTION, F3 = SELECT, F4 = START, F6 = Atari key, F7 = CAPS",
    .reset_help = "Hold to press RESET: pulls ANTIC's /RNMI (pin 6) to ground",
    .create = at_create,
    .destroy = at_destroy,
    .power_on = at_power_on,
    .run = at_run,
    .joins = at_joins,
    .key_down = at_key_down,
    .key_up = at_key_up,
    .frame_update = at_frame_update,
    .host_key = at_host_key,
    .host_char = at_host_char,
    .monitor = at_monitor,
    .files_title = "Open a cartridge ROM or a save state",
    .cart_slot = "J109", .cart_name = at_cart_name, .remove_cart = at_remove_cart,
    .file_kind = at_file_kind,
    .load = at_load,
    .cpu_status = at_cpu_status,
    .cpu_regs = at_cpu_regs,
    .print_text = at_print_text,
};
