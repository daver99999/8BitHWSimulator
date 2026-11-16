import json, math
exec(open('mknets.py').read().split("nets={}")[0].replace("print(","(lambda *a:None)("))
inv={}
for (ic,p),g in pm.items():
    if g is not None: inv.setdefault(f(g),[]).append(f'{ic}.{p}')
def members(g): return ' '.join(sorted(inv.get(f(g),[]))) or '(none)'
dots=E['dots']
def grp_of_dot(dx,dy):
    c=[(dseg(dx,dy,s),gi) for gi,s in allseg]
    c.sort(); return [gi for d,gi in c if d<2.7]
names=['M1','HALT','MREQ','IORQ','WR','RD','RFSH','REFSH','KBD','SYNC','VIDEO','RAM','INT',"A8'","A14'","A15'",'CPU']
out={}
for w in W:
    if w[4] in names and abs(h(w)-7.5)<.05 and w[1]>240:
        wx,wy=cen(w)
        cands=sorted((math.hypot(wx-dx,wy-dy),dx,dy) for dx,dy in dots)
        d,dx,dy=cands[0]
        gs=sorted(set(grp_of_dot(dx,dy)))
        print(w[4],(round(wx),round(wy)),'dot d',round(d,1),(round(dx),round(dy)),'grps',gs,[members(g) for g in gs])
