#pragma once
/*#
    # m6522.h

    MOS Technology 6522 Versatile Interface Adapter (VIA), as used in the
    Commodore PET (UB15 at $E840: user port, IEEE handshake bits, cassette
    #2, sound through the shift register on CB2, retrace flag on CB1).

    Written for the PET additions to c64-hw-sim. Same calling style as
    m6526.h and m6520.h: m6522_tick() once per bus cycle (PHI2).

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C file to create the implementation.

    ## Pins used by m6522_tick()

    - RS0..RS3      register select (A0..A3 of the CPU bus)
    - D0..D7, RW    data bus and read/write (RW=1: CPU read)
    - CS            set by the caller when the chip is selected in this cycle
    - RES           1 = reset asserted
    - CA1 CA2 CB1 CB2   control lines
    - PA0..7, PB0..7    ports; use m6522_pa_oe() etc. to know which pins the
                    chip really drives
    - IRQ           returned set when the interrupt output is asserted (low)

    ## Emulated

    - T1: one-shot / free-running, PB7 output; period N+2 in free-run
    - T2: one-shot timer, or pulse counting on PB6
    - shift register, all 8 modes (T2, Phi2 and external CB1 clock)
    - CA1/CB1 edge flags, CA2/CB2 input, handshake, pulse and manual modes
    - port A / B input latching (ACR bits 0 and 1)
    - IFR / IER with the usual set/clear semantics

    ## Not emulated

    - the exact cycle of the shift register's first clock edge and the rare
      timer-latch races (the values here are within a cycle of the real chip)

    ## zlib/libpng license (same as the rest of the chips directory)
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M6522_PIN_RS0   (0)
#define M6522_PIN_RS1   (1)
#define M6522_PIN_RS2   (2)
#define M6522_PIN_RS3   (3)
#define M6522_PIN_D0    (16)
#define M6522_PIN_RW    (24)
#define M6522_PIN_RES   (25)
#define M6522_PIN_IRQ   (26)
#define M6522_PIN_CS    (40)
#define M6522_PIN_CA1   (41)
#define M6522_PIN_CA2   (42)
#define M6522_PIN_CB1   (43)
#define M6522_PIN_CB2   (44)
#define M6522_PIN_PA0   (48)
#define M6522_PIN_PB0   (56)

#define M6522_RS0   (1ULL<<M6522_PIN_RS0)
#define M6522_RS1   (1ULL<<M6522_PIN_RS1)
#define M6522_RS2   (1ULL<<M6522_PIN_RS2)
#define M6522_RS3   (1ULL<<M6522_PIN_RS3)
#define M6522_RS    (M6522_RS0|M6522_RS1|M6522_RS2|M6522_RS3)
#define M6522_RW    (1ULL<<M6522_PIN_RW)
#define M6522_RES   (1ULL<<M6522_PIN_RES)
#define M6522_IRQ   (1ULL<<M6522_PIN_IRQ)
#define M6522_CS    (1ULL<<M6522_PIN_CS)
#define M6522_CA1   (1ULL<<M6522_PIN_CA1)
#define M6522_CA2   (1ULL<<M6522_PIN_CA2)
#define M6522_CB1   (1ULL<<M6522_PIN_CB1)
#define M6522_CB2   (1ULL<<M6522_PIN_CB2)
#define M6522_PA_PINS (0xFFULL<<M6522_PIN_PA0)
#define M6522_PB_PINS (0xFFULL<<M6522_PIN_PB0)
#define M6522_PB6   (1ULL<<(M6522_PIN_PB0+6))
#define M6522_PB7   (1ULL<<(M6522_PIN_PB0+7))

#define M6522_GET_DATA(p)       ((uint8_t)((p)>>M6522_PIN_D0))
#define M6522_SET_DATA(p,d)     {p=((p)&~(0xFFULL<<M6522_PIN_D0))|(((uint64_t)(d)&0xFF)<<M6522_PIN_D0);}
#define M6522_GET_PA(p)         ((uint8_t)((p)>>M6522_PIN_PA0))
#define M6522_GET_PB(p)         ((uint8_t)((p)>>M6522_PIN_PB0))
#define M6522_SET_PAB(p,a,b)    {p=((p)&~(M6522_PA_PINS|M6522_PB_PINS))|(((uint64_t)(a)&0xFF)<<M6522_PIN_PA0)|(((uint64_t)(b)&0xFF)<<M6522_PIN_PB0);}

// register numbers
#define M6522_REG_ORB   (0x0)
#define M6522_REG_ORA   (0x1)
#define M6522_REG_DDRB  (0x2)
#define M6522_REG_DDRA  (0x3)
#define M6522_REG_T1CL  (0x4)
#define M6522_REG_T1CH  (0x5)
#define M6522_REG_T1LL  (0x6)
#define M6522_REG_T1LH  (0x7)
#define M6522_REG_T2CL  (0x8)
#define M6522_REG_T2CH  (0x9)
#define M6522_REG_SR    (0xA)
#define M6522_REG_ACR   (0xB)
#define M6522_REG_PCR   (0xC)
#define M6522_REG_IFR   (0xD)
#define M6522_REG_IER   (0xE)
#define M6522_REG_ORA2  (0xF)   // port A without handshake

// interrupt flag bits
#define M6522_IRQ_CA2   (1<<0)
#define M6522_IRQ_CA1   (1<<1)
#define M6522_IRQ_SR    (1<<2)
#define M6522_IRQ_CB2   (1<<3)
#define M6522_IRQ_CB1   (1<<4)
#define M6522_IRQ_T2    (1<<5)
#define M6522_IRQ_T1    (1<<6)

typedef struct {
    uint8_t ora, orb, ddra, ddrb;
    uint8_t ira_latch, irb_latch;   // input latches (ACR0 / ACR1)
    uint16_t t1c, t1l;
    uint16_t t2c;
    uint8_t t2ll;                   // T2 low latch
    uint8_t sr, acr, pcr, ifr, ier;
    // timers
    bool t1_armed, t1_reload, t2_armed;
    bool pb7_out;                   // PB7 level driven by T1
    // control lines
    bool ca1, ca2, cb1, cb2, pb6;   // last input levels (edge detect)
    bool ca2_out, cb2_out;
    bool ca2_pulse, cb2_pulse;
    // shift register
    bool sr_run;                    // a shift is in progress
    uint8_t sr_count;               // shifts done
    uint8_t sr_div;                 // internal shift clock divider (T2 low byte or Phi2)
    bool cb1_int;                   // internal CB1 clock level
    bool cb1_out_level;             // level the chip drives on CB1 (internal clock modes)
    bool cb2_sr;                    // level the chip drives on CB2 (shift out)
    bool in_reset;
} m6522_t;

void m6522_init(m6522_t* c);
void m6522_reset(m6522_t* c);
uint64_t m6522_tick(m6522_t* c, uint64_t pins);

// which pins does the chip drive (as opposed to read)
static inline uint8_t m6522_pa_oe(const m6522_t* c) { return c->ddra; }
static inline uint8_t m6522_pb_oe(const m6522_t* c) {
    return (uint8_t)(c->ddrb | ((c->acr & 0x80) ? 0x80 : 0));      // T1 drives PB7
}
static inline bool m6522_ca2_oe(const m6522_t* c) { return (c->pcr & 0x08) != 0; }
static inline bool m6522_cb2_oe(const m6522_t* c) {
    return (c->pcr & 0x80) != 0 || (c->acr & 0x10) != 0;            // output mode or shifting out
}
static inline bool m6522_cb1_oe(const m6522_t* c) {
    const int m = (c->acr >> 2) & 7;
    return m != 0 && m != 3 && m != 7;                              // internal shift clock
}

#ifdef __cplusplus
} // extern "C"
#endif

/*--- IMPLEMENTATION ---------------------------------------------------------*/
#ifdef CHIPS_IMPL

