#!/usr/bin/env python3
"""make_icon.py - draw the program's icon: an 8-bit home computer (a breadbin
style machine with a blue READY. screen) and write

    src/c64-hw-sim.ico   the icon in the .exe (src/icon.rc, build.bat, Makefile)
    src/icon_data.c      the icon of the window and the taskbar (48, 32, 16 px)

    python3 tools/make_icon.py          (needs Pillow)

Drawn at 1024 px and shrunk for each size. No logo or artwork of any
machine is copied: it is a generic keyboard computer with a monitor.
"""
import io
import os
import struct
import sys
from PIL import Image, ImageDraw

S = 1024            # master size
U = S / 256.0       # 1 design unit

def u(v):
    return int(round(v * U))

def poly(d, pts, fill, outline=None, width=0):
    d.polygon([(u(x), u(y)) for x, y in pts], fill=fill, outline=outline)

def rrect(d, x0, y0, x1, y1, r, fill):
    d.rounded_rectangle([u(x0), u(y0), u(x1), u(y1)], radius=u(r), fill=fill)

BEIGE = (216, 203, 176, 255)
BEIGE_LIGHT = (234, 224, 202, 255)
BEIGE_DARK = (172, 158, 130, 255)
BEIGE_SHADE = (141, 128, 104, 255)
KEY = (96, 82, 70, 255)
KEY_TOP = (128, 112, 96, 255)
BEZEL = (58, 58, 62, 255)
BEZEL_LIGHT = (92, 92, 98, 255)
BORDER = (136, 126, 203, 255)       # light blue
SCREEN = (64, 49, 141, 255)         # blue
TEXT = (160, 150, 235, 255)

def draw():
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # monitor
    rrect(d, 40, 14, 216, 150, 14, BEZEL)
    rrect(d, 44, 18, 212, 100, 12, BEZEL_LIGHT)
    rrect(d, 40, 14, 216, 146, 14, BEZEL)
    rrect(d, 50, 24, 206, 136, 8, BORDER)
    rrect(d, 62, 36, 194, 124, 4, SCREEN)
    # READY. in blocks (a 5x7-ish pixel font, 3 units a pixel), then a cursor
    def block(x, y, w=3, h=3, c=TEXT):
        d.rectangle([u(x), u(y), u(x + w) - 1, u(y + h) - 1], fill=c)
    glyphs = {
        'R': ["XXX.", "X..X", "XXX.", "X.X.", "X..X"],
        'E': ["XXXX", "X...", "XXX.", "X...", "XXXX"],
        'A': [".XX.", "X..X", "XXXX", "X..X", "X..X"],
        'D': ["XXX.", "X..X", "X..X", "X..X", "XXX."],
        'Y': ["X..X", "X..X", ".XX.", ".X..", ".X.."],
        '.': ["...", "...", "...", "...", ".X."],
    }
    x = 68
    for ch in "READY.":
        g = glyphs[ch]
        for r, row in enumerate(g):
            for c, cell in enumerate(row):
                if cell == "X":
                    block(x + c * 3, 44 + r * 3)
        x += (len(g[0]) + 1) * 3
    block(68, 66, 7, 12)           # the cursor
    # a little stand
    poly(d, [(112, 146), (144, 146), (150, 158), (106, 158)], BEZEL)
    # the keyboard unit: slanted top, front lip
    poly(d, [(58, 160), (198, 160), (236, 214), (20, 214)], BEIGE_LIGHT)          # top face
    poly(d, [(20, 214), (236, 214), (240, 236), (16, 236)], BEIGE)                # front
    poly(d, [(16, 236), (240, 236), (240, 242), (16, 242)], BEIGE_SHADE)          # base shade
    poly(d, [(16, 214), (20, 214), (16, 236)], BEIGE_DARK)
    poly(d, [(236, 214), (240, 236), (240, 214)], BEIGE_DARK)
    # keys: rows in perspective (the top face runs from y 160 to y 214)
    def lerp(a, b, t):
        return a + (b - a) * t
    rows = [(166, 176, 11), (178, 190, 12), (192, 204, 11)]
    for (ya, yb, n) in rows:
        ta = (ya - 160) / 54.0
        tb = (yb - 160) / 54.0
        xl_a, xr_a = lerp(58, 20, ta), lerp(198, 236, ta)
        xl_b, xr_b = lerp(58, 20, tb), lerp(198, 236, tb)
        wa = (xr_a - xl_a) / n
        wb = (xr_b - xl_b) / n
        gap = 0.18
        for k in range(n):
            pts = [(xl_a + wa * (k + gap), ya), (xl_a + wa * (k + 1 - gap), ya),
                   (xl_b + wb * (k + 1 - gap), yb), (xl_b + wb * (k + gap), yb)]
            poly(d, pts, KEY)
            pts2 = [(xl_a + wa * (k + gap), ya), (xl_a + wa * (k + 1 - gap), ya),
                    (lerp(xl_a + wa * (k + 1 - gap), xl_b + wb * (k + 1 - gap), .35), lerp(ya, yb, .35)),
                    (lerp(xl_a + wa * (k + gap), xl_b + wb * (k + gap), .35), lerp(ya, yb, .35))]
            poly(d, pts2, KEY_TOP)
    # the space bar
    ya, yb = 206, 212
    ta, tb = (ya - 160) / 54.0, (yb - 160) / 54.0
    xa0, xa1 = lerp(58, 20, ta) + 40 * (1 + ta * .2), lerp(198, 236, ta) - 40 * (1 + ta * .2)
    xb0, xb1 = lerp(58, 20, tb) + 40 * (1 + tb * .2), lerp(198, 236, tb) - 40 * (1 + tb * .2)
    poly(d, [(xa0, ya), (xa1, ya), (xb1, yb), (xb0, yb)], KEY)
    # the power LED
    d.ellipse([u(214), u(222), u(222), u(230)], fill=(220, 40, 40, 255))
    d.ellipse([u(216), u(223), u(219), u(226)], fill=(255, 170, 170, 255))
    return im

