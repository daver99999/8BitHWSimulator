#!/usr/bin/env python3
"""c64_250425_from_250407.py - make the C64 ASSY 250425's board tables from the
250407's (src/c64_board_data.c).

    python3 tools/c64_250425_from_250407.py src/c64_board_data.c > src/c64b_board_data.c

The 250425 is the 250407 with the clock PLL (U29 74LS74, U30 74LS193, U32
MC4044, the 74LS629 VCO U31 and their loop passives) replaced by the 8701
clock generator at U31 (schematic 251469 in the service manual: Y1 across
pins 14 and 13, +5V from the clock supply on pins 12 and 15, ground on 2, 4
and 9, DOT CLOCK on pin 6, COLOR CLOCK on pin 8, the PAL/NTSC jumper on
pin 7). Everything else is the 250407 (same chips, same references).

Not known: the 250425's own copper. The geometry is the 250407's, with the
8701 in the footprint of the 74LS629 (a DIP16 as well) and the PLL parts
removed. Correct it from a photo of the board when there is one.
"""
import re
import sys

REMOVE_ICS = {"U29", "U30", "U32"}
# the loop passives that only served the PLL: the VCO supply filter, the
# loop filter and its transistor, the range pot, the divider's capacitors
REMOVE_PASSIVES = {"C83", "C85", "C86", "C107", "C108", "Q7", "R16", "R17", "R19", "R26", "R27"}
# the 8701's pins (the PT_8701 of src/parts.c) and the nets they go to
PINS_8701 = {"2": "Earth", "4": "Earth", "6": "Net-(FB17-Pad1)", "7": "jumper_select",
             "8": "Net-(FB16-Pad1)", "9": "Earth", "12": "+Vc", "13": "Net-(C70-Pad1)",
             "14": "Net-(U31-Pad12)", "15": "+Vc"}
# The schematic (251469) has Y1 straight across pins 14 and 13 and a trimmer
# CT1 from pin 13 to ground, where the 250407 has C70 in series with the crystal.
RENAME_NETS = {"Net-(U31-Pad12)": "XTAL1", "Net-(C70-Pad1)": "XTAL2"}
# U27 (74LS08) uses other gates on the 251469 than on the 250407 replica:
# pin -> net (None: unchanged name below)
U27_251469 = {"1": "AEC", "2": "/DMA", "3": "CAEC", "4": "AEC", "5": "/COLOR", "6": "Net-(U27-Pad11)",
              "8": "60Hz", "9": "Net-(R100-Pad2)", "10": "+5V", "11": "RDY", "12": "BA", "13": "/DMA"}
# R31, the /AEC pull-up, is 180 ohms on the 251469 (1K on the replica)
VALUES = {"R31": "180", "C70": "trimmer"}
REF_RENAME = {"C70": "CT1"}


# Parts that sit elsewhere on the 250425 than on the 250407 (position of the
# part's centre in the 2400 x 1131 photo of an 250425, and a quarter turn
# counter clockwise per `turns`). The photo maps to the board outline with
# PX0/PY0 (the board's top left corner in the photo) and PSX/PSY (pixels per mm).
PX0, PY0, PSX, PSY = 28.0, 60.0, 6.0303, 5.8666
MOVES = {
    "U17": (1026, 690, 0),     # the PLA is below the SID here (the replica has it above)
    "U18": (1000, 335, 0),     # the SID: centred between the 6510 and the passives to its right
    "U19": (1481, 823, 1),     # the VIC lies on its side in the shielded corner, notch to the left
    "U31": (1580, 697, 1),     # the 8701
    "Y1": (1575, 597, 0),
    "C70": (1665, 575, 0),     # the trimmer (yellow)
    "C58": (1655, 652, 0),
}


def move_part(ps, target, x0, y0):
    """ps: list of (name, x, y, w, h, net) -> the same, moved so its centre is at
    the photo position and turned; x0/y0: the board outline's corner (mm)."""
    cx, cy, turns = target
    mx = x0 + (cx - PX0) / PSX
    my = y0 + (cy - PY0) / PSY
    xs = [float(p[1].rstrip("f")) for p in ps]
    ys = [float(p[2].rstrip("f")) for p in ps]
    ox, oy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    out = []
    for (name, x, y, w, h, nn) in ps:
        dx, dy = float(x.rstrip("f")) - ox, float(y.rstrip("f")) - oy
        w, h = float(w.rstrip("f")), float(h.rstrip("f"))
        for _ in range(turns % 4):
            dx, dy = dy, -dx
            w, h = h, w
        out.append((name, "%.3ff" % (mx + dx), "%.3ff" % (my + dy), "%.3ff" % w, "%.3ff" % h, nn))
    return out