void m6522_init(m6522_t* c) {
    c->t1c = 0xFFFF; c->t1l = 0xFFFF; c->t2c = 0xFFFF; c->t2ll = 0xFF; c->sr = 0;
    m6522_reset(c);
}

void m6522_reset(m6522_t* c) {
    // reset clears every register except the timers' counters/latches and the shift register
    c->ora = c->orb = c->ddra = c->ddrb = 0;
    c->acr = c->pcr = c->ifr = c->ier = 0;
    c->ira_latch = c->irb_latch = 0xFF;
    c->t1_armed = c->t1_reload = c->t2_armed = false;
    c->pb7_out = true;
    c->ca2_out = c->cb2_out = true;
    c->ca2_pulse = c->cb2_pulse = false;
    c->sr_run = false; c->sr_count = 0; c->sr_div = 0;
    c->cb1_int = true; c->cb1_out_level = true; c->cb2_sr = true;
    c->in_reset = true;
}

static inline bool m6522_ca2_mode_out(const m6522_t* c) { return (c->pcr & 0x08) != 0; }
static inline bool m6522_cb2_mode_out(const m6522_t* c) { return (c->pcr & 0x80) != 0; }

// after PCR is written: set the CA2/CB2 output level for manual and idle modes
static void m6522_pcr_update(m6522_t* c) {
    switch ((c->pcr >> 1) & 7) {
        case 6: c->ca2_out = false; break;
        case 7: c->ca2_out = true;  break;
        case 4: case 5: c->ca2_out = true; break;   // handshake / pulse idle high
        default: break;
    }
    switch ((c->pcr >> 5) & 7) {
        case 6: c->cb2_out = false; break;
        case 7: c->cb2_out = true;  break;
        case 4: case 5: c->cb2_out = true; break;
        default: break;
    }
}

