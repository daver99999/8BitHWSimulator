import json, math, re
exec(open('pinmap2.py').read().split("if __name__")[0])
sig=[(w[4],)+cen(w) for w in W if re.match(r"^[AD]\d+'?$",w[4])]
from collections import Counter
tap={}   # group -> name
conf=[]
for gi,g in enumerate(G):
    pts=[]
    for s in g: pts+=[(s[0],s[1]),(s[2],s[3])]
    cnt=Counter((round(x,1),round(y,1)) for x,y in pts)
    for (x,y),c in cnt.items():
        if c!=1 or not nearest_bus(x,y): continue
        # orientation of the stub at this end
        seg=[s for s in g if (abs(s[0]-x)<.2 and abs(s[1]-y)<.2) or (abs(s[2]-x)<.2 and abs(s[3]-y)<.2)][0]
        horiz=abs(seg[1]-seg[3])<.01
        best=None
        for (n,wx,wy) in sig:
            if horiz:
                dy=y-wy   # label above the line
                cost=abs(dy-3.8)+max(0,abs(wx-x)-14)*0.5+ (0 if True else 0)
                # label tends to sit within 16 pt of the bus end
                cost+=abs(wx-x)*0.15
            else:
                cost=abs(abs(wx-x)-6)+max(0,abs(wy-y)-10)*.5+abs(wy-y)*.15
            if best is None or cost<best[0]: best=(cost,n)
        tap[gi]=best[1]; 
        if best[0]>4: conf.append((gi,best))
json.dump({str(k):v for k,v in tap.items()},open('tap.json','w'))
print(len(tap),'taps; suspicious:',conf)
byname={}
for gi,n in tap.items(): byname.setdefault(n,[]).append(gi)
for n in sorted(byname): print(n,byname[n])
