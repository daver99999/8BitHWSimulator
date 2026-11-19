/* apple2plus.c - the Apple ][+ (rev 0 motherboard, 1979).
 *
 * The board makes all its own timing: the 14.31818 MHz crystal Y1 (a clock source on its pin 1, wired to the input of the
 * gate B2.8: the transistor oscillator around it is not simulated) feeds the counters D11-D14 and the shift registers
 * B1, C2, which make the 7 MHz, the colour clocks, the phases of the RAM (RAS, AX, CAS) and PHI0, the 1.023 MHz clock
 * of the 6502 (the one of every line is two crystal periods longer, as on the real machine). Nothing in this file drives
 * a bus cycle: a step is half a period of the crystal (35 ns), 28 steps are what sim.cycle counts as a cycle.
 * The video is not a chip but a 14.31818 MHz stream of dots: the board's VIDEO_DATA with the composite SYNC. monitor()
 * decodes the NTSC colour from it (the artifact colours of the high resolution graphics, too).
 * The keyboard is outside the board: the keyboard connector A7 is a part that holds the key's seven bits and its strobe. */
#include "apple2plus.h"
#include "sokol/sokol_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

static const part_def_t a2_part_defs[] = {
#include "apple2plus_parts.inc"
};

static const conn_def_t a2_conn_defs[] = {
    { "CP0", "POWER", "1=GND 2=GND 3=+5V 4=+12V 5=-12V 6=-5V" },
    { "SPKR1", "SPEAKER", "1=SPEAKER 2=+5V" },
    { "J2", "CASSETTE OUT", "S=OUT T=GND" },
    { "J14", "GAME I/O", "1=+5V 2=PB0 3=PB1 4=PB2 5=STROBE 6=PDL0 7=PDL2 8=GND 9=ANN0 10=PDL1 11=PDL3 12=ANN1 13=ANN2 14=ANN3 15=SW3 16=NC" },
};

/* Power. */
static int a2_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

/* The solder jumpers that are bridged on the board (the 60 Hz ones and the three of the character generator) are wires. */
static int a2_kind_of(const char *ref) {
    static const char *const links[] = { "JP9", "JP10", "JP11", "JP12", "JP16" };
    for (size_t i = 0; i < sizeof(links) / sizeof(links[0]); i++)
        if (!strcmp(ref, links[i])) return PART_LINK;
    if (!strncmp(ref, "ROM-", 4) || !strcmp(ref, "U3")) return PART_IC;
    if (!strcmp(ref, "Y1") || !strncmp(ref, "Q", 1) || ref[0] == 'L' || !strncmp(ref, "TP", 2)) return PART_OTHER;
    if (!strncmp(ref, "Cap", 3) || !strncmp(ref, "CT", 2) || !strncmp(ref, "CR", 2) || !strcmp(ref, "C15")) return PART_OTHER;
    if (ref[0] == 'R' && ((ref[1] >= '0' && ref[1] <= '9') || ref[1] == 'A')) return PART_RESISTOR;
    if (!strcmp(ref, "CP0") || !strcmp(ref, "SPKR1") || !strncmp(ref, "SLOT", 4) || !strcmp(ref, "J0") || !strcmp(ref, "J2") ||
        !strcmp(ref, "J14") || !strcmp(ref, "J18") || !strcmp(ref, "K14")) return PART_CONNECTOR;
    if (!strncmp(ref, "JP", 2)) return PART_OTHER;
    if (!strcmp(ref, "K13")) return PART_OTHER;     /* the LM741 of the cassette input: not simulated */
    return PART_IC;
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static int a2_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

/* The ROMs: roms/apple2plus/apple2plus.rom is the 12K of $D000-$FFFF, chargen.rom the 2K of the character generator. */
static bool read_file(const char *path, uint8_t *buf, size_t n) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(buf, 1, n, f);
    fclose(f);
    return got == n;
}

static void a2_find_roms(board_t *b) {
    apple2plus_t *z = A2(b);
    static const char *const dirs[] = { "roms/apple2plus", "roms", "." };
    uint8_t rom[12288];
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        char path[512];
        if (!(z->rom_mask & 0x3F)) {
            snprintf(path, sizeof(path), "%s/apple2plus.rom", dirs[d]);
            if (read_file(path, rom, sizeof(rom))) {
                for (int i = 0; i < 6; i++) memcpy(z->rom[i], rom + i * 2048, 2048);
                z->rom_mask |= 0x3F;
            }
        }
        if (!(z->rom_mask & 0x40)) {
            snprintf(path, sizeof(path), "%s/chargen.rom", dirs[d]);
            if (read_file(path, z->chars, sizeof(z->chars))) z->rom_mask |= 0x40;
        }
    }
}

