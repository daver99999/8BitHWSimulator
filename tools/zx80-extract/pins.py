import json, math
E=json.load(open('ex.json')); G=json.load(open('groups.json'))['groups']
W=E['words']; B=E['bodies']
def cen(w): return ((w[0]+w[2])/2,(w[1]+w[3])/2)
def h(w): return round(w[3]-w[1],1)
def dseg(px,py,s):
    x0,y0,x1,y1=s; dx,dy=x1-x0,y1-y0; L=dx*dx+dy*dy
    t=0 if L==0 else max(0,min(1,((px-x0)*dx+(py-y0)*dy)/L))
    return math.hypot(px-(x0+t*dx),py-(y0+t*dy))
allseg=[(gi,s) for gi,g in enumerate(G) for s in g]
def near_group(px,py,maxd=9):
    best=None
    for gi,s in allseg:
        dd=dseg(px,py,s)
        if best is None or dd<best[0]: best=(dd,gi)
    return best if best and best[0]<=maxd else None
# gate labels (height 7.5 numeric, excluding in power labels)
gates=[(w[4],)+cen(w) for w in W if abs(h(w)-7.5)<.05 and w[4].isdigit() and w[4] in ('11','12','13','14','15','16','17','18','19','20')]
icname={ '1':'Z80','2':'ROM','3':'2114','4':'2114','5':'LS373','6':'LS157','7':'LS157','8':'LS157','9':'LS165','10':'LS365','11':'LS00','12':'LS00','13':'LS04','14':'LS05','15':'LS05','16':'LS10','17':'LS32','18':'LS74','19':'LS74','20':'LS86','21':'LS93','22':'7805'}
# gate pin groups by type: (inputs..., output)
GT={'LS00':[(1,2,3),(4,5,6),(9,10,8),(12,13,11)],'LS32':[(1,2,3),(4,5,6),(9,10,8),(12,13,11)],'LS86':[(1,2,3),(4,5,6),(9,10,8),(12,13,11)],
 'LS04':[(1,2),(3,4),(5,6),(9,8),(11,10),(13,12)],'LS05':[(1,2),(3,4),(5,6),(9,8),(11,10),(13,12)],
 'LS10':[(1,2,13,12),(3,4,5,6),(9,10,11,8)]}
pinwords=[(int(w[4]),)+cen(w) for w in W if abs(h(w)-5.4)<.05 and w[4].isdigit()]
res={}
for lab,gx,gy in gates:
    t=icname[lab]
    if t not in GT: continue
    near=[(n,x,y) for (n,x,y) in pinwords if math.hypot(x-gx,y-gy)<26]
    best=None
    for gi_,pins in enumerate(GT[t]):
        found=[]
        for p in pins:
            c=[(math.hypot(x-gx,y-gy),x,y) for (n,x,y) in near if n==p]
            if c: found.append((p,min(c)))
        sc=len(found)
        if best is None or sc>best[0] or (sc==best[0] and sum(f[1][0] for f in found)<best[2]): best=(sc,found,sum(f[1][0] for f in found),pins)
    res[(lab,round(gx),round(gy))]=best
for k,v in sorted(res.items(),key=lambda kv:(int(kv[0][0]),kv[0][2],kv[0][1])):
    print(k,icname[k[0]],'match',v[0],'/',len(v[3]),'pins',v[3],[(p,round(f[1]),round(f[2])) for p,f in v[1]])

print('---- IC rects')
icl=[(w[4],)+cen(w) for w in W if abs(h(w)-8.6)<.05 and w[4].startswith('IC') and len(w[4])>2 or (abs(h(w)-8.6)<.05 and w[4] in('IC1','IC2','IC3','IC4','IC5','IC6','IC7','IC8','IC9'))]
rects={}
for nm,x,y in icl:
    c=[b for b in B if b[0][0]<=x<=b[0][2] and b[0][1]<=y<=b[0][3] and (b[0][2]-b[0][0])>40]
    if c:
        b=min(c,key=lambda b:(b[0][2]-b[0][0])*(b[0][3]-b[0][1])); rects[nm]=b[0]; print(nm,[round(v) for v in b[0]],b[1])
    else: print(nm,'no rect',round(x),round(y))
