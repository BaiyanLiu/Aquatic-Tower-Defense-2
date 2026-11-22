"""Cover the Mantis towers' backs (the abdomen) with overlapping scales into the sculpted body so they read clearly.
Usage: python mantis_back.py <src dir> <dst dir>   (src holds the untouched Mantis*.atdm)"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import atdm

SRC, DST = sys.argv[1], sys.argv[2]

# abdomen plate centres and widths (pivot 1, from the Mantis part tables; the same in all three forms)
CENTRES = np.array([[-38.0, 0, 10.0], [-33.88, 0, 11.94], [-28.0, 0, 15.0], [-21.25, 0, 19.06], [-14.0, 0, 24.0],
                    [-6.88, 0, 29.81], [0.0, 0, 36.0], [6.88, 0, 42.75], [12.0, 0, 48.0]], np.float32)
HALF_WIDTH = np.array([8.7, 9.5, 10.2, 11.0, 11.7, 12.5, 13.2, 14.0, 14.7], np.float32)

SUBDIV = (1, 1, 2)   # extra splits of the back's triangles per level of detail
LOD_LOOK = ((1.0, 1.0, 3), (0.8, 1.0, 3), (0.6, 1.0, 2))    # relief, colour, smoothing per level of detail: the coarsest
                                                         # level (fully zoomed out) is split twice and keeps flatter scales: unsplit, its few vertices smeared them into blotches
RAD = 6.5          # scale length along the body
ROW = 5.0          # rows overlap: closer than a scale is long
COL = 8.6          # scale width around the body
AMP = 1.5          # relief
RIM_DARK = 0.55     # vertex colour along each scale's free edge
MID_LIGHT = 1.15

seg_len = np.linalg.norm(np.diff(CENTRES, axis=0), axis=1)
arc = np.concatenate([[0], np.cumsum(seg_len)])
# plate k spans from the boundary behind it to the one in front of it
bounds = np.concatenate([[arc[0] - seg_len[0] * 0.5], (arc[:-1] + arc[1:]) * 0.5])   # rear boundary of plate k
fronts = np.concatenate([bounds[1:], [arc[-1]]])
LAST = len(CENTRES) - 2   # the last plate runs into the head: stop before it


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def project(P):
    """arc length along the abdomen, the radial vector and the local half width for each point"""
    best_d = np.full(len(P), np.inf)
    t = np.zeros(len(P)); rad = np.zeros_like(P); hw = np.zeros(len(P))
    for i in range(len(CENTRES) - 1):
        a, b = CENTRES[i], CENTRES[i + 1]
        ab = b - a
        u = np.clip(((P - a) @ ab) / (ab @ ab), 0, 1)
        q = a + u[:, None] * ab
        r = P - q
        d = np.linalg.norm(r, axis=1)
        m = d < best_d
        best_d[m] = d[m]; t[m] = arc[i] + u[m] * seg_len[i]; rad[m] = r[m]
        hw[m] = HALF_WIDTH[i] + u[m] * (HALF_WIDTH[i + 1] - HALF_WIDTH[i])
    # behind the first centre: extend straight back
    a, b = CENTRES[0], CENTRES[1]
    ab = (b - a) / np.linalg.norm(b - a)
    s = (P - a) @ ab
    m = s < 0
    t[m] = s[m]; rad[m] = (P - a)[m] - s[m][:, None] * ab; hw[m] = HALF_WIDTH[0]
    return t, rad, hw


def recompute_normals(L):
    key = np.round(L.pos * 1000).astype(np.int64)
    _, weld = np.unique(key, axis=0, return_inverse=True)
    weld = weld.ravel()
    tri = L.pos[L.idx]
    fn = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    acc = np.zeros((weld.max() + 1, 3))
    for c in range(3):
        np.add.at(acc, weld[L.idx[:, c]], fn)
    n = acc[weld]
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)


def subdivide(L, region):
    """splits each triangle touching the region into four, so the scales have enough vertices to show"""
    tris = L.idx
    hit = region[tris].any(axis=1)
    keep, split = tris[~hit], tris[hit]
    mids = {}
    pos, nrm, col = [L.pos], [L.nrm], [L.col]
    nxt = [len(L.pos)]

    def mid(a, b):
        key = (a, b) if a < b else (b, a)
        if key not in mids:
            mids[key] = nxt[0]; nxt[0] += 1
            pos.append(((L.pos[a] + L.pos[b]) * 0.5)[None])
            n = L.nrm[a, :3].astype(np.float64) + L.nrm[b, :3]
            n = n / max(np.linalg.norm(n), 1e-6) * 127
            nrm.append(np.array([[*np.round(n), 0]], np.int8))
            c = (L.col[a].astype(int) + L.col[b]) // 2
            c[3] = L.col[a, 3]
            col.append(c[None].astype(np.uint8))
        return mids[key]

    out = [keep]
    for a, b, c in split:
        ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
        out.append(np.array([[a, ab, ca], [ab, b, bc], [ca, bc, c], [ab, bc, ca]], np.uint32))
    L.pos = np.concatenate(pos).astype(np.float32)
    L.nrm = np.concatenate(nrm).astype(np.int8)
    L.col = np.concatenate(col).astype(np.uint8)
    L.idx = np.concatenate(out).astype(np.uint32)


def region_of(L):
    t, rad, hw = project(L.pos.astype(np.float64))
    r = np.linalg.norm(rad, axis=1)
    return (t > bounds[0] - 3) & (t < fronts[LAST] + 3) & (r / hw < 1.5) & (r / hw > 0.4)


def neighbour_mean(L, values):
    """each vertex's mean of its neighbours' values (welded by position, so seams don't split it)"""
    key = np.round(L.pos * 1000).astype(np.int64)
    _, weld = np.unique(key, axis=0, return_inverse=True)
    weld = weld.ravel()
    e = np.concatenate([L.idx[:, [0, 1]], L.idx[:, [1, 2]], L.idx[:, [2, 0]]])
    a, b = weld[e[:, 0]], weld[e[:, 1]]
    nw = weld.max() + 1
    wv = np.zeros((nw, values.shape[1])); np.add.at(wv, weld, values); cnt0 = np.bincount(weld, minlength=nw)[:, None]
    wv /= np.maximum(cnt0, 1)
    acc = np.zeros_like(wv); cnt = np.zeros((nw, 1))
    np.add.at(acc, a, wv[b]); np.add.at(acc, b, wv[a]); np.add.at(cnt, a, 1); np.add.at(cnt, b, 1)
    return (acc / np.maximum(cnt, 1))[weld]


def carve(L, relief=1.0, colour=1.0, smoothing=3):
    t, rad, hw = project(L.pos.astype(np.float64))
    r = np.linalg.norm(rad, axis=1)
    k = np.clip(np.searchsorted(bounds, t, side='right') - 1, 0, len(bounds) - 1)
    f = (t - bounds[k]) / (fronts[k] - bounds[k])
    axis = (CENTRES[-1] - CENTRES[0]); axis = axis / np.linalg.norm(axis)
    # where: on the abdomen (not the head), near the shell surface, top and sides rather than the belly
    axis = (CENTRES[-1] - CENTRES[0]); axis /= np.linalg.norm(axis)
    up = np.array([0, 0, 1.0]) - axis[2] * axis; up /= np.linalg.norm(up)
    rdir = rad / np.maximum(r[:, None], 1e-6)
    w = smooth(-0.55, -0.05, rdir @ up)
    w *= smooth(bounds[0] - 1, bounds[0] + 2, t) * (1 - smooth(fronts[LAST] - 2, fronts[LAST] + 1.5, t))
    w *= 1 - smooth(1.15, 1.45, r / hw)          # legs and claws that melt into the body stay as they are
    w *= smooth(0.45, 0.7, r / hw)
    w[(t < bounds[0] - 1) | (t > fronts[LAST] + 1.5)] = 0
    # first smooth away the shell's small bumps under the scales: at game zoom they and the scales together read as noise
    for _ in range(smoothing):
        P = L.pos.astype(np.float64)
        L.pos = (P + (neighbour_mean(L, P) - P) * (0.7 * w)[:, None]).astype(np.float32)
        N = L.nrm[:, :3].astype(np.float64)
        N = N + (neighbour_mean(L, N) - N) * (0.8 * w)[:, None]
        N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-6)
        L.nrm[:, :3] = np.clip(np.round(N * 127), -127, 127).astype(np.int8)
    # overlapping scales: rows of rounded scales, each row shifted half a scale; the scale nearer the head lies on top,
    # its free rounded edge pointing back over the next row (like a fish's)
    side = np.cross(axis, up)
    ang = np.arctan2(rdir @ side, rdir @ up)
    U = ang * hw                     # around the body
    V = t                            # along it, towards the head

    def scales(u, v):
        best_cv = np.full(len(u), -np.inf); h = np.zeros(len(u)); edge = np.zeros(len(u))
        j0 = np.floor(v / ROW).astype(int)
        for dj in (-1, 0, 1, 2):
            j = j0 + dj
            off = (j % 2) * 0.5 * COL
            i0 = np.round((u - off) / COL).astype(int)
            for di in (-1, 0, 1):
                cu = (i0 + di) * COL + off
                cv = j * ROW
                du = (u - cu) / (COL * 0.62)
                dv = (v - cv) / RAD
                # a scale is a round lobe whose free edge faces the tail; it starts under the scale in front
                d = np.sqrt(du * du + np.where(dv < 0, dv * dv, (dv / 1.8) ** 2))
                cover = (d < 1.0) & (cv > best_cv)
                hh = (0.35 + 0.65 * np.clip(0.5 - dv * 0.5, 0, 1)) * (1 - smooth(0.82, 1.0, d))
                h = np.where(cover, hh, h); edge = np.where(cover, d, edge); best_cv = np.where(cover, cv, best_cv)
        return h, edge

    h, edge = scales(U, V)
    n = L.nrm[:, :3].astype(np.float64)
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-6)
    disp = AMP * relief * (h - 0.5) * w
    L.pos = (L.pos + n * disp[:, None]).astype(np.float32)
    # tilt the smooth normal by the scale's slope (finite differences of the same pattern)
    e = 0.25
    dh_dv = (scales(U, V + e)[0] - scales(U, V - e)[0]) / (2 * e) * AMP * relief * w
    dh_du = (scales(U + e, V)[0] - scales(U - e, V)[0]) / (2 * e) * AMP * relief * w
    tv = axis - (n @ axis)[:, None] * n
    tu = np.cross(n, tv)
    tu /= np.maximum(np.linalg.norm(tu, axis=1, keepdims=True), 1e-6)
    tn = n - dh_dv[:, None] * tv - dh_du[:, None] * tu
    tn /= np.maximum(np.linalg.norm(tn, axis=1, keepdims=True), 1e-6)
    # colour: a dark rim along each scale's free edge, a lighter middle
    rim = smooth(0.62, 0.92, edge)
    shade = (1 - (1 - RIM_DARK) * rim) * (1 + (MID_LIGHT - 1) * (1 - smooth(0.2, 0.6, edge)))
    shade = 1 + (shade - 1) * w * colour
    rgb = L.col[:, :3].astype(np.float64) * shade[:, None]
    L.col[:, :3] = np.clip(np.round(rgb), 0, 255).astype(np.uint8)
    m = w > 0.001
    L.nrm[m, :3] = np.clip(np.round(tn[m] * 127), -127, 127).astype(np.int8)
    return int(m.sum())


for name in ('Mantis', 'MantisSmasher', 'MantisSpearer'):
    ms = atdm.read(f'{SRC}/{name}.atdm')
    for m in ms:
        if m.solid and m.pivot == 1:
            for li, L in enumerate(m.lods):
                for _ in range(SUBDIV[li] if li < len(SUBDIV) else 0):
                    subdivide(L, region_of(L))
            print(name, [carve(L, *LOD_LOOK[min(li, len(LOD_LOOK) - 1)]) for li, L in enumerate(m.lods)], [len(L.pos) for L in m.lods])
    atdm.write(f'{DST}/{name}.atdm', ms)
