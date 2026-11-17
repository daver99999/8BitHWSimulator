/* pet.c - the Commodore PET 4032 / 8032 (8032080-03 universal dynamic
 * board, logic diagram 8032081 rev E).
 *
 * The board makes its own clock: the 16 MHz crystal Y1 (modelled as a
 * source on its OSC_C leg, PT_XTAL16) feeds the 74S04 UE4, the 74177
 * counters UD3, UE6 and UE7 divide it into the 8, 4, 2 and 1 MHz clocks and
 * the character addresses, the 74LS164 UE3 makes the phase signals that
 * time the RAM and the video. The CPU's phi0 input is CLK1, 1 MHz, from
 * that chain. Nothing in this file drives a bus cycle: a step of the
 * board is half a period of the crystal (31 ns), the cores of the 6502,
 * 6520, 6522 and 6545 (pet_chips.c) watch their own clock pins.
 *
 * sim.cycle counts microseconds (CPU cycles); every one is 32 steps.
 * The two boards differ in the links: the 4032 (40 columns: links 1 and
 * 3 closed, the character clock is the 8 MHz CLK8) and the 8032 (80
 * columns: the 16 MHz clock, the second half of the display RAM UC6/UC7).
 * They are separate netlists, bd_pet4032 and bd_pet8032. */
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

#define RAM_CHIP(ref, label) { ref, &PT_4116, label, ROLE_RAM, "4116\n16Kx1", "4116 DRAM, 16K x 1 bit of the 32K RAM" }
#define ROM_CHIP(ref, label, range, what) \
    { ref, &PT_2332, label, ROLE_ROM, "2332\n" range, "ROM socket " ref " (" range "): " what }
#define GLUE(ref, type, label, what) { ref, type, label, ROLE_GLUE, NULL, what }

