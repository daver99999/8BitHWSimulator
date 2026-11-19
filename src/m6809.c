/* m6809.c - the MC6809 core (see chips/m6809.h): a coroutine that does one bus cycle per call.
 * The coroutine trick: Y() leaves the function and the next call comes back to the same line through the
 * switch (c->line). A case label belongs to the nearest switch, so no yield may sit inside a nested switch:
 * the code between the yields uses if chains and gotos, and the helper functions (which never yield) do the rest. */
#include "m6809.h"
#include <string.h>

enum { M_INH, M_IMM, M_DIR, M_IDX, M_EXT };
enum { K_NONE, K_RD8, K_RD16, K_ST8, K_ST16, K_RMW, K_JMP, K_JSR, K_LEA, K_SPECIAL };
enum { IR_NONE, IR_NMI, IR_FIRQ, IR_IRQ, IR_SWI, IR_SWI2, IR_SWI3 };
enum { W_NONE, W_SYNC, W_CWAI };

static void set_nz8(m6809_t *c, uint8_t r) {
    c->cc = (uint8_t)((c->cc & ~(M6809_N | M6809_Z)) | ((r & 0x80) ? M6809_N : 0) | (r == 0 ? M6809_Z : 0));
}
static void set_nz16(m6809_t *c, uint16_t r) {
    c->cc = (uint8_t)((c->cc & ~(M6809_N | M6809_Z)) | ((r & 0x8000) ? M6809_N : 0) | (r == 0 ? M6809_Z : 0));
}
static void set_flag(m6809_t *c, uint8_t f, bool on) { if (on) c->cc |= f; else c->cc &= (uint8_t)~f; }

static uint8_t add8(m6809_t *c, uint8_t a, uint8_t b, unsigned cin) {
    unsigned r = (unsigned)a + b + cin;
    uint8_t r8 = (uint8_t)r;
    set_flag(c, M6809_H, ((a ^ b ^ r) & 0x10) != 0);
    set_flag(c, M6809_V, ((a ^ r) & (b ^ r) & 0x80) != 0);
    set_flag(c, M6809_C, r > 0xFF);
    set_nz8(c, r8);
    return r8;
}
static uint8_t sub8(m6809_t *c, uint8_t a, uint8_t b, unsigned cin) {
    unsigned r = (unsigned)a - b - cin;
    uint8_t r8 = (uint8_t)r;
    set_flag(c, M6809_V, ((a ^ b) & (a ^ r) & 0x80) != 0);
    set_flag(c, M6809_C, (r & 0x100) != 0);
    set_nz8(c, r8);
    return r8;
}
static uint16_t add16(m6809_t *c, uint16_t a, uint16_t b) {
    uint32_t r = (uint32_t)a + b;
    uint16_t r16 = (uint16_t)r;
    set_flag(c, M6809_V, ((a ^ r) & (b ^ r) & 0x8000) != 0);
    set_flag(c, M6809_C, r > 0xFFFF);
    set_nz16(c, r16);
    return r16;
}
static uint16_t sub16(m6809_t *c, uint16_t a, uint16_t b) {
    uint32_t r = (uint32_t)a - b;
    uint16_t r16 = (uint16_t)r;
    set_flag(c, M6809_V, ((a ^ b) & (a ^ r) & 0x8000) != 0);
    set_flag(c, M6809_C, (r & 0x10000) != 0);
    set_nz16(c, r16);
    return r16;
}

