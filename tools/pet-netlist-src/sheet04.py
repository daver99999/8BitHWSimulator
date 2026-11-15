from netlib import N
N.sheet_is('04')
ADDR = {8:'BA0',7:'BA1',6:'BA2',5:'BA3',4:'BA4',3:'BA5',2:'BA6',1:'BA7',23:'BA8',22:'BA9',19:'BA10',18:'BA11'}
DATA = {9:'D0',10:'D1',11:'D2',13:'D3',14:'D4',15:'D5',16:'D6',17:'D7'}
# 7 ROM sockets, all drawn with the 2332 pinout (UD7 is labelled 2316 but wired the same)
SEL = {'UD6':'SELF','UD7':'UD7_CS1','UD8':'SELD','UD9':'SELC','UD10':'SELB','UD11':'SELA','UD12':'SEL9'}
for ref, sel in SEL.items():
    N.chip(ref,'2332')
    N.pins(ref, ADDR); N.pins(ref, DATA)
    N.pins(ref, {21:'NOROM', 20:sel})     # CS2 = NO ROM rail (R40 100R to +5V)
N.chip('R40','RES'); N.pins('R40', {1:'+5V', 2:'NOROM'})
N.chip('R44','RES'); N.pins('R44', {1:'+5V', 2:'UE14_STB1'})
N.chip('UE14','7425'); N.chip('UE5','LS00')
N.pins('UE14', {5:'X8XX', 4:'X8XX', 2:'SELE', 1:'SELE', 3:'UE14_STB1', 6:'UE14_Y'})
N.pins('UE5',  {1:'UE14_Y', 2:'UE14_Y', 3:'UD7_CS1'})
N.note('UD6 CS1 = SELF inferred (its select line is off the crop); verify')