static const part_def_t pet_part_defs[] = {
    { "UB14", &PT_PET6502, "CPU", ROLE_CHIP, "6502", "6502 CPU, 1 MHz: phi0 is CLK1 from the 74177 chain" },
    { "UB12", &PT_PET6520, "PIA1", ROLE_CHIP, "6520\n$E810", "6520 PIA 1: keyboard scan (port A), key rows in (port B), cassette" },
    { "UB16", &PT_PET6520, "PIA2", ROLE_CHIP, "6520\n$E820", "6520 PIA 2: IEEE-488 data lines, handshake" },
    { "UB15", &PT_PET6522, "VIA", ROLE_CHIP, "6522\n$E840", "6522 VIA: user port, IEEE-488 control lines, cassette 2, CB2 sound" },
    { "UB13", &PT_PET6545, "CRTC", ROLE_CHIP, "6545\n$E880", "6545 CRT controller: display addresses, DE, HSYNC, VSYNC" },
    ROM_CHIP("UD6", "ROM F", "$F000-$FFFF", "KERNAL"),
    ROM_CHIP("UD7", "ROM E", "$E000-$E7FF", "screen editor"),
    ROM_CHIP("UD8", "ROM D", "$D000-$DFFF", "BASIC 4"),
    ROM_CHIP("UD9", "ROM C", "$C000-$CFFF", "BASIC 4"),
    ROM_CHIP("UD10", "ROM B", "$B000-$BFFF", "BASIC 4"),
    ROM_CHIP("UD11", "ROM A", "$A000-$AFFF", "expansion ROM"),
    ROM_CHIP("UD12", "ROM 9", "$9000-$9FFF", "expansion ROM"),
    { "UA3", &PT_2316, "CHAR", ROLE_ROM, "2316\ncharacters", "2316 character generator ROM: 128 characters of 8 x 8 dots" },
    RAM_CHIP("UA4", "RAM"), RAM_CHIP("UA5", "RAM"), RAM_CHIP("UA6", "RAM"), RAM_CHIP("UA7", "RAM"),
    RAM_CHIP("UA8", "RAM"), RAM_CHIP("UA9", "RAM"), RAM_CHIP("UA10", "RAM"), RAM_CHIP("UA11", "RAM"),
    RAM_CHIP("UA12", "RAM"), RAM_CHIP("UA13", "RAM"), RAM_CHIP("UA14", "RAM"), RAM_CHIP("UA15", "RAM"),
    RAM_CHIP("UA16", "RAM"), RAM_CHIP("UA17", "RAM"), RAM_CHIP("UA18", "RAM"), RAM_CHIP("UA19", "RAM"),
    { "UC4", &PT_2114_PET, "VRAM", ROLE_VRAM, "2114\nscreen\nbits 0-3", "2114 video RAM, even addresses, bits 0-3" },
    { "UC5", &PT_2114_PET, "VRAM", ROLE_VRAM, "2114\nscreen\nbits 4-7", "2114 video RAM, even addresses, bits 4-7" },
    { "UC6", &PT_2114_PET, "VRAM", ROLE_VRAM, "2114\nscreen\nodd 0-3", "2114 video RAM, odd addresses, bits 0-3 (80 columns)" },
    { "UC7", &PT_2114_PET, "VRAM", ROLE_VRAM, "2114\nscreen\nodd 4-7", "2114 video RAM, odd addresses, bits 4-7 (80 columns)" },
    { "UE4", &PT_74S04_OSC, "74S04", ROLE_CLOCK, "oscillator", "74S04 oscillator: the crystal Y1 is in its feedback" },
    { "Y1", &PT_XTAL16, "16MHz", ROLE_CLOCK, "crystal", "16 MHz crystal (modelled as a clock source)" },
    { "UD16", &PT_LM555, "LM555", ROLE_CLOCK, "reset", "LM555 power-on reset timer: holds /RES low for a while" },
    { "UD3", &PT_74177, "74177", ROLE_GLUE, "clock\ndivider", "74177 counter: 16 MHz to 8, 4, 2 and 1 MHz" },
    { "UE6", &PT_74177, "74177", ROLE_GLUE, "RA\ncounter", "74177 counter: refresh addresses RA1-RA4" },
    { "UE7", &PT_74177, "74177", ROLE_GLUE, "RA\ncounter", "74177 counter: refresh addresses RA4-RA7" },
    { "UE3", &PT_74LS164, "74LS164", ROLE_GLUE, "phase\nshift", "74LS164 shift register: the RAM and video phase signals" },
    { "UA2", &PT_74166, "74166", ROLE_GLUE, "video\nshift reg", "74166 video shift register: character dots to the screen" },
    GLUE("UB1", &PT_74LS74R, "74LS74", "74LS74: display enable delays"),
    GLUE("UB2", &PT_74LS74R, "74LS74", "74LS74: display enable delays"),
    GLUE("UC1", &PT_74S74, "74S74", "74S74: video timing"),
    GLUE("UE1", &PT_74S74, "74S74", "74S74: RAM timing"),
    GLUE("UB3", &PT_74LS373, "74LS373", "74LS373: video latch (the character ROM address)"),
    GLUE("UB8", &PT_74LS373, "74LS373", "74LS373: video latch, odd display RAM (80 columns)"),
    GLUE("UB4", &PT_74LS244, "74LS244", "74LS244: display RAM data to the CPU"),
    GLUE("UB5", &PT_74LS244, "74LS244", "74LS244: CPU data to the display RAM"),
    GLUE("UB6", &PT_74LS244, "74LS244", "74LS244: display RAM data to the CPU (odd, 80 columns)"),
    GLUE("UB7", &PT_74LS244, "74LS244", "74LS244: CPU data to the display RAM (odd, 80 columns)"),
    GLUE("UB9", &PT_74LS244, "74LS244", "74LS244: data bus buffer, bits 4-7"),
    GLUE("UB10", &PT_74LS244, "74LS244", "74LS244: data bus buffer, bits 0-3"),
    GLUE("UD13", &PT_74LS244, "74LS244", "74LS244: address buffer"),
    GLUE("UD14", &PT_74LS244, "74LS244", "74LS244: address buffer"),
    GLUE("UE8", &PT_74LS244, "74LS244", "74LS244: address buffer"),
    GLUE("UE9", &PT_74LS244, "74LS244", "74LS244: address buffer"),
    GLUE("UE10", &PT_74LS244, "74LS244", "74LS244: address buffer"),
    GLUE("UC8", &PT_74LS157, "74LS157", "74LS157: RAM address multiplexer"),
    GLUE("UC9", &PT_74LS157, "74LS157", "74LS157: RAM address multiplexer"),
    GLUE("UC10", &PT_74LS157, "74LS157", "74LS157: RAM address multiplexer"),
    GLUE("UE2", &PT_74LS157, "74LS157", "74LS157: display RAM address multiplexer"),
    GLUE("UC3", &PT_74LS138, "74LS138", "74LS138: display RAM read/write enables"),
    GLUE("UC11", &PT_74LS145, "74LS145", "74LS145: keyboard row select (open collector)"),
    GLUE("UE12", &PT_74154, "74154", "74154: address decoder, one 4K page per output"),
    GLUE("UC2", &PT_74LS86, "74LS86", "74LS86: video inversion and gating"),
    GLUE("UD1", &PT_74LS02, "74LS02", "74LS02"),
    GLUE("UD4", &PT_74LS10, "74LS10", "74LS10"),
    GLUE("UE13", &PT_74LS10, "74LS10", "74LS10: RAM and ROM decode"),
    GLUE("UD5", &PT_74LS00, "74LS00", "74LS00"),
    GLUE("UE5", &PT_74LS00, "74LS00", "74LS00: RAS and CAS enables"),
    GLUE("UE11", &PT_74LS00, "74LS00", "74LS00: RAM on, BA15, read/write enables"),
    GLUE("UE15", &PT_74LS04, "74LS04", "74LS04"),
    GLUE("UE14", &PT_7425, "7425", "7425: the E8xx decode"),
    GLUE("UD15", &PT_7417, "7417", "7417: open collector buffers"),
    GLUE("UD2", &PT_74S04, "74S04", "74S04: 16 MHz and the load pulse"),
    GLUE("UA20", &PT_MC3446, "MC3446", "MC3446: IEEE-488 bus transceiver (data lines)"),
    GLUE("UB17", &PT_MC3446, "MC3446", "MC3446: IEEE-488 bus transceiver (data lines)"),
    GLUE("UC12", &PT_MC3446, "MC3446", "MC3446: IEEE-488 bus transceiver (control lines)"),
};

