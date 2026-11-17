/* plus4.c - the Commodore Plus/4 (schematic 310164, 1984).
 *
 * The same TED (U1), 8501 CPU (U2) and 7700 PLA (U19) as the C16 (c16_chips.c), on a board with 64K of RAM:
 * eight 4164s (U11-U18, one bit each) behind the address multiplexers U9 and U10, four 23128 ROMs - BASIC 3.5
 * (U23), the KERNAL (U24) and the 3-plus-1 function ROMs (U25 low, U26 high) - selected through the ROM latch
 * (U21, $FDD0-$FDDF) and the 74LS139 U20, a 6551 ACIA (U3, $FD00) and a second 6529B (U5, $FD10: the user port,
 * and bit 2 is the cassette sense). The keyboard is the C16's 8 x 8 matrix between the 6529B U27 at $FD30 (the
 * rows) and the TED's K lines (the columns, latched by writing $FF08). Joysticks, cassette and serial bus are
 * wired as on the C16; the joystick selects are the data bus bits D1 and D2 through the 74LS08 U4. */
#include "plus4.h"
#include "sokol/sokol_app.h"   /* key codes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#define GLUE(ref, type, label, what) { ref, type, label, ROLE_GLUE, NULL, what }

static const part_def_t plus4_part_defs[] = {
    { "U2", &PT_7501, "CPU", ROLE_CHIP, "8501", "8501 CPU, 0.89 MHz: phase 0 comes from the TED; AEC and BA (RDY) from the TED stop it while the TED uses the bus" },
    { "U1", &PT_7360, "TED", ROLE_CHIP, "7360 TED\nvideo + sound\n$FF00-$FF1F",
      "7360 TED: video, sound, timers, the DRAM and ROM strobes, the system clock and the keyboard latch" },
    { "U27", &PT_6529B, "KEYPORT", ROLE_CHIP, "6529B\n$FD30", "6529B single port: the keyboard rows (the TED's K lines are the columns)" },
    { "U5", &PT_6529B, "USER PORT", ROLE_CHIP, "6529B\n$FD10", "6529B single port: the user port bits P0, P2-P7 (P1 senses CTS); P2 is also the cassette sense" },
    { "U3", &PT_6551, "ACIA", ROLE_CHIP, "6551A\n$FD00", "6551A serial interface at $FD00-$FD03 for the user port (the registers only: nothing is sent or received)" },
    { "U23", &PT_C16ROM, "BASIC", ROLE_ROM, "23128 ROM\nBASIC 3.5\n$8000-$BFFF", "23128 BASIC 3.5 ROM (318006-01), $8000-$BFFF" },
    { "U24", &PT_C16ROM, "KERNAL", ROLE_ROM, "23128 ROM\nKERNAL\n$C000-$FFFF", "23128 KERNAL ROM (318005-05), $C000-$FFFF" },
    { "U25", &PT_C16ROM, "FUNC LOW", ROLE_ROM, "23128 ROM\n3-plus-1\n$8000-$BFFF", "23128 function ROM low (317053-01), $8000-$BFFF when selected through the ROM latch" },
    { "U26", &PT_C16ROM, "FUNC HIGH", ROLE_ROM, "23128 ROM\n3-plus-1\n$C000-$FFFF", "23128 function ROM high (317054-01), $C000-$FFFF when selected through the ROM latch" },
    { "U11", &PT_4164, "RAM", ROLE_RAM, "4164\n64K x 1\nbit 0", "4164 dynamic RAM, 64K x 1: bit 0 of every byte" },
    { "U12", &PT_4164, "RAM", ROLE_RAM, "4164\n64K x 1\nbit 1", "4164 dynamic RAM, 64K x 1: bit 1 of every byte" },
    { "U13", &PT_4164, "RAM", ROLE_RAM, "4164\n64K x 1\nbit 2", "4164 dynamic RAM, 64K x 1: bit 2 of every byte" },
    { "U14", &PT_4164, "RAM", ROLE_RAM, "4164\n64K x 1\nbit 3", "4164 dynamic RAM, 64K x 1: bit 3 of every byte" },
    { "U15", &PT_4164, "RAM", ROLE_RAM, "4164\n64K x 1\nbit 4", "4164 dynamic RAM, 64K x 1: bit 4 of every byte" },
    { "U16", &PT_4164, "RAM", ROLE_RAM, "4164\n64K x 1\nbit 5", "4164 dynamic RAM, 64K x 1: bit 5 of every byte" },
    { "U17", &PT_4164, "RAM", ROLE_RAM, "4164\n64K x 1\nbit 6", "4164 dynamic RAM, 64K x 1: bit 6 of every byte" },
    { "U18", &PT_4164, "RAM", ROLE_RAM, "4164\n64K x 1\nbit 7", "4164 dynamic RAM, 64K x 1: bit 7 of every byte" },
    { "Y1", &PT_XTAL14, "14.31818", ROLE_CLOCK, "crystal", "14.31818 MHz crystal (modelled as a clock source on the TED's CLKIN)" },
    { "U8", &PT_LM555, "NE555", ROLE_CLOCK, "reset", "NE555 power-on reset timer: holds /RESET low for a while (through the 7406 U7)" },
    GLUE("U19", &PT_7700, "7700 PLA", "7700-010 PLA: decodes $FD00-$FDDF (the 6551, the user port, the keyboard port, the ROM latch) and makes phase 2"),
    GLUE("U21", &PT_74LS175, "74LS175", "74LS175: the ROM latch, written by any write to $FDD0-$FDDF (the low address bits)"),
    GLUE("U20", &PT_74LS139, "74LS139", "74LS139: turns the TED's /CS0 and /CS1 and the ROM latch into the ROM and cartridge selects"),
    GLUE("U22", &PT_74LS27, "74LS27", "74LS27: the ROM latch clock and the high ROM select"),
    GLUE("U4", &PT_74LS08, "74LS08", "74LS08: the joystick selects, from D2 (port 0) and D1 (port 1)"),
    GLUE("U6", &PT_74LS04, "74LS04", "74LS04: buffers between the 6551 and the user port"),
    GLUE("U7", &PT_7406, "7406", "7406: open collector buffers: /RESET, /BRESET, the serial bus, the cassette motor"),
    GLUE("U9", &PT_74LS257, "74LS257", "74LS257: DRAM address multiplexer for A0, A1, A2, A3 and A14, A8, A9, A10"),
    GLUE("U10", &PT_74LS257, "74LS257", "74LS257: DRAM address multiplexer for A4-A7 and A11-A13, A15"),
};