/* NEG COM LSR ROR ASR ASL ROL DEC INC TST CLR: the low nibble of the opcode says which */
static uint8_t unary(m6809_t *c, int which, uint8_t v) {
    uint8_t r = v;
    unsigned c0 = (c->cc & M6809_C) ? 1u : 0u;
    switch (which) {
    case 0x0: r = (uint8_t)(0 - v); set_nz8(c, r); set_flag(c, M6809_V, v == 0x80); set_flag(c, M6809_C, v != 0); break;
    case 0x3: r = (uint8_t)~v; set_nz8(c, r); set_flag(c, M6809_V, false); set_flag(c, M6809_C, true); break;
    case 0x4: set_flag(c, M6809_C, v & 1); r = (uint8_t)(v >> 1); set_nz8(c, r); break;
    case 0x6: set_flag(c, M6809_C, v & 1); r = (uint8_t)((v >> 1) | (c0 << 7)); set_nz8(c, r); break;
    case 0x7: set_flag(c, M6809_C, v & 1); r = (uint8_t)((v >> 1) | (v & 0x80)); set_nz8(c, r); break;
    case 0x8: set_flag(c, M6809_C, v & 0x80); r = (uint8_t)(v << 1); set_nz8(c, r);
              set_flag(c, M6809_V, ((c->cc & M6809_N) != 0) != ((c->cc & M6809_C) != 0)); break;
    case 0x9: set_flag(c, M6809_C, v & 0x80); r = (uint8_t)((v << 1) | c0); set_nz8(c, r);
              set_flag(c, M6809_V, ((c->cc & M6809_N) != 0) != ((c->cc & M6809_C) != 0)); break;
    case 0xA: r = (uint8_t)(v - 1); set_nz8(c, r); set_flag(c, M6809_V, v == 0x80); break;
    case 0xC: r = (uint8_t)(v + 1); set_nz8(c, r); set_flag(c, M6809_V, v == 0x7F); break;
    case 0xD: set_nz8(c, v); set_flag(c, M6809_V, false); break;
    case 0xF: r = 0; c->cc = (uint8_t)((c->cc & ~(M6809_N | M6809_V | M6809_C)) | M6809_Z); break;
    default: break;
    }
    return r;
}
static bool unary_valid(int lo) { return lo == 0 || lo == 3 || lo == 4 || lo == 6 || lo == 7 || lo == 8 || lo == 9 || lo == 0xA || lo == 0xC || lo == 0xD || lo == 0xF; }

static bool cond(const m6809_t *c, int cc) {
    bool C = c->cc & M6809_C, Z = c->cc & M6809_Z, N = c->cc & M6809_N, V = c->cc & M6809_V;
    switch (cc & 0xF) {
    case 0x0: return true;
    case 0x1: return false;
    case 0x2: return !C && !Z;
    case 0x3: return C || Z;
    case 0x4: return !C;
    case 0x5: return C;
    case 0x6: return !Z;
    case 0x7: return Z;
    case 0x8: return !V;
    case 0x9: return V;
    case 0xA: return !N;
    case 0xB: return N;
    case 0xC: return N == V;
    case 0xD: return N != V;
    case 0xE: return !Z && (N == V);
    default:  return Z || (N != V);
    }
}

static uint16_t *idx_reg(m6809_t *c, int r) {
    switch (r & 3) { case 0: return &c->x; case 1: return &c->y; case 2: return &c->u; default: return &c->s; }
}
static uint16_t get_reg(m6809_t *c, int code) {
    switch (code & 0xF) {
    case 0: return (uint16_t)(c->a << 8 | c->b);
    case 1: return c->x;
    case 2: return c->y;
    case 3: return c->u;
    case 4: return c->s;
    case 5: return c->pc;
    case 8: return (uint16_t)(0xFF00 | c->a);
    case 9: return (uint16_t)(0xFF00 | c->b);
    case 10: return (uint16_t)(0xFF00 | c->cc);
    case 11: return (uint16_t)(0xFF00 | c->dp);
    default: return 0xFFFF;
    }
}
static void set_reg(m6809_t *c, int code, uint16_t v) {
    switch (code & 0xF) {
    case 0: c->a = (uint8_t)(v >> 8); c->b = (uint8_t)v; break;
    case 1: c->x = v; break;
    case 2: c->y = v; break;
    case 3: c->u = v; break;
    case 4: c->s = v; c->nmi_armed = 1; break;
    case 5: c->pc = v; break;
    case 8: c->a = (uint8_t)v; break;
    case 9: c->b = (uint8_t)v; break;
    case 10: c->cc = (uint8_t)v; break;
    case 11: c->dp = (uint8_t)v; break;
    default: break;
    }
}

/* the 16-bit register an 8x..Fx opcode works on: LDX, LDY, STU... */
static uint16_t *reg16_of(m6809_t *c) {
    int lo = c->op & 0xF;
    bool bside = (c->op & 0x40) != 0;
    if (lo == 0xC && !bside) return c->page == 0 ? &c->x : c->page == 1 ? &c->y : &c->s;       /* CMPX CMPY CMPS */
    if (lo == 0xE || lo == 0xF) return bside ? (c->page == 1 ? &c->s : &c->u) : (c->page == 1 ? &c->y : &c->x);
    return &c->x;
}

