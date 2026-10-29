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
(the way a person faces at Angle 0). The faces are textured from prim pages
400 (turret) and 401 (terminal): server\\textures\\shared\\prims\\tex400hi.tga
and tex401hi.tga, made by uc_make_security_textures.py.

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
FACE_FLAG_FIXED = 1 << 5

DRAW_FLAGS = POLY_FLAG_GOURAD | POLY_FLAG_TEXTURED

#
# Texture pages (made by uc_make_security_textures.py). Prim page n lives in
# server\textures\shared\prims\texNNNhi.tga and is texture page 704 + n.
#

PRIM_PAGE_TURRET = 400
PRIM_PAGE_TERMINAL = 401

#
# Each page is four 32x32 pixel tiles. UVs go 0..32 across the page, so a
# tile is 16 UV units. (tile u, tile v) of each material:
#

METAL, HAZARD, BARREL, LENS = (0, 0), (1, 0), (0, 1), (1, 1)
CASE, SCREEN, KEYS, DARK = (0, 0), (1, 0), (0, 1), (1, 1)


class Mesh:
    """Every face gets its own corner points, so the game's per-point normals
    come out flat (crisp edges) and every face can have its own UVs."""

    def __init__(self, name, prim_page):
        self.name = name
        self.page = prim_page
        self.pts = []
        self.tris = []      # (a, b, c, (ua,va), (ub,vb), (uc,vc))

    def point(self, x, y, z):
        self.pts.append((int(round(x)), int(round(y)), int(round(z))))
        return len(self.pts) - 1

    def face(self, corners, mat):
        """A flat convex polygon (list of (x,y,z), wound counter-clockwise seen
        from outside), textured with tile 'mat' stretched over it."""
        # Which axis does it face along? Project onto the other two for UVs.
        ax, ay, az = corners[0]
        bx, by, bz = corners[1]
        cx, cy, cz = corners[2]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx - ax, cy - ay, cz - az
        n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
        k = max(range(3), key=lambda i: abs(n[i]))
        if k == 0:     # facing x: u along z, v down y
            proj = [(p[2] * (1 if n[0] < 0 else -1), -p[1]) for p in corners]
        elif k == 1:   # facing y: u along x, v along z
            proj = [(p[0], p[2] * (1 if n[1] > 0 else -1)) for p in corners]
        else:          # facing z: u along x, v down y
            proj = [(p[0] * (1 if n[2] > 0 else -1), -p[1]) for p in corners]
        pu = [p[0] for p in proj]
        pv = [p[1] for p in proj]
        u0, u1, v0, v1 = min(pu), max(pu), min(pv), max(pv)
        tu, tv = mat
        uv = []
        for (u, v) in proj:
            fu = (u - u0) / (u1 - u0) if u1 > u0 else 0.5
            fv = (v - v0) / (v1 - v0) if v1 > v0 else 0.5
            uv.append((tu * 16 + 1 + int(round(fu * 14)), tv * 16 + 1 + int(round(fv * 14))))
        idx = [self.point(*p) for p in corners]
        for i in range(1, len(idx) - 1):
            self.tris.append((idx[0], idx[i], idx[i + 1], uv[0], uv[i], uv[i + 1]))

    def quad(self, a, b, c, d, mat):
        self.face([a, b, c, d], mat)

    def _six(self, p, mats):
        """The six sides of a box-like thing from its 8 corners p[ix,iy,iz].
        mats: material for -x, +x, -y, +y, -z (front), +z (back)."""
        self.quad(p[0, 0, 0], p[0, 0, 1], p[0, 1, 1], p[0, 1, 0], mats[0])   # -x
        self.quad(p[1, 0, 0], p[1, 1, 0], p[1, 1, 1], p[1, 0, 1], mats[1])   # +x
        self.quad(p[0, 0, 0], p[1, 0, 0], p[1, 0, 1], p[0, 0, 1], mats[2])   # -y
        self.quad(p[0, 1, 0], p[0, 1, 1], p[1, 1, 1], p[1, 1, 0], mats[3])   # +y
        self.quad(p[0, 0, 0], p[0, 1, 0], p[1, 1, 0], p[1, 0, 0], mats[4])   # -z
        self.quad(p[0, 0, 1], p[1, 0, 1], p[1, 1, 1], p[0, 1, 1], mats[5])   # +z

    @staticmethod
    def _mats(mat):
        return mat if isinstance(mat, (list, tuple)) and len(mat) == 6 else [mat] * 6

    def box(self, x0, x1, y0, y1, z0, z1, mat):
        p = {}
        for ix, x in enumerate((x0, x1)):
            for iy, y in enumerate((y0, y1)):
                for iz, z in enumerate((z0, z1)):
                    p[ix, iy, iz] = (x, y, z)
        self._six(p, self._mats(mat))

    def sloped_box(self, x0, x1, y0, z0, z1, y1_front, y1_back, mat):
        """A box whose top slopes from y1_front (at z0) to y1_back (at z1)."""
        p = {}
        for ix, x in enumerate((x0, x1)):
            for iz, z in enumerate((z0, z1)):
                p[ix, 0, iz] = (x, y0, z)
                p[ix, 1, iz] = (x, y1_front if iz == 0 else y1_back, z)
        self._six(p, self._mats(mat))

    def prism(self, sides, r0, r1, y0, y1, mat, rot=0.0):
        """A closed n-sided column, radius r0 at the bottom and r1 at the top."""
        bot = []
        top = []
        for i in range(sides):
            a = rot + 2 * math.pi * i / sides
            bot.append((math.sin(a) * r0, y0, math.cos(a) * r0))
            top.append((math.sin(a) * r1, y1, math.cos(a) * r1))
        for i in range(sides):
            j = (i + 1) % sides
            self.quad(bot[i], bot[j], top[j], top[i], mat)
        self.face(list(reversed(bot)), mat)
        self.face(top, mat)

    def tilted_box(self, cx, cy, cz, w, h, d, tilt_deg, mat):
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
                    p[ix, iy, iz] = (cx + x, cy + yy, cz + zz)
        self._six(p, self._mats(mat))

    def finish(self):
        """Make every triangle wind like the game's meshes: the signed volume
        comes out negative. The builders make CCW-outward triangles (positive
        volume), so flip them all - then check."""
        self.tris = [(a, c, b, ua, uc, ub) for (a, b, c, ua, ub, uc) in self.tris]
        v = self.volume()
        assert v < 0, "%s: winding check failed (%f)" % (self.name, v)

    def volume(self):
        v = 0.0
        P = self.pts
        for t in self.tris:
            ax, ay, az = P[t[0]]
            bx, by, bz = P[t[1]]
            cx, cy, cz = P[t[2]]
            v += (ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)) / 6.0
        return v


