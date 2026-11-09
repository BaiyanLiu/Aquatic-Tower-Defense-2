# Finds groups of model parts that float apart from the rest of their model (in the in-game pose of animated pivots).
# Usage: python Tools/model_gap_check.py [tolerance_uu]   (needs numpy). Many hits are deliberate: bubbles, sparks, shadow discs, juggled balls, orbiting fish, the hovering jelly and lionfish.
import re, glob, sys, math
import numpy as np

base = r"C:\Users\Baiyan\Documents\Unreal Projects\AquaticTowerDefense\Source\AquaticTowerDefense\TowerDefense"
num = r"(-?[\d.]+)f?"
TOL = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5

def qmat(q):
    x, y, z, w = q
    n = math.sqrt(x*x+y*y+z*z+w*w) or 1
    x, y, z, w = x/n, y/n, z/n, w/n
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])

def floats(s):
    return [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?(?:e-?\d+)?", s)]

models = {}
for f in sorted(glob.glob(base + r"\TDModelData*.cpp")):
    text = open(f, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"const FTDModelPivot (\w+)Pivots\[\] =\s*\{(.*?)\n\t\};", text, re.S):
        name = m.group(1)
        piv = []
        for line in m.group(2).splitlines():
            pm = re.search(r'TEXT\("(\w+)"\),\s*(-?\d+),(.*)', line)
            if pm:
                v = floats(pm.group(3))
                piv.append((pm.group(1), int(pm.group(2)), v[0:3], v[3:7]))
        pm2 = re.search(r"const FTDModelPart %sParts\[\] =\s*\{(.*?)\n\t\};" % name, text, re.S)
        parts = []
        for i, line in enumerate(pm2.group(1).splitlines()):
            line = line.split("//")[0]
            if "{" not in line:
                continue
            v = floats(line)
            parts.append(dict(pivot=int(v[0]), shape=int(v[1]), mat=int(v[2]), flags=int(v[3]), L=v[4:7], S=v[7:10], Q=v[10:14], C=v[14:18]))
        models[name] = (piv, parts)

MINSIZE = {}
creatures = ["Crab", "Piranha", "Turtle", "Manta", "Kraken", "SeaStar", "Cuttlefish", "ShockEel"]

def rot(axis, a):
    c, s = math.cos(a), math.sin(a)
    if axis == 'x': return np.array([[1,0,0],[0,c,-s],[0,s,c]])
    if axis == 'y': return np.array([[c,0,s],[0,1,0],[-s,0,c]])
    return np.array([[c,-s,0],[s,c,0],[0,0,1]])
def POSE(name, pn, R, tc):
    if pn == 'Club': return rot('y', math.radians(-118))
    if pn == 'Tail' and name.startswith('Humpback'): return rot('y', -0.6)
    if pn == 'Tentacle' and name.startswith('Lionfish'):
        side = -1 if tc[0] == 0 else 1; tc[0] += 1
        return rot('x', math.radians(side*-14)) @ rot('z', math.radians(side*-58))
    return R
rng = np.random.default_rng(1)
def samples(shape, n=400):
    # points in a unit (-0.5..0.5) shape, volume and surface
    p = rng.uniform(-0.5, 0.5, (n*4, 3))
    if shape == 1:
        p = p[np.linalg.norm(p, axis=1) <= 0.5]
    elif shape == 2:
        p = p[np.linalg.norm(p[:, :2], axis=1) <= 0.5]
    elif shape == 3:
        r = 0.5 * (0.5 - p[:, 2])
        p = p[np.linalg.norm(p[:, :2], axis=1) <= r]
    elif shape == 4:
        p[:, 2] = 0
    return p[:n]

def inside(shape, local, half, tol):
    # local: points in part frame (uu). Inflate by tol.
    h = np.maximum(half + tol, 1e-3)
    u = local / (2*h)   # into unit -0.5..0.5
    if shape == 0 or shape == 4:
        return np.all(np.abs(u) <= 0.5, axis=1)
    if shape == 1:
        return np.linalg.norm(u, axis=1) <= 0.5
    if shape == 2:
        return (np.linalg.norm(u[:, :2], axis=1) <= 0.5) & (np.abs(u[:, 2]) <= 0.5)
    if shape == 3:
        r = 0.5 * (0.5 - u[:, 2])
        return (np.linalg.norm(u[:, :2], axis=1) <= r + tol/(2*h[0])) & (np.abs(u[:, 2]) <= 0.5)
    return np.zeros(len(local), bool)

for name, (piv, parts) in models.items():
    minsize = 3.0 if name in creatures else (0.0 if name.startswith(("Shot", "Drop")) else 2.5)
    # pivot world transforms
    PW = []
    tcount = [0]
    for (pn, par, L, Q) in piv:
        R = qmat(Q); T = np.array(L)
        R = POSE(name, pn, R, tcount)
        if par >= 0:
            PR, PT = PW[par]
            R = PR @ R; T = PR @ T + PT
        PW.append((R, T))
    P = []
    for i, p in enumerate(parts):
        if max(p["S"]) < minsize:
            continue
        PR, PT = PW[p["pivot"]]
        R = PR @ qmat(p["Q"]); T = PR @ np.array(p["L"]) + PT
        S = np.array(p["S"])
        pts = samples(p["shape"]) * S
        world = pts @ R.T + T
        P.append(dict(i=i, p=p, R=R, T=T, half=S/2, world=world, vol=float(np.prod(S))))
    n = len(P)
    adj = [set() for _ in range(n)]
    for a in range(n):
        for b in range(a+1, n):
            A, B = P[a], P[b]
            if np.linalg.norm(A["T"]-B["T"]) > (np.linalg.norm(A["half"]) + np.linalg.norm(B["half"]) + TOL*2):
                continue
            loc = (A["world"] - B["T"]) @ B["R"]
            if inside(B["p"]["shape"], loc, B["half"], TOL).any():
                adj[a].add(b); adj[b].add(a); continue
            loc = (B["world"] - A["T"]) @ A["R"]
            if inside(A["p"]["shape"], loc, A["half"], TOL).any():
                adj[a].add(b); adj[b].add(a)
    seen = [-1]*n; comps = []
    for s in range(n):
        if seen[s] >= 0: continue
        stack = [s]; seen[s] = len(comps); c = []
        while stack:
            k = stack.pop(); c.append(k)
            for j in adj[k]:
                if seen[j] < 0:
                    seen[j] = len(comps); stack.append(j)
        comps.append(c)
    comps.sort(key=lambda c: -sum(P[k]["vol"] for k in c))
    main = sum(P[k]["vol"] for k in comps[0])
    for c in comps[1:]:
        vol = sum(P[k]["vol"] for k in c)
        biggest = max(c, key=lambda k: P[k]["vol"])
        bp = P[biggest]
        # nearest gap to anything else
        print(f"{name}: {len(c)} part(s) vol {vol:.0f} ({100*vol/main:.2f}% of main), biggest #{bp['i']} shape {bp['p']['shape']} mat {bp['p']['mat']} pivot {piv[bp['p']['pivot']][0]} at {np.round(bp['T'],1).tolist()} size {bp['p']['S']}")
