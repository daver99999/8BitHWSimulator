/* c128_drive.c - a disk drive on the serial bus of the C128 (or any machine with the same bus): it answers as
 * device 8 on the ATN, CLK, DATA and SRQ lines of the serial connector, a pin-level device with the timing of the
 * Standard Serial protocol, and keeps a D64, D71 or D81 disk image.
 *
 * It stands for a 1541, 1571 or 1581: the drive's own CPU and controller are not simulated, only what the
 * computer can see: the bus protocol, the Commodore DOS channels (files, the directory, the command channel,
 * direct sector access) and, for the C128, the fast serial and burst commands that the CP/M boot uses.
 *
 * The bus: every line is an open collector, a line reads low if anyone pulls it. The pull is DRV_LO, the
 * release DRV_Z. Time is counted in microseconds, which is system clock cycles. */
#include "c128_drive.h"
#include "parts.h"
#include "chips_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* The disk image                                                            */
/* ------------------------------------------------------------------------ */

enum { DISK_NONE, DISK_D64, DISK_D71, DISK_D81 };

static int zone_sectors(int kind, int t) {
    if (kind == DISK_D81) return 40;
    if (kind == DISK_D71 && t > 35) t -= 35;
    return t < 18 ? 21 : t < 25 ? 19 : t < 31 ? 18 : 17;
}
static int disk_tracks(int kind) { return kind == DISK_D81 ? 80 : kind == DISK_D71 ? 70 : 35; }

/* The offset of track t (1...), sector s in the image, or -1. */
static long disk_offset(const diskdrive_t *d, int t, int s) {
    if (d->kind == DISK_NONE || t < 1 || t > disk_tracks(d->kind) || s < 0 || s >= zone_sectors(d->kind, t)) return -1;
    long n = 0;
    if (d->kind == DISK_D81) n = (long)(t - 1) * 40;
    else for (int i = 1; i < t; i++) n += zone_sectors(d->kind, i);
    return (n + s) * 256;
}
static uint8_t *disk_sector(diskdrive_t *d, int t, int s) {
    long o = disk_offset(d, t, s);
    return o >= 0 && o + 256 <= (long)d->size ? d->img + o : NULL;
}

static int dir_track(const diskdrive_t *d) { return d->kind == DISK_D81 ? 40 : 18; }

bool drive_mount(diskdrive_t *d, const char *path, char *msg, int msglen) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(msg, (size_t)msglen, "Can't open %s", path); return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    int kind = (n == 174848 || n == 175531 || n == 196608 || n == 197376) ? DISK_D64 :
               (n == 349696 || n == 351062) ? DISK_D71 : (n == 819200 || n == 822400) ? DISK_D81 : DISK_NONE;
    if (kind == DISK_NONE) {
        fclose(f);
        snprintf(msg, (size_t)msglen, "%s is not a D64, D71 or D81 disk image (%ld bytes)", path, n);
        return false;
    }
    uint8_t *img = malloc((size_t)n);
    if (!img || fread(img, 1, (size_t)n, f) != (size_t)n) { free(img); fclose(f); snprintf(msg, (size_t)msglen, "Can't read %s", path); return false; }
    fclose(f);
    free(d->img);
    d->img = img;
    d->size = (size_t)n;
    d->kind = kind;
    d->dirty = false;
    snprintf(d->path, sizeof(d->path), "%s", path);
    d->changed = 1;                                 /* a disk change, for the status */
    snprintf(msg, (size_t)msglen, "%s: %s disk image", path, kind == DISK_D81 ? "D81 (1581)" : kind == DISK_D71 ? "D71 (1571)" : "D64 (1541)");
    return true;
}

void drive_eject(diskdrive_t *d) {
    drive_flush(d);
    free(d->img);
    d->img = NULL;
    d->size = 0;
    d->kind = DISK_NONE;
    d->path[0] = 0;
    d->changed = 1;
}

/* Write the image back to its file (after a change). */
void drive_flush(diskdrive_t *d) {
    if (!d->dirty || !d->img || !d->path[0]) return;
    FILE *f = fopen(d->path, "wb");
    if (f) { fwrite(d->img, 1, d->size, f); fclose(f); }
    d->dirty = false;
}

const char *drive_disk_name(const diskdrive_t *d) { return d->kind == DISK_NONE ? NULL : d->path; }

