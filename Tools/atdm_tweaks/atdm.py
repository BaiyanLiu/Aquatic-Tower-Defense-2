"""Read / write ATDM baked model files (format: TDModels.cpp ReadBakedModel)."""
import struct, zlib
import numpy as np


class Lod:
    pass


class Mesh:
    pass


def read(path):
    f = open(path, 'rb').read()
    assert f[:4] == b'ATDM'
    ver, raw = struct.unpack_from('<II', f, 4)
    d = zlib.decompress(f[12:])
    assert len(d) == raw
    at = 0

    def take(fmt):
        nonlocal at
        v = struct.unpack_from(fmt, d, at)
        at += struct.calcsize(fmt)
        return v

    def arr(dtype, n):
        nonlocal at
        a = np.frombuffer(d, dtype=dtype, count=n, offset=at).copy()
        at += a.nbytes
        return a

    meshes = []
    (n,) = take('<H')
    for _ in range(n):
        m = Mesh()
        m.pivot, m.solid, nl, nlooks = take('<BBBB')
        m.looks = []
        for _ in range(nlooks):
            col = list(take('<4f'))
            mat, flags, vert, pad = take('<4B')
            m.looks.append({'color': col, 'mat': mat, 'flags': flags, 'vert': vert, 'pad': pad})
        (npal,) = take('<B')
        m.palette = [list(take('<3f')) for _ in range(npal)]
        m.lods = []
        for _ in range(nl):
            L = Lod()
            nv, ni = take('<II')
            L.pos = arr('<f4', nv * 3).reshape(nv, 3)
            L.nrm = arr('i1', nv * 4).reshape(nv, 4)
            L.col = arr('u1', nv * 4).reshape(nv, 4)
            L.idx = arr('<u4', ni).reshape(-1, 3)
            m.lods.append(L)
        meshes.append(m)
    assert at == len(d), (at, len(d))
    return meshes


def write(path, meshes):
    out = bytearray()
    out += struct.pack('<H', len(meshes))
    for m in meshes:
        out += struct.pack('<BBBB', m.pivot, m.solid, len(m.lods), len(m.looks))
        for lk in m.looks:
            out += struct.pack('<4f', *lk['color'])
            out += struct.pack('<4B', lk['mat'], lk['flags'], lk['vert'], lk['pad'])
        out += struct.pack('<B', len(m.palette))
        for p in m.palette:
            out += struct.pack('<3f', *p)
        for L in m.lods:
            out += struct.pack('<II', len(L.pos), L.idx.size)
            out += L.pos.astype('<f4').tobytes()
            out += L.nrm.astype('i1').tobytes()
            out += L.col.astype('u1').tobytes()
            out += L.idx.astype('<u4').tobytes()
    raw = bytes(out)
    open(path, 'wb').write(b'ATDM' + struct.pack('<II', 1, len(raw)) + zlib.compress(raw, 9))


def components(L, mask=None):
    """Connected components (by shared vertex index, and by coincident positions) of a LOD; returns label per vertex."""
    nv = len(L.pos)
    parent = np.arange(nv)

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    for t in L.idx:
        union(t[0], t[1]); union(t[1], t[2])
    # weld coincident positions
    key = np.round(L.pos * 1000).astype(np.int64)
    seen = {}
    for i, k in enumerate(map(tuple, key)):
        if k in seen:
            union(i, seen[k])
        else:
            seen[k] = i
    return np.array([find(i) for i in range(nv)])