static void alu8(m6809_t *c, uint8_t v) {
    uint8_t *r = (c->op & 0x40) ? &c->b : &c->a;
    unsigned cin = (c->cc & M6809_C) ? 1u : 0u;
    switch (c->op & 0xF) {
    case 0x0: *r = sub8(c, *r, v, 0); break;
    case 0x1: (void)sub8(c, *r, v, 0); break;
    case 0x2: *r = sub8(c, *r, v, cin); break;
    case 0x4: *r &= v; set_nz8(c, *r); set_flag(c, M6809_V, false); break;
    case 0x5: set_nz8(c, (uint8_t)(*r & v)); set_flag(c, M6809_V, false); break;
    case 0x6: *r = v; set_nz8(c, v); set_flag(c, M6809_V, false); break;
    case 0x8: *r ^= v; set_nz8(c, *r); set_flag(c, M6809_V, false); break;
    case 0x9: *r = add8(c, *r, v, cin); break;
    case 0xA: *r |= v; set_nz8(c, *r); set_flag(c, M6809_V, false); break;
    case 0xB: *r = add8(c, *r, v, 0); break;
    default: break;
    }
}

static void alu16(m6809_t *c, uint16_t v) {
    int lo = c->op & 0xF;
    bool bside = (c->op & 0x40) != 0;
    uint16_t d = (uint16_t)(c->a << 8 | c->b);
    if (lo == 0x3) {
        if (bside) { d = add16(c, d, v); c->a = (uint8_t)(d >> 8); c->b = (uint8_t)d; }
        else if (c->page == 0) { d = sub16(c, d, v); c->a = (uint8_t)(d >> 8); c->b = (uint8_t)d; }
        else if (c->page == 1) (void)sub16(c, d, v);                /* CMPD */
        else (void)sub16(c, c->u, v);                               /* CMPU */
    } else if (lo == 0xC && bside) {                                /* LDD */
        c->a = (uint8_t)(v >> 8); c->b = (uint8_t)v; set_nz16(c, v); set_flag(c, M6809_V, false);
    } else if (lo == 0xC) {                                         /* CMPX CMPY CMPS */
        (void)sub16(c, *reg16_of(c), v);
    } else if (lo == 0xE) {                                         /* LDX LDY LDU LDS */
        *reg16_of(c) = v; set_nz16(c, v); set_flag(c, M6809_V, false);
        if (bside && c->page == 1) c->nmi_armed = 1;                /* LDS arms the NMI */
    }
}

static uint16_t store_value(m6809_t *c) {
    int lo = c->op & 0xF;
    if (lo == 0x7) return (c->op & 0x40) ? c->b : c->a;
    if (lo == 0xD) return (uint16_t)(c->a << 8 | c->b);             /* STD */
    return *reg16_of(c);
}