def main():
    text = open(sys.argv[1]).read()
    head = text[:text.index("static const char *const nets[]")]
    nets_txt = text[text.index("static const char *const nets[]"):text.index("static const bd_pad_t pads[]")]
    pads_txt = text[text.index("static const bd_pad_t pads[]"):text.index("static const bd_comp_t comps[]")]
    comps_txt = text[text.index("static const bd_comp_t comps[]"):text.index("const board_data_t")]
    outline = re.search(r"\{\s*([-\d.]+f, [-\d.]+f, [-\d.]+f, [-\d.]+f)\s*\},\s*nets", text).group(1)
    nets = re.findall(r'^    "(.*)",$', nets_txt, re.M)
    pads = re.findall(r'^    \{ "([^"]*)", ([^,]+), ([^,]+), ([^,]+), ([^,]+), (-?\d+) \},$', pads_txt, re.M)
    comps = re.findall(r'^    \{ "([^"]*)", "(.*)", "([^"]*)", (\d+), (\d+) \},$', comps_txt, re.M)
    OX0, OY0 = 16.592, 51.911
    assert len(nets) == 249 and len(pads) == 1205 and len(comps) == 215, (len(nets), len(pads), len(comps))
    out_comps = []     # (ref, value, footprint, [(pad, x, y, w, h, netname)])
    for ref, val, fp, first, n in comps:
        if ref in REMOVE_ICS or ref in REMOVE_PASSIVES:
            continue
        ps = []
        for i in range(int(first), int(first) + int(n)):
            name, x, y, w, h, net = pads[i]
            nn = nets[int(net)] if int(net) >= 0 else None
            if ref == "U31":
                nn = PINS_8701.get(name)
            if ref == "U27":
                nn = U27_251469.get(name, nn)
            if ref == "C70" and name == "2":
                nn = "Earth"
            if nn in RENAME_NETS:
                nn = RENAME_NETS[nn]
            ps.append((name, x, y, w, h, nn))
        if ref in MOVES:
            ps = move_part(ps, MOVES[ref], OX0, OY0)
        if ref == "U31":
            val = "MOS8701"
        val = VALUES.get(ref, val)
        ref = REF_RENAME.get(ref, ref)
        out_comps.append((ref, val, fp, ps))
    used = []
    seen = set()
    for _, _, _, ps in out_comps:
        for p in ps:
            if p[5] is not None and p[5] not in seen:
                seen.add(p[5])
                used.append(p[5])
    idx = {n: i for i, n in enumerate(used)}
    o = []
    o.append("/* c64b_board_data.c - GENERATED by tools/c64_250425_from_250407.py, do not edit.\n"
             " * C64 ASSY 250425: the 250407's geometry and netlist (bwack's KiCad replica, CERN OHL v1.2)\n"
             " * with the clock PLL replaced by the 8701 (schematic 251469). The 250425's own copper\n"
             " * is not known: see the script. */\n")
    o.append('#include "board_data.h"\n\nstatic const char *const nets[] = {\n')
    for n in used:
        o.append('    "%s",\n' % n)
    o.append("};\n\nstatic const bd_pad_t pads[] = {\n")
    comp_lines = []
    cur = 0
    for ref, val, fp, ps in out_comps:
        comp_lines.append('    { "%s", "%s", "%s", %d, %d },\n' % (ref, val, fp, cur, len(ps)))
        for name, x, y, w, h, nn in ps:
            o.append('    { "%s", %s, %s, %s, %s, %d },\n' % (name, x, y, w, h, idx[nn] if nn is not None else -1))
        cur += len(ps)
    o.append("};\n\nstatic const bd_comp_t comps[] = {\n")
    o.extend(comp_lines)
    o.append("};\n\nconst board_data_t bd_c64b = {\n    { %s },\n    nets, %d, pads, %d, comps, %d,\n};\n" %
             (outline, len(used), cur, len(out_comps)))
    sys.stdout.write("".join(o))


main()
