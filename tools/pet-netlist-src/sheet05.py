from netlib import N
N.sheet_is('05')
FA = ['FA%d'%i for i in range(7)]
OUT = [18,16,14,12,3,5,7]             # 74LS244 output pins in the order FA0..FA6
# UE8 refresh address buffer: inputs RA1..RA7 on pins 2,4,6,8,17,15,13
N.chip('UE8','LS244')
IN = [2,4,6,8,17,15,13]
N.pins('UE8', {p:'RA%d'%(i+1) for i,p in enumerate(IN)})
N.pins('UE8', {o:FA[i] for i,o in enumerate(OUT)})
N.pins('UE8', {1:'CLK1B', 19:'CLK1B'})
# UE9 row address (RAS phase): BA0..BA4, BA5 (via RAM-type jumper), BA6
N.chip('UE9','LS244')
rowbits = ['BA0','BA1','BA2','BA3','BA4','BA5','BA6']
N.pins('UE9', {p:rowbits[i] for i,p in enumerate(IN)})
N.pins('UE9', {o:FA[i] for i,o in enumerate(OUT)})
N.pins('UE9', {1:'RAS_EN', 19:'RAS_EN'})
# UE10 column address (CAS phase): BA13 (via jumper), BA7..BA12
N.chip('UE10','LS244')
colbits = ['BA13','BA7','BA8','BA9','BA10','BA11','BA12']
N.pins('UE10', {p:colbits[i] for i,p in enumerate(IN)})
N.pins('UE10', {o:FA[i] for i,o in enumerate(OUT)})
N.pins('UE10', {1:'CAS_EN', 19:'CAS_EN'})
# enables: UE5 gates (74LS00) and UE4 inverter (74S04)
N.chip('UE5','LS00'); N.chip('UE4','S04')
N.pins('UE5', {9:'CLK1B', 10:'MUXA_N', 8:'RAS_EN',  12:'MUXA', 13:'CLK1B', 11:'CAS_EN'})
N.pins('UE4', {13:'MUXA', 12:'MUXA_N'})
# 16 x 4116: even refs = upper block (RAS0/CAS1), odd = lower block (RAS0/CAS0)
ADDRP = {5:'FA0',7:'FA1',6:'FA2',12:'FA3',11:'FA4',10:'FA5',13:'FA6'}
bit = 7
for n in range(4, 20, 2):
    for ref, cas in (('UA%d'%n,'CAS1'), ('UA%d'%(n+1),'CAS0')):
        N.chip(ref,'4116')
        N.pins(ref, ADDRP)
        N.pins(ref, {3:'RAM_RW', 4:'RAS0', 15:cas, 2:'D%d'%bit, 14:'D%d'%bit})
    bit -= 1
N.note('FA lines have 68R series resistors R32-R38 on the board; electrically transparent, omitted. UA4/5=D7 .. UA18/19=D0. UE9.15=BA5 and UE10.2=BA13 inferred from the 4116 14-bit address need (RAM-type jumper block not traced). Both banks share RAS0.')
