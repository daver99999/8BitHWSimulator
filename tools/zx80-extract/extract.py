import pymupdf as fitz, math, json
p=fitz.open('/mnt/user-data/uploads/f38735eb-zx80_schema.pdf')[0]
dr=p.get_drawings()
segs=[];bus=[];dots=[];bodies=[]
for x in dr:
    its=x['items']; kinds=''.join(i[0] for i in its); w=x['width'] or 0
    r=x['rect']
    if x['type']=='fs' and kinds=='cccc' and r.width<8 and r.height<8:
        dots.append(((r.x0+r.x1)/2,(r.y0+r.y1)/2)); continue
    if x['type']=='s' and set(kinds)=={'l'}:
        pts=[its[0][1]]+[i[2] for i in its]
        closed = math.dist(pts[0],pts[-1])<0.5 and len(its)>2
        small = r.width<16 and r.height<16 and len(its)>=3
        if len(its)==3 and not closed:
            a,b,c=[(i[1],i[2]) for i in its]
            horiz = abs(a[0].y-a[1].y)<.01 and abs(c[0].y-c[1].y)<.01 and abs(b[0].x-b[1].x)<.01
            vert = abs(a[0].x-a[1].x)<.01 and abs(c[0].x-c[1].x)<.01 and abs(b[0].y-b[1].y)<.01
            if horiz and abs(a[0].x-c[1].x)<.5 and 8<abs(a[0].x-a[1].x)<20 and 8<abs(b[0].y-b[1].y)<40: small=True
            if vert and abs(a[0].y-c[1].y)<.5 and 8<abs(a[0].y-a[1].y)<20 and 8<abs(b[0].x-b[1].x)<40: small=True
        if w>1.9 and not closed:
            for i in its: bus.append((i[1].x,i[1].y,i[2].x,i[2].y))
            continue
        if w<1.0 and not closed and not small:
            for i in its: segs.append((i[1].x,i[1].y,i[2].x,i[2].y))
            continue
    bodies.append(([r.x0,r.y0,r.x1,r.y1],kinds,x['type']))
words=[(w[0],w[1],w[2],w[3],w[4]) for w in p.get_text('words')]
json.dump({'bus':bus,'segs':segs,'dots':dots,'words':words,'bodies':bodies},open('ex.json','w'))
print(len(segs),len(bus),len(dots),len(words),len(bodies))