/* The connectors: what each pin is for (reference, name, "PAD=FUNCTION"). */
static const conn_def_t pet_conn_defs[] = {
    { "J1", "IEEE-488", "1=DIO1 2=DIO2 3=DIO3 4=DIO4 5=EOI 6=DAV 7=NRFD 8=NDAC 9=IFC 10=SRQ 11=ATN 12=CHASSIS 13=DIO5 "
                        "14=DIO6 15=DIO7 16=DIO8 17=GND 18=GND 19=GND 20=GND 21=GND 22=GND 23=GND 24=GND" },
    { "J2", "USER PORT", "1=GND 2=VIDEO 3=SRQ 4=EOI 5=DIAG 6=CASS_READ2 7=CASS_WRITE 8=CASS_READ1 9=VERT_DRIVE "
                         "10=HORIZ_DRIVE 11=GRAPHIC 12=GND 13=GND 14=CA1 15=PA0 16=PA1 17=PA2 18=PA3 19=PA4 "
                         "20=PA5 21=PA6 22=PA7 23=CB2 24=GND" },
    { "J3", "CASSETTE 1", "1=GND 2=+5V 3=MOTOR 4=READ 5=WRITE 6=SENSE" },
    { "J6", "CASSETTE 2", "1=GND 2=+5V 3=MOTOR 4=READ 5=WRITE 6=SENSE" },
    { "J4", "EXPANSION", "2=BD0 3=BD1 4=BD2 5=BD3 6=BD4 7=BD5 8=BD6 9=BD7 12=SEL4 13=SEL5 14=SEL6 15=SEL7 16=SEL8 "
                         "17=SEL9 18=SELA 19=SELB 20=NO_ROM 22=RES 23=RDY 24=NMI" },
    { "J9", "BUS", "2=BA0 3=BA1 4=BA2 5=BA3 6=BA4 7=BA5 8=BA6 9=BA7 10=BA8 11=BA9 12=BA10 13=BA11 14=BA12 15=BA13 "
                   "16=BA14 17=BA15 18=SYNC 19=IRQ 20=DIAG 21=PHI2 22=R/W 23=R/W_N" },
    { "J8", "KEYBOARD", "1=ROW0 2=ROW1 3=ROW2 4=ROW3 5=ROW4 6=ROW5 7=ROW6 8=ROW7 9=ROW8 10=ROW9 12=GND 13=KIN0 14=KIN1 "
                        "15=KIN2 16=KIN3 17=KIN4 18=KIN5 19=KIN6 20=KIN7" },
    { "J7", "POWER", "1=+12V 2=-5V 3=+9V" },
};

static int pet_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int pet_kind_of(const char *ref) {
    if (!strcmp(ref, "UB11")) return PART_RESISTOR;     /* the key-in pull-up array */
    if (ref[0] == 'U') return PART_IC;
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    if ((!strncmp(ref, "CR", 2) || !strncmp(ref, "VR", 2)) && ref[2] >= '0' && ref[2] <= '9') return PART_RESISTOR;   /* the power section */
    if (ref[0] == 'J') return PART_CONNECTOR;
    return PART_OTHER;
}

/* ------------------------------------------------------------------------ */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------ */

/* The sockets, in the order of pet_t.rom. */
static const char *const rom_refs[7] = { "UD6", "UD7", "UD8", "UD9", "UD10", "UD11", "UD12" };

bool pet_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n) {
    pet_t *p = PET(b);
    if (!strcmp(ref, "UA3")) {
        if (n != 2048) return false;
        memcpy(p->chars, data, n);
        p->rom_mask |= 0x80;
        return true;
    }
    for (int i = 0; i < 7; i++)
        if (!strcmp(ref, rom_refs[i])) {
            if (n != 4096 && n != 2048) return false;
            memset(p->rom[i], 0xFF, 4096);
            memcpy(p->rom[i], data, n);
            if (n == 2048) memcpy(p->rom[i] + 2048, data, n);   /* the editor: 2K in a 4K socket */
            p->rom_mask |= 1u << i;
            return true;
        }
    return false;
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static int pet_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

static void pet_keys_register(pet_t *z);

/* Which socket a ROM image goes in, from the Commodore part number or the
 * usual name in its file name (BASIC 4, KERNAL 4, the 40 or 80 column
 * editor, the character set). NULL if the name says nothing. For the
 * editor, *other is set if it is the one of the other board. */
static bool ci_contains(const char *s, const char *key) {
    size_t n = strlen(key);
    for (; *s; s++)
        if (!strncasecmp(s, key, n)) return true;
    return false;
}

static const char *pet_rom_ref(const char *path, bool cols80, bool *other) {
    static const struct { const char *key, *ref; int ed; } map[] = {
        { "901465-22", "UD6", 0 }, { "901465-23", "UD10", 0 }, { "901465-20", "UD9", 0 },
        { "901465-21", "UD8", 0 }, { "901447-10", "UA3", 0 }, { "901499-01", "UD7", 40 },
        { "901474-03", "UD7", 80 }, { "characters", "UA3", 0 }, { "kernal-4", "UD6", 0 },
        { "edit-4-40", "UD7", 40 }, { "edit-4-80", "UD7", 80 },
    };
    const char *name = board_basename(path);
    if (other) *other = false;
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (!ci_contains(name, map[i].key)) continue;
        if (map[i].ed && other) *other = (map[i].ed == 80) != cols80;
        return map[i].ref;
    }
    return NULL;
}

/* A ROM file into its socket: false and a message if it is not one. */
static bool pet_fit_rom(board_t *b, const char *path, char *msg, int msglen) {
    bool other;
    const char *ref = pet_rom_ref(path, PET(b)->cols80, &other);
    if (!ref) { snprintf(msg, (size_t)msglen, "%s: don't know which ROM this is", path); return false; }
    uint8_t buf[4097];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(buf, 1, sizeof(buf), f) : 0;
    if (f) fclose(f);
    if (!pet_set_rom(b, ref, buf, n)) {
        snprintf(msg, (size_t)msglen, "%s: %zu bytes don't fit socket %s", path, n, ref);
        return false;
    }
    snprintf(msg, (size_t)msglen, "%s fitted in %s%s", board_basename(path), ref,
             other ? " (an editor for the other column count)" : "");
    return true;
}

