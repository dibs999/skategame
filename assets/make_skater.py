# Skater v4: lofted single-surface body parts — no sphere composites.
# Parts along -Y, origin at proximal joint, meters. Same part names as v3.
import sys, math
sys.path.insert(0, '/home/ubuntu/repos/skategame/assets')
from objwrite import write_obj

objects = {}
NSEG = 14

def loft(name, rings, nseg=NSEG, cap_top=True, cap_bot=True):
    # rings: list of (cx, cy, cz, rx, rz) from proximal (y=0) downward
    ov, of = objects.setdefault(name, ([], []))
    rows = []
    for (cx, cy, cz, rx, rz) in rings:
        row = []
        for i in range(nseg):
            a = 2 * math.pi * i / nseg
            ov.append((cx + rx * math.cos(a), cy, cz + rz * math.sin(a)))
            row.append(len(ov) - 1)
        rows.append(row)
    for r in range(len(rows) - 1):
        for i in range(nseg):
            j = (i + 1) % nseg
            of.append((rows[r][i], rows[r + 1][i], rows[r + 1][j], rows[r][j]))
    for end, cap in ((0, cap_top), (len(rows) - 1, cap_bot)):
        if not cap: continue
        ci = len(ov)
        cx, cy, cz, rx, rz = rings[end]
        ov.append((cx, cy, cz))
        for i in range(nseg):
            j = (i + 1) % nseg
            if end == 0:
                of.append((ci, rows[end][i], rows[end][j], rows[end][j]))
            else:
                of.append((ci, rows[end][j], rows[end][i], rows[end][i]))

# ---- torso: shoulders wide, chest, tapering waist ----
loft("torso", [
    (0,  0.00,  0,     0.175, 0.095),   # shoulder line
    (0, -0.07,  0.005, 0.185, 0.105),   # chest
    (0, -0.16,  0.005, 0.160, 0.100),
    (0, -0.27,  0,     0.135, 0.090),   # ribs
    (0, -0.37,  0,     0.120, 0.085),   # waist
    (0, -0.44,  0,     0.125, 0.088),   # hem
])

# ---- pelvis: hips with slight butt protrusion (-z back) ----
loft("pelvis", [
    (0,  0.00,  0,     0.128, 0.085),
    (0, -0.07, -0.006, 0.138, 0.092),
    (0, -0.13, -0.010, 0.132, 0.094),
    (0, -0.18, -0.004, 0.112, 0.080),
])

# ---- neck ----
loft("neck", [
    (0,  0.00,  0,     0.048, 0.045),
    (0, -0.09,  0.005, 0.042, 0.040),
])

# ---- head: cranium with tapered jaw, forward shift ----
loft("head", [
    (0,  0.01,  0,     0.055, 0.053),   # crown top
    (0, -0.025, 0.004, 0.073, 0.070),   # cranium
    (0, -0.08,  0.006, 0.075, 0.071),   # widest
    (0, -0.125, 0.009, 0.066, 0.062),   # cheekbones
    (0, -0.16,  0.013, 0.052, 0.050),   # jaw
    (0, -0.183, 0.016, 0.034, 0.036),   # chin
])

# ---- cap: dome + brim disc forward ----
loft("cap", [
    (0,  0.026, 0,     0.056, 0.054),
    (0, -0.01,  0.004, 0.083, 0.080),
    (0, -0.06,  0.004, 0.085, 0.082),
    (0, -0.085, 0.004, 0.081, 0.078),
], cap_bot=False)
# brim: thin elliptical puck protruding forward (+z)
loft("cap", [
    (0, -0.095, 0.085, 0.062, 0.095),
    (0, -0.104, 0.085, 0.060, 0.092),
])

# ---- upperarm: shoulder cap, bicep bulge, taper to elbow ----
loft("upperarm", [
    (0,  0.00,  0,     0.052, 0.049),
    (0, -0.08,  0,     0.058, 0.054),   # bicep sleeve
    (0, -0.15,  0,     0.050, 0.047),   # sleeve hem taper
    (0, -0.22,  0,     0.042, 0.040),
    (0, -0.31,  0,     0.037, 0.035),   # elbow
])

# ---- forearm ----
loft("forearm", [
    (0,  0.00,  0,     0.046, 0.044),
    (0, -0.09,  0,     0.048, 0.045),   # forearm muscle
    (0, -0.19,  0,     0.038, 0.036),
    (0, -0.29,  0,     0.030, 0.028),   # wrist
])

# ---- hand: flattened, slightly elongated forward ----
loft("hand", [
    (0,  0.00,  0,     0.030, 0.024),   # wrist
    (0, -0.04,  0.004, 0.038, 0.020),   # palm — wide and flat
    (0, -0.09,  0.008, 0.036, 0.018),
    (0, -0.13,  0.010, 0.026, 0.014),   # fingers
    (0, -0.155, 0.011, 0.016, 0.010),   # fingertips
])

# ---- thigh: hip, quad sweep, knee ----
loft("thigh", [
    (0,  0.00,  0,     0.108, 0.098),
    (0, -0.12,  0.008, 0.116, 0.103),   # quad
    (0, -0.25,  0.005, 0.104, 0.092),
    (0, -0.38,  0,     0.082, 0.074),
    (0, -0.47,  0,     0.066, 0.061),   # knee
])

# ---- shin: calf belly, ankle ----
loft("shin", [
    (0,  0.00,  0,     0.062, 0.058),
    (0, -0.10, -0.012, 0.072, 0.066),   # calf
    (0, -0.22, -0.008, 0.060, 0.055),
    (0, -0.36,  0,     0.045, 0.042),
    (0, -0.47,  0,     0.036, 0.034),   # ankle
])

# ---- shoe: runs forward +z as it descends; elongated toe ----
loft("shoe", [
    (0,  0.00,  0.00,  0.052, 0.050),   # ankle collar
    (0, -0.03,  0.01,  0.056, 0.062),
    (0, -0.055, 0.045, 0.054, 0.085),   # midfoot, longer in z
    (0, -0.06,  0.10,  0.050, 0.105),   # toe box
    (0, -0.055, 0.16,  0.042, 0.070),   # toe taper
    (0, -0.05,  0.19,  0.030, 0.040),   # toe tip
])

write_obj(objects, '/home/ubuntu/repos/skategame/assets/skater.obj', 1.0)
