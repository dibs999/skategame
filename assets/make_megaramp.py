# Blender headless: mega ramp asset matching the in-game heightfield EXACTLY.
# Objects tagged by material prefix: wood_*, metal_*, concrete_*.
# UVs: box-projected by dominant normal axis (written manually into the OBJ).
import bpy, math

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)

Z0, Z1 = 30.0, 50.0
verts = {}   # name -> list of (x,y,z)
faces = {}   # name -> list of (i0,i1,i2,i3) quads into global vert list per object

def surf_hf(name, x0, x1, fn, nx=48):
    """heightfield surface strip z0..z1, quads; returns nothing, adds to mesh data"""
    vs = []
    for i in range(nx + 1):
        x = x0 + (x1 - x0) * i / nx
        y = fn(x)
        vs.append((x, y, Z0))
        vs.append((x, y, Z1))
    fs = []
    for i in range(nx):
        a = i * 2
        fs.append((a, a + 1, a + 3, a + 2))
    return vs, fs

def qp_type2(x):  # roll-in: foot 64, radius 8, rising toward -x
    dx = 64.0 - x
    if dx < 0 or dx > 8: return 0.0
    return 8.0 - math.sqrt(max(0.0, 64.0 - dx * dx))

def kicker(x):    # type 0: foot 69, radius 5.5
    dx = x - 69.0
    if dx < 0 or dx > 5.5: return 0.0
    return 5.5 - math.sqrt(max(0.0, 30.25 - dx * dx))

def landing(x):   # linear 3 -> 0 over x77..91
    return 3.0 + (x - 77.0) / 14.0 * (0.0 - 3.0)

def quarter(x):   # type 0: foot 91, radius 7
    dx = x - 91.0
    if dx < 0 or dx > 7: return 0.0
    return 7.0 - math.sqrt(max(0.0, 49.0 - dx * dx))

# build merged mesh via bmesh-like manual construction: collect ALL wood surface verts/faces
objects = {}

def add_surface(name, vs, fs, thickness=0.15):
    """surface grid + side skirts + underside, as one object"""
    out_v = list(vs)
    out_f = list(fs)
    # side skirts: drop edges down by thickness along z0/z1 borders
    n = len(vs) // 2  # rows
    base = len(out_v)
    for i in range(len(vs)):
        x, y, z = vs[i]
        out_v.append((x, y - thickness, z))
    for i in range(n):
        a = i * 2
        if i < n - 1:
            na = (i + 1) * 2
            # z0 side skirt
            out_f.append((a, a + nx0(base, 0), 0, 0))  # placeholder fixed below
    # simpler: rebuild skirts properly
    out_f = out_f[:len(fs)]
    for i in range(n - 1):
        a, na = i * 2, (i + 1) * 2
        out_f.append((a, na, base + na, base + a))          # z0 skirt
        out_f.append((a + 1, base + a + 1, base + na + 1, na + 1))  # z1 skirt
    # end caps
    out_f.append((0, base + 0, base + 1, 1))
    e = (n - 1) * 2
    out_f.append((e, e + 1, base + e + 1, base + e))
    objects.setdefault(name, ([], []))
    ov, of = objects[name]
    ofs = len(ov)
    ov.extend(out_v)
    for q in out_f:
        of.append(tuple(ofs + i for i in q))

def nx0(b, x): return b + x  # dummy used above, removed below

# NOTE: the placeholder hack above is overwritten; keep clean implementation:
objects = {}

def add_surf(name, vs, fs, thickness=0.15):
    n = len(vs) // 2
    out_v = list(vs)
    base = len(out_v)
    for (x, y, z) in vs:
        out_v.append((x, y - thickness, z))
    out_f = list(fs)
    for i in range(n - 1):
        a, na = i * 2, (i + 1) * 2
        out_f.append((a, na, base + na, base + a))
        out_f.append((a + 1, base + a + 1, base + na + 1, na + 1))
    out_f.append((0, base + 0, base + 1, 1))
    e = (n - 1) * 2
    out_f.append((e, e + 1, base + e + 1, base + e))
    ov, of = objects.setdefault(name, ([], []))
    ofs = len(ov)
    ov.extend(out_v)
    for q in out_f:
        of.append(tuple(ofs + i for i in q))