/* At start: the ROMs found in roms/pet, roms or the current folder. Both
 * editors may be there: the board takes the one of its column count. */
static void pet_find_roms(board_t *b) {
    static const char *const dirs[] = { "roms/pet", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir))) {
            size_t l = strlen(e->d_name);
            if (l < 5 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom"))) continue;
            char path[1024], msg[256];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], e->d_name);
            bool other;
            const char *ref = pet_rom_ref(path, PET(b)->cols80, &other);
            if (!ref || other) continue;
            int i;
            for (i = 0; i < 7 && strcmp(ref, rom_refs[i]); i++) ;
            unsigned bit = i < 7 ? 1u << i : 0x80;
            if (PET(b)->rom_mask & bit) continue;       /* an earlier folder had it */
            pet_fit_rom(b, path, msg, sizeof(msg));
        }
        closedir(dir);
    }
}

static void pet_create(board_t *b) {
    pet_t *z = PET(b);
    sim_t *s = &b->sim;
    z->cols80 = b->m == &machine_pet8032;
    z->chiclet = !z->cols80 && pet_chiclet;
    z->p_cpu = sim_find_part(s, "UB14");
    z->p_pia1 = sim_find_part(s, "UB12");
    z->p_pia2 = sim_find_part(s, "UB16");
    z->p_via = sim_find_part(s, "UB15");
    z->p_crtc = sim_find_part(s, "UB13");
    z->p_xtal = sim_find_part(s, "Y1");
    z->p_char = sim_find_part(s, "UA3");
    memset(z->rom, 0xFF, sizeof(z->rom));
    memset(z->chars, 0, sizeof(z->chars));
    z->rom_mask = 0;
    for (int i = 0; i < 7; i++) {
        int idx = sim_find_part(s, rom_refs[i]);
        if (idx >= 0) ((rom_state_t *)s->parts[idx].state)->data = z->rom[i];
    }
    if (z->p_char >= 0) ((rom_state_t *)s->parts[z->p_char].state)->data = z->chars;
    pet_find_roms(b);
    z->pin_12v = pet_pin_of(b, "J7", 1);
    z->pin_m5v = pet_pin_of(b, "J7", 2);
    z->pin_9v = pet_pin_of(b, "J7", 3);
    z->pin_video = pet_pin_of(b, "J2", 2);
    z->pin_hsync = pet_pin_of(b, "UB13", 39);
    z->pin_vsync = pet_pin_of(b, "UB13", 40);
    z->pin_shift_clk = pet_pin_of(b, "UA2", 7);     /* the video shift register's clock */
    for (int r = 0; r < 10; r++) z->pin_krow[r] = pet_pin_of(b, "J8", r + 1);
    for (int c = 0; c < 8; c++) z->pin_kin[c] = pet_pin_of(b, "J8", 13 + c);
    for (int bit = 0; bit < 8; bit++) {     /* UA4/UA5 hold D7 ... UA18/UA19 D0; the odd ones are the lower 16K */
        char ref[8];
        snprintf(ref, sizeof(ref), "UA%d", 18 - 2 * bit);
        z->ram_part[1][bit] = sim_find_part(&b->sim, ref);
        snprintf(ref, sizeof(ref), "UA%d", 19 - 2 * bit);
        z->ram_part[0][bit] = sim_find_part(&b->sim, ref);
    }
    b->pin_reset = pet_pin_of(b, "UB14", 40);       /* 6502 /RES to VSS, as an external reset switch would */
    b->pin_reset_gnd = pet_pin_of(b, "UB14", 21);
    pet_keys_register(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
}

static void pet_power_on(board_t *b) {
    pet_t *z = PET(b);
    sim_t *s = &b->sim;
    s->clock_level = 0;
    z->beam_x = z->beam_y = 0;
    z->vsync_prev = z->hsync_prev = z->shift_clk_prev = 0;
    memset(b->screen, 0, (size_t)PET_SCREEN_W * PET_SCREEN_H * sizeof(uint32_t));
    sim_drive(s, z->pin_12v, DRIVE(S_SUPPLY, 1));
    sim_drive(s, z->pin_m5v, DRIVE(S_SUPPLY, 0));
    sim_drive(s, z->pin_9v, DRIVE(S_SUPPLY, 1));
}

/* The power section (sheet 11). The rails come from the pseudo connector J7;
 * whether they are there depends on the parts that make them on the real
 * board: the bridges CR2 (the negative supply) and CR4 (+12V's raw supply),
 * the regulators VR3/VR4 (+5V, two in parallel), VR2 (+12V) and
 * VR1 (-5V), and the protection diodes across them (a shorted one ties the
 * output to the input). The 4116s need all three of +5V, +12V and -5V. */
static bool pet_broken(board_t *b, const char *ref) {
    int i = sim_find_part(&b->sim, ref);
    if (i < 0) return false;
    const part_t *p = &b->sim.parts[i];
    return p->removed || (p->rfault != RF_OK && p->rfault != RF_WEAK);
}
static bool pet_weak(board_t *b, const char *ref) {
    int i = sim_find_part(&b->sim, ref);
    return i >= 0 && b->sim.parts[i].rfault == RF_WEAK;
}

static void pet_power(board_t *b) {
    sim_t *s = &b->sim;
    pet_t *z = PET(b);
    bool v9 = true;     /* +9V UNREG comes straight from the raw power connector */
    bool reg5 = !pet_broken(b, "VR3") || !pet_broken(b, "VR4");
    bool v5 = v9 && reg5 && !pet_broken(b, "CR6") && !pet_broken(b, "CR7");
    bool p12 = !pet_broken(b, "CR4") && !pet_broken(b, "VR2") && !pet_broken(b, "CR5");
    bool m5 = !pet_broken(b, "CR2") && !pet_broken(b, "VR1") && !pet_broken(b, "CR1");
    bool sag = (p12 && pet_weak(b, "VR2")) || (m5 && pet_weak(b, "VR1")) ||
               (v5 && (pet_weak(b, "VR3") || pet_weak(b, "VR4")));
    sim_drive(s, z->pin_12v, p12 ? DRIVE(S_SUPPLY, 1) : DRV_Z);
    sim_drive(s, z->pin_m5v, m5 ? DRIVE(S_SUPPLY, 0) : DRV_Z);
    sim_drive(s, z->pin_9v, v9 ? DRIVE(S_SUPPLY, 1) : DRV_Z);
    for (int i = 0; i < s->nparts; i++) {
        part_t *p = &s->parts[i];
        if (p->kind != PART_IC) continue;
        bool dram = p->commit == PT_4116.commit;
        p->no_supply = !v5 || (dram && (!p12 || !m5));
        p->supply_weak = sag && dram;
    }
}

static void pet_joins(board_t *b) {
    sim_t *s = &b->sim;
    pet_power(b);
    /* A closed key joins its row to its column (unless a pin is open). */
    pet_t *z = PET(b);
    for (int row = 0; row < 10; row++)
        for (int col = 0; col < 8; col++) {
            if (!((z->key_matrix[row] >> col) & 1)) continue;
            int pr = z->pin_krow[row], pc = z->pin_kin[col];
            if (pr < 0 || pc < 0 || s->pins[pr].fault == PF_OPEN || s->pins[pc].fault == PF_OPEN) continue;
            sim_join(s, s->pins[pr].net, s->pins[pc].net);
        }
}

/* The picture: what a monitor would show. The beam goes right at every
 * rising edge of the video shift register's clock (one dot: 8 MHz on the
 * 40 column board, 16 MHz on the 80 column one), back to the left when
 * HSYNC rises and up when VSYNC rises (a monitor syncs on the leading
 * edge: the CRTC's sync pulse can run into the next line). A dot is lit
 * when VIDEO is high. */
#define PET_LIT  0xFF60F060u
#define PET_DARK 0xFF001400u
static void pet_beam(board_t *b) {
    pet_t *z = PET(b);
    sim_t *s = &b->sim;
    int hs = sim_read(s, z->pin_hsync), vs = sim_read(s, z->pin_vsync), ck = sim_read(s, z->pin_shift_clk);
    if (hs && !z->hsync_prev) { z->beam_x = 0; z->beam_y++; }
    if (vs && !z->vsync_prev) z->beam_y = 0;
    z->hsync_prev = hs;
    z->vsync_prev = vs;
    if (ck && !z->shift_clk_prev) {
        uint32_t c = sim_read(s, z->pin_video) ? PET_LIT : PET_DARK;
        int scale = z->cols80 ? 1 : 2, x = z->beam_x * scale;
        if (z->beam_y >= 0 && z->beam_y < PET_SCREEN_H)
            for (int k = 0; k < scale && x + k < PET_SCREEN_W; k++) b->screen[z->beam_y * PET_SCREEN_W + x + k] = c;
        z->beam_x++;
    }
    z->shift_clk_prev = ck;
}

/* One half period of the crystal. */
static void pet_step(board_t *b) {
    pet_t *z = PET(b);
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, z->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    pet_beam(b);
    board_sample(b);
}

void pet_run_steps(board_t *b, int steps) {
    sim_t *s = &b->sim;
    uint32_t *phase = &PET(b)->step_in_cycle;
    for (int i = 0; i < steps; i++) {
        if (*phase == 0) s->cycle++;
        pet_step(b);
        if (++*phase == PET_STEPS_PER_CYCLE) *phase = 0;
    }
}

static void pet_run(board_t *b, int cycles) {
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        for (int k = 0; k < PET_STEPS_PER_CYCLE; k++) pet_step(b);
    }
}