static void a2_create(board_t *b) {
    apple2plus_t *z = A2(b);
    sim_t *s = &b->sim;
    static const char *const rom_refs[6] = { "ROM-D0", "ROM-D8", "ROM-E0", "ROM-E8", "ROM-F0", "ROM-F8" };
    z->p_cpu = sim_find_part(s, "U3");
    z->p_xtal = sim_find_part(s, "Y1");
    z->p_kbd = sim_find_part(s, "A7");
    z->p_chr = sim_find_part(s, "A5");
    memset(z->rom, 0xFF, sizeof(z->rom));
    memset(z->chars, 0, sizeof(z->chars));
    z->rom_mask = 0;
    for (int i = 0; i < 6; i++) {
        int idx = sim_find_part(s, rom_refs[i]);
        if (idx >= 0) ((rom_state_t *)s->parts[idx].state)->data = z->rom[i];
    }
    if (z->p_chr >= 0) ((rom_state_t *)s->parts[z->p_chr].state)->data = z->chars;
    a2_find_roms(b);
    z->pin_xtal = a2_pin_of(b, "Y1", 1);
    z->pin_osc = a2_pin_of(b, "B2", 10);
    z->pin_video = a2_pin_of(b, "B10", 5);
    z->pin_sync = a2_pin_of(b, "C13", 8);
    z->pin_12v = a2_pin_of(b, "CP0", 4);
    z->pin_m12v = a2_pin_of(b, "CP0", 5);
    z->pin_m5v = a2_pin_of(b, "CP0", 6);
    b->pin_reset = a2_pin_of(b, "U3", 40);          /* the RESET key: /RESET to GND */
    b->pin_reset_gnd = a2_pin_of(b, "U3", 21);
}

static void a2_power_on(board_t *b) {
    apple2plus_t *z = A2(b);
    sim_t *s = &b->sim;
    s->clock_level = 0;
    z->beam_x = z->beam_y = 0;
    z->sync_prev = 1;
    z->sync_low = 0;
    z->step_in_cycle = 0;
    z->keyq_n = 0;
    z->strobe_off = z->next_key = 0;
    memset(z->raw, 0, sizeof(z->raw));
    memset(b->screen, 0, (size_t)A2_SCREEN_W * A2_SCREEN_H * sizeof(uint32_t));
    sim_drive(s, z->pin_12v, DRIVE(S_SUPPLY, 1));
    sim_drive(s, z->pin_m5v, DRIVE(S_SUPPLY, 0));
    sim_drive(s, z->pin_m12v, DRIVE(S_SUPPLY, 0));
}

/* The transistor oscillator of the crystal is not simulated: the crystal's pin drives the gate that squares it. */
static void a2_joins(board_t *b) {
    apple2plus_t *z = A2(b);
    sim_t *s = &b->sim;
    if (z->pin_xtal >= 0 && z->pin_osc >= 0 && s->pins[z->pin_xtal].net >= 0 && s->pins[z->pin_osc].net >= 0)
        sim_join(s, s->pins[z->pin_xtal].net, s->pins[z->pin_osc].net);
}

/* ------------------------------------------------------------------------ */
/* Keyboard                                                                  */
/* ------------------------------------------------------------------------ */

#ifndef STROBE_CYCLES
#define STROBE_CYCLES 600
#endif
#define KEY_GAP       25000     /* about 25 ms between two keys: the program takes each one before the next comes (the Apple has no key buffer) */
#define KEY_GAP_CR    300000    /* after RETURN the line is run: keys typed meanwhile would be lost */

static void a2_key_down(board_t *b, int key) {
    apple2plus_t *z = A2(b);
    if (key > 0 && key < 128 && z->keyq_n < (int)sizeof(z->keyq)) z->keyq[z->keyq_n++] = (uint8_t)key;
}
static void a2_key_up(board_t *b, int key) { (void)b; (void)key; }

static void a2_keys_tick(board_t *b) {
    apple2plus_t *z = A2(b);
    sim_t *s = &b->sim;
    if (z->strobe_off && s->cycle >= z->strobe_off) {
        z->strobe_off = 0;
        apple2_kbc_set(&s->parts[z->p_kbd], -1, 0, -1);
        sim_mark_dirty(s, z->p_kbd);
    }
    if (!z->strobe_off && z->keyq_n && s->cycle >= z->next_key) {
        int k = z->keyq[0];
        memmove(z->keyq, z->keyq + 1, (size_t)--z->keyq_n);
        apple2_kbc_set(&s->parts[z->p_kbd], k, 1, -1);
        sim_mark_dirty(s, z->p_kbd);
        z->strobe_off = s->cycle + STROBE_CYCLES;
        z->next_key = s->cycle + (k == 0x0D ? KEY_GAP_CR : KEY_GAP);
    }
}

