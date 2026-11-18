/* c128.c - the Commodore 128 (schematic 310378 rev 7A), in C128 mode with the 40 column screen.
 *
 * The board runs on the 8564 VIC-IIe (U21), which makes the system clock out of the dot clock of the 8701 (U28):
 * a cycle is 16 steps (half periods of the dot clock), see c128_chips.c. The machine starts in Z80 mode: the 8722
 * MMU (U7) puts the Z80's addresses 4K up, so the Z80 runs the start-up code that is the last 4K of the 32K
 * system ROM (U32), sets up the MMU and hands over to the 8502 (U6) through the bus request logic of U8, U31,
 * U37, U56, U57, U60 and U61, and the latches U12 and U13 that carry the Z80's data. Nothing in this file drives
 * a bus cycle: the chips watch their own clocks. This file has the keyboard, the joysticks, the ROMs and the
 * power-on reset. Not in this release: CP/M. */
#include "c128.h"
#include "sokol/sokol_app.h"   /* key codes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#define GLUE(ref, type, label, what) { ref, type, label, ROLE_GLUE, NULL, what }
#define DRAM(ref, bit, bank) { ref, &PT_4164, "RAM", ROLE_RAM, "4164\nbit D" #bit "\nbank " #bank, \
    "4164 DRAM, holds data bit D" #bit " of every byte of RAM bank " #bank }

