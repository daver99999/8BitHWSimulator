import json, math
exec(open('pinmap2.py').read().split("if __name__")[0])
tap=json.load(open('tap.json'))
pm={}
def put(ic,pin,x,y,below=False):
    r=pin_group(x,y,below=below)
    pm[(ic,pin)]=r[1] if r else None
    if not r: print('NO GROUP',ic,pin,round(x),round(y))
for k,v in res.items():
    ic='IC'+k[0]
    for p,f in v[1]: put(ic,p,f[1],f[2],below=True)
def wide(ic,pin,cx,cy,rad):
    c=[(math.hypot(x-cx,y-cy),x,y) for (n,x,y) in pinwords if n==pin and math.hypot(x-cx,y-cy)<rad]
    if c: d,x,y=min(c); put(ic,pin,x,y,below=True)
wide('IC20',12,99,774,45); wide('IC16',11,314,739,45)
FF={'IC18a':(604,714,625,735),'IC19b':(657,715,680,735),'IC19a':(707,715,731,735),'IC18b':(120,846,150,876)}
for nm,(x0,y0,x1,y1) in FF.items():
    for (n,x,y) in pinwords:
        if x0-16<=x<=x1+16 and y0-20<=y<=y1+22: put(nm,n,x,y,below=True)
for nm,(x0,y0,x1,y1) in ICR.items():
    for (n,x,y) in pinwords:
        if x0-14<=x<=x1+14 and y0-6<=y<=y1+6: put(nm,n,x,y)
json.dump({f'{k[0]}.{k[1]}':v for k,v in pm.items()},open('pm3.json','w'))
for ic in sorted({k[0] for k in pm}):
    print(ic, ' '.join(f'{p}:{pm[(ic,p)]}' for p in sorted(p for (i,p) in pm if i==ic)))
