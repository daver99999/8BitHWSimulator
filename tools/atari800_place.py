"""atari800_place.py - where the parts of the Atari 800's plug-in boards sit.

Each board is a table in the pixels of its source picture and a scale (px per mm):
  cpu   the CPU card (the board marked 5): photos of a real card (13 px/mm), R308, R311 from the Field Service Manual's silkscreen (Figure 2-11)
  pers  the personality (OS ROM) card: the manual's silkscreen (Figure 2-9)
  ram   a 16K RAM card (marked 11), used for all three: the manual's silkscreen (Figure 2-15)
  mb    the main board: the manual's silkscreen (Figure 2-6); the parts it does not name are laid out in a strip by the generator
Chips are given as (x, y, mode) of the middle of the chip, mode R: notch right, pin 1 at the top right (the CPU card),
L: notch left, pin 1 at the bottom left (pins along x), T: notch up, pin 1 at the top left (pins down), B: notch down, pin 1 at the bottom right (pins down).  Two-lead parts: (x, y, 'h' or 'v').
The silkscreen drawings are not to scale, so the scale of each is fitted to its chips; positions are good to a few mm."""

BOARDS = {}

BOARDS['cpu'] = dict(
    k=13.0, org=(215, 95), edge=('J101C', 1165, 440, 1750),
    IC={'Z302': (705, 235, 'R'), 'Z301': (994, 250, 'R'), 'Z303': (770, 490, 'R'), 'A303': (648, 700, 'R'), 'Z304': (480, 962, 'R'),
        'A301': (615, 1470, 'R'), 'A302': (655, 2030, 'R')},
    TWO={'C305': (470, 160, 'h'), 'C301': (470, 205, 'h'), 'R309': (470, 340, 'v'), 'R312': (440, 505, 'h'), 'R313': (430, 862, 'h'), 'R310': (385, 1280, 'h'),
         'R307': (1010, 455, 'v'), 'C307': (1048, 480, 'v'), 'C306': (1085, 470, 'v'), 'C304': (275, 650, 'v'), 'C302': (255, 1460, 'v'), 'C303': (235, 1975, 'v'),
         'R305': (410, 1690, 'h'), 'R304': (410, 1735, 'h'), 'R303': (410, 1790, 'h'), 'R302': (415, 1835, 'h'), 'R306': (670, 1665, 'h'),
         'R308': (1135, 258, 'v'), 'R311': (378, 2186, 'h')})

BOARDS['pers'] = dict(
    k=8.6, org=(220, 40), edge=('PERSC', 830, 80, 1300),
    IC={'A401': (485, 1215, 'L'), 'A403': (452, 987, 'L'), 'A402': (486, 755, 'L'), 'Z401': (462, 591, 'L'), 'Z403': (772, 690, 'L'), 'Z402': (772, 426, 'L')},
    TWO={'C403': (322, 1200, 'v'), 'C406': (288, 970, 'v'), 'C405': (322, 740, 'v'), 'C404': (340, 590, 'v'), 'C407': (445, 60, 'h'), 'C408': (445, 160, 'h'),
         'C409': (445, 260, 'h'), 'C410': (445, 360, 'h'), 'C401': (770, 195, 'h'), 'C402': (770, 222, 'h'),
         'L401': (420, 108, 'h'), 'L402': (420, 208, 'h'), 'L403': (420, 308, 'h'), 'R403': (632, 108, 'h'),
         'R404': (745, 495, 'h'), 'R401': (745, 525, 'h'), 'R402': (745, 558, 'h')})

ram_ic = {}
ram_two = {}
for i, y in enumerate((322, 438, 565, 678, 785, 897, 1017, 1127)):
    ram_ic['Z%d' % (505 + i)] = (437, y, 'L')
for ref, (x, y) in {'C503': (338, 300), 'C502': (523, 300), 'C505': (338, 430), 'C504': (523, 423), 'C507': (338, 540), 'C506': (523, 540), 'C509': (338, 655),
                    'C508': (523, 650), 'C511': (338, 765), 'C510': (523, 765), 'C513': (338, 880), 'C512': (523, 885), 'C515': (338, 995), 'C514': (523, 1000),
                    'C517': (338, 1105), 'C516': (523, 1100)}.items():
    ram_two[ref] = (x, y, 'v')
ram_two.update({'C501': (690, 455, 'h'), 'C519': (690, 785, 'h'), 'R501': (632, 385, 'v'), 'L501': (660, 322, 'v'),
                'C518': (668, 215, 'v'), 'C521': (688, 215, 'v'), 'C522': (706, 215, 'v'), 'C523': (724, 215, 'v')})
