#pragma once
/* coco_chips.h - the chips of the Tandy Color Computer 2 / Dragon / MC-10 family, see coco_chips.c. */
#include "parts.h"
#include "../chips/m6809.h"

extern const part_type_t PT_XTAL_COCO, PT_6809E, PT_SAM, PT_6847T1;

/* The CPU core of a MC6809E part (registers, for the monitor and for tests). */
const m6809_t *coco_cpu_core(sim_t *s, int part);

/* The 6883 SAM's state, as the software sees it (the 16 control bits V0..TY at bits 0..15) and the video counter. */
unsigned coco_sam_bits(sim_t *s, int part);
unsigned coco_sam_counter(sim_t *s, int part);

/* The picture the video chip drew: COCO_FB_W x COCO_FB_H pixels, 0xAABBGGRR. */
#define COCO_FB_W 320
#define COCO_FB_H 240
const uint32_t *coco_vdg_screen(sim_t *s, int part);
/* The video chip's frame count (it counts the frames it has finished). */
uint32_t coco_vdg_frames(sim_t *s, int part);
int coco_vdg_last_line(sim_t *s, int part, uint8_t *chr, int *nb);
