from netlib import N
N.sheet_is('02')
# IEEE-488 PIA UB16 (6520) at E820
N.chip('UB16','6520')
N.pins('UB16', {33:'D0',32:'D1',31:'D2',30:'D3',29:'D4',28:'D5',27:'D6',26:'D7'})
N.pins('UB16', {24:'BA5', 23:'SELE', 22:'X8XX', 36:'BA0', 35:'BA1', 21:'RW', 25:'BPHI2', 34:'RES'})
N.pins('UB16', {i+2:'DI%d'%(i+1) for i in range(8)})        # PA0-7 <- bus receivers
N.pins('UB16', {i+10:'DO%d'%(i+1) for i in range(8)})       # PB0-7 -> bus drivers
N.pins('UB16', {40:'ATN_IN', 39:'NDAC_OUT', 18:'SRQ_IN', 19:'DAV_OUT', 38:'IRQ', 37:'IRQ'})
# MC3446 bus transceivers
for ref in ('UA20','UB17','UC12'):
    N.chip(ref,'MC3446'); N.pins(ref, {4:'GND', 12:'GND'})
N.pins('UA20', {1:'DAV_IN',3:'DAV_OUT',15:'NRFD_IN',13:'NRFD_OUT',9:'NDAC_IN',11:'NDAC_OUT',7:'ATN_IN',5:'ATN_OUT',
                2:'DAV',14:'NRFD',10:'NDAC',6:'ATN'})
for ref, base in (('UB17',0),('UC12',4)):
    d = {}
    for k,(rx,tx,bus) in enumerate(((1,3,2),(15,13,14),(7,5,6),(9,11,10))):
        d[rx]='DI%d'%(base+k+1); d[tx]='DO%d'%(base+k+1); d[bus]='DIO%d'%(base+k+1)
    N.pins(ref, d)
# EOI and IFC open-collector drivers, UD15 (7417)
N.chip('UD15','7417')
N.pins('UD15', {11:'EOI_OUT', 10:'EOI', 13:'RES', 12:'IFC'})
for ref,a,b,net in (('R20','+5V','EOI','2.4K'),('R21','EOI','GND','5.1K'),('R18','+5V','IFC','2.4K'),('R19','IFC','GND','5.1K')):
    N.chip(ref,'RES'); N.pins(ref, {1:a, 2:b})
N.chip('R49','RES'); N.pins('R49', {1:'GND', 2:'CHASSIS'})
N.chip('J2','CONN'); N.pins('J2', {4:'EOI'})
# IEEE-488 edge connector J1/J12 (letters A..N mapped to 13..24 in order A B C D E F H J K L M N)
N.chip('J1','CONN')
N.pins('J1', {1:'DIO1',2:'DIO2',3:'DIO3',4:'DIO4',5:'EOI',6:'DAV',7:'NRFD',8:'NDAC',9:'IFC',10:'SRQ_IN',11:'ATN',12:'CHASSIS',
              13:'DIO5',14:'DIO6',15:'DIO7',16:'DIO8'})
N.pins('J1', {p:'GND' for p in range(17,25)})
# I/O select for VIA: CS1 = (X8XX & BA6) via UE13 gate 3 (74LS10) and UE15 inverter (74LS04)
N.chip('UE13','LS10'); N.chip('UE15','LS04')
N.pins('UE13', {10:'X8XX', 9:'X8XX', 11:'BA6', 8:'VIA_CS1_N'})
N.pins('UE15', {13:'VIA_CS1_N', 12:'CS1'})
N.note('Gate labelled UB3 on sheet 2 is UE13 (74LS10, third gate); UB3 is the 74LS373. PB/PA<->DO/DI pairing of PA3 and PB0-3 inferred from in-order pattern; CA1=ATN_IN, CA2=NDAC_OUT, CB1=SRQ_IN, CB2=DAV_OUT. R/W drawn as R/W here but BR/W on sheet 10: reconcile on sheet 1. EOI_IN net = EOI (tap to sheet 3).')
