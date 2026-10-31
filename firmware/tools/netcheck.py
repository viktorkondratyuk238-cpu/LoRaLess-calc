import re, math, sys
from collections import defaultdict

src = open('lora-calculator_jlcpcb.kicad_sch', encoding='utf-8').read()

# ---- tiny s-expression parser ----
def parse(s):
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|\(|\)|[^\s()"]+', s)
    stack = [[]]
    for t in tokens:
        if t == '(':
            stack.append([])
        elif t == ')':
            node = stack.pop()
            stack[-1].append(node)
        elif t.startswith('"'):
            stack[-1].append(t[1:-1].replace('\\"', '"'))
        else:
            stack[-1].append(t)
    return stack[0][0]

tree = parse(src)

def children(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]

def first(node, name):
    r = children(node, name)
    return r[0] if r else None

def key(x, y):
    return (round(x, 2), round(y, 2))

# ---- library pins: lib_id -> list of (x, y, angle, pin_number) in library coords (y up) ----
libpins = {}
for ls in children(first(tree, 'lib_symbols'), 'symbol'):
    lib_id = ls[1]
    pins = []
    def walk(n):
        for c in n:
            if isinstance(c, list) and c:
                if c[0] == 'pin':
                    at = first(c, 'at'); num = first(c, 'number')
                    pins.append((float(at[1]), float(at[2]), float(at[3]), num[1]))
                elif c[0] == 'symbol':
                    walk(c)
    walk(ls)
    libpins[lib_id] = pins

def rot(x, y, deg):
    r = math.radians(deg)
    return (x*math.cos(r) - y*math.sin(r), x*math.sin(r) + y*math.cos(r))

# ---- union-find over points ----
parent = {}
def find(p):
    parent.setdefault(p, p)
    while parent[p] != p:
        parent[p] = parent[parent[p]]
        p = parent[p]
    return p
def union(a, b):
    ra, rb = find(a), find(b)
    if ra != rb: parent[ra] = rb

wires = []
for w in children(tree, 'wire'):
    pts = first(w, 'pts')
    xy = [(float(p[1]), float(p[2])) for p in children(pts, 'xy')]
    if len(xy) == 2:
        a, b = key(*xy[0]), key(*xy[1])
        wires.append((a, b))
        union(a, b)

def on_segment(p, a, b):
    (px, py), (ax, ay), (bx, by) = p, a, b
    if abs(ax-bx) > 0.01 and abs(ay-by) > 0.01: return False   # only straight wires
    if min(ax,bx)-0.01 <= px <= max(ax,bx)+0.01 and min(ay,by)-0.01 <= py <= max(ay,by)+0.01:
        cross = (bx-ax)*(py-ay) - (by-ay)*(px-ax)
        return abs(cross) < 0.05
    return False

# ---- named nets from labels ----
label_pts = []   # (point, name)
for kind in ('label', 'global_label', 'hierarchical_label'):
    for l in children(tree, kind):
        at = first(l, 'at')
        p = key(float(at[1]), float(at[2]))
        label_pts.append((p, l[1]))
        find(p)

# ---- placed symbols: pins ----
pin_nodes = {}   # (ref, pin) -> point
symbols = []
for s in children(tree, 'symbol'):
    lib = first(s, 'lib_id')
    if not lib: continue
    lib_id = lib[1]
    at = first(s, 'at')
    sx, sy, sa = float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0
    ref = None; value = None
    for p in children(s, 'property'):
        if p[1] == 'Reference': ref = p[2]
        if p[1] == 'Value': value = p[2]
    if ref is None: continue
    if ref.startswith('#'): continue          # power symbol helper refs
    symbols.append((ref, value, lib_id))
    for (lx, ly, la, num) in libpins.get(lib_id, []):
        rx, ry = rot(lx, ly, sa)
        p = key(sx + rx, sy - ry)
        pin_nodes[(ref, num)] = p
        find(p)

# power symbols: net named by their Value
power_pts = []
for s in children(tree, 'symbol'):
    lib = first(s, 'lib_id')
    if not lib or not lib[1].startswith('power:'): continue
    at = first(s, 'at')
    value = None; ref = None
    for p in children(s, 'property'):
        if p[1] == 'Value': value = p[2]
        if p[1] == 'Reference': ref = p[2]
    sx, sy, sa = float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0
    for (lx, ly, la, num) in libpins.get(lib[1], []):
        rx, ry = rot(lx, ly, sa)
        p = key(sx + rx, sy - ry)
        find(p)
        power_pts.append((p, value))

# connect pins/labels that sit on a wire's straight run
all_pts = set(parent.keys())
for (a, b) in wires:
    for p in all_pts:
        if on_segment(p, a, b):
            union(p, a)

# ---- collect net names ----
net_name = {}
for p, name in label_pts + power_pts:
    r = find(p)
    net_name.setdefault(r, name)

def net_of(ref, pin):
    p = pin_nodes.get((ref, pin))
    if p is None: return '??(no pin)'
    r = find(p)
    return net_name.get(r, f'net@{r[0]},{r[1]}')

if __name__ == '__main__':
    print('placed symbols:', len(symbols))
    for ref, value, lib_id in sorted(symbols, key=lambda x: (x[0][0], int(re.sub(r'\D','',x[0]) or 0))):
        print(f'{ref:8s} {lib_id:40s} {value}')
