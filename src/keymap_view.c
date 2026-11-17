/* keymap_view.c - the "Keyboard" window: the machine's keys with the PC key that types each.
 *  - the Spectrum family's 40 keys (the ZX Spectrum 48K boards and the TS2068 scan the same 8 x 5 matrix):
 *    the BASIC keyword in K mode, the key itself, the SYMBOL SHIFT legend, and under them what to press on the
 *    PC (the key, and the character that types the SYMBOL SHIFT legend: the table of ts2068.c's keymap);
 *  - the C64's 66 keys in their physical places (the PC mapping of c64.c: c64_host_key, c64_host_char);
 *  - the PET's keys as the editor ROM's key table has them (pet.c), in the order of the scanned matrix:
 *    the 4032's graphics keyboard and the 8032's business one are not laid out like the real keyboard here. */
#include "keymap_view.h"
#include <math.h>
#include "ts2068.h"
#include "ts1000.h"
#include "ts1500.h"
#include "zx80.h"
#include "zx48.h"
#include "c64.h"
#include "pet.h"
#include "vic20.h"

typedef struct {
    const char *cap;        /* the key's own legend */
    const char *keyword;    /* K mode: the BASIC keyword (letters) */
    const char *sym;        /* the SYMBOL SHIFT legend */
    const char *pc;         /* what to press on the PC for the key itself */
    const char *pcsym;      /* the PC character that types the SYMBOL SHIFT legend; NULL: hold Tab and press the key */
    int kind;               /* 0 normal, 1 CAPS SHIFT, 2 SYMBOL SHIFT, 3 ENTER, 4 SPACE */
} skey_t;

static const skey_t row1[10] = {
    { "1", "", "!", "1", "!", 0 }, { "2", "", "@", "2", "@", 0 }, { "3", "", "#", "3", "#", 0 },
    { "4", "", "$", "4", "$", 0 }, { "5", "", "%", "5", "%", 0 }, { "6", "", "&", "6", "&", 0 },
    { "7", "", "'", "7", "'", 0 }, { "8", "", "(", "8", "(", 0 }, { "9", "", ")", "9", ")", 0 },
    { "0", "", "_", "0", "_", 0 } };
static const skey_t row2[10] = {
    { "Q", "PLOT", "<=", "Q", NULL, 0 }, { "W", "DRAW", "<>", "W", NULL, 0 }, { "E", "REM", ">=", "E", NULL, 0 },
    { "R", "RUN", "<", "R", "<", 0 }, { "T", "RAND", ">", "T", ">", 0 }, { "Y", "RETURN", "AND", "Y", NULL, 0 },
    { "U", "IF", "OR", "U", NULL, 0 }, { "I", "INPUT", "AT", "I", NULL, 0 }, { "O", "POKE", ";", "O", ";", 0 },
    { "P", "PRINT", "\"", "P", "\"", 0 } };
static const skey_t row3[10] = {
    { "A", "NEW", "STOP", "A", NULL, 0 }, { "S", "SAVE", "NOT", "S", NULL, 0 }, { "D", "DIM", "STEP", "D", NULL, 0 },
    { "F", "FOR", "TO", "F", NULL, 0 }, { "G", "GOTO", "THEN", "G", NULL, 0 }, { "H", "GO SUB", "^", "H", "^", 0 },
    { "J", "LOAD", "-", "J", "-", 0 }, { "K", "LIST", "+", "K", "+", 0 }, { "L", "LET", "=", "L", "=", 0 },
    { "ENTER", "", "", "Enter", NULL, 3 } };
static const skey_t row4[10] = {
    { "CAPS SHIFT", "", "", "L Shift", NULL, 1 }, { "Z", "COPY", ":", "Z", ":", 0 }, { "X", "CLEAR", "\xC2\xA3", "X", NULL, 0 },
    { "C", "CONT", "?", "C", "?", 0 }, { "V", "CLS", "/", "V", "/", 0 }, { "B", "BORDER", "*", "B", "*", 0 },
    { "N", "NEXT", ",", "N", ",", 0 }, { "M", "PAUSE", ".", "M", ".", 0 },
    { "SYMBOL SHIFT", "", "", "Tab", NULL, 2 }, { "SPACE", "BREAK", "", "Space", NULL, 4 } };

/* The TS1000 / ZX81: 40 keys, the K-mode keyword above, the SHIFT legend below (decoded from the ROM's key tables). */
static const skey_t zrow1[10] = {
    { "1", "", "EDIT", "1", "Tab", 0 }, { "2", "", "AND", "2", "-", 0 }, { "3", "", "THEN", "3", "-", 0 },
    { "4", "", "TO", "4", "-", 0 }, { "5", "", "\xE2\x86\x90", "5", "Left", 0 }, { "6", "", "\xE2\x86\x93", "6", "Down", 0 },
    { "7", "", "\xE2\x86\x91", "7", "Up", 0 }, { "8", "", "\xE2\x86\x92", "8", "Right", 0 },
    { "9", "", "GRAPHICS", "9", "-", 0 }, { "0", "", "RUBOUT", "0", "Bksp", 0 } };
static const skey_t zrow2[10] = {
    { "Q", "PLOT", "", "Q", "-", 0 }, { "W", "UNPLOT", "OR", "W", "-", 0 }, { "E", "REM", "STEP", "E", "-", 0 },
    { "R", "RUN", "<=", "R", "-", 0 }, { "T", "RAND", "<>", "T", "-", 0 }, { "Y", "RETURN", ">=", "Y", "-", 0 },
    { "U", "IF", "$", "U", "$", 0 }, { "I", "INPUT", "(", "I", "(", 0 }, { "O", "POKE", ")", "O", ")", 0 },
    { "P", "PRINT", "\"", "P", "\"", 0 } };
static const skey_t zrow3[10] = {
    { "A", "NEW", "STOP", "A", "-", 0 }, { "S", "SAVE", "LPRINT", "S", "-", 0 }, { "D", "DIM", "SLOW", "D", "-", 0 },
    { "F", "FOR", "FAST", "F", "-", 0 }, { "G", "GOTO", "LLIST", "G", "-", 0 }, { "H", "GOSUB", "**", "H", "-", 0 },
    { "J", "LOAD", "-", "J", "-", 0 }, { "K", "LIST", "+", "K", "+", 0 }, { "L", "LET", "=", "L", "=", 0 },
    { "NEWLINE", "", "", "Enter", NULL, 3 } };
