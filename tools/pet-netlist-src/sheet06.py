from netlib import N
from config import COLS
N.sheet_is('06')
# ---------------- 16 MHz oscillator ----------------
N.chip('UE4','S04'); N.chip('UE5','LS00'); N.chip('UD2','S04')
N.pins('UE4', {1:'OSC_A', 2:'OSC_B', 3:'OSC_B', 4:'OSC_C', 5:'OSC_C', 6:'UE4_6'})
N.chip('C54','CAP'); N.chip('Y1','XTAL'); N.chip('R30','RES'); N.chip('R29','RES'); N.chip('R31','RES')
N.pins('C54', {1:'OSC_A', 2:'OSC_X1'}); N.pins('Y1', {1:'OSC_X1', 2:'OSC_C'})
N.pins('R30', {1:'OSC_A', 2:'OSC_B'});  N.pins('R29', {1:'OSC_B', 2:'OSC_C'})
N.pins('R31', {1:'+5V', 2:'R31_NODE'})
N.pins('UE5', {4:'R31_NODE', 5:'UE4_6', 6:'16MHZ'})
# ---------------- dot clock counter UD3 (74177): CKA=16MHz, QA->CKB ----------------
N.chip('UD3','74177')
N.pins('UD3', {1:'PULLUP1', 8:'16MHZ', 6:'CLK8', 5:'CLK8', 9:'CLK4', 2:'CLK2', 12:'CLK1', 13:'INIT'})
# 40C/80C jumpers (J1,J2 and J3,J4): board set to 40C
if COLS==40:   # J1 closed: CLK8 straight to output, J2 open
    N.pins('UD2', {11:'CLK8', 10:'CLK16_8'})
else:          # J2 closed: /16MHz via UD2, J1 open
    N.pins('UD2', {11:'16MHZ', 10:'CLK16_8'})
N.chip('UD4','LS10')
N.pins('UD4', {4:('CLK1_N' if COLS==40 else 'CLK8'), 5:'CLK4', 3:'CLK2', 6:'LOAD_SR'})   # J3 (40C) / J4 (80C)
# CLK1 inverters
N.pins('UE4', {11:'CLK1', 10:'CLK1_N', 9:'CLK1_N', 8:'CLK1B'})
# ---------------- 8 phases ----------------
N.chip('UE3','LS164')
N.pins('UE3', {1:'CLK1', 2:'+5V', 8:'16MHZ', 9:'INIT', 3:'B02A', 4:'B02B', 5:'B02C', 6:'B02D', 11:'B02F', 12:'B02G'})
N.pins('UD2', {5:'B02B', 6:'B02B_N', 9:'B02G', 8:'B02G_N'})
# ---------------- phase mux UE2 (74LS157): S=BRW, G=RAM_ON ----------------
N.chip('UE2','LS157')
N.pins('UE2', {1:'BRW', 15:'RAM_ON',
               2:'B02B', 3:'B02A', 11:'B02C', 10:'B02B', 14:'B02D', 13:'B02C',
               4:'FF1_CK', 9:'MUXA', 12:'CASTIM'})
# ---------------- RAS flip-flops UE1 (74S74) and RAS0 gate ----------------
N.chip('UE1','S74')
N.pins('UE1', {2:'INIT', 4:'INIT', 3:'FF1_CK', 5:'FF1_Q', 1:'B02G_N',
               12:'INIT', 10:'PULLUP1', 11:'B02B_N', 9:'FF2_Q', 13:'B02F'})
N.chip('UD1','LS02')
N.pins('UD1', {9:'FF1_Q', 8:'FF2_Q', 10:'RAS0_D'})
# memory size jumpers: 32K = W and Z fitted -> both inputs of gate 2 low
N.pins('UD1', {5:'GND', 6:'GND', 4:'UD1_4'})
N.pins('UD4', {9:'UD1_4', 10:'BA15_N', 11:'BPHI2', 8:'RAM_ON'})
# ---------------- CAS gates UD5 (74LS00); RAM-type jumper: 4116 -> BA14 ----------------
N.pins('UD5', {4:'BA14', 5:'BA14', 6:'BA14_N', 12:'CASTIM', 13:'BA14', 11:'CAS1_D', 9:'BA14_N', 10:'CASTIM', 8:'CAS0_D'})
for ref,val,a,b in (('R7','39','RAS0_D','RAS0'),('R11','27','CAS1_D','CAS1'),('R10','27','CAS0_D','CAS0')):
    N.chip(ref,'RES'); N.pins(ref, {1:a, 2:b})
# ---------------- refresh address counter UE6/UE7 ----------------
N.chip('UE6','74177'); N.chip('UE7','74177')
N.pins('UE6', {8:'CLK1_N', 6:'RA1', 5:'RA1', 9:'RA2', 2:'RA3', 12:'RA4', 13:'INIT', 1:'PULLUP1'})
N.pins('UE7', {8:'RA4', 6:'RA5', 5:'RA5', 9:'RA6', 2:'RA7', 13:'INIT', 1:'PULLUP1'})
N.chip('R28','RES'); N.pins('R28', {1:'+5V', 2:'INIT'})
N.note('Jumpers: 40C (J1,J3 closed; 80C J2,J4 open), memory 32K (W,Z closed), RAM type 4116 (J closed, H open) all ASSUMED for a 4032 - verify on board. UE3.A=CLK1 and UD3 clocking read by geometry. INIT driver is on sheet 8.')
