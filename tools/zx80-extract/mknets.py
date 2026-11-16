import json, math
exec(open('pinmap3.py').read().split("json.dump({f'{k[0]}")[0].replace("print('NO GROUP',ic,pin,round(x),round(y))","pass"))
par=list(range(len(G)))
def f(a):
    while par[a]!=a: par[a]=par[par[a]]; a=par[a]
    return a
def u(a,b): par[f(a)]=f(b)
byn={}
for gi,n in tap.items(): byn.setdefault(n,[]).append(int(gi))
# rails: words 5V/0V -> nearest group
def nearg(wx,wy,maxd=14):
    best=None
    for gi,g in enumerate(G):
        for s in g:
            dd=dseg(wx,wy,s)
            if best is None or dd<best[0]: best=(dd,gi)
    return best[1] if best and best[0]<=maxd else None
rail={'5V':[], '0V':[]}
for w in W:
    if w[4] in ('5V','+5V','0V') and abs(h(w)-7.5)<.05:
        wx,wy=cen(w); g=nearg(wx,wy,12)
        if g is not None: rail['0V' if w[4]=='0V' else '5V'].append(g)
both=set(rail['5V'])&set(rail['0V'])
for k in rail: rail[k]=[g for g in rail[k] if g not in both]
print('dropped',both)
for k,v in rail.items():
    print(k,v)
    for g in v[1:]: u(v[0],g)
for n,gs in byn.items():
    for g in gs[1:]: u(gs[0],g)
# label-merges for primed lines
lab={}
for w in W:
    if w[4] in ("A8'","A14'","A15'"):
        wx,wy=cen(w); g=nearg(wx,wy,10)
        if g is not None: lab.setdefault(w[4],set()).add(g)
for n,gs in lab.items():
    print('label',n,gs)
nets={}
for (ic,p),g in pm.items():
    if g is None: continue
    nets.setdefault(f(g),[]).append(f'{ic}.{p}')
for n,gs in byn.items(): pass
json.dump({'pm':{f'{k[0]}.{k[1]}':v for k,v in pm.items()},'par':[f(i) for i in range(len(G))]},open('nets.json','w'))
for r,m in sorted(nets.items(),key=lambda kv:-len(kv[1])):
    tn=[n for n,gs in byn.items() if f(gs[0])==r]
    print(('['+','.join(tn)+'] ' if tn else '')+str(r)+': '+' '.join(sorted(m)))