static const conn_def_t plus4_conn_defs[] = {
    { "CN1", "POWER", "1=+5V 2=GND 3=9VAC 4=9VAC" },
    { "CN2", "SERIAL", "1=SRQ 2=GND 3=ATN 4=CLK 5=DATA 6=RESET" },
    { "CN3", "CASSETTE", "1=GND 2=+5V 3=MOTOR 4=READ 5=WRITE 6=SENSE 7=GND" },
    { "CN4", "EXPANSION", "1=GND 2=+5V 3=+5V 4=/IRQ 5=R/W 6=C1HIGH 7=C2LOW 8=C2HIGH 9=/CS1 10=/CS0 11=/CAS 12=MUX 13=BA 14=D7 "
                          "15=D6 16=D5 17=D4 18=D3 19=D2 20=D1 21=D0 22=AEC 23=EXT_AUDIO 24=PHI2 25=GND A=GND B=C1LOW C=/BRESET "
                          "D=/RAS E=PHI0 F=A15 H=A14 J=A13 K=A12 L=A11 M=A10 N=A9 P=A8 R=A7 S=A6 T=A5 U=A4 V=A3 W=A2 X=A1 Y=A0" },
    { "CN5", "JOYSTICK 0", "1=UP 2=DOWN 3=LEFT 4=RIGHT 5=+5V 6=FIRE 7=GND 8=SELECT" },
    { "CN6", "JOYSTICK 1", "1=UP 2=DOWN 3=LEFT 4=RIGHT 5=+5V 6=FIRE 7=GND 8=SELECT" },
    { "CN7", "AUDIO/VIDEO", "1=LUM 2=GND 3=AUDIO_OUT 4=VIDEO 5=AUDIO_IN 6=COLOR" },
    { "CN8", "KEYBOARD", "1=ROW7 2=COL7 3=GND 4=+5V 5=ROW6 6=COL4 7=ROW2 8=COL5 9=COL6 10=ROW5 11=ROW4 12=ROW3 13=COL2 14=COL1 "
                         "15=ROW0 16=COL3 17=COL0 18=ROW1" },
    { "CN9", "USER PORT", "A=GND B=P0 C=RXD D=RTS E=DTR F=P7 H=DCD J=P6 K=CTS L=DSR M=TXD N=GND 1=GND 2=+5V 3=/BRESET 4=P2 5=P3 "
                          "6=P4 7=P5 8=RXC 9=ATN 10=9VAC 11=9VAC 12=GND" },
};

