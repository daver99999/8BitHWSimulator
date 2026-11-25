/* apple2e.c - the Apple IIe (820-0064-B motherboard, unenhanced, 6502).
 *
 * As with the II+, the board makes all its own timing: the 14.31818 MHz crystal Y1 (a clock source on its pin 1, wired to the
 * input of the buffer UE1 pin 12: the transistor oscillator is not simulated) feeds the timing HAL UD1 and the dividers UC1, which
 * make PHI0, RAS, CAS, Q3 and the video clocks. The MMU (UE4) and IOU (UE5) are behavioural models of the real chips
 * (apple2e_chips.c). The keyboard encoder UE14 is a part that holds the code of the key the machine has typed. The picture is
 * the dot stream of the video output decoded like the II+'s. The purpose is to troubleshoot the board, not to load programs: there
 * is no disk or tape. */
#include "apple2e.h"
#include "sokol/sokol_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

static const part_def_t ae_part_defs[] = {
#include "apple2e_parts.inc"
};

static const conn_def_t ae_conn_defs[] = {
    { "J14", "POWER", "1=GND 2=GND 3=+5V 4=+12V 5=-12V 6=-5V" },
    { "J20", "POWER (alternate)", "1=-5V 2=+12V 3=+5V 4=GND" },
    { "J18", "SPEAKER", "1=+5V 2=SPEAKER" },
    { "J8", "GAME DB9", "1=SW1 2=+5V 3=GND 4=PDL2 5=PDL0 6=SW2 7=SW0 8=PDL1 9=PDL3" },
    { "J11", "NTSC OUT", "1=VIDEO 2=GND" },
    { "J10", "CASSETTE OUT", "1=OUT 2=GND" },
    { "J9", "CASSETTE IN", "1=IN 2=GND" },
};

static int ae_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V") || !strcmp(name, "+5C")) return RAIL_VCC;
    return RAIL_NONE;
}

/* The filter inductors (L1-L6), the bridged solder jumpers (X2, X5) and the net tie NT1 are wires. */
static int ae_kind_of(const char *ref) {
    if (!strcmp(ref, "X2") || !strcmp(ref, "X5") || !strcmp(ref, "NT1")) return PART_LINK;
    if (ref[0] == 'L' && ref[1] >= '1' && ref[1] <= '9') return PART_LINK;
    if (!strcmp(ref, "UC4") || !strcmp(ref, "UA11")) return !strcmp(ref, "UA11") ? PART_OTHER : PART_IC;
    if (!strcmp(ref, "Y1") || !strncmp(ref, "OSC", 3) || ref[0] == 'Q' || !strncmp(ref, "SA", 2) || !strncmp(ref, "VA", 2) || ref[0] == 'D' || !strncmp(ref, "CR", 2) || ref[0] == 'X') return PART_OTHER;
    if (ref[0] == 'C' && ref[1] >= '0' && ref[1] <= '9') return PART_OTHER;
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    if (!strncmp(ref, "RP", 2)) return PART_RESISTOR;
    if (ref[0] == 'J') return PART_CONNECTOR;
    return PART_IC;
}

static int ae_pin_of(board_t *b, const char *ref, int number) {
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

/* roms/apple2e/: 342-0135-B.bin (CD, UE8: its /OE is ROMEN1'), 342-0134-A.bin (EF, UE10: ROMEN2'), 342-0132-B.bin (keyboard, UE12), 342-0133-A.bin (video, UF4) */
static void ae_find_roms(board_t *b) {
    apple2e_t *z = AE(b);
    static const char *const dirs[] = { "roms/apple2e", "roms", "." };
    static const struct { const char *name; int bit; size_t off, n; } roms[4] = {
        { "342-0135-B.bin", 0, offsetof(apple2e_t, rom_cd), 8192 }, { "342-0134-A.bin", 1, offsetof(apple2e_t, rom_ef), 8192 },
        { "342-0132-B.bin", 2, offsetof(apple2e_t, kbrom), 2048 }, { "342-0133-A.bin", 3, offsetof(apple2e_t, vrom), 4096 } };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++)
        for (int i = 0; i < 4; i++) {
            char path[512];
            if (z->rom_mask & (1u << roms[i].bit)) continue;
            snprintf(path, sizeof(path), "%s/%s", dirs[d], roms[i].name);
            if (read_file(path, (uint8_t *)z + roms[i].off, roms[i].n)) z->rom_mask |= 1u << roms[i].bit;
        }
}

