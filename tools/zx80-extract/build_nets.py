import json, math, re
exec(open('pins.py').read().split("print('---- IC rects')")[0].replace("for k,v in sorted(res.items()","for k,v in []and sorted(res.items()"))
bus=E['bus']
ICR={'IC1':(337,375,377,539),'IC8':(497,371,529,449),'IC6':(592,371,625,449),'IC7':(592,468,625,542),'IC5':(497,483,530,579),
 'IC9':(175,502,206,595),'IC2':(711,390,752,524),'IC21':(649,586,681,656),'IC3':(113,350,142,420),'IC4':(206,349,235,419),'IC10':(75,505,104,596)}
def nearest_bus(px,py,d=2.5):
    for s in bus:
        if dseg(px,py,s)<d: return True
    return False
# signal label words (height 5.4 non-numeric)
sigw=[(w[4],)+cen(w) for w in W if not w[4].isdigit() and abs(h(w)-5.4)<.05 or (not w[4].isdigit() and abs(h(w)-7.5)<.05)]
names={}   # group -> set(names)
def addname(gi,n): names.setdefault(gi,set()).add(n)
for gi,g in enumerate(G):
    pts=[]
    for s in g: pts+= [(s[0],s[1]),(s[2],s[3])]
    # free ends: endpoints not touching another seg endpoint of the same group
    from collections import Counter
    cnt=Counter((round(x,1),round(y,1)) for x,y in pts)
    free=[p for p in cnt if cnt[p]==1]
    for (x,y) in free:
        onbus = nearest_bus(x,y)
        # nearest word
        best=None
        for (n,wx,wy) in sigw:
            dd=math.hypot(wx-x,wy-y)
            if best is None or dd<best[0]: best=(dd,n)
        if best and best[0]<(14 if onbus else 9): addname(gi,best[1]+('@bus' if onbus else ''))
json.dump({str(k):sorted(v) for k,v in names.items()},open('gnames.json','w'),indent=0)
print(len(names),'named groups')
for gi,v in sorted(names.items()): print(gi,sorted(v))