/* ------------------------------------------------------------------------ */
/* Commodore DOS: BAM, directory, files                                      */
/* ------------------------------------------------------------------------ */

static int blocks_free(diskdrive_t *d) {
    int free_blocks = 0;
    if (d->kind == DISK_D81) {
        for (int half = 0; half < 2; half++) {
            const uint8_t *b = disk_sector(d, 40, 1 + half);
            if (!b) continue;
            for (int t = 1; t <= 40; t++) if (half * 40 + t != 40) free_blocks += b[0x10 + (t - 1) * 6];
        }
    } else {
        const uint8_t *b = disk_sector(d, 18, 0);
        if (!b) return 0;
        for (int t = 1; t <= 35; t++) if (t != 18) free_blocks += b[4 + (t - 1) * 4];
        if (d->kind == DISK_D71) for (int t = 36; t <= 70; t++) if (t != 53) free_blocks += b[0xDD + (t - 36)];
    }
    return free_blocks;
}

/* A name match with the wildcards ? and *: both are in PETSCII, the disk's is padded with $A0. */
static bool name_match(const uint8_t *pat, int plen, const uint8_t *name) {
    int nlen = 16;
    while (nlen > 0 && name[nlen - 1] == 0xA0) nlen--;
    int i = 0, j = 0;
    for (; i < plen; i++, j++) {
        if (pat[i] == '*') return true;
        if (j >= nlen) return false;
        if (pat[i] != '?' && pat[i] != name[j]) return false;
    }
    return j == nlen;
}

/* The directory entry (a pointer into the image) of the first file matching, or NULL. type: 0 any, 'P', 'S'... */
static uint8_t *dir_find(diskdrive_t *d, const uint8_t *pat, int plen, int type) {
    int t = dir_track(d), s = d->kind == DISK_D81 ? 3 : 1;
    for (int guard = 0; t && guard < 200; guard++) {
        uint8_t *sec = disk_sector(d, t, s);
        if (!sec) return NULL;
        for (int i = 0; i < 8; i++) {
            uint8_t *e = sec + i * 32;
            int ft = e[2] & 7;
            if (!(e[2] & 0x80) || ft == 0) continue;
            if (type) {
                static const char tc[] = "DSPUR";
                if (ft > 4 || tc[ft] != type) continue;
            }
            if (name_match(pat, plen, e + 5)) return e;
        }
        t = sec[0];
        s = sec[1];
    }
    return NULL;
}