static void ae_create(board_t *b) {
    apple2e_t *z = AE(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "UC4");
    z->p_xtal = sim_find_part(s, "Y1");
    z->p_kbd = sim_find_part(s, "UE14");
    z->p_kbrom = sim_find_part(s, "UE12");
    memset(z->rom_cd, 0xFF, sizeof(z->rom_cd));
    memset(z->rom_ef, 0xFF, sizeof(z->rom_ef));
    memset(z->kbrom, 0, sizeof(z->kbrom));
    memset(z->vrom, 0, sizeof(z->vrom));
    z->rom_mask = 0;
    static const struct { const char *ref; size_t off; } at[4] = {
        { "UE8", offsetof(apple2e_t, rom_cd) }, { "UE10", offsetof(apple2e_t, rom_ef) },
        { "UE12", offsetof(apple2e_t, kbrom) }, { "UF4", offsetof(apple2e_t, vrom) } };
    for (int i = 0; i < 4; i++) {
        int idx = sim_find_part(s, at[i].ref);
        if (idx >= 0) ((rom_state_t *)s->parts[idx].state)->data = (uint8_t *)z + at[i].off;
    }
    ae_find_roms(b);
    z->pin_xtal = ae_pin_of(b, "Y1", 1);
    z->pin_osc = ae_pin_of(b, "UE1", 12);
    z->pin_video = ae_pin_of(b, "UC5", 6);
    z->pin_sync = ae_pin_of(b, "UE5", 39);
    z->pin_a10 = ae_pin_of(b, "UE12", 19);
    z->pin_a9 = ae_pin_of(b, "UE12", 22);
    z->pin_12v = ae_pin_of(b, "J14", 4);
    z->pin_m12v = ae_pin_of(b, "J14", 5);
    z->pin_m5v = ae_pin_of(b, "J20", 1);
    z->pin_m5c = ae_pin_of(b, "J13", 3);
    b->pin_reset = ae_pin_of(b, "UC4", 40);
    b->pin_reset_gnd = ae_pin_of(b, "UC4", 21);
}

static void ae_power_on(board_t *b) {
    apple2e_t *z = AE(b);
    sim_t *s = &b->sim;
    s->clock_level = 0;
    z->beam_x = z->beam_y = 0;
    z->sync_prev = 1;
    z->sync_low = 0;
    z->step_in_cycle = 0;
    z->keyq_n = 0;
    z->strobe_off = z->akd_off = z->next_key = 0;
    memset(z->raw, 0, sizeof(z->raw));
    memset(b->screen, 0, (size_t)AE_SCREEN_W * AE_SCREEN_H * sizeof(uint32_t));
    sim_drive(s, z->pin_12v, DRIVE(S_SUPPLY, 1));
    sim_drive(s, z->pin_m5v, DRIVE(S_SUPPLY, 0));
    sim_drive(s, z->pin_m12v, DRIVE(S_SUPPLY, 0));
    sim_drive(s, z->pin_m5c, DRIVE(S_SUPPLY, 0));
    /* The keyboard (and the game connector) pull the open apple, closed apple and SW2 switch inputs low while nothing is pressed. */
    for (int k = 2; k <= 4; k++) sim_drive(s, ae_pin_of(b, "J15", k), DRIVE(S_PULL, 0));
}

static void ae_joins(board_t *b) {
    apple2e_t *z = AE(b);
    sim_t *s = &b->sim;
    if (z->pin_xtal >= 0 && z->pin_osc >= 0 && s->pins[z->pin_xtal].net >= 0 && s->pins[z->pin_osc].net >= 0)
        sim_join(s, s->pins[z->pin_xtal].net, s->pins[z->pin_osc].net);
}

/* ------------------------------------------------------------------------ */
/* Keyboard                                                                  */
/* ------------------------------------------------------------------------ */

