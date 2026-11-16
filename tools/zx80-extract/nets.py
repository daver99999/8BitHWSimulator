import json, math
E=json.load(open('ex.json'))
segs=[tuple(s) for s in E['segs']]; dots=E['dots']
N=len(segs); par=list(range(N))
def f(a):
    while par[a]!=a: par[a]=par[par[a]]; a=par[a]
    return a
def u(a,b): par[f(a)]=f(b)
T=0.9
def dist_pt_seg(px,py,s):
    x0,y0,x1,y1=s
    dx,dy=x1-x0,y1-y0
    L=dx*dx+dy*dy
    t=0 if L==0 else max(0,min(1,((px-x0)*dx+(py-y0)*dy)/L))
    return math.hypot(px-(x0+t*dx),py-(y0+t*dy))
ends=[]
for i,s in enumerate(segs):
    ends.append((s[0],s[1],i)); ends.append((s[2],s[3],i))
for (px,py,i) in ends:
    for j,s in enumerate(segs):
        if j!=i and dist_pt_seg(px,py,s)<T: u(i,j)
for (px,py) in dots:
    hit=[j for j,s in enumerate(segs) if dist_pt_seg(px,py,s)<2.6]
    for a in hit[1:]: u(hit[0],a)
comp={}
for i in range(N): comp.setdefault(f(i),[]).append(i)
print(len(comp),'wire groups')
json.dump({'groups':[ [segs[i] for i in v] for v in comp.values()]},open('groups.json','w'))
