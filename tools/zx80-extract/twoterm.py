import json, math, re
exec(open('mknets.py').read().split("nets={}")[0].replace("print(","(lambda *a:None)("))
inv={}
for (ic,p),g in pm.items():
    if g is not None: inv.setdefault(f(g),[]).append(f'{ic}.{p}')
def members(g): return ' '.join(sorted(inv.get(f(g),[])))[:60] or '-'
bodies=E['bodies']
res_bodies=[b for b in bodies if b[1]=='re' and 4<(b[0][2]-b[0][0])<40 and 4<(b[0][3]-b[0][1])<40 and not ((b[0][2]-b[0][0])>25 and (b[0][3]-b[0][1])>25)]
lab=[(w[4],)+cen(w) for w in W if re.match(r'^R\d+$',w[4]) or re.match(r'^C\d+$',w[4])]
for nm,x,y in sorted(lab,key=lambda t:(t[0][0],int(t[0][1:]))):
    # nearest body
    best=None
    for b in res_bodies:
        r=b[0]; cx,cy=(r[0]+r[2])/2,(r[1]+r[3])/2
        d=math.hypot(cx-x,cy-y)
        if best is None or d<best[0]: best=(d,r)
    # wire ends near the body
    ends=[]
    if nm[0]=='R' and best and best[0]<28:
        r=best[1]
        for gi,s in allseg:
            for (ex,ey) in ((s[0],s[1]),(s[2],s[3])):
                if r[0]-2<=ex<=r[2]+2 and r[1]-2<=ey<=r[3]+2: ends.append((gi,round(ex),round(ey)))
        gs=sorted(set(g for g,_,_ in ends))
        print(nm,'body',[round(v) for v in r],'groups',gs,[members(g) for g in gs])
    else:
        # capacitor: ends within 14pt of label
        c=sorted({(round(math.hypot(ex-x,ey-y),1),gi) for gi,s in allseg for (ex,ey) in ((s[0],s[1]),(s[2],s[3])) if math.hypot(ex-x,ey-y)<20})
        print(nm,'near',[(g,d,members(g)) for d,g in c[:6]])