#define STROBE_CYCLES 200
#define AKD_CYCLES    600
#define KEY_GAP       25000
#define KEY_GAP_CR    300000

/* The code the encoder must give for an ASCII character: the address of the keyboard ROM (A10 and A9 as the board has them
 * now) whose data is that character. */
static int ae_key_code(board_t *b, int key) {
    apple2e_t *z = AE(b);
    sim_t *s = &b->sim;
    int hi = (sim_read(s, z->pin_a10) ? 0x400 : 0) | (sim_read(s, z->pin_a9) ? 0x200 : 0);
    for (int a = 0; a < 512; a++)
        if ((z->kbrom[hi | a] & 0x7F) == key) return a;
    return -1;
}

static void ae_key_down(board_t *b, int key) {
    apple2e_t *z = AE(b);
    if (key > 0 && key < 128 && z->keyq_n < (int)sizeof(z->keyq)) z->keyq[z->keyq_n++] = (uint8_t)key;
}
static void ae_key_up(board_t *b, int key) { (void)b; (void)key; }
static void ae_frame_update(board_t *b, uint32_t us) { (void)b; (void)us; }

static void ae_keys_tick(board_t *b) {
    apple2e_t *z = AE(b);
    sim_t *s = &b->sim;
    if (z->p_kbd < 0) return;
    if (z->strobe_off && s->cycle >= z->strobe_off) {
        z->strobe_off = 0;
        apple2e_kbenc_set(&s->parts[z->p_kbd], -1, -1, 0);
        sim_mark_dirty(s, z->p_kbd);
    }
    if (z->akd_off && s->cycle >= z->akd_off) {
        z->akd_off = 0;
        apple2e_kbenc_set(&s->parts[z->p_kbd], -1, 0, -1);
        sim_mark_dirty(s, z->p_kbd);
    }
    if (!z->strobe_off && !z->akd_off && z->keyq_n && s->cycle >= z->next_key) {
        int k = z->keyq[0];
        memmove(z->keyq, z->keyq + 1, (size_t)--z->keyq_n);
        int code = ae_key_code(b, k);
        if (code < 0) return;
        apple2e_kbenc_set(&s->parts[z->p_kbd], code, 1, 1);
        sim_mark_dirty(s, z->p_kbd);
        z->strobe_off = s->cycle + STROBE_CYCLES;
        z->akd_off = s->cycle + AKD_CYCLES;
        z->next_key = s->cycle + (k == 0x0D ? KEY_GAP_CR : KEY_GAP);
    }
}

static int ae_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return 0x0D;
    case SAPP_KEYCODE_BACKSPACE: case SAPP_KEYCODE_LEFT: return 0x08;
    case SAPP_KEYCODE_RIGHT: return 0x15;
    case SAPP_KEYCODE_ESCAPE: return 0x1B;
    case SAPP_KEYCODE_UP: return 0x0B;
    case SAPP_KEYCODE_DOWN: return 0x0A;
    case SAPP_KEYCODE_TAB: return 0x09;
    default: return 0;
    }
}
static int ae_host_char(int c) { return c; }

/* ------------------------------------------------------------------------ */
/* Running, and the picture                                                  */
/* ------------------------------------------------------------------------ */

/* Dots are taken at every rising edge of the crystal. The beam returns when SYNC goes low (the leading edge of the pulse);
 * a line that is low for most of its length is vertical sync, after which the picture starts at the top. */
static void ae_beam(board_t *b) {
    apple2e_t *z = AE(b);
    sim_t *s = &b->sim;
    if (!s->clock_level) return;
    int sync = sim_read(s, z->pin_sync) ? 1 : 0;
    int vid = sim_read(s, z->pin_video) ? 1 : 0;
    if (!sync) z->sync_low++;
    if (z->sync_prev && !sync) {                    /* falling edge of SYNC */
        if (z->sync_low_last > 300) z->vs_seen = 1;
        z->beam_x = 0;
        z->beam_y++;
        if (z->vs_seen && z->sync_low_last <= 300) { z->vs_seen = 0; z->beam_y = 0; }
    }
    if (!z->sync_prev && sync) { z->sync_low_last = z->sync_low; z->sync_low = 0; }
    z->sync_prev = sync;
    if (z->beam_y >= 0 && z->beam_y < AE_LINES && z->beam_x < AE_LINE_SAMPLES) z->raw[z->beam_y][z->beam_x] = (uint8_t)vid;
    z->beam_x++;
}