static const skey_t zrow4[10] = {
    { "SHIFT", "", "", "L Shift", NULL, 1 }, { "Z", "COPY", ":", "Z", ":", 0 }, { "X", "CLEAR", ";", "X", ";", 0 },
    { "C", "CONT", "?", "C", "?", 0 }, { "V", "CLS", "/", "V", "/", 0 }, { "B", "SCROLL", "*", "B", "*", 0 },
    { "N", "NEXT", "<", "N", "<", 0 }, { "M", "PAUSE", ">", "M", ">", 0 },
    { ".", "", ",", ".", ",", 0 }, { "SPACE", "BREAK", "\xC2\xA3", "Space", NULL, 4 } };
/* The ZX80's keyboard (from a photo): the keyword above the key, the key, the shifted legend. */
static const skey_t yrow1[10] = {
    { "1", "NOT", "", "1", "-", 0 }, { "2", "AND", "", "2", "-", 0 }, { "3", "THEN", "", "3", "-", 0 },
    { "4", "TO", "", "4", "-", 0 }, { "5", "", "\xE2\x86\x90", "5", "Left", 0 }, { "6", "", "\xE2\x86\x93", "6", "Down", 0 },
    { "7", "", "\xE2\x86\x91", "7", "Up", 0 }, { "8", "", "\xE2\x86\x92", "8", "Right", 0 },
    { "9", "HOME", "", "9", "-", 0 }, { "0", "RUBOUT", "", "0", "Bksp", 0 } };
static const skey_t yrow2[10] = {
    { "Q", "NEW", "", "Q", "-", 0 }, { "W", "LOAD", "", "W", "-", 0 }, { "E", "SAVE", "", "E", "-", 0 },
    { "R", "RUN", "", "R", "-", 0 }, { "T", "CONT", "", "T", "-", 0 }, { "Y", "REM", "\"", "Y", "\"", 0 },
    { "U", "IF", "$", "U", "$", 0 }, { "I", "INPUT", "(", "I", "(", 0 }, { "O", "PRINT", ")", "O", ")", 0 },
    { "P", "", "*", "P", "*", 0 } };
static const skey_t yrow3[10] = {
    { "A", "LIST", "", "A", "-", 0 }, { "S", "STOP", "", "S", "-", 0 }, { "D", "DIM", "", "D", "-", 0 },
    { "F", "FOR", "", "F", "-", 0 }, { "G", "GOTO", "", "G", "-", 0 }, { "H", "POKE", "**", "H", "^", 0 },
    { "J", "RAND", "-", "J", "-", 0 }, { "K", "LET", "+", "K", "+", 0 }, { "L", "", "=", "L", "=", 0 },
    { "NEWLINE", "EDIT", "", "Enter", NULL, 3 } };
static const skey_t yrow4[10] = {
    { "SHIFT", "", "", "L Shift", NULL, 1 }, { "Z", "", ":", "Z", ":", 0 }, { "X", "CLEAR", ";", "X", ";", 0 },
    { "C", "CLS", "?", "C", "?", 0 }, { "V", "GOSUB", "/", "V", "/", 0 }, { "B", "RET", "OR", "B", "-", 0 },
    { "N", "NEXT", "<", "N", "<", 0 }, { "M", "", ">", "M", ">", 0 },
    { ".", "", ",", ".", ",", 0 }, { "SPACE", "BREAK", "\xC2\xA3", "Space", NULL, 4 } };
static bool zx81_mode;      /* draw_key: a SHIFT legend without a PC character is typed with Shift held */

static void line(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect r, const char *s, struct nk_color c) {
    if (s && *s) text_centered(cv, ctx, r, s, c);
}

static bool is_spectrum(const machine_t *m) {
    return m == &machine_ts2068 || m == &machine_zx48 || m == &machine_zx48i2 || m == &machine_zx48i1 ||
           m == &machine_zx48i4b || m == &machine_zx48i6a;
}
static bool is_ts1000(const machine_t *m) { return m == &machine_ts1000 || m == &machine_ts1500 || m == &machine_zx80; }   /* the TS1500 has the same 40 key keyboard */
static bool is_c64(const machine_t *m) { return m == &machine_c64 || m == &machine_c64b || m == &machine_c64c; }
/* The VIC-20 keyboard has the C64's layout (and the same PC keys). */
static bool is_vic20(const machine_t *m) { return m == &machine_vic20a || m == &machine_vic20b; }
static bool is_c16(const machine_t *m) { return m == &machine_c16 || m == &machine_plus4; }  /* the Plus/4 has the same keyboard */
static bool is_c128(const machine_t *m) { return m == &machine_c128; }
static bool is_pet(const machine_t *m) { return m == &machine_pet4032 || m == &machine_pet8032; }

bool keymap_supported(const machine_t *m) { return is_spectrum(m) || is_ts1000(m) || is_c64(m) || is_vic20(m) || is_c16(m) || is_c128(m) || is_pet(m); }

/* A key of the other machines: the shifted legend (red), the key (white), the PC key (green); style: 0 normal,
 * 1 modifier, 2 no PC key. */
static void draw_gkey(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect r, const char *top,
                      const char *main, const char *pc, int style) {
    struct nk_rect b = nk_rect(r.x + 1.5f, r.y + 1.5f, r.w - 3, r.h - 3);
    nk_fill_rect(cv, b, 3, style == 1 ? nk_rgb(70, 70, 100) : nk_rgb(58, 58, 64));
    nk_stroke_rect(cv, b, 3, 1, nk_rgb(120, 120, 130));
    float fh = ctx->style.font->height + 1;
    bool two = top && *top;
    if (two) line(cv, ctx, nk_rect(b.x, b.y + 1, b.w, fh), top, nk_rgb(255, 110, 110));
    line(cv, ctx, nk_rect(b.x, b.y + (two ? fh : (b.h - fh) / 2 - fh / 2), b.w, fh + 3), main, nk_rgb(240, 240, 230));
    line(cv, ctx, nk_rect(b.x, b.y + b.h - fh - 1, b.w, fh), pc, style == 2 ? nk_rgb(120, 120, 120) : nk_rgb(180, 230, 160));
}

/* One C64 key: legend, shifted legend, PC key; x and width in key units. */
typedef struct { float x, w; const char *top, *main, *pc; int style; } c64key_t;
#define K1(t, m, p) { 0, 1, t, m, p, 0 }