uint32_t pet_cpu_cycles(board_t *b) { return pet_cpu_cycles_of(&b->sim.parts[PET(b)->p_cpu]); }

uint8_t pet_vram_peek(board_t *b, int addr) {
    pet_t *z = PET(b);
    sim_t *s = &b->sim;
    const char *lo = "UC4", *hi = "UC5";
    if (z->cols80) {
        if (addr & 1) { lo = "UC6"; hi = "UC7"; }
        addr >>= 1;
    }
    int a = sim_find_part(s, lo), c = sim_find_part(s, hi);
    if (a < 0 || c < 0) return 0;
    return (uint8_t)(((sram_state_t *)s->parts[a].state)->cells[addr & 1023] |
                     ((sram_state_t *)s->parts[c].state)->cells[addr & 1023] << 4);
}

/* ------------------------------------------------------------------------ */
/* The rest of the machine interface                                         */
/* ------------------------------------------------------------------------ */

/* The keyboard. Rows 0-9 are the 74145's outputs, columns 0-7 PIA1's port B
 * (KIN0-7, pulled up on the board). The tables name the key of every
 * crosspoint, columns 7 down to 0 as the PET keyboard documentation lists
 * them. A single character is the key's own cap, "~" is no key (or one we
 * don't type: the keypad of the business keyboard, the control characters).
 * The graphics keyboard (4032) has its digits on the keypad and ! " # $ % ' & \ ( )
 * on the top row; the business keyboard (8032) is a typewriter's. Source:
 * masswerk.at/nowgobang/2023/pet-keys-2001-edition (the chiclet matrix; the 2001N's
 * graphics keyboard is said to share it: verify against the editor ROM). */
