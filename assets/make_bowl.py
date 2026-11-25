# Blender headless: sunken bowl asset matching terrainHeight() EXACTLY.
# circular bowl at (-34,-26) R=10 D=2.6, smoothstep profile, coping ring, tile band.
import sys, math
sys.path.insert(0, "/home/ubuntu/repos/skategame/assets")
from objwrite import write_obj, add_cyl

BX, BZ, R, D = -34.0, -26.0, 10.0, 2.6

def bowlh(d):
    if d >= R: return 0.0
    if d < R * 0.45: return -D
    t = (R - d) / (R * 0.55)
    return -D * (t * t * (3 - 2 * t))

objects = {}
# radial mesh: rings from center to rim
NR, NS = 14, 48
ov, of = objects.setdefault("concrete_bowl", ([], []))
for ri in range(NR + 1):
    d = R * ri / NR
    for si in range(NS):
        a = si / NS * 2 * math.pi
        x = BX + math.cos(a) * d
        z = BZ + math.sin(a) * d
        ov.append((x, bowlh(d), z))
def idx(ri, si): return ri * NS + si % NS
for ri in range(NR):
    for si in range(NS):
        a = idx(ri, si); b = idx(ri, si + 1); c2 = idx(ri + 1, si + 1); d2 = idx(ri + 1, si)
        # winding: normals must point up/inward-facing-up — verify sign empirically
        of.append((a, b, c2, d2))
# deck ring outside the rim (r 10..11.5 flat) so the lip reads finished
ov2, of2 = objects.setdefault("concrete_bowldeck", ([], []))
for ri in range(2):
    d = R + 1.5 * ri
    for si in range(NS):
        a = si / NS * 2 * math.pi
        ov2.append((BX + math.cos(a) * d, 0.0, BZ + math.sin(a) * d))
def idx2(ri, si): return ri * NS + si % NS
for si in range(NS):
    a = idx2(0, si); b = idx2(0, si + 1); c2 = idx2(1, si + 1); d2 = idx2(1, si)
    of2.append((a, b, c2, d2))
# coping ring around the lip
for si in range(NS):
    a0 = si / NS * 2 * math.pi
    a1 = (si + 1) / NS * 2 * math.pi
    add_cyl(objects, "metal_coping",
            BX + math.cos(a0) * R, 0.05, BZ + math.sin(a0) * R,
            BX + math.cos(a1) * R, 0.05, BZ + math.sin(a1) * R, 0.04, 6)


write_obj(objects, "/home/ubuntu/repos/skategame/assets/bowl.obj", 2.0)