static void draw_c64(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect a) {
    static const c64key_t r0[] = { { 0, 1, "", "\xE2\x86\x90", "-", 2 }, K1("!", "1", "1  !"), K1("\"", "2", "2  \""), K1("#", "3", "3  #"),
        K1("$", "4", "4  $"), K1("%", "5", "5  %"), K1("&", "6", "6  &"), K1("'", "7", "7  '"), K1("(", "8", "8  ("),
        K1(")", "9", "9  )"), K1("", "0", "0"), K1("", "+", "+"), K1("", "-", "-"), K1("", "\xC2\xA3", "~"),
        { 0, 1, "CLR", "HOME", "Home", 0 }, { 0, 1, "INST", "DEL", "Bksp", 0 } };
    static const c64key_t r1[] = { { 0, 1.5f, "", "CTRL", "Tab", 1 }, K1("", "Q", "Q"), K1("", "W", "W"), K1("", "E", "E"),
        K1("", "R", "R"), K1("", "T", "T"), K1("", "Y", "Y"), K1("", "U", "U"), K1("", "I", "I"), K1("", "O", "O"),
        K1("", "P", "P"), K1("", "@", "@"), K1("", "*", "*"), { 0, 1, "", "\xE2\x86\x91", "-", 2 },
        { 0, 1.5f, "", "RESTORE", "PgUp", 1 } };
    static const c64key_t r2[] = { { 0, 1, "RUN", "STOP", "Esc", 1 }, { 0, 1, "SHIFT", "LOCK", "-", 2 }, K1("", "A", "A"),
        K1("", "S", "S"), K1("", "D", "D"), K1("", "F", "F"), K1("", "G", "G"), K1("", "H", "H"), K1("", "J", "J"),
        K1("", "K", "K"), K1("", "L", "L"), K1("[", ":", ":  ["), K1("]", ";", ";  ]"), K1("", "=", "="),
        { 0, 2, "", "RETURN", "Enter", 1 } };
    static const c64key_t r3[] = { { 0, 1, "", "C=", "L Alt", 1 }, { 0, 1, "", "SHIFT", "Shift", 1 }, K1("", "Z", "Z"),
        K1("", "X", "X"), K1("", "C", "C"), K1("", "V", "V"), K1("", "B", "B"), K1("", "N", "N"), K1("", "M", "M"),
        K1("<", ",", ",  <"), K1(">", ".", ".  >"), K1("?", "/", "/  ?"), { 0, 2, "", "SHIFT", "Shift", 1 },
        { 0, 1, "", "CRSR", "Dn/Up", 0 } , { 0, 1, "", "CRSR", "Rt/Lt", 0 } };
    static const struct { const c64key_t *k; int n; } rows[4] = { { r0, 16 }, { r1, 15 }, { r2, 15 }, { r3, 15 } };
    float u = a.w / 17.2f, kh = 56;
    for (int y = 0; y < 4; y++) {
        float x = 0;
        for (int i = 0; i < rows[y].n; i++) {
            const c64key_t *k = &rows[y].k[i];
            draw_gkey(cv, ctx, nk_rect(a.x + x * u, a.y + y * kh, k->w * u, kh), k->top, k->main, k->pc, k->style);
            x += k->w;
        }
    }
    draw_gkey(cv, ctx, nk_rect(a.x + 3.5f * u, a.y + 4 * kh, 8 * u, kh), "", "SPACE", "Space", 0);
    static const char *const fk[4][3] = { { "f2", "f1", "F1" }, { "f4", "f3", "F3" }, { "f6", "f5", "F5" }, { "f8", "f7", "F7" } };
    for (int i = 0; i < 4; i++)
        draw_gkey(cv, ctx, nk_rect(a.x + 16.2f * u, a.y + i * kh, 1.0f * u, kh), fk[i][0], fk[i][1], fk[i][2], 0);
}

/* The C16's keyboard (c16.c's matrix): the C64's typewriter layout with its own cursor keys and F1-F3 + HELP. */
static void draw_c16(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect a) {
    static const c64key_t r0[] = { { 0, 1, "", "ESC", "Ins", 1 }, K1("!", "1", "1  !"), K1("\"", "2", "2  \""), K1("#", "3", "3  #"),
        K1("$", "4", "4  $"), K1("%", "5", "5  %"), K1("&", "6", "6  &"), K1("'", "7", "7  '"), K1("(", "8", "8  ("),
        K1(")", "9", "9  )"), K1("", "0", "0"), K1("", "+", "+"), K1("", "-", "-"), K1("", "\xC2\xA3", "~"),
        { 0, 1, "CLR", "HOME", "Home", 0 }, { 0, 1, "INST", "DEL", "Bksp", 0 } };
    static const c64key_t r1[] = { { 0, 1.5f, "", "CTRL", "Tab", 1 }, K1("", "Q", "Q"), K1("", "W", "W"), K1("", "E", "E"),
        K1("", "R", "R"), K1("", "T", "T"), K1("", "Y", "Y"), K1("", "U", "U"), K1("", "I", "I"), K1("", "O", "O"),
        K1("", "P", "P"), K1("", "@", "@"), K1("", "*", "*"), { 0, 1, "", "UP", "Up", 0 } };
    static const c64key_t r2[] = { { 0, 1, "RUN", "STOP", "Esc", 1 }, K1("", "A", "A"),
        K1("", "S", "S"), K1("", "D", "D"), K1("", "F", "F"), K1("", "G", "G"), K1("", "H", "H"), K1("", "J", "J"),
        K1("", "K", "K"), K1("", "L", "L"), K1("[", ":", ":  ["), K1("]", ";", ";  ]"), K1("", "=", "="),
        { 0, 2, "", "RETURN", "Enter", 1 } };
    static const c64key_t r3[] = { { 0, 1, "", "C=", "L Alt", 1 }, { 0, 1.5f, "", "SHIFT", "Shift", 1 }, K1("", "Z", "Z"),
        K1("", "X", "X"), K1("", "C", "C"), K1("", "V", "V"), K1("", "B", "B"), K1("", "N", "N"), K1("", "M", "M"),
        K1("<", ",", ",  <"), K1(">", ".", ".  >"), K1("?", "/", "/  ?"), { 0, 1, "", "LEFT", "Left", 0 },
        { 0, 1, "", "DOWN", "Down", 0 }, { 0, 1, "", "RIGHT", "Right", 0 } };
    static const struct { const c64key_t *k; int n; } rows[4] = { { r0, 16 }, { r1, 14 }, { r2, 14 }, { r3, 15 } };
    float u = a.w / 17.2f, kh = 56;
    for (int y = 0; y < 4; y++) {
        float x = 0;
        for (int i = 0; i < rows[y].n; i++) {
            const c64key_t *k = &rows[y].k[i];
            draw_gkey(cv, ctx, nk_rect(a.x + x * u, a.y + y * kh, k->w * u, kh), k->top, k->main, k->pc, k->style);
            x += k->w;
        }
    }
    draw_gkey(cv, ctx, nk_rect(a.x + 3.5f * u, a.y + 4 * kh, 8 * u, kh), "", "SPACE", "Space", 0);
    static const char *const fk[4][3] = { { "", "F1", "F1" }, { "", "F2", "F2" }, { "", "F3", "F3" }, { "", "HELP", "F4" } };
    for (int i = 0; i < 4; i++)
        draw_gkey(cv, ctx, nk_rect(a.x + 16.2f * u, a.y + i * kh, 1.0f * u, kh), fk[i][0], fk[i][1], fk[i][2], 0);
}

