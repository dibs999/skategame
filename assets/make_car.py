import sys, math
sys.path.insert(0, '/home/ubuntu/repos/skategame/assets')
from objwrite import write_obj, add_cyl

objects = {}

def sgn_pow(v, e):
    return math.copysign(abs(v) ** e, v)

# body: superellipse cross-sections along z (front at -z)
stations = [
    (-2.05, 0.42, 0.55, 0.15),
    (-1.60, 0.45, 0.78, 0.20),
    (-0.90, 0.50, 0.82, 0.26),
    (-0.20, 0.58, 0.83, 0.33),
    ( 0.60, 0.58, 0.82, 0.33),
    ( 1.30, 0.52, 0.80, 0.24),
    ( 2.05, 0.45, 0.60, 0.18),
]
SEG = 12
ov, of = objects.setdefault('paint_body', ([], []))
rings = []
for (z, yc, w, h) in stations:
    ring = []
    for i in range(SEG):
        a = i / SEG * 2 * math.pi
        ca, sa = math.cos(a), math.sin(a)
        # x horizontal, y vertical; flat bottom: clamp y min
        px = w * sgn_pow(ca, 0.65)
        py = yc + h * sgn_pow(sa, 0.65)
        if py < 0.24: py = 0.24
        ov.append((px, py, z))
        ring.append(len(ov) - 1)
    rings.append(ring)
for r in range(len(rings) - 1):
    for i in range(SEG):
        j = (i + 1) % SEG
        of.append((rings[r][i], rings[r][j], rings[r + 1][j], rings[r + 1][i]))
# end caps: fan to center point
for end in (0, len(rings) - 1):
    cidx = len(ov)
    zc = stations[end][0]
    yc = stations[end][1]
    ov.append((0, yc, zc))
    for i in range(SEG):
        j = (i + 1) % SEG
        if end == 0:
            of.append((cidx, rings[end][j], rings[end][i], rings[end][i]))
        else:
            of.append((cidx, rings[end][i], rings[end][j], rings[end][j]))

# glasshouse: narrower darker band over cabin z -0.55..0.95
ov2, of2 = objects.setdefault('glass_cabin', ([], []))
gstations = [
    (-0.55, 0.78, 0.68, 0.16),
    (-0.20, 0.85, 0.72, 0.20),
    ( 0.60, 0.85, 0.72, 0.20),
    ( 0.95, 0.76, 0.66, 0.14),
]
grings = []
for (z, yc, w, h) in gstations:
    ring = []
    for i in range(SEG):
        a = i / SEG * 2 * math.pi
        ca, sa = math.cos(a), math.sin(a)
        px = w * sgn_pow(ca, 0.6)
        py = yc + h * sgn_pow(sa, 0.6)
        ring.append(len(ov2))
        ov2.append((px, py, z))
    grings.append(ring)
for r in range(len(grings) - 1):
    for i in range(SEG):
        j = (i + 1) % SEG
        of2.append((grings[r][i], grings[r][j], grings[r + 1][j], grings[r + 1][i]))

# wheels
for sx in (-0.78, 0.78):
    for sz in (-1.35, 1.35):
        add_cyl(objects, 'rubber_wheel', sx, 0.28, sz - 0.16, sx, 0.28, sz + 0.16, 0.29, 12)
        add_cyl(objects, 'metal_hub', sx, 0.28, sz - 0.17, sx, 0.28, sz + 0.17, 0.12, 10)

write_obj(objects, '/home/ubuntu/repos/skategame/assets/car.obj', 1.5)
