#!/usr/bin/env python3
"""vhdl2c.py - turn the small VHDL subset of frozen-signal's Apple IIe MMU/IOU re-implementation (CC0,
https://github.com/frozen-signal/Apple_IIe_MMU_IOU, redrawn from Apple's own schematics) into a C model.

The design is flattened (every component instance gets its own copy of the signals, prefixed by the instance label) and run
the way VHDL runs it, in delta cycles: in each delta every concurrent assignment and every process is evaluated against the
values the signals had at the start of the delta, then all the new values are stored at once; this repeats until nothing
changes. rising_edge(S) / falling_edge(S) is true in the first delta after S changed. A signal that no branch of a process
assigns keeps its value (that is how the latches are written). 'Z' on an assignment is a separate bit mask (a tri-state output).

Supported: entity (generic with a default, port in/out/inout of std_logic, std_logic_vector, unsigned), architecture with
component declarations (skipped), signal / constant declarations, concurrent assignments (plain, when/else, with-select),
component instances with port map (named association, open), processes (if/elsif/else, case, assignments to signals,
slices and elements), expressions with not and or nand nor xor xnor = /= < > <= >= + & and the aggregates (others => 'x').

Usage: vhdl2c.py TOP_ENTITY PREFIX OUT.c  FILE.vhdl...  [-G NAME=value]...
"""
import re, sys

# ------------------------------------------------------------------ lexer
TOK = re.compile(r"""\s*(?:
    (?P<str>x?"[^"]*")|(?P<chr>'.')|(?P<num>\d+)|(?P<id>[A-Za-z_][A-Za-z_0-9]*)|
    (?P<op><=|=>|:=|/=|>=|[().<>=,;:&+\-|*]))""", re.X | re.I)

def lex(text):
    text = re.sub(r'--[^\n]*', '', text)
    out, pos = [], 0
    while pos < len(text):
        m = TOK.match(text, pos)
        if not m or m.end() == pos:
            if text[pos:].strip() == '': break
            raise SyntaxError('lex error near %r' % text[pos:pos + 30])
        pos = m.end()
        k = m.lastgroup
        v = m.group(k)
        if k == 'id': v = v.lower()
        out.append((k, v))
    return out

class P:
    def __init__(self, toks): self.t, self.i = toks, 0
    def peek(self, o=0): return self.t[self.i + o] if self.i + o < len(self.t) else ('eof', '')
    def next(self): x = self.peek(); self.i += 1; return x
    def isid(self, *w): k, v = self.peek(); return k == 'id' and (not w or v in w)
    def isop(self, *w): k, v = self.peek(); return k == 'op' and v in w
    def accept(self, v):
        if self.peek()[1] == v and self.peek()[0] in ('id', 'op'): self.i += 1; return True
        return False
    def expect(self, v):
        if not self.accept(v): raise SyntaxError('expected %r got %r at token %d' % (v, self.peek(), self.i))
    def ident(self):
        k, v = self.next()
        if k != 'id': raise SyntaxError('identifier expected, got %r' % (v,))
        return v

# ------------------------------------------------------------------ parser: types, expressions
def parse_type(p):
    name = p.ident()
    if name == 'std_logic': return 1
    if name in ('std_logic_vector', 'unsigned'):
        p.expect('(')
        hi = int(p.next()[1]); d = p.ident(); lo = int(p.next()[1]); p.expect(')')
        assert d == 'downto'
        return hi - lo + 1
    raise SyntaxError('type %s' % name)

LOGIC = ('and', 'or', 'nand', 'nor', 'xor', 'xnor')

def parse_expr(p):
    e = parse_rel(p)
    while p.isid(*LOGIC):
        op = p.ident(); e = ('bin', op, e, parse_rel(p))
    return e

def parse_rel(p):
    e = parse_add(p)
    if p.isop('=', '/=', '<', '>', '<=', '>='):
        op = p.next()[1]; e = ('bin', op, e, parse_add(p))
    return e

def parse_add(p):
    e = parse_un(p)
    while p.isop('+', '-', '&'):
        op = p.next()[1]; e = ('bin', op, e, parse_un(p))
    return e