/* The C128's keyboard: the C64's typewriter block plus the top row (ESC TAB ALT CAPS and HELP LF 40/80 NO SCROLL), the
 * cursor cluster and the numeric keypad (c128.c's matrix: the extra keys are on the VIC's K0-K2 lines). */
static void draw_c128(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect a) {
    static const c64key_t r0[] = { { 0, 1, "", "\xE2\x86\x90", "-", 2 }, K1("!", "1", "1  !"), K1("\"", "2", "2  \""), K1("#", "3", "3  #"),
        K1("$", "4", "4  $"), K1("%", "5", "5  %"), K1("&", "6", "6  &"), K1("'", "7", "7  '"), K1("(", "8", "8  ("),
        K1(")", "9", "9  )"), K1("", "0", "0"), K1("", "+", "+"), K1("", "-", "-"), K1("", "\xC2\xA3", "~"),
        { 0, 1, "CLR", "HOME", "Home", 0 }, { 0, 1, "INST", "DEL", "Bksp", 0 } };
    static const c64key_t r1[] = { { 0, 1.5f, "", "CTRL", "Tab", 1 }, K1("", "Q", "Q"), K1("", "W", "W"), K1("", "E", "E"),
        K1("", "R", "R"), K1("", "T", "T"), K1("", "Y", "Y"), K1("", "U", "U"), K1("", "I", "I"), K1("", "O", "O"),
        K1("", "P", "P"), K1("", "@", "@"), K1("", "*", "*"), { 0, 1, "", "\xE2\x86\x91", "-", 2 },
        { 0, 1.5f, "", "RESTORE", "PgUp", 1 } };
    static const c64key_t r2[] = { { 0, 1, "RUN", "STOP", "Esc", 1 }, { 0, 1, "SHIFT", "LOCK", "-", 2 }, K1("", "A", "A"),
        K1("", "S", "S"), K1("", "D", "D"), K1("", "F", "F"), K1("", "G", "G"), K1("", "H", "H"), K1("", "J", "J"),
        K1("", "K", "K"), K1("", "L", "L"), K1("[", ":", ":  ["), K1("]", ";", ";  ]"), K1("", "=", "="),
        { 0, 2, "", "RETURN", "Enter", 1 } };
    static const c64key_t r3[] = { { 0, 1, "", "C=", "L Alt", 1 }, { 0, 1, "", "SHIFT", "Shift", 1 }, K1("", "Z", "Z"),
        K1("", "X", "X"), K1("", "C", "C"), K1("", "V", "V"), K1("", "B", "B"), K1("", "N", "N"), K1("", "M", "M"),
        K1("<", ",", ",  <"), K1(">", ".", ".  >"), K1("?", "/", "/  ?"), { 0, 2, "", "SHIFT", "Shift", 1 },
        { 0, 1, "", "CRSR", "-", 2 }, { 0, 1, "", "CRSR", "-", 2 } };
    static const struct { const c64key_t *k; int n; } rows[4] = { { r0, 16 }, { r1, 15 }, { r2, 15 }, { r3, 15 } };
    float u = a.w / 25.0f, kh = 46, y0 = kh + 6;          /* y0: below the top row */
    static const struct { const char *top, *main, *pc; int style; } toprow[4] = {
        { "", "ESC", "Ins", 1 }, { "", "TAB", "PgDn", 1 }, { "", "ALT", "R Alt", 1 }, { "", "CAPS", "F12", 1 } };
    for (int i = 0; i < 4; i++)
        draw_gkey(cv, ctx, nk_rect(a.x + i * u, a.y, u, kh), toprow[i].top, toprow[i].main, toprow[i].pc, toprow[i].style);
    for (int y = 0; y < 4; y++) {
        float x = 0;
        for (int i = 0; i < rows[y].n; i++) {
            const c64key_t *k = &rows[y].k[i];
            draw_gkey(cv, ctx, nk_rect(a.x + x * u, a.y + y0 + y * kh, k->w * u, kh), k->top, k->main, k->pc, k->style);
            x += k->w;
        }
    }
    draw_gkey(cv, ctx, nk_rect(a.x + 3.5f * u, a.y + y0 + 4 * kh, 8 * u, kh), "", "SPACE", "Space", 0);
    static const char *const fk[4][3] = { { "f2", "f1", "F1" }, { "f4", "f3", "F3" }, { "f6", "f5", "F5" }, { "f8", "f7", "F7" } };
    for (int i = 0; i < 4; i++)
        draw_gkey(cv, ctx, nk_rect(a.x + 16.4f * u, a.y + y0 + i * kh, 1.0f * u, kh), fk[i][0], fk[i][1], fk[i][2], 0);
    /* the top row of the right hand block */
    static const char *const hk[4][3] = { { "", "HELP", "F9" }, { "", "LINE FEED", "F10" }, { "", "40/80", "F11" }, { "", "NO SCROLL", "ScrLk" } };
    for (int i = 0; i < 4; i++)
        draw_gkey(cv, ctx, nk_rect(a.x + (18.2f + i) * u, a.y, u, kh), hk[i][0], hk[i][1], hk[i][2], i == 2 ? 1 : 0);
    /* the cursor cluster (an inverted T) and the keypad */
    static const char *const cur[4][2] = { { "UP", "Up" }, { "LEFT", "Left" }, { "DOWN", "Down" }, { "RIGHT", "Right" } };
    draw_gkey(cv, ctx, nk_rect(a.x + 18.2f * u, a.y + y0 + 2 * kh, u, kh), "", cur[0][0], cur[0][1], 0);
    draw_gkey(cv, ctx, nk_rect(a.x + 17.2f * u, a.y + y0 + 3 * kh, u, kh), "", cur[1][0], cur[1][1], 0);
    draw_gkey(cv, ctx, nk_rect(a.x + 18.2f * u, a.y + y0 + 3 * kh, u, kh), "", cur[2][0], cur[2][1], 0);
    draw_gkey(cv, ctx, nk_rect(a.x + 19.2f * u, a.y + y0 + 3 * kh, u, kh), "", cur[3][0], cur[3][1], 0);
    static const char *const kp[4][4] = { { "7", "8", "9", "+" }, { "4", "5", "6", "-" }, { "1", "2", "3", "ENTER" }, { "0", "0", ".", "" } };
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            const char *l = kp[y][x];
            if (!*l || (y == 3 && x == 1)) continue;
            char pc[16];
            snprintf(pc, sizeof(pc), strcmp(l, "ENTER") ? "Num %s" : "Num Ent", l);
            float w = (y == 3 && x == 0) ? 2.0f : 1.0f, h = strcmp(l, "ENTER") ? 1.0f : 2.0f;
            draw_gkey(cv, ctx, nk_rect(a.x + (21.2f + x) * u, a.y + y0 + y * kh, w * u, h * kh), "", l, pc, 0);
        }
}

