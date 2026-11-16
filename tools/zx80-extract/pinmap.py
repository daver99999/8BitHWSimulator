import json, math
exec(open('build_nets.py').read().split("# signal label words")[0])
pm={}
def put(ic,pin,x,y,maxd=7.5):
    pm[(ic,pin)]=near_group(x,y,maxd)
for k,v in res.items():
    ic='IC'+k[0]
    for p,f in v[1]: put(ic,p,f[1],f[2])
# manual: LS86 gate at (99,774) pin 12 and LS10 (314,739) pin 11 missing -> look wider
def wide(ic,pin,cx,cy,rad):
    c=[(math.hypot(x-cx,y-cy),x,y) for (n,x,y) in pinwords if n==pin and math.hypot(x-cx,y-cy)<rad]
    if c: d,x,y=min(c); put(ic,pin,x,y); print('wide',ic,pin,round(x),round(y))
wide('IC20',12,99,774,45); wide('IC16',11,314,739,45)
# flip-flops: rects
FF={'IC18a':(604,714,625,735),'IC19b':(657,715,680,735),'IC19a':(707,715,731,735),'IC18b':(120,846,150,876)}
for nm,(x0,y0,x1,y1) in FF.items():
    for (n,x,y) in pinwords:
        if x0-16<=x<=x1+16 and y0-20<=y<=y1+22: put(nm,n,x,y)
# big ICs
for nm,(x0,y0,x1,y1) in ICR.items():
    for (n,x,y) in pinwords:
        if x0-14<=x<=x1+14 and y0-6<=y<=y1+6: put(nm,n,x,y)
json.dump({f'{k[0]}.{k[1]}':(v[1] if v else None) for k,v in pm.items()},open('pm.json','w'))
for ic in sorted({k[0] for k in pm}):
    print(ic, ' '.join(f'{p}:{(pm[(ic,p)][1] if pm[(ic,p)] else "-")}' for p in sorted(p for (i,p) in pm if i==ic)))