def parse_un(p):
    if p.isid('not'): p.next(); return ('not', parse_un(p))
    return parse_prim(p)

def parse_prim(p):
    k, v = p.peek()
    if k == 'chr': p.next(); return ('lit', {'0': 0, '1': 1}.get(v[1], 'Z' if v[1] in 'zZ' else 0), 1, v[1].upper())
    if k == 'str':
        p.next()
        if v[0] in 'xX': bits = ''.join(format(int(c, 16), '04b') for c in v[2:-1])
        else: bits = v[1:-1]
        return ('lit', int(bits, 2) if bits else 0, len(bits), None)
    if k == 'num': p.next(); return ('int', int(v))
    if k == 'op' and v == '(':
        p.next()
        if p.isid('others'):
            p.next(); p.expect('=>'); c = parse_prim(p); p.expect(')')
            return ('others', c)
        e = parse_expr(p)
        if p.isop(','): raise SyntaxError('aggregates other than (others => x)')
        p.expect(')')
        return ('paren', e)
    if k == 'id':
        name = p.ident()
        if name in ('rising_edge', 'falling_edge'):
            p.expect('('); a = parse_expr(p); p.expect(')')
            return ('edge', name, a)
        if p.isop('('):
            p.next()
            a = parse_add(p)
            if p.isid('downto'):
                p.next(); b = parse_add(p); p.expect(')')
                return ('slice', name, a[1], b[1])
            p.expect(')')
            return ('idx', name, a[1])
        return ('id', name)
    raise SyntaxError('expression at %r' % (p.peek(),))

# ------------------------------------------------------------------ parser: design units
class Entity:
    def __init__(s): s.name = ''; s.generics = {}; s.ports = []   # ports: (name, dir, width)
class Arch:
    def __init__(s): s.entity = ''; s.signals = {}; s.consts = {}; s.stmts = []

def skip_to_semicolon(p):
    while not p.isop(';'): p.next()
    p.next()

def parse_file(text, ents, archs):
    p = P(lex(text))
    while p.peek()[0] != 'eof':
        if p.isid('library', 'use'): skip_to_semicolon(p); continue
        if p.isid('entity'):
            p.next(); e = Entity(); e.name = p.ident(); p.expect('is')
            if p.isid('generic'):
                p.next(); p.expect('(')
                while True:
                    n = p.ident(); p.expect(':'); parse_type(p)
                    p.expect(':='); d = parse_expr(p); e.generics[n] = d
                    if p.accept(';'): continue
                    break
                p.expect(')'); p.expect(';')
            if p.isid('port'):
                p.next(); p.expect('(')
                while True:
                    names = [p.ident()]
                    while p.accept(','): names.append(p.ident())
                    p.expect(':'); d = p.ident(); w = parse_type(p)
                    if p.isop(':='): p.next(); parse_expr(p)
                    for n in names: e.ports.append((n, d, w))
                    if p.accept(';'): continue
                    break
                p.expect(')'); p.expect(';')
            p.expect('end'); p.accept('entity')
            if not p.isop(';'): p.next()
            p.expect(';')
            ents[e.name] = e
        elif p.isid('architecture'):
            p.next(); a = Arch(); p.ident(); p.expect('of'); a.entity = p.ident(); p.expect('is')
            parse_decls(p, a)
            p.expect('begin')
            a.stmts = parse_conc(p)
            p.expect('end'); p.accept('architecture')
            if not p.isop(';'): p.next()
            p.expect(';')
            archs[a.entity] = a
        else:
            raise SyntaxError('unexpected %r' % (p.peek(),))

def parse_decls(p, a):
    while not p.isid('begin'):
        if p.isid('component'):
            while not (p.isid('end') and p.peek(1)[1] == 'component'): p.next()
            skip_to_semicolon(p)
        elif p.isid('signal'):
            p.next(); names = [p.ident()]
            while p.accept(','): names.append(p.ident())
            p.expect(':'); w = parse_type(p); init = None
            if p.accept(':='): init = parse_expr(p)
            p.expect(';')
            for n in names: a.signals[n] = (w, init)
        elif p.isid('constant'):
            p.next(); n = p.ident(); p.expect(':'); w = parse_type(p); p.expect(':='); v = parse_expr(p); p.expect(';')
            a.consts[n] = (w, v)
        else:
            raise SyntaxError('declaration %r' % (p.peek(),))