static int a2_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return 0x0D;
    case SAPP_KEYCODE_BACKSPACE: case SAPP_KEYCODE_LEFT: return 0x08;
    case SAPP_KEYCODE_RIGHT: return 0x15;
    case SAPP_KEYCODE_ESCAPE: return 0x1B;
    case SAPP_KEYCODE_UP: return 0x0B;      /* not on the II+, but the editor takes it */
    case SAPP_KEYCODE_DOWN: return 0x0A;
    default: return 0;
    }
}

/* The Apple ][+ has no lower case. */
static int a2_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

/* ------------------------------------------------------------------------ */
/* Running, and the picture                                                  */
/* ------------------------------------------------------------------------ */

/* Dots are taken at every rising edge of the crystal. The beam returns when SYNC goes low (the leading edge of the pulse);
 * a line that is low for most of its length is vertical sync, after which the picture starts at the top. */
static void a2_beam(board_t *b) {
    apple2plus_t *z = A2(b);
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
    if (z->beam_y >= 0 && z->beam_y < A2_LINES && z->beam_x < A2_LINE_SAMPLES) z->raw[z->beam_y][z->beam_x] = (uint8_t)vid;
    z->beam_x++;
}

static void a2_step(board_t *b) {
    apple2plus_t *z = A2(b);
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, z->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    a2_beam(b);
    board_sample(b);
}

void apple2plus_run_steps(board_t *b, int steps) {
    apple2plus_t *z = A2(b);
    sim_t *s = &b->sim;
    for (int i = 0; i < steps; i++) {
        if (z->step_in_cycle == 0) { s->cycle++; a2_keys_tick(b); }
        a2_step(b);
        if (++z->step_in_cycle == A2_STEPS_PER_CYCLE) z->step_in_cycle = 0;
    }
}

static void a2_run(board_t *b, int cycles) {
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        a2_keys_tick(b);
        for (int k = 0; k < A2_STEPS_PER_CYCLE; k++) a2_step(b);
    }
}

/* NTSC: the colour subcarrier is the crystal / 4, so a dot window of four samples is a colour cycle. Each pixel of the
 * picture is the average of the 4 samples around it: its brightness, and the chroma from the phase of the cycle. */
#define A2_X0 226
#define A2_Y0 33
static uint32_t a2_rgb(float y, float i, float q) {
    float r = y + 0.956f * i + 0.621f * q, g = y - 0.272f * i - 0.647f * q, bl = y - 1.106f * i + 1.703f * q;
    float v[3] = { r, g, bl };
    uint32_t out = 0xFF000000u;
    for (int k = 0; k < 3; k++) {
        float c = v[k] < 0 ? 0 : v[k] > 1 ? 1 : v[k];
        out |= (uint32_t)(c * 255.0f + 0.5f) << (8 * k);
    }
    return out;
}

