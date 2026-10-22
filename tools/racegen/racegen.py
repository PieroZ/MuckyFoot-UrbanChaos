#!/usr/bin/env python3
"""
racegen.py - race track generator for Urban Chaos (256x256 "big map" fork).

Makes a new mission from scratch on a 256x256 map:

    data/<name>.iam              the map (grass, hills, an asphalt circuit, trees)
    data/Lighting/<name>.lgt     lighting (floodlights along the track)
    levels/<name>.ucm            the mission (testdrive1a.ucm moved onto the new track)
    clumps/<name>.txc            textures (a copy of testdrive1a.txc)
    <name>_preview.png           a picture of the map

Usage:

    python racegen.py --game "C:\\Urban Chaos Retail" [--name bigrace] [--seed 7]
                      [--laps 3] [--width 9] [--out <dir>] [--install]

--game     the Urban Chaos folder (needs levels/testdrive1a.ucm, data/Lighting/oval1.lgt
           and clumps/testdrive1a.txc from the retail game).
--out      where to write (default: ./out). With --install the files go straight into
           the game folder and the mission is added to data/urban.sty (a backup
           urban.sty.bak is made first).

Only the textures and objects that testdrive1a already uses are used, so the copied
texture clump has everything the map needs.
"""

import argparse
import math
import os
import random
import shutil
import struct
import sys

SIZE = 256                  # mapsquares
BIG_MAP_SAVE_TYPE = 100     # added to save_type in big .iam files (see pap.h)
SAVE_TYPE = 25              # the format of the retail maps
TEXTURE_SET = 20            # the "oval circuit" world textures

# Floor textures (texture page numbers, all used by oval1.iam)

TEX_GRASS      = 470
TEX_GRASS_DARK = 285
TEX_ROAD       = 351        # plain asphalt
TEX_ROAD_EDGE  = 323        # asphalt with a white line along one side
TEX_ROAD_CROSS = 328        # cross hatching (start / finish)
TEX_VERGE      = 296        # concrete
TEX_DIRT       = 302

# Rotation (bits 10-11 of the texture word) that puts the edge line of
# TEX_ROAD_EDGE on the side facing -x, +x, -z, +z (worked out from oval1.iam).

EDGE_ROT = {(-1, 0): 0, (1, 0): 2, (0, -1): 3, (0, 1): 1}

PRIM_TREES = (58, 59)       # the street trees used all over road4.iam / oval1.iam

ALT_SHIFT = 3               # PAP_ALT_SHIFT


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