static void ae_step(board_t *b) {
    apple2e_t *z = AE(b);
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, z->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    ae_beam(b);
    board_sample(b);
}

void apple2e_run_steps(board_t *b, int steps) {
    apple2e_t *z = AE(b);
    sim_t *s = &b->sim;
    for (int i = 0; i < steps; i++) {
        if (z->step_in_cycle == 0) { s->cycle++; ae_keys_tick(b); }
        ae_step(b);
        if (++z->step_in_cycle == AE_STEPS_PER_CYCLE) z->step_in_cycle = 0;
    }
}

static void ae_run(board_t *b, int cycles) {
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        ae_keys_tick(b);
        for (int k = 0; k < AE_STEPS_PER_CYCLE; k++) ae_step(b);
    }
}

/* NTSC: the colour subcarrier is the crystal / 4, so a dot window of four samples is a colour cycle. Each pixel of the
 * picture is the average of the 4 samples around it: its brightness, and the chroma from the phase of the cycle. */
#define AE_X0 226
#define AE_Y0 33
static uint32_t ae_rgb(float y, float i, float q) {
    float r = y + 0.956f * i + 0.621f * q, g = y - 0.272f * i - 0.647f * q, bl = y - 1.106f * i + 1.703f * q;
    float v[3] = { r, g, bl };
    uint32_t out = 0xFF000000u;
    for (int k = 0; k < 3; k++) {
        float c = v[k] < 0 ? 0 : v[k] > 1 ? 1 : v[k];
        out |= (uint32_t)(c * 255.0f + 0.5f) << (8 * k);
    }
    return out;
}