/* The directory as a BASIC program (what LOAD "$",8 reads). */
static int dir_listing(diskdrive_t *d, const uint8_t *pat, int plen, int type, uint8_t *out, int max) {
    int n = 0;
    uint8_t *hdr = disk_sector(d, dir_track(d), 0);
#define PUT(b) do { if (n < max) out[n] = (uint8_t)(b); n++; } while (0)
    PUT(0x01); PUT(0x04);
    int link_at = n;
    PUT(0); PUT(0); PUT(0); PUT(0);                        /* link, line number 0 */
    PUT(0x12); PUT('"');
    const uint8_t *nm = hdr ? hdr + (d->kind == DISK_D81 ? 4 : 0x90) : NULL;
    const uint8_t *id = hdr ? hdr + (d->kind == DISK_D81 ? 22 : 0xA2) : NULL;
    for (int i = 0; i < 16; i++) PUT(nm ? (nm[i] == 0xA0 ? ' ' : nm[i]) : ' ');
    PUT('"'); PUT(' ');
    PUT(id ? id[0] : '0'); PUT(id ? id[1] : '0'); PUT(' ');
    PUT(d->kind == DISK_D81 ? '3' : '2'); PUT(d->kind == DISK_D81 ? 'D' : 'A'); PUT(0);
    if (n <= max) { int a = 0x0401 + (n - 2); out[link_at] = (uint8_t)a; out[link_at + 1] = (uint8_t)(a >> 8); }
    int t = dir_track(d), s = d->kind == DISK_D81 ? 3 : 1;
    for (int guard = 0; t && guard < 200; guard++) {
        uint8_t *sec = disk_sector(d, t, s);
        if (!sec) break;
        for (int i = 0; i < 8; i++) {
            uint8_t *e = sec + i * 32;
            int ft = e[2] & 7;
            if (ft == 0 && !(e[2] & 0x80)) continue;
            if (!(e[2] & 0x80) && ft == 0) continue;
            if (e[2] == 0) continue;
            if (plen && !name_match(pat, plen, e + 5)) continue;
            if (type) { static const char tc[] = "DSPUR"; if (ft > 4 || tc[ft] != type) continue; }
            int blocks = e[30] | e[31] << 8;
            int start = n;
            PUT(0); PUT(0);
            PUT(blocks & 255); PUT(blocks >> 8);
            for (int k = blocks < 10 ? 3 : blocks < 100 ? 2 : blocks < 1000 ? 1 : 0; k > 0; k--) PUT(' ');
            PUT('"');
            int len = 16;
            while (len > 0 && e[5 + len - 1] == 0xA0) len--;
            for (int k = 0; k < len; k++) PUT(e[5 + k]);
            PUT('"');
            for (int k = len; k < 16; k++) PUT(' ');
            PUT(' ');
            static const char *tn[] = { "DEL", "SEQ", "PRG", "USR", "REL", "CBM", "DIR", "???" };
            const char *tt = tn[ft & 7];
            PUT((e[2] & 0x80) ? ' ' : '*');                 /* a file that was not closed */
            PUT(tt[0]); PUT(tt[1]); PUT(tt[2]);
            PUT((e[2] & 0x40) ? '<' : ' ');
            PUT(' '); PUT(' ');
            PUT(0);
            if (start + 1 < max) { int a = 0x0401 + (n - 2); out[start] = (uint8_t)a; out[start + 1] = (uint8_t)(a >> 8); }
        }
        t = sec[0];
        s = sec[1];
    }
    int start = n;
    PUT(0); PUT(0);
    int fb = blocks_free(d);
    PUT(fb & 255); PUT(fb >> 8);
    const char *tail = "BLOCKS FREE.";
    for (const char *p = tail; *p; p++) PUT(*p);
    for (int k = 0; k < 13; k++) PUT(' ');
    PUT(0);
    if (start + 1 < max) { int a = 0x0401 + (n - 2); out[start] = (uint8_t)a; out[start + 1] = (uint8_t)(a >> 8); }
    PUT(0); PUT(0);
#undef PUT
    return n > max ? max : n;
}

/* ------------------------------------------------------------------------ */
/* Channels                                                                  */
/* ------------------------------------------------------------------------ */

enum { CH_FREE, CH_FILE, CH_DIR, CH_CMD, CH_BUF };

static void set_status(diskdrive_t *d, int code, const char *text, int track, int sector) {
    snprintf(d->status, sizeof(d->status), "%02d,%s,%02d,%02d\r", code, text, track, sector);
    d->status_pos = 0;
}
static void status_ok(diskdrive_t *d) { set_status(d, 0, " OK", 0, 0); }
static void status_power(diskdrive_t *d) {
    snprintf(d->status, sizeof(d->status), "73,CBM DOS V10 %s,00,00\r", d->kind == DISK_D81 ? "1581" : d->kind == DISK_D71 ? "1571" : "1541");
    d->status_pos = 0;
}

/* The next byte of a channel: false if there is none. */
static bool chan_next(diskdrive_t *d, dchan_t *c, uint8_t *b, bool *eoi) {
    switch (c->type) {
    case CH_DIR:
        if (c->pos >= c->len) return false;
        *b = d->dirbuf[c->pos++];
        *eoi = c->pos >= c->len;
        return true;
    case CH_FILE: {
        if (c->pos >= c->len) {
            if (!c->t) return false;
            uint8_t *sec = disk_sector(d, c->t, c->s);
            if (!sec) return false;
            memcpy(c->buf, sec + 2, 254);
            c->pos = 0;
            if (sec[0]) { c->t = sec[0]; c->s = sec[1]; c->len = 254; c->last = false; }
            else { c->len = sec[1] > 1 ? sec[1] - 1 : 0; c->t = 0; c->last = true; }
            if (c->len == 0) return false;
        }
        *b = c->buf[c->pos++];
        *eoi = c->last && c->pos >= c->len;
        return true;
    }
    case CH_BUF:
        if (c->pos >= 256) return false;
        *b = c->buf[c->pos++];
        *eoi = c->pos >= 256;
        return true;
    default:
        return false;
    }
}

