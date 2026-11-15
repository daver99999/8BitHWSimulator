from netlib import N
N.sheet_is('10')
N.chip('UB13','CRTC')
d = {33:'D0',32:'D1',31:'D2',30:'D3',29:'D4',28:'D5',27:'D6',26:'D7'}
ta = {i+4:'TA%d'%i for i in range(10)}
N.pins('UB13', d); N.pins('UB13', ta)
N.pins('UB13', {2:'RES', 3:'LPSTB', 14:'NC_TA10', 15:'NC_TA11', 16:'INVERT', 17:'CHR_OPTION', 18:'DISP_ENA', 19:'NC_CURSOR',
                21:'CLK1', 22:'BRW', 23:'BPHI2', 24:'BA0', 25:'CRTC_CS',
                34:'CRA4', 35:'CRA3', 36:'RRA2', 37:'RRA1', 38:'RRA0', 39:'HSYNC', 40:'VSYNC'})
# I/O inverter and pen strobe inverter (UD2, 74S04)
N.chip('UD2','S04')
N.pins('UD2', {13:'SELE', 12:'IO', 3:'PEN_STROBE', 4:'LPSTB'})
N.chip('R6','RES'); N.pins('R6', {1:'+5V', 2:'PEN_STROBE'})
# CRTC chip select = NAND(IO, X8XX, BA7)  (UE13, 74LS10)
N.chip('UE13','LS10')
N.pins('UE13', {1:'IO', 2:'X8XX', 13:'BA7', 12:'CRTC_CS'})
# NO ROW = NOR(RA3,RA4)  (UD1, 74LS02)
N.chip('UD1','LS02')
N.pins('UD1', {2:'CRA4', 3:'CRA3', 1:'NO_ROW'})
# Sync polarity buffers (UC2, 74LS86): jumpers 2 and 3 choose PULLUP (invert) or GND (pass)
N.chip('UC2','LS86')
N.pins('UC2', {5:'PULLUP1', 4:'VSYNC', 6:'VERT_DRIVE', 1:'HSYNC', 2:'GND', 3:'HORIZ_DRIVE'})  # jumpers 2,3 on ground side (user board: continuity to GND)
N.chip('R3','RES'); N.pins('R3', {1:'+5V', 2:'HORIZ_DRIVE'})
N.note('Jumper 2/3 fitted state unknown (PULLUP=invert / GND=pass); check user board. CRTC E pin drawn as BA2 - probably BO2 (phase 2) drafting error; model needs a clock there.')

N.note('S10: vertical sync jumper verified by meter: UC2 pin 5 on PULLUP1 (inverting); horizontal pin 2 on GND (non-inverting).')