/* Fills kind, mode, opsz and the cycle count of the instruction from page and op. */
static void decode(m6809_t *c) {
    int op = c->op, hi = op >> 4, lo = op & 0xF, pg = c->page;
    bool bside = op >= 0xC0;
    c->kind = K_SPECIAL; c->mode = M_INH; c->opsz = 1; c->target = 2;
    if (pg == 0 && hi <= 7 && hi != 1 && hi != 2 && hi != 3 && hi != 4 && hi != 5) {
        c->mode = hi == 0 ? M_DIR : hi == 6 ? M_IDX : M_EXT;
        if (lo == 0xE) { c->kind = K_JMP; c->target = c->mode == M_DIR ? 3 : c->mode == M_IDX ? 3 : 4; }
        else if (unary_valid(lo)) { c->kind = K_RMW; c->target = c->mode == M_EXT ? 7 : 6; }
        else c->kind = K_NONE;
        return;
    }
    if (pg == 0 && hi == 3 && lo <= 3) { c->kind = K_LEA; c->mode = M_IDX; c->target = 4; return; }
    if (hi < 8) return;                                     /* the other opcodes are SPECIAL: handled one by one */
    bool valid;
    if (pg == 0) valid = true;
    else if (pg == 1) valid = bside ? (lo == 0xE || lo == 0xF) : (lo == 3 || lo == 0xC || lo == 0xE || lo == 0xF);
    else valid = !bside && (lo == 3 || lo == 0xC);
    c->mode = (hi == 8 || hi == 0xC) ? M_IMM : (hi == 9 || hi == 0xD) ? M_DIR : (hi == 0xA || hi == 0xE) ? M_IDX : M_EXT;
    if (!valid) { c->kind = K_NONE; c->mode = M_INH; return; }
    int mode = c->mode;
    if (lo == 0x7) c->kind = mode == M_IMM ? K_NONE : K_ST8;
    else if (lo == 3 || lo == 0xC || lo == 0xE) c->kind = K_RD16;
    else if (lo == 0xD) c->kind = bside ? (mode == M_IMM ? K_NONE : K_ST16) : (mode == M_IMM ? K_SPECIAL : K_JSR);
    else if (lo == 0xF) c->kind = mode == M_IMM ? K_NONE : K_ST16;
    else c->kind = K_RD8;
    if (c->kind == K_NONE) { c->mode = M_INH; return; }
    if (c->kind == K_SPECIAL) { c->mode = M_INH; c->target = 7; return; }      /* BSR */
    if (c->kind == K_RD16) c->opsz = 2;
    /* the cycle count */
    switch (c->kind) {
    case K_RD8: c->target = mode == M_IMM ? 2 : mode == M_EXT ? 5 : 4; break;
    case K_RD16: {
        bool ld = (lo == 0xC && bside) || lo == 0xE;
        c->target = (uint8_t)((mode == M_IMM ? (ld ? 3 : 4) : mode == M_EXT ? (ld ? 6 : 7) : (ld ? 5 : 6)) + (pg ? 1 : 0));
        break; }
    case K_ST8: c->target = mode == M_EXT ? 5 : 4; break;
    case K_ST16: c->target = (uint8_t)((mode == M_EXT ? 6 : 5) + (pg ? 1 : 0)); break;
    case K_JSR: c->target = mode == M_EXT ? 8 : 7; break;
    default: break;
    }
}

/* The indexed postbyte: works out what it can without the bus. nfetch offset bytes still have to be read; the
 * address is then base + offset (or PC + offset), and if c->ind the address holds the real address. */
static void idx_prep(m6809_t *c) {
    uint8_t pb = c->post;
    uint16_t *r = idx_reg(c, pb >> 5);
    uint16_t base = *r;
    int extra = 0;
    c->ind = 0; c->nfetch = 0; c->pcrel = 0; c->tmp = base;
    if (!(pb & 0x80)) {                                 /* 5-bit signed offset */
        int off = pb & 0x1F;
        if (off & 0x10) off -= 32;
        c->ea = (uint16_t)(base + off); extra = 1;
    } else {
        c->ind = (pb & 0x10) != 0;
        switch (pb & 0x0F) {
        case 0x0: c->ea = base; *r = (uint16_t)(base + 1); extra = 2; break;
        case 0x1: c->ea = base; *r = (uint16_t)(base + 2); extra = 3; break;
        case 0x2: *r = (uint16_t)(base - 1); c->ea = *r; extra = 2; break;
        case 0x3: *r = (uint16_t)(base - 2); c->ea = *r; extra = 3; break;
        case 0x4: c->ea = base; break;
        case 0x5: c->ea = (uint16_t)(base + (int8_t)c->b); extra = 1; break;
        case 0x6: c->ea = (uint16_t)(base + (int8_t)c->a); extra = 1; break;
        case 0xB: c->ea = (uint16_t)(base + (uint16_t)(c->a << 8 | c->b)); extra = 4; break;
        case 0x8: c->nfetch = 1; extra = 1; break;
        case 0x9: c->nfetch = 2; extra = 4; break;
        case 0xC: c->nfetch = 1; c->pcrel = 1; extra = 1; break;
        case 0xD: c->nfetch = 2; c->pcrel = 1; extra = 5; break;
        case 0xF: c->nfetch = 2; c->tmp = 0; extra = 2; c->ind = 1; break;   /* [n16] */
        default: c->ea = base; break;
        }
        if (c->ind && (pb & 0x0F) != 0x0F) extra += 3;
    }
    c->target = (uint8_t)(c->target + extra);
}

