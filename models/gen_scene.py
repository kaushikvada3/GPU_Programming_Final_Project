#!/usr/bin/env python3
"""Emit a multi-shape scene OBJ for the ray tracer.

Shapes sit on a ground plane and each gets its own `usemtl` group so the loader's
named-material palette colors them. Flat shapes carry no normals (loader uses the
face normal); the sphere carries vertex normals for smooth shading.
"""
import math

V, N, F = [], [], []   # vertices, normals, faces: (mtl, [(vi, ni_or_None), ...])

def add_v(p):
    V.append(p)
    return len(V)          # 1-based OBJ index

def add_n(n):
    N.append(n)
    return len(N)

def quad(mtl, a, b, c, d):
    F.append((mtl, [(a, None), (b, None), (c, None), (d, None)]))

def tri(mtl, a, b, c):
    F.append((mtl, [(a, None), (b, None), (c, None)]))

def box(mtl, cx, cz, sx, sy, sz):
    x0, x1 = cx - sx/2, cx + sx/2
    z0, z1 = cz - sz/2, cz + sz/2
    y0, y1 = 0.0, sy
    v = [add_v((x, y, z)) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
    # index helper into the 2x2x2 grid above: [x][y][z]
    g = lambda ix, iy, iz: v[ix*4 + iy*2 + iz]
    quad(mtl, g(0,0,0), g(0,0,1), g(0,1,1), g(0,1,0))  # -x
    quad(mtl, g(1,0,1), g(1,0,0), g(1,1,0), g(1,1,1))  # +x
    quad(mtl, g(0,0,0), g(1,0,0), g(1,0,1), g(0,0,1))  # -y
    quad(mtl, g(0,1,1), g(1,1,1), g(1,1,0), g(0,1,0))  # +y
    quad(mtl, g(1,0,0), g(0,0,0), g(0,1,0), g(1,1,0))  # -z
    quad(mtl, g(0,0,1), g(1,0,1), g(1,1,1), g(0,1,1))  # +z

def tri_prism(mtl, cx, cz, w, h, depth):
    # triangular cross-section in the x-y plane, extruded along z
    x0, x1 = cx - w/2, cx + w/2
    z0, z1 = cz - depth/2, cz + depth/2
    apex_x = cx
    front = [add_v((x0, 0, z0)), add_v((x1, 0, z0)), add_v((apex_x, h, z0))]
    back  = [add_v((x0, 0, z1)), add_v((x1, 0, z1)), add_v((apex_x, h, z1))]
    tri(mtl, front[0], front[2], front[1])   # front cap
    tri(mtl, back[0],  back[1],  back[2])     # back cap
    quad(mtl, front[0], front[1], back[1], back[0])  # bottom
    quad(mtl, front[1], front[2], back[2], back[1])  # right slope
    quad(mtl, front[2], front[0], back[0], back[2])  # left slope

def pyramid(mtl, cx, cz, base, h):
    x0, x1 = cx - base/2, cx + base/2
    z0, z1 = cz - base/2, cz + base/2
    b = [add_v((x0,0,z0)), add_v((x1,0,z0)), add_v((x1,0,z1)), add_v((x0,0,z1))]
    apex = add_v((cx, h, cz))
    quad(mtl, b[0], b[3], b[2], b[1])         # base
    for i in range(4):
        tri(mtl, b[i], b[(i+1)%4], apex)      # sides

def sphere(mtl, cx, cy, cz, r, stacks=28, slices=40):
    base = len(V)
    nbase = len(N)
    for i in range(stacks+1):
        phi = math.pi * i/stacks
        for j in range(slices+1):
            th = 2*math.pi * j/slices
            nx = math.sin(phi)*math.cos(th)
            ny = math.cos(phi)
            nz = math.sin(phi)*math.sin(th)
            add_v((cx + r*nx, cy + r*ny, cz + r*nz))
            add_n((nx, ny, nz))
    def idx(i, j):
        k = i*(slices+1) + j
        return base + k + 1, nbase + k + 1
    for i in range(stacks):
        for j in range(slices):
            a, b, c, d = idx(i,j), idx(i,j+1), idx(i+1,j+1), idx(i+1,j)
            F.append((mtl, [a, b, c]))
            F.append((mtl, [a, c, d]))

# ---- scene layout: four shapes in a row on a grey ground ----
quad("grey", add_v((-9,0,-6)), add_v((9,0,-6)), add_v((9,0,6)), add_v((-9,0,6)))
box("red",        -4.5, 0.0, 2.2, 2.2, 2.2)
tri_prism("green", -1.5, 0.0, 2.4, 2.6, 2.2)
pyramid("gold",     1.5, 0.0, 2.4, 2.8)
sphere("glass",     4.8, 1.2, 0.0, 1.2)

with open("models/scene.obj", "w") as f:
    f.write("# multi-shape demo scene\n")
    for p in V: f.write(f"v {p[0]:.5f} {p[1]:.5f} {p[2]:.5f}\n")
    for n in N: f.write(f"vn {n[0]:.5f} {n[1]:.5f} {n[2]:.5f}\n")
    cur = None
    for mtl, idxs in F:
        if mtl != cur:
            f.write(f"usemtl {mtl}\n"); cur = mtl
        toks = []
        for vi, ni in idxs:
            toks.append(f"{vi}//{ni}" if ni is not None else f"{vi}")
        f.write("f " + " ".join(toks) + "\n")

print(f"wrote models/scene.obj: {len(V)} verts, {len(N)} normals, {len(F)} faces")