/* OPEN: the name that came with channel sa. */
static void chan_open(diskdrive_t *d, int sa, const uint8_t *name, int len) {
    dchan_t *c = &d->ch[sa & 15];
    memset(c, 0, sizeof(*c));
    if ((sa & 15) == 15) {
        c->type = CH_CMD;
        if (len) drive_command(d, name, len);
        return;
    }
    if (d->kind == DISK_NONE) { set_status(d, 74, "DRIVE NOT READY", 0, 0); return; }
    if (len && name[0] == '#') { c->type = CH_BUF; c->pos = 0; status_ok(d); return; }
    if (len && name[0] == '$') {
        const uint8_t *pat = name + 1;
        int plen = len - 1;
        for (int i = 0; i < plen; i++) if (pat[i] == ':') { pat += i + 1; plen -= i + 1; break; }
        if (plen > 0 && pat == name + 1 && pat[0] >= '0' && pat[0] <= '9') { pat++; plen--; }
        int type = 0;
        for (int i = 0; i < plen; i++) if (pat[i] == '=') { type = plen > i + 1 ? pat[i + 1] : 0; plen = i; break; }
        c->type = CH_DIR;
        c->len = dir_listing(d, pat, plen, type, d->dirbuf, (int)sizeof(d->dirbuf));
        c->pos = 0;
        status_ok(d);
        return;
    }
    /* a file: "[0:]name[,P[,R]]" */
    const uint8_t *p = name;
    int n = len;
    for (int i = 0; i < n; i++) if (p[i] == ':') { p += i + 1; n -= i + 1; break; }
    int comma = n;
    for (int i = 0; i < n; i++) if (p[i] == ',') { comma = i; break; }
    int write = sa == 1;
    for (int i = comma; i + 1 < n; i++) if (p[i] == ',' && (p[i + 1] == 'W')) write = 1;
    if (write) { set_status(d, 26, "WRITE PROTECT ON", 0, 0); return; }
    uint8_t *e = dir_find(d, p, comma, 0);
    if (!e) { set_status(d, 62, "FILE NOT FOUND", 0, 0); return; }
    c->type = CH_FILE;
    c->t = e[3];
    c->s = e[4];
    c->pos = c->len = 0;
    status_ok(d);
}

static void chan_close(diskdrive_t *d, int sa) { memset(&d->ch[sa & 15], 0, sizeof(dchan_t)); (void)d; }

/* The command channel: the commands this drive knows. */
void drive_command(diskdrive_t *d, const uint8_t *cmd, int len) {
    char s[64];
    int n = len > 60 ? 60 : len;
    while (n > 0 && (cmd[n - 1] == '\r' || cmd[n - 1] == '\n')) n--;
    memcpy(s, cmd, (size_t)n);
    s[n] = 0;
    if (n == 0) { status_ok(d); return; }
    if (d->kind == DISK_NONE && s[0] != 'U' && s[0] != 'I') { set_status(d, 74, "DRIVE NOT READY", 0, 0); return; }
    if (s[0] == 'I') { status_ok(d); d->changed = 0; return; }
    if (s[0] == 'U' && (s[1] == '1' || s[1] == 'A' || s[1] == '2' || s[1] == 'B')) {
        int ch = 0, dr = 0, t = 0, sec = 0;
        const char *p = strchr(s, ':');
        p = p ? p + 1 : s + 2;
        if (sscanf(p, "%d %d %d %d", &ch, &dr, &t, &sec) == 4 || sscanf(p, "%d,%d,%d,%d", &ch, &dr, &t, &sec) == 4) {
            dchan_t *c = &d->ch[ch & 15];
            if (c->type != CH_BUF) { set_status(d, 70, "NO CHANNEL", 0, 0); return; }
            uint8_t *b = disk_sector(d, t, sec);
            if (!b) { set_status(d, 66, "ILLEGAL TRACK OR SECTOR", t, sec); return; }
            if (s[1] == '1' || s[1] == 'A') { memcpy(c->buf, b, 256); c->pos = 0; }
            else { memcpy(b, c->buf, 256); d->dirty = true; }
            status_ok(d);
            return;
        }
        set_status(d, 30, "SYNTAX ERROR", 0, 0);
        return;
    }
    if (!strncmp(s, "B-P", 3) || !strncmp(s, "B-R", 3) || !strncmp(s, "B-W", 3)) {
        int ch = 0, dr = 0, t = 0, sec = 0;
        const char *p = strchr(s, ':');
        p = p ? p + 1 : s + 3;
        int got = sscanf(p, "%d %d %d %d", &ch, &dr, &t, &sec);
        if (got < 2) got = sscanf(p, "%d,%d,%d,%d", &ch, &dr, &t, &sec);
        dchan_t *c = &d->ch[ch & 15];
        if (c->type != CH_BUF) { set_status(d, 70, "NO CHANNEL", 0, 0); return; }
        if (s[2] == 'P') { c->pos = dr; status_ok(d); return; }
        uint8_t *b = disk_sector(d, t, sec);
        if (!b) { set_status(d, 66, "ILLEGAL TRACK OR SECTOR", t, sec); return; }
        if (s[2] == 'R') { memcpy(c->buf, b, 256); c->pos = 0; } else { memcpy(b, c->buf, 256); d->dirty = true; }
        status_ok(d);
        return;
    }
    if (s[0] == 'U' && s[1] == '0') { drive_burst_command(d, cmd, len); return; }
    status_ok(d);
}

