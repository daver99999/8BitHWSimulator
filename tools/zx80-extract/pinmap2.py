import json, math
exec(open('build_nets.py').read().split("# signal label words")[0])
def pin_group(wx,wy,maxc=5.5,below=False):
    best=None
    for gi,s in allseg:
        x0,y0,x1,y1=s
        if abs(y0-y1)<0.01:   # horizontal
            xl,xh=min(x0,x1),max(x0,x1)
            dx=0 if xl-2<=wx<=xh+2 else min(abs(wx-xl),abs(wx-xh))
            dy=y0-wy
            # free end close to word
            endd=min(math.hypot(wx-x0,wy-y0),math.hypot(wx-x1,wy-y1))
            if endd>16: continue
            cost=dx+(min(abs(dy-3.7),abs(dy+3.7)) if below else abs(dy-3.7))*1.0
        elif abs(x0-x1)<0.01:
            yl,yh=min(y0,y1),max(y0,y1)
            dy=0 if yl-2<=wy<=yh+2 else min(abs(wy-yl),abs(wy-yh))
            dxs=abs(x0-wx)
            endd=min(math.hypot(wx-x0,wy-y0),math.hypot(wx-x1,wy-y1))
            if endd>16: continue
            cost=dy+abs(dxs-4.5)*0.8
        else: continue
        if best is None or cost<best[0]: best=(cost,gi,s)
    return best if best and best[0]<=maxc else None
if __name__=='__main__':
    for p in (30,31,32,33,40,1,2,16,17):
        for (n,x,y) in pinwords:
            if n==p and 337-14<=x<=377+14 and 375<=y<=539:
                print(p,pin_group(x,y))