/* The PET's key matrix (pet.c's table), as the 10 rows the keyboard is scanned in. */
static void pet_pc(const char *n, bool cols80, char *out, size_t sz, char *top, size_t tsz) {
    top[0] = 0;
    if (!strcmp(n, "~")) { out[0] = 0; return; }
    static const struct { const char *n, *pc; } named[] = {
        { "RETURN", "Enter" }, { "DEL", "Bksp" }, { "UP", "Up" }, { "DOWN", "Down" }, { "RIGHT", "Rt/Lt" },
        { "HOME", "Home/Del" }, { "STOP", "Esc" }, { "RVS", "End" }, { "TAB", "Tab" }, { "RSHIFT", "R Shift" },
        { "LSHIFT", "Shift" }, { "SPACE", "Space" }, { "LARROW", "-" }, { "ESC", "-" }, { "RPT", "-" }, { "SLOCK", "-" } };
    for (size_t i = 0; i < sizeof named / sizeof named[0]; i++)
        if (!strcmp(n, named[i].n)) { snprintf(out, sz, "%s", named[i].pc); return; }
    if (n[0] == 'K' && n[1] >= '0' && n[1] <= '9' && !n[2]) { snprintf(out, sz, "-"); return; }   /* the keypad: no PC key */
    if (n[1] == 0) {
        static const char *const from = "1234567890;:,./-", *const to = "!\"#$%&'() +*<>?=";
        const char *q = cols80 ? strchr(from, n[0]) : NULL;
        if (q && to[q - from] != ' ') {
            snprintf(top, tsz, "%c", to[q - from]);
            snprintf(out, sz, "%c  %c", n[0], to[q - from]);
        } else snprintf(out, sz, "%s", n);
        return;
    }
    snprintf(out, sz, "%s", n);
}

/* The PET keyboards as typewriter-style keyboards: x, y and width in key units; the token is the key's name
 * in pet.c's table (so the PC key comes from it). Both layouts are from photos, except the graphics
 * keyboard's [ ] @ < > keys, which are placed from the matrix. */
typedef struct { float x, y, w; const char *tok; } pkey_t;
#define PK(x, y, tok) { x, y, 1, tok }
static const pkey_t pet_gfx[] = {      /* from a photo of a 4032's keyboard */
    PK(0, 0, "@"), PK(1, 0, "!"), PK(2, 0, "\""), PK(3, 0, "#"), PK(4, 0, "$"), PK(5, 0, "%"), PK(6, 0, "'"), PK(7, 0, "&"),
    PK(8, 0, "\\"), PK(9, 0, "("), PK(10, 0, ")"), PK(11, 0, "LARROW"), PK(12, 0, "["), PK(13, 0, "]"),
    { 0, 1, 1.5f, "RVS" }, PK(1.5f, 1, "Q"), PK(2.5f, 1, "W"), PK(3.5f, 1, "E"), PK(4.5f, 1, "R"), PK(5.5f, 1, "T"),
    PK(6.5f, 1, "Y"), PK(7.5f, 1, "U"), PK(8.5f, 1, "I"), PK(9.5f, 1, "O"), PK(10.5f, 1, "P"), PK(11.5f, 1, "UP"),
    PK(12.5f, 1, "<"), PK(13.5f, 1, ">"),
    { 0, 2, 1.5f, "SLOCK" }, PK(1.5f, 2, "A"), PK(2.5f, 2, "S"), PK(3.5f, 2, "D"), PK(4.5f, 2, "F"), PK(5.5f, 2, "G"),
    PK(6.5f, 2, "H"), PK(7.5f, 2, "J"), PK(8.5f, 2, "K"), PK(9.5f, 2, "L"), PK(10.5f, 2, ":"), PK(11.5f, 2, "STOP"),
    { 12.5f, 2, 1.9f, "RETURN" },
    { 0, 3, 1.7f, "LSHIFT" }, PK(1.7f, 3, "Z"), PK(2.7f, 3, "X"), PK(3.7f, 3, "C"), PK(4.7f, 3, "V"), PK(5.7f, 3, "B"),
    PK(6.7f, 3, "N"), PK(7.7f, 3, "M"), PK(8.7f, 3, ","), PK(9.7f, 3, ";"), PK(10.7f, 3, "?"), { 11.7f, 3, 2.7f, "RSHIFT" },
    { 2.3f, 4, 9.3f, "SPACE" },
    PK(15.3f, 0, "HOME"), PK(16.3f, 0, "DOWN"), PK(17.3f, 0, "RIGHT"), PK(18.3f, 0, "DEL"),
    PK(15.3f, 1, "7"), PK(16.3f, 1, "8"), PK(17.3f, 1, "9"), PK(18.3f, 1, "/"),
    PK(15.3f, 2, "4"), PK(16.3f, 2, "5"), PK(17.3f, 2, "6"), PK(18.3f, 2, "*"),
    PK(15.3f, 3, "1"), PK(16.3f, 3, "2"), PK(17.3f, 3, "3"), PK(18.3f, 3, "+"),
    PK(15.3f, 4, "0"), PK(16.3f, 4, "."), PK(17.3f, 4, "-"), PK(18.3f, 4, "=") };
