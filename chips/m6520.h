#pragma once
/*#
    # m6520.h

    MOS Technology 6520 Peripheral Interface Adapter (PIA), as used twice in
    the Commodore PET (IEEE-488 at $E820, keyboard/cassette at $E810).

    Written for the PET additions to c64-hw-sim (not part of floooh's chips).
    Same calling style as m6526.h: m6520_tick() once per bus cycle.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C file to create the implementation.

    ## Pins used by m6520_tick()

    - RS0, RS1      register select (A0/A1 of the CPU bus)
    - D0..D7        data bus
    - RW            1 = CPU read, 0 = CPU write
    - CS            set by the caller when the chip is selected in this cycle
                    (the caller combines /CS0, CS1, /CS2 and PHI2)
    - RES           1 = reset asserted (the caller inverts /RES)
    - CA1 CA2 CB1 CB2  control lines (levels in, outputs reported in the struct)
    - PA0..7, PB0..7   peripheral ports. On return, bits of output lines
                    carry the output value; use c->ddra / c->ddrb to know which
                    pins the chip is actually driving.
    - IRQA, IRQB    returned set when the interrupt output is asserted (low)

    ## Behaviour that matters for the PET

    - reading the port data register clears both flags of that side
    - CA2/CB2 output modes: manual, handshake (goes low on the strobe event and
      returns high on the next active CA1/CB1 edge), pulse (low for one cycle)
    - reading port A returns the pin levels; reading port B returns the output
      register for output lines and the pin levels for input lines
    - flag bit 6 reads 0 when CA2/CB2 is an output
    - register writes to the control register cannot change the flag bits

    ## zlib/libpng license (same as the rest of the chips directory)
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M6520_PIN_RS0   (0)
#define M6520_PIN_RS1   (1)
#define M6520_PIN_D0    (16)
#define M6520_PIN_D1    (17)
#define M6520_PIN_D2    (18)
#define M6520_PIN_D3    (19)
#define M6520_PIN_D4    (20)
#define M6520_PIN_D5    (21)
#define M6520_PIN_D6    (22)
#define M6520_PIN_D7    (23)
#define M6520_PIN_RW    (24)
#define M6520_PIN_RES   (25)
#define M6520_PIN_IRQA  (26)
#define M6520_PIN_IRQB  (27)
#define M6520_PIN_CS    (40)
#define M6520_PIN_CA1   (41)
#define M6520_PIN_CA2   (42)
#define M6520_PIN_CB1   (43)
#define M6520_PIN_CB2   (44)
#define M6520_PIN_PA0   (48)
#define M6520_PIN_PB0   (56)

#define M6520_RS0   (1ULL<<M6520_PIN_RS0)
#define M6520_RS1   (1ULL<<M6520_PIN_RS1)
#define M6520_RS    (M6520_RS0|M6520_RS1)
#define M6520_RW    (1ULL<<M6520_PIN_RW)
#define M6520_RES   (1ULL<<M6520_PIN_RES)
#define M6520_IRQA  (1ULL<<M6520_PIN_IRQA)
#define M6520_IRQB  (1ULL<<M6520_PIN_IRQB)
#define M6520_CS    (1ULL<<M6520_PIN_CS)
#define M6520_CA1   (1ULL<<M6520_PIN_CA1)
#define M6520_CA2   (1ULL<<M6520_PIN_CA2)
#define M6520_CB1   (1ULL<<M6520_PIN_CB1)
#define M6520_CB2   (1ULL<<M6520_PIN_CB2)
#define M6520_PA_PINS (0xFFULL<<M6520_PIN_PA0)
#define M6520_PB_PINS (0xFFULL<<M6520_PIN_PB0)

#define M6520_GET_DATA(p)       ((uint8_t)((p)>>M6520_PIN_D0))
#define M6520_SET_DATA(p,d)     {p=((p)&~(0xFFULL<<M6520_PIN_D0))|(((uint64_t)(d)&0xFF)<<M6520_PIN_D0);}
#define M6520_GET_PA(p)         ((uint8_t)((p)>>M6520_PIN_PA0))
#define M6520_GET_PB(p)         ((uint8_t)((p)>>M6520_PIN_PB0))
#define M6520_SET_PAB(p,a,b)    {p=((p)&~(M6520_PA_PINS|M6520_PB_PINS))|(((uint64_t)(a)&0xFF)<<M6520_PIN_PA0)|(((uint64_t)(b)&0xFF)<<M6520_PIN_PB0);}

typedef struct {
    uint8_t ora, ddra, cra;
    uint8_t orb, ddrb, crb;
    bool irqa1, irqa2, irqb1, irqb2;    // interrupt flags (bits 7 and 6 of the control registers)
    bool ca1, ca2, cb1, cb2;            // last input levels, for edge detection
    bool ca2_out, cb2_out;              // output levels when CA2/CB2 are outputs
    bool ca2_pulse, cb2_pulse;          // a pulse is pending: back to high at the next cycle
    uint8_t pa_in, pb_in;               // port pin levels seen in the last tick
    bool in_reset;
} m6520_t;

void m6520_init(m6520_t* c);
void m6520_reset(m6520_t* c);
uint64_t m6520_tick(m6520_t* c, uint64_t pins);
// the output state of CA2/CB2: true when the chip drives the pin (mode bit 5 set)
static inline bool m6520_ca2_oe(const m6520_t* c) { return (c->cra & 0x20) != 0; }
static inline bool m6520_cb2_oe(const m6520_t* c) { return (c->crb & 0x20) != 0; }

#ifdef __cplusplus
} // extern "C"
#endif

/*--- IMPLEMENTATION ---------------------------------------------------------*/
#ifdef CHIPS_IMPL