static const part_def_t c128_part_defs[] = {
    { "U6", &PT_8502, "8502", ROLE_CHIP, "8502", "8502 CPU, 1 MHz: runs in C128 mode and C64 mode; its P0-P2 select ROM in C64 mode, P6 senses CAPS LOCK" },
    { "U10", &PT_Z80A, "Z80A", ROLE_CHIP, "Z80A", "Z80A CPU: runs at start-up (the 8722 is in Z80 mode) and in CP/M mode, at 2 MHz" },
    { "U7", &PT_8722, "8722 MMU", ROLE_CHIP, "8722 MMU\n$D500-$D50B\n$FF00-$FF04", "8722 MMU: banks the RAM and ROM, relocates pages 0 and 1, makes the Z80's addresses, holds the mode" },
    { "U11", &PT_8721, "8721 PLA", ROLE_PLA, "8721 PLA\nmemory map", "8721 PLA: decodes the chip selects from the processors' address and the MMU's bank lines" },
    { "U21", &PT_8564, "VIC-IIe", ROLE_CHIP, "8564 VIC-IIe\n$D000-$D3FF", "8564 VIC-IIe video chip, $D000-$D02F: 40 column picture, the DRAM strobes and the clocks of the 8502 and the Z80" },
    { "U22", &PT_8563, "VDC", ROLE_CHIP, "8563 VDC\n$D600-$D601", "8563 VDC: the 80 column video chip (its picture is on the RGBI monitor input)" },
    { "U5", &PT_6581_128, "SID", ROLE_CHIP, "6581\n$D400-$D41C", "6581 SID sound chip, $D400-$D7FF" },
    { "U1", &PT_6526_128, "CIA1", ROLE_CHIP, "6526\n$DC00-$DCFF", "6526 CIA #1, $DC00-$DCFF: keyboard, joysticks, IRQ" },
    { "U4", &PT_6526_128, "CIA2", ROLE_CHIP, "6526\n$DD00-$DDFF", "6526 CIA #2, $DD00-$DDFF: serial bus, user port, VIC bank bits, NMI" },
    { "CN6", &PT_IECDRV, "SERIAL", ROLE_OTHER, "serial bus\ndrive 8", "Serial bus connector: ATN, CLK, DATA and SRQ. With a disk image inserted a drive (device 8) answers on it" },
    { "U32", &PT_C128ROM, "SYSROM", ROLE_ROM, "23256 ROM\n252343-04\nsystem", "23256 system ROM (252343-04, 32K): the Z80 start-up, the C64 OS (first half), the C128 KERNAL (second half)" },
    { "U33", &PT_C128ROM, "BASICLO", ROLE_ROM, "23128 ROM\nBASIC 7.0\n$4000-$7FFF", "23128 BASIC 7.0 low (318018-04), $4000-$7FFF" },
    { "U34", &PT_C128ROM, "BASICHI", ROLE_ROM, "23128 ROM\nBASIC 7.0\n$8000-$AFFF", "23128 BASIC 7.0 high (318019-04), $8000-$BFFF" },
    { "U35", &PT_C128ROM, "ROM4", ROLE_ROM, "23128 ROM\n(empty)\n$C000-$FFFF", "23128 socket for the KERNAL on boards with 16K ROMs; not fitted with the 32K system ROM" },
    { "U36", &PT_C128ROM, "FUNCROM", ROLE_ROM, "23256 ROM\nfunction ROM\n(empty)", "23256 socket for the internal function ROM (not fitted)" },
    { "U18", &PT_C128CHR, "CHAR", ROLE_ROM, "390059-01\ncharacters", "390059-01 character ROM: the C64's set (first 4K) and the C128's (second 4K)" },
    { "U19", &PT_6116, "COLORRAM", ROLE_VRAM, "2016\ncolor RAM\n$D800-$DBFF", "2016 color RAM, $D800-$DBFF (4 bits used), read by the VIC every character" },
    DRAM("U38", 0, 0), DRAM("U39", 1, 0), DRAM("U40", 2, 0), DRAM("U41", 3, 0), DRAM("U42", 4, 0), DRAM("U43", 5, 0),
    DRAM("U44", 6, 0), DRAM("U45", 7, 0),
    DRAM("U46", 0, 1), DRAM("U47", 1, 1), DRAM("U48", 2, 1), DRAM("U49", 3, 1), DRAM("U50", 4, 1), DRAM("U51", 5, 1),
    DRAM("U52", 6, 1), DRAM("U53", 7, 1),
    { "U23", &PT_4416, "VDCRAM", ROLE_RAM, "4416\nVDC RAM\nlow", "4416 video RAM of the VDC, data bits 0-3 (not used by the model)" },
    { "U25", &PT_4416, "VDCRAM", ROLE_RAM, "4416\nVDC RAM\nhigh", "4416 video RAM of the VDC, data bits 4-7 (not used by the model)" },
    { "U28", &PT_8701, "8701", ROLE_CLOCK, "8701\nclocks", "8701 clock generator: the dot clock and the color clock from the 14.31818 MHz crystal Y2" },
    GLUE("U2", &PT_4066, "4066", "4066 switch: selects the paddles read by the SID"),
    GLUE("U3", &PT_74LS138, "74LS138", "74LS138 I/O decoder: SID, the VDC, CIA1, CIA2, I/O1, I/O2"),
    GLUE("U8", &PT_74LS08, "74LS08", "74LS08 AND gates: the bus request logic, F_R/W, the Z80's write"),
    GLUE("U9", &PT_74LS32, "74F32", "74F32 OR gates: the RAM /CAS strobes"),
    GLUE("U12", &PT_74LS373, "74LS373", "74LS373: holds the system data for the Z80's reads (ZD0-ZD7)"),
    GLUE("U13", &PT_74LS244, "74LS244", "74LS244: the Z80's data to the system data bus while it writes"),
    GLUE("U14", &PT_74LS257, "74LS257", "74LS257 DRAM address multiplexer: A4-A7 / TA12-TA15"),
    GLUE("U15", &PT_74LS257, "74LS257", "74LS257 DRAM address multiplexer: A0-A3 / TA8-TA11"),
    GLUE("U16", &PT_74LS14, "74LS14", "74LS14 Schmitt inverters (reset, not simulated)"),
    GLUE("U17", &PT_74LS373, "74ALS373", "74ALS373: latches the VIC's address VMA0-5, VA6, VA7 onto SA0-SA7 while it owns the bus"),
    GLUE("U20", &PT_4066, "4066", "4066: connects the color RAM to D0-D3 in the processors' cycles"),
    GLUE("U24", &PT_74LS244, "74LS244", "74LS244: the VDC's RGBI and syncs (not used by the model)"),
    GLUE("U26", &PT_74LS257, "74LS257", "74LS257: the VIC's bank bits VA14-15 or A6-7 on MA6-MA7"),
    GLUE("U27", &PT_556, "556", "556 timer: power-on reset (replaced by the machine's reset)"),
    GLUE("U29", &PT_7406, "7406", "7406 inverters: the dot clock, /AEC"),
    GLUE("U30", &PT_7406, "7406", "7406 open collector drivers: the serial bus"),
    GLUE("U31", &PT_74LS00, "74LS00", "74LS00 NAND gates: the Z80's write strobe /ZWRT, the I/O decoder enable"),
    GLUE("U37", &PT_7406, "7406", "7406: bus request, bus acknowledge"),
    GLUE("U54", &PT_74LS32, "74LS32", "74LS32 OR gates: A4/A5 and A6/A7 to the MMU"),
    GLUE("U55", &PT_74LS245, "74F245", "74F245: the processors' address A0-A7 to the video side"),
    GLUE("U56", &PT_74LS74_128, "74LS74", "74LS74: delays the Z80's /WR to the system clock"),
    GLUE("U57", &PT_7407, "7407", "7407 buffers: system R/W"),
    GLUE("U58", &PT_74LS00, "74LS03", "74LS03 (not used by the model)"),
    GLUE("U60", &PT_7407, "7407", "7407 buffers: /BUSRQ from BA, the Z80's clock"),
    GLUE("U61", &PT_74LS08, "74LS08", "74LS08 AND gates: AEC, RDY, halt of the 8502"),
    GLUE("U62", &PT_74LS244, "74LS244", "74LS244: the translated address TA8-TA15 to the bus for a DMA device"),
    GLUE("U63", &PT_7406, "7406", "7406: VA14, VA15 from CIA2"),
};

static const conn_def_t c128_conn_defs[] = {
    { "CN2", "CASSETTE", "A=GND B=+5V C=MOTOR D=WRITE E=SENSE F=READ" },
    { "CN3", "CONTROL PORT 1", "1=UP 2=DOWN 3=LEFT 4=RIGHT 5=POT_Y 6=FIRE 7=+5V 8=GND 9=POT_X" },
    { "CN4", "CONTROL PORT 2", "1=UP 2=DOWN 3=LEFT 4=RIGHT 5=POT_Y 6=FIRE 7=+5V 8=GND 9=POT_X" },
    { "CN5", "KEYBOARD", "1=KEY 2=KEY 3=RESTORE 4=+5V 5=ROW3 6=ROW6 7=ROW5 8=ROW4 9=ROW7 10=ROW2 11=ROW1 12=ROW0 13=COL0 "
      "14=COL6 15=COL5 16=COL4 17=COL3 18=COL2 19=COL1 20=COL7 21=K0 22=K1 23=K2 24=40/80 25=CAPS_LOCK" },
    { "CN6", "SERIAL", "1=SRQ_IN 2=GND 3=ATN 4=CLK 5=DATA" },
    { "CN9", "USER PORT", "1=GND 2=+5V 7=CNT2 B=/FLAG2 C=PB0 D=PB1 E=PB2 F=PB3 H=PB4 J=PB5 K=PB6 L=PB7 M=PA2" },
};