static const pkey_t pet_biz[] = {      /* from a photo of an 8032's keyboard */
    PK(0.5f, 0, "LARROW"), PK(1.5f, 0, "1"), PK(2.5f, 0, "2"), PK(3.5f, 0, "3"), PK(4.5f, 0, "4"), PK(5.5f, 0, "5"),
    PK(6.5f, 0, "6"), PK(7.5f, 0, "7"), PK(8.5f, 0, "8"), PK(9.5f, 0, "9"), PK(10.5f, 0, "0"), PK(11.5f, 0, ":"),
    PK(12.5f, 0, "-"), PK(13.5f, 0, "UP"), PK(14.5f, 0, "RIGHT"), PK(15.5f, 0, "STOP"),
    { 0.4f, 1, 1.5f, "TAB" }, PK(1.9f, 1, "Q"), PK(2.9f, 1, "W"), PK(3.9f, 1, "E"), PK(4.9f, 1, "R"), PK(5.9f, 1, "T"),
    PK(6.9f, 1, "Y"), PK(7.9f, 1, "U"), PK(8.9f, 1, "I"), PK(9.9f, 1, "O"), PK(10.9f, 1, "P"), PK(11.9f, 1, "["),
    PK(12.9f, 1, "\\"), PK(13.9f, 1, "DOWN"), PK(14.9f, 1, "DEL"),
    { 0, 2, 0.9f, "ESC" }, { 0.9f, 2, 1, "SLOCK" }, PK(1.9f, 2, "A"), PK(2.9f, 2, "S"), PK(3.9f, 2, "D"), PK(4.9f, 2, "F"),
    PK(5.9f, 2, "G"), PK(6.9f, 2, "H"), PK(7.9f, 2, "J"), PK(8.9f, 2, "K"), PK(9.9f, 2, "L"), PK(10.9f, 2, ";"),
    PK(11.9f, 2, "@"), PK(12.9f, 2, "]"), { 13.9f, 2, 2, "RETURN" },
    PK(0, 3, "RVS"), { 1, 3, 1.4f, "LSHIFT" }, PK(2.4f, 3, "Z"), PK(3.4f, 3, "X"), PK(4.4f, 3, "C"), PK(5.4f, 3, "V"),
    PK(6.4f, 3, "B"), PK(7.4f, 3, "N"), PK(8.4f, 3, "M"), PK(9.4f, 3, ","), PK(10.4f, 3, "."), PK(11.4f, 3, "/"),
    { 12.4f, 3, 2, "RSHIFT" }, PK(14.4f, 3, "RPT"), { 15.4f, 3, 1.1f, "HOME" },
    { 2.6f, 4, 9.6f, "SPACE" },
    PK(17.4f, 0, "K7"), PK(18.4f, 0, "K8"), PK(19.4f, 0, "K9"),
    PK(17.4f, 1, "K4"), PK(18.4f, 1, "K5"), PK(19.4f, 1, "K6"),
    PK(17.4f, 2, "K1"), PK(18.4f, 2, "K2"), PK(19.4f, 2, "K3"),
    { 17.4f, 3, 3, "K0" } };

/* The chiclet keyboard of the PET 2001 (from a photo): the same matrix as the graphics keyboard, other places. RETURN is two rows tall. */
#define CK(x, y, tok) { x, y, 1, tok }
static const pkey_t pet_chic[] = {
    CK(0.55f, 0, "!"), CK(1.53f, 0, "\""), CK(2.51f, 0, "#"), CK(3.49f, 0, "$"), CK(4.47f, 0, "%"), CK(5.45f, 0, "'"),
    CK(6.43f, 0, "&"), CK(7.41f, 0, "\\"), CK(8.39f, 0, "("), CK(9.37f, 0, ")"), CK(10.35f, 0, "LARROW"),
    CK(0.40f, 1, "Q"), CK(1.38f, 1, "W"), CK(2.36f, 1, "E"), CK(3.34f, 1, "R"), CK(4.32f, 1, "T"), CK(5.30f, 1, "Y"),
    CK(6.28f, 1, "U"), CK(7.26f, 1, "I"), CK(8.24f, 1, "O"), CK(9.22f, 1, "P"), CK(10.20f, 1, "UP"),
    CK(0.25f, 2, "A"), CK(1.23f, 2, "S"), CK(2.21f, 2, "D"), CK(3.19f, 2, "F"), CK(4.17f, 2, "G"), CK(5.15f, 2, "H"),
    CK(6.13f, 2, "J"), CK(7.11f, 2, "K"), CK(8.09f, 2, "L"), CK(9.07f, 2, ":"), CK(10.05f, 2, "RETURN"),
    CK(0.10f, 3, "Z"), CK(1.08f, 3, "X"), CK(2.06f, 3, "C"), CK(3.04f, 3, "V"), CK(4.02f, 3, "B"), CK(5.00f, 3, "N"),
    CK(5.98f, 3, "M"), CK(6.96f, 3, ","), CK(7.94f, 3, ";"), CK(8.92f, 3, "?"),
    CK(0, 4, "LSHIFT"), CK(1.0f, 4, "RVS"), CK(2.0f, 4, "@"), CK(3.0f, 4, "["), CK(4.0f, 4, "]"), { 5.0f, 4, 2.0f, "SPACE" },
    CK(7.05f, 4, "<"), CK(8.05f, 4, ">"), CK(9.05f, 4, "STOP"), CK(10.05f, 4, "RSHIFT"),
    PK(12.0f, 0, "HOME"), PK(13.0f, 0, "DOWN"), PK(14.0f, 0, "RIGHT"), PK(15.0f, 0, "DEL"),
    PK(12.0f, 1, "7"), PK(13.0f, 1, "8"), PK(14.0f, 1, "9"), PK(15.0f, 1, "/"),
    PK(12.0f, 2, "4"), PK(13.0f, 2, "5"), PK(14.0f, 2, "6"), PK(15.0f, 2, "*"),
    PK(12.0f, 3, "1"), PK(13.0f, 3, "2"), PK(14.0f, 3, "3"), PK(15.0f, 3, "+"),
    PK(12.0f, 4, "0"), PK(13.0f, 4, "."), PK(14.0f, 4, "-"), PK(15.0f, 4, "=") };

