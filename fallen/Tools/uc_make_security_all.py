#!/usr/bin/env python3
"""
uc_make_security_all.py - builds the anim-prim file for the security system
(turrets and computer terminals, see Source/security.cpp).

    python uc_make_security_all.py [output.all]      (default: anim006.all)

Copy the result to Release\\data\\anim006.all. Slot 6 is BAT_TYPE_TURRET,
the game loads it as anim_chunk[6].

The file has two meshes (MultiObject[0] and [1]), both with one body part:
    mesh 0 - turret: octagonal pedestal, armoured head, twin barrels, sensor
    mesh 1 - computer terminal: cabinet, sloping keyboard desk, monitor
and 20 animations that are all the same single still keyframe (the game
turns the turret by turning the whole Thing, and raises / lowers the hidden
ones by moving it, so nothing has to be animated).

Units: about 1 unit = 1 cm, y is up, the front of both models is -z
(the way a person faces at Angle 0). The faces are untextured (gouraud,
lit grey) so the models don't depend on any texture page. To texture them
later, change DRAW_FLAGS / TEX_PAGE / the UVs below.

Format notes (see io.cpp load_anim_system / read_a_prim / load_insert_game_chunk):
    SLONG save_type (5), SLONG mesh_count
    per mesh: SLONG save_type (4), SLONG start_obj, SLONG end_obj
        per part: char name[32], SLONG sp, ep, PrimPoint[ep-sp] (3 x SWORD),
                  SLONG sf3, ef3, PrimFace3[] (28 bytes), SLONG sf4, ef4, PrimFace4[] (34 bytes)
    chunk: SLONG save_type (5), SLONG ElementCount, SWORD MaxPeopleTypes,
           SWORD MaxAnimFrames, SLONG MaxElements, SWORD MaxKeyFrames, SWORD MaxFightCols
           BodyDef[MaxPeopleTypes] (20 bytes), UWORD 666
           ULONG addr, GameKeyFrame[] (20 bytes, links as indices), UWORD 666
           ULONG addr, GameKeyFrameElement[] (20 bytes), UWORD 666
           GameKeyFrame*[MaxAnimFrames] (indices), UWORD 666
           ULONG addr, GameFightCol[] (16 bytes), UWORD 666
Triangles are wound the way the game's own meshes are (negative signed volume).
"""

import math
import struct
import sys

POLY_FLAG_GOURAD = 1 << 0
POLY_FLAG_TEXTURED = 1 << 1

DRAW_FLAGS = POLY_FLAG_GOURAD   # untextured
TEX_PAGE = 0


