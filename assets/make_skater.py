# Blender headless: segmented skater body v2 — composite shapes per part for human silhouette.
# Each part along -Y, origin at proximal joint. Units: meters.
import bpy, math

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)

def _top_zero(o):
    ys = [v.co.y for v in o.data.vertices]
    top = max(ys)
    for v in o.data.vertices:
        v.co = (v.co.x, v.co.y - top, v.co.z)

def ball(scale, seg=20, rings=16):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings)
    o = bpy.context.object
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.shade_smooth()
    return o

def cone(r_top, r_bot, length, seg=20):
    bpy.ops.mesh.primitive_cone_add(vertices=seg, radius1=r_bot, radius2=r_top, depth=length)
    o = bpy.context.object
    bpy.ops.object.shade_smooth()
    o.rotation_euler = (math.radians(-90), 0, 0)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(rotation=True)
    return o

def shift(o, dx, dy, dz):
    for v in o.data.vertices:
        v.co = (v.co.x + dx, v.co.y + dy, v.co.z + dz)

def joinpart(name, objs):
    # join objects into one named part, origin at top (proximal)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = bpy.context.object
    o.name = name
    _top_zero(o)
    return o

# ---- torso: chest ball overlapping tapered trunk, flattened front-back ----
trunk = cone(0.185, 0.115, 0.44, seg=22)
for v in trunk.data.vertices:
    v.co = (v.co.x, v.co.y, v.co.z * 0.60)
chest = ball((0.175, 0.115, 0.095), seg=22, rings=16)
shift(chest, 0, -0.10, 0)
joinpart("torso", [trunk, chest])

# ---- pelvis ----
pelv = ball((0.16, 0.115, 0.10), seg=20, rings=14)
butt = ball((0.15, 0.10, 0.085), seg=18, rings=12)
shift(butt, 0, -0.06, -0.03)
joinpart("pelvis", [pelv, butt])

# ---- head: cranium + jaw taper ----
cran = ball((0.082, 0.10, 0.086), seg=22, rings=16)
jaw = ball((0.062, 0.055, 0.066), seg=16, rings=12)
shift(jaw, 0, -0.095, 0.018)
joinpart("head", [cran, jaw])

# ---- neck ----
neck = cone(0.045, 0.05, 0.09, seg=16)
joinpart("neck", [neck])

# ---- cap: dome + forward brim ----
dome = ball((0.088, 0.055, 0.092), seg=20, rings=12)
brim = ball((0.055, 0.012, 0.06), seg=14, rings=8)
shift(brim, 0, -0.03, 0.11)
joinpart("cap", [dome, brim])

# ---- arms: muscle-shaped limbs ----
ua = cone(0.066, 0.048, 0.31, seg=18)
bic = ball((0.06, 0.09, 0.055), seg=14, rings=10)
shift(bic, 0, -0.10, 0)
joinpart("upperarm", [ua, bic])

fa = cone(0.048, 0.038, 0.29, seg=18)
joinpart("forearm", [fa])

hand = ball((0.042, 0.06, 0.032), seg=14, rings=10)
for v in hand.data.vertices:
    v.co = (v.co.x, v.co.y, v.co.z * 0.7)
joinpart("hand", [hand])

# ---- legs: thigh with quad bulge, shin with calf ----
th = cone(0.098, 0.068, 0.47, seg=20)
quad = ball((0.085, 0.13, 0.08), seg=16, rings=12)
shift(quad, 0, -0.13, 0.01)
joinpart("thigh", [th, quad])

sh = cone(0.068, 0.048, 0.47, seg=20)
calf = ball((0.058, 0.11, 0.06), seg=16, rings=12)
shift(calf, 0, -0.09, -0.015)
joinpart("shin", [sh, calf])

# ---- shoe: sole wedge + toe box + heel ----
sole = ball((0.052, 0.024, 0.135), seg=16, rings=10)
upper = ball((0.05, 0.045, 0.105), seg=16, rings=12)
shift(upper, 0, -0.028, -0.012)
heel = ball((0.048, 0.05, 0.05), seg=14, rings=10)
shift(heel, 0, -0.025, -0.085)
shoeo = joinpart("shoe", [sole, upper, heel])
# extend toe forward +z: scale z slightly toward front already in shapes

# ---- write OBJ manually ----
path = "/home/ubuntu/repos/skategame/assets/skater.obj"
ofs = 1
with open(path, "w") as fh:
    for o in bpy.data.objects:
        if o.type != 'MESH': continue
        m = o.data
        m.calc_loop_triangles()
        fh.write("o %s\n" % o.name)
        for v in m.vertices:
            fh.write("v %.5f %.5f %.5f\n" % (v.co.x, v.co.y, v.co.z))
        for v in m.vertices:
            fh.write("vn %.4f %.4f %.4f\n" % (v.normal.x, v.normal.y, v.normal.z))
        for t in m.loop_triangles:
            a, b, cc = t.vertices
            fh.write("f %d//%d %d//%d %d//%d\n" % (a+ofs, a+ofs, b+ofs, b+ofs, cc+ofs, cc+ofs))
        ofs += len(m.vertices)
print("EXPORTED", path)