// the effect of a read or write of port A's data register with handshake
static void m6522_porta_handshake(m6522_t* c) {
    const int m = (c->pcr >> 1) & 7;
    c->ifr &= ~M6522_IRQ_CA1;
    if (m != 1 && m != 3) {         // not "independent" input modes
        c->ifr &= ~M6522_IRQ_CA2;
    }
    if (m == 4) {
        c->ca2_out = false;
    } else if (m == 5) {
        c->ca2_out = false;
        c->ca2_pulse = true;
    }
}

static void m6522_portb_handshake(m6522_t* c) {
    const int m = (c->pcr >> 5) & 7;
    c->ifr &= ~M6522_IRQ_CB1;
    if (m != 1 && m != 3) {
        c->ifr &= ~M6522_IRQ_CB2;
    }
}

static void m6522_portb_write_strobe(m6522_t* c) {
    const int m = (c->pcr >> 5) & 7;
    if (m == 4) {
        c->cb2_out = false;
    } else if (m == 5) {
        c->cb2_out = false;
        c->cb2_pulse = true;
    }
}

// shift register: one tick of the shift clock (an internal clock half period)
static void m6522_shift_clock(m6522_t* c, bool new_level, bool cb2_in) {
    const int mode = (c->acr >> 2) & 7;
    const bool falling = c->cb1_int && !new_level;
    const bool rising  = !c->cb1_int && new_level;
    c->cb1_int = new_level;
    if (mode >= 4) {
        if (falling) {      // shift out: present bit 7 on CB2, rotate
            const uint8_t b7 = c->sr >> 7;
            c->cb2_sr = b7 != 0;
            c->sr = (uint8_t)((c->sr << 1) | b7);
            if (mode != 4) {
                if (++c->sr_count >= 8) {
                    c->sr_run = false;
                    c->ifr |= M6522_IRQ_SR;
                }
            }
        }
    } else if (mode != 0) {
        if (rising) {       // shift in: sample CB2
            c->sr = (uint8_t)((c->sr << 1) | (cb2_in ? 1 : 0));
            if (++c->sr_count >= 8) {
                c->sr_run = false;
                c->ifr |= M6522_IRQ_SR;
            }
        }
    }
}