void m6520_init(m6520_t* c) {
    m6520_reset(c);
}

void m6520_reset(m6520_t* c) {
    c->ora = c->ddra = c->cra = 0;
    c->orb = c->ddrb = c->crb = 0;
    c->irqa1 = c->irqa2 = c->irqb1 = c->irqb2 = false;
    c->ca2_out = c->cb2_out = true;
    c->ca2_pulse = c->cb2_pulse = false;
    c->in_reset = true;
}

// is this control register's line 2 an output?  bits 5..3 = 1xx
// handshake mode:  CR5=1, CR4=0, CR3=0 (read/write handshake) or CR3=1 (pulse)
static inline bool m6520_ctl_out(uint8_t cr)       { return (cr & 0x20) != 0; }
static inline bool m6520_ctl_manual(uint8_t cr)    { return (cr & 0x30) == 0x30; }
static inline bool m6520_ctl_pulse(uint8_t cr)     { return (cr & 0x38) == 0x28; }
static inline bool m6520_ctl_handshake(uint8_t cr) { return (cr & 0x38) == 0x20; }

// the output level of line 2 after a control register write
static bool m6520_ctl_write_level(uint8_t cr) {
    if (!m6520_ctl_out(cr)) {
        return true;                    // input mode: no output, keep the level high
    }
    if (m6520_ctl_manual(cr)) {
        return (cr & 0x08) != 0;        // manual: bit 3 is the level
    }
    return true;                        // handshake / pulse idle high
}

