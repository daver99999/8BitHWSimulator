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
#include "zx48.h"
#include "c64.h"
#include "pet.h"

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

static void line(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect r, const char *s, struct nk_color c) {
    if (s && *s) text_centered(cv, ctx, r, s, c);
}

static bool is_spectrum(const machine_t *m) {
    return m == &machine_ts2068 || m == &machine_zx48 || m == &machine_zx48i2 || m == &machine_zx48i1 ||
           m == &machine_zx48i4b || m == &machine_zx48i6a;
}
static bool is_c64(const machine_t *m) { return m == &machine_c64 || m == &machine_c64c; }
static bool is_pet(const machine_t *m) { return m == &machine_pet4032 || m == &machine_pet8032; }

bool keymap_supported(const machine_t *m) { return is_spectrum(m) || is_c64(m) || is_pet(m); }

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

/* The PET's key matrix (pet.c's table), as the 10 rows the keyboard is scanned in. */
static void pet_pc(const char *n, bool cols80, char *out, size_t sz, char *top, size_t tsz) {
    top[0] = 0;
    if (!strcmp(n, "~")) { out[0] = 0; return; }
    static const struct { const char *n, *pc; } named[] = {
        { "RETURN", "Enter" }, { "DEL", "Bksp" }, { "UP", "Up" }, { "DOWN", "Down" }, { "RIGHT", "Rt/Lt" },
        { "HOME", "Home/Del" }, { "STOP", "Esc" }, { "RVS", "End" }, { "TAB", "Tab" }, { "RSHIFT", "R Shift" },
        { "LSHIFT", "Shift" }, { "SPACE", "Space" }, { "LARROW", "-" }, { "ESC", "-" }, { "RPT", "-" } };
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

static void draw_pet(struct nk_command_buffer *cv, struct nk_context *ctx, struct nk_rect a, bool cols80) {
    float cw = a.w / 8, kh = 38;
    for (int row = 0; row < 10; row++)
        for (int col = 0; col < 8; col++) {
            const char *n = pet_key_legend(cols80, row, col);
            if (!strcmp(n, "~")) continue;
            char pc[40], top[8];
            pet_pc(n, cols80, pc, sizeof pc, top, sizeof top);
            char main[16];
            if (n[0] == 'K' && n[1] >= '0' && n[1] <= '9' && !n[2]) snprintf(main, sizeof main, "%c (pad)", n[1]);
            else snprintf(main, sizeof main, "%s", !strcmp(n, "LARROW") ? "\xE2\x86\x90" : n);
            bool mod = !strcmp(n, "LSHIFT") || !strcmp(n, "RSHIFT");
            draw_gkey(cv, ctx, nk_rect(a.x + col * cw, a.y + row * kh, cw, kh), "", main, pc, mod ? 1 : (pc[0] == '-' && !pc[1] ? 2 : 0));
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
    else if (k->sym[0]) snprintf(pc, sizeof pc, "%s  Tab+%s", k->pc, k->pc);
    else snprintf(pc, sizeof pc, "%s", k->pc);
    line(cv, ctx, nk_rect(b.x, b.y + b.h - fh - 1, b.w, fh), pc, nk_rgb(180, 230, 160));
}

void keymap_window(struct nk_context *ctx, bool *open, const machine_t *m, float bw, float h) {
    static bool shown;
    if (!*open) { shown = false; return; }
    float ww = 740, wh = fminf(h - 60, is_pet(m) ? 620 : 520);
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
        } else if (is_c64(m)) {
            nk_layout_row_dynamic(ctx, 20, 1);
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
            nk_label(ctx, c80 ? "The 8032's business keyboard, key by key as the scan matrix has them." :
                               "The 4032's graphics keyboard, key by key as the scan matrix has them.", NK_TEXT_LEFT);
            nk_label(ctx, "Each key: the key (white) and the PC key (green); - : no PC key.", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 38 * 10 + 6, 1);
            struct nk_rect a;
            if (nk_widget(&a, ctx) != NK_WIDGET_INVALID) draw_pet(nk_window_get_canvas(ctx), ctx, a, c80);
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label_colored(ctx, "Rt/Lt: the Right arrow is CRSR right, the Left arrow is it shifted. Home/Del: Home is HOME,", NK_TEXT_LEFT, COL_DIMTEXT);
            nk_label_colored(ctx, "Del is CLR. Shift (left) is the PC Shift key. Letters are capitals without Shift, as on the PET.", NK_TEXT_LEFT, COL_DIMTEXT);
        }
    } else {
        *open = false;
    }
    nk_end(ctx);
}