def add_flat(name, x0, x1, y):
    vs = [(x0, y, Z0), (x0, y, Z1), (x1, y, Z1), (x1, y, Z0)]
    ov, of = objects.setdefault(name, ([], []))
    ofs = len(ov)
    ov.extend(vs)
    of.append((ofs + 0, ofs + 1, ofs + 2, ofs + 3))

def add_box(name, x0, y0, x1, y1):
    """solid box spanning z0..z1"""
    ov, of = objects.setdefault(name, ([], []))
    ofs = len(ov)
    for x, y, z in [(x0,y0,Z0),(x1,y0,Z0),(x1,y1,Z0),(x0,y1,Z0),
                    (x0,y0,Z1),(x1,y0,Z1),(x1,y1,Z1),(x0,y1,Z1)]:
        ov.append((x, y, z))
    # windings verified: normals point outward for every face
    for q in [(0,1,5,4),(2,3,7,6),(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5)]:
        of.append(tuple(ofs + i for i in q))

def add_cyl(name, x0, y0, z0, x1, y1, z1, r, seg=10):
    ov, of = objects.setdefault(name, ([], []))
    dx, dy, dz = x1 - x0, y1 - y0, z1 - z0
    L = math.sqrt(dx*dx + dy*dy + dz*dz)
    ux, uy, uz = dx/L, dy/L, dz/L
    # perpendicular basis
    if abs(uy) < 0.9: ax, ay, az = 0, 1, 0
    else: ax, ay, az = 1, 0, 0
    bx, by, bz = ay*uz - az*uy, az*ux - ax*uz, ax*uy - ay*ux
    bl = math.sqrt(bx*bx + by*by + bz*bz)
    bx, by, bz = bx/bl, by/bl, bz/bl
    cx2, cy2, cz2 = uy*bz - uz*by, uz*bx - ux*bz, ux*by - uy*bx
    ofs = len(ov)
    for end in range(2):
        ex, ey, ez = (x0, y0, z0) if end == 0 else (x1, y1, z1)
        for i in range(seg):
            a = i / seg * 2 * math.pi
            ca, sa = math.cos(a), math.sin(a)
            ov.append((ex + (bx*ca + cx2*sa)*r, ey + (by*ca + cy2*sa)*r, ez + (bz*ca + cz2*sa)*r))
    for i in range(seg):
        j = (i + 1) % seg
        of.append((ofs + i, ofs + j, ofs + seg + j, ofs + seg + i))

# ---- wood surfaces (exact heightfield replication) ----
vs, fs = surf_hf("w", 56, 64, qp_type2, 56)
add_surf("wood_rollin", vs, fs)
add_flat("wood_rollin", 64, 69, 0.0)   # flat run to the kicker (same object groups below)
vs, fs = surf_hf("w", 69, 73, kicker, 24)
add_surf("wood_kicker", vs, fs)
add_flat("wood_kicker", 73, 77, 0.0)   # gap floor strip (visual only; physics gap remains)
vs, fs = surf_hf("w", 77, 91, landing, 28)
add_surf("wood_landing", vs, fs)
vs, fs = surf_hf("w", 91, 98, quarter, 40)
add_surf("wood_quarter", vs, fs)
# tower + deck boxes
add_box("wood_tower", 50, 0, 56, 8)
add_box("wood_tower", 56, 7.9, 56.2, 8)  # lip cap
add_box("wood_deck", 98, 0, 106, 7)
# metal coping along lips + safety rails
add_cyl("metal_coping", 56, 8.02, Z0, 56, 8.02, Z1, 0.045)
add_cyl("metal_coping", 73, 1.72, Z0, 73, 1.72, Z1, 0.045)
add_cyl("metal_coping", 98, 7.02, Z0, 98, 7.02, Z1, 0.045)
# side railings only — a rail across the back edge sits exactly in the filmer
# camera line and blocks the whole view (real mega towers are open there)
for z in (Z0, Z1):
    add_cyl("metal_rail", 50.1, 8, z, 50.1, 8.9, z, 0.025)
    add_cyl("metal_rail", 50.1, 8.9, z, 55.9, 8.9, z, 0.025)
    add_cyl("metal_rail", 55.9, 8, z, 55.9, 8.9, z, 0.025)
    add_cyl("metal_rail", 105.8, 7, z, 105.8, 8.1, z, 0.025)
    add_cyl("metal_rail", 98.2, 7, z, 98.2, 8.1, z, 0.025)
    add_cyl("metal_rail", 98.2, 8.1, z, 105.8, 8.1, z, 0.025)

