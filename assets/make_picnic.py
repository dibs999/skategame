import sys, math
sys.path.insert(0, '/home/ubuntu/repos/skategame/assets')
from objwrite import write_obj, add_cyl

objects = {}
CX, CZ = 8.0, -22.0

def box(objects, name, x0, y0, z0, x1, y1, z1):
    ov, of = objects.setdefault(name, ([], []))
    v0 = len(ov)
    for (x, y, z) in [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),
                      (x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]:
        ov.append((x, y, z))
    for f in [(0,1,5,4),(2,3,7,6),(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5)]:
        of.append(tuple(v0+i for i in f))

# slatted tabletop: 5 planks with gaps, x spans +-0.85, z across width
top_y = 0.72
pw = 0.22  # plank width in z
gap = 0.03
zstart = CZ - 0.65
for i in range(5):
    z0 = zstart + i * (pw + gap)
    box(objects, 'wood_slats', CX - 0.85, top_y - 0.025, z0, CX + 0.85, top_y + 0.01, z0 + pw)
# seats: 2 planks each side
for side in (-1, 1):
    zc = CZ + side * 1.05
    for i in range(2):
        z0 = zc - 0.14 + i * 0.17
        box(objects, 'wood_slats', CX - 0.85, 0.42, z0, CX + 0.85, 0.45, z0 + 0.14)
# A-frame metal legs
for sx in (-0.55, 0.55):
    add_cyl(objects, 'metal_frame', CX + sx, 0, CZ - 0.45, CX + sx * 1.35, 0.70, CZ - 0.15, 0.035, 8)
    add_cyl(objects, 'metal_frame', CX + sx, 0, CZ + 0.45, CX + sx * 1.35, 0.70, CZ + 0.15, 0.035, 8)
add_cyl(objects, 'metal_frame', CX - 0.7, 0.5, CZ, CX + 0.7, 0.5, CZ, 0.03, 8)

write_obj(objects, '/home/ubuntu/repos/skategame/assets/picnic.obj', 1.5)