static int c128_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V") || !strcmp(name, "+9V/+12V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int c128_kind_of(const char *ref) {
    if (ref[0] == 'U') return PART_IC;
    if (ref[0] == 'R') return PART_RESISTOR;
    if (!strncmp(ref, "FB", 2) || ref[0] == 'L' || !strncmp(ref, "EM", 2)) return PART_LINK;
    if (!strncmp(ref, "CN", 2) && ref[2] >= '0' && ref[2] <= '9') return PART_CONNECTOR;
    return PART_OTHER;
}

/* ------------------------------------------------------------------------ */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------ */

static void c128_attach_roms(board_t *b) {
    c128_t *z = C128(b);
    sim_t *s = &b->sim;
    static const struct { const char *ref; } none[] = { { "U36" } };
    int i;
    if ((i = sim_find_part(s, "U32")) >= 0) c128_rom_set(&s->parts[i], z->rom_mask & 1 ? z->rom32 : NULL, 32768);
    /* U35 (ROM4, the KERNAL at $C000 and the Z80's start-up code) reads the second half of the system ROM. */
    if ((i = sim_find_part(s, "U35")) >= 0) c128_rom_set(&s->parts[i], z->rom_mask & 1 ? z->rom32 + 16384 : NULL, 16384);
    if ((i = sim_find_part(s, "U33")) >= 0) c128_rom_set(&s->parts[i], z->rom_mask & 2 ? z->basic_lo : NULL, 16384);
    if ((i = sim_find_part(s, "U34")) >= 0) c128_rom_set(&s->parts[i], z->rom_mask & 4 ? z->basic_hi : NULL, 16384);
    for (size_t k = 0; k < sizeof(none) / sizeof(none[0]); k++)
        if ((i = sim_find_part(s, none[k].ref)) >= 0) c128_rom_set(&s->parts[i], NULL, 16384);
    if ((i = sim_find_part(s, "U18")) >= 0) c128_rom_set(&s->parts[i], z->rom_mask & 8 ? z->chr : NULL, 8192);
}

/* Which socket a ROM image belongs in, from its name; the data and size it takes. */
static const char *c128_rom_ref(const char *path) {
    static const struct { const char *key, *ref; } map[] = {
        { "252343", "U32" }, { "318018", "U33" }, { "318019", "U34" }, { "390059", "U18" },
    };
    const char *name = board_basename(path);
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        size_t n = strlen(map[i].key);
        for (const char *s = name; *s; s++)
            if (!strncasecmp(s, map[i].key, n)) return map[i].ref;
    }
    return NULL;
}

static bool c128_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n) {
    c128_t *z = C128(b);
    if (!strcmp(ref, "U32") && n == 32768) { memcpy(z->rom32, data, n); z->rom_mask |= 1; }
    else if (!strcmp(ref, "U33") && n == 16384) { memcpy(z->basic_lo, data, n); z->rom_mask |= 2; }
    else if (!strcmp(ref, "U34") && n == 16384) { memcpy(z->basic_hi, data, n); z->rom_mask |= 4; }
    else if (!strcmp(ref, "U18") && n == 8192) { memcpy(z->chr, data, n); z->rom_mask |= 8; }
    else return false;
    c128_attach_roms(b);
    return true;
}

static bool c128_fit_rom(board_t *b, const char *path, char *msg, int msglen) {
    const char *ref = c128_rom_ref(path);
    if (!ref) { snprintf(msg, (size_t)msglen, "%s: don't know which ROM this is", path); return false; }
    uint8_t buf[32769];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(buf, 1, sizeof(buf), f) : 0;
    if (f) fclose(f);
    if (!c128_set_rom(b, ref, buf, n)) {
        snprintf(msg, (size_t)msglen, "%s: %zu bytes don't fit socket %s", path, n, ref);
        return false;
    }
    snprintf(msg, (size_t)msglen, "%s fitted in %s", board_basename(path), ref);
    return true;
}

static void c128_find_roms(board_t *b) {
    static const char *const dirs[] = { "roms/c128", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir))) {
            size_t l = strlen(e->d_name);
            if (l < 5 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom"))) continue;
            char path[1024], msg[256];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], e->d_name);
            const char *ref = c128_rom_ref(path);
            if (!ref) continue;
            unsigned bit = !strcmp(ref, "U32") ? 1u : !strcmp(ref, "U33") ? 2u : !strcmp(ref, "U34") ? 4u : 8u;
            if (C128(b)->rom_mask & bit) continue;
            c128_fit_rom(b, path, msg, sizeof(msg));
        }
        closedir(dir);
    }
}

/* ------------------------------------------------------------------------ */
/* The keyboard                                                              */
/* ------------------------------------------------------------------------ */