def parse_target(p):
    name = p.ident()
    if p.isop('('):
        p.next(); a = int(p.next()[1])
        if p.isid('downto'):
            p.next(); b = int(p.next()[1]); p.expect(')'); return ('slice', name, a, b)
        p.expect(')'); return ('idx', name, a)
    return ('id', name)

def parse_cond_chain(p):
    """expr [when cond else expr ...] -> [(value, cond or None)]"""
    chain = []
    while True:
        v = parse_expr(p)
        if p.isid('when'):
            p.next(); c = parse_expr(p); chain.append((v, c)); p.expect('else'); continue
        chain.append((v, None)); return chain

def parse_conc(p):
    stmts = []
    while not p.isid('end'):
        if p.isid('with'):
            p.next(); sel = parse_expr(p); p.expect('select'); tgt = parse_target(p); p.expect('<=')
            arms = []
            while True:
                v = parse_expr(p); p.expect('when')
                if p.isid('others'): p.next(); c = None
                else: c = parse_expr(p)
                arms.append((v, c))
                if p.accept(','): continue
                break
            p.expect(';'); stmts.append(('with', sel, tgt, arms))
        elif p.isid('process') or (p.peek(1) == ('op', ':') and p.peek(2)[1] == 'process'):
            if not p.isid('process'): p.next(); p.next()
            p.next(); sens = []
            if p.accept('('):
                while not p.isop(')'): p.next()
                p.next()
            p.accept('is'); p.expect('begin')
            body = parse_seq(p)
            p.expect('end'); p.expect('process'); p.expect(';')
            stmts.append(('process', body))
        elif p.peek(1) == ('op', ':') and p.peek(2)[0] == 'id':     # instance
            label = p.ident(); p.next(); comp = p.ident()
            if p.isid('generic'):
                p.next(); p.expect('map'); p.expect('(')
                depth = 1
                while depth:
                    k, v = p.next()
                    depth += (v == '(') - (v == ')')
            p.expect('port'); p.expect('map'); p.expect('(')
            assoc = []
            while True:
                f = p.ident(); p.expect('=>')
                if p.isid('open'): p.next(); a = None
                else: a = parse_expr(p)
                assoc.append((f, a))
                if p.accept(','): continue
                break
            p.expect(')'); p.expect(';')
            stmts.append(('inst', label, comp, assoc))
        else:
            tgt = parse_target(p); p.expect('<=')
            chain = parse_cond_chain(p); p.expect(';')
            stmts.append(('assign', tgt, chain))
    return stmts

def parse_seq(p):
    out = []
    while not (p.isid('end') or p.isid('elsif') or p.isid('else') or p.isid('when')):
        if p.isid('if'):
            p.next(); arms = []
            c = parse_expr(p); p.expect('then'); arms.append((c, parse_seq(p)))
            els = None
            while True:
                if p.isid('elsif'):
                    p.next(); c = parse_expr(p); p.expect('then'); arms.append((c, parse_seq(p)))
                elif p.isid('else'):
                    p.next(); els = parse_seq(p)
                else: break
            p.expect('end'); p.expect('if'); p.expect(';')
            out.append(('if', arms, els))
        elif p.isid('case'):
            p.next(); sel = parse_expr(p); p.expect('is'); arms = []
            while p.isid('when'):
                p.next()
                if p.isid('others'): p.next(); ch = None
                else:
                    ch = [parse_expr(p)]
                    while p.accept('|'): ch.append(parse_expr(p))
                p.expect('=>'); arms.append((ch, parse_seq(p)))
            p.expect('end'); p.expect('case'); p.expect(';')
            out.append(('case', sel, arms))
        elif p.isid('null'): p.next(); p.expect(';')
        else:
            tgt = parse_target(p); p.expect('<=')
            chain = parse_cond_chain(p); p.expect(';')
            out.append(('assign', tgt, chain))
    return out