static int plus4_rail_of(const char *name) {
    if (!strcmp(name, "GND")) return RAIL_GND;
    if (!strcmp(name, "+5V")) return RAIL_VCC;
    return RAIL_NONE;
}

static int plus4_kind_of(const char *ref) {
    if (ref[0] == 'U') return PART_IC;
    if (ref[0] == 'R' && ref[1] >= '0' && ref[1] <= '9') return PART_RESISTOR;
    if (!strncmp(ref, "RP", 2)) return PART_RESISTOR;
    if (!strncmp(ref, "FB", 2) || ref[0] == 'L' || ref[0] == 'F' || !strcmp(ref, "J9")) return PART_LINK;     /* J9 is fitted (N.C.), J8 is not */
    if (!strncmp(ref, "CN", 2) && ref[2] >= '0' && ref[2] <= '9') return PART_CONNECTOR;
    return PART_OTHER;
}

/* ------------------------------------------------------------------------ */
/* ROMs                                                                      */
/* ------------------------------------------------------------------------ */

static int plus4_rom_index(const char *ref) {
    static const char *const refs[4] = { "U23", "U24", "U25", "U26" };
    for (int i = 0; i < 4; i++)
        if (!strcmp(ref, refs[i])) return i;
    return -1;
}

bool plus4_set_rom(board_t *b, const char *ref, const uint8_t *data, size_t n) {
    plus4_t *z = PLUS4(b);
    int i = plus4_rom_index(ref);
    if (i < 0 || n != 16384) return false;
    memcpy(z->rom[i], data, n);
    z->rom_mask |= 1u << i;
    return true;
}

static const char *plus4_rom_ref(const char *path) {
    static const struct { const char *key, *ref; bool own_folder; } map[] = {
        { "318006", "U23", false }, { "318005", "U24", false }, { "318004", "U24", false },
        { "317053", "U25", false }, { "317054", "U26", false },
        { "basic", "U23", true }, { "kernal", "U24", true },
    };
    const char *name = board_basename(path);
    bool own = strstr(path, "plus4/") || strstr(path, "plus4\\");
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (map[i].own_folder && !own) continue;
        size_t n = strlen(map[i].key);
        for (const char *s = name; *s; s++)
            if (!strncasecmp(s, map[i].key, n)) return map[i].ref;
    }
    return NULL;
}

static bool plus4_fit_rom(board_t *b, const char *path, char *msg, int msglen) {
    const char *ref = plus4_rom_ref(path);
    if (!ref) { snprintf(msg, (size_t)msglen, "%s: don't know which ROM this is", path); return false; }
    uint8_t buf[16385];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(buf, 1, sizeof(buf), f) : 0;
    if (f) fclose(f);
    if (!plus4_set_rom(b, ref, buf, n)) {
        snprintf(msg, (size_t)msglen, "%s: %zu bytes don't fit socket %s", path, n, ref);
        return false;
    }
    snprintf(msg, (size_t)msglen, "%s fitted in %s", board_basename(path), ref);
    return true;
}

/* At start: the ROMs found in roms/plus4, roms or the current folder. */
static void plus4_find_roms(board_t *b) {
    static const char *const dirs[] = { "roms/plus4", "roms", "." };
    for (size_t d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *e;
        while ((e = readdir(dir))) {
            size_t l = strlen(e->d_name);
            if (l < 5 || (strcasecmp(e->d_name + l - 4, ".bin") && strcasecmp(e->d_name + l - 4, ".rom"))) continue;
            char path[1024], msg[256];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], e->d_name);
            const char *ref = plus4_rom_ref(path);
            if (!ref) continue;
            if (PLUS4(b)->rom_mask & (1u << plus4_rom_index(ref))) continue;
            plus4_fit_rom(b, path, msg, sizeof(msg));
        }
        closedir(dir);
    }
}

/* ------------------------------------------------------------------------ */
/* The board                                                                 */
/* ------------------------------------------------------------------------ */

static int plus4_pin_of(board_t *b, const char *ref, int number) {
    int p = sim_find_part(&b->sim, ref);
    return p < 0 ? -1 : part_pin(&b->sim.parts[p], number);
}

