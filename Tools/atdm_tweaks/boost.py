"""Make the Eel towers' lights and the Mantis towers' antennal scales far more obvious, editing the baked .atdm files in place.
Usage: python boost.py <src dir> <dst dir>"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import atdm

SRC, DST = sys.argv[1], sys.argv[2]

# ---- Eel lights
DOT_GLOW = 1.9       # HDR multiplier for the body lights, bulbs and arcs (unlit, so above 1 blooms)
BASE_GLOW = 1.8      # the glowing pads on the rock base
SATURATE = 1.8       # push the hue away from white so the brighter light keeps its colour
DOT_SCALE = 1.8      # body dots grow this much
BULB_SCALE = 1.45    # lamp bulbs inside the glass domes
DENSE_SCALE = 1.3    # dots of a look with many of them
TIP_SCALE = 1.4      # Overload's antenna balls
HALO_SCALE = 1.5    # a soft see-through shell of the light's colour around each body dot
HALO_OPACITY = 0.18
SPHERE_SEGS = ((20, 14), (14, 10), (10, 7))   # the round dots and halos per level of detail (the baked dots were octagons;
                                              # grown, their facets showed at game zoom)
DOME_OPACITY = 0.5   # the lamp domes take the bulb's colour, so the whole globe glows


def is_eye_glint(c):
    return abs(c[0] - 0.95) < 0.02 and abs(c[1] - 0.97) < 0.02 and abs(c[2] - 1.0) < 0.02


def light(c, f):
    m = (c[0] + c[1] + c[2]) / 3.0
    return [max(0.03, m + (c[k] - m) * SATURATE) * f for k in range(3)]


def sphere(centre, radius, look, rgb, segs):
    """a smooth UV sphere: positions, packed normals, colours (alpha = look), triangles (indices from 0)"""
    nu, nv = segs
    th = np.linspace(0, np.pi, nv + 1)
    ph = np.linspace(0, 2 * np.pi, nu, endpoint=False)
    T, P = np.meshgrid(th, ph, indexing='ij')
    n = np.stack([np.sin(T) * np.cos(P), np.sin(T) * np.sin(P), np.cos(T)], -1).reshape(-1, 3)
    pos = (centre + n * radius).astype(np.float32)
    nrm = np.concatenate([np.round(n * 127), np.zeros((len(n), 1))], 1).astype(np.int8)
    col = np.tile(np.array([*rgb, look], np.uint8), (len(n), 1))
    tris = []
    for i in range(nv):
        for j in range(nu):
            a, b = i * nu + j, i * nu + (j + 1) % nu
            c, d = (i + 1) * nu + j, (i + 1) * nu + (j + 1) % nu
            tris += [[a, c, b], [b, c, d]]
    return pos, nrm, col, np.array(tris, np.uint32)


def eel(name):
    ms = atdm.read(f'{SRC}/{name}.atdm')
    solid = {m.pivot: m for m in ms if m.solid}
    for m in ms:
        if m.solid:
            continue
        orig = [list(lk['color']) for lk in m.looks]
        for li, lk in enumerate(m.looks):
            if lk['mat'] == 1 and not is_eye_glint(lk['color']):
                f = BASE_GLOW if (m.pivot == 0 and li == 0) else DOT_GLOW
                lk['color'] = light(orig[li], f) + [lk['color'][3]]
        if m.pivot == 0:
            # glass domes over the bulbs glow in the bulb's colour
            bulb = next((li for li, lk in enumerate(m.looks) if lk['mat'] == 1 and li != 0), None)
            for li, lk in enumerate(m.looks):
                if lk['mat'] == 2 and bulb is not None:
                    lk['color'] = light(orig[bulb], 1.2) + [DOME_OPACITY]
        halo_look = {}
        for lod_i, L in enumerate(m.lods):
            lab = atdm.components(L)
            S = solid[m.pivot].lods[min(lod_i, len(solid[m.pivot].lods) - 1)] if m.pivot in solid else None
            add_pos, add_nrm, add_col, add_idx = [], [], [], []
            # a look with many dots (Overload's charge rings) grows less, or the dots run together
            per_look = {}
            for cid in np.unique(lab):
                sel = lab == cid
                lk_i = int(np.bincount(L.col[sel][:, 3]).argmax())
                per_look[lk_i] = per_look.get(lk_i, 0) + 1
            base = len(L.pos)
            drop = np.zeros(len(L.pos), bool)
            for cid in np.unique(lab):
                sel = lab == cid
                look = int(np.bincount(L.col[sel][:, 3]).argmax())
                lk = m.looks[look]
                if lk['mat'] != 1 or is_eye_glint(lk['color']):
                    continue
                P = L.pos[sel]
                c = P.mean(0)
                ext = (P.max(0) - P.min(0)).max()
                segs = SPHERE_SEGS[min(lod_i, len(SPHERE_SEGS) - 1)]
                rgb = L.col[sel][0, :3]

                def replace(centre, radius, halo_r=None):
                    # drop the old faceted shape's triangles, add a round one (and its halo)
                    nonlocal drop
                    drop |= sel
                    for rr, lk_i in ((radius, look), (halo_r, halo_look.get(look))):
                        if rr is None:
                            continue
                        sp, sn, sc, st = sphere(centre, rr, lk_i, rgb, segs)
                        off = base + sum(len(x) for x in add_pos)
                        add_pos.append(sp); add_nrm.append(sn); add_col.append(sc); add_idx.append(st + off)

                if m.pivot == 0 and look != 0 and 5.5 < ext < 8.5:
                    replace(c, ext * 0.5 * BULB_SCALE)      # bulb in its dome
                elif m.pivot == 1 and ext < 3.6 and S is not None:
                    # a body dot: grow it and sit it proud of the skin, along the nearest skin normal
                    d = np.linalg.norm(S.pos - c, axis=1)
                    k = d.argmin()
                    n = S.nrm[k, :3].astype(np.float32)
                    n /= max(np.linalg.norm(n), 1e-6)
                    dense = per_look.get(look, 0) > 24
                    grow = DENSE_SCALE if dense else DOT_SCALE
                    r = ext * 0.5 * grow
                    nc = S.pos[k] + n * (r * 0.45)
                    if not dense and look not in halo_look:
                        halo_look[look] = len(m.looks)
                        m.looks.append({'color': light(orig[look], 1.4) + [HALO_OPACITY], 'mat': 2, 'flags': 0, 'vert': 0, 'pad': 0})
                    replace(nc, r, None if dense else r * HALO_SCALE)
                elif m.pivot == 1 and 4.0 < ext < 5.5:
                    replace(c, ext * 0.5 * TIP_SCALE)       # antenna ball
            L.idx = L.idx[~drop[L.idx].any(axis=1)]
            if add_pos:
                L.pos = np.concatenate([L.pos] + add_pos).astype(np.float32)
                L.nrm = np.concatenate([L.nrm] + add_nrm)
                L.col = np.concatenate([L.col] + add_col)
                L.idx = np.concatenate([L.idx] + add_idx)
    atdm.write(f'{DST}/{name}.atdm', ms)
    print('wrote', name, [(round(lk['color'][0], 2), round(lk['color'][1], 2), round(lk['color'][2], 2), round(lk['color'][3], 2), lk['mat']) for m in ms if not m.solid for lk in m.looks])


# ---- Mantis antennal scales (an orange paddle with a coloured spot either side of the face, pivot 1)
SCALE_XY = 2.4
SCALE_Z = 1.5
LIFT = 1.5


def mantis(name):
    ms = atdm.read(f'{SRC}/{name}.atdm')
    for m in ms:
        if not (m.solid and m.pivot == 1):
            continue
        for L in m.lods:
            lab = atdm.components(L)
            done = 0
            for side in (-1, 1):
                group = np.zeros(len(L.pos), bool)
                for cid in np.unique(lab):
                    sel = lab == cid
                    P = L.pos[sel]
                    lo, hi = P.min(0), P.max(0)
                    ctr = (lo + hi) * 0.5
                    if (hi - lo).max() > 12 or np.sign(ctr[1]) != side:
                        continue
                    paddle = 25.5 < lo[0] < 27 and 35 < hi[0] < 36.5 and 3.5 < abs(ctr[1]) < 8.5 and 48 < lo[2] < 49.5 and 52 < hi[2] < 53
                    spot = 32.5 < lo[0] < 34.5 and 36 < hi[0] < 37.5 and 7.5 < abs(ctr[1]) < 9.5 and 50.5 < ctr[2] < 53
                    if paddle or spot:
                        group |= sel
                if not group.any():
                    continue
                # hinge at the paddle's inner end against the head
                anchor = np.array([26.1, side * 4.0, 50.5], np.float32)
                s = np.array([SCALE_XY, SCALE_XY, SCALE_Z], np.float32)
                L.pos[group] = anchor + (L.pos[group] - anchor) * s + np.array([0, 0, LIFT], np.float32)
                nrm = L.nrm[group, :3].astype(np.float32) / s
                nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-6)
                L.nrm[group, :3] = np.clip(np.round(nrm * 127), -127, 127).astype(np.int8)
                done += 1
            assert done == 2, (name, done)
    atdm.write(f'{DST}/{name}.atdm', ms)
    print('wrote', name)


for n in ('Eel', 'EelStorm', 'EelOverload'):
    eel(n)
# the face paddles stay as baked (user wanted the back plates instead, see mantis_back.py)