# ------------------------------------------------------------------ flattening
class Flat:
    def __init__(s):
        s.sigs = {}          # qualified name -> [width, init, role]  role: in / out / inout / int
        s.order = []
        s.stmts = []         # flat statements, signal names qualified
        s.zsigs = set()      # signals that are assigned 'Z' somewhere
    def add(s, name, width, init=0, role='int'):
        if name in s.sigs: raise KeyError('duplicate ' + name)
        s.sigs[name] = [width, init, role]; s.order.append(name)

def const_eval(e, consts):
    """value of a constant expression (literal / named constant)"""
    if e[0] == 'lit': return e[1]
    if e[0] == 'id':
        v = consts[e[1]]; return const_eval(v, consts) if isinstance(v, tuple) else v
    if e[0] == 'paren': return const_eval(e[1], consts)
    if e[0] == 'others': return 0xFFFFFFFF if const_eval(e[1], consts) else 0
    raise ValueError('not constant: %r' % (e,))

def qualify(e, scope):
    """rename the identifiers of an expression through scope: local name -> ('sig', qname) / ('const', value)"""
    k = e[0]
    if k in ('lit', 'int', 'others'):
        return e if k != 'others' else ('others', qualify(e[1], scope))
    if k == 'id':
        kind, v = scope[e[1]]
        return ('id', v) if kind == 'sig' else ('lit', v[0], v[1], None)
    if k in ('idx', 'slice'):
        kind, v = scope[e[1]]
        assert kind == 'sig', e
        return (k, v) + e[2:]
    if k == 'not' or k == 'paren': return (k, qualify(e[1], scope))
    if k == 'bin': return ('bin', e[1], qualify(e[2], scope), qualify(e[3], scope))
    if k == 'edge': return ('edge', e[1], qualify(e[2], scope))
    raise ValueError(k)

def qtarget(t, scope):
    kind, v = scope[t[1]]
    assert kind == 'sig', t
    return (t[0], v) + t[2:]

def qstmts(stmts, scope):
    out = []
    for s in stmts:
        if s[0] == 'assign':
            out.append(('assign', qtarget(s[1], scope), [(qualify(v, scope), qualify(c, scope) if c else None) for v, c in s[2]]))
        elif s[0] == 'if':
            out.append(('if', [(qualify(c, scope), qstmts(b, scope)) for c, b in s[1]], qstmts(s[2], scope) if s[2] is not None else None))
        elif s[0] == 'case':
            out.append(('case', qualify(s[1], scope),
                        [([qualify(c, scope) for c in ch] if ch else None, qstmts(b, scope)) for ch, b in s[2]]))
        else: raise ValueError(s[0])
    return out

def instantiate(flat, ents, archs, ename, prefix, port_actuals, generics):
    ent, arch = ents[ename], archs[ename]
    scope = {}
    for n, d, w in ent.ports:
        q = prefix + n
        flat.add(q, w, 0, 'int')
        scope[n] = ('sig', q)
    for n, v in ent.generics.items(): scope[n] = ('const', (generics.get(n, const_eval(v, {})), 1))
    for n, (w, v) in arch.consts.items():
        scope[n] = ('const', (const_eval(v, {k: x[1][0] for k, x in scope.items() if x[0] == 'const'} | {k: v2 for k, v2 in []}), w))
    for n, (w, init) in arch.signals.items():
        q = prefix + n
        iv = 0
        if init is not None:
            iv = const_eval(init, {k: x[1][0] for k, x in scope.items() if x[0] == 'const'})
        flat.add(q, w, iv & mask(w), 'int')
        scope[n] = ('sig', q)
    # port associations of this instance
    for f, a, qa in port_actuals:
        pass
    for st in arch.stmts:
        if st[0] == 'assign':
            flat.stmts.append(('assign', qtarget(st[1], scope), [(qualify(v, scope), qualify(c, scope) if c else None) for v, c in st[2]]))
        elif st[0] == 'with':
            sel = qualify(st[1], scope)
            chain = []
            conds = []
            for v, c in st[3]:
                if c is None: chain.append((qualify(v, scope), None))
                else:
                    cc = ('bin', '=', sel, qualify(c, scope)); chain.append((qualify(v, scope), cc))
            # others must come last: move it
            chain.sort(key=lambda x: x[1] is None)
            flat.stmts.append(('assign', qtarget(st[2], scope), chain))
        elif st[0] == 'process':
            flat.stmts.append(('process', qstmts(st[1], scope)))
        elif st[0] == 'inst':
            _, label, comp, assoc = st
            sub = prefix + label + '__'
            ce = ents[comp]
            dirs = {n: d for n, d, w in ce.ports}
            sub_actual = []
            instantiate(flat, ents, archs, comp, sub, [], {})
            for f, a in assoc:
                fq = sub + f
                if a is None: continue
                if dirs[f] == 'in':
                    flat.stmts.append(('assign', ('id', fq), [(qualify(a, scope), None)]))
                else:
                    # an output drives the actual (a signal, element or slice of one)
                    if a[0] not in ('id', 'idx', 'slice'): raise ValueError('output actual %r' % (a,))
                    t = qtarget(a, scope)
                    flat.stmts.append(('assign', t, [(('id', fq), None)]))
        else:
            raise ValueError(st[0])
    return scope