void m6809_init(m6809_t *c) {
    memset(c, 0, sizeof(*c));
    c->need_reset = 1;
    c->cc = M6809_F | M6809_I;
    c->addr = 0xFFFF; c->rw = 1;
    c->nmi_prev = 1;
}

#define Y_(n)    do { c->line = (n); c->nc++; c->cycles++; return; case (n):; } while (0)
#define Y()      Y_(__COUNTER__ + 1)      /* (the argument is expanded once, so both uses of n agree) */
#define RD(a)    do { c->addr = (uint16_t)(a); c->rw = 1; c->avma = 1; Y(); } while (0)
#define WR(a, d) do { c->addr = (uint16_t)(a); c->dout = (uint8_t)(d); c->rw = 0; c->avma = 1; Y(); } while (0)
#define IDLE()   do { c->addr = 0xFFFF; c->rw = 1; c->avma = 0; Y(); } while (0)
#define FETCH()  do { RD(c->pc); c->pc++; } while (0)       /* the byte is in c->din afterwards */
#define SPC (c->page * 256 + c->op)     /* the opcode with its page: no locals live across a yield */
#define PUSH_ON_U (c->ret == 1 && c->page == 0 && c->op == 0x36)
#define PULL_ON_U (c->page == 0 && c->op == 0x37)
#define PUSH16S(v) do { c->s--; WR(c->s, (v) & 0xFF); c->s--; WR(c->s, (v) >> 8); } while (0)

