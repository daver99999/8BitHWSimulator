import json, math, re, sys, collections
exec(open('final2.py').read().split("print({k:v")[0])
U(271,216)   # the inverse video switch: XOR input 4 on the shift register's /QH (the drawn default)
# ---- explicit rails
GND_PINS={('IC18a',2),('IC9',10),('IC9',15),('IC6',5),('IC6',15),('IC7',15),('IC8',15),('IC5',1),('IC20',1),('IC21',10),('IC3',9),('IC4',9),('IC1',29),('IC2',12),('IC10',8)}
VCC_PINS={('IC18a',3),('IC18b',10),('IC18b',13),('IC19b',10),('IC19b',13),('IC19a',4),('IC20',9),('IC20',13),('IC21',5),('IC3',18),('IC4',18),('IC1',11),('IC2',24),('IC10',16),('IC9',16),('IC5',20)}
RAILEND={'R3':(270,'+5V'),'R12':(243,'+5V'),'R13':(414,'+5V'),'R14':(414,'+5V'),'R15':(414,'+5V'),'R16':(414,'+5V'),'R17':(414,'+5V'),
 'R18':(401,'+5V'),'R20':(411,'+5V'),'R21':(228,'+5V'),'R22':(228,'+5V'),'R23':(228,'+5V'),'R28':(404,'+5V'),
 'R1':(387,'GND'),'R2':(214,'GND'),'R25':(410,'GND'),'R34':(18,'GND')}
# ---- chips and their pin->net id
chips={}   # ref -> type
conn=collections.OrderedDict()   # (ref,pin) -> netid or rail string
def real(ic): return ic[:-1] if ic[-1] in 'ab' and ic[:2]=='IC' and ic[2:-1].isdigit() else ic
off={'IC18a':0,'IC18b':0,'IC19a':0,'IC19b':0}
TYPES={'IC1':'Z80','IC2':'ROM8332','IC3':'RAM2114','IC4':'RAM2114','IC5':'LS373','IC6':'LS157','IC7':'LS157','IC8':'LS157','IC9':'LS165','IC10':'LS365',
 'IC11':'LS00','IC12':'LS00','IC13':'LS04','IC14':'LS05','IC15':'LS05','IC16':'LS10','IC17':'LS32','IC18':'LS74','IC19':'LS74','IC20':'LS86','IC21':'LS93'}
for (ic,pin),g in pm.items():
    if g is None: continue
    r=real(ic); chips[r]=TYPES[r]
    if (ic,pin) in GND_PINS or (r,pin) in GND_PINS: conn[(r,pin)]='GND'
    elif (ic,pin) in VCC_PINS or (r,pin) in VCC_PINS: conn[(r,pin)]='+5V'
    else: conn[(r,pin)]=f(g)
for (ic,pin) in list(GND_PINS)+list(VCC_PINS):
    r=real(ic); chips[r]=TYPES[r]; conn[(r,pin)]='GND' if (ic,pin) in GND_PINS else '+5V'
# supply pins not seen: IC3.9 IC4.9 etc handled; gate ICs get power automatically
# two-terminal parts
parts2={}
for nm,gs in RT.items():
    a,b=gs
    if nm in RAILEND:
        gr,rail=RAILEND[nm]
        other=[x for x in gs if x!=gr][0]
        conn[(nm,1)]=rail; conn[(nm,2)]=f(other); chips[nm]='RES'
    else:
        conn[(nm,1)]=f(a); conn[(nm,2)]=f(b); chips[nm]='RES'
# capacitors / crystal
def cap(nm,ga,gb):
    chips[nm]='CAP'; conn[(nm,1)]=f(ga) if isinstance(ga,int) else ga; conn[(nm,2)]=f(gb) if isinstance(gb,int) else gb
cap('C9',215,380); cap('C10',227,'GND'); cap('C11',412,409); cap('C15',378,'GND')
chips['X1']='XTAL'; conn[('X1',1)]=f(378); conn[('X1',2)]=f(374)
# diodes D3..D10 (anode=row, cathode=bus), D11 (anode=D6 net, cathode=KBD)
DIO={'D3':(364,363),'D4':(362,290),'D5':(361,291),'D6':(360,292),'D7':(369,293),'D8':(370,294),'D9':(371,295),'D10':(372,296),'D11':(220,244)}
for d,(a,k) in DIO.items():
    chips[d]='DIODE'; conn[(d,1)]=f(a); conn[(d,2)]=f(k)
# keyboard connectors: KB1 columns D0..D4, KB2 rows A8..A15
chips['KB1']='CONN'; chips['KB2']='CONN'
for i,g in enumerate((382,383,384,385,386),1): conn[('KB1',i)]=f(g)
for i,d in enumerate(('D3','D4','D5','D6','D7','D8','D9','D10'),1): conn[('KB2',i)]=f(DIO[d][0])
# EAR input etc: C12 / R29: EAR jack chain; keep simple: EAR jack to IC10.2 through C12 and R29
# video: R30/R32 etc keep as RES with their groups (already in RT); VIDEO/SYNC nets
for nm in ('R29','R31','R33','R34','R35'):
    for k in [k for k in conn if k[0]==nm]: del conn[k]
    chips.pop(nm,None)
# ---- names
tapn={int(k):v for k,v in tap.items()}
name_by_root={}
name_fix={'D11':'KBDSEL'}
for gi,n in tapn.items():
    nn=n.replace("'",'P'); nn=name_fix.get(nn,nn); name_by_root[f(gi)]=nn