/* ------------------------------------------------------------------------ */
/* Burst commands of the 1571 and 1581 (U0 + a byte with the command)        */
/* ------------------------------------------------------------------------ */

/* The physical position of a 512 byte sector of the 1581: cylinder 0-79, side 0-1, sector 1-10. */
static uint8_t *disk_sector512(diskdrive_t *d, int cyl, int side, int sec) {
    if (d->kind != DISK_D81 || cyl < 0 || cyl > 79 || side < 0 || side > 1 || sec < 1 || sec > 10) return NULL;
    long o = (long)cyl * 10240 + side * 5120 + (sec - 1) * 512;
    return o + 512 <= (long)d->size ? d->img + o : NULL;
}

void drive_burst_command(diskdrive_t *d, const uint8_t *cmd, int len) {
    /* cmd = "U0" + the command byte and its parameters (the leading $55 $30 are 'U' '0') */
    d->burst_len = len > 16 ? 16 : len;
    memcpy(d->burst, cmd, (size_t)d->burst_len);
    d->burst_pending = 1;
    status_ok(d);
}

/* ------------------------------------------------------------------------ */
/* The bus                                                                   */
/* ------------------------------------------------------------------------ */

enum {
    S_IDLE,                 /* not addressed */
    S_RX_READY,             /* receiving: wait for the sender to release CLK (DATA is pulled) */
    S_RX_START,             /* DATA released, wait for CLK to be pulled (EOI after 200 us) */
    S_RX_EOI,               /* the EOI acknowledge: DATA pulled for 60 us */
    S_RX_BIT_HI,            /* wait for CLK to rise: the bit is on DATA */
    S_RX_BIT_LO,            /* wait for CLK to fall */
    S_TX_TURN,              /* after the talk command: wait for the controller to release CLK */
    S_TX_NEXT,              /* between bytes */
    S_TX_WAIT,              /* nothing to send: hold CLK until the controller lets go */
    S_TX_READY,             /* CLK released, wait for DATA released (the receiver is ready) */
    S_TX_EOI,               /* the last byte: wait for the receiver's EOI acknowledge ... */
    S_TX_EOI_LO,            /* ... DATA pulled by it ... */
    S_TX_BIT_A,             /* CLK pulled, the bit on DATA: hold */
    S_TX_BIT_B,             /* CLK released, the bit is valid: hold */
    S_TX_ACK,               /* after the byte: wait for the receiver to pull DATA */
    S_TX_DONE,              /* the last byte went: lines released, wait for UNTALK */
};

#define T_BIT       70      /* microseconds the sender holds each clock phase */
#define T_EOI       200     /* the receiver waits this long before it answers with an EOI acknowledge */
#define T_EOI_ACK   70
#define T_EOI_WAIT  260     /* the sender waits this long for the acknowledge */
#define T_BETWEEN   100

void drive_reset(diskdrive_t *d) {
    d->state = S_IDLE;
    d->pull_clk = d->pull_data = d->pull_srq = 0;
    d->listening = d->talking = d->atn_cmd = d->opening = 0;
    d->atn = d->clk = d->data = d->srq = 1;
    d->cmdlen = d->namelen = 0;
    d->burst_pending = d->bq_len = d->bq_pos = 0;
    memset(d->ch, 0, sizeof(d->ch));
    status_power(d);
}

