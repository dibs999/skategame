import sys, math
sys.path.insert(0, '/home/ubuntu/repos/skategame/assets')
from objwrite import write_obj, add_cyl

objects = {}

def box(objects, name, x0, y0, z0, x1, y1, z1):
    ov, of = objects.setdefault(name, ([], []))
    v0 = len(ov)
    for (x, y, z) in [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),
                      (x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]:
        ov.append((x, y, z))
    for f in [(0,1,5,4),(2,3,7,6),(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5)]:
        of.append(tuple(v0+i for i in f))

def bench(cx, cz):
    # slatted seat: 4 planks along x, gaps in z
    pw, gap = 0.09, 0.025
    z0 = cz - 0.2
    for i in range(4):
        za = z0 + i * (pw + gap)
        box(objects, 'wood_slats', cx - 1.05, 0.40, za, cx + 1.05, 0.435, za + pw)
    # metal frame: two side loops + feet
    for sx in (-0.9, 0.9):
        add_cyl(objects, 'metal_frame', cx + sx, 0, cz - 0.18, cx + sx, 0.40, cz - 0.18, 0.03, 8)
        add_cyl(objects, 'metal_frame', cx + sx, 0, cz + 0.18, cx + sx, 0.40, cz + 0.18, 0.03, 8)
        add_cyl(objects, 'metal_frame', cx + sx, 0, cz - 0.24, cx + sx, 0, cz + 0.24, 0.028, 8)

bench(-20.1, 20)
bench(8.1, 30)

write_obj(objects, '/home/ubuntu/repos/skategame/assets/bench.obj', 1.5)
