"""pcbparts.py - which chip of a PCB is which model of the simulator (used by kicad_pcb2c.py).

partdefs(prefix, parts) gives the text of the machine's part table (src/<prefix>_parts.inc: one { ref, model, label, role,
note, help } line per chip that has a model) and the list of the ICs that have none."""
import re


def ddbit(part):
    """the data bit a 4164 stores: the DD<n> net its D and Q pins are on"""
    for name, _, _, _, _, net in part['pads']:
        if name in ('2', '14') and net:
            m = re.search(r'DD(\d)$', net)
            if m: return int(m.group(1))
    return None


def line(ref, model, label, role, note, text):
    n = 'NULL' if note is None else '"%s"' % note
    return '    { "%s", &%s, "%s", %s, %s, "%s" },\n' % (ref, model, label, role, n, text)


# chip values of the Tandy Color Computer 2 (26-3134B/3136B) and what models them
COCO2 = {
    'MC6809E': ('PT_6809E', '6809E', 'ROLE_CHIP', 'cpu', 'MC6809E CPU, clocked by the SAM (E and Q): 0.895 MHz'),
    'SN74LS785N': ('PT_SAM', 'SAM', 'ROLE_CHIP', 'sam', '74LS785 (6883) SAM: the memory map, the clocks and the RAM timing, the video address counter'),
    'MC6847T1': ('PT_6847T1', 'VDG', 'ROLE_CHIP', 'vdg', '6847T1 video chip: draws the picture, and is the transceiver between the CPU bus and the RAM bus'),
    '6821': ('PT_PET6520', '6821', 'ROLE_CHIP', 'pia1', '6821 PIA 1 ($FF20): sound, cassette, serial, the video modes'),
    'SC67331P': ('PT_PET6520', '6821', 'ROLE_CHIP', 'pia0', '6821 PIA 0 ($FF00): the keyboard and the joysticks, the 60 Hz interrupt'),
    '27C128': ('PT_27128', 'ROM', 'ROLE_ROM', 'ROM', '27C128 ROM: Color BASIC and Extended BASIC ($8000-$BFFF)'),
    '74LS138': ('PT_74LS138', '74LS138', 'ROLE_GLUE', None, '74LS138: the chip selects of the SAM\'s S0-S2 (RAM, ROMs, the PIAs, the cartridge)'),
    '74LS02': ('PT_74LS02', '74LS02', 'ROLE_GLUE', None, '74LS02: the ROM select and the decoder enable'),
}


def coco2(parts):
    out, missing = [], []
    for p in sorted(parts, key=lambda p: p['ref']):
        ref, val = p['ref'], p['value']
        if not ref.startswith('IC') and ref != 'X1': continue
        if ref == 'X1':
            out.append(line(ref, 'PT_XTAL_COCO', 'XTAL', 'ROLE_CLOCK', 'xtal', '14.31818 MHz crystal: the SAM divides it by 16 for the CPU'))
        elif val == 'KM4164':
            bit = ddbit(p)
            lab = 'RAM' if bit is None else 'RAM'
            out.append(line(ref, 'PT_4164', lab, 'ROLE_RAM', '4164\\n64K x 1\\nbit %s' % ('?' if bit is None else bit),
                            '4164 DRAM, 64K x 1: bit %s of the 64K' % ('?' if bit is None else bit)))
        elif val in COCO2:
            m = COCO2[val]
            out.append(line(ref, m[0], m[1], m[2], m[3], m[4]))
        else:
            missing.append('%s %s' % (ref, val))
    return ''.join(out), missing