ram_ic.update({'Z501': (695, 537, 'T'), 'Z503': (695, 692, 'T'), 'Z504': (695, 878, 'T'), 'Z502': (695, 1059, 'T')})
BOARDS['ram'] = dict(k=6.7, org=(300, 190), edge=('RB', 800, 230, 1160), IC=ram_ic, TWO=ram_two)

# ---- the main board: px of the manual's page (Figure 2-6), 5.8 px/mm (fitted to the 40-pin POKEY and PIA) ----
mb_ic = {'A102': (773, 316, 'T'), 'A101': (1002, 349, 'T'), 'Z103': (745, 528, 'L'), 'Z104': (745, 612, 'L'), 'A104': (1005, 632, 'B'),
         'A103': (1087, 915, 'R'), 'Z102': (1364, 505, 'T'), 'Z101': (1364, 642, 'T'), 'Z106': (1364, 884, 'T'), 'Z105': (1364, 1018, 'T'),
         'Q101': (422, 204, 'T'), 'Q107': (746, 730, 'T'), 'Q105': (967, 801, 'T'), 'Q103': (1574, 151, 'T'), 'Q102': (1616, 105, 'T'), 'Q104': (1463, 1161, 'T')}
mb_two = {}
# resistor column (left, x=633) and capacitor columns
for i, (ref, y) in enumerate((('R102', 142), ('R175', 161), ('R138', 179), ('R139', 198), ('R140', 217), ('R141', 235), ('R130', 255), ('R131', 273), ('R142', 292),
        ('R143', 311), ('R144', 330), ('R145', 349), ('R176', 367), ('R133', 386), ('R132', 405), ('R110', 423), ('R114', 442), ('R111', 460), ('R112', 479),
        ('R113', 498), ('R117', 517), ('R115', 536), ('R118', 558), ('R116', 577), ('R121', 595), ('R120', 614), ('R119', 632), ('R122', 652), ('R123', 670),
        ('R125', 688), ('R126', 707), ('R124', 726), ('R178', 744), ('R149', 763), ('R148', 782), ('R147', 800), ('R146', 819), ('R135', 836), ('R134', 856),
        ('R177', 875), ('R150', 893), ('R151', 913), ('R152', 932))):
    mb_two[ref] = (633, y, 'h')
for ref, x, y in (('C104', 493, 194), ('C194', 493, 213), ('C143', 493, 232), ('C144', 493, 251), ('C145', 493, 269), ('C146', 493, 288), ('C135', 493, 308),
        ('C136', 493, 326), ('C123', 452, 354), ('C147', 452, 374), ('C148', 452, 395), ('C149', 452, 415), ('C150', 493, 425), ('C195', 493, 443),
        ('C139', 493, 461), ('C138', 493, 480), ('C113', 493, 499), ('C109', 493, 518), ('C112', 493, 537), ('C111', 493, 556), ('C110', 493, 575),
        ('C106', 493, 593), ('C108', 493, 612), ('C105', 493, 630), ('C107', 493, 648), ('C116', 493, 666), ('C115', 493, 686), ('C114', 493, 704),
        ('C117', 493, 723), ('C118', 493, 743), ('C120', 493, 761), ('C121', 493, 780), ('C119', 493, 799), ('C196', 493, 817), ('C156', 493, 836),
        ('C155', 493, 854), ('C154', 493, 872), ('C153', 493, 892), ('C140', 452, 919), ('C137', 452, 938), ('C151', 452, 958), ('C197', 452, 976),
        ('C157', 493, 992), ('C158', 493, 1010), ('C159', 493, 1028)):
    mb_two[ref] = (x, y, 'h')
for ref, y in (('C127', 246), ('C128', 276), ('C129', 307), ('C130', 337), ('C134', 366), ('C133', 396), ('C132', 426), ('C131', 458)):
    mb_two[ref] = (882, y, 'h')
