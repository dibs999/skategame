import sys, math
sys.path.insert(0, '/home/ubuntu/repos/skategame/assets')
from objwrite import write_obj, add_cyl

Z0, Z1 = -38.0, -30.0
objects = {}

def qpwest(x):
    dx = 44.5 - x
    return 2.5 - math.sqrt(max(0.0, 6.25 - dx * dx))
def qpeast(x):
    dx = x - 49.5
    return 2.5 - math.sqrt(max(0.0, 6.25 - dx * dx))

def surf(objects, name, x0, x1, fn, nx=24):
    nz = 6
    ov, of = objects.setdefault(name, ([], []))
    idx = []
    for i in range(nx + 1):
        x = x0 + (x1 - x0) * i / nx
        y = fn(x)
        row = []
        for j in range(nz + 1):
            z = Z0 + (Z1 - Z0) * j / nz
            ov.append((x, y, z))
            row.append(len(ov) - 1)
        idx.append(row)
    for i in range(nx):
        for j in range(nz):
            a, b = idx[i][j], idx[i][j + 1]
            c2, d2 = idx[i + 1][j], idx[i + 1][j + 1]
            of.append((a, c2, d2, b))

surf(objects, 'wood_mini_west', 42.0, 44.5, qpwest)
surf(objects, 'wood_mini_east', 49.5, 52.0, qpeast)

def add_flat(objects, name, x0, z0, x1, z1, y, th=0.12):
    ov, of = objects.setdefault(name, ([], []))
    v0 = len(ov)
    for (x, yy, z) in [(x0,y,z0),(x1,y,z0),(x1,y,z1),(x0,y,z1),
                       (x0,y-th,z0),(x1,y-th,z0),(x1,y-th,z1),(x0,y-th,z1)]:
        ov.append((x, yy, z))
    for f in [(0,1,2,3),(7,6,5,4),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)]:
        of.append(tuple(v0+i for i in f))

add_flat(objects, 'wood_mini_flat', 44.4, Z0, 49.6, Z1, 0.12, 0.12)
add_flat(objects, 'wood_mini_deckw', 39.0, Z0, 42.0, Z1, 2.5, 2.5)
add_flat(objects, 'wood_mini_decke', 52.0, Z0, 55.0, Z1, 2.5, 2.5)

add_cyl(objects, 'metal_coping', 42, 2.52, Z0, 42, 2.52, Z1, 0.045, 10)
add_cyl(objects, 'metal_coping', 52, 2.52, Z0, 52, 2.52, Z1, 0.045, 10)

write_obj(objects, '/home/ubuntu/repos/skategame/assets/miniramp.obj', 2.5)