static void draw_pet(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect a, bool cols80) {
    bool chic = !cols80 && pet_chiclet;
    const pkey_t *keys = chic ? pet_chic : cols80 ? pet_biz : pet_gfx;
    int n = chic ? (int)(sizeof pet_chic / sizeof pet_chic[0]) : cols80 ? (int)(sizeof pet_biz / sizeof pet_biz[0]) : (int)(sizeof pet_gfx / sizeof pet_gfx[0]);
    float u = a.w / (chic ? 16.2f : cols80 ? 20.6f : 19.4f), kh = 46;
    for (int i = 0; i < n; i++) {
        const char *t = keys[i].tok;
        char pc[40], top[8], label[16];
        pet_pc(t, cols80, pc, sizeof pc, top, sizeof top);
        snprintf(label, sizeof label, "%s", t);
        if (!strcmp(t, "LARROW")) snprintf(label, sizeof label, "\xE2\x86\x90");
        else if (!strcmp(t, "UP")) snprintf(label, sizeof label, "\xE2\x86\x91");
        else if (!strcmp(t, "RIGHT")) { snprintf(label, sizeof label, "CRSR\xE2\x86\x92"); snprintf(top, sizeof top, "\xE2\x86\x90"); }
        else if (!strcmp(t, "DOWN")) { snprintf(label, sizeof label, "CRSR\xE2\x86\x93"); snprintf(top, sizeof top, "\xE2\x86\x91"); }
        else if (!strcmp(t, "HOME")) snprintf(top, sizeof top, "CLR");
        else if (!strcmp(t, "STOP")) snprintf(top, sizeof top, "RUN");
        else if (!strcmp(t, "RVS")) snprintf(top, sizeof top, "OFF");
        else if (!strcmp(t, "SLOCK")) { snprintf(top, sizeof top, "SHIFT"); snprintf(label, sizeof label, "LOCK"); }
        else if (!strcmp(t, "LSHIFT") || !strcmp(t, "RSHIFT")) snprintf(label, sizeof label, "SHIFT");
        else if (!strcmp(t, "DEL")) snprintf(top, sizeof top, "INST");
        else if (t[0] == 'K' && t[1] >= '0' && t[1] <= '9' && !t[2]) snprintf(label, sizeof label, "%c", t[1]);
        bool mod = !strcmp(t, "LSHIFT") || !strcmp(t, "RSHIFT");
        draw_gkey(cv, ctx, nk_rect(a.x + keys[i].x * u, a.y + keys[i].y * kh, keys[i].w * u, (chic && !strcmp(t, "RETURN")) ? 2 * kh : kh), top, label, pc,
                  mod ? 1 : (pc[0] == '-' && !pc[1] ? 2 : 0));
    }
}

static void draw_key(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect r, const skey_t *k) {
    static const struct nk_color fill[5] = { { 58, 58, 64, 255 }, { 70, 70, 100, 255 }, { 100, 60, 60, 255 },
                                             { 58, 58, 64, 255 }, { 58, 58, 64, 255 } };
    struct nk_rect b = nk_rect(r.x + 2, r.y + 2, r.w - 4, r.h - 4);
    nk_fill_rect(cv, b, 4, fill[k->kind]);
    nk_stroke_rect(cv, b, 4, 1, nk_rgb(120, 120, 130));
    float fh = ctx->style.font->height + 1;
    if (k->kind == 0) {
        line(cv, ctx, nk_rect(b.x, b.y + 1, b.w, fh), k->keyword, nk_rgb(110, 210, 240));
        line(cv, ctx, nk_rect(b.x, b.y + fh, b.w, fh + 4), k->cap, nk_rgb(240, 240, 230));
        line(cv, ctx, nk_rect(b.x, b.y + 2 * fh + 4, b.w, fh), k->sym, nk_rgb(255, 110, 110));
    } else {
        line(cv, ctx, nk_rect(b.x, b.y + fh, b.w, fh + 4), k->cap, nk_rgb(240, 240, 230));
        if (k->kind == 4) line(cv, ctx, nk_rect(b.x, b.y + 1, b.w, fh), k->keyword, nk_rgb(110, 210, 240));
    }
    char pc[48];
    if (k->kind != 0) snprintf(pc, sizeof pc, "PC: %s", k->pc);
    else if (k->pcsym) snprintf(pc, sizeof pc, "%s  %s", k->pc, k->pcsym);
    else if (zx81_mode && k->sym[0]) snprintf(pc, sizeof pc, "%s  Shift+%s", k->pc, k->pc);
    else if (k->sym[0]) snprintf(pc, sizeof pc, "%s  Tab+%s", k->pc, k->pc);
    else snprintf(pc, sizeof pc, "%s", k->pc);
    line(cv, ctx, nk_rect(b.x, b.y + b.h - fh - 1, b.w, fh), pc, nk_rgb(180, 230, 160));
}

