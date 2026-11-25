import sys, math, random
sys.path.insert(0, '/home/ubuntu/repos/skategame/assets')
from objwrite import write_obj, add_cyl

objects = {}

def sphere_blob(objects, name, cx, cy, cz, r, jitter, seed, nlat=6, nlon=8, squash=0.85):
    random.seed(seed)
    ov, of = objects.setdefault(name, ([], []))
    v0 = len(ov)
    # jittered sphere verts
    jtab = {}
    def jit(i, j):
        key = (i % (nlon + 1), j)
        if key not in jtab:
            jtab[key] = 1.0 + random.uniform(-jitter, jitter)
        return jtab[key]
    idx = []
    for j in range(nlat + 1):
        phi = -math.pi / 2 + math.pi * j / nlat
        row = []
        for i in range(nlon + 1):
            ii = i % nlon
            th = 2 * math.pi * i / nlon
            rr = r * jit(ii, j)
            x = cx + rr * math.cos(phi) * math.cos(th)
            y = cy + rr * math.sin(phi) * squash
            z = cz + rr * math.cos(phi) * math.sin(th)
            ov.append((x, y, z))
            row.append(len(ov) - 1)
        idx.append(row)
    for j in range(nlat):
        for i in range(nlon):
            a, b = idx[j][i], idx[j][i + 1]
            c2, d2 = idx[j + 1][i], idx[j + 1][i + 1]
            of.append((a, b, d2, c2))

def tree(x, z, trunkh, canopyy, seed):
    random.seed(seed)
    # tapered trunk: two stacked cylinders
    add_cyl(objects, 'bark_trunk', x, 0, z, x + random.uniform(-0.1, 0.1), trunkh * 0.6, z + random.uniform(-0.1, 0.1), 0.17, 8)
    tx, tz = x + random.uniform(-0.15, 0.15), z + random.uniform(-0.15, 0.15)
    add_cyl(objects, 'bark_trunk', x, trunkh * 0.55, z, tx, trunkh, tz, 0.11, 7)
    # branches reaching into canopy blobs
    blobs = []
    for b in range(4):
        ang = random.uniform(0, 6.28)
        rr = random.uniform(0.4, 1.1)
        bx = x + math.cos(ang) * rr
        bz = z + math.sin(ang) * rr
        by = canopyy + random.uniform(-0.7, 0.7)
        br = random.uniform(0.7, 1.15)
        blobs.append((bx, by, bz, br))
        add_cyl(objects, 'bark_trunk', tx, trunkh - 0.2, tz, bx, by - br * 0.3, bz, 0.05, 6)
    # main canopy
    blobs.append((x, canopyy + 0.5, z, 1.25))
    for i, (bx, by, bz, br) in enumerate(blobs):
        sphere_blob(objects, 'leaf_canopy', bx, by, bz, br, 0.22, seed * 31 + i)

random.seed(7)
for k, (x, z) in enumerate([(-40, 10), (40, -20), (-8, 36)]):
    tree(x, z, 2.6, 3.6, 100 + k)
for k, (x, z) in enumerate([(-60, 30), (-58, -18), (60, 10), (56, -30), (-30, 50), (20, 48)]):
    tree(x, z, 2.9, 3.9, 200 + k)

write_obj(objects, '/home/ubuntu/repos/skategame/assets/trees.obj', 1.2)