def make_turret():
    m = Mesh("turret", PRIM_PAGE_TURRET)
    # Pedestal (octagonal, so it looks the same however the turret turns)
    m.prism(8, 30, 24, 0, 12, HAZARD, rot=math.pi / 8)
    m.prism(8, 16, 14, 12, 40, METAL, rot=math.pi / 8)
    # Turntable ring
    m.prism(8, 24, 24, 40, 46, BARREL, rot=math.pi / 8)
    # Armoured head, front (-z) sloped down
    m.sloped_box(-24, 24, 46, -24, 22, 72, 80, METAL)
    # Twin barrels
    m.box(-13, -6, 56, 63, -74, -24, BARREL)
    m.box(6, 13, 56, 63, -74, -24, BARREL)
    # Muzzle brakes
    m.box(-15, -4, 54, 65, -80, -72, BARREL)
    m.box(4, 15, 54, 65, -80, -72, BARREL)
    # Sensor housing on top with a red eye at the front
    m.box(-7, 7, 80, 90, -14, 2, METAL)
    m.box(-4, 4, 82, 88, -20, -14, [METAL, METAL, METAL, METAL, LENS, METAL])
    # Red eye on the front of the head too, between the barrels
    m.box(-4, 4, 64, 72, -26, -23, [METAL, METAL, METAL, METAL, LENS, METAL])
    m.finish()
    return m


def make_terminal():
    m = Mesh("terminal", PRIM_PAGE_TERMINAL)
    # Cabinet
    m.box(-30, 30, 0, 72, -14, 22, CASE)
    # Plinth
    m.box(-33, 33, 0, 6, -17, 25, DARK)
    # Sloping keyboard desk sticking out in front (keys on top)
    m.sloped_box(-28, 28, 62, -34, -14, 72, 80, [CASE, CASE, CASE, KEYS, CASE, CASE])
    # Monitor housing, leaned back a little, screen at the front
    m.tilted_box(0, 104, 6, 52, 40, 22, 12, [DARK, DARK, DARK, DARK, SCREEN, DARK])
    # Monitor stand
    m.box(-8, 8, 72, 86, 0, 12, DARK)
    m.finish()
    return m


def pack_part(mesh):
    out = bytearray()
    name = mesh.name.encode("ascii")[:31]
    out += name + b"\0" * (32 - len(name))
    out += struct.pack("<ii", 0, len(mesh.pts))
    for x, y, z in mesh.pts:
        out += struct.pack("<hhh", x, y, z)

    #
    # The faces are written the way the game's own .all files have them (the
    # "raw" editor form): a 256 x 256 virtual page made of 8 x 8 tiles of 32,
    # TexturePage = page / 64 and the tile chosen by where the UVs are. At load
    # time TEXTURE_fix_prim_textures() turns that into the real page + 0..32
    # UVs (the same as the game's bike, bat, ... prims), and level editors that
    # read .all files (pieroz-directx) find the texture the same way.
    #

    page = 704 + mesh.page              # prims\texNNNhi.tga is texture page 704 + NNN
    tex_page = page // 64
    tile = page % 64
    base_u = (tile % 8) * 32
    base_v = (tile // 8) * 32

    def raw(u, v):
        return base_u + min(u, 31), base_v + min(v, 31)

    out += struct.pack("<ii", 0, len(mesh.tris))
    for a, b, c, ua, ub, uc in mesh.tris:
        for u, v in (ua, ub, uc):
            assert 0 <= u <= 32 and 0 <= v <= 32
        ra, rb, rc = raw(*ua), raw(*ub), raw(*uc)
        # PrimFace3: TexturePage, DrawFlags, Points[3], UV[3][2], Bright[3],
        #            ThingIndex, Col2, FaceFlags, Type, ID
        out += struct.pack("<BB3H6B3hhHHBb",
                           tex_page, DRAW_FLAGS, a, b, c,
                           ra[0], ra[1], rb[0], rb[1], rc[0], rc[1],
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
