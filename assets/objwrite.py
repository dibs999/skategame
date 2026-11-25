import math

def write_obj(objects, path, uvscale):
    vi = 1
    with open(path, "w") as fh:
        for name in sorted(objects):
            ov, of = objects[name]
            fh.write("o %s\n" % name)
            nrm = [[0.0, 0.0, 0.0] for _ in ov]
            for q in of:
                a, b, c, d = [ov[i] for i in q]
                ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
                vx, vy, vz = d[0]-a[0], d[1]-a[1], d[2]-a[2]
                nx = uy*vz - uz*vy; ny = uz*vx - ux*vz; nz = ux*vy - uy*vx
                l = math.sqrt(nx*nx+ny*ny+nz*nz) or 1.0
                for i in q:
                    nrm[i][0] += nx/l; nrm[i][1] += ny/l; nrm[i][2] += nz/l
            for v in ov:
                fh.write("v %.5f %.5f %.5f\n" % v)
            for i, v in enumerate(ov):
                nx, ny, nz = nrm[i]
                ax2, ay2, az2 = abs(nx), abs(ny), abs(nz)
                if ay2 >= ax2 and ay2 >= az2: u2, vv = v[0] / uvscale, v[2] / uvscale
                elif ax2 >= az2: u2, vv = v[2] / uvscale, v[1] / uvscale
                else: u2, vv = v[0] / uvscale, v[1] / uvscale
                fh.write("vt %.5f %.5f\n" % (u2, vv))
            for n in nrm:
                l = math.sqrt(n[0]**2 + n[1]**2 + n[2]**2) or 1.0
                fh.write("vn %.4f %.4f %.4f\n" % (n[0]/l, n[1]/l, n[2]/l))
            for q in of:
                for tri in ((q[0], q[1], q[2]), (q[0], q[2], q[3])):
                    fh.write("f " + " ".join("%d/%d/%d" % (vi + t, vi + t, vi + t) for t in tri) + "\n")
            vi += len(ov)
    print("EXPORTED", path)

def add_cyl(objects, name, x0, y0, z0, x1, y1, z1, r, seg=10):
    ov, of = objects.setdefault(name, ([], []))
    dx, dy, dz = x1 - x0, y1 - y0, z1 - z0
    L = math.sqrt(dx*dx + dy*dy + dz*dz)
    ux, uy, uz = dx/L, dy/L, dz/L
    if abs(uy) < 0.9: ax, ay, az = 0, 1, 0
    else: ax, ay, az = 1, 0, 0
    bx, by, bz = ay*uz - az*uy, az*ux - ax*uz, ax*uy - ay*ux
    bl = math.sqrt(bx*bx + by*by + bz*bz)
    bx, by, bz = bx/bl, by/bl, bz/bl
    cx2, cy2, cz2 = uy*bz - uz*by, uz*bx - ux*bz, ux*by - uy*bx
    ofs = len(ov)
    for end in range(2):
        ex, ey, ez = (x0, y0, z0) if end == 0 else (x1, y1, z1)
        for i in range(seg):
            a = i / seg * 2 * math.pi
            ca, sa = math.cos(a), math.sin(a)
            ov.append((ex + (bx*ca + cx2*sa)*r, ey + (by*ca + cy2*sa)*r, ez + (bz*ca + cz2*sa)*r))
    for i in range(seg):
        j = (i + 1) % seg
        of.append((ofs + i, ofs + j, ofs + seg + j, ofs + seg + i))
