/* Unit tests of the PET part models (src/pet_parts.c): each chip sits on a throwaway
 * board; a driver part has one pin per chip pin so the test can force any input. */
#include "sim.h"
#include "parts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static int rail_of(const char *n) { return !strcmp(n, "GND") ? RAIL_GND : !strcmp(n, "+5V") ? RAIL_VCC : RAIL_NONE; }
static int kind_of(const char *r) { (void)r; return PART_IC; }

typedef struct { sim_t s; int chip, drv; int n; bd_pad_t pads[2 * 65]; const char *nets[70]; char names[70][8]; bd_comp_t comps[2]; board_data_t bd; } rig_t;

static rig_t *rig_new(const part_type_t *t, int npins) {
    rig_t *r = calloc(1, sizeof(*r));
    r->n = npins;
    for (int i = 0; i < npins; i++) { snprintf(r->names[i], 8, "n%d", i + 1); r->nets[i] = r->names[i]; }
    r->nets[npins] = "GND"; r->nets[npins + 1] = "+5V";
    /* find power pins from the type's pin string */
    char *copy = strdup(t->pins), *save = NULL; int k = 0;
    int power_net[70];
    for (char *tok = strtok_r(copy, " ", &save); tok; tok = strtok_r(NULL, " ", &save), k++) {
        char *c = strchr(tok, ':');
        power_net[k] = (c && c[1] == 'P') ? (!strncmp(tok, "GND", 3) ? npins : npins + 1) : -1;
    }
    free(copy);
    for (int i = 0; i < npins; i++) {
        static char num[65][4];
        snprintf(num[i], 4, "%d", i + 1);
        r->pads[i] = (bd_pad_t){ num[i], 0, 0, 1, 1, power_net[i] >= 0 ? power_net[i] : i };
        r->pads[npins + i] = (bd_pad_t){ num[i], 0, 0, 1, 1, i };
    }
    r->comps[0] = (bd_comp_t){ "U1", "chip", "DIP", 0, npins };
    r->comps[1] = (bd_comp_t){ "D1", "drv", "DIP", npins, npins };
    r->bd = (board_data_t){ {0, 0, 10, 10}, r->nets, npins + 2, r->pads, 2 * npins, r->comps, 2 };
    sim_desc_t d = { rail_of, kind_of, 32.0 };
    sim_init(&r->s, &r->bd, &d);
    part_set_type(&r->s, 0, t);
    r->chip = 0; r->drv = 1;
    sim_rebuild(&r->s);
    for (int i = 0; i < r->s.nparts; i++) if (r->s.parts[i].reset) r->s.parts[i].reset(&r->s, &r->s.parts[i]);
    for (int i = 0; i < r->s.nparts; i++) if (r->s.parts[i].eval) sim_mark_dirty(&r->s, i);
    sim_settle(&r->s);
    return r;
}
static void settle(rig_t *r) {
    sim_settle(&r->s);
    sim_commit(&r->s);
    sim_settle(&r->s);
}
static void drive(rig_t *r, int pin, int v) { sim_drive(&r->s, part_pin(&r->s.parts[r->drv], pin), v < 0 ? DRV_Z : DRV_BIT(v)); }
static int level(rig_t *r, int pin) { return sim_read(&r->s, part_pin(&r->s.parts[r->chip], pin)); }
static void step(rig_t *r) { r->s.cycle++; settle(r); }
static void pins(rig_t *r, const int *p, const int *v, int n) { for (int i = 0; i < n; i++) drive(r, p[i], v[i]); settle(r); }

