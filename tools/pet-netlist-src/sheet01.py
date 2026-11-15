from netlib import N
N.sheet_is('01')
# ---- RAM ON / BA15 / ROM9 gating ----
N.chip('UE11','LS00'); N.chip('UE13','LS10'); N.chip('UE14','7425'); N.chip('UE15','LS04')
N.pins('UE11', {10:'RAM_ON', 9:'BA15_N', 8:'UE11_8',        # gate A
                4:'BA15', 5:'BA15', 6:'BA15_N',             # gate B (inverter)
                12:'TP7_NODE', 13:'RD_EN', 11:'WR_EN',      # gate C: WR_EN = not RD_EN
                1:'BRW', 2:'UE13_6', 3:'RD_EN'})              # gate D: RD_EN = NAND(R/W, UE13_6): the read buffer is on for reads of the selected windows only (pin 1 runs down to the R/W line; the SEL8 bar goes to UE13 pin 3)
N.pins('UE13', {3:'SEL8', 4:'UE11_8', 5:'ROM9_NODE', 6:'UE13_6'})
N.chip('R42','RES'); N.pins('R42', {1:'+5V', 2:'ROM9_NODE'})   # jumper M (ROM9_NODE<->SEL9) open
N.chip('R39','RES'); N.pins('R39', {1:'+5V', 2:'TP7_NODE'})
# ---- X8XX decode (E8xx window) ----
N.chip('R45','RES'); N.pins('R45', {1:'+5V', 2:'UE14_STB2'})
N.pins('UE14', {13:'BA8', 12:'BA9', 9:'BA10', 10:'BA11_N', 11:'UE14_STB2', 8:'X8XX'})
N.pins('UE15', {3:'BA11', 4:'BA11_N'})
# ---- buffered data bus: UB10 = bits 0-3, UB9 = bits 4-7 (74LS244, bidirectional) ----
for ref, bits in (('UB10',(0,1,2,3)), ('UB9',(4,5,6,7))):
    N.chip(ref,'LS244')
    N.pins(ref, {19:'RD_EN', 1:'WR_EN'})
    # group 1 (1A->1Y) = CPU->BD (write); group 2 (2A->2Y) = BD->CPU (read)
    for k,b in enumerate(bits):
        a1 = 2 + 2*k; y2 = 3 + 2*k            # CPU-side pins (2/3, 4/5, 6/7, 8/9)
        y1 = 18 - 2*k; a2 = 17 - 2*k          # BD-side pins (18/17, 16/15, 14/13, 12/11)
        N.pins(ref, {a1:'D%d'%b, y2:'D%d'%b, y1:'BD%d'%b, a2:'BD%d'%b})
# ---- address decoder UE12 (74154): A..D = BA12..BA15, G1 = G2 = GND ----
N.chip('UE12','74154')
N.pins('UE12', {23:'BA12', 22:'BA13', 21:'BA14', 20:'BA15', 18:'GND', 19:'GND'})
outs = {5:'SEL4',6:'SEL5',7:'SEL6',8:'SEL7',9:'SEL8',10:'SEL9',11:'SELA',13:'SELB',14:'SELC',15:'SELD',16:'SELE',17:'SELF'}
N.pins('UE12', outs)
N.note('UE12 G1 (pin 18) wired to GND with G2; the line from the connector NO ROM pin did not clearly reach G1 - verify. Jumper M (ROM9) assumed open. /IO on sheets 2,3,10 = SELE.')

# ================= CPU and address buffers =================
N.chip('UB14','6502')
cpu = {2:'RDY', 4:'IRQ', 6:'NMI', 7:'SYNC', 34:'RW', 37:'CLK1', 38:'SO', 39:'CPU_PHI2', 40:'RES'}
cpu.update({9+i:'AB%d'%i for i in range(12)})            # A0-A11 on pins 9-20
cpu.update({22+i:'AB%d'%(12+i) for i in range(4)})       # A12-A15 on pins 22-25
cpu.update({33-i:'D%d'%i for i in range(8)})             # D0 on pin 33 ... D7 on pin 26
N.pins('UB14', cpu)
# pull-ups on CPU control inputs
for ref, val, net in (('R15','1K','RES'),('R43','1K','NMI'),('R12','1K','RDY'),('R1','1K','SO'),('R14','3.3K','IRQ')):
    N.chip(ref,'RES'); N.pins(ref, {1:'+5V', 2:net})
# address buffers UD13 (AB0-7), UD14 (AB8-15), always enabled
N.chip('UD13','LS244'); N.chip('UD14','LS244')
N.pins('UD13', {1:'GND', 19:'GND'}); N.pins('UD14', {1:'GND', 19:'GND'})
INP = [17,15,13,11,8,6,4,2]; OUTP = [3,5,7,9,12,14,16,18]
N.pins('UD13', {INP[i]:'AB%d'%i for i in range(8)});  N.pins('UD13', {OUTP[i]:'BA%d'%i for i in range(8)})
INP2 = [8,6,4,2,11,13,15,17]; OUTP2 = [12,14,16,18,9,7,5,3]
N.pins('UD14', {INP2[i]:'AB%d'%(8+i) for i in range(8)}); N.pins('UD14', {OUTP2[i]:'BA%d'%(8+i) for i in range(8)})
# ================= clock and R/W drivers (UD15 7417, UE15 74LS04) =================
N.pins('UD15', {1:'CPU_PHI2', 2:'BPHI2', 3:'BPHI2', 4:'CPHI2', 9:'RW', 8:'BRW'})
for ref, net in (('R13','BPHI2'),('R16','CPHI2'),('R17','BRW')):
    N.chip(ref,'RES'); N.pins(ref, {1:'+5V', 2:net})
N.pins('UE15', {11:'BRW', 10:'BRW_N', 9:'BRW_N', 8:'UE15_8'})
N.chip('R46','RES'); N.pins('R46', {1:'UE15_8', 2:'RAM_RW'})
# ================= power-up reset: LM555 -> UE15 -> UD15 -> RES =================
N.chip('UD16','LM555')
N.pins('UD16', {1:'GND', 2:'C50_NODE', 3:'RST555', 4:'+5V', 5:'CTRL555', 6:'C51_NODE', 7:'C51_NODE', 8:'+5V'})
for ref, a, b in (('R24','+5V','C50_NODE'),('R25','+5V','C51_NODE'),('C50','C50_NODE','GND'),('C51','C51_NODE','GND'),('C52','CTRL555','GND')):
    N.chip(ref, 'CAP' if ref.startswith('C') else 'RES'); N.pins(ref, {1:a, 2:b})
N.pins('UE15', {1:'RST555', 2:'RST_N'})
N.pins('UD15', {5:'RST_N', 6:'RES'})
# ================= edge connectors =================
N.chip('J4','CONN'); N.chip('J9','CONN')
j4 = {2+i:'BD%d'%i for i in range(8)}
j4.update({12:'SEL4',13:'SEL5',14:'SEL6',15:'SEL7',16:'SEL8',17:'SEL9',18:'SELA',19:'SELB',20:'NOROM',22:'RES',23:'RDY',24:'NMI'})
N.pins('J4', j4)
j9 = {2+i:'BA%d'%i for i in range(16)}
j9.update({18:'SYNC',19:'IRQ',21:'CPHI2',22:'BRW',23:'BRW_N'})
N.pins('J9', j9)
N.note('6502 pin 35 jumper pad (16MHz) left open; phi1 (pin 3) unconnected. UB14.5 NC. Sheet 10 CRTC E pin set to BPHI2 (BA2 label cannot be right: BA2 tag list omits sheet 10).')
