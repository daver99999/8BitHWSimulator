from netlib import N
N.sheet_is('03')
LET = dict(zip('ABCDEFHJKLMN', range(101,113)))     # connector letter pins -> 101..112
D = {33-i:'D%d'%i for i in range(8)}
# ---------------- VIA UB15 (6522) at E840 ----------------
N.chip('UB15','6522')
N.pins('UB15', D)
N.pins('UB15', {24:'CS1', 23:'SELE', 38:'BA0', 37:'BA1', 36:'BA2', 35:'BA3', 22:'RW', 25:'BPHI2', 34:'RES', 21:'IRQ',
                40:'UP_CA1', 39:'GRAPHIC'})
N.pins('UB15', {2+i:'UP_PA%d'%i for i in range(8)})
N.pins('UB15', {10:'NDAC_IN', 11:'NRFD_OUT', 12:'ATN_OUT', 13:'CASS_WRITE', 14:'CASS_MOTOR2_CTL', 15:'VERT_DRIVE',
                16:'NRFD_IN', 17:'DAV_IN', 18:'CASS_READ2', 19:'CB2'})
# ---------------- PIA UB12 (6520) at E810: keyboard / cassette 1 ----------------
N.chip('UB12','6520')
N.pins('UB12', D)
N.pins('UB12', {24:'BA4', 23:'SELE', 22:'X8XX', 36:'BA0', 35:'BA1', 21:'RW', 25:'BPHI2', 34:'RES', 38:'IRQ', 37:'IRQ',
                40:'CASS_READ1', 39:'EOI_OUT', 18:'VERT_DRIVE', 19:'CASS_MOTOR1_CTL'})
N.pins('UB12', {2:'KEY_A', 3:'KEY_B', 4:'KEY_C', 5:'KEY_D', 6:'CASS_SW1', 7:'CASS_SW2', 8:'EOI_IN', 9:'DIAG'})
N.pins('UB12', {10+i:'KIN%d'%i for i in range(8)})
# ---------------- keyboard row decoder UC11 (74LS145) ----------------
N.chip('UC11','LS145')
N.pins('UC11', {15:'KEY_A', 14:'KEY_B', 13:'KEY_C', 12:'KEY_D'})
for n, pin in enumerate((1,2,3,4,5,6,7,9,10,11)):
    N.pins('UC11', {pin:'KROW%d'%n})
# key-in pull-ups UB11 (7 x 10K network, common on pin 14)
N.chip('UB11','RNET14')
N.pins('UB11', {14:'+5V'})
N.pins('UB11', {13:'KIN0', 12:'KIN1', 11:'KIN2', 10:'KIN3', 9:'KIN4', 8:'KIN5', 6:'KIN6', 7:'KIN7'})
# ---------------- keyboard connector J8 ----------------
N.chip('J8','CONN')
N.pins('J8', {n+1:'KROW%d'%n for n in range(10)})
N.pins('J8', {LET[c]:'KIN%d'%i for i,c in enumerate('ABCDEFHJ')})
N.pins('J8', {12:'GND'})      # pin 12 is grounded on the schematic
# ---------------- user port J2 ----------------
N.chip('J2','CONN')
N.pins('J2', {LET['B']:'UP_CA1'})
N.pins('J2', {LET[c]:'UP_PA%d'%i for i,c in enumerate('CDEFHJKL')})
N.pins('J2', {3:'SRQ_IN', 2:'VIDEO', 10:'HORIZ_DRIVE', 9:'VERT_DRIVE', 7:'CASS_WRITE', 5:'DIAG',
              11:'GRAPHIC', 12:'GND', LET['M']:'CB2', 8:'CASS_READ1', 6:'CASS_READ2', LET['A']:'GND', LET['N']:'GND', 1:'GND'})
# ---------------- cassette connectors ----------------
N.chip('J6','CONN'); N.chip('J3','CONN')
N.pins('J6', {1:'GND', 2:'+5V', 4:'CASS_READ2', 3:'CASS_MOTOR2', 5:'CASS_WRITE', 6:'CASS_SW2'})
N.pins('J3', {1:'GND', 2:'+5V', 5:'CASS_WRITE', 3:'CASS_MOTOR1', 4:'CASS_READ1', 6:'CASS_SW1'})
# J9 pin 20 = DIAG (J9 declared on sheet 1)
N.pins('J9', {20:'DIAG'})
# ---------------- sound ----------------
N.chip('UD5','LS00'); N.chip('R8','RES'); N.chip('E1','RES')
N.pins('UD5', {1:'CB2', 2:'CB2', 3:'SPKR_DRV'})
N.pins('R8', {1:'SPKR_DRV', 2:'SPKR'}); N.pins('E1', {1:'SPKR', 2:'GND'})
# ---------------- cassette motor drivers (analog) ----------------
N.chip('UE16','Q2T2222'); N.chip('Q1','Q3'); N.chip('Q2','Q3')
for ref, typ in (('R22','RES'),('R23','RES'),('R26','RES'),('R27','RES'),('R47','RES'),('R48','RES'),
                 ('VR5','RES'),('VR6','RES'),('C60','CAP'),('C61','CAP')):
    N.chip(ref, typ)
# upper pair -> #2 motor (Q2)
N.pins('R22', {1:'CASS_MOTOR2_CTL', 2:'UE16_B1'}); N.pins('R23', {1:'UE16_B1', 2:'GND'})
N.pins('UE16', {2:'UE16_B1', 3:'GND', 1:'UE16_N1', 6:'UE16_N1', 7:'+9V_UNREG', 5:'Q2_BASE'})
N.pins('R47', {1:'UE16_N1', 2:'+9V_UNREG'}); N.pins('VR5', {1:'UE16_N1', 2:'GND'}); N.pins('C60', {1:'UE16_N1', 2:'+9V_UNREG'})
N.pins('Q2', {1:'Q2_BASE', 2:'+9V_UNREG', 3:'CASS_MOTOR2'})
# lower pair -> #1 motor (Q1)
N.pins('R26', {1:'CASS_MOTOR1_CTL', 2:'UE16_B2'}); N.pins('R27', {1:'UE16_B2', 2:'GND'})
N.pins('UE16', {13:'UE16_B2', 12:'GND', 14:'UE16_N2', 9:'UE16_N2', 8:'+9V_UNREG', 10:'Q1_BASE'})
N.pins('R48', {1:'UE16_N2', 2:'+9V_UNREG'}); N.pins('VR6', {1:'UE16_N2', 2:'GND'}); N.pins('C61', {1:'UE16_N2', 2:'+9V_UNREG'})
N.pins('Q1', {1:'Q1_BASE', 2:'+9V_UNREG', 3:'CASS_MOTOR1'})
N.note('Analog cassette stage entered as passive parts without pin-level checks. Uncertain: VIA PB4 -> #2 motor and PIA CB2 -> #1 motor (follows real-PET assignment, geometry unclear); VIA CB1 = cassette #2 read; keyboard connector pin 12 unused; KIN0-7 in picture order on UB11. UD5 inputs tied to VIA CB2.')