#define K_SPACE   0x20
#define K_DEL     0x01
#define K_CLR     0x02
#define K_STOP    0x03
#define K_CSRLEFT 0x08
#define K_CSRRIGHT 0x09
#define K_CSRDOWN 0x0A
#define K_CSRUP   0x0B
#define K_HOME    0x0C
#define K_RETURN  0x0D
#define K_CTRL    0x0E
#define K_CBM     0x0F
#define K_INST    0x10
#define K_F1      0xF1
#define K_F3      0xF3
#define K_F5      0xF5
#define K_F7      0xF7
#define K_RESTORE 0xFF
#define K_ESC     0x1B
#define K_TAB     0x80
#define K_ALT     0x81
#define K_HELP    0x82
#define K_LF      0x83
#define K_NOSCRL  0x84
#define K_ERIGHT  0x85
#define K_ELEFT   0x86
#define K_EDOWN   0x87
#define K_EUP     0x88
#define K_KPENTER 0x89
#define K_KPDOT   0x8A
#define K_KP0     0x90          /* + digit */
#define K_KPPLUS  0x9A
#define K_KPMINUS 0x9B
#define K_CAPS    0x130
#define K_4080    0x131
#define K_JOY     0x200         /* + 5 * port + switch */

static void c128_keys_register(c128_t *z) {
    kbd_init(&z->kbd, 2);
    /* The C64's matrix: line = CIA1 port A bit, column = port B bit. */
    const char *keymap =
        "        3WA4ZSE 5RD6CFTX7YG8BHUV9IJ0MKON+PL-.:@,~*;  = /1  2  Q "
        "        #wa$zse %rd&cftx'yg(bhuv)ij0mkon pl >[ <$ ]    ?!  \"  q ";
    kbd_register_modifier(&z->kbd, 0, 7, 1);    /* left shift */
    kbd_register_modifier(&z->kbd, 1, 2, 7);    /* CTRL */
    for (int shift = 0; shift < 2; shift++)
        for (int column = 0; column < 8; column++)
            for (int line = 0; line < 8; line++) {
                int c = keymap[shift * 64 + line * 8 + column];
                if (c != 0x20) kbd_register_key(&z->kbd, c, column, line, shift ? 1 : 0);
            }
    kbd_register_key(&z->kbd, K_SPACE, 4, 7, 0);
    kbd_register_key(&z->kbd, K_CSRLEFT, 2, 0, 1);
    kbd_register_key(&z->kbd, K_CSRRIGHT, 2, 0, 0);
    kbd_register_key(&z->kbd, K_CSRDOWN, 7, 0, 0);
    kbd_register_key(&z->kbd, K_CSRUP, 7, 0, 1);
    kbd_register_key(&z->kbd, K_DEL, 0, 0, 0);
    kbd_register_key(&z->kbd, K_INST, 0, 0, 1);
    kbd_register_key(&z->kbd, K_HOME, 3, 6, 0);
    kbd_register_key(&z->kbd, K_CLR, 3, 6, 1);
    kbd_register_key(&z->kbd, K_RETURN, 1, 0, 0);
    kbd_register_key(&z->kbd, K_CTRL, 2, 7, 0);
    kbd_register_key(&z->kbd, K_CBM, 5, 7, 0);
    kbd_register_key(&z->kbd, K_STOP, 7, 7, 0);
    kbd_register_key(&z->kbd, K_F1, 4, 0, 0);
    kbd_register_key(&z->kbd, K_F3, 5, 0, 0);
    kbd_register_key(&z->kbd, K_F5, 6, 0, 0);
    kbd_register_key(&z->kbd, K_F7, 3, 0, 0);
    kbd_register_key(&z->kbd, K_RESTORE, 0, 11, 0);
    /* The extra keys: three more columns (the VIC's K0-K2, kbd lines 8-10), read on port B (column = row of the
     * connector, bit 7 first). */
    static const int ek[3][8] = {
        { K_HELP, K_KP0 + 8, K_KP0 + 5, K_TAB, K_KP0 + 2, K_KP0 + 4, K_KP0 + 7, K_KP0 + 1 },
        { K_ESC, K_KPPLUS, K_KPMINUS, K_LF, K_KPENTER, K_KP0 + 6, K_KP0 + 9, K_KP0 + 3 },
        { K_ALT, K_KP0, K_KPDOT, K_EUP, K_EDOWN, K_ELEFT, K_ERIGHT, K_NOSCRL },
    };
    for (int l = 0; l < 3; l++)
        for (int bit = 0; bit < 8; bit++) kbd_register_key(&z->kbd, ek[l][bit], bit, 8 + l, 0);
}

