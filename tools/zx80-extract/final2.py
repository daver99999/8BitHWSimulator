import json, math, re, sys
exec(open('final.py').read().split("json.dump({'pm'")[0])
# ---- drop group->rail unions made by the word search: rebuild union without rails
par=list(range(len(G)))
for n,gs in byn.items():
    for g in gs[1:]: u(gs[0],g)
M1G=pm[('IC1',27)]; U(M1G,268,394)
U(MREQN,209,406); U(286,393); U(pm[('IC1',20)],274); U(pm[('IC1',21)],275); U(pm[('IC1',22)],273,93,96)
U(pm[('IC1',28)],202,289); U(pm[('IC1',18)],206); U(278,244); U(pm[('IC1',6)],413); U(381,288); U(380,265)
U(400,423); U(428,422)
for n,gs in lab.items(): U(*list(gs))
U(183,277)
U(92,408)     # RAM C.S.
# EAR chain / misc stubs are handled as parts below
two={}   # ref -> (groupA, groupB)
# resistors: terminal groups from twoterm results
exec(open('twoterm.py').read().split("bodies=E['bodies']")[1].split("lab=[")[0].join(["bodies=E['bodies']",""]))
res_bodies2=res_bodies
def rterm(nm):
    for (n,x,y) in [(w[4],)+cen(w) for w in W if w[4]==nm]:
        best=None
        for b in res_bodies2:
            r=b[0]; cx,cy=(r[0]+r[2])/2,(r[1]+r[3])/2; d=math.hypot(cx-x,cy-y)
            if best is None or d<best[0]: best=(d,r)
        r=best[1]; gs=[]
        for gi,s in allseg:
            for (ex,ey) in ((s[0],s[1]),(s[2],s[3])):
                if r[0]-2<=ex<=r[2]+2 and r[1]-2<=ey<=r[3]+2 and gi not in gs: gs.append(gi)
        return gs
RT={}
for i in list(range(1,36)):
    nm=f'R{i}'
    if nm in('R26',): continue
    gs=rterm(nm)
    if gs: RT[nm]=gs
print({k:v for k,v in RT.items() if len(v)!=2})
json.dump({'RT':RT},open('rt.json','w'))