class Mesh:
    def __init__(self, name):
        self.name = name
        self.pts = []
        self.tris = []

    def point(self, x, y, z):
        self.pts.append((int(round(x)), int(round(y)), int(round(z))))
        return len(self.pts) - 1

    def tri(self, a, b, c):
        self.tris.append((a, b, c))

    def quad(self, a, b, c, d):
        # a b c d in order round the face
        self.tri(a, b, c)
        self.tri(a, c, d)

    def box(self, x0, x1, y0, y1, z0, z1):
        p = {}
        for ix, x in enumerate((x0, x1)):
            for iy, y in enumerate((y0, y1)):
                for iz, z in enumerate((z0, z1)):
                    p[ix, iy, iz] = self.point(x, y, z)
        # outward-facing, counter-clockwise seen from outside (fixed up later)
        self.quad(p[0, 0, 0], p[0, 0, 1], p[0, 1, 1], p[0, 1, 0])   # -x
        self.quad(p[1, 0, 0], p[1, 1, 0], p[1, 1, 1], p[1, 0, 1])   # +x
        self.quad(p[0, 0, 0], p[1, 0, 0], p[1, 0, 1], p[0, 0, 1])   # -y
        self.quad(p[0, 1, 0], p[0, 1, 1], p[1, 1, 1], p[1, 1, 0])   # +y
        self.quad(p[0, 0, 0], p[0, 1, 0], p[1, 1, 0], p[1, 0, 0])   # -z
        self.quad(p[0, 0, 1], p[1, 0, 1], p[1, 1, 1], p[0, 1, 1])   # +z

    def sloped_box(self, x0, x1, y0, z0, z1, y1_front, y1_back):
        """A box whose top slopes from y1_front (at z0) to y1_back (at z1)."""
        p = {}
        for ix, x in enumerate((x0, x1)):
            for iz, z in enumerate((z0, z1)):
                p[ix, 0, iz] = self.point(x, y0, z)
                p[ix, 1, iz] = self.point(x, y1_front if iz == 0 else y1_back, z)
        self.quad(p[0, 0, 0], p[0, 0, 1], p[0, 1, 1], p[0, 1, 0])
        self.quad(p[1, 0, 0], p[1, 1, 0], p[1, 1, 1], p[1, 0, 1])
        self.quad(p[0, 0, 0], p[1, 0, 0], p[1, 0, 1], p[0, 0, 1])
        self.quad(p[0, 1, 0], p[0, 1, 1], p[1, 1, 1], p[1, 1, 0])
        self.quad(p[0, 0, 0], p[0, 1, 0], p[1, 1, 0], p[1, 0, 0])
        self.quad(p[0, 0, 1], p[1, 0, 1], p[1, 1, 1], p[0, 1, 1])

    def prism(self, sides, r0, r1, y0, y1, rot=0.0):
        """A closed n-sided column, radius r0 at the bottom and r1 at the top."""
        bot = []
        top = []
        for i in range(sides):
            a = rot + 2 * math.pi * i / sides
            bot.append(self.point(math.sin(a) * r0, y0, math.cos(a) * r0))
            top.append(self.point(math.sin(a) * r1, y1, math.cos(a) * r1))
        cb = self.point(0, y0, 0)
        ct = self.point(0, y1, 0)
        for i in range(sides):
            j = (i + 1) % sides
            self.quad(bot[i], bot[j], top[j], top[i])
            self.tri(cb, bot[j], bot[i])
            self.tri(ct, top[i], top[j])

    def tilted_box(self, cx, cy, cz, w, h, d, tilt_deg):
        """A box centred on (cx,cy,cz), w wide (x), h high (y), d deep (z),
        leaned back by tilt_deg about the x axis."""
        t = math.radians(tilt_deg)
        ct, st = math.cos(t), math.sin(t)
        p = {}
        for ix, x in enumerate((-w / 2, w / 2)):
            for iy, y in enumerate((-h / 2, h / 2)):
                for iz, z in enumerate((-d / 2, d / 2)):
                    yy = y * ct - z * st
                    zz = y * st + z * ct
                    p[ix, iy, iz] = self.point(cx + x, cy + yy, cz + zz)
        self.quad(p[0, 0, 0], p[0, 0, 1], p[0, 1, 1], p[0, 1, 0])
        self.quad(p[1, 0, 0], p[1, 1, 0], p[1, 1, 1], p[1, 0, 1])
        self.quad(p[0, 0, 0], p[1, 0, 0], p[1, 0, 1], p[0, 0, 1])
        self.quad(p[0, 1, 0], p[0, 1, 1], p[1, 1, 1], p[1, 1, 0])
        self.quad(p[0, 0, 0], p[0, 1, 0], p[1, 1, 0], p[1, 0, 0])
        self.quad(p[0, 0, 1], p[1, 0, 1], p[1, 1, 1], p[0, 1, 1])

    def finish(self):
        """Make every triangle wind like the game's meshes: the signed volume of
        each closed piece comes out negative. Our builders make CCW-outward
        triangles (positive volume), so flip them all - then check."""
        self.tris = [(a, c, b) for (a, b, c) in self.tris]
        v = self.volume()
        assert v < 0, "%s: winding check failed (%f)" % (self.name, v)

    def volume(self):
        v = 0.0
        P = self.pts
        for a, b, c in self.tris:
            ax, ay, az = P[a]
            bx, by, bz = P[b]
            cx, cy, cz = P[c]
            v += (ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)) / 6.0
        return v