mb_two.update({'R127': (880, 488, 'h'), 'R128': (996, 513, 'h'), 'C122': (956, 179, 'h'), 'R157': (1068, 299, 'v'), 'R167': (1097, 381, 'v'), 'C181': (1115, 380, 'v'),
    'R191': (1059, 550, 'h'), 'C172': (1060, 569, 'h'), 'R193': (1086, 588, 'h'), 'R192': (1086, 607, 'h'), 'R163': (1086, 626, 'h'), 'R164': (1086, 645, 'h'),
    'R165': (1086, 665, 'h'), 'C180': (1087, 682, 'h'), 'R166': (1086, 700, 'h'), 'C161': (1087, 719, 'h'), 'R199': (1087, 738, 'h'), 'R206': (1087, 757, 'h'),
    'R155': (1087, 775, 'h'), 'R156': (922, 604, 'h'), 'CR102': (922, 624, 'h'), 'C198': (922, 640, 'h'), 'R204': (922, 659, 'h'), 'R203': (922, 681, 'h'),
    'R201': (922, 698, 'h'), 'R202': (922, 718, 'h'), 'R188': (922, 737, 'h'), 'R200': (922, 755, 'h'), 'C171': (922, 772, 'h'),
    'R159': (840, 625, 'v'), 'L102': (796, 783, 'v'), 'C169': (833, 748, 'v'), 'R190': (862, 739, 'v'), 'R136': (709, 864, 'v'), 'R153': (729, 864, 'v'),
    'L106': (759, 862, 'v'), 'C179': (797, 882, 'v'), 'L101': (876, 822, 'v'), 'C173': (878, 880, 'h'), 'R162': (906, 862, 'v'), 'R195': (926, 862, 'v'),
    'R174': (945, 862, 'v'), 'R179': (964, 862, 'v'), 'R205': (982, 862, 'v'), 'C164': (978, 739, 'v'), 'C170': (999, 739, 'v'), 'R197': (1016, 739, 'v'),
    'R198': (1034, 739, 'v'), 'C163': (1001, 831, 'v'), 'R181': (1018, 834, 'v'), 'CR108': (1049, 831, 'v'), 'R183': (1068, 831, 'v'), 'R180': (1089, 831, 'v'),
    'R182': (1106, 831, 'v'), 'R154': (1125, 831, 'v'), 'C162': (1012, 922, 'v'), 'L110': (1072, 972, 'h'),
    'R184': (680, 940, 'v'), 'R187': (700, 945, 'v'), 'R186': (722, 945, 'v'), 'R185': (739, 945, 'v'), 'L108': (764, 945, 'v'), 'R168': (781, 945, 'v'),
    'R194': (801, 945, 'v'), 'L105': (819, 945, 'v'), 'R189': (838, 945, 'v'), 'L104': (856, 945, 'v'), 'R169': (874, 945, 'v'), 'R170': (893, 945, 'v'),
    'R173': (912, 945, 'v'), 'R171': (931, 945, 'v'), 'R172': (949, 945, 'v'), 'L109': (967, 945, 'v'), 'L103': (986, 945, 'v'), 'L107': (627, 962, 'h'),
    'C174': (767, 1114, 'h'), 'C175': (769, 1128, 'h'), 'C183': (824, 1102, 'v'), 'C184': (843, 1102, 'v'), 'C187': (860, 1102, 'v'), 'C185': (880, 1102, 'v'),
    'C177': (898, 1102, 'v'), 'C186': (917, 1102, 'v'), 'C188': (935, 1102, 'v'), 'C192': (954, 1102, 'v'), 'C193': (987, 1095, 'v'),
    'C152': (448, 1100, 'v'), 'C160': (467, 1100, 'v'), 'C142': (485, 1100, 'v'), 'C141': (502, 1100, 'v'), 'C190': (521, 1100, 'v'),
    'C165': (563, 1095, 'v'), 'C168': (581, 1095, 'v'), 'C167': (599, 1095, 'v'), 'C166': (618, 1095, 'v'), 'C189': (636, 1095, 'v'), 'C191': (657, 1095, 'v'),
    'C182': (675, 1095, 'v'), 'C178': (692, 1095, 'v'), 'C176': (711, 1095, 'v'),
    'R103': (1368, 746, 'h'), 'R109': (1368, 366, 'h'), 'R108': (1368, 393, 'h'), 'C101': (1368, 437, 'h'), 'C199': (1363, 188, 'v'), 'C125': (1368, 817, 'h'),
    'C124': (1368, 951, 'h'), 'R196': (1316, 1122, 'v'), 'R160': (1333, 1122, 'v'), 'R161': (1353, 1122, 'v'), 'CR104': (1371, 1122, 'v'),
    'CR105': (1390, 1122, 'v'), 'CR106': (1410, 1122, 'v'), 'CR107': (1428, 1122, 'v'), 'R106': (1498, 145, 'h'), 'R107': (1498, 162, 'h'),
    'C103': (1498, 127, 'h'), 'R105': (1796, 144, 'h'), 'X101': (1673, 137, 'h'), 'CR101': (1411, 130, 'h')})
for i, ref in enumerate('ABCDEF'):
    mb_two['R129' + ref] = (1071, 360 + 25 * i, 'h')
for i, ref in enumerate('ABCDEFGHI'):
    mb_two['R158' + ref] = (700, 305 + 15 * i, 'h')