uint64_t m6522_tick(m6522_t* c, uint64_t pins) {
    if (pins & M6522_RES) {
        m6522_reset(c);
        c->ca1 = (pins & M6522_CA1) != 0; c->ca2 = (pins & M6522_CA2) != 0;
        c->cb1 = (pins & M6522_CB1) != 0; c->cb2 = (pins & M6522_CB2) != 0;
        c->pb6 = (pins & M6522_PB6) != 0;
        return pins & ~M6522_IRQ;
    }
    c->in_reset = false;

    uint8_t pa_pins = M6522_GET_PA(pins);
    uint8_t pb_pins = M6522_GET_PB(pins);
    const bool ca1 = (pins & M6522_CA1) != 0, ca2 = (pins & M6522_CA2) != 0;
    bool cb1 = (pins & M6522_CB1) != 0;
    const bool cb2 = (pins & M6522_CB2) != 0;
    const bool pb6 = (pins & M6522_PB6) != 0;
    const int sr_mode = (c->acr >> 2) & 7;
    const bool sr_internal_clk = (sr_mode != 0 && sr_mode != 3 && sr_mode != 7);
    if (sr_internal_clk) {
        cb1 = c->cb1_int;       // CB1 is an output: the chip sees its own clock
    }

    // --- control line edges
    if (ca1 != c->ca1) {
        const bool active = (c->pcr & 0x01) ? ca1 : !ca1;
        if (active) {
            c->ifr |= M6522_IRQ_CA1;
            if (c->acr & 0x01) c->ira_latch = pa_pins;
            if (((c->pcr >> 1) & 7) == 4) c->ca2_out = true;     // handshake ends
        }
    }
    if (!m6522_ca2_mode_out(c) && ca2 != c->ca2) {
        const bool pos = (c->pcr & 0x04) != 0;
        if (pos ? ca2 : !ca2) c->ifr |= M6522_IRQ_CA2;
    }
    if (cb1 != c->cb1 && !sr_internal_clk) {
        const bool active = (c->pcr & 0x10) ? cb1 : !cb1;
        if (active) {
            c->ifr |= M6522_IRQ_CB1;
            if (c->acr & 0x02) c->irb_latch = pb_pins;
            if (((c->pcr >> 5) & 7) == 4) c->cb2_out = true;
        }
        if (sr_mode == 3 || sr_mode == 7) {     // external shift clock
            const bool falling = c->cb1 && !cb1;
            const bool rising  = !c->cb1 && cb1;
            if (sr_mode == 3 && rising && c->sr_run) {
                c->sr = (uint8_t)((c->sr << 1) | (cb2 ? 1 : 0));
                if (++c->sr_count >= 8) { c->sr_run = false; c->ifr |= M6522_IRQ_SR; }
            } else if (sr_mode == 7 && falling && c->sr_run) {
                const uint8_t b7 = c->sr >> 7;
                c->cb2_sr = b7 != 0;
                c->sr = (uint8_t)((c->sr << 1) | b7);
                if (++c->sr_count >= 8) { c->sr_run = false; c->ifr |= M6522_IRQ_SR; }
            }
        }
    }
    if (!m6522_cb2_mode_out(c) && sr_mode == 0 && cb2 != c->cb2) {
        const bool pos = (c->pcr & 0x40) != 0;
        if (pos ? cb2 : !cb2) c->ifr |= M6522_IRQ_CB2;
    }
    c->ca1 = ca1; c->ca2 = ca2; c->cb2 = cb2;
    if (!sr_internal_clk) c->cb1 = cb1;

    // pulses end after one cycle
    if (c->ca2_pulse) { c->ca2_pulse = false; c->ca2_out = true; }
    if (c->cb2_pulse) { c->cb2_pulse = false; c->cb2_out = true; }

    // --- timers
    if (c->t1_reload) {
        c->t1_reload = false;
        c->t1c = c->t1l;
    } else {
        const bool wrap = (c->t1c == 0);
        c->t1c--;
        if (wrap && c->t1_armed) {
            c->ifr |= M6522_IRQ_T1;
            if (c->acr & 0x40) {            // free-running
                c->t1_reload = true;
                if (c->acr & 0x80) c->pb7_out = !c->pb7_out;
            } else {
                c->t1_armed = false;
                if (c->acr & 0x80) c->pb7_out = true;
            }
        }
    }
    if (c->acr & 0x20) {                    // T2 counts falling edges on PB6
        if (c->pb6 && !pb6) {
            const bool wrap = (c->t2c == 0);
            c->t2c--;
            if (wrap && c->t2_armed) { c->ifr |= M6522_IRQ_T2; c->t2_armed = false; }
        }
    } else {
        const bool wrap = (c->t2c == 0);
        c->t2c--;
        if (wrap && c->t2_armed) { c->ifr |= M6522_IRQ_T2; c->t2_armed = false; }
    }
    c->pb6 = pb6;

    // --- shift register clock generation (internal clock modes)
    if (sr_internal_clk && (c->sr_run || sr_mode == 4)) {
        if (sr_mode == 2 || sr_mode == 6) {
            // Phi2: shift clock = Phi2 / 2
            m6522_shift_clock(c, !c->cb1_int, cb2);
        } else {
            // T2 low byte sets the half period: N+2 cycles
            if (c->sr_div == 0) {
                c->sr_div = c->t2ll;
                m6522_shift_clock(c, !c->cb1_int, cb2);
            } else {
                c->sr_div--;
            }
        }
    }

    // --- bus access
    if (pins & M6522_CS) {
        const uint8_t rs = (uint8_t)(pins & M6522_RS);
        if (pins & M6522_RW) {
            uint8_t data = 0xFF;
            switch (rs) {
                case M6522_REG_ORB:
                    m6522_portb_handshake(c);
                    {
                        const uint8_t in = (c->acr & 0x02) ? c->irb_latch : pb_pins;
                        data = (uint8_t)((in & ~c->ddrb) | (c->orb & c->ddrb));
                        if (c->acr & 0x80) {        // PB7 is the T1 output
                            data = (uint8_t)((data & 0x7F) | (c->pb7_out ? 0x80 : 0));
                        }
                    }
                    break;
                case M6522_REG_ORA:
                    m6522_porta_handshake(c);
                    /* fallthrough */
                case M6522_REG_ORA2:
                    data = (c->acr & 0x01) ? c->ira_latch : pa_pins;
                    // the 6522 returns the pin levels; for output lines these follow the register
                    data = (uint8_t)((data & ~c->ddra) | (c->ora & c->ddra));
                    break;
                case M6522_REG_DDRB: data = c->ddrb; break;
                case M6522_REG_DDRA: data = c->ddra; break;
                case M6522_REG_T1CL: c->ifr &= ~M6522_IRQ_T1; data = (uint8_t)c->t1c; break;
                case M6522_REG_T1CH: data = (uint8_t)(c->t1c >> 8); break;
                case M6522_REG_T1LL: data = (uint8_t)c->t1l; break;
                case M6522_REG_T1LH: data = (uint8_t)(c->t1l >> 8); break;
                case M6522_REG_T2CL: c->ifr &= ~M6522_IRQ_T2; data = (uint8_t)c->t2c; break;
                case M6522_REG_T2CH: data = (uint8_t)(c->t2c >> 8); break;
                case M6522_REG_SR:
                    data = c->sr;
                    c->ifr &= ~M6522_IRQ_SR;
                    if (sr_mode != 0) { c->sr_run = true; c->sr_count = 0; c->sr_div = 0; }
                    break;
                case M6522_REG_ACR: data = c->acr; break;
                case M6522_REG_PCR: data = c->pcr; break;
                case M6522_REG_IFR:
                    data = (uint8_t)(c->ifr & 0x7F);
                    if (c->ifr & c->ier & 0x7F) data |= 0x80;
                    break;
                case M6522_REG_IER: data = (uint8_t)(c->ier | 0x80); break;
            }
            M6522_SET_DATA(pins, data);
        } else {
            const uint8_t data = M6522_GET_DATA(pins);
            switch (rs) {
                case M6522_REG_ORB:
                    c->orb = data;
                    m6522_portb_handshake(c);
                    m6522_portb_write_strobe(c);
                    break;
                case M6522_REG_ORA:
                    c->ora = data;
                    m6522_porta_handshake(c);
                    break;
                case M6522_REG_ORA2: c->ora = data; break;
                case M6522_REG_DDRB: c->ddrb = data; break;
                case M6522_REG_DDRA: c->ddra = data; break;
                case M6522_REG_T1CL: case M6522_REG_T1LL:
                    c->t1l = (uint16_t)((c->t1l & 0xFF00) | data);
                    break;
                case M6522_REG_T1CH:
                    c->t1l = (uint16_t)((data << 8) | (c->t1l & 0x00FF));
                    c->t1c = (uint16_t)(c->t1l + 1);     // times out N+1.5 cycles after the load
                    c->ifr &= ~M6522_IRQ_T1;
                    c->t1_armed = true;
                    c->t1_reload = false;
                    if (c->acr & 0x80) c->pb7_out = false;
                    break;
                case M6522_REG_T1LH:
                    c->t1l = (uint16_t)((data << 8) | (c->t1l & 0x00FF));
                    c->ifr &= ~M6522_IRQ_T1;
                    break;
                case M6522_REG_T2CL: c->t2ll = data; break;
                case M6522_REG_T2CH:
                    c->t2c = (uint16_t)(((data << 8) | c->t2ll) + 1);
                    c->ifr &= ~M6522_IRQ_T2;
                    c->t2_armed = true;
                    break;
                case M6522_REG_SR:
                    c->sr = data;
                    c->ifr &= ~M6522_IRQ_SR;
                    if (sr_mode != 0) { c->sr_run = true; c->sr_count = 0; c->sr_div = 0; }
                    break;
                case M6522_REG_ACR: c->acr = data; break;
                case M6522_REG_PCR: c->pcr = data; m6522_pcr_update(c); break;
                case M6522_REG_IFR: c->ifr &= (uint8_t)~(data & 0x7F); break;
                case M6522_REG_IER:
                    if (data & 0x80) c->ier |= (uint8_t)(data & 0x7F);
                    else c->ier &= (uint8_t)~(data & 0x7F);
                    break;
            }
        }
    }

    // --- outputs
    uint8_t pa_out = (uint8_t)((pa_pins & ~c->ddra) | (c->ora & c->ddra));
    uint8_t pb_out = (uint8_t)((pb_pins & ~c->ddrb) | (c->orb & c->ddrb));
    if (c->acr & 0x80) {
        pb_out = (uint8_t)((pb_out & 0x7F) | (c->pb7_out ? 0x80 : 0));
    }
    M6522_SET_PAB(pins, pa_out, pb_out);
    if (m6522_ca2_mode_out(c)) {
        pins = c->ca2_out ? (pins | M6522_CA2) : (pins & ~M6522_CA2);
    }
    {
        const int m = (c->acr >> 2) & 7;
        if (m >= 4) {
            pins = c->cb2_sr ? (pins | M6522_CB2) : (pins & ~M6522_CB2);
        } else if (m6522_cb2_mode_out(c)) {
            pins = c->cb2_out ? (pins | M6522_CB2) : (pins & ~M6522_CB2);
        }
        if (m6522_cb1_oe(c)) {
            pins = c->cb1_int ? (pins | M6522_CB1) : (pins & ~M6522_CB1);
        }
    }
    if (c->ifr & c->ier & 0x7F) pins |= M6522_IRQ; else pins &= ~M6522_IRQ;
    return pins;
}

#endif // CHIPS_IMPL