/* A byte arrived from the controller. */
static void rx_byte(diskdrive_t *d, uint8_t b) {
    if (d->atn_cmd) {                                   /* a command under ATN */
        d->last_cmd = b;
        if (b == 0x3F) d->listening = 0;
        else if (b == 0x5F) d->talking = 0;
        else if ((b & 0xE0) == 0x20) { d->listening = (b & 0x1F) == d->device; if (d->listening) d->talking = 0; }
        else if ((b & 0xE0) == 0x40) { d->talking = (b & 0x1F) == d->device; if (d->talking) d->listening = 0; }
        else if (d->listening || d->talking) {
            d->sa = b & 15;
            if ((b & 0xF0) == 0xE0) chan_close(d, d->sa);
            else if ((b & 0xF0) == 0xF0) { d->opening = 1; d->namelen = 0; }
        }
        return;
    }
    if (!d->listening) return;
    if (d->opening) {
        if (d->namelen < (int)sizeof(d->name)) d->name[d->namelen++] = b;
    } else if (d->sa == 15) {
        if (d->cmdlen < (int)sizeof(d->cmd)) d->cmd[d->cmdlen++] = b;
    }
}

/* Fetch the next byte to send on the channel being talked on. */
static bool tx_fetch(diskdrive_t *d) {
    if (d->sa == 15) {
        if (d->status_pos >= (int)strlen(d->status)) status_ok(d);
        d->tx_byte = (uint8_t)d->status[d->status_pos++];
        d->tx_eoi = d->status[d->status_pos] == 0;
        return true;
    }
    bool eoi = false;
    uint8_t b;
    if (!chan_next(d, &d->ch[d->sa], &b, &eoi)) return false;
    d->tx_byte = b;
    d->tx_eoi = eoi;
    return true;
}

/* What was listened to is complete (UNLISTEN, or attention again): the name of an OPEN, or a command. */
static void end_listen(diskdrive_t *d) {
    if (d->opening) {
        chan_open(d, d->sa, d->name, d->namelen);
        d->opening = 0;
    } else if (d->sa == 15 && d->cmdlen) {
        drive_command(d, d->cmd, d->cmdlen);
    }
    d->cmdlen = 0;
}

static void tx_begin_bits(diskdrive_t *d, uint64_t now) {
    d->pull_clk = 1;
    d->pull_data = !(d->tx_byte & 1);
    d->nbits = 1;
    d->t0 = now;
    d->state = S_TX_BIT_A;
}