#define PK_RETURN 0x0D
#define PK_DEL    0x01
#define PK_UP     0x0B
#define PK_DOWN   0x0A
#define PK_RIGHT  0x09
#define PK_LEFT   0x08
#define PK_HOME   0x0C
#define PK_CLR    0x02
#define PK_STOP   0x03
#define PK_RVS    0x12
#define PK_LARROW 0x1B
#define PK_ESC    0x1F
#define PK_TAB    0x14
#define PK_RPT    0x15
#define PK_RSHIFT 0x16
#define PK_SPACE  0x20

static const char *const pet_keys_graphics[10][8] = {
    { "RIGHT", "HOME", "LARROW", "(", "&", "%", "#", "!" },
    { "DEL", "DOWN", "~", ")", "\\", "'", "$", "\"" },
    { "9", "7", "UP", "O", "U", "T", "E", "Q" },
    { "/", "8", "~", "P", "I", "Y", "R", "W" },
    { "6", "4", "~", "L", "J", "G", "D", "A" },
    { "*", "5", "~", ":", "K", "H", "F", "S" },
    { "3", "1", "RETURN", ";", "M", "B", "C", "Z" },
    { "+", "2", "~", "?", ",", "N", "V", "X" },
    { "-", "0", "RSHIFT", ">", "~", "]", "@", "LSHIFT" },
    { "=", ".", "~", "STOP", "<", "SPACE", "[", "RVS" },
};
static const char *const pet_keys_business[10][8] = {
    /* from the 901474-03 editor key table: plain ASCII digits = main row (shift -> symbols),
       $B0-$B9 codes = numeric keypad (K#, exempt from shift) */
    { "~", "~", "RIGHT", "K8", "-", "8", "5", "2" },
    { "K9", "~", "UP", "K7", "0", "7", "4", "1" },
    { "K5", ";", "K", "]", "H", "F", "S", "ESC" },
    { "K6", "@", "L", "RETURN", "J", "G", "D", "A" },
    { "DEL", "P", "I", "\\", "Y", "R", "W", "TAB" },
    { "K4", "[", "O", "DOWN", "U", "T", "E", "Q" },
    { "K3", "RSHIFT", "~", "~", ".", "B", "C", "LSHIFT" },
    { "K2", "RPT", "~", "K0", ",", "N", "V", "Z" },
    { "K1", "/", "~", "HOME", "M", "SPACE", "X", "RVS" },
    { "~", "~", ":", "STOP", "9", "6", "3", "LARROW" },
};

/* The key at a place of the key matrix (row 0-9, column 0-7 as scanned), for the keyboard map window. */
const char *pet_key_legend(bool cols80, int row, int col) {
    return (cols80 ? pet_keys_business : pet_keys_graphics)[row][col];
}

static void pet_keys_register(pet_t *z) {
    const char *const (*t)[8] = z->cols80 ? pet_keys_business : pet_keys_graphics;
    kbd_init(&z->kbd, 2);
    for (int row = 0; row < 10; row++)
        for (int col = 0; col < 8; col++) {
            const char *n = t[row][7 - col];
            if (!strcmp(n, "LSHIFT")) { kbd_register_modifier(&z->kbd, 0, col, row); continue; }
            int key = 0, shifted = 0;
            if (n[0] == 'K' && n[1] >= '0' && n[1] <= '9' && n[2] == 0) key = 0x80 + (n[1] - '0');
            else if (n[1] == 0) key = (unsigned char)n[0];
            else if (!strcmp(n, "RETURN")) key = PK_RETURN;
            else if (!strcmp(n, "DEL")) key = PK_DEL;
            else if (!strcmp(n, "UP")) key = PK_UP;
            else if (!strcmp(n, "DOWN")) key = PK_DOWN;
            else if (!strcmp(n, "RIGHT")) { key = PK_RIGHT; shifted = PK_LEFT; }
            else if (!strcmp(n, "HOME")) { key = PK_HOME; shifted = PK_CLR; }
            else if (!strcmp(n, "STOP")) key = PK_STOP;
            else if (!strcmp(n, "RVS")) key = PK_RVS;
            else if (!strcmp(n, "LARROW")) key = PK_LARROW;
            else if (!strcmp(n, "ESC")) key = PK_ESC;
            else if (!strcmp(n, "TAB")) key = PK_TAB;
            else if (!strcmp(n, "RPT")) key = PK_RPT;
            else if (!strcmp(n, "RSHIFT")) key = PK_RSHIFT;
            else if (!strcmp(n, "SPACE")) key = PK_SPACE;
            if (!key) continue;
            kbd_register_key(&z->kbd, key, col, row, 0);
            if (key >= 'A' && key <= 'Z') kbd_register_key(&z->kbd, key + 32, col, row, 1);   /* shifted: lower case */
            if (shifted) kbd_register_key(&z->kbd, shifted, col, row, 1);
            if (z->cols80 && n[1] == 0) {   /* business keyboard shifted legends */
                static const char *const from = "1234567890;:,./-", *const to = "!\"#$%&'() +*<>?=";
                const char *q = strchr(from, n[0]);
                if (q && to[q - from] != ' ') kbd_register_key(&z->kbd, (unsigned char)to[q - from], col, row, 1);
            }
        }
}