void m6809_cycle(m6809_t *c, uint8_t din, bool nmi_n, bool firq_n, bool irq_n, bool reset_n, bool halt_n) {
    c->din = din;
    c->in_nmi = !nmi_n; c->in_firq = !firq_n; c->in_irq = !irq_n; c->in_halt = !halt_n;
    if (c->nmi_prev && !nmi_n) c->nmi_pend = 1;
    c->nmi_prev = nmi_n;
    if (!reset_n) {                         /* reset: the bus idles, the registers get their reset values */
        c->need_reset = 1; c->line = 0; c->addr = 0xFFFF; c->rw = 1; c->avma = 0; c->lic = 0; c->ba = c->bs = 0;
        c->cc |= M6809_F | M6809_I; c->dp = 0; c->nmi_armed = 0; c->nmi_pend = 0; c->wait = W_NONE; c->halted = 0;
        return;
    }
    switch (c->line) {
    case 0:
    for (;;) {
        if (c->need_reset) {
            c->need_reset = 0;
            IDLE(); IDLE(); IDLE();
            RD(0xFFFE); c->tmp = (uint16_t)(c->din << 8);
            RD(0xFFFF); c->pc = (uint16_t)(c->tmp | c->din);
            IDLE();
        }
        /* ---- the boundary between two instructions: HALT, interrupts ---- */
        c->nc = 0;
        while (c->in_halt && c->wait == W_NONE) { c->halted = 1; c->ba = c->bs = 1; IDLE(); c->nc = 0; }
        c->halted = 0; c->ba = c->bs = 0;
        c->irqkind = IR_NONE;
        if (c->nmi_pend && c->nmi_armed) { c->irqkind = IR_NMI; c->nmi_pend = 0; }
        else if (c->in_firq && !(c->cc & M6809_F)) c->irqkind = IR_FIRQ;
        else if (c->in_irq && !(c->cc & M6809_I)) c->irqkind = IR_IRQ;
        if (c->wait != W_NONE) {
            if (c->irqkind == IR_NONE) {
                if (c->wait == W_SYNC && (c->in_firq || c->in_irq || c->nmi_pend)) c->wait = W_NONE;    /* a masked interrupt ends SYNC too */
                else { IDLE(); c->nc = 0; continue; }
            } else if (c->wait == W_CWAI) {                     /* the state is already on the stack */
                c->wait = W_NONE;
                goto vector;
            } else c->wait = W_NONE;
        }
        if (c->irqkind != IR_NONE) {
            RD(c->pc); RD(c->pc); IDLE();                       /* two dummy fetches and an internal cycle */
            c->regs = (c->irqkind == IR_FIRQ) ? 0x81 : 0xFF;
            if (c->regs == 0xFF) c->cc |= M6809_E; else c->cc &= (uint8_t)~M6809_E;
            c->ret = 0;
            goto do_push;
        }
        /* ---- fetch and decode ---- */
        FETCH(); c->op = c->din; c->page = 0;
        if (c->op == 0x10 || c->op == 0x11) { c->page = (uint8_t)(c->op - 0x0F); FETCH(); c->op = c->din; }
        decode(c);
        c->ea = 0;
        /* ---- the operand's address ---- */
        if (c->mode == M_IMM) { c->ea = c->pc; c->pc = (uint16_t)(c->pc + c->opsz); }
        else if (c->mode == M_DIR) { FETCH(); c->ea = (uint16_t)(c->dp << 8 | c->din); }
        else if (c->mode == M_EXT) { FETCH(); c->tmp = (uint16_t)(c->din << 8); FETCH(); c->ea = (uint16_t)(c->tmp | c->din); }
        else if (c->mode == M_IDX) {
            FETCH(); c->post = c->din;
            idx_prep(c);
            if (c->nfetch == 1) { FETCH(); c->val = (uint16_t)(int16_t)(int8_t)c->din; }
            else if (c->nfetch == 2) { FETCH(); c->val = (uint16_t)(c->din << 8); FETCH(); c->val |= c->din; }
            if (c->nfetch) c->ea = (uint16_t)((c->pcrel ? c->pc : c->tmp) + c->val);
            if (c->ind) {
                RD(c->ea); c->tmp = (uint16_t)(c->din << 8);
                RD(c->ea + 1); c->ea = (uint16_t)(c->tmp | c->din);
            }
        }
        /* ---- the operation ---- */
        if (c->kind == K_RD8) { RD(c->ea); alu8(c, c->din); }
        else if (c->kind == K_RD16) {
            RD(c->ea); c->tmp = (uint16_t)(c->din << 8);
            RD(c->ea + 1); alu16(c, (uint16_t)(c->tmp | c->din));
        }
        else if (c->kind == K_ST8) {
            c->val = store_value(c);
            set_nz8(c, (uint8_t)c->val); set_flag(c, M6809_V, false);
            WR(c->ea, c->val);
        }
        else if (c->kind == K_ST16) {
            c->val = store_value(c);
            set_nz16(c, c->val); set_flag(c, M6809_V, false);
            WR(c->ea, c->val >> 8);
            WR(c->ea + 1, c->val & 0xFF);
        }
        else if (c->kind == K_RMW) {
            RD(c->ea);
            c->val = unary(c, c->op & 0xF, c->din);
            IDLE();
            if ((c->op & 0xF) != 0xD) WR(c->ea, c->val);
        }
        else if (c->kind == K_JMP) c->pc = c->ea;
        else if (c->kind == K_JSR) { c->res = c->pc; c->pc = c->ea; PUSH16S(c->res); }
        else if (c->kind == K_LEA) {
            int w = c->op & 3;
            if (w == 0) { c->x = c->ea; set_flag(c, M6809_Z, c->ea == 0); }
            else if (w == 1) { c->y = c->ea; set_flag(c, M6809_Z, c->ea == 0); }
            else if (w == 2) { c->s = c->ea; c->nmi_armed = 1; }
            else c->u = c->ea;
        }
        else if (c->kind == K_SPECIAL) {
            if (c->page == 0 && c->op >= 0x40 && c->op <= 0x5F) {            /* NEGA..CLRB */
                uint8_t *r = (c->op & 0x10) ? &c->b : &c->a;
                if (unary_valid(c->op & 0xF)) *r = unary(c, c->op & 0xF, *r);
            }
            else if (c->page == 0 && c->op >= 0x20 && c->op <= 0x2F) {       /* short branches */
                FETCH(); c->val = (uint16_t)(int16_t)(int8_t)c->din;
                if (cond(c, c->op)) c->pc = (uint16_t)(c->pc + c->val);
                c->target = 3;
            }
            else if (c->page == 1 && c->op >= 0x21 && c->op <= 0x2F) {       /* long branches */
                FETCH(); c->val = (uint16_t)(c->din << 8); FETCH(); c->val |= c->din;
                c->target = 5;
                if (cond(c, c->op)) { c->pc = (uint16_t)(c->pc + c->val); c->target = 6; }
            }
            else if (SPC == 0x8D) {                                      /* BSR */
                FETCH(); c->val = (uint16_t)(int16_t)(int8_t)c->din;
                c->res = c->pc; c->pc = (uint16_t)(c->pc + c->val);
                PUSH16S(c->res);
            }
            else if (SPC == 0x16 || SPC == 0x17) {                        /* LBRA, LBSR */
                FETCH(); c->val = (uint16_t)(c->din << 8); FETCH(); c->val |= c->din;
                c->target = 5;
                if (SPC == 0x17) { c->res = c->pc; c->target = 9; }
                c->pc = (uint16_t)(c->pc + c->val);
                if (SPC == 0x17) PUSH16S(c->res);
            }
            else if (c->page == 0 && c->op >= 0x34 && c->op <= 0x37) {       /* PSHS PULS PSHU PULU */
                FETCH(); c->regs = c->din;
                c->target = 5;
                for (int i = 0; i < 8; i++) if (c->regs & (1 << i)) c->target = (uint8_t)(c->target + (i >= 4 ? 2 : 1));
                if (c->op & 1) goto do_pull;
                c->ret = 1;
                goto do_push;
            }
            else if (SPC == 0x13) { c->wait = W_SYNC; c->target = 4; }
            else if (SPC == 0x19) {                                      /* DAA */
                unsigned a = c->a, cf = 0, msn = a & 0xF0, lsn = a & 0x0F;
                if (lsn > 9 || (c->cc & M6809_H)) cf |= 0x06;
                if (msn > 0x80 && lsn > 9) cf |= 0x60;
                if (msn > 0x90 || (c->cc & M6809_C)) cf |= 0x60;
                unsigned r = a + cf;
                if (r > 0xFF) c->cc |= M6809_C;
                c->a = (uint8_t)r; set_nz8(c, c->a);
            }
            else if (SPC == 0x1A) { FETCH(); c->cc |= c->din; c->target = 3; }
            else if (SPC == 0x1C) { FETCH(); c->cc &= c->din; c->target = 3; }
            else if (SPC == 0x1D) { c->a = (c->b & 0x80) ? 0xFF : 0x00; set_nz16(c, (uint16_t)(c->a << 8 | c->b)); set_flag(c, M6809_V, false); }
            else if (SPC == 0x1E || SPC == 0x1F) {                        /* EXG, TFR */
                FETCH();
                {
                    int src = c->din >> 4, dst = c->din & 0xF;
                    uint16_t sv = get_reg(c, src), dv = get_reg(c, dst);
                    if (SPC == 0x1E) set_reg(c, src, dv);
                    set_reg(c, dst, sv);
                    c->target = (SPC == 0x1E) ? 8 : 6;
                }
            }
            else if (SPC == 0x39) {                                      /* RTS */
                RD(c->s); c->s++; c->tmp = (uint16_t)(c->din << 8);
                RD(c->s); c->s++; c->pc = (uint16_t)(c->tmp | c->din);
                c->target = 5;
            }
            else if (SPC == 0x3A) { c->x = (uint16_t)(c->x + c->b); c->target = 3; }
            else if (SPC == 0x3B) {                                      /* RTI */
                RD(c->s); c->s++; c->cc = c->din;
                c->regs = (c->cc & M6809_E) ? 0xFE : 0x80;
                c->target = (c->cc & M6809_E) ? 15 : 6;
                goto do_pull;
            }
            else if (SPC == 0x3C) {                                      /* CWAI */
                FETCH(); c->cc &= c->din; c->cc |= M6809_E;
                IDLE();
                c->regs = 0xFF; c->wait = W_CWAI; c->ret = 2;
                goto do_push;
            }
            else if (SPC == 0x3D) {                                      /* MUL */
                uint16_t r = (uint16_t)(c->a * c->b);
                c->a = (uint8_t)(r >> 8); c->b = (uint8_t)r;
                set_flag(c, M6809_Z, r == 0); set_flag(c, M6809_C, (r & 0x80) != 0);
                c->target = 11;
            }
            else if (SPC == 0x3F || SPC == 0x13F || SPC == 0x23F) {       /* SWI, SWI2, SWI3 */
                c->irqkind = SPC == 0x3F ? IR_SWI : SPC == 0x13F ? IR_SWI2 : IR_SWI3;
                c->target = c->page ? 20 : 19;
                IDLE();
                c->regs = 0xFF; c->cc |= M6809_E; c->ret = 3;
                goto do_push;
            }
            /* everything else (NOP, illegal opcodes): 2 cycles */
        }
        /* ---- pad to the cycle count ---- */
        while (c->nc < c->target) IDLE();
        continue;

    do_push:
        /* push the registers named in c->regs (bit 7 PC .. bit 0 CC); then c->ret says where on:
         * 0 an interrupt (vector), 1 PSHS/PSHU (pad), 2 CWAI (wait), 3 SWI (vector, then pad) */
        {
            for (c->tmp = 8; c->tmp > 0; ) {
                c->tmp--;                                       /* c->tmp is the register's bit */
                if (!(c->regs & (1 << c->tmp))) continue;
                if (c->tmp == 7) c->res = c->pc;
                else if (c->tmp == 6) c->res = PUSH_ON_U ? c->s : c->u;
                else if (c->tmp == 5) c->res = c->y;
                else if (c->tmp == 4) c->res = c->x;
                else if (c->tmp == 3) c->res = c->dp;
                else if (c->tmp == 2) c->res = c->b;
                else if (c->tmp == 1) c->res = c->a;
                else c->res = c->cc;
                if (c->tmp >= 4) {
                    if (PUSH_ON_U) { c->u--; WR(c->u, c->res & 0xFF); c->u--; WR(c->u, c->res >> 8); }
                    else { c->s--; WR(c->s, c->res & 0xFF); c->s--; WR(c->s, c->res >> 8); }
                } else {
                    if (PUSH_ON_U) { c->u--; WR(c->u, c->res); } else { c->s--; WR(c->s, c->res); }
                }
            }
        }
        if (c->ret == 1) { while (c->nc < c->target) IDLE(); continue; }
        if (c->ret == 2) continue;                              /* CWAI: now wait at the boundary */
    vector:
        if (c->irqkind == IR_NMI) { c->tmp = 0xFFFC; c->cc |= M6809_F | M6809_I; }
        else if (c->irqkind == IR_FIRQ) { c->tmp = 0xFFF6; c->cc |= M6809_F | M6809_I; }
        else if (c->irqkind == IR_IRQ) { c->tmp = 0xFFF8; c->cc |= M6809_I; }
        else if (c->irqkind == IR_SWI) { c->tmp = 0xFFFA; c->cc |= M6809_F | M6809_I; }
        else if (c->irqkind == IR_SWI2) c->tmp = 0xFFF4;
        else c->tmp = 0xFFF2;
        IDLE();
        RD(c->tmp); c->res = (uint16_t)(c->din << 8);
        RD(c->tmp + 1); c->pc = (uint16_t)(c->res | c->din);
        IDLE();
        if (c->irqkind >= IR_SWI) while (c->nc < c->target) IDLE();
        continue;

    do_pull:
        /* PULS/PULU, and RTI after its CC: pull the registers named in c->regs, CC first, PC last */
        {
            for (c->tmp = 0; c->tmp < 8; c->tmp++) {
                if (!(c->regs & (1 << c->tmp))) continue;
                if (c->tmp >= 4) {
                    if (PULL_ON_U) { RD(c->u); c->u++; c->res = (uint16_t)(c->din << 8); RD(c->u); c->u++; c->res |= c->din; }
                    else { RD(c->s); c->s++; c->res = (uint16_t)(c->din << 8); RD(c->s); c->s++; c->res |= c->din; }
                } else {
                    if (PULL_ON_U) { RD(c->u); c->u++; } else { RD(c->s); c->s++; }
                    c->res = c->din;
                }
                if (c->tmp == 7) c->pc = c->res;
                else if (c->tmp == 6) { if (PULL_ON_U) c->s = c->res; else c->u = c->res; }
                else if (c->tmp == 5) c->y = c->res;
                else if (c->tmp == 4) c->x = c->res;
                else if (c->tmp == 3) c->dp = (uint8_t)c->res;
                else if (c->tmp == 2) c->b = (uint8_t)c->res;
                else if (c->tmp == 1) c->a = (uint8_t)c->res;
                else c->cc = (uint8_t)c->res;
            }
        }
        while (c->nc < c->target) IDLE();
        continue;
    }
    }
}
