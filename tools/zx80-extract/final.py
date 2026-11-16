import json, math, re, sys
exec(open('mknets.py').read().split("nets={}")[0].replace("print(","(lambda *a:None)("))
# ---- manual pin fixes (labels sitting between two stubs)
pm[('IC16',1)]=289; pm[('IC16',2)]=288; pm[('IC16',13)]=286; pm[('IC16',5)]=205; pm[('IC16',4)]=204; pm[('IC16',3)]=206
# flip-flop extra pins (stubs tied to a rail are drawn without a pin number in some cases)
# ---- extra merges by label (group ids come from this deterministic extraction)
def U(*gs):
    for g in gs[1:]: u(gs[0],g)
M1G=pm[('IC1',27)]; U(M1G,268,394)
MREQN=pm[('IC1',19)]; U(MREQN,209,406)
U(286,393)                                   # MREQ (active high)
U(pm[('IC1',20)],274)                        # IORQ
U(pm[('IC1',21)],275)                        # RD
U(pm[('IC1',22)],273,93,96)                  # WR
U(pm[('IC1',28)],202,289)                    # RFSH
U(pm[('IC1',18)],206)                        # HALT
U(278,244)                                   # KBD
U(pm[('IC1',6)],413)                         # CPU clock
U(381,288)                                   # /phi
U(380,265)                                   # 2phi
U(400,423)                                   # SYNC
U(428,422)                                   # VIDEO
for n,gs in lab.items(): U(*list(gs))        # A8' A14' A15'
# A0 into the OR gate IC17.13
U(183,277)
# rail fixes: group 92 (RAM C.S.) must not be on 0V
json.dump({'pm':{f'{k[0]}.{k[1]}':v for k,v in pm.items()}},open('pm_final.json','w'))
inv={}
for (ic,p),g in pm.items():
    if g is not None: inv.setdefault(f(g),[]).append(f'{ic}.{p}')
for r,m in sorted(inv.items(),key=lambda kv:-len(kv[1])):
    tn=[n for n,gs in byn.items() if f(gs[0])==r]
    print(('['+','.join(tn)+'] ' if tn else '')+str(r)+': '+' '.join(sorted(m)))
