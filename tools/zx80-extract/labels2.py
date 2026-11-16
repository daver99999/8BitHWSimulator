import json, math
exec(open('mknets.py').read().split("nets={}")[0].replace("print(","(lambda *a:None)("))
inv={}
for (ic,p),g in pm.items():
    if g is not None: inv.setdefault(f(g),[]).append(f'{ic}.{p}')
def members(g): return ' '.join(sorted(inv.get(f(g),[]))) or '(none)'
names=['M1','HALT','MREQ','IORQ','WR','RD','RFSH','REFSH','KBD','SYNC','VIDEO','RAM','INT','NMI',"A8'","A14'","A15'",'CPU']
for w in W:
    if w[4] in names and abs(h(w)-7.5)<.05:
        wx,wy=cen(w); best=None
        for gi,g in enumerate(G):
            for s in g:
                for (x,y) in ((s[0],s[1]),(s[2],s[3])):
                    dd=math.hypot(wx-x,wy-y)
                    if best is None or dd<best[0]: best=(dd,gi,round(x),round(y))
        print(w[4],(round(wx),round(wy)),'->grp',best[1],'d',round(best[0],1),'at',best[2:],':',members(best[1]))