# ------------------------------------------------------------------ C emission
def mask(w): return (1 << w) - 1

class Em:
    def __init__(s, flat, prefix):
        s.f, s.pre = flat, prefix
        s.idx = {n: i for i, n in enumerate(flat.order)}
        s.lines = []
    def cname(s, n): return n
    def W(s, e):
        k = e[0]
        if k == 'lit': return e[2]
        if k == 'int': return None
        if k == 'id': return s.f.sigs[e[1]][0]
        if k == 'idx': return 1
        if k == 'slice': return e[2] - e[3] + 1
        if k == 'not' or k == 'paren': return s.W(e[1])
        if k == 'edge': return 1
        if k == 'bin':
            op = e[1]
            if op in ('=', '/=', '<', '>', '<=', '>='): return 1
            if op == '&': return s.W(e[2]) + s.W(e[3])
            a, b = s.W(e[2]), s.W(e[3])
            if a is None: return b
            if b is None: return a
            return max(a, b)
        if k == 'others': return None
        raise ValueError(k)
    def X(s, e, want=None):
        """C expression for e (an unsigned value, already masked to its width)"""
        k = e[0]
        if k == 'lit': return '%du' % e[1] if e[1] != 'Z' else '0u'
        if k == 'int': return '%du' % e[1]
        if k == 'id': return 'c[%d]' % s.idx[e[1]]
        if k == 'idx': return '((c[%d] >> %d) & 1u)' % (s.idx[e[1]], e[2])
        if k == 'slice': return '((c[%d] >> %d) & %du)' % (s.idx[e[1]], e[3], mask(e[2] - e[3] + 1))
        if k == 'paren': return s.X(e[1], want)
        if k == 'not':
            w = s.W(e[1]) or want or 1
            return '((~%s) & %du)' % (s.X(e[1], w), mask(w))
        if k == 'edge':
            a = e[2]
            assert a[0] == 'id', a
            i = s.idx[a[1]]
            if e[1] == 'rising_edge': return '((c[%d] & ~l[%d]) & 1u)' % (i, i)
            return '((~c[%d] & l[%d]) & 1u)' % (i, i)
        if k == 'others':
            return '0u'
        if k == 'bin':
            op = e[1]
            wa, wb = s.W(e[2]), s.W(e[3])
            w = max(x for x in (wa, wb, 1) if x is not None)
            if op in ('and', 'or', 'xor'):
                w = s.W(e) or want or 1
                c = {'and': '&', 'or': '|', 'xor': '^'}[op]
                return '((%s %s %s) & %du)' % (s.X(e[2], w), c, s.X(e[3], w), mask(w))
            if op in ('nand', 'nor', 'xnor'):
                w = s.W(e) or want or 1
                c = {'nand': '&', 'nor': '|', 'xnor': '^'}[op]
                return '((~(%s %s %s)) & %du)' % (s.X(e[2], w), c, s.X(e[3], w), mask(w))
            if op in ('=', '/=', '<', '>', '<=', '>='):
                cc = {'=': '==', '/=': '!='}.get(op, op)
                return '(%s %s %s)' % (s.X(e[2], w), cc, s.X(e[3], w))
            if op == '+' or op == '-':
                w = s.W(e) or want or 1
                return '((%s %s %s) & %du)' % (s.X(e[2], w), op, s.X(e[3], w), mask(w))
            if op == '&':
                return '((%s << %d) | %s)' % (s.X(e[2]), wb, s.X(e[3]))
        raise ValueError(e)

    def assign(s, ind, tgt, chain, ctx):
        """emit one (conditional) assignment; ctx: 'conc' or 'seq' (for seq a plain assignment is the same)"""
        f = s.f
        name = tgt[1]
        tw = f.sigs[name][0]
        i = s.idx[name]
        if tgt[0] == 'id': lo, w = 0, tw
        elif tgt[0] == 'idx': lo, w = tgt[2], 1
        else: lo, w = tgt[3], tgt[2] - tgt[3] + 1
        m = mask(w) << lo
        def store(v):
            if v[0] == 'lit' and v[1] == 'Z' or (v[0] == 'others' and v[1][0] == 'lit' and v[1][1] == 'Z'):
                f.zsigs.add(name)
                return '%sz[%d] |= %du;' % (ind, i, m)
            if v[0] == 'others':
                bit = v[1][1]
                val = mask(w) if bit == 1 else 0
                return '%snx[%d] = (nx[%d] & ~%du) | %du; %sz[%d] &= ~%du;' % (ind, i, i, m, (val << lo) & m, ind, i, m) if name in f.zsigs else \
                       '%snx[%d] = (nx[%d] & ~%du) | %du;' % (ind, i, i, m, (val << lo) & m)
            x = s.X(v, w)
            zc = ' z[%d] &= ~%du;' % (i, m) if name in ZSET else ''
            return '%snx[%d] = (nx[%d] & ~%du) | ((%s << %d) & %du);%s' % (ind, i, i, m, x, lo, m, zc)
        opened = False
        for n, (v, c) in enumerate(chain):
            if c is None:
                if n == 0:
                    s.lines.append(store(v))
                else:
                    s.lines.append('%s} else {' % ind)
                    s.lines.append(store(v))
                break
            s.lines.append('%s%sif (%s) {' % (ind, '' if n == 0 else '} else ', s.X(c, 1)))
            opened = True
            s.lines.append(store(v))
        if opened: s.lines.append('%s}' % ind)
    def seq(s, ind, body):
        for st in body:
            if st[0] == 'assign':
                if len(st[2]) == 1 and st[2][0][1] is None:
                    s.assign(ind, st[1], st[2], 'seq')
                else:
                    s.lines.append('%s{' % ind)
                    s.assign(ind + '  ', st[1], st[2], 'seq')
                    s.lines.append('%s}' % ind)
            elif st[0] == 'if':
                for n, (c, b) in enumerate(st[1]):
                    s.lines.append('%s%sif (%s) {' % (ind, '' if n == 0 else '} else ', s.X(c, 1)))
                    s.seq(ind + '  ', b)
                if st[2] is not None:
                    s.lines.append('%s} else {' % ind); s.seq(ind + '  ', st[2])
                s.lines.append('%s}' % ind)
            elif st[0] == 'case':
                sw = s.W(st[1])
                first = True
                for ch, b in st[2]:
                    if ch is None:
                        cond = '1'
                    else:
                        cond = ' || '.join('(%s == %s)' % (s.X(st[1], sw), s.X(c, sw)) for c in ch)
                    s.lines.append('%s%sif (%s) {' % (ind, '' if first else '} else ', cond))
                    first = False
                    s.seq(ind + '  ', b)
                s.lines.append('%s}' % ind)