static void c128_sync_keys(board_t *b) {
    c128_t *z = C128(b);
    bool changed = false;
    for (int line = 0; line < 12; line++) {
        uint16_t m = z->kbd.scanout_column_masks[line];
        if (m != z->key_matrix[line]) { z->key_matrix[line] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

static void c128_key_down(board_t *b, int key) {
    c128_t *z = C128(b);
    if (key >= K_JOY && key < K_JOY + 10) { z->joy[(key - K_JOY) / 5] |= (uint8_t)(1 << ((key - K_JOY) % 5)); board_rebuild(b); return; }
    if (key == K_CAPS) { z->caps ^= 1; board_rebuild(b); return; }
    if (key == K_4080) { z->key4080 ^= 1; board_rebuild(b); return; }
    kbd_key_down(&z->kbd, key);
    c128_sync_keys(b);
}
static void c128_key_up(board_t *b, int key) {
    c128_t *z = C128(b);
    if (key >= K_JOY && key < K_JOY + 10) { z->joy[(key - K_JOY) / 5] &= (uint8_t)~(1 << ((key - K_JOY) % 5)); board_rebuild(b); return; }
    if (key == K_CAPS || key == K_4080) return;
    kbd_key_up(&z->kbd, key);
    c128_sync_keys(b);
}
static void c128_frame_update(board_t *b, uint32_t us) {
    c128_t *z = C128(b);
    if (z->cbm_hold && b->sim.cycle > 1500000) { z->cbm_hold = 0; kbd_key_up(&z->kbd, K_CBM); }   /* the start-up code has looked */
    kbd_update(&z->kbd, us);
    c128_sync_keys(b);
}

static int c128_host_key(int keycode) {
    switch (keycode) {
    case SAPP_KEYCODE_ENTER: return K_RETURN;
    case SAPP_KEYCODE_BACKSPACE: return K_DEL;
    case SAPP_KEYCODE_RIGHT: return K_ERIGHT;
    case SAPP_KEYCODE_LEFT: return K_ELEFT;
    case SAPP_KEYCODE_DOWN: return K_EDOWN;
    case SAPP_KEYCODE_UP: return K_EUP;
    case SAPP_KEYCODE_HOME: return K_HOME;
    case SAPP_KEYCODE_ESCAPE: return K_STOP;
    case SAPP_KEYCODE_INSERT: return K_ESC;
    case SAPP_KEYCODE_PAGE_UP: return K_RESTORE;
    case SAPP_KEYCODE_PAGE_DOWN: return K_TAB;
    case SAPP_KEYCODE_TAB: return K_CTRL;
    case SAPP_KEYCODE_LEFT_ALT: return K_CBM;
    case SAPP_KEYCODE_RIGHT_ALT: return K_ALT;
    case SAPP_KEYCODE_F1: return K_F1;
    case SAPP_KEYCODE_F3: return K_F3;
    case SAPP_KEYCODE_F5: return K_F5;
    case SAPP_KEYCODE_F7: return K_F7;
    case SAPP_KEYCODE_F9: return K_HELP;
    case SAPP_KEYCODE_F10: return K_LF;
    case SAPP_KEYCODE_F11: return K_4080;
    case SAPP_KEYCODE_F12: return K_CAPS;
    case SAPP_KEYCODE_SCROLL_LOCK: return K_NOSCRL;
    case SAPP_KEYCODE_KP_0: return K_KP0;
    case SAPP_KEYCODE_KP_1: return K_KP0 + 1;
    case SAPP_KEYCODE_KP_2: return K_KP0 + 2;
    case SAPP_KEYCODE_KP_3: return K_KP0 + 3;
    case SAPP_KEYCODE_KP_4: return K_KP0 + 4;
    case SAPP_KEYCODE_KP_5: return K_KP0 + 5;
    case SAPP_KEYCODE_KP_6: return K_KP0 + 6;
    case SAPP_KEYCODE_KP_7: return K_KP0 + 7;
    case SAPP_KEYCODE_KP_8: return K_KP0 + 8;
    case SAPP_KEYCODE_KP_9: return K_KP0 + 9;
    case SAPP_KEYCODE_KP_ADD: return K_KPPLUS;
    case SAPP_KEYCODE_KP_SUBTRACT: return K_KPMINUS;
    case SAPP_KEYCODE_KP_DECIMAL: return K_KPDOT;
    case SAPP_KEYCODE_KP_ENTER: return K_KPENTER;
    case SAPP_KEYCODE_RIGHT_CONTROL: return K_JOY + 9;      /* joystick 2 fire */
    default: return 0;
    }
}

/* Like the C64, the C128 types capitals without shift. */
static int c128_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

const char *c128_key_legend(int line, int col) { (void)line; (void)col; return ""; }

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static int c128_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

#define POR_CYCLES 3000          /* the 556's power-on reset (about 3 ms in the model) */

static void c128_create(board_t *b) {
    c128_t *z = C128(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "U6");
    z->p_z80 = sim_find_part(s, "U10");
    z->p_vic = sim_find_part(s, "U21");
    z->p_mmu = sim_find_part(s, "U7");
    z->p_cia[0] = sim_find_part(s, "U1");
    z->p_cia[1] = sim_find_part(s, "U4");
    z->p_sid = sim_find_part(s, "U5");
    z->p_vdc = sim_find_part(s, "U22");
    z->p_osc = sim_find_part(s, "U28");
    z->p_y2 = sim_find_part(s, "Y2");
    z->p_cn5 = sim_find_part(s, "CN5");
    z->p_cn6 = sim_find_part(s, "CN6");
    z->net_gnd = sim_find_net(s, "GND");
    memset(z->raster, 0, sizeof(z->raster));
    z->rom_mask = 0;
    c128_find_roms(b);
    c128_attach_roms(b);
    if (z->p_vic >= 0) c128_vic_set_screen(&s->parts[z->p_vic], z->raster, C128_VIC_W, C128_VIC_H);
    static const int col_pin[8] = { 13, 19, 18, 17, 16, 15, 14, 20 }, row_pin[8] = { 12, 11, 10, 5, 8, 7, 6, 9 };
    for (int i = 0; i < 8; i++) {
        z->pin_col[i] = c128_pin_of(b, "CN5", col_pin[i]);
        z->pin_row[i] = c128_pin_of(b, "CN5", row_pin[i]);
    }
    for (int i = 0; i < 3; i++) z->pin_k[i] = c128_pin_of(b, "CN5", 21 + i);
    z->pin_restore = c128_pin_of(b, "CN5", 3);
    z->pin_4080 = c128_pin_of(b, "CN5", 24);
    z->pin_caps = c128_pin_of(b, "CN5", 25);
    static const int jpin[5] = { 1, 2, 3, 4, 6 };
    for (int j = 0; j < 2; j++)
        for (int d = 0; d < 5; d++) z->pin_joy[j][d] = c128_pin_of(b, j ? "CN4" : "CN3", jpin[d]);
    z->pin_por = c128_pin_of(b, "U7", 2);                  /* /RES: the power-on reset holds it low */
    z->pin_tod[0] = c128_pin_of(b, "U1", 19);
    z->pin_tod[1] = c128_pin_of(b, "U4", 19);
    b->pin_reset = z->pin_por;                              /* the RESET button shorts /RES to GND */
    b->pin_reset_gnd = c128_pin_of(b, "U7", 34);
    c128_keys_register(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
    z->caps = z->key4080 = 0;
    memset(z->joy, 0, sizeof(z->joy));
}

static void c128_power_on(board_t *b) {
    b->sim.clock_level = 0;
    memset(C128(b)->raster, 0, sizeof(C128(b)->raster));
}

static void c128_joins(board_t *b) {
    sim_t *s = &b->sim;
    c128_t *z = C128(b);
    #define OKPIN(p) ((p) >= 0 && s->pins[p].fault != PF_OPEN)
    for (int line = 0; line < 11; line++)
        for (int col = 0; col < 8; col++) {
            if (!((z->key_matrix[line] >> col) & 1)) continue;
            int pa = line < 8 ? z->pin_col[line] : z->pin_k[line - 8], pb = z->pin_row[col];
            if (OKPIN(pa) && OKPIN(pb)) sim_join(s, s->pins[pa].net, s->pins[pb].net);
        }
    if ((z->key_matrix[11] & 1) && OKPIN(z->pin_restore)) sim_join(s, s->pins[z->pin_restore].net, z->net_gnd);
    if (z->caps && OKPIN(z->pin_caps)) sim_join(s, s->pins[z->pin_caps].net, z->net_gnd);
    if (z->key4080 && OKPIN(z->pin_4080)) sim_join(s, s->pins[z->pin_4080].net, z->net_gnd);
    for (int j = 0; j < 2; j++)
        for (int d = 0; d < 5; d++)
            if (((z->joy[j] >> d) & 1) && OKPIN(z->pin_joy[j][d])) sim_join(s, s->pins[z->pin_joy[j][d]].net, z->net_gnd);
    #undef OKPIN
    /* the 8701 oscillates while its crystal Y2 is good */
    if (z->p_osc >= 0) {
        vco_state_t *vco = s->parts[z->p_osc].state;
        vco->crystal_ok = z->p_y2 < 0 || (s->parts[z->p_y2].rfault == RF_OK && !s->parts[z->p_y2].removed);
        for (int n = 1; n <= 2 && z->p_y2 >= 0; n++) {
            int pi = part_pin(&s->parts[z->p_y2], n);
            if (pi >= 0 && s->pins[pi].fault == PF_OPEN) vco->crystal_ok = false;
        }
    }
}

/* One half period of the dot clock. */
static void c128_step(board_t *b) {
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, C128(b)->p_osc);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void c128_run(board_t *b, int cycles) {
    c128_t *z = C128(b);
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        if (z->pin_por >= 0) sim_drive(s, z->pin_por, s->cycle < POR_CYCLES ? DRV_LO : DRV_Z);
        /* the 60 Hz of the mains for the CIAs' time of day clocks */
        int level = (int)((s->cycle / 8523) & 1);
        for (int k = 0; k < 2; k++) if (z->pin_tod[k] >= 0) sim_drive(s, z->pin_tod[k], DRV_BIT(level));
        for (int k = 0; k < C128_STEPS_PER_CYCLE; k++) c128_step(b);
    }
}

/* RAM: the cell of an address in the 4164s. */
static int c128_cell(uint16_t addr) { return ((addr & 0xFF) << 8) | (addr >> 8); }

static uint8_t c128_ram_peek(board_t *b, int bank, uint16_t addr) {
    uint8_t v = 0;
    char ref[8];
    for (int bit = 0; bit < 8; bit++) {
        snprintf(ref, sizeof(ref), "U%d", 38 + bank * 8 + bit);
        int p = sim_find_part(&b->sim, ref);
        if (p >= 0) v |= (uint8_t)(((dram_state_t *)b->sim.parts[p].state)->cells[c128_cell(addr)] << bit);
    }
    return v;
}

static int c128_ram_cell(board_t *b, int part, uint16_t addr) {
    (void)b;
    const part_t *p = &b->sim.parts[part];
    return p->reset == PT_4164.reset ? c128_cell(addr) : -1;
}

static void c128_monitor(board_t *b, uint32_t *out) {
    c128_t *z = C128(b);
    if (z->mon80 && z->p_vdc >= 0) { c128_vdc_render(&b->sim, &b->sim.parts[z->p_vdc], out, C128_SCREEN_W, C128_SCREEN_H); return; }
    for (int y = 0; y < C128_VIC_H; y++)
        for (int x = 0; x < C128_VIC_W; x++) {
            uint32_t c = z->raster[y * C128_VIC_W + x];
            uint32_t *d = out + 2 * y * C128_SCREEN_W + 2 * x;
            d[0] = d[1] = d[C128_SCREEN_W] = d[C128_SCREEN_W + 1] = c;
        }
}
void c128_set_monitor80(board_t *b, bool on) { C128(b)->mon80 = on; }
bool c128_is_c64(board_t *b) { return b->sim.parts[C128(b)->p_mmu].dead ? false : c128_mmu_c64_of(&b->sim.parts[C128(b)->p_mmu]); }
/* C64 mode as on the real machine: switch on (or reset) with the C= key held, and the Z80's start-up code starts
 * the C64 ROMs. The key is let go after 1.5 s. */
void c128_set_c64(board_t *b, bool on) {
    c128_t *z = C128(b);
    board_power_on(b);
    z->cbm_hold = on;
    if (on) kbd_key_down(&z->kbd, K_CBM);
    c128_sync_keys(b);
}
static const char *c128_mode_label(board_t *b) {
    return c128_is_c64(b) ? "Mode: C64  - click for C128" : "Mode: C128  - click for C64";
}
static void c128_mode_toggle(board_t *b) { c128_set_c64(b, !c128_is_c64(b)); }
static const char *c128_monitor_label(board_t *b) {
    return C128(b)->mon80 ? "Monitor: RGBI, 80 col (VDC)  - click for 40 col" : "Monitor: composite, 40 col (VIC)  - click for 80 col";
}
/* Move the cable to the other output, put the 40/80 DISPLAY key where the Kernal will set that screen up and
 * switch the machine off and on again (the Kernal reads the key at start-up only). */
static void c128_monitor_toggle(board_t *b) {
    c128_t *z = C128(b);
    z->mon80 ^= 1;
    z->key4080 = z->mon80;
    board_rebuild(b);
    board_power_on(b);
}

/* ------------------------------------------------------------------------ */
/* Files                                                                     */
/* ------------------------------------------------------------------------ */

static const file_kind_t c128_rom_kind = { "ROM image", "Fit in its socket", false, ROLE_ROM };
static const file_kind_t c128_disk_kind = { "disk image", "Insert in drive 8", false, ROLE_OTHER };

static bool is_disk_path(const char *path) {
    size_t pl = strlen(path);
    return pl > 4 && (!strcasecmp(path + pl - 4, ".d64") || !strcasecmp(path + pl - 4, ".d71") || !strcasecmp(path + pl - 4, ".d81"));
}

/* A disk image in drive 8: the drive answers on the serial bus from then on. */
bool c128_disk_insert(board_t *b, const char *path, char *msg, int msglen) {
    c128_t *z = C128(b);
    if (z->p_cn6 < 0) { snprintf(msg, (size_t)msglen, "this board has no serial connector"); return false; }
    diskdrive_t *d = drive_of(&b->sim.parts[z->p_cn6]);
    if (!drive_mount(d, path, msg, msglen)) return false;
    d->enabled = 1;
    if (!d->device) d->device = 8;
    drive_reset(d);
    return true;
}
bool c128_disk_active(board_t *b) {
    c128_t *z = C128(b);
    if (z->p_cn6 < 0) return false;
    const diskdrive_t *d = drive_of(&b->sim.parts[z->p_cn6]);
    return d->enabled && b->sim.cycle - d->active < 150000;      /* the light stays on for a moment */
}
void c128_disk_eject(board_t *b) {
    c128_t *z = C128(b);
    if (z->p_cn6 < 0) return;
    diskdrive_t *d = drive_of(&b->sim.parts[z->p_cn6]);
    drive_eject(d);
    d->enabled = 0;
    d->pull_clk = d->pull_data = d->pull_srq = 0;
}

static const file_kind_t *c128_file_kind(const char *path, char *info, int len, bool *bad) {
    const file_kind_t *state = board_state_file_kind(path, info, len, bad);
    if (state) return state;
    size_t pl = strlen(path);
    if (is_disk_path(path)) { snprintf(info, (size_t)len, "a floppy disk for the drive on the serial bus"); return &c128_disk_kind; }
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        const char *ref = c128_rom_ref(path);
        if (!ref) return NULL;
        snprintf(info, (size_t)len, "goes in socket %s", ref);
        return &c128_rom_kind;
    }
    return NULL;
}

static const char *c128_cart_name(board_t *b) {
    const c128_t *z = C128(b);
    if (z->p_cn6 < 0) return NULL;
    const char *p = drive_disk_name(drive_of(&b->sim.parts[z->p_cn6]));
    const char *base = p ? strrchr(p, '/') : NULL;
    const char *base2 = p ? strrchr(p, '\\') : NULL;
    if (base2 > base) base = base2;
    return p ? (base ? base + 1 : p) : NULL;
}
static void c128_remove_cart(board_t *b) { c128_disk_eject(b); }

static bool c128_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    (void)run;
    if (is_disk_path(path)) return c128_disk_insert(b, path, msg, msglen);
    if (!c128_fit_rom(b, path, msg, msglen)) return false;
    board_config_changed(b);
    board_power_on(b);
    return true;
}

static void c128_destroy(board_t *b) { (void)b; }

static const char *c128_cpu_status(board_t *b) {
    const c128_t *z = C128(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!(z->rom_mask & 1)) return "no system ROM (put the C128 ROMs in roms/c128)";
    if (!(z->rom_mask & 2) || !(z->rom_mask & 4)) return "no BASIC ROM (put the C128 ROMs in roms/c128)";
    if (!(z->rom_mask & 8)) return "no character ROM (put the C128 ROMs in roms/c128)";
    return "running";
}

static void c128_cpu_regs(board_t *b, char *buf, int len) {
    c128_t *z = C128(b);
    char a[96], c[96];
    c128_cpu_regs_of(&b->sim.parts[z->p_cpu], a, sizeof(a));
    c128_z80_regs_of(&b->sim.parts[z->p_z80], c, sizeof(c));
    snprintf(buf, (size_t)len, "8502 %s  %s", a, c);
}

static void c128_print_text(board_t *b) {
    c128_t *z = C128(b);
    if (z->mon80 && z->p_vdc >= 0) {                    /* the 80 column screen: the VDC's own RAM */
        const part_t *v = &b->sim.parts[z->p_vdc];
        uint16_t base = (uint16_t)(c128_vdc_reg_of(v, 12) << 8 | c128_vdc_reg_of(v, 13));
        int inc = c128_vdc_reg_of(v, 27);
        for (int y = 0; y < 25; y++) {
            char line[88];
            for (int x = 0; x < 80; x++) {
                int code = c128_vdc_peek(v, (uint16_t)(base + y * (80 + inc) + x)) & 0x7F;
                line[x] = code == 0 ? '@' : code <= 26 ? (char)('A' + code - 1) : code < 64 ? "[\\]^_ !\"#$%&'()*+,-./0123456789:;<=>?"[code - 27] : '#';
            }
            line[80] = 0;
            int end = 80;
            while (end > 0 && line[end - 1] == ' ') line[--end] = 0;
            printf("|%s\n", line);
        }
        return;
    }
    /* The 40 column screen: the video matrix at $0400 of bank 0 (screen codes to ASCII). */
    for (int y = 0; y < 25; y++) {
        char line[48];
        for (int x = 0; x < 40; x++) {
            int code = c128_ram_peek(b, 0, (uint16_t)(0x0400 + y * 40 + x)) & 0x7F;
            char ch = '?';
            if (code == 0) ch = '@';
            else if (code >= 1 && code <= 26) ch = (char)('A' + code - 1);
            else if (code >= 27 && code < 64) ch = "[\\]^_ !\"#$%&'()*+,-./0123456789:;<=>?"[code - 27];
            else if (code >= 64) ch = '#';
            line[x] = ch;
        }
        line[40] = 0;
        int end = 40;
        while (end > 0 && line[end - 1] == ' ') line[--end] = 0;
        printf("|%s\n", line);
    }
}

const machine_t machine_c128 = {
    .size = sizeof(c128_t),
    .sim = { .rail_of = c128_rail_of, .kind_of = c128_kind_of, .cycles_per_us = 0.985248 },
    .cpu_hz = C128_FREQ,
    .cycles_per_frame = C128_CYCLES_PER_FRAME,
    .steps_per_cycle = C128_STEPS_PER_CYCLE,
    .screen_w = C128_SCREEN_W, .screen_h = C128_SCREEN_H,
    .ready_cycles = 4 * C128_FREQ,
    .conns = c128_conn_defs, .nconns = sizeof(c128_conn_defs) / sizeof(c128_conn_defs[0]),
    .link_resistors = "R102 R29 R30 R_MA0 R_MA1 R_MA2 R_MA3 R_MA4 R_MA5 R_MA6 R_MA7",
    .crystals = "Y1 Y2",
    .diodes = "CR", .jumpers = "J",
    .supplies = "+9V/+12V",
    .keys_help = "Keys: type; Esc = RUN/STOP, Insert = ESC, Tab = CTRL, Left Alt = C=, Right Alt = ALT, Home = HOME, Backspace = INST/DEL, "
                 "arrow keys = the cursor keys, PgUp = RESTORE, F1/F3/F5/F7, F9 = HELP, F10 = LINE FEED, F11 = 40/80 DISPLAY (hold before power on: pressed = 80 columns), "
                 "F12 = CAPS LOCK, Scroll Lock = NO SCROLL, keypad = the numeric keypad",
    .reset_help = "Hold to press RESET: shorts the MMU's /RES (U7 pin 2) to GND",
    .create = c128_create,
    .destroy = c128_destroy,
    .power_on = c128_power_on,
    .run = c128_run,
    .joins = c128_joins,
    .key_down = c128_key_down,
    .key_up = c128_key_up,
    .frame_update = c128_frame_update,
    .host_key = c128_host_key,
    .host_char = c128_host_char,
    .monitor = c128_monitor,
    .mode_label = c128_mode_label,
    .mode_toggle = c128_mode_toggle,
    .monitor_label = c128_monitor_label,
    .monitor_toggle = c128_monitor_toggle,
    .files_title = "Open a ROM image or save state",
    .file_kind = c128_file_kind,
    .cart_slot = "CN6",
    .cart_name = c128_cart_name,
    .remove_cart = c128_remove_cart,
    .load = c128_load,
    .cpu_status = c128_cpu_status,
    .cpu_regs = c128_cpu_regs,
    .print_text = c128_print_text,
    .ram_cell = c128_ram_cell,
    .id = "c128",
    .issue = "310378",
    .name = "Commodore 128",
    .board_name = "C128 (PCB 310378 rev 7A)",
    .data = &bd_c128,
    .parts = c128_part_defs, .nparts = sizeof(c128_part_defs) / sizeof(c128_part_defs[0]),
};