static void a2_monitor(board_t *b, uint32_t *out) {
    apple2plus_t *z = A2(b);
    /* The hue of a dot pattern, measured on the lo-res colours against the usual Apple palette, is turned by 303 degrees; the
     * brightness curve and the chroma gain make a quarter of the dots in a cycle (a dark colour) as bright as a monitor does. */
    const float cs = 0.5446f, sn = -0.8387f, gain = 1.4f;
    for (int y = 0; y < A2_SCREEN_H; y++) {
        const uint8_t *line = z->raw[A2_Y0 + y / 2];
        for (int x = 0; x < A2_SCREEN_W; x++) {
            int sx = A2_X0 + x;
            float sum = 0, ci = 0, cq = 0;
            for (int k = 0; k < 4; k++) {
                float d = line[(sx + k) % A2_LINE_SAMPLES];
                float ph = (float)((sx + k) & 3) * 1.5707963f;
                sum += d; ci += d * cosf(ph); cq += d * sinf(ph);
            }
            ci *= 0.25f * gain; cq *= 0.25f * gain;
            out[y * A2_SCREEN_W + x] = a2_rgb(powf(sum * 0.25f, 0.7f), ci * cs - cq * sn, ci * sn + cq * cs);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* RAM                                                                       */
/* ------------------------------------------------------------------------ */

/* 24 4116s in three rows of 8 (data bit 0 in C3/D3/E3 to bit 7 in C10/D10/E10; row C is $0000-$3FFF). The CPU address reaches a
 * chip as two 7 bit halves on RA0-RA5 and CRA6, one with AX high (A7 A2 A0 A8 A3 A1 and A12: the chips take this as their row, /RAS
 * having fallen by then) and one with AX low (A10 A4 A5 A11 A9 A6 and A13: the column). In the dram_cycle() layout the cell is
 * (row << 8) | column, each half's bits being RA5, RA3, RA4, RA2, RA1, RA0, CRA6 from the lowest. */
static int a2_dram_addr(const int bits[7]) {
    /* bits: the values on RA0, RA1, RA2, RA3, RA4, RA5, CRA6 */
    return bits[5] | bits[3] << 1 | bits[4] << 2 | bits[2] << 3 | bits[1] << 4 | bits[0] << 5 | bits[6] << 6;
}
static int a2_cell(uint16_t a) {
    int row[7] = { a >> 7 & 1, a >> 2 & 1, a & 1, a >> 8 & 1, a >> 3 & 1, a >> 1 & 1, a >> 12 & 1 };
    int col[7] = { a >> 10 & 1, a >> 4 & 1, a >> 5 & 1, a >> 11 & 1, a >> 9 & 1, a >> 6 & 1, a >> 13 & 1 };
    return a2_dram_addr(row) << 8 | a2_dram_addr(col);
}

static int a2_ram_part(board_t *b, uint16_t addr, int bit) {
    static const char rows[3] = { 'C', 'D', 'E' };
    int bank = addr >> 14;
    if (bank > 2) return -1;
    char ref[8];
    snprintf(ref, sizeof(ref), "%c%d", rows[bank], 3 + bit);
    return sim_find_part(&b->sim, ref);
}

uint8_t apple2plus_ram_peek(board_t *b, uint16_t addr) {
    uint8_t v = 0;
    int cell = a2_cell(addr);
    for (int bit = 0; bit < 8; bit++) {
        int p = a2_ram_part(b, addr, bit);
        if (p >= 0) v |= (uint8_t)(((dram_state_t *)b->sim.parts[p].state)->cells[cell] << bit);
    }
    return v;
}

static void a2_print_text(board_t *b) {
    for (int y = 0; y < 24; y++) {
        char line[41];
        uint16_t base = (uint16_t)(0x400 + (y & 7) * 0x80 + (y >> 3) * 0x28);
        for (int x = 0; x < 40; x++) {
            int c = apple2plus_ram_peek(b, (uint16_t)(base + x)) & 0x7F;
            line[x] = (char)(c < 0x20 ? c + 0x40 : c);
        }
        line[40] = 0;
        printf("%s\n", line);
    }
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

static const file_kind_t *a2_file_kind(const char *path, char *info, int len, bool *bad) {
    return board_state_file_kind(path, info, len, bad);
}
static bool a2_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)b; (void)run;
    snprintf(msg, (size_t)msglen, "%s: the Apple ][+ takes no files yet (a disk drive is not built)", path);
    return false;
}
static void a2_destroy(board_t *b) { (void)b; }
static const char *a2_cpu_status(board_t *b) {
    const apple2plus_t *z = A2(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if ((z->rom_mask & 0x3F) != 0x3F) return "no ROMs (put them in roms/apple2plus)";
    if (!(z->rom_mask & 0x40)) return "no character ROM (put it in roms/apple2plus)";
    return "running";
}
static void a2_cpu_regs(board_t *b, char *buf, int len) { pet_cpu_regs_of(&b->sim.parts[A2(b)->p_cpu], buf, len); }

const machine_t machine_apple2plus = {
    .id = "apple2plus",
    .name = "Apple ][+",
    .board_name = "Apple ][+ motherboard rev 0",
    .issue = "II+",
    .size = sizeof(apple2plus_t),
    .data = &bd_apple2plus,
    .sim = { .rail_of = a2_rail_of, .kind_of = a2_kind_of, .cycles_per_us = 1.0227 },
    .cpu_hz = A2_FREQ,
    .cycles_per_frame = A2_CYCLES_PER_FRAME,
    .steps_per_cycle = A2_STEPS_PER_CYCLE,
    .screen_w = A2_SCREEN_W, .screen_h = A2_SCREEN_H,
    .ready_cycles = 2 * A2_FREQ,
    .parts = a2_part_defs, .nparts = sizeof(a2_part_defs) / sizeof(a2_part_defs[0]),
    .conns = a2_conn_defs, .nconns = sizeof(a2_conn_defs) / sizeof(a2_conn_defs[0]),
    .link_resistors = "R14",
    .crystals = "Y1",
    .diodes = "", .jumpers = "", .supplies = "+12V -5V -12V",
    .keys_help = "Keys: type (capitals only); Esc = ESC, Backspace or Left = back arrow",
    .reset_help = "Hold to press RESET: shorts the 6502 /RESET (U3 pin 40) to GND",
    .create = a2_create,
    .destroy = a2_destroy,
    .power_on = a2_power_on,
    .run = a2_run,
    .joins = a2_joins,
    .key_down = a2_key_down,
    .key_up = a2_key_up,
    .host_key = a2_host_key,
    .host_char = a2_host_char,
    .monitor = a2_monitor,
    .files_title = "Open a save state",
    .file_kind = a2_file_kind,
    .load = a2_load,
    .cpu_status = a2_cpu_status,
    .cpu_regs = a2_cpu_regs,
    .print_text = a2_print_text,
};