explicit={('IC2',8):'A0P',('IC2',7):'A1P',('IC2',6):'A2P',('IC2',5):'A3P',('IC2',4):'A4P',('IC2',3):'A5P',('IC2',2):'A6P',('IC2',1):'A7P',
 ('IC10',6):'KBD0',('IC10',10):'KBD1',('IC10',12):'KBD2',('IC10',4):'KBD3',('IC10',14):'KBD4',('D3',1):'ROW0',('D4',1):'ROW1',('D5',1):'ROW2',('D6',1):'ROW3',('D7',1):'ROW4',('D8',1):'ROW5',('D9',1):'ROW6',('D10',1):'ROW7',
 ('IC8',14):'LC2',('IC8',5):'LC1',('IC8',11):'LC0',('IC5',6):'LB0',('IC5',5):'LB5',('IC5',15):'LB1',('IC5',16):'LB2',('IC5',19):'LB3',('IC5',2):'LB4',('IC5',12):'LB7',('IC9',1):'SHLD',('IC1',7):'D4',('IC1',8):'D3',('IC1',9):'D5',('IC1',10):'D6',('IC1',12):'D2',('IC1',13):'D7',('IC1',14):'D0',('IC1',15):'D1',
 ('IC1',6):'CPUCLK',('IC1',17):'NMI',('IC1',16):'INT',('IC1',18):'HALT',('IC1',19):'MREQ',('IC1',20):'IORQ',('IC1',21):'RD',('IC1',22):'WR',
 ('IC1',27):'M1',('IC1',28):'RFSH',('IC1',26):'RESET',('IC1',25):'BUSRQ',('IC1',24):'WAIT',('IC13',2):'MREQP',('IC9',1):'SHLD',('IC5',11):'LE',
 ('IC5',9):'LATQ3',('IC20',6):'VIDEO',('IC19',5):'SYNC',('IC19',6):'SYNCN',('IC18',9):'PHI',('IC18',8):'PHIN',('IC20',3):'CLK2',
 ('IC11',2):'PHIP',('IC11',10):'VRST',('IC11',8):'VSET',('IC9',7):'SRQN',('IC9',9):'SRQ',('IC12',8):'RAMSEL',('IC3',8):'RAMCS',('IC12',11):'ROMSELN',
 ('IC6',7):'ROMCS',('IC17',11):'KBDSEL',('IC20',12):'XA',('IC20',8):'XB',('IC20',10):'XC',('IC15',1):'NOPC',('IC14',1):'NOPNODE',
 ('IC10',2):'EAR',('IC5',8):'NOPD',('IC16',8):'LOADN',('IC2',20):'ROMCS',('IC9',2):'SRCLK',('IC20',4):'VIDSEL',('IC12',3):'LATQ',
 ('IC12',6):'LATQN',('IC13',8):'LOADI',('IC11',5):'LOADD',('IC11',6):'LATSET',('IC11',3):'LATRST',('IC17',9):'NOPQ',('IC17',8):'NOPHI',
 ('IC15',10):'CPUCLK'}
used=set()
roots=collections.defaultdict(list)
for (ref,pin),v in conn.items():
    if isinstance(v,int): roots[v].append((ref,pin))
netname={}
for r,mem in roots.items():
    if r in name_by_root: netname[r]=name_by_root[r]; continue
    nm=None
    for (ref,pin) in mem:
        if (ref,pin) in explicit: nm=explicit[(ref,pin)]; break
    if nm is None:
        # 18a/19a mapped to 18/19 by real(): lookup with a/b suffix too
        pass
    if nm is None:
        ref,pin=sorted(mem)[0]; nm=f'N_{ref}_{pin}'
    netname[r]=nm
# duplicates names?
cnt=collections.Counter(netname.values()); dup=[n for n,c in cnt.items() if c>1]
print('dup names',dup)
out=[]
out.append('from netlib import N')
out.append("N.sheet_is('01')")
for ref in sorted(chips,key=lambda r:(re.sub(r'\d+','',r),int(re.sub(r'\D','',r) or 0))):
    t=chips[ref]
    out.append(f"N.chip('{ref}', '{t}')")
    pins={pin:(netname[v] if isinstance(v,int) else v) for (rf,pin),v in conn.items() if rf==ref}
    if pins: out.append(f"N.pins('{ref}', {json.dumps({str(k):v for k,v in sorted(pins.items())}).replace(chr(34),chr(39)).replace(chr(39)+'1'+chr(39)+':','1:')})")
import sys
sys.path.insert(0,'/home/claude/pethwsimulator/tools/zx80-netlist-src')
import pinlib
out2=[]
for ref in sorted(chips,key=lambda r:(re.sub(r'\d+','',r),int(re.sub(r'\D','',r) or 0))):
    t=chips[ref]
    out2.append("N.chip('%s', '%s')"%(ref,t))
    pins={pin:(netname[v] if isinstance(v,int) else v) for (rf,pin),v in conn.items() if rf==ref and not (t in pinlib.PWR and pin in pinlib.PWR[t])}
    if pins:
        items=', '.join("%d:'%s'"%(k,v) for k,v in sorted(pins.items()))
        out2.append("N.pins('%s', {%s})"%(ref,items))
open('sheet_body.py','w').write('\n'.join(out2))
print(len(chips),'chips',len(roots),'nets')
for r,mem in sorted(roots.items(),key=lambda kv:netname[kv[0]]): print(netname[r],':',' '.join(f'{a}.{b}' for a,b in sorted(mem)))