def make_turret():
    m = Mesh("turret")
    # Pedestal (octagonal, so it looks the same however the turret turns)
    m.prism(8, 30, 24, 0, 12, rot=math.pi / 8)
    m.prism(8, 16, 14, 12, 40, rot=math.pi / 8)
    # Turntable ring
    m.prism(8, 24, 24, 40, 46, rot=math.pi / 8)
    # Armoured head, front (-z) sloped down
    m.sloped_box(-24, 24, 46, -24, 22, 72, 80)
    # Twin barrels
    m.box(-13, -6, 56, 63, -74, -24)
    m.box(6, 13, 56, 63, -74, -24)
    # Muzzle brakes
    m.box(-15, -4, 54, 65, -80, -72)
    m.box(4, 15, 54, 65, -80, -72)
    # Sensor dome / camera on top
    m.box(-7, 7, 80, 90, -14, 2)
    m.box(-4, 4, 82, 88, -20, -14)
    m.finish()
    return m


def make_terminal():
    m = Mesh("terminal")
    # Cabinet
    m.box(-30, 30, 0, 72, -14, 22)
    # Plinth
    m.box(-33, 33, 0, 6, -17, 25)
    # Sloping keyboard desk sticking out in front
    m.sloped_box(-28, 28, 62, -34, -14, 72, 80)
    # Monitor housing, leaned back a little
    m.tilted_box(0, 104, 6, 52, 40, 22, 12)
    # Monitor stand
    m.box(-8, 8, 72, 86, 0, 12)
    m.finish()
    return m


def pack_part(mesh):
    out = bytearray()
    name = mesh.name.encode("ascii")[:31]
    out += name + b"\0" * (32 - len(name))
    out += struct.pack("<ii", 0, len(mesh.pts))
    for x, y, z in mesh.pts:
        out += struct.pack("<hhh", x, y, z)
    out += struct.pack("<ii", 0, len(mesh.tris))
    for a, b, c in mesh.tris:
        # PrimFace3: TexturePage, DrawFlags, Points[3], UV[3][2], Bright[3],
        #            ThingIndex, Col2, FaceFlags, Type, ID
        out += struct.pack("<BB3H6B3hhHHBb",
                           TEX_PAGE, DRAW_FLAGS, a, b, c,
                           0, 0, 0, 0, 0, 0,
                           0, 0, 0,
                           0, 0, 0, 0, 0)
    out += struct.pack("<ii", 0, 0)   # no quads
    return out


def build(meshes):
    out = bytearray()
    out += struct.pack("<ii", 5, len(meshes))
    for m in meshes:
        out += struct.pack("<iii", 4, 0, 1)    # one part: object 0..1
        out += pack_part(m)

    element_count = 1
    max_people_types = 10
    max_anims = 20
    max_elements = 1
    max_keyframes = 2
    max_fightcols = 1

    out += struct.pack("<iihhihh", 5, element_count, max_people_types, max_anims,
                       max_elements, max_keyframes, max_fightcols)

    # Body defs: part 0 everywhere
    out += bytes(20 * max_people_types)
    out += struct.pack("<H", 666)

    # Keyframes: 0 = unused, 1 = the still frame, looping onto itself
    out += struct.pack("<I", 0)
    out += struct.pack("<BBhiiii", 0, 0, 0, 0, -1, -1, -1)
    out += struct.pack("<BBhiiii", 0, 25, 1, 0, 1, 1, -1)
    out += struct.pack("<H", 666)

    # Elements: identity matrix (three rows of 10-bit 511s), no offset
    out += struct.pack("<I", 0)
    out += struct.pack("<iiihhhH", 511 << 20, 511 << 10, 511, 0, 0, 0, 0)
    out += struct.pack("<H", 666)

    # Anim list: everything is keyframe 1
    out += struct.pack("<%di" % max_anims, *([0] + [1] * (max_anims - 1)))
    out += struct.pack("<H", 666)

    # One empty fight col
    out += struct.pack("<I", 0)
    out += struct.pack("<HHBBBBBBBBi", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1)
    out += struct.pack("<H", 666)
    return bytes(out)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "anim006.all"
    meshes = [make_turret(), make_terminal()]
    data = build(meshes)
    with open(path, "wb") as f:
        f.write(data)
    for m in meshes:
        xs = [p[0] for p in m.pts]
        ys = [p[1] for p in m.pts]
        zs = [p[2] for p in m.pts]
        print("%-9s %3d points %3d triangles  x %d..%d  y %d..%d  z %d..%d" %
              (m.name, len(m.pts), len(m.tris), min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
    print("wrote %s (%d bytes)" % (path, len(data)))


if __name__ == "__main__":
    main()