# edge connectors and jacks: (x, y of the top, y of the bottom, pin 1 at the 'bottom' or 'top'); 'H' ones run along x: (x left, x right, y, ...)
mb_edge = {'J109': (1230, 315, 542, 'bottom'), 'J108': (1230, 783, 1010, 'bottom'), 'J107': (1455, 405, 917, 'bottom'), 'J104': (1559, 405, 917, 'bottom'),
           'J103': (1663, 405, 917, 'bottom'), 'J102': (1768, 405, 917, 'bottom'), 'J101': (1889, 307, 961, 'bottom'), 'J106': (418, 580, 840, 'top'),
           'J110': (355, 165, 335, 'top'), 'J111': (355, 395, 565, 'top'), 'J112': (355, 790, 955, 'top'), 'J113': (365, 1010, 1179, 'top')}
mb_hedge = {'J115': (673, 1011, 1159), 'J114': (588, 649, 1157)}
BOARDS['mb'] = dict(k=5.8, org=(300, 150), edge=None, IC=mb_ic, TWO=mb_two, EDGE2=mb_edge, HEDGE=mb_hedge)

def _dip(base, n, names, cx, cy, mode):
    pads, _ = base.pads_for('DIP-%d' % n, names, 0, 0)
    half = n // 2; wr = base.dip_geom(n)
    clx, cly = wr / 2, (half - 1) * 1.27
    if mode == 'R': f = lambda lx, ly: (cx - (ly - cly), cy + (lx - clx))
    elif mode == 'L': f = lambda lx, ly: (cx + (ly - cly), cy - (lx - clx))
    elif mode == 'B': f = lambda lx, ly: (cx - (lx - clx), cy - (ly - cly))
    else: f = lambda lx, ly: (cx + (lx - clx), cy + (ly - cly))
    return [(nm,) + f(lx, ly) + (w, h) for (nm, lx, ly, w, h) in pads]

def place(board, suffix, x0, y0, chips, names_of, base):
    """the pads of one board, its top left at (x0, y0) of the sheet; returns (pads by ref, width, height)"""
    B = BOARDS[board]
    k = B['k']; ox, oy = B['org']
    out = {}
    def mm(px, py): return (px - ox) / k, (py - oy) / k
    for ref, (px, py, mode) in B['IC'].items():
        r = ref + suffix
        if r not in chips: continue
        cx, cy = mm(px, py)
        out[r] = _dip(base, chips[r][2], names_of(r), cx, cy, mode)
    for ref, (px, py, d) in B['TWO'].items():
        r = ref + suffix
        if r not in chips: continue
        names = names_of(r); cx, cy = mm(px, py)
        span = 10.16 if chips[r][0] in ('R', 'L') else 5.08
        dx, dy = (span / 2, 0) if d == 'h' else (0, span / 2)
        out[r] = [(names[0], cx - dx, cy - dy, 1.6, 1.6), (names[1], cx + dx, cy + dy, 1.6, 1.6)]
    for ref, (ex, ey0, ey1, p1) in B.get('EDGE2', {}).items():
        if ref not in chips: continue
        names = names_of(ref); n = len(names); half = (n + 1) // 2
        cx, _ = mm(ex, 0)
        pts = []
        for i, nm in enumerate(names):
            f = (i // 2) / max(half - 1, 1)
            _, yy = mm(0, ey1 - (ey1 - ey0) * f if p1 == 'bottom' else ey0 + (ey1 - ey0) * f)
            pts.append((nm, cx + (i % 2) * 3.0 - 1.5, yy, 2.4, 1.8))
        out[ref] = pts
    for ref, (ex0, ex1, ey) in B.get('HEDGE', {}).items():
        if ref not in chips: continue
        names = names_of(ref); n = len(names); half = (n + 1) // 2
        _, cy = mm(0, ey)
        pts = []
        for i, nm in enumerate(names):
            xx, _ = mm(ex0 + (ex1 - ex0) * (i // 2) / max(half - 1, 1), 0)
            pts.append((nm, xx, cy + (i % 2) * 3.0 - 1.5, 1.8, 2.4))
        out[ref] = pts
    eref, ex, ey0, ey1 = B['edge'] or (None, 0, 0, 0)
    eref = (eref + suffix) if eref == 'RB' else eref
    if eref in chips:
        names = names_of(eref); n = len(names); half = (n + 1) // 2
        cx, _ = mm(ex, 0)
        pts = []
        for i, nm in enumerate(names):
            _, yy = mm(0, ey0 + (ey1 - ey0) * (i // 2) / (half - 1))
            pts.append((nm, cx + (i % 2) * 3.0 - 1.5, yy, 2.4, 1.8))
        out[eref] = pts
    xs = [p[1] for pts in out.values() for p in pts]; ys = [p[2] for pts in out.values() for p in pts]
    mx, my = min(xs) - 5, min(ys) - 5
    for r in out: out[r] = [(nm, x - mx + x0, y - my + y0, w, h) for (nm, x, y, w, h) in out[r]]
    return out, max(xs) - mx + 5, max(ys) - my + 5
