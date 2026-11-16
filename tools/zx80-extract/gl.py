import json, math, re
exec(open('pinmap2.py').read().split("if __name__")[0])
names=['M1','HALT','MREQ','IORQ','WR','RD','RFSH','REFSH','KBD','SYNC','VIDEO','RAM','INT','NMI','RESET','WAIT','BUSRQ','BUSAC',"A8'","A14'","A15'",'MALT','CPU']
GL=[w for w in W if w[4] in names]
for w in GL:
    wx,wy=cen(w)
    # find nearest free end
    best=None
    for gi,g in enumerate(G):
        for s in g:
            for (x,y) in ((s[0],s[1]),(s[2],s[3])):
                dd=math.hypot(wx-x,wy-y)
                if best is None or dd<best[0]: best=(dd,gi,round(x),round(y))
    print(w[4],round(wx),round(wy),h(w),best)
