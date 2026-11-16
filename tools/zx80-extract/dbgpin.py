import sys, math, json
exec(open('pinmap2.py').read().split("if __name__")[0])
def show(ic,pin,wx,wy):
    c=[]
    for gi,s in allseg:
        for (x,y) in ((s[0],s[1]),(s[2],s[3])):
            c.append((math.hypot(wx-x,wy-y),gi,round(x,1),round(y,1),[round(v,1) for v in s]))
    c.sort()
    print(ic,pin,(wx,wy)); 
    seen=set()
    for d,gi,x,y,s in c[:8]:
        print('   ',round(d,1),'grp',gi,(x,y),s)
for a in sys.argv[1:]:
    ic,pin,x,y=a.split(','); show(ic,int(pin),float(x),float(y))