def smoothstep(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


class Noise:
    """Value noise, a few octaves - for hills and grass patches."""

    def __init__(self, seed, cell):
        self.r = random.Random(seed)
        self.cell = cell
        self.g = {}

    def _v(self, i, j):
        k = (i, j)
        if k not in self.g:
            self.g[k] = self.r.uniform(-1, 1)
        return self.g[k]

    def __call__(self, x, z):
        x /= self.cell
        z /= self.cell
        i, j = math.floor(x), math.floor(z)
        fx, fz = x - i, z - j
        fx = fx * fx * (3 - 2 * fx)
        fz = fz * fz * (3 - 2 * fz)
        a = self._v(i, j) * (1 - fx) + self._v(i + 1, j) * fx
        b = self._v(i, j + 1) * (1 - fx) + self._v(i + 1, j + 1) * fx
        return a * (1 - fz) + b * fz


def catmull_rom_loop(pts, per_seg):
    out = []
    n = len(pts)
    for i in range(n):
        p0, p1, p2, p3 = pts[(i - 1) % n], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
        for k in range(per_seg):
            t = k / per_seg
            t2, t3 = t * t, t * t * t
            out.append(tuple(
                0.5 * ((2 * p1[c]) + (-p0[c] + p2[c]) * t +
                       (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t2 +
                       (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t3)
                for c in range(2)))
    return out


class Polyline:
    """A closed polyline with arc length, projection and tangents."""

    def __init__(self, pts):
        self.p = pts
        self.n = len(pts)
        self.s = [0.0]
        for i in range(self.n):
            a, b = pts[i], pts[(i + 1) % self.n]
            self.s.append(self.s[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
        self.length = self.s[-1]

    def at(self, s):
        """Point and unit tangent at arc length s."""
        s %= self.length
        lo, hi = 0, self.n
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if self.s[mid] <= s:
                lo = mid
            else:
                hi = mid
        a, b = self.p[lo], self.p[(lo + 1) % self.n]
        seg = self.s[lo + 1] - self.s[lo]
        t = (s - self.s[lo]) / seg if seg else 0
        tx, tz = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
        return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t), (tx, tz)

    def project(self, x, z):
        """(s, signed lateral distance, distance) of the nearest point. +d = right of travel."""
        best = None
        for i in range(self.n):
            a, b = self.p[i], self.p[(i + 1) % self.n]
            dx, dz = b[0] - a[0], b[1] - a[1]
            l2 = dx * dx + dz * dz
            t = 0 if l2 == 0 else max(0, min(1, ((x - a[0]) * dx + (z - a[1]) * dz) / l2))
            px, pz = a[0] + dx * t, a[1] + dz * t
            d2 = (x - px) ** 2 + (z - pz) ** 2
            if best is None or d2 < best[0]:
                l = math.sqrt(l2) or 1
                side = (dx * (z - a[1]) - dz * (x - a[0])) / l
                best = (d2, self.s[i] + t * l, side)
        return best[1], best[2], math.sqrt(best[0])


def heading_byte(tx, tz):
    """EventPoint.Direction: 0 = +z, 64 = +x (worked out from testdrive1a.ucm)."""
    return math.atan2(tx, tz) / (2 * math.pi) * 256


# ---------------------------------------------------------------------------
# The track
# ---------------------------------------------------------------------------

def make_track(rng, margin):
    """A random closed circuit inside the map."""
    c = SIZE / 2
    rmax = c - margin
    while True:
        n = rng.randint(10, 14)
        ctrl = []
        for i in range(n):
            a = 2 * math.pi * i / n + rng.uniform(-0.18, 0.18)
            r = rmax * rng.uniform(0.55, 1.0)
            ctrl.append((c + math.cos(a) * r, c + math.sin(a) * r))
        pts = catmull_rom_loop(ctrl, 24)
        if any(not (margin <= x <= SIZE - margin and margin <= z <= SIZE - margin) for x, z in pts):
            continue
        poly = Polyline(pts)
        # Reject tracks that come too close to themselves (would merge into a blob).
        ok = True
        step = 4.0
        samples = [poly.at(i * step)[0] for i in range(int(poly.length / step))]
        m = len(samples)
        for i in range(m):
            for j in range(i + 12, m):
                if (i + m - j) < 12:
                    continue
                if math.hypot(samples[i][0] - samples[j][0], samples[i][1] - samples[j][1]) < 26:
                    ok = False
                    break
            if not ok:
                break
        if ok:
            return poly


def start_on_straight(poly):
    """Rotates the loop so that it starts on its straightest bit."""
    best = None
    for i in range(0, int(poly.length), 2):
        _, (ax, az) = poly.at(i - 20)
        _, (bx, bz) = poly.at(i + 20)
        bend = abs(math.atan2(ax * bz - az * bx, ax * bx + az * bz))
        if best is None or bend < best[0]:
            best = (bend, i)
    pts = [poly.at(best[1] + k * poly.length / poly.n)[0] for k in range(poly.n)]
    return Polyline(pts)


# ---------------------------------------------------------------------------
# The map (.iam)
# ---------------------------------------------------------------------------

class Map:
    def __init__(self):
        self.tex = [[0] * SIZE for _ in range(SIZE)]     # [x][z]
        self.alt = [[0] * SIZE for _ in range(SIZE)]
        self.flags = [[0] * SIZE for _ in range(SIZE)]
        self.obs = []                                     # (x, y, z, prim, yaw) world coords

    def ground(self, wx, wz):
        """Ground height (world units) at a world position."""
        x, z = wx / 256.0, wz / 256.0
        i, j = int(min(SIZE - 2, max(0, math.floor(x)))), int(min(SIZE - 2, max(0, math.floor(z))))
        fx, fz = x - i, z - j
        a = self.alt[i][j] * (1 - fx) + self.alt[i + 1][j] * fx
        b = self.alt[i][j + 1] * (1 - fx) + self.alt[i + 1][j + 1] * fx
        return int(round((a * (1 - fz) + b * fz) * (1 << ALT_SHIFT)))


def build_map(poly, width, rng, seed):
    m = Map()
    half = width / 2.0
    hills = Noise(seed * 7 + 1, 40)
    hills2 = Noise(seed * 7 + 2, 13)
    patches = Noise(seed * 7 + 3, 9)

    # Coarse acceleration: only project points that might be near the track.
    near = set()
    step = 1.0
    for i in range(int(poly.length / step)):
        (px, pz), _ = poly.at(i * step)
        for dx in range(-int(half) - 9, int(half) + 10):
            for dz in range(-int(half) - 9, int(half) + 10):
                xx, zz = int(px) + dx, int(pz) + dz
                if 0 <= xx < SIZE and 0 <= zz < SIZE:
                    near.add((xx, zz))

    # Heights: rolling hills, with the track itself on a smooth, gentle profile.
    track_level = lambda s: 4.0 * math.sin(2 * math.pi * s / poly.length * 3) + 2.0 * math.sin(2 * math.pi * s / poly.length * 7 + 1)

    for x in range(SIZE):
        for z in range(SIZE):
            h = 14 * hills(x, z) + 4 * hills2(x, z)
            if (x, z) in near:
                s, d, dist = poly.project(x, z)
                w = smoothstep(half + 1.5, half + 8, dist)
                h = track_level(s) * (1 - w) + h * w
            # Flat-ish edge of the map.
            edge = min(x, z, SIZE - 1 - x, SIZE - 1 - z)
            h *= smoothstep(0, 10, edge)
            m.alt[x][z] = int(round(max(-40, min(40, h))))

    # Floor textures, from the square centres.
    start_s = 0.0
    for x in range(SIZE):
        for z in range(SIZE):
            cx, cz = x + 0.5, z + 0.5
            page, rot = TEX_GRASS, rng.randint(0, 3)
            if patches(x, z) > 0.6:
                page = TEX_GRASS_DARK
            if (x, z) in near:
                s, d, dist = poly.project(cx, cz)
                if dist < half:
                    page, rot = TEX_ROAD, 0
                    if dist > half - 1:
                        # Edge line facing outwards (snapped to an axis).
                        (px, pz), _ = poly.at(s)
                        ox, oz = cx - px, cz - pz
                        if abs(ox) > abs(oz):
                            key = (1 if ox > 0 else -1, 0)
                        else:
                            key = (0, 1 if oz > 0 else -1)
                        page, rot = TEX_ROAD_EDGE, EDGE_ROT[key]
                    ds = (s - start_s + poly.length / 2) % poly.length - poly.length / 2
                    if -1.0 <= ds < 1.0:
                        page, rot = TEX_ROAD_CROSS, 0
                elif dist < half + 1.2:
                    page, rot = TEX_VERGE, 0
                elif dist < half + 2.5 and patches(x * 3, z * 3) > 0.2:
                    page, rot = TEX_DIRT, rng.randint(0, 3)
            m.tex[x][z] = page | (rot << 10)

    # Trees on the grass, away from the track.
    tries = 0
    while len(m.obs) < 700 and tries < 20000:
        tries += 1
        x = rng.uniform(6, SIZE - 6)
        z = rng.uniform(6, SIZE - 6)
        if (int(x), int(z)) in near:
            s, d, dist = poly.project(x, z)
            if dist < half + 6:
                continue
        if hills(x * 2, z * 2) < -0.2 and rng.random() < 0.7:
            continue            # leave some open fields
        wx, wz = int(x * 256), int(z * 256)
        m.obs.append((wx, m.ground(wx, wz), wz, rng.choice(PRIM_TREES), rng.randint(0, 255)))

    return m


def write_iam(m, path):
    out = bytearray()
    out += struct.pack('<i', SAVE_TYPE + BIG_MAP_SAVE_TYPE)
    out += struct.pack('<i', SIZE)
    out += struct.pack('<i', 0)                      # ob_size (not used)

    # PAP_Hi rows: Texture, Flags, Alt, Height.
    for x in range(SIZE):
        for z in range(SIZE):
            out += struct.pack('<HHbb', m.tex[x][z], m.flags[x][z], m.alt[x][z], 0)

    out += struct.pack('<H', 0)                      # no animating prims

    # No buildings: just the unused element 0 of every array.
    out += struct.pack('<5H', 1, 1, 1, 1, 1)         # dbuildings, dfacets, dstyles, paint_mem, dstoreys
    out += bytes(24) + bytes(26) + bytes(2) + bytes(1) + bytes(6)
    out += struct.pack('<HHi', 1, 1, 1)              # inside storeys / stairs / blocks
    out += bytes(22) + bytes(10) + bytes(1)
    out += struct.pack('<HH', 1, 1)                  # walkables, roof faces
    out += bytes(22) + bytes(12)                     # big maps store 12-byte roof faces

    # Objects, sorted into the lo-res mapwho.
    lo = SIZE // 4
    buckets = {}
    for ob in m.obs:
        wx, wy, wz, prim, yaw = ob
        key = (wx >> 10, wz >> 10)
        b = buckets.setdefault(key, [])
        if len(b) < 31:
            b.append(ob)
    obs = [None]
    mapwho = [[(0, 0)] * lo for _ in range(lo)]
    for lx in range(lo):
        for lz in range(lo):
            b = buckets.get((lx, lz), [])
            if b:
                mapwho[lx][lz] = (len(obs), len(b))
                obs.extend(b)
    assert len(obs) < 2048
    out += struct.pack('<i', len(obs))
    out += bytes(8)
    for wx, wy, wz, prim, yaw in obs[1:]:
        out += struct.pack('<hBBBBBB', wy, (wx & 0x3ff) >> 2, (wz & 0x3ff) >> 2, prim, yaw, 0, 0)
    for lx in range(lo):
        for lz in range(lo):
            idx, num = mapwho[lx][lz]
            out += struct.pack('<H', (idx & 2047) | (num << 11))

    out += struct.pack('<i', TEXTURE_SET)
    out += bytes(2 * 200 * 5)                        # psx_textures_xy
    open(path, 'wb').write(out)
    return len(obs) - 1


# ---------------------------------------------------------------------------
# Lighting (.lgt)
# ---------------------------------------------------------------------------

def write_lgt(template, poly, width, m, path):
    d = bytearray(open(template, 'rb').read())
    size_light, max_lights, _ = struct.unpack('<3i', d[:12])
    size_light &= 0xffff
    assert size_light == 20
    # Clear all lights.
    for i in range(max_lights):
        d[12 + i * 20:32 + i * 20] = bytes(20)
    n = 1
    s = 0.0
    while s < poly.length and n < max_lights:
        (px, pz), (tx, tz) = poly.at(s)
        side = 1 if (n & 1) else -1
        lx = (px + (-tz) * side * (width / 2 + 3)) * 256
        lz = (pz + (tx) * side * (width / 2 + 3)) * 256
        rec = struct.pack('<BbbbBBBB3i', 255, 64, 99, 118, (n + 1) % max_lights, 1, 0, 0,
                          int(lx), m.ground(lx, lz) + 384, int(lz))
        d[12 + n * 20:32 + n * 20] = rec
        n += 1
        s += 22.0
    # ed_light_free: first unused light.
    off = 12 + max_lights * 20
    d[off:off + 4] = struct.pack('<i', n)
    open(path, 'wb').write(d)
    return n - 1


# ---------------------------------------------------------------------------
# The mission (.ucm): testdrive1a moved from oval1 onto the new track
# ---------------------------------------------------------------------------

UCM_HDR = 4 + 4 + 260 * 5 + 6 + 2
EP_SIZE = 74
WPT_CREATE_ENEMIES = 3
WPT_CREATE_PLAYER = 2
WPT_CREATE_VEHICLE = 4
TT_VISIBLECOUNTDOWN = 21
TT_COUNTDOWN = 19


def oval_centreline():
    """The middle of the oval1 circuit, in the direction testdrive1a is driven."""
    pts = []
    xe, xw, zn, zs = 61.5, 47.0, 34.0, 88.0
    cx, r = (xe + xw) / 2, (xe - xw) / 2
    for i in range(40):                      # east straight, going +z
        pts.append((xe, zn + (zs - zn) * i / 40))
    for i in range(30):                      # south turn
        a = math.pi * i / 30
        pts.append((cx + r * math.cos(a), zs + r * math.sin(a)))
    for i in range(40):                      # west straight, going -z
        pts.append((xw, zs - (zs - zn) * i / 40))
    for i in range(30):                      # north turn
        a = math.pi + math.pi * i / 30
        pts.append((cx + r * math.cos(a), zn + r * math.sin(a)))
    return Polyline(pts)


def set_str(d, index, text):
    off = 8 + index * 260
    b = text.encode('latin-1')
    d[off:off + 260] = b + bytes(260 - len(b))


def write_ucm(template, poly, width, m, name, laps_time_scale, path):
    d = bytearray(open(template, 'rb').read())
    oval = oval_centreline()

    set_str(d, 1, 'Data\\Lighting\\%s.lgt' % name)
    set_str(d, 2, 'Data\\%s.iam' % name)
    set_str(d, 3, name)

    # Where the start (the squad car) is on the oval.
    eps = []
    for i in range(512):
        off = UCM_HDR + i * EP_SIZE
        eps.append(off)

    car = None
    for off in eps:
        if d[off + 3] and d[off + 2] == WPT_CREATE_VEHICLE:
            x, y, z = struct.unpack('<3i', d[off + 58:off + 70])
            car = (x / 256.0, z / 256.0)
            break
    s_car, _, _ = oval.project(*car)

    # The car starts a little before the start/finish line of the new track.
    s_start_new = -6.0
    scale = poly.length / oval.length

    moved = 0
    for off in eps:
        if not d[off + 3]:
            continue
        wpt = d[off + 2]
        trig = d[off + 4]
        x, y, z = struct.unpack('<3i', d[off + 58:off + 70])
        ox, oz = x / 256.0, z / 256.0
        s, side, dist = oval.project(ox, oz)
        s_rel = ((s - s_car) % oval.length)
        s_new = s_start_new + s_rel * scale
        (px, pz), (tx, tz) = poly.at(s_new)
        _, (otx, otz) = oval.at(s)
        # Keep the distance to the middle of the track; things far away
        # (the grandstands) are brought in a bit.
        lateral = side
        if abs(lateral) > width / 2 + 10:
            lateral = math.copysign(width / 2 + 10, lateral)
        nx = px + (-tz) * lateral
        nz = pz + (tx) * lateral
        nx = min(SIZE - 3, max(2, nx))
        nz = min(SIZE - 3, max(2, nz))
        wx, wz = int(nx * 256), int(nz * 256)
        ground = m.ground(wx, wz)
        if wpt in (WPT_CREATE_ENEMIES, WPT_CREATE_PLAYER, WPT_CREATE_VEHICLE) or y < 200:
            wy = ground
        else:
            wy = ground + y                  # cameras etc.: same height above the ground
        d[off + 58:off + 70] = struct.pack('<3i', wx, wy, wz)

        # Turn it by as much as the track turns.
        turn = heading_byte(tx, tz) - heading_byte(otx, otz)
        d[off + 6] = int(round(d[off + 6] + turn)) & 0xff

        # Time limits grow with the track.
        if trig in (TT_VISIBLECOUNTDOWN, TT_COUNTDOWN):
            rad = struct.unpack('<i', d[off + 54:off + 58])[0]
            if rad > 100:
                d[off + 54:off + 58] = struct.pack('<i', int(rad * laps_time_scale))
        moved += 1

    open(path, 'wb').write(d)
    return moved, scale


# ---------------------------------------------------------------------------
# Preview
# ---------------------------------------------------------------------------

def write_preview(m, poly, path):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return False
    col = {TEX_GRASS: (70, 140, 50), TEX_GRASS_DARK: (40, 90, 40), TEX_ROAD: (70, 70, 75),
           TEX_ROAD_EDGE: (200, 200, 200), TEX_ROAD_CROSS: (230, 230, 60),
           TEX_VERGE: (150, 150, 150), TEX_DIRT: (120, 90, 50)}
    im = Image.new('RGB', (SIZE, SIZE))
    for x in range(SIZE):
        for z in range(SIZE):
            c = col.get(m.tex[x][z] & 0x3ff, (255, 0, 255))
            k = 1 + m.alt[x][z] / 120.0
            im.putpixel((x, z), tuple(max(0, min(255, int(v * k))) for v in c))
    for wx, wy, wz, prim, yaw in m.obs:
        im.putpixel((wx >> 8, wz >> 8), (20, 60, 20))
    im = im.resize((SIZE * 3, SIZE * 3), Image.NEAREST)
    dr = ImageDraw.Draw(im)
    (px, pz), (tx, tz) = poly.at(0)
    dr.line([(px * 3, pz * 3), ((px + tx * 15) * 3, (pz + tz * 15) * 3)], fill=(255, 0, 0), width=3)
    im.save(path)
    return True


# ---------------------------------------------------------------------------

def add_to_story(sty_path, name, title):
    text = open(sty_path, 'rb').read().decode('latin-1')
    if (' %s.ucm ' % name) in text:
        return False
    shutil.copyfile(sty_path, sty_path + '.bak')
    ids = [int(l.split(':')[0]) for l in text.splitlines() if l[:1].isdigit()]
    new_id = max(ids) + 1
    line = '%d : 0 : 1 : 0 : 1 : 0 : 25 : %s.ucm : %s : Big map race - complete 3 laps of the long circuit within the time limit.' % (new_id, name, title)
    nl = '\r\n' if '\r\n' in text else '\n'
    pos = text.find('[districts]')
    # Put it just before the comment line that ends the mission list.
    cut = text.rfind('//', 0, pos)
    text = text[:cut] + line + nl + text[cut:]
    open(sty_path, 'wb').write(text.encode('latin-1'))
    return new_id


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', required=True)
    ap.add_argument('--name', default='bigrace')
    ap.add_argument('--title', default='Big Race')
    ap.add_argument('--seed', type=int, default=7)
    ap.add_argument('--width', type=float, default=9)
    ap.add_argument('--out', default='out')
    ap.add_argument('--install', action='store_true')
    a = ap.parse_args()

    name = a.name
    if len(name) > 30 or not name.replace('_', '').isalnum():
        sys.exit('bad --name')

    tmpl_ucm = os.path.join(a.game, 'levels', 'testdrive1a.ucm')
    tmpl_lgt = os.path.join(a.game, 'data', 'Lighting', 'oval1.lgt')
    tmpl_txc = os.path.join(a.game, 'clumps', 'testdrive1a.txc')
    for f in (tmpl_ucm, tmpl_lgt):
        if not os.path.exists(f):
            sys.exit('missing ' + f)

    root = a.game if a.install else a.out
    for sub in ('data', os.path.join('data', 'Lighting'), 'levels', 'clumps'):
        os.makedirs(os.path.join(root, sub), exist_ok=True)

    rng = random.Random(a.seed)
    print('making the track...')
    poly = make_track(rng, margin=14 + a.width)
    poly = start_on_straight(poly)
    print('  length %.0f mapsquares' % poly.length)

    print('making the map...')
    m = build_map(poly, a.width, rng, a.seed)
    n_obs = write_iam(m, os.path.join(root, 'data', name + '.iam'))
    print('  %d trees' % n_obs)

    n_lights = write_lgt(tmpl_lgt, poly, a.width, m, os.path.join(root, 'data', 'Lighting', name + '.lgt'))
    print('  %d lights' % n_lights)

    oval_len = oval_centreline().length
    time_scale = poly.length / oval_len * 1.15
    moved, scale = write_ucm(tmpl_ucm, poly, a.width, m, name, time_scale,
                             os.path.join(root, 'levels', name + '.ucm'))
    print('  mission: %d event points moved, track %.1fx as long as the oval' % (moved, scale))

    if os.path.exists(tmpl_txc):
        dst = os.path.join(root, 'clumps', name + '.txc')
        if not os.path.exists(dst):
            shutil.copyfile(tmpl_txc, dst)
    else:
        print('  (no clumps/testdrive1a.txc - copy it to clumps/%s.txc yourself)' % name)

    if write_preview(m, poly, os.path.join(a.out if not a.install else '.', name + '_preview.png')):
        print('  preview written')

    if a.install:
        sty = os.path.join(a.game, 'data', 'urban.sty')
        r = add_to_story(sty, name, a.title)
        print('  urban.sty: ' + ('added as mission %d' % r if r else 'already there'))


if __name__ == '__main__':
    main()