static void pet_sync_keys(board_t *b) {
    pet_t *z = PET(b);
    bool changed = false;
    for (int row = 0; row < 10; row++) {
        uint16_t m = z->kbd.scanout_column_masks[row];
        if (m != z->key_matrix[row]) { z->key_matrix[row] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

static void pet_key_down(board_t *b, int key) { kbd_key_down(&PET(b)->kbd, key); pet_sync_keys(b); }
static void pet_key_up(board_t *b, int key) { kbd_key_up(&PET(b)->kbd, key); pet_sync_keys(b); }
static void pet_frame_update(board_t *b, uint32_t us) { kbd_update(&PET(b)->kbd, us); pet_sync_keys(b); }

static int pet_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return PK_RETURN;
    case SAPP_KEYCODE_BACKSPACE: return PK_DEL;
    case SAPP_KEYCODE_LEFT: return PK_LEFT;
    case SAPP_KEYCODE_RIGHT: return PK_RIGHT;
    case SAPP_KEYCODE_UP: return PK_UP;
    case SAPP_KEYCODE_DOWN: return PK_DOWN;
    case SAPP_KEYCODE_HOME: return PK_HOME;
    case SAPP_KEYCODE_DELETE: return PK_CLR;
    case SAPP_KEYCODE_ESCAPE: return PK_STOP;
    case SAPP_KEYCODE_END: return PK_RVS;
    case SAPP_KEYCODE_TAB: return PK_TAB;
    case SAPP_KEYCODE_RIGHT_SHIFT: return PK_RSHIFT;
    default: return 0;
    }
}

/* Like the C64, the PET types capitals without shift. */
static int pet_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static void pet_monitor(board_t *b, uint32_t *out) {
    memcpy(out, b->screen, (size_t)PET_SCREEN_W * PET_SCREEN_H * sizeof(uint32_t));
}

/* Main RAM: 16 4116s, one data bit each. The row address is BA0-BA6, the
 * column address BA13 and BA7-BA12 (sheet 5), the bank (lower or upper 16K)
 * BA14; a cell is (row << 8) | column, as dram_cycle() has it. */
static dram_state_t *pet_ram_cell(board_t *b, uint16_t addr, int bit, int *cell) {
    pet_t *z = PET(b);
    int part = z->ram_part[(addr >> 14) & 1][bit];
    if (addr >= 0x8000 || part < 0) return NULL;
    int row = addr & 0x7F, col = ((addr >> 13) & 1) | (((addr >> 7) & 0x3F) << 1);
    *cell = (row << 8) | col;
    return b->sim.parts[part].state;
}

void pet_ram_poke(board_t *b, uint16_t addr, uint8_t v) {
    for (int bit = 0; bit < 8; bit++) {
        int cell;
        dram_state_t *st = pet_ram_cell(b, addr, bit, &cell);
        if (st) st->cells[cell] = (v >> bit) & 1;
    }
}

uint8_t pet_ram_peek(board_t *b, uint16_t addr) {
    uint8_t v = 0;
    for (int bit = 0; bit < 8; bit++) {
        int cell;
        dram_state_t *st = pet_ram_cell(b, addr, bit, &cell);
        if (st) v |= (uint8_t)(st->cells[cell] << bit);
    }
    return v;
}

/* A program file (.prg, or .pet): the load address, then the bytes. BASIC
 * programs start at $0401. */
static const file_kind_t pet_rom = { "ROM image", "Fit in its socket", false, ROLE_ROM };
static const file_kind_t pet_prg = { "program", "Load and RUN", true, -1 };

static const file_kind_t *pet_file_kind(const char *path, char *info, int len, bool *bad) {
    const file_kind_t *state = board_state_file_kind(path, info, len, bad);
    if (state) return state;
    size_t pl = strlen(path);
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        bool other;
        const char *ref = pet_rom_ref(path, false, &other);
        if (!ref) return NULL;
        snprintf(info, (size_t)len, "goes in socket %s", ref);
        return &pet_rom;
    }
    if (pl < 5 || (strcasecmp(path + pl - 4, ".prg") && strcasecmp(path + pl - 4, ".pet"))) return NULL;
    FILE *f = fopen(path, "rb");
    uint8_t h[4] = {0};
    size_t n = f ? fread(h, 1, sizeof(h), f) : 0;
    if (f) fclose(f);
    unsigned addr = n >= 2 ? h[0] | h[1] << 8 : 0;
    *bad = n < 3;
    if (n < 3) snprintf(info, (size_t)len, "too short for a program");
    else if (addr == 0x0401) snprintf(info, (size_t)len, "BASIC program");
    else snprintf(info, (size_t)len, "machine code at $%04X", addr);
    return &pet_prg;
}