void keymap_window(struct nk_context *ctx, bool *open, const machine_t *m, float bw, float h) {
    static bool shown;
    if (!*open) { shown = false; return; }
    float ww = 760, wh = fminf(h - 60, 520);
    struct nk_rect r = nk_rect(fmaxf((bw - ww) / 2, 0), 50, ww, wh);
    if (!shown) {
        nk_window_show(ctx, "Keyboard", NK_SHOWN);
        nk_window_set_bounds(ctx, "Keyboard", r);
        nk_window_set_focus(ctx, "Keyboard");
        shown = true;
    }
    if (nk_begin(ctx, "Keyboard", r, NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE)) {
        if (!keymap_supported(m)) {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "No keyboard map for this machine yet.", NK_TEXT_LEFT);
            nk_label_colored(ctx, m->keys_help, NK_TEXT_LEFT, COL_DIMTEXT);
        } else if (is_spectrum(m)) {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "Each key: BASIC keyword (blue), the key (white), SYMBOL SHIFT legend (red),", NK_TEXT_LEFT);
            nk_label(ctx, "and under it, green: the PC key, and the character that types the red legend.", NK_TEXT_LEFT);
            float kh = 72;
            nk_layout_row_dynamic(ctx, kh * 4 + 6, 1);
            struct nk_rect a;
            if (nk_widget(&a, ctx) != NK_WIDGET_INVALID) {
                struct nk_command_buffer *cv = nk_window_get_canvas(ctx);
                const skey_t *rows[4] = { row1, row2, row3, row4 };
                float kw = a.w / 10;
                for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 10; x++)
                        draw_key(cv, ctx, nk_rect(a.x + x * kw, a.y + y * kh, kw, kh), &rows[y][x]);
            }
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label_colored(ctx, "Number keys + CAPS SHIFT: 1 EDIT, 2 CAPS LOCK, 3 TRUE VIDEO, 4 INV VIDEO,", NK_TEXT_LEFT, COL_DIMTEXT);
            nk_label_colored(ctx, "  5 \xE2\x86\x90, 6 \xE2\x86\x93, 7 \xE2\x86\x91, 8 \xE2\x86\x92, 9 GRAPHICS, 0 DELETE.", NK_TEXT_LEFT, COL_DIMTEXT);
            nk_label(ctx, "On the PC: the arrow keys are the cursor keys, Backspace is DELETE (CAPS SHIFT+0),", NK_TEXT_LEFT);
            nk_label(ctx, "Esc is BREAK (CAPS SHIFT+SPACE), Left Shift is CAPS SHIFT, Tab is SYMBOL SHIFT.", NK_TEXT_LEFT);
            nk_label(ctx, "Typing a character that needs SYMBOL SHIFT (+ - = \" ; ...) presses it for you.", NK_TEXT_LEFT);
        } else if (is_ts1000(m)) {
            nk_layout_row_dynamic(ctx, 20, 1);
            bool zx80 = m == &machine_zx80 && !zx80_kbd81;
            nk_label(ctx, zx80 ? "The ZX80 keyboard: keyword (blue), the key (white), SHIFT legend (red)," :
                                 "The ZX81 / TS1000 keyboard: K-mode keyword (blue), the key (white), SHIFT legend (red),", NK_TEXT_LEFT);
            nk_label(ctx, "and under it, green: the PC key, and the character that types the red legend.", NK_TEXT_LEFT);
            float kh = 72;
            nk_layout_row_dynamic(ctx, kh * 4 + 6, 1);
            struct nk_rect a;
            if (nk_widget(&a, ctx) != NK_WIDGET_INVALID) {
                struct nk_command_buffer *cv = nk_window_get_canvas(ctx);
                const skey_t *rows[4] = { zrow1, zrow2, zrow3, zrow4 };
                if (zx80) { rows[0] = yrow1; rows[1] = yrow2; rows[2] = yrow3; rows[3] = yrow4; }
                float kw = a.w / 10;
                zx81_mode = true;
                for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 10; x++)
                        draw_key(cv, ctx, nk_rect(a.x + x * kw, a.y + y * kh, kw, kh), &rows[y][x]);
                zx81_mode = false;
            }
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label(ctx, "RUBOUT is the same as DELETE: SHIFT+0 on the ZX81, Backspace on the PC.", NK_TEXT_LEFT);
            nk_label(ctx, zx80 ? "Arrow keys are the cursors (SHIFT+5..8), Tab is EDIT (SHIFT+NEWLINE), Esc is BREAK (Space), Enter is NEWLINE." :
                                 "Arrow keys are the cursors (SHIFT+5..8), Tab is EDIT (SHIFT+1), Esc is BREAK (Space), Enter is NEWLINE.", NK_TEXT_LEFT);
            nk_label_colored(ctx, "Left Shift is SHIFT. Typing a symbol that needs SHIFT (\" ( ) $ : ; ? / + - = * < > ,) presses it for you.", NK_TEXT_LEFT, COL_DIMTEXT);
        } else if (is_c16(m)) {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "The C16 / Plus/4 keyboard: an 8 x 8 matrix between the 6529B ($FD30, the rows) and the TED's K lines.", NK_TEXT_LEFT);
            nk_label(ctx, "Each key: the shifted legend (red), the key (white), the PC key (green).", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 56 * 5 + 6, 1);
            struct nk_rect a;
            if (nk_widget(&a, ctx) != NK_WIDGET_INVALID) draw_c16(nk_window_get_canvas(ctx), ctx, a);
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label_colored(ctx, "Shift is the PC Shift key; a shifted character typed on the PC presses it for you. Letters are capitals without Shift.", NK_TEXT_LEFT, COL_DIMTEXT);
        } else if (is_c128(m)) {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "The C128 keyboard: the C64's matrix plus three more lines (the VIC's K0-K2) for the top row, cursors and keypad.", NK_TEXT_LEFT);
            nk_label(ctx, "Each key: the shifted legend (red), the key (white), the PC key (green); - : no PC key.", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 46 * 6 + 12, 1);
            struct nk_rect a;
            if (nk_widget(&a, ctx) != NK_WIDGET_INVALID) draw_c128(nk_window_get_canvas(ctx), ctx, a);
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label_colored(ctx, "CAPS (F12) and 40/80 (F11) are toggles that stay down. Right Ctrl is joystick 2 fire. Shift is the PC Shift key.", NK_TEXT_LEFT, COL_DIMTEXT);
        } else if (is_c64(m) || is_vic20(m)) {
            nk_layout_row_dynamic(ctx, 20, 1);
            if (is_vic20(m)) nk_label(ctx, "The VIC-20's keyboard: the C64's layout, scanned as an 8 x 8 matrix by the second 6522.", NK_TEXT_LEFT);
            nk_label(ctx, "Each key: the shifted legend (red), the key (white), the PC key (green).", NK_TEXT_LEFT);
            nk_label(ctx, "A shifted character typed on the PC (! \" # ...) presses SHIFT for you.", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 56 * 5 + 6, 1);
            struct nk_rect a;
            if (nk_widget(&a, ctx) != NK_WIDGET_INVALID) draw_c64(nk_window_get_canvas(ctx), ctx, a);
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label_colored(ctx, "Shift is the PC Shift key. Dn/Up = Down arrow (Up arrow is SHIFT+CRSR); Rt/Lt = Right arrow", NK_TEXT_LEFT, COL_DIMTEXT);
            nk_label_colored(ctx, "(Left arrow is SHIFT+CRSR). f2, f4, f6, f8: Shift+F1, F3, F5, F7. - : no PC key.", NK_TEXT_LEFT, COL_DIMTEXT);
        } else {
            bool c80 = m == &machine_pet8032;
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, c80 ? "The 8032's business keyboard (typewriter layout, numeric keypad on the right)." :
                               pet_chiclet ? "The PET's chiclet keyboard (the graphics keyboard's matrix; numeric keypad on the right)." :
                               "The 4032's graphics keyboard (typewriter layout, numeric keypad on the right).", NK_TEXT_LEFT);
            nk_label(ctx, "Each key: the shifted legend (red), the key (white), the PC key (green); - : no PC key.", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 46 * 5 + 6, 1);
            struct nk_rect a;
            if (nk_widget(&a, ctx) != NK_WIDGET_INVALID) draw_pet(nk_window_get_canvas(ctx), ctx, a, c80);
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label_colored(ctx, "Rt/Lt: the Right arrow is CRSR right, the Left arrow is it shifted. Home/Del: Home is HOME, Del is CLR.", NK_TEXT_LEFT, COL_DIMTEXT);
            nk_label_colored(ctx, "Shift is the PC Shift key. Letters are capitals without Shift, as on the PET. Keys with no PC key: -.", NK_TEXT_LEFT, COL_DIMTEXT);
        }
    } else {
        *open = false;
    }
    nk_end(ctx);
}