static void plus4_keys_register(plus4_t *z);

static void plus4_create(board_t *b) {
    plus4_t *z = PLUS4(b);
    sim_t *s = &b->sim;
    z->p_cpu = sim_find_part(s, "U2");
    z->p_ted = sim_find_part(s, "U1");
    z->p_xtal = sim_find_part(s, "Y1");
    for (int i = 0; i < 8; i++) {
        char ref[8];
        snprintf(ref, sizeof(ref), "U%d", 11 + i);
        z->p_ram[i] = sim_find_part(s, ref);
    }
    memset(z->rom, 0xFF, sizeof(z->rom));
    z->rom_mask = 0;
    for (int i = 0; i < 4; i++) {
        char ref[8];
        snprintf(ref, sizeof(ref), "U%d", 23 + i);
        int idx = sim_find_part(s, ref);
        if (idx >= 0) ((rom_state_t *)s->parts[idx].state)->data = z->rom[i];
    }
    plus4_find_roms(b);
    if (z->p_ted >= 0) ted_set_screen(&s->parts[z->p_ted], z->raster);
    memset(z->raster, 0, sizeof(z->raster));
    static const int row_pin[8] = { 15, 18, 7, 12, 11, 10, 5, 1 }, col_pin[8] = { 17, 14, 13, 16, 6, 8, 9, 2 };
    for (int i = 0; i < 8; i++) {
        z->pin_row[i] = plus4_pin_of(b, "CN8", row_pin[i]);
        z->pin_col[i] = plus4_pin_of(b, "CN8", col_pin[i]);
    }
    static const int jpin[5] = { 1, 2, 3, 4, 6 };
    for (int j = 0; j < 2; j++) {
        const char *cn = j ? "CN6" : "CN5";
        for (int d = 0; d < 5; d++) z->pin_joy[j][d] = plus4_pin_of(b, cn, jpin[d]);
        z->pin_joy_sel[j] = plus4_pin_of(b, cn, 8);
    }
    memset(z->joy, 0, sizeof(z->joy));
    /* The joystick diodes CR11-CR15 (port 0) and CR16-CR20 (port 1): the anode (pin 2) on a TED K line, the
     * cathode (pin 1) on the joystick's switch. */
    for (int i = 11; i <= 20; i++) {
        char ref[8];
        snprintf(ref, sizeof(ref), "CR%d", i);
        int pi = sim_find_part(s, ref);
        if (pi < 0) continue;
        part_t *dp = &s->parts[pi];
        if (part_pin(dp, 1) < 0 || part_pin(dp, 2) < 0) continue;
        resistor_state_t *st = calloc(1, sizeof(*st));
        dp->state = st;
        dp->state_size = sizeof(*st);
        dp->kind = PART_RESISTOR;
        st->nel = 1;
        st->el[0] = (element_t){ part_pin(dp, 2), part_pin(dp, 1), EL_ANALOG };
        element_set_type(s, dp, 0, EL_DIODE);
    }
    /* The EMI T-filters EM1-EM5 pass the signal from pin 1 to pin 3 (pin 2 is ground). */
    for (int i = 1; i <= 5; i++) {
        char ref[8];
        snprintf(ref, sizeof(ref), "EM%d", i);
        int pi = sim_find_part(s, ref);
        if (pi < 0) continue;
        part_t *ep = &s->parts[pi];
        if (part_pin(ep, 1) < 0 || part_pin(ep, 3) < 0) continue;
        resistor_state_t *st = calloc(1, sizeof(*st));
        ep->state = st;
        ep->state_size = sizeof(*st);
        ep->kind = PART_RESISTOR;
        st->nel = 1;
        st->el[0] = (element_t){ part_pin(ep, 1), part_pin(ep, 3), EL_LINK };
    }
    b->pin_reset = plus4_pin_of(b, "U2", 40);             /* 7501 /RES to VSS, as an external reset switch would */
    b->pin_reset_gnd = plus4_pin_of(b, "U2", 20);
    plus4_keys_register(z);
    memset(z->key_matrix, 0, sizeof(z->key_matrix));
}

static void plus4_power_on(board_t *b) {
    b->sim.clock_level = 0;
    memset(PLUS4(b)->raster, 0, sizeof(PLUS4(b)->raster));
}