# ---- write OBJ with box-projected UVs ----
def write_obj(path, uvscale):
    ofs = 1
    tofs = 1
    with open(path, "w") as fh:
        for name in sorted(objects):
            ov, of = objects[name]
            fh.write("o %s\n" % name)
            # compute normals per face, accumulate per-vertex
            nrm = [[0.0, 0.0, 0.0] for _ in ov]
            for q in of:
                a, b, c, d = [ov[i] for i in q]
                ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
                vx, vy, vz = d[0]-a[0], d[1]-a[1], d[2]-a[2]
                nx = uy*vz - uz*vy; ny = uz*vx - ux*vz; nz = ux*vy - uy*vx
                l = math.sqrt(nx*nx+ny*ny+nz*nz) or 1.0
                for i in q:
                    nrm[i][0] += nx/l; nrm[i][1] += ny/l; nrm[i][2] += nz/l
            for v in ov:
                fh.write("v %.5f %.5f %.5f\n" % v)
            # box-projected UVs by dominant normal axis
            for i, v in enumerate(ov):
                nx, ny, nz = nrm[i]
                ax, ay, az = abs(nx), abs(ny), abs(nz)
                if ay >= ax and ay >= az: u2, v2 = v[0] / uvscale, v[2] / uvscale
                elif ax >= az: u2, v2 = v[2] / uvscale, v[1] / uvscale
                else: u2, v2 = v[0] / uvscale, v[1] / uvscale
                fh.write("vt %.5f %.5f\n" % (u2, v2))
            for n in nrm:
                l = math.sqrt(n[0]**2 + n[1]**2 + n[2]**2) or 1.0
                fh.write("vn %.4f %.4f %.4f\n" % (n[0]/l, n[1]/l, n[2]/l))
            for q in of:
                # quad -> two triangles
                ids = [(q[0], q[0]), (q[1], q[1]), (q[2], q[2]), (q[3], q[3])]
                t1 = ids[:3]; t2 = [ids[0], ids[2], ids[3]]
                for tri in (t1, t2):
                    fh.write("f " + " ".join("%d/%d/%d" % (vi + 1 - 0 + ofs - 1 + 1 - 1 + 1, vi + ofs, vi + ofs) for vi, _ in tri) + "\n")
            ofs += len(ov)
    print("EXPORTED", path)

# fix index math: write faces as v/vt/vn with same index for all three (they align per vertex)
def write_obj2(path, uvscale):
    vi = 1
    with open(path, "w") as fh:
        for name in sorted(objects):
            ov, of = objects[name]
            fh.write("o %s\n" % name)
            nrm = [[0.0, 0.0, 0.0] for _ in ov]
            for q in of:
                a, b, c, d = [ov[i] for i in q]
                ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
                vx, vy, vz = d[0]-a[0], d[1]-a[1], d[2]-a[2]
                nx = uy*vz - uz*vy; ny = uz*vx - ux*vz; nz = ux*vy - uy*vx
                l = math.sqrt(nx*nx+ny*ny+nz*nz) or 1.0
                for i in q:
                    nrm[i][0] += nx/l; nrm[i][1] += ny/l; nrm[i][2] += nz/l
            for v in ov:
                fh.write("v %.5f %.5f %.5f\n" % v)
            for i, v in enumerate(ov):
                nx, ny, nz = nrm[i]
                ax2, ay2, az2 = abs(nx), abs(ny), abs(nz)
                if ay2 >= ax2 and ay2 >= az2: u2, vv = v[0] / uvscale, v[2] / uvscale
                elif ax2 >= az2: u2, vv = v[2] / uvscale, v[1] / uvscale
                else: u2, vv = v[0] / uvscale, v[1] / uvscale
                fh.write("vt %.5f %.5f\n" % (u2, vv))
            for n in nrm:
                l = math.sqrt(n[0]**2 + n[1]**2 + n[2]**2) or 1.0
                fh.write("vn %.4f %.4f %.4f\n" % (n[0]/l, n[1]/l, n[2]/l))
            for q in of:
                for tri in ((q[0], q[1], q[2]), (q[0], q[2], q[3])):
                    fh.write("f " + " ".join("%d/%d/%d" % (vi + t, vi + t, vi + t) for t in tri) + "\n")
            vi += len(ov)
    print("EXPORTED", path)

write_obj2("/home/ubuntu/repos/skategame/assets/megaramp.obj", 2.5)