# chip values of the Dragon 64 (issue 6 board) and what models them
DRAGON = {
    'MC6809E': ('PT_6809E', '6809E', 'ROLE_CHIP', 'cpu', 'MC6809E CPU, clocked by the SAM (E and Q): 0.89 MHz'),
    '6883': ('PT_SAM6883', 'SAM', 'ROLE_CHIP', 'sam', '6883 SAM: the memory map, the clocks and the RAM timing, the video address counter, VCLK for the 6847'),
    '6847': ('PT_6847', 'VDG', 'ROLE_CHIP', 'vdg', '6847 video chip: draws the picture from the bytes the RAM latch IC20 gives it'),
    '6551': ('PT_6551', '6551', 'ROLE_CHIP', None, '6551 ACIA: the serial port ($FF04)'),
    '27C128': ('PT_27128', 'ROM', 'ROLE_ROM', 'ROM', '27C128 ROM: BASIC (IC34 and IC37, 16K each, selected by /ROM0 and /ROM1)'),
    '74LS138': ('PT_74LS138', '74LS138', 'ROLE_GLUE', None, '74LS138: the chip selects of the SAM\'s S0-S2 (RAM, ROMs, the PIAs, the ACIA, the cartridge)'),
    '74LS02': ('PT_74LS02', '74LS02', 'ROLE_GLUE', None, '74LS02: the decoder enables'),
    '74LS00': ('PT_74LS00', '74LS00', 'ROLE_GLUE', None, '74LS00: the ROM selects, and the 6847 clock gating of the PAL timing'),
    '74LS32': ('PT_74LS32', '74LS32', 'ROLE_GLUE', None, '74LS32: the SAM\'s DA0 and the PAL line stretch counter decode'),
    '74LS86': ('PT_74LS86', '74LS86', 'ROLE_GLUE', None, '74LS86: the SAM\'s horizontal sync and the PAL timing'),
    '74LS20': ('PT_74LS20', '74LS20', 'ROLE_GLUE', None, '74LS20: the PAL line and field stretch decode'),
    '74LS74': ('PT_74LS74F', '74LS74', 'ROLE_GLUE', None, '74LS74: the frame sync latch, the horizontal sync latch and the PAL clock gate'),
    '74LS244': ('PT_74LS244', '74LS244', 'ROLE_GLUE', None, '74LS244: buffer (the RAM data to the CPU bus, the printer port)'),
    '74LS273': ('PT_74LS273', '74LS273', 'ROLE_GLUE', None, '74LS273: latches the RAM\'s data for the 6847 (clocked by /RAS)'),
    '74LS393': ('PT_74LS393', '74LS393', 'ROLE_GLUE', None, '74LS393: counters of the PAL timing'),
    '74LS390': ('PT_74LS390', '74LS390', 'ROLE_GLUE', None, '74LS390: counter of the PAL timing'),
    '4069': ('PT_74LS04', '4069', 'ROLE_GLUE', None, '4069: inverters (the PIAs\' address line A2 and the serial levels)'),
}


def dragon(parts):
    out, missing = [], []
    for p in sorted(parts, key=lambda p: p['ref']):
        ref, val = p['ref'], p['value']
        if not ref.startswith('IC') and ref != 'XL3': continue
        if ref == 'XL3':
            out.append(line(ref, 'PT_XTAL_COCO', 'XTAL', 'ROLE_CLOCK', 'xtal', '14.218 MHz crystal: the SAM divides it by 16 for the CPU'))
        elif val == '4164':
            out.append(line(ref, 'PT_4164', 'RAM', 'ROLE_RAM', '4164\\n64K x 1', '4164 DRAM, 64K x 1: one bit of the 64K'))
        elif val == '6821':
            pia = 'pia1' if ref == 'IC8' else 'pia0'
            out.append(line(ref, 'PT_PET6520', '6821', 'ROLE_CHIP', pia, '6821 PIA %s' % ('1 ($FF20): sound, cassette, ROM select, the video modes' if ref == 'IC8' else '0 ($FF00): the keyboard and the joysticks, the 50 Hz interrupt')))
        elif val in DRAGON:
            m = DRAGON[val]
            out.append(line(ref, m[0], m[1], m[2], m[3], m[4]))
        else:
            missing.append('%s %s' % (ref, val))
    return ''.join(out), missing


def partdefs(prefix, parts):
    head = '/* %s_parts.inc - GENERATED by tools/kicad_pcb2c.py (tools/pcbparts.py), do not edit: the chips and their models. */\n' % prefix
    if prefix == 'coco2':
        text, missing = coco2(parts)
        return head + text, missing
    if prefix == 'dragon64':
        text, missing = dragon(parts)
        return head + text, missing
    return head, []
