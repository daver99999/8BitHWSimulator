import json, math
exec(open('labels3.py').read().split("names=[")[0])
names=['M1','HALT','MREQ','IORQ','WR','RD','RFSH','KBD','SYNC','VIDEO','RAM','INT',"A8'","A14'","A15'",'CPU','2','x','Ø']
allseg_all=[s for g in G for s in g]
def overbar(wx0,wx1,wy0):
    for s in allseg_all:
        if abs(s[1]-s[3])<0.01 and wy0-9<s[1]<wy0+1 and min(s[0],s[2])>=wx0-4 and max(s[0],s[2])<=wx1+4 and abs(s[0]-s[2])<(wx1-wx0)+9 and abs(s[0]-s[2])>3: return True
    return False
for w in W:
    if w[4] in names and abs(h(w)-7.5)<.05 and 240<w[1]:
        wx,wy=cen(w)
        # candidate groups: wire ends within 20pt
        c={}
        for gi,s in allseg:
            for (x,y) in ((s[0],s[1]),(s[2],s[3])):
                dd=math.hypot(wx-x,wy-y)
                if dd<22: c[gi]=min(c.get(gi,99),dd)
        cl=sorted(c.items(),key=lambda kv:kv[1])[:4]
        print(('~' if overbar(w[0],w[2],w[1]) else ' ')+w[4],(round(wx),round(wy)),[(g,round(d),members(g)[:40]) for g,d in cl])
