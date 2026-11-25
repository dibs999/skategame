# Blender headless: builds a segmented low-poly skater body, exports Wavefront OBJ.
# Each body part is a separate named object, modeled pointing along -Y (bone dir convention:
# origin at the proximal joint, mesh extends toward -Y). Units: meters.
import bpy, math, sys

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o, do_unlink=True)

def smooth(obj):
    for p in obj.data.polygons: p.use_smooth = True

def part_uvsphere(name, loc, scale, seg=16, rings=12):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, location=(0,0,0))
    o = bpy.context.object; o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    # shift so origin is at top (proximal) and mesh extends toward -Y
    for v in o.data.vertices:
        x,y,z = v.co.x, v.co.y, v.co.z
        v.co = (x, z - loc, y)  # reorient: blender z-up -> y-down bone axis
    smooth(o)
    return o

def cyl(name, r0, r1, length, seg=12, taper_top=True):
    # tapered cylinder along -Y, origin at proximal end
    bpy.ops.mesh.primitive_cone_add(vertices=seg, radius1=r1, radius2=r0, depth=length, location=(0,0,0))
    o = bpy.context.object; o.name = name
    for v in o.data.vertices:
        x,y,z = v.co.x, v.co.y, v.co.z
        v.co = (x, z - length*0.0 - 0.0, y)
    # recenter: cone is centered; shift so top (radius2 end, +z) is at origin and extends -Y
    for v in o.data.vertices:
        v.co = (v.co.x, v.co.y - length*0.5 + 0.0, v.co.z)
    # hmm simpler: rotate z-axis onto -Y and shift
    smooth(o)
    return o

# Simplest robust approach: build each part as primitive(s), join per part, then transform
# so the part extends along -Y with origin at proximal joint. We do the reorientation in
# the exporter by rotating the object instead of vertices.

def make_limb(name, r_top, r_bot, length, seg=12):
    bpy.ops.mesh.primitive_cone_add(vertices=seg, radius1=r_bot, radius2=r_top, depth=length)
    o = bpy.context.object; o.name = name
    bpy.ops.object.shade_smooth()
    # cone: +z is radius2 (top/proximal). rotate so +z -> +Y? we want mesh along -Y with
    # proximal end at origin: rotate 90deg about X so z axis becomes -Y? rotating -90 about X
    # maps +z to -Y? test: rot X by +90: z->y? use quaternion: we want +z to map to +Y up...
    # Convention: we export with transform applied: proximal end (former +z cap) at y=0,
    # distal end at y=-length.
    o.rotation_euler = (math.radians(-90), 0, 0)  # +z -> -y? verify: rotating about X by -90: z axis goes to -Y yes
    o.location = (0, 0, 0)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(rotation=True, location=True)
    # after rotation mesh spans y in [-length/2, +length/2] flipped; shift proximal to 0
    zs = [v.co.y for v in o.data.vertices]
    top = max(zs)
    for v in o.data.vertices:
        v.co = (v.co.x, v.co.y - top, v.co.z)
    return o

def make_ball(name, scale, seg=14, rings=10):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings)
    o = bpy.context.object; o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.shade_smooth()
    ys = [v.co.y for v in o.data.vertices]
    top = max(ys)
    for v in o.data.vertices:
        v.co = (v.co.x, v.co.y - top, v.co.z)
    return o

def make_box(name, scale, bevel=0.02):
    bpy.ops.mesh.primitive_cube_add()
    o = bpy.context.object; o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.shade_smooth()
    ys = [v.co.y for v in o.data.vertices]
    top = max(ys)
    for v in o.data.vertices:
        v.co = (v.co.x, v.co.y - top, v.co.z)
    return o

# torso: tapered cylinder (waist narrower, chest wide), squashed front-to-back,
# origin at shoulder line, extends down to hips
torso = make_limb("torso", 0.175, 0.115, 0.42, seg=16)
for v in torso.data.vertices:
    v.co = (v.co.x, v.co.y, v.co.z * 0.62)   # flatten front-back
# pelvis/hips block
make_ball("pelvis", (0.165, 0.12, 0.105))
# head: origin at neck top, extends down? better: center-ish. use ball with origin at top
make_ball("head", (0.082, 0.105, 0.088), seg=18, rings=14)
# cap brim: flattened box
make_box("cap", (0.09, 0.02, 0.11))
# upper/lower arms: slim tapered cylinders
make_limb("upperarm", 0.062, 0.052, 0.31)
make_limb("forearm", 0.052, 0.042, 0.29)
make_ball("hand", (0.045, 0.055, 0.04), seg=10, rings=8)
# upper/lower legs: thicker
make_limb("thigh", 0.095, 0.072, 0.47)
make_limb("shin", 0.068, 0.052, 0.47)
# shoe: box-ish, origin at ankle top, extends down and forward (+z toe)
shoe = make_box("shoe", (0.055, 0.035, 0.13), bevel=0.03)

# write OBJ manually: o + v/vn/f, full control, no exporter-version issues
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
        # per-vertex normals after smooth shading
        for v in m.vertices:
            fh.write("vn %.4f %.4f %.4f\n" % (v.normal.x, v.normal.y, v.normal.z))
        for t in m.loop_triangles:
            a, b, cc = t.vertices
            fh.write("f %d//%d %d//%d %d//%d\n" % (a+ofs, a+ofs, b+ofs, b+ofs, cc+ofs, cc+ofs))
        ofs += len(m.vertices)
print("EXPORTED", path)