int main(void) {
    /* 74LS02: pin 1 = NOR(2,3) */
    { rig_t *r = rig_new(&PT_74LS02, 14);
      drive(r, 2, 0); drive(r, 3, 0); settle(r); CHECK(level(r, 1) == 1);
      drive(r, 3, 1); settle(r); CHECK(level(r, 1) == 0);
      drive(r, 11, 0); drive(r, 12, 0); settle(r); CHECK(level(r, 13) == 1); }
    /* 74LS10 gate 1: pins 1,2,13 -> 12 */
    { rig_t *r = rig_new(&PT_74LS10, 14);
      drive(r, 1, 1); drive(r, 2, 1); drive(r, 13, 1); settle(r); CHECK(level(r, 12) == 0);
      drive(r, 13, 0); settle(r); CHECK(level(r, 12) == 1); }
    /* 74LS20 */
    { rig_t *r = rig_new(&PT_74LS20, 14);
      int p[4] = {1, 2, 4, 5}, v[4] = {1, 1, 1, 1}; pins(r, p, v, 4); CHECK(level(r, 6) == 0);
      drive(r, 5, 0); settle(r); CHECK(level(r, 6) == 1); }
    /* 7425: strobe low -> output high */
    { rig_t *r = rig_new(&PT_7425, 14);
      drive(r, 1, 0); drive(r, 2, 0); drive(r, 4, 0); drive(r, 5, 0); drive(r, 3, 1); settle(r); CHECK(level(r, 6) == 1);
      drive(r, 1, 1); settle(r); CHECK(level(r, 6) == 0);
      drive(r, 3, 0); settle(r); CHECK(level(r, 6) == 1); }
    /* 74LS86 */
    { rig_t *r = rig_new(&PT_74LS86, 14);
      drive(r, 1, 1); drive(r, 2, 0); settle(r); CHECK(level(r, 3) == 1);
      drive(r, 2, 1); settle(r); CHECK(level(r, 3) == 0); }
    /* 7417 open collector: low in -> low out, high in -> floating (pulled up by the TTL read) */
    { rig_t *r = rig_new(&PT_7417, 14);
      drive(r, 1, 0); settle(r); CHECK(level(r, 2) == 0);
      drive(r, 1, 1); settle(r); CHECK(r->s.pins[part_pin(&r->s.parts[0], 2)].drive == DRV_Z); }
    /* 74LS138: select 5 with G1 high, G2A/B low */
    { rig_t *r = rig_new(&PT_74LS138, 16);
      drive(r, 1, 1); drive(r, 2, 0); drive(r, 3, 1); drive(r, 4, 0); drive(r, 5, 0); drive(r, 6, 1); settle(r);
      CHECK(level(r, 10) == 0);        /* Y5 = pin 10 */
      CHECK(level(r, 9) == 1 && level(r, 11) == 1);
      drive(r, 6, 0); settle(r); CHECK(level(r, 10) == 1); }
    /* 74154: code 14 (D=1 C=1 B=1 A=0) with both enables low -> Y14 = pin 16 */
    { rig_t *r = rig_new(&PT_74154, 24);
      drive(r, 18, 0); drive(r, 19, 0); drive(r, 20, 1); drive(r, 21, 1); drive(r, 22, 1); drive(r, 23, 0); settle(r);
      CHECK(level(r, 16) == 0 && level(r, 17) == 1 && level(r, 15) == 1);
      drive(r, 18, 1); settle(r); CHECK(level(r, 16) == 1); }
    /* 74LS145: code 3 -> Y3 = pin 4 low; code 12 -> all off */
    { rig_t *r = rig_new(&PT_74LS145, 16);
      drive(r, 15, 1); drive(r, 14, 1); drive(r, 13, 0); drive(r, 12, 0); settle(r);
      CHECK(r->s.pins[part_pin(&r->s.parts[0], 4)].drive == DRV_LO && r->s.pins[part_pin(&r->s.parts[0], 3)].drive == DRV_Z);
      drive(r, 15, 0); drive(r, 14, 0); drive(r, 13, 1); drive(r, 12, 1); settle(r);
      CHECK(r->s.pins[part_pin(&r->s.parts[0], 4)].drive == DRV_Z); }
    /* 74LS244: 1G low passes 1A1 -> 1Y1, 2G high blocks group 2 */
    { rig_t *r = rig_new(&PT_74LS244, 20);
      drive(r, 1, 0); drive(r, 19, 1); drive(r, 2, 1); drive(r, 11, 1); settle(r);
      CHECK(level(r, 18) == 1);
      drive(r, 2, 0); settle(r); CHECK(level(r, 18) == 0);
      drive(r, 1, 1); settle(r);          /* off: reads as floating TTL = 1 or the old charge: just not driven */
      CHECK(r->s.pins[part_pin(&r->s.parts[0], 18)].drive == DRV_Z);
      drive(r, 19, 0); drive(r, 11, 1); settle(r); CHECK(level(r, 9) == 1);
      CHECK(r->s.pins[part_pin(&r->s.parts[0], 9)].drive == DRV_HI); }
    /* MC3446: driver input high pulls the bus low and the receiver reads it back inverted */
    { rig_t *r = rig_new(&PT_MC3446, 16);
      drive(r, 4, 0); drive(r, 12, 0); drive(r, 3, 0); settle(r);
      CHECK(level(r, 2) == 1);          /* bus pulled up */
      CHECK(level(r, 1) == 0);          /* receiver = not bus */
      drive(r, 3, 1); settle(r);
      CHECK(level(r, 2) == 0 && level(r, 1) == 1); }
    /* 74S74: clock, async clear and preset */
    { rig_t *r = rig_new(&PT_74S74, 14);
      drive(r, 1, 1); drive(r, 4, 1); drive(r, 2, 1); drive(r, 3, 0); settle(r);
      CHECK(level(r, 5) == 0);
      drive(r, 3, 1); settle(r); CHECK(level(r, 5) == 1 && level(r, 6) == 0);  /* rising edge: Q=D */
      drive(r, 3, 0); drive(r, 2, 0); settle(r); CHECK(level(r, 5) == 1);
      drive(r, 1, 0); settle(r); CHECK(level(r, 5) == 0 && level(r, 6) == 1); /* async clear */
      drive(r, 1, 1); drive(r, 4, 0); settle(r); CHECK(level(r, 5) == 1);       /* async preset */
      drive(r, 4, 1); settle(r); CHECK(level(r, 5) == 1);                       /* holds */
      drive(r, 1, 0); drive(r, 4, 0); settle(r); CHECK(level(r, 5) == 1 && level(r, 6) == 1); }
    /* 74177: CK1 -> QA divides by 2; QA tied to CK2 -> QB QC QD count */
    { rig_t *r = rig_new(&PT_74177, 14);
      drive(r, 1, 1); drive(r, 13, 0); drive(r, 8, 1); settle(r);     /* cleared */
      CHECK(level(r, 5) == 0 && level(r, 9) == 0 && level(r, 2) == 0 && level(r, 12) == 0);
      drive(r, 13, 1); settle(r);
      /* wire QA to CK2 by hand: one step later the test copies it */
      int qa_prev = 0, count = 0;
      for (int i = 0; i < 64; i++) {
          drive(r, 8, (i & 1) ? 1 : 0); step(r);          /* falling edges every other step */
          int qa = level(r, 5);
          drive(r, 6, qa); step(r);
          if (qa_prev && !qa) { }
          qa_prev = qa;
          count = level(r, 9) | (level(r, 2) << 1) | (level(r, 12) << 2);
      }
      CHECK(count >= 0 && count < 8);
      /* QA must have toggled many times and the 3 bit counter must be moving */
      int seen[8] = {0};
      for (int i = 0; i < 200; i++) {
          drive(r, 8, (i & 1) ? 1 : 0); step(r); drive(r, 6, level(r, 5)); step(r); step(r);
          seen[level(r, 9) | (level(r, 2) << 1) | (level(r, 12) << 2)] = 1;
      }
      int distinct = 0; for (int i = 0; i < 8; i++) distinct += seen[i];
      CHECK(distinct == 8);
      /* parallel load */
      drive(r, 3, 1); drive(r, 4, 1); drive(r, 10, 0); drive(r, 11, 1); drive(r, 1, 0); settle(r);
      CHECK(level(r, 5) == 1 && level(r, 9) == 0 && level(r, 2) == 1 && level(r, 12) == 1); }
    /* 74LS164 */
    { rig_t *r = rig_new(&PT_74LS164, 14);
      drive(r, 9, 0); drive(r, 8, 0); drive(r, 1, 1); drive(r, 2, 1); settle(r);
      drive(r, 9, 1); settle(r);
      for (int i = 0; i < 3; i++) { drive(r, 8, 1); settle(r); drive(r, 8, 0); settle(r); }
      CHECK(level(r, 3) == 1 && level(r, 4) == 1 && level(r, 5) == 1 && level(r, 6) == 0);
      drive(r, 1, 0);
      for (int i = 0; i < 2; i++) { drive(r, 8, 1); settle(r); drive(r, 8, 0); settle(r); }
      CHECK(level(r, 3) == 0 && level(r, 4) == 0 && level(r, 5) == 1 && level(r, 6) == 1 && level(r, 10) == 1);
      drive(r, 9, 0); settle(r); CHECK(level(r, 3) == 0 && level(r, 10) == 0 && level(r, 13) == 0); }
    /* 74166: parallel load 0xA5 then shift out MSB first */
    { rig_t *r = rig_new(&PT_74166, 16);
      static const int par[8] = { 2, 3, 4, 5, 10, 11, 12, 14 };
      int d[8]; for (int i = 0; i < 8; i++) d[i] = (0xA5 >> i) & 1;
      drive(r, 9, 1); drive(r, 6, 0); drive(r, 1, 0); drive(r, 7, 0); drive(r, 15, 0);
      pins(r, par, d, 8);
      drive(r, 7, 1); settle(r); drive(r, 7, 0); settle(r);       /* load */
      drive(r, 15, 1); settle(r);
      int bits[8];
      for (int i = 0; i < 8; i++) { bits[i] = level(r, 13); drive(r, 7, 1); settle(r); drive(r, 7, 0); settle(r); }
      int exp[8] = {1,0,1,0,0,1,0,1};
      for (int i = 0; i < 8; i++) CHECK(bits[i] == exp[i]);
      CHECK(level(r, 13) == 0);        /* serial input is 0, shifted in */ }
    /* 2316: selected when CS1 low, CS2, CS3 high */
    { rig_t *r = rig_new(&PT_2316, 24);
      static uint8_t img[2048]; for (int i = 0; i < 2048; i++) img[i] = (uint8_t)(i * 3 + 1);
      ((rom_state_t*)r->s.parts[0].state)->data = img;
      static const int ap[11] = { 8, 7, 6, 5, 4, 3, 2, 1, 23, 22, 19 };
      int a[11]; for (int i = 0; i < 11; i++) a[i] = (0x155 >> i) & 1;
      drive(r, 20, 0); drive(r, 18, 0); drive(r, 21, 1); pins(r, ap, a, 11);
      static const int dp[8] = { 9, 10, 11, 13, 14, 15, 16, 17 };
      for (int i = 0; i < 5; i++) settle(r);
      { int e = 0; for (int i = 0; i < 8; i++) e |= level(r, dp[i]) << i; CHECK(e != img[0x155]); }   /* not yet: the access time */
      for (int i = 0; i < 20; i++) settle(r);
      int v = 0; for (int i = 0; i < 8; i++) v |= level(r, dp[i]) << i;
      CHECK(v == img[0x155]);
      drive(r, 21, 0); settle(r);
      CHECK(r->s.pins[part_pin(&r->s.parts[0], 9)].drive == DRV_Z); }
    /* LM555 power-up reset: OUT high for the first few ms, then low */
    { rig_t *r = rig_new(&PT_LM555, 8);
      r->s.cycle = 0; r->s.parts[0].reset(&r->s, &r->s.parts[0]); sim_mark_dirty(&r->s, 0); settle(r);
      CHECK(level(r, 3) == 1);
      r->s.cycle = (uint64_t)(4000 * 32); sim_mark_dirty(&r->s, 0); settle(r);
      CHECK(level(r, 3) == 0); }
    printf(fails ? "pet_parts: %d FAILED\n" : "pet_parts: all ok\n", fails);
    return fails != 0;
}