static void plus4_joins(board_t *b) {
    sim_t *s = &b->sim;
    plus4_t *z = PLUS4(b);
    /* A closed key joins its row (a 6529 port bit) to its column (a TED K line), unless a pin is open. */
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++) {
            if (!((z->key_matrix[row] >> col) & 1)) continue;
            int pr = z->pin_row[row], pc = z->pin_col[col];
            if (pr < 0 || pc < 0 || s->pins[pr].fault == PF_OPEN || s->pins[pc].fault == PF_OPEN) continue;
            sim_join(s, s->pins[pr].net, s->pins[pc].net);
        }
    /* A closed joystick switch joins its direction (or fire) pin to pin 8, the select the 74LS125 drives. */
    for (int j = 0; j < 2; j++)
        for (int d = 0; d < 5; d++) {
            int pa = z->pin_joy[j][d], ps = z->pin_joy_sel[j];
            if (!((z->joy[j] >> d) & 1) || pa < 0 || ps < 0 || s->pins[pa].fault == PF_OPEN || s->pins[ps].fault == PF_OPEN) continue;
            sim_join(s, s->pins[pa].net, s->pins[ps].net);
        }
}

/* One half period of the crystal. */
static void plus4_step(board_t *b) {
    sim_t *s = &b->sim;
    s->clock_level ^= 1;
    sim_mark_dirty(s, PLUS4(b)->p_xtal);
    sim_settle(s);
    sim_commit(s);
    sim_settle(s);
    board_sample(b);
}

static void plus4_run(board_t *b, int cycles) {
    sim_t *s = &b->sim;
    for (int i = 0; i < cycles; i++) {
        s->cycle++;
        for (int k = 0; k < PLUS4_STEPS_PER_CYCLE; k++) plus4_step(b);
    }
}

uint32_t plus4_cpu_cycles(board_t *b) { return c16_cpu_cycles_of(&b->sim.parts[PLUS4(b)->p_cpu]); }

/* RAM: the cell of an address in the 4164s. The row is A0-A7 on the DRAM's A0-A7; the column is A14, A8-A13 and
 * A15 on A0-A7 (the wiring of the multiplexers U9 and U10). U11 holds bit 0 of every byte, U18 bit 7. */
static int plus4_cell(uint16_t addr) {
    int a = addr >> 8;
    return ((addr & 0xFF) << 8) | ((a >> 6) & 1) | ((a & 0x3F) << 1) | (a & 0x80);
}

void plus4_ram_poke(board_t *b, uint16_t addr, uint8_t v) {
    plus4_t *z = PLUS4(b);
    int cell = plus4_cell(addr);
    for (int i = 0; i < 8; i++)
        if (z->p_ram[i] >= 0) ((dram_state_t *)b->sim.parts[z->p_ram[i]].state)->cells[cell] = (v >> i) & 1;
}

