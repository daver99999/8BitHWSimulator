/* c128_drive.h - the disk drive on the serial bus (c128_drive.c) */
#ifndef C128_DRIVE_H
#define C128_DRIVE_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* One channel of the drive (secondary address 0-15). */
typedef struct {
    uint8_t type;
    uint8_t t, s;                   /* a file: the next sector of the chain */
    uint8_t last;                   /* the sector in buf is the last one of the file */
    int pos, len;
    uint8_t buf[256];
} dchan_t;

typedef struct diskdrive diskdrive_t;
struct diskdrive {
    /* the disk */
    uint8_t *img;                   /* the disk image (allocated; not part of a saved state) */
    size_t size;
    int kind;
    char path[512];
    bool dirty;
    uint8_t changed;
    /* the bus */
    uint8_t enabled, device;
    uint8_t atn, clk, data, srq;    /* the levels last seen (1 = released) */
    uint8_t pull_clk, pull_data, pull_srq;  /* what the drive pulls low */
    int state;
    uint64_t t0;
    uint8_t armed, nbits, shift, eoi, atn_cmd, listening, talking, sa, opening, last_cmd;
    uint8_t tx_byte, tx_eoi;
    /* the channels */
    dchan_t ch[16];
    uint8_t name[64];
    int namelen;
    uint8_t cmd[64];
    int cmdlen;
    char status[48];
    int status_pos;
    uint8_t dirbuf[8192];
    /* burst commands (the 1571/1581 fast serial) */
    uint8_t burst[16];
    int burst_len, burst_pending;
    uint8_t fast_req;               /* the controller asked for fast serial */
    /* a byte on its way out in fast serial: bit time counter */
    int fbit;
    uint8_t fbyte;
    uint64_t ft0;
    uint8_t bq[1100];               /* the burst reply: status bytes and sector data */
    int bq_len, bq_pos;
};

struct part;
diskdrive_t *drive_of(struct part *p);
bool drive_mount(diskdrive_t *d, const char *path, char *msg, int msglen);
void drive_eject(diskdrive_t *d);
void drive_flush(diskdrive_t *d);
const char *drive_disk_name(const diskdrive_t *d);
void drive_reset(diskdrive_t *d);
void drive_command(diskdrive_t *d, const uint8_t *cmd, int len);
void drive_burst_command(diskdrive_t *d, const uint8_t *cmd, int len);
void drive_poll(diskdrive_t *d, int atn, int clk, int data, int srq, uint64_t now);
#endif