ZSET = set()

def emit(flat, prefix, out_path, top_ent):
    global ZSET
    em = Em(flat, prefix)
    # first pass to learn which signals are tri-state
    for st in flat.stmts:
        pass
    def find_z(e):
        if e[0] == 'lit' and e[1] == 'Z': return True
        if e[0] == 'others' and e[1][0] == 'lit' and e[1][1] == 'Z': return True
        return False
    def scan(stmts):
        for st in stmts:
            if st[0] == 'assign':
                if any(find_z(v) for v, c in st[2]): ZSET.add(st[1][1])
            elif st[0] == 'process': scan(st[1])
            elif st[0] == 'if':
                for c, b in st[1]: scan(b)
                if st[2]: scan(st[2])
            elif st[0] == 'case':
                for ch, b in st[2]: scan(b)
    scan(flat.stmts)
    # emit the delta function
    for st in flat.stmts:
        if st[0] == 'assign':
            if len(st[2]) == 1 and st[2][0][1] is None: em.assign('  ', st[1], st[2], 'conc')
            else:
                em.lines.append('  {'); em.assign('    ', st[1], st[2], 'conc'); em.lines.append('  }')
        elif st[0] == 'process':
            em.lines.append('  {'); em.seq('    ', st[1]); em.lines.append('  }')
    n = len(flat.order)
    o = []
    o.append('/* %s - GENERATED by tools/vhdl2c.py from frozen-signal/Apple_IIe_MMU_IOU (CC0), do not edit. */' % out_path.split('/')[-1])
    o.append('#include <stdint.h>\n#include <string.h>\n#include <stdbool.h>\n')
    o.append('#define %s_NSIG %d' % (prefix.upper(), n))
    for i, name in enumerate(flat.order):
        if flat.sigs[name][2] == 'int' and '__' in name: continue
    top_ports = [(pn, d, w) for pn, d, w in TOP_PORTS]
    o.append('typedef struct { uint32_t c[%d], l[%d], z[%d]; } %s_t;' % (n, n, n, prefix))
    for pn, d, w in top_ports:
        o.append('#define %s_%s %d   /* %s, %d bit%s */' % (prefix.upper(), pn.upper(), em.idx[pn], d, w, 's' if w > 1 else ''))
    o.append('static const uint32_t %s_init[%d] = {%s};' % (prefix, n, ', '.join(str(flat.sigs[x][1]) for x in flat.order)))
    o.append('static const uint8_t %s_zcap[%d] = {%s};' % (prefix, n, ', '.join('1' if x in ZSET else '0' for x in flat.order)))
    o.append('static void %s_reset(%s_t *s) { memcpy(s->c, %s_init, sizeof(s->c)); memcpy(s->l, %s_init, sizeof(s->l)); memset(s->z, 0, sizeof(s->z)); }' % (prefix, prefix, prefix, prefix))
    o.append('/* one delta: returns true if some signal changed */')
    o.append('static bool %s_delta(%s_t *s) {' % (prefix, prefix))
    o.append('  uint32_t *c = s->c, *l = s->l, nx[%d], z[%d];' % (n, n))
    o.append('  memcpy(nx, c, sizeof(nx)); memset(z, 0, sizeof(z));')
    o.extend(em.lines)
    o.append('  bool ch = false;')
    o.append('  for (int i = 0; i < %d; i++) { l[i] = c[i]; if (nx[i] != c[i]) { c[i] = nx[i]; ch = true; } }' % n)
    o.append('  memcpy(s->z, z, sizeof(z));')
    o.append('  return ch;')
    o.append('}')
    o.append('/* run deltas until nothing changes (at most 64); returns the number of deltas */')
    o.append('static int %s_run(%s_t *s) {' % (prefix, prefix))
    o.append('  int k = 0;')
    o.append('  while (k < 64 && %s_delta(s)) k++;' % prefix)
    o.append('  memcpy(s->l, s->c, sizeof(s->l));')
    o.append('  return k;')
    o.append('}')
    open(out_path, 'w').write('\n'.join(o) + '\n')
    print(out_path, n, 'signals,', len(flat.stmts), 'statements, tri-state:', ', '.join(sorted(ZSET)) or '-')

TOP_PORTS = []

def main():
    args = sys.argv[1:]
    gen = {}
    while '-G' in args:
        i = args.index('-G'); k, v = args[i + 1].split('='); gen[k.lower()] = int(v); del args[i:i + 2]
    top, prefix, out = args[:3]
    files = args[3:]
    ents, archs = {}, {}
    for f in files: parse_file(open(f).read(), ents, archs)
    flat = Flat()
    scope = instantiate(flat, ents, archs, top.lower(), '', [], gen)
    for n, d, w in ents[top.lower()].ports:
        TOP_PORTS.append((n, d, w))
    emit(flat, prefix, out, top.lower())

if __name__ == '__main__':
    main()
