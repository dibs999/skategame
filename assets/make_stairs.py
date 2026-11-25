import sys, math
sys.path.insert(0, '/home/ubuntu/repos/skategame/assets')
from objwrite import write_obj, add_cyl

objects = {}
X0, X1, Z0, D, PLAT_H = 15.5, 24.5, -6.0, 2.0, 1.5

def add_box(objects, name, x0, y0, z0, x1, y1, z1):
    ov, of = objects.setdefault(name, ([], []))
    v0 = len(ov)
    for x in (x0, x1):
        for y in (y0, y1):
            for z in (z0, z1):
                pass
    # verts: z0 plane 0-3, z1 plane 4-7  (x0y0z0, x1y0z0, x1y1z0, x0y1z0, then z1)
    for (x, y, z) in [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),
                      (x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]:
        ov.append((x, y, z))
    for f in [(0,1,5,4),(2,3,7,6),(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5)]:
        of.append(tuple(v0+i for i in f))

# steps with slightly recessed risers: tread slab + nosing strip
for i in range(5):
    zA = Z0 + i * D
    top = PLAT_H - (i + 1) * 0.25
    add_box(objects, 'concrete_steps', X0, 0, zA, X1, top, zA + D)
    # nosing strip: thin darker cap at the front edge of each tread
    add_box(objects, 'concrete_nosing', X0, top - 0.02, zA + D - 0.10, X1, top + 0.005, zA + D)

# hubbas: smooth sloped wedges flanking the stairs (6 segments z -6..+6)
def wedge(objects, name, x0, x1, zA, zB, yA, yB):
    # sloped top from (zA,yA) to (zB,yB), solid down to y0=0
    ov, of = objects.setdefault(name, ([], []))
    v0 = len(ov)
    # 8 verts: bottom rect + sloped top rect
    for (x, y, z) in [(x0,0,zA),(x1,0,zA),(x1,yA,zA),(x0,yA,zA),
                      (x0,0,zB),(x1,0,zB),(x1,yB,zB),(x0,yB,zB)]:
        ov.append((x, y, z))
    for f in [(0,1,5,4),(2,3,7,6),(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5)]:
        of.append(tuple(v0+i for i in f))

zs, ze = Z0, Z0 + 6 * D
yTopA, yTopB = PLAT_H + 0.22, PLAT_H + 0.22 - 6 * 0.25
wedge(objects, 'paint_hubba', X0 - 1.3, X0 - 0.05, zs, ze, yTopA, yTopB)
wedge(objects, 'paint_hubba', X1 + 0.05, X1 + 1.3, zs, ze, yTopA, yTopB)

# handrails: sloped tubes + posts
def rail(x):
    add_cyl(objects, 'metal_rail', x, 2.15, -6.4, x, 0.85, 4.2, 0.04, 12)
    for (pz, ph) in [(-6.2, 1.9), (-1.2, 1.5), (4.0, 0.7)]:
        add_cyl(objects, 'metal_rail', x, 0, pz, x, ph, pz, 0.03, 8)
rail(18.0)
rail(16.6)

write_obj(objects, '/home/ubuntu/repos/skategame/assets/stairs.obj', 2.0)