static void ae_monitor(board_t *b, uint32_t *out) {
    apple2e_t *z = AE(b);
    /* The hue of a dot pattern, measured on the lo-res colours against the usual Apple palette, is turned by 303 degrees; the
     * brightness curve and the chroma gain make a quarter of the dots in a cycle (a dark colour) as bright as a monitor does. */
    const float cs = 0.5446f, sn = -0.8387f, gain = 1.4f;
    for (int y = 0; y < AE_SCREEN_H; y++) {
        const uint8_t *line = z->raw[AE_Y0 + y / 2];
        for (int x = 0; x < AE_SCREEN_W; x++) {
            int sx = AE_X0 + x;
            float sum = 0, ci = 0, cq = 0;
            for (int k = 0; k < 4; k++) {
                float d = line[(sx + k) % AE_LINE_SAMPLES];
                float ph = (float)((sx + k) & 3) * 1.5707963f;
                sum += d; ci += d * cosf(ph); cq += d * sinf(ph);
            }
            ci *= 0.25f * gain; cq *= 0.25f * gain;
            out[y * AE_SCREEN_W + x] = ae_rgb(powf(sum * 0.25f, 0.7f), ci * cs - cq * sn, ci * sn + cq * cs);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* RAM                                                                       */
/* ------------------------------------------------------------------------ */

/* Eight 6664s (UF6-UF13, data bit 0-7). */
static int ae_rev8(int v) { int r = 0; for (int i = 0; i < 8; i++) r |= ((v >> i) & 1) << (7 - i); return r; }
/* The MMU puts A0-A5, A7, A8 on RA0-RA7 at /RAS and A9, A6, A10-A15 on RA0-RA7 at /CAS (MMU_RA of the chip), and the board wires RA0
 * to the 6664's A7 ... RA7 to A0: a chip's address is the RA byte reversed. */
static int ae_cell(uint16_t a) {
    int row = (a & 0x3F) | (a >> 7 & 1) << 6 | (a >> 8 & 1) << 7;
    int col = (a >> 9 & 1) | (a >> 6 & 1) << 1 | (a >> 10 & 1) << 2 | (a >> 11 & 1) << 3 | (a >> 12 & 1) << 4 | (a >> 13 & 1) << 5 |
              (a >> 14 & 1) << 6 | (a >> 15 & 1) << 7;
    return ae_rev8(row) << 8 | ae_rev8(col);
}
uint8_t apple2e_ram_peek(board_t *b, uint16_t addr) {
    uint8_t v = 0;
    int cell = ae_cell(addr);
    for (int bit = 0; bit < 8; bit++) {
        char ref[8];
        snprintf(ref, sizeof(ref), "UF%d", 6 + bit);
        int p = sim_find_part(&b->sim, ref);
        if (p >= 0) v |= (uint8_t)(((dram_state_t *)b->sim.parts[p].state)->cells[cell] << bit);
    }
    return v;
}

static void ae_print_text(board_t *b) {
    for (int y = 0; y < 24; y++) {
        char line[41];
        uint16_t base = (uint16_t)(0x400 + (y & 7) * 0x80 + (y >> 3) * 0x28);
        for (int x = 0; x < 40; x++) {
            int c = apple2e_ram_peek(b, (uint16_t)(base + x)) & 0x7F;
            line[x] = (char)(c < 0x20 ? c + 0x40 : c);
        }
        line[40] = 0;
        printf("%s\n", line);
    }
}

static const file_kind_t *ae_file_kind(const char *path, char *info, int len, bool *bad) {
    return board_state_file_kind(path, info, len, bad);
}
static bool ae_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)run;
    snprintf(msg, (size_t)msglen, "%s: the Apple IIe takes no files (the board is for troubleshooting)", path);
    return false;
}
static void ae_destroy(board_t *b) { (void)b; }
static const char *ae_cpu_status(board_t *b) {
    const apple2e_t *z = AE(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if ((z->rom_mask & 0xF) != 0xF) return "no ROMs (put them in roms/apple2e)";
    return "running";
}
static void ae_cpu_regs(board_t *b, char *buf, int len) { pet_cpu_regs_of(&b->sim.parts[AE(b)->p_cpu], buf, len); }

const machine_t machine_apple2e = {
    .id = "apple2e",
    .name = "Apple IIe",
    .board_name = "Apple IIe motherboard 820-0064-B",
    .issue = "IIe",
    .size = sizeof(apple2e_t),
    .data = &bd_apple2e,
    .sim = { .rail_of = ae_rail_of, .kind_of = ae_kind_of, .cycles_per_us = 1.0227 },
    .cpu_hz = AE_FREQ,
    .cycles_per_frame = AE_CYCLES_PER_FRAME,
    .steps_per_cycle = AE_STEPS_PER_CYCLE,
    .screen_w = AE_SCREEN_W, .screen_h = AE_SCREEN_H,
    .ready_cycles = 2 * AE_FREQ,
    .parts = ae_part_defs, .nparts = sizeof(ae_part_defs) / sizeof(ae_part_defs[0]),
    .conns = ae_conn_defs, .nconns = sizeof(ae_conn_defs) / sizeof(ae_conn_defs[0]),
    .link_resistors = "R30 R73 R41 R42 R43 R44 R21 R22 R23 R24",     /* series damping and protection resistors: wires */
    .crystals = "Y1",
    .diodes = "", .jumpers = "", .supplies = "+12V -5V -12V -5C",
    .keys_help = "Keys: type (lower case works); Esc = ESC, Backspace or Left = back arrow",
    .reset_help = "Hold to press RESET: shorts the 6502 /RESET (UC4 pin 40) to GND",
    .create = ae_create,
    .destroy = ae_destroy,
    .power_on = ae_power_on,
    .run = ae_run,
    .joins = ae_joins,
    .key_down = ae_key_down,
    .key_up = ae_key_up,
    .frame_update = ae_frame_update,
    .host_key = ae_host_key,
    .host_char = ae_host_char,
    .monitor = ae_monitor,
    .files_title = "Open a save state",
    .file_kind = ae_file_kind,
    .load = ae_load,
    .cpu_status = ae_cpu_status,
    .cpu_regs = ae_cpu_regs,
    .print_text = ae_print_text,
};