uint64_t m6520_tick(m6520_t* c, uint64_t pins) {
    if (pins & M6520_RES) {
        m6520_reset(c);
        // port pins are inputs; report the inputs through
        c->pa_in = M6520_GET_PA(pins);
        c->pb_in = M6520_GET_PB(pins);
        c->ca1 = (pins & M6520_CA1) != 0;
        c->ca2 = (pins & M6520_CA2) != 0;
        c->cb1 = (pins & M6520_CB1) != 0;
        c->cb2 = (pins & M6520_CB2) != 0;
        return pins & ~(M6520_IRQA | M6520_IRQB);
    }
    c->in_reset = false;

    // a pulse (CA2/CB2 low for one cycle) ends at the next tick
    if (c->ca2_pulse) { c->ca2_pulse = false; c->ca2_out = true; }
    if (c->cb2_pulse) { c->cb2_pulse = false; c->cb2_out = true; }

    // sample the peripheral inputs: the level a pin has is the output value for output lines
    uint8_t pa_pins = M6520_GET_PA(pins);
    uint8_t pb_pins = M6520_GET_PB(pins);
    c->pa_in = pa_pins;
    c->pb_in = pb_pins;

    // --- control line edges (before the bus access: a same-cycle edge is seen first)
    const bool ca1 = (pins & M6520_CA1) != 0;
    const bool cb1 = (pins & M6520_CB1) != 0;
    const bool ca2 = (pins & M6520_CA2) != 0;
    const bool cb2 = (pins & M6520_CB2) != 0;

    // CA1 / CB1
    if (ca1 != c->ca1) {
        const bool rising = ca1;
        const bool active = (c->cra & 0x02) ? rising : !rising;
        if (active) {
            c->irqa1 = true;
            if (m6520_ctl_handshake(c->cra)) {
                c->ca2_out = true;      // read handshake ends on the active CA1 edge
            }
        }
    }
    if (cb1 != c->cb1) {
        const bool rising = cb1;
        const bool active = (c->crb & 0x02) ? rising : !rising;
        if (active) {
            c->irqb1 = true;
            if (m6520_ctl_handshake(c->crb)) {
                c->cb2_out = true;      // write handshake ends on the active CB1 edge
            }
        }
    }
    // CA2 / CB2 as inputs
    if (!m6520_ctl_out(c->cra) && ca2 != c->ca2) {
        const bool rising = ca2;
        const bool active = (c->cra & 0x10) ? rising : !rising;
        if (active) c->irqa2 = true;
    }
    if (!m6520_ctl_out(c->crb) && cb2 != c->cb2) {
        const bool rising = cb2;
        const bool active = (c->crb & 0x10) ? rising : !rising;
        if (active) c->irqb2 = true;
    }
    c->ca1 = ca1; c->cb1 = cb1; c->ca2 = ca2; c->cb2 = cb2;

    // --- bus access
    if (pins & M6520_CS) {
        const uint8_t rs = (uint8_t)(pins & M6520_RS);
        if (pins & M6520_RW) {
            uint8_t data = 0xFF;
            switch (rs) {
                case 0:     // port A data / DDRA
                    if (c->cra & 0x04) {
                        data = (uint8_t)((pa_pins & ~c->ddra) | (c->ora & c->ddra));
                        c->irqa1 = c->irqa2 = false;
                        if (m6520_ctl_handshake(c->cra)) {
                            c->ca2_out = false;
                        } else if (m6520_ctl_pulse(c->cra)) {
                            c->ca2_out = false;
                            c->ca2_pulse = true;
                        }
                    } else {
                        data = c->ddra;
                    }
                    break;
                case 1:     // CRA
                    data = (uint8_t)((c->cra & 0x3F) | (c->irqa1 ? 0x80 : 0) |
                                     ((c->irqa2 && !m6520_ctl_out(c->cra)) ? 0x40 : 0));
                    break;
                case 2:     // port B data / DDRB
                    if (c->crb & 0x04) {
                        data = (uint8_t)((pb_pins & ~c->ddrb) | (c->orb & c->ddrb));
                        c->irqb1 = c->irqb2 = false;
                    } else {
                        data = c->ddrb;
                    }
                    break;
                case 3:     // CRB
                    data = (uint8_t)((c->crb & 0x3F) | (c->irqb1 ? 0x80 : 0) |
                                     ((c->irqb2 && !m6520_ctl_out(c->crb)) ? 0x40 : 0));
                    break;
            }
            M6520_SET_DATA(pins, data);
        } else {
            const uint8_t data = M6520_GET_DATA(pins);
            switch (rs) {
                case 0:
                    if (c->cra & 0x04) c->ora = data; else c->ddra = data;
                    break;
                case 1:
                    c->cra = (uint8_t)(data & 0x3F);
                    c->ca2_out = m6520_ctl_write_level(c->cra);
                    break;
                case 2:
                    if (c->crb & 0x04) {
                        c->orb = data;
                        if (m6520_ctl_handshake(c->crb)) {
                            c->cb2_out = false;
                        } else if (m6520_ctl_pulse(c->crb)) {
                            c->cb2_out = false;
                            c->cb2_pulse = true;
                        }
                    } else {
                        c->ddrb = data;
                    }
                    break;
                case 3:
                    c->crb = (uint8_t)(data & 0x3F);
                    c->cb2_out = m6520_ctl_write_level(c->crb);
                    break;
            }
        }
    }

    // --- outputs
    // ports: output lines carry the output register, input lines pass the input through
    const uint8_t pa_out = (uint8_t)((pa_pins & ~c->ddra) | (c->ora & c->ddra));
    const uint8_t pb_out = (uint8_t)((pb_pins & ~c->ddrb) | (c->orb & c->ddrb));
    M6520_SET_PAB(pins, pa_out, pb_out);
    // control lines 2 as outputs
    if (m6520_ctl_out(c->cra)) {
        pins = c->ca2_out ? (pins | M6520_CA2) : (pins & ~M6520_CA2);
    }
    if (m6520_ctl_out(c->crb)) {
        pins = c->cb2_out ? (pins | M6520_CB2) : (pins & ~M6520_CB2);
    }
    // interrupt outputs (asserted = bit set)
    const bool irqa = (c->irqa1 && (c->cra & 0x01)) ||
                      (c->irqa2 && (c->cra & 0x08) && !m6520_ctl_out(c->cra));
    const bool irqb = (c->irqb1 && (c->crb & 0x01)) ||
                      (c->irqb2 && (c->crb & 0x08) && !m6520_ctl_out(c->crb));
    pins = irqa ? (pins | M6520_IRQA) : (pins & ~M6520_IRQA);
    pins = irqb ? (pins | M6520_IRQB) : (pins & ~M6520_IRQB);
    return pins;
}

#endif // CHIPS_IMPL