static bool pet_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    size_t pl = strlen(path);
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        if (!pet_fit_rom(b, path, msg, msglen)) return false;
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
    if (end > 0x8000 || start < 0x0200) {
        free(buf);
        snprintf(msg, (size_t)msglen, "$%04X-$%04X is outside the PET's RAM", start, end);
        return false;
    }
    for (size_t i = 2; i < n; i++) pet_ram_poke(b, (uint16_t)(start + i - 2), buf[i]);
    free(buf);
    bool basic = start == 0x0401;
    if (basic) {    /* BASIC 4: end of the program = start of the variables, arrays, strings ($2A-$2F) */
        for (int p = 0x2A; p <= 0x2E; p += 2) {
            pet_ram_poke(b, (uint16_t)p, end & 0xFF);
            pet_ram_poke(b, (uint16_t)(p + 1), end >> 8);
        }
    }
    if (run) {      /* type RUN (or SYS) <return> through the keyboard buffer ($026F, its count at $9E) */
        char cmd[16];
        snprintf(cmd, sizeof(cmd), basic ? "RUN\r" : "SYS%u\r", start);
        int len = (int)strlen(cmd);
        for (int i = 0; i < len; i++) pet_ram_poke(b, (uint16_t)(0x026F + i), (uint8_t)cmd[i]);
        pet_ram_poke(b, 0x9E, (uint8_t)len);
    }
    snprintf(msg, (size_t)msglen, "loaded %s at $%04X-$%04X", path, start, end - 1);
    return true;
}

static void pet_destroy(board_t *b) { (void)b; }

static const char *pet_cpu_status(board_t *b) {
    const pet_t *z = PET(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!(z->rom_mask & 0x01)) return "no KERNAL ROM (put the PET ROMs in roms/pet)";
    if (!(z->rom_mask & 0x80)) return "no character ROM (put the PET ROMs in roms/pet)";
    return "running";
}

static void pet_cpu_regs(board_t *b, char *buf, int len) {
    pet_cpu_regs_of(&b->sim.parts[PET(b)->p_cpu], buf, len);
}

static void pet_print_text(board_t *b) {
    /* The screen as the display RAM holds it (screen codes to ASCII). */
    pet_t *z = PET(b);
    int cols = z->cols80 ? 80 : 40;
    for (int y = 0; y < 25; y++) {
        char line[96];
        for (int x = 0; x < cols; x++) {
            int c = pet_vram_peek(b, y * cols + x) & 0x7F;
            line[x] = c == 0 ? '@' : c < 27 ? (char)('A' + c - 1) : c >= 32 && c < 64 ? (char)c : '.';
        }
        line[cols] = 0;
        printf("%s\n", line);
    }
}

#define PET_MACHINE \
    .size = sizeof(pet_t), \
    .sim = { .rail_of = pet_rail_of, .kind_of = pet_kind_of, .cycles_per_us = 1.0 }, \
    .cpu_hz = PET_FREQ, \
    .cycles_per_frame = PET_CYCLES_PER_FRAME, \
    .steps_per_cycle = PET_STEPS_PER_CYCLE, \
    .screen_w = PET_SCREEN_W, .screen_h = PET_SCREEN_H, \
    .ready_cycles = 3 * PET_FREQ, \
    .parts = pet_part_defs, .nparts = sizeof(pet_part_defs) / sizeof(pet_part_defs[0]), \
    .conns = pet_conn_defs, .nconns = sizeof(pet_conn_defs) / sizeof(pet_conn_defs[0]), \
    .link_resistors = "R7 R10 R11 R46", \
    .crystals = "Y1", \
    .diodes = "", .jumpers = "", \
    .supplies = "+12V -5V +9V_UNREG", \
    .keys_help = "Keys: type; Esc = STOP, End = RVS, Home/Del = HOME/CLR", \
    .reset_help = "Hold to press RESET: shorts the 6502 /RES (UB14 pin 40) to GND", \
    .create = pet_create, \
    .destroy = pet_destroy, \
    .power_on = pet_power_on, \
    .run = pet_run, \
    .joins = pet_joins, \
    .key_down = pet_key_down, \
    .key_up = pet_key_up, \
    .frame_update = pet_frame_update, \
    .host_key = pet_host_key, \
    .host_char = pet_host_char, \
    .monitor = pet_monitor, \
    .files_title = "Open a program or save state", \
    .file_kind = pet_file_kind, \
    .load = pet_load, \
    .cpu_status = pet_cpu_status, \
    .cpu_regs = pet_cpu_regs, \
    .print_text = pet_print_text

/* Option (4032): which keyboard is fitted. The chiclet keyboard of the 2001 has the graphics keyboard's matrix: only the
 * keys' places and legends differ (the Keyboard window). */
bool pet_chiclet;
static const char *const pet_kbd_values[] = { "Graphics", "Chiclet" };
static const char *const pet_kbd_ids[] = { "graphics", "chiclet" };
static const machine_option_t pet_options[] = {
    { "keyboard", "Keyboard", "The 4032's graphics keyboard, or the chiclet keyboard (same matrix, different key places and legends)",
      pet_kbd_values, pet_kbd_ids, 2 },
};
static int pet_get_option(board_t *b, int option) { (void)option; return PET(b)->chiclet; }
static void pet_set_option(board_t *b, int option, int value) {
    (void)option;
    PET(b)->chiclet = value != 0;
    pet_chiclet = PET(b)->chiclet;
}

const machine_t machine_pet4032 = {
    PET_MACHINE,
    .id = "pet4032",
    .issue = "4032 (40 col)",
    .name = "Commodore PET 4032",
    .board_name = "PET 8032080-03, 40 columns",
    .data = &bd_pet4032,
    .options = pet_options, .noptions = 1,
    .get_option = pet_get_option,
    .set_option = pet_set_option,
};

const machine_t machine_pet8032 = {
    PET_MACHINE,
    .id = "pet8032",
    .issue = "8032 (80 col)",
    .name = "Commodore PET 8032",
    .board_name = "PET 8032080-03, 80 columns",
    .data = &bd_pet8032,
};
