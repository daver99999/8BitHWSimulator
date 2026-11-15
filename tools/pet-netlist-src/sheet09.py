# Sheet 9: ODD display RAMs (80-column). Fitted on user's board (UC6/UC7 seen in photo); import only when COLS==80.
from netlib import N
N.sheet_is('09')
N.chip('UB8','LS373'); N.chip('UB6','LS244'); N.chip('UB7','LS244'); N.chip('UC6','2114'); N.chip('UC7','2114')
N.pins('UB8', {1:'CLK1B',11:'VIDEO_LATCH',
               3:'OSD0',4:'OSD1',7:'OSD2',8:'OSD3',18:'OSD4',17:'OSD5',14:'OSD6',13:'OSD7',
               2:'LSD0',5:'LSD1',6:'LSD2',9:'LSD3',19:'LSD4',16:'LSD5',15:'LSD6',12:'LSD7'})
for ref,n0 in (('UB6',0),('UB7',4)):
    N.pins(ref, {1:'READ_ODD',19:'WRITE_ODD'})
    for k,(ipin,ypin,ain,yout) in enumerate(((2,18,17,3),(4,16,15,5),(6,14,13,7),(8,12,11,9))):
        N.pins(ref, {ipin:'OSD%d'%(n0+k), yout:'OSD%d'%(n0+k), ain:'BD%d'%(n0+k), ypin:'BD%d'%(n0+k)})
apins=[5,6,7,4,3,2,1,17,16,15]
for ref,osd in (('UC6',('OSD0','OSD1','OSD2','OSD3')),('UC7',('OSD4','OSD5','OSD6','OSD7'))):
    d={p:'SA%d'%i for i,p in enumerate(apins)}
    d.update({10:'WRITE_ODD',8:'GND',14:osd[0],13:osd[1],12:osd[2],11:osd[3]})
    N.pins(ref,d)
N.note('S9: odd display set is 80-col only; BOM marks UB6/UB7/UB8/UC6/UC7 not fitted on -03. Its latch OC=CLK1B would fight the even latch (OC grounded in 40C) if both were fitted.')
