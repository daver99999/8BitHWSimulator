import json, math, re
exec(open('twoterm.py').read().split("bodies=E['bodies']")[0])
# find diode bodies: nonwire drawings 'lll' closed triangles ~ 14x? near D labels
lab=[(w[4],)+cen(w) for w in W if re.match(r'^D(3|4|5|6|7|8|9|10|11)$',w[4]) and abs(h(w)-7.5)<.1]
print(lab)
for nm,x,y in lab:
    c=sorted({(round(math.hypot(ex-x,ey-y),1),gi,round(ex),round(ey)) for gi,s in allseg for (ex,ey) in ((s[0],s[1]),(s[2],s[3])) if math.hypot(ex-x,ey-y)<30})
    print(nm,(round(x),round(y)),[(g,d,(ex,ey),members(g)) for d,g,ex,ey in c[:6]])
# diode bodies near
for nm,x,y in lab:
    for b in E['bodies']:
        r=b[0]
        if r[0]-30<x<r[2]+30 and r[1]-30<y<r[3]+30 and (r[2]-r[0])<30 and (r[3]-r[1])<30 and b[1] in('lll','l','llll'): print(nm,[round(v,1) for v in r],b[1])