uint8_t plus4_ram_peek(board_t *b, uint16_t addr) {
    plus4_t *z = PLUS4(b);
    int cell = plus4_cell(addr);
    uint8_t v = 0;
    for (int i = 0; i < 8; i++)
        if (z->p_ram[i] >= 0) v |= (uint8_t)((((dram_state_t *)b->sim.parts[z->p_ram[i]].state)->cells[cell] & 1) << i);
    return v;
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
static const char *const plus4_keys[8][8] = {
    { "DEL", "RETURN", "POUND", "HELP", "F1", "F2", "F3", "@" },
    { "3", "W", "A", "4", "Z", "S", "E", "LSHIFT" },
    { "5", "R", "D", "6", "C", "F", "T", "X" },
    { "7", "Y", "G", "8", "B", "H", "U", "V" },
    { "9", "I", "J", "0", "M", "K", "O", "N" },
    { "DOWN", "P", "L", "UP", ".", ":", "-", "," },
    { "LEFT", "*", ";", "RIGHT", "ESC", "=", "+", "/" },
    { "1", "HOME", "CTRL", "2", "SPACE", "CBM", "Q", "STOP" },
};

const char *plus4_key_legend(int row, int col) { return plus4_keys[row & 7][col & 7]; }

static void plus4_keys_register(plus4_t *z) {
    kbd_init(&z->kbd, 2);
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++) {
            const char *n = plus4_keys[row][col];
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

static void plus4_sync_keys(board_t *b) {
    plus4_t *z = PLUS4(b);
    bool changed = false;
    for (int row = 0; row < 8; row++) {
        uint16_t m = z->kbd.scanout_column_masks[row];
        if (m != z->key_matrix[row]) { z->key_matrix[row] = m; changed = true; }
    }
    if (changed) board_rebuild(b);
}

#define CK_JOY 0x100        /* + 5 * port + switch: the joysticks */
static void plus4_key_down(board_t *b, int key) {
    if (key >= CK_JOY && key < CK_JOY + 10) { PLUS4(b)->joy[(key - CK_JOY) / 5] |= (uint8_t)(1 << ((key - CK_JOY) % 5)); board_rebuild(b); return; }
    kbd_key_down(&PLUS4(b)->kbd, key);
    plus4_sync_keys(b);
}
static void plus4_key_up(board_t *b, int key) {
    if (key >= CK_JOY && key < CK_JOY + 10) { PLUS4(b)->joy[(key - CK_JOY) / 5] &= (uint8_t)~(1 << ((key - CK_JOY) % 5)); board_rebuild(b); return; }
    kbd_key_up(&PLUS4(b)->kbd, key);
    plus4_sync_keys(b);
}
static void plus4_frame_update(board_t *b, uint32_t us) { kbd_update(&PLUS4(b)->kbd, us); plus4_sync_keys(b); }

static int plus4_host_key(int keycode) {
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
    case SAPP_KEYCODE_KP_8: return CK_JOY;                  /* joystick 1: the keypad */
    case SAPP_KEYCODE_KP_2: return CK_JOY + 1;
    case SAPP_KEYCODE_KP_4: return CK_JOY + 2;
    case SAPP_KEYCODE_KP_6: return CK_JOY + 3;
    case SAPP_KEYCODE_KP_0: return CK_JOY + 4;
    case SAPP_KEYCODE_PAGE_UP: return CK_JOY + 5;           /* joystick 2 */
    case SAPP_KEYCODE_PAGE_DOWN: return CK_JOY + 6;
    case SAPP_KEYCODE_DELETE: return CK_JOY + 7;
    case SAPP_KEYCODE_END: return CK_JOY + 8;
    case SAPP_KEYCODE_RIGHT_CONTROL: return CK_JOY + 9;
    default: return 0;
    }
}

/* Like the C64, the Plus/4 types capitals without shift. */
static int plus4_host_char(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static void plus4_monitor(board_t *b, uint32_t *out) {
    memcpy(out, PLUS4(b)->raster, sizeof(PLUS4(b)->raster));
}

/* ------------------------------------------------------------------------ */
/* Files                                                                     */
/* ------------------------------------------------------------------------ */

static const file_kind_t plus4_rom_kind = { "ROM image", "Fit in its socket", false, ROLE_ROM };
static const file_kind_t plus4_prg = { "program", "Load and RUN", true, -1 };

static const file_kind_t *plus4_file_kind(const char *path, char *info, int len, bool *bad) {
    const file_kind_t *state = board_state_file_kind(path, info, len, bad);
    if (state) return state;
    size_t pl = strlen(path);
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        const char *ref = plus4_rom_ref(path);
        if (!ref) return NULL;
        snprintf(info, (size_t)len, "goes in socket %s", ref);
        return &plus4_rom_kind;
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
    return &plus4_prg;
}

static bool plus4_load(board_t *b, const char *path, bool run, char *msg, int msglen) {
    size_t pl = strlen(path);
    if (pl > 4 && (!strcasecmp(path + pl - 4, ".bin") || !strcasecmp(path + pl - 4, ".rom"))) {
        if (!plus4_fit_rom(b, path, msg, msglen)) return false;
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
    if (start < 0x0400 || end > 0xFD00) {
        free(buf);
        snprintf(msg, (size_t)msglen, "$%04X-$%04X doesn't fit the Plus/4's RAM ($0400-$FCFF)", start, end);
        return false;
    }
    for (size_t i = 2; i < n; i++) plus4_ram_poke(b, (uint16_t)(start + i - 2), buf[i]);
    free(buf);
    bool basic = start == 0x1001;
    if (basic) {    /* end of the program = start of the variables, arrays, end of arrays ($2D-$32) */
        for (int p = 0x2D; p <= 0x31; p += 2) {
            plus4_ram_poke(b, (uint16_t)p, end & 0xFF);
            plus4_ram_poke(b, (uint16_t)(p + 1), end >> 8);
        }
    }
    if (run) {      /* type RUN (or SYS) <return> through the keyboard buffer ($0527, its count at $EF) */
        char cmd[16];
        snprintf(cmd, sizeof(cmd), basic ? "RUN\r" : "SYS%u\r", start);
        int len = (int)strlen(cmd);
        for (int i = 0; i < len; i++) plus4_ram_poke(b, (uint16_t)(0x0527 + i), (uint8_t)cmd[i]);
        plus4_ram_poke(b, 0xEF, (uint8_t)len);
    }
    snprintf(msg, (size_t)msglen, "loaded %s at $%04X-$%04X", path, start, end - 1);
    return true;
}

static void plus4_destroy(board_t *b) { (void)b; }

static const char *plus4_cpu_status(board_t *b) {
    const plus4_t *z = PLUS4(b);
    if (b->sim.parts[z->p_cpu].dead) return "dead";
    if (!(z->rom_mask & 2)) return "no KERNAL ROM (put the Plus/4 ROMs in roms/plus4)";
    if (!(z->rom_mask & 1)) return "no BASIC ROM (put the Plus/4 ROMs in roms/plus4)";
    return "running";
}

static void plus4_cpu_regs(board_t *b, char *buf, int len) { c16_cpu_regs_of(&b->sim.parts[PLUS4(b)->p_cpu], buf, len); }

static void plus4_print_text(board_t *b) {
    /* The screen as the video matrix holds it (screen codes to ASCII). */
    plus4_t *z = PLUS4(b);
    const part_t *ted = &b->sim.parts[z->p_ted];
    unsigned base = ((unsigned)(ted_reg_of(ted, 0x14) & 0xF8) << 8) + 0x400;
    for (int y = 0; y < 25; y++) {
        char line[48];
        for (int x = 0; x < 40; x++) {
            int c = plus4_ram_peek(b, (uint16_t)((base + (unsigned)(y * 40 + x)) & 0xFFFF)) & 0x7F;
            line[x] = c == 0 ? '@' : c < 27 ? (char)('A' + c - 1) : c >= 32 && c < 64 ? (char)c : '.';
        }
        line[40] = 0;
        printf("%s\n", line);
    }
}

const machine_t machine_plus4 = {
    .size = sizeof(plus4_t),
    .sim = { .rail_of = plus4_rail_of, .kind_of = plus4_kind_of, .cycles_per_us = 0.894886 },
    .cpu_hz = PLUS4_FREQ,
    .cycles_per_frame = PLUS4_CYCLES_PER_FRAME,
    .steps_per_cycle = PLUS4_STEPS_PER_CYCLE,
    .screen_w = PLUS4_SCREEN_W, .screen_h = PLUS4_SCREEN_H,
    .ready_cycles = 3 * PLUS4_FREQ,
    .conns = plus4_conn_defs, .nconns = sizeof(plus4_conn_defs) / sizeof(plus4_conn_defs[0]),
    .link_resistors = "RP3 RP4",
    .crystals = "Y1",
    .diodes = "CR", .jumpers = "",
    .supplies = "5V_RAW 5V_F 5V_SW 9V_IN 9V_B 9V_L 9V_SW 9V_FUSED UNREG",
    .keys_help = "Keys: type; Esc = RUN/STOP, Insert = ESC, Tab = CTRL, Left Alt = C=, Home = HOME, Backspace = INST/DEL, arrow keys, F1-F3, F4 = HELP; joystick 1 (port 0): keypad 8 2 4 6, 0 = fire; joystick 2 (port 1): PgUp PgDn Del End, Right Ctrl = fire",
    .reset_help = "Hold to press RESET: shorts the 8501 /RES (U2 pin 40) to GND",
    .create = plus4_create,
    .destroy = plus4_destroy,
    .power_on = plus4_power_on,
    .run = plus4_run,
    .joins = plus4_joins,
    .key_down = plus4_key_down,
    .key_up = plus4_key_up,
    .frame_update = plus4_frame_update,
    .host_key = plus4_host_key,
    .host_char = plus4_host_char,
    .monitor = plus4_monitor,
    .files_title = "Open a program or save state",
    .file_kind = plus4_file_kind,
    .load = plus4_load,
    .cpu_status = plus4_cpu_status,
    .cpu_regs = plus4_cpu_regs,
    .print_text = plus4_print_text,
    .id = "plus4",
    .issue = "310164",
    .name = "Commodore Plus/4",
    .board_name = "Plus/4 (schematic 310164)",
    .data = &bd_plus4,
    .parts = plus4_part_defs, .nparts = sizeof(plus4_part_defs) / sizeof(plus4_part_defs[0]),
};