/* One look at the bus: atn, clk, data, srq are the levels (1 = released) and now the time in microseconds. */
void drive_poll(diskdrive_t *d, int atn, int clk, int data, int srq, uint64_t now) {
    if (!d->enabled) return;
    int atn_fall = d->atn && !atn, atn_rise = !d->atn && atn, clk_low = !clk;
    d->atn = (uint8_t)atn;
    d->clk = (uint8_t)clk;
    d->data = (uint8_t)data;
    d->srq = (uint8_t)srq;

    if (atn_fall) {                                     /* attention: every device answers within 1 ms */
        if (d->listening) end_listen(d);
        d->atn_cmd = 1;
        d->state = S_RX_READY;
        d->armed = (uint8_t)clk_low;
        d->pull_data = 1;
        d->pull_clk = 0;
        return;
    }
    if (atn_rise) {
        d->atn_cmd = 0;
        if (d->talking) {                               /* the controller turns around and becomes the receiver */
            d->state = S_TX_TURN;
            d->pull_data = 0;
            d->pull_clk = 0;
        } else if (d->listening) {
            d->state = S_RX_READY;
            d->armed = (uint8_t)clk_low;
            d->pull_data = 1;
        } else {
            d->state = S_IDLE;
            d->pull_data = d->pull_clk = 0;
        }
        return;
    }

    switch (d->state) {
    case S_RX_READY:
        if (clk_low) d->armed = 1;
        else if (d->armed) {                            /* the sender is ready: so are we */
            d->pull_data = 0;
            d->state = S_RX_START;
            d->t0 = now;
            d->eoi = 0;
        }
        break;
    case S_RX_START:
        if (clk_low) { d->state = S_RX_BIT_HI; d->nbits = 0; d->shift = 0; }
        else if (!d->eoi && now - d->t0 >= T_EOI) {     /* the sender waits: that is the last byte */
            d->eoi = 1;
            d->pull_data = 1;
            d->t0 = now;
            d->state = S_RX_EOI;
        }
        break;
    case S_RX_EOI:
        if (now - d->t0 >= T_EOI_ACK) { d->pull_data = 0; d->state = S_RX_START; }
        break;
    case S_RX_BIT_HI:
        if (clk) {
            d->shift = (uint8_t)((d->shift >> 1) | (data ? 0x80 : 0));
            d->nbits++;
            d->state = S_RX_BIT_LO;
        }
        break;
    case S_RX_BIT_LO:
        if (clk_low) {
            if (d->nbits < 8) { d->state = S_RX_BIT_HI; break; }
            d->pull_data = 1;                           /* busy: the byte is taken */
            rx_byte(d, d->shift);
            d->state = d->atn_cmd || d->listening ? S_RX_READY : S_IDLE;
            d->armed = 1;
        }
        break;
    case S_TX_TURN:
        if (clk) {                                      /* the controller released CLK: it is ours now */
            d->pull_clk = 1;
            d->pull_data = 0;
            d->state = S_TX_NEXT;
            d->t0 = now;
        }
        break;
    case S_TX_NEXT:
        if (now - d->t0 >= T_BETWEEN) {
            if (tx_fetch(d)) { d->pull_clk = 0; d->state = S_TX_READY; }     /* ready to send */
            else d->state = S_TX_WAIT;                  /* nothing to send: CLK stays pulled */
        }
        break;
    case S_TX_READY:
        if (data) {                                     /* every receiver is ready */
            if (d->tx_eoi) { d->t0 = now; d->state = S_TX_EOI; }
            else tx_begin_bits(d, now);
        }
        break;
    case S_TX_EOI:
        if (!data) d->state = S_TX_EOI_LO;              /* the receiver acknowledges the EOI ... */
        else if (now - d->t0 > 4000) d->state = S_TX_DONE;
        break;
    case S_TX_EOI_LO:
        if (data) tx_begin_bits(d, now);                /* ... and lets go */
        break;
    case S_TX_BIT_A:
        if (now - d->t0 >= T_BIT) { d->pull_clk = 0; d->t0 = now; d->state = S_TX_BIT_B; }
        break;
    case S_TX_BIT_B:
        if (now - d->t0 >= T_BIT) {
            d->t0 = now;
            if (d->nbits >= 8) { d->pull_clk = 1; d->pull_data = 0; d->state = S_TX_ACK; }
            else { d->pull_clk = 1; d->pull_data = !((d->tx_byte >> d->nbits) & 1); d->nbits++; d->state = S_TX_BIT_A; }
        }
        break;
    case S_TX_ACK:
        if (!data) {                                    /* the receiver took the byte */
            d->t0 = now;
            if (d->tx_eoi) { d->pull_clk = 0; d->state = S_TX_DONE; }
            else d->state = S_TX_NEXT;
        } else if (now - d->t0 > 5000) {
            d->pull_clk = 0;
            d->state = S_TX_DONE;
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------------ */
/* The part: the serial connector with a drive behind it                     */
/* ------------------------------------------------------------------------ */

static void drv_eval(sim_t *s, part_t *p) {
    diskdrive_t *d = p->state;
    pin_out(s, p, 1, d->enabled && d->pull_srq ? DRV_LO : DRV_Z);
    pin_out(s, p, 3, DRV_Z);
    pin_out(s, p, 4, d->enabled && d->pull_clk ? DRV_LO : DRV_Z);
    pin_out(s, p, 5, d->enabled && d->pull_data ? DRV_LO : DRV_Z);
}
static void drv_commit(sim_t *s, part_t *p) {
    diskdrive_t *d = p->state;
    if (!d->enabled) return;
    uint8_t pc = d->pull_clk, pd = d->pull_data, ps = d->pull_srq;
    drive_poll(d, pin_in(s, p, 3), pin_in(s, p, 4), pin_in(s, p, 5), pin_in(s, p, 1), s->cycle);
    if (pc != d->pull_clk || pd != d->pull_data || ps != d->pull_srq) sim_mark_dirty(s, (int)(p - s->parts));
}
static void drv_reset(sim_t *s, part_t *p) {
    (void)s;
    diskdrive_t *d = p->state;
    drive_reset(d);
    if (!d->device) d->device = 8;
}
const part_type_t PT_IECDRV = { "serial port",
    "SRQ:X GND:O ATN:X CLK:X DATA:X RESET:m",
    drv_eval, drv_commit, drv_reset, sizeof(diskdrive_t), TECH_NMOS };
diskdrive_t *drive_of(part_t *p) { return p->state; }
