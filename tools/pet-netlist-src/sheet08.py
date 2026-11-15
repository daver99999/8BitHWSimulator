from netlib import N
from config import COLS
N.sheet_is('08')
# ---- video shift register + character ROM ----
N.chip('UA3','2316'); N.chip('UA2','74166')
N.pins('UA3', {8:'RRA0',7:'RRA1',6:'RRA2', 5:'LSD0',4:'LSD1',3:'LSD2',2:'LSD3',1:'LSD4',23:'LSD5',22:'LSD6',
               19:'GRAPHIC',18:'CHR_OPTION',20:'GND',21:'INIT',
               9:'CD0',10:'CD1',11:'CD2',13:'CD3',14:'CD4',15:'CD5',16:'CD6',17:'CD7'})
N.pins('UA2', {15:'LOAD_SR',7:'CLK16_8',6:'GND',1:'GND',
               2:'CD0',3:'CD1',4:'CD2',5:'CD3',10:'CD4',11:'CD5',12:'CD6',14:'CD7',13:'SR_OUT'})
# ---- display-enable delay and video output flops ----
N.chip('UB1','LS74'); N.chip('UB2','LS74'); N.chip('UC1','S74'); N.chip('UC2','LS86')
N.chip('UD4','LS10'); N.chip('UD1','LS02'); N.chip('UD2','S04'); N.chip('R2','RES'); N.chip('R5','RES')
N.pins('UB1', {1:'INIT',2:'DISP_ENA',3:'CLK1B',4:'PULLUP1',5:'DISP_D1',
               13:'INIT',12:'DISP_D1',11:'LOAD_SR',10:'PULLUP1',9:'DISP_D2'})
N.pins('UD2', {1:'LOAD_SR',2:'LOAD_SR_N'})
N.pins('UB2', {1:'INIT',2:'LSD7',3:'LOAD_SR_N',4:'PULLUP1',5:'LSD7_D1',
               13:'INIT',12:'LSD7_D1',11:'CLK16_8',10:'PULLUP1',9:'LSD7_D2'})
N.pins('R2', {1:'+5V',2:'PULLUP1'})
N.pins('UC2', {13:'LSD7_D2',12:'SR_OUT',11:'VID_X'})
N.pins('UC1', {11:'CLK16_8',10:'PULLUP1',12:'VID_X',13:'INIT',9:'UC1_Q_A',
               3:'CLK16_8',4:'PULLUP1',2:'DISP_D2',1:'INIT',5:'UC1_Q_B'})
N.pins('UD4', {1:'UC1_Q_A',2:'NO_ROW',13:'UC1_Q_B',12:'VID_NAND'})
N.pins('UC2', {9:'VID_NAND',10:'INVERT',8:'VIDEO'})
N.pins('R5', {1:'VIDEO',2:'+5V'})
N.pins('UD1', {12:'LOAD_SR',11:'CLK1B',13:'VIDEO_LATCH'})
# ---- even display latch / buffers / RAM ----
N.chip('UB3','LS373'); N.chip('UB4','LS244'); N.chip('UB5','LS244'); N.chip('UC3','LS138')
N.chip('UC4','2114'); N.chip('UC5','2114')
# 40C jumper position: OC grounded (80C would use CLK1)
N.pins('UB3', {1:('GND' if COLS==40 else 'CLK1_N'),11:'VIDEO_LATCH',
               3:'ESD0',4:'ESD1',7:'ESD2',8:'ESD3',18:'ESD4',17:'ESD5',14:'ESD6',13:'ESD7',
               2:'LSD0',5:'LSD1',6:'LSD2',9:'LSD3',19:'LSD4',16:'LSD5',15:'LSD6',12:'LSD7'})
# 244: 1G(pin1)=READ_EVEN, ESD->BD via 1A(2,4,6,8)->1Y(18,16,14,12); 2G(pin19)=WRITE_EVEN, BD->ESD via 2A(17,15,13,11)->2Y(3,5,7,9)
for ref,esd,bd in (('UB4',(0,1,2,3),(0,1,2,3)),('UB5',(4,5,6,7),(4,5,6,7))):
    N.pins(ref, {1:'READ_EVEN',19:'WRITE_EVEN'})
    for k,(ipin,ypin,ain,yout) in enumerate(((2,18,17,3),(4,16,15,5),(6,14,13,7),(8,12,11,9))):
        N.pins(ref, {ipin:'ESD%d'%esd[k], yout:'ESD%d'%esd[k], ain:'BD%d'%bd[k], ypin:'BD%d'%bd[k]})
# 40C: A input of 138 grounded (80C would take BA0)
N.pins('UC3', {6:'CLK1B',4:'BA11',5:'SEL8',3:'BPHI2',2:'BRW',1:('GND' if COLS==40 else 'BA0'),
               11:'WRITE_EVEN',10:'WRITE_ODD',9:'READ_EVEN',7:'READ_ODD'})
SA=['SA%d'%i for i in range(10)]
apins=[5,6,7,4,3,2,1,17,16,15]
for ref,esd in (('UC4',('ESD0','ESD1','ESD2','ESD3')),('UC5',('ESD4','ESD5','ESD6','ESD7'))):
    d={p:s for p,s in zip(apins,SA)}
    d.update({10:'WRITE_EVEN',8:'GND',14:esd[0],13:esd[1],12:esd[2],11:esd[3]})
    N.pins(ref,d)
N.note('S8: R2 1K is the only source of PULLUP1 (+5V pull-up, shared with UB2.10); no other driver exists on any sheet. UA2 /CLR pin 9 verified unconnected on real board (no continuity to +5V, INIT or GND); floating TTL input reads high.')
N.note('S8: UA3 CS3 (pin 21) joins the INIT vertical; CS1 grounded, CS2=CHR_OPTION (2316 mask-programmed polarity).')
N.note('S8: J5/J6 (UB3 OC) and J7/J8 (UC3 A) follow config.COLS: 40C OC=GND,A=GND; 80C OC=CLK1,A=BA0 (user board: pins not grounded = 80C).')
N.note('S8: VIDEO_LATCH is UD1 pin 13 (LS02 gate 4); drawn "COPY LATCH" annotation only.')