def png_bytes(im):
    b = io.BytesIO()
    im.save(b, "PNG")
    return b.getvalue()

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    src = os.path.join(here, "..", "src")
    master = draw()
    sizes = [256, 128, 64, 48, 32, 24, 16]
    imgs = {s: master.resize((s, s), Image.LANCZOS) for s in sizes}
    # the .ico: PNG entries
    pngs = {s: png_bytes(imgs[s]) for s in sizes}
    out = bytearray(struct.pack("<HHH", 0, 1, len(sizes)))
    off = 6 + 16 * len(sizes)
    for s in sizes:
        out += struct.pack("<BBBBHHII", 0 if s >= 256 else s, 0 if s >= 256 else s, 0, 0, 1, 32, len(pngs[s]), off)
        off += len(pngs[s])
    for s in sizes:
        out += pngs[s]
    open(os.path.join(src, "c64-hw-sim.ico"), "wb").write(bytes(out))
    # the window icon
    lines = ["/* icon_data.c - the program's icon for the window and the taskbar (the",
             " * .exe's own is src/c64-hw-sim.ico). Made by tools/make_icon.py: do not",
             " * edit. RGBA, 8 bits each, rows from the top. */",
             '#include "icon_data.h"', ""]
    for s in (48, 32, 16):
        data = imgs[s].tobytes()
        lines.append("static const unsigned char icon%d[%d] = {" % (s, len(data)))
        for i in range(0, len(data), 16):
            lines.append("    " + ",".join("0x%02x" % b for b in data[i:i + 16]) + ",")
        lines.append("};")
        lines.append("")
    lines.append("const icon_image_t icon_images[ICON_NIMAGES] = {")
    for s in (48, 32, 16):
        lines.append("    { %d, %d, icon%d, sizeof(icon%d) }," % (s, s, s, s))
    lines.append("};")
    open(os.path.join(src, "icon_data.c"), "w").write("\n".join(lines) + "\n")
    master.resize((512, 512), Image.LANCZOS).save("/tmp/icon_new_512.png")
    sheet = Image.new("RGBA", (256 + 128 + 64 + 48 + 32 + 24 + 16 + 70, 270), (60, 60, 60, 255))
    x = 10
    for s in sizes:
        sheet.paste(imgs[s], (x, 10), imgs[s]); x += s + 10
    sheet.save("/tmp/icon_new_sizes.png")

main()
