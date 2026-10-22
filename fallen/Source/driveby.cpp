//
// Drive-by: shooting the pistol out of the car window. See driveby.h
//

#include "game.h"
#include "statedef.h"
#include "pcom.h"
#include "combat.h"
#include "collide.h"
#include "pyro.h"
#include "mfx.h"
#include "sound.h"
#include "night.h"
#include "Special.h"
#include "pap.h"
#include "vehicle.h"
#include "ddlib.h"
#include "font2d.h"
#include "..\DDEngine\Headers\panel.h"
#include "..\DDEngine\Headers\poly.h"
#include "config_extras.h"
#include "fc.h"
#include "driveby.h"
#include "cnet.h"
#include <math.h>

extern SLONG VEH_flat_tyres     (Thing *p_car);
extern SLONG VEH_puncture_tyre  (Thing *p_car, SLONG wheel);
extern void  VEH_get_wheel_pos  (Thing *p_car, SLONG wheel, SLONG *wx, SLONG *wz);
extern void  VEH_get_half_size  (Thing *p_car, SLONG *half_width, SLONG *half_length);
extern void  VEH_reduce_health  (Thing *p_car, Thing *p_person, SLONG damage);
extern void  VEH_shot_at_tyres  (Thing *p_car, Thing *p_shooter, SLONG gun_type);

#define DRIVEBY_RANGE			0x1400		// How far you can shoot (0x100 = one mapsquare)
#define DRIVEBY_CONE_COS		(-0.25F)	// Targets in front and to the sides (up to ~105 degrees off the nose)
#define DRIVEBY_MAX_TARGETS		24
#define DRIVEBY_MAX_VISIBLE		8			// Line-of-sight is only checked for the best few
#define DRIVEBY_FIRE_MS			300			// Time between shots
#define DRIVEBY_RELOAD_MS		1200		// Putting a new clip in
#define DRIVEBY_PISTOL_DAMAGE	70			// Same as on foot
#define DRIVEBY_FIND			32
#define DRIVEBY_TYRE_DAMAGE		15			// A shot in the tyre hurts the car a bit too

typedef struct
{
	THING_INDEX thing;
	SBYTE       wheel;		// -1 => the person / the car body
	float       x;			// Where to aim (world units)
	float       y;
	float       z;
	float       score;		// Lower is better
	float       chance;		// Of hitting, 0..1

} DRIVEBY_Target;

static struct
{
	SLONG          init;
	SLONG          enabled;
	UBYTE          key_fire;
	UBYTE          key_next;
	UBYTE          key_tyres;
	float          aim_speed;	// Mouse sensitivity for the crosshair

	SLONG          active;		// Drive-by controls in use this frame
	SLONG          tyres;		// Aiming at tyres
	THING_INDEX    locked;		// The current target...
	SBYTE          locked_wheel;
	SLONG          has_target;
	DRIVEBY_Target target;		// ...and where it is this frame

	SLONG          was_next;	// Key states last frame
	SLONG          was_tyres;

	SLONG          aiming;		// Holding the right mouse button: aiming with the mouse
	float          aim_x;		// Where the crosshair is (screen pixels- the middle)
	float          aim_y;
	float          aim_yaw;		// Which way we're aiming (radians: direction (sin, cos) in x,z)
	float          aim_pitch;	// Up / down (radians)
	float          cam_blend;	// 0..1 how far the camera has moved to the window
	ULONG          cam_tick;	// GetTickCount() last time we moved it
	SLONG          cam_x;		// Where the window camera is and what it looks at
	SLONG          cam_y;
	SLONG          cam_z;
	SLONG          look_x;
	SLONG          look_y;
	SLONG          look_z;
	SLONG          has_aim_target;
	DRIVEBY_Target aim_target;	// What the crosshair is on (worked out when drawing)

	ULONG          next_shot;	// GetTickCount() when we can shoot again
	CBYTE          msg[64];
	ULONG          msg_until;

} DRIVEBY;

static void DRIVEBY_message(const CBYTE *msg, ULONG ms)
{
	strncpy(DRIVEBY.msg, msg, sizeof(DRIVEBY.msg) - 1);
	DRIVEBY.msg[sizeof(DRIVEBY.msg) - 1] = 0;
	DRIVEBY.msg_until = GetTickCount() + ms;
}

static void DRIVEBY_init(void)
{
	if (DRIVEBY.init)
	{
		return;
	}

	DRIVEBY.init      = TRUE;
	DRIVEBY.enabled   = ENV_get_value_number("driveby",       1,  "Extra");
	DRIVEBY.key_fire  = UBYTE(ENV_get_value_number("driveby_fire",  29, "Keyboard"));	// Left Ctrl
	DRIVEBY.key_next  = UBYTE(ENV_get_value_number("driveby_next",  42, "Keyboard"));	// Left Shift
	DRIVEBY.key_tyres = UBYTE(ENV_get_value_number("driveby_tyres", 58, "Keyboard"));	// Caps Lock
	DRIVEBY.aim_speed = float(ENV_get_value_number("driveby_mouse_speed", 100, "Extra")) * 0.01F;	// %
}

SLONG DRIVEBY_active(void)
{
	if (CNET_network_game)
	{
		return FALSE;
	}

	return DRIVEBY.active;
}

//
// How likely a shot is to hit.
//

static float DRIVEBY_hit_chance(Thing *p_car, Thing *p_target, SLONG wheel, float dist, SLONG aimed)
{
	float chance = 0.92F - 0.55F * (dist / float(DRIVEBY_RANGE));

	if (wheel >= 0)
	{
		chance *= (aimed) ? 0.75F : 0.6F;	// A tyre is a small target (easier when you aim yourself).
	}

	//
	// Harder when we're going fast...
	//

	float own = float(abs(p_car->Velocity)) / 1500.0F;

	if (own > 1.0F) {own = 1.0F;}

	chance -= 0.25F * own;

	//
	// ...and when they are.
	//

	if (p_target->Class == CLASS_VEHICLE)
	{
		float theirs = float(abs(p_target->Velocity)) / 1500.0F;

		if (theirs > 1.0F) {theirs = 1.0F;}

		chance -= 0.15F * theirs;
	}

	if (chance < 0.05F) {chance = 0.05F;}
	if (chance > 0.95F) {chance = 0.95F;}

	return chance;
}

//
// Finds everything we could shoot at and picks the target.
//

static void DRIVEBY_find_targets(Thing *p_person, Thing *p_car, SLONG want_next)
{
	SLONG i;
	SLONG j;
	SLONG w;

	DRIVEBY_Target cand[DRIVEBY_MAX_TARGETS];
	SLONG          num = 0;

	float sx = float(p_car->WorldPos.X >> 8);
	float sy = float(p_car->WorldPos.Y >> 8) + 80.0F;	// Out of the window
	float sz = float(p_car->WorldPos.Z >> 8);

	SLONG angle = p_car->Genus.Vehicle->Angle & 2047;

	float fx = -float(SIN(angle)) * (1.0F / 65536.0F);	// The way the car is pointing
	float fz = -float(COS(angle)) * (1.0F / 65536.0F);

	THING_INDEX found[DRIVEBY_FIND];

	SLONG num_found = THING_find_sphere(
						SLONG(sx), SLONG(sy), SLONG(sz),
						DRIVEBY_RANGE,
						found,
						DRIVEBY_FIND,
						(DRIVEBY.tyres) ? (1 << CLASS_VEHICLE) : ((1 << CLASS_VEHICLE) | (1 << CLASS_PERSON)));

	for (i = 0; i < num_found; i++)
	{
		Thing *p_found = TO_THING(found[i]);

		if (p_found == p_car || p_found == p_person)
		{
			continue;
		}

		if (p_found->State == STATE_DEAD)
		{
			continue;
		}

		//
		// The points on this thing we could aim at.
		//

		float px[4];
		float py[4];
		float pz[4];
		SBYTE pw[4];
		SLONG np = 0;

		if (p_found->Class == CLASS_VEHICLE)
		{
			if (DRIVEBY.tyres)
			{
				SLONG flat = VEH_flat_tyres(p_found);

				float cx = float(p_found->WorldPos.X >> 8);
				float cz = float(p_found->WorldPos.Z >> 8);

				for (w = 0; w < 4; w++)
				{
					if (flat & (1 << w))
					{
						continue;
					}

					SLONG wx;
					SLONG wz;

					VEH_get_wheel_pos(p_found, w, &wx, &wz);

					//
					// Only the tyres on the side facing us.
					//

					float ox = float(wx) - cx;
					float oz = float(wz) - cz;
					float tx = sx - cx;
					float tz = sz - cz;

					if (ox * tx + oz * tz < 0.0F)
					{
						continue;
					}

					px[np] = float(wx);
					py[np] = float(PAP_calc_map_height_at(wx, wz) + 18);
					pz[np] = float(wz);
					pw[np] = SBYTE(w);
					np    += 1;
				}
			}
			else
			{
				px[0] = float(p_found->WorldPos.X >> 8);
				py[0] = float(p_found->WorldPos.Y >> 8) + 64.0F;
				pz[0] = float(p_found->WorldPos.Z >> 8);
				pw[0] = -1;
				np    = 1;
			}
		}
		else
		if (p_found->Class == CLASS_PERSON)
		{
			if (p_found->Genus.Person->PlayerID)
			{
				continue;
			}

			if (p_found->State == STATE_DYING)
			{
				continue;
			}

			if (p_found->Genus.Person->Flags & (FLAG_PERSON_DRIVING | FLAG_PERSON_PASSENGER))
			{
				continue;	// Shoot his car instead.
			}

			px[0] = float(p_found->WorldPos.X >> 8);
			py[0] = float(p_found->WorldPos.Y >> 8) + 80.0F;
			pz[0] = float(p_found->WorldPos.Z >> 8);
			pw[0] = -1;
			np    = 1;
		}

		for (j = 0; j < np && num < DRIVEBY_MAX_TARGETS; j++)
		{
			float dx = px[j] - sx;
			float dz = pz[j] - sz;

			float dist = sqrtf(dx * dx + dz * dz);

			if (dist < 48.0F || dist > float(DRIVEBY_RANGE))
			{
				continue;
			}

			float cosang = (dx * fx + dz * fz) / dist;

			if (cosang < DRIVEBY_CONE_COS)
			{
				continue;	// Behind us.
			}

			DRIVEBY_Target *t = &cand[num++];

			t->thing  = found[i];
			t->wheel  = pw[j];
			t->x      = px[j];
			t->y      = py[j];
			t->z      = pz[j];
			t->score  = dist * (1.6F - cosang);	// Prefer things ahead of us
			t->chance = DRIVEBY_hit_chance(p_car, p_found, pw[j], dist, FALSE);
		}
	}

	//
	// Best first.
	//

	for (i = 1; i < num; i++)
	{
		DRIVEBY_Target t = cand[i];

		for (j = i - 1; j >= 0 && cand[j].score > t.score; j--)
		{
			cand[j + 1] = cand[j];
		}

		cand[j + 1] = t;
	}

	//
	// Keep the ones we can actually see.
	//

	DRIVEBY_Target vis[DRIVEBY_MAX_VISIBLE];
	SLONG          num_vis = 0;

	for (i = 0; i < num && num_vis < DRIVEBY_MAX_VISIBLE; i++)
	{
		if (there_is_a_los(
				SLONG(sx), SLONG(sy), SLONG(sz),
				SLONG(cand[i].x), SLONG(cand[i].y) + 8, SLONG(cand[i].z),
				LOS_FLAG_IGNORE_PRIMS))
		{
			vis[num_vis++] = cand[i];
		}
	}

	DRIVEBY.has_target = FALSE;

	if (num_vis == 0)
	{
		DRIVEBY.locked = 0;

		return;
	}

	//
	// Still got the same target?
	//

	SLONG current = -1;

	for (i = 0; i < num_vis; i++)
	{
		if (vis[i].thing == DRIVEBY.locked && vis[i].wheel == DRIVEBY.locked_wheel)
		{
			current = i;

			break;
		}
	}

	if (current < 0)
	{
		current = 0;		// Lost it- take the best one.
	}
	else
	if (want_next)
	{
		current = (current + 1) % num_vis;
	}

	DRIVEBY.target       = vis[current];
	DRIVEBY.locked       = vis[current].thing;
	DRIVEBY.locked_wheel = vis[current].wheel;
	DRIVEBY.has_target   = TRUE;
}

//
// Aiming with the mouse (right button held): the camera goes to the window
// on the side you're aiming at and the mouse turns it. The crosshair stays in
// the middle of the screen.
//

extern volatile HWND  hDDLibWindow;
extern volatile SLONG MouseAccDX;	// GMouse.cpp: all the mouse movement since we last took it
extern volatile SLONG MouseAccDY;
extern void RecenterMouse(void);

#define DRIVEBY_PITCH_UP		0.45F	// How far you can look up / down (radians)
#define DRIVEBY_PITCH_DOWN		0.55F
#define DRIVEBY_MOUSE_RAD		0.0035F	// Radians per mouse pixel at 100%
#define DRIVEBY_CAM_BLEND_MS	180.0F	// Time for the camera to move to / from the window

static void DRIVEBY_aim_start(void)
{
	DRIVEBY.has_aim_target = FALSE;

	//
	// Start aiming the way the camera is looking now.
	//

	FC_Cam *fc = &FC_cam[0];

	float dx = float((fc->focus_x >> 8) - (fc->x >> 8));
	float dz = float((fc->focus_z >> 8) - (fc->z >> 8));

	if (fabsf(dx) + fabsf(dz) < 1.0F)
	{
		dz = 1.0F;
	}

	DRIVEBY.aim_yaw   = atan2f(dx, dz);
	DRIVEBY.aim_pitch = -0.08F;		// Slightly down- where the tyres are

	MouseAccDX = 0;
	MouseAccDY = 0;

	RecenterMouse();

	ShowCursor(FALSE);
}

static void DRIVEBY_aim_stop(void)
{
	DRIVEBY.has_aim_target = FALSE;

	ShowCursor(TRUE);
}

static void DRIVEBY_aim_move(void)
{
	SLONG dx = MouseAccDX;
	SLONG dy = MouseAccDY;

	MouseAccDX = 0;
	MouseAccDY = 0;

	if (GetForegroundWindow() == hDDLibWindow)
	{
		RecenterMouse();	// So the cursor never gets stuck at the edge of the window.
	}

	float rad = DRIVEBY_MOUSE_RAD * DRIVEBY.aim_speed;

	DRIVEBY.aim_yaw   -= float(dx) * rad;	// Mouse right => turn right
	DRIVEBY.aim_pitch -= float(dy) * rad;	// Mouse up    => look up

	if (DRIVEBY.aim_pitch >  DRIVEBY_PITCH_UP)   {DRIVEBY.aim_pitch =  DRIVEBY_PITCH_UP;}
	if (DRIVEBY.aim_pitch < -DRIVEBY_PITCH_DOWN) {DRIVEBY.aim_pitch = -DRIVEBY_PITCH_DOWN;}

	if (DRIVEBY.aim_yaw >  3.14159265F) {DRIVEBY.aim_yaw -= 6.28318531F;}
	if (DRIVEBY.aim_yaw < -3.14159265F) {DRIVEBY.aim_yaw += 6.28318531F;}
}

//
// Where the window camera goes: just outside the car body on the side you're
// aiming at, at window height, looking along the aim.
//

static void DRIVEBY_place_camera(Thing *p_car)
{
	float dx = sinf(DRIVEBY.aim_yaw) * cosf(DRIVEBY.aim_pitch);
	float dy = sinf(DRIVEBY.aim_pitch);
	float dz = cosf(DRIVEBY.aim_yaw) * cosf(DRIVEBY.aim_pitch);

	float hx = sinf(DRIVEBY.aim_yaw);	// Flat aim direction
	float hz = cosf(DRIVEBY.aim_yaw);

	//
	// How far from the middle of the car to its side in that direction?
	//

	SLONG half_w;
	SLONG half_l;

	VEH_get_half_size(p_car, &half_w, &half_l);

	SLONG angle = p_car->Genus.Vehicle->Angle & 2047;

	float fx = -float(SIN(angle)) * (1.0F / 65536.0F);	// The car's nose
	float fz = -float(COS(angle)) * (1.0F / 65536.0F);
	float rx =  float(COS(angle)) * (1.0F / 65536.0F);	// The car's right
	float rz = -float(SIN(angle)) * (1.0F / 65536.0F);

	float along = fabsf(hx * fx + hz * fz);
	float side  = fabsf(hx * rx + hz * rz);

	float out = 1e6F;

	if (along > 0.01F) {float o = float(half_l) / along; if (o < out) {out = o;}}
	if (side  > 0.01F) {float o = float(half_w) / side;  if (o < out) {out = o;}}

	out += 24.0F;

	float carx = float(p_car->WorldPos.X >> 8);
	float cary = float(p_car->WorldPos.Y >> 8);
	float carz = float(p_car->WorldPos.Z >> 8);

	float cx = carx + hx * out;
	float cy = cary + 88.0F;
	float cz = carz + hz * out;

	//
	// Don't put it inside a wall.
	//

	SLONG i;

	for (i = 0; i < 3; i++)
	{
		if (there_is_a_los(
				SLONG(carx), SLONG(cy), SLONG(carz),
				SLONG(cx),   SLONG(cy), SLONG(cz),
				LOS_FLAG_IGNORE_PRIMS))
		{
			break;
		}

		cx = carx + (cx - carx) * 0.5F;
		cz = carz + (cz - carz) * 0.5F;
	}

	DRIVEBY.cam_x  = SLONG(cx);
	DRIVEBY.cam_y  = SLONG(cy);
	DRIVEBY.cam_z  = SLONG(cz);
	DRIVEBY.look_x = SLONG(cx + dx * 1024.0F);
	DRIVEBY.look_y = SLONG(cy + dy * 1024.0F);
	DRIVEBY.look_z = SLONG(cz + dz * 1024.0F);
}

//
// For fc.cpp: returns how much of the window camera to use (0..256).
//

SLONG DRIVEBY_camera(SLONG *x, SLONG *y, SLONG *z, SLONG *look_x, SLONG *look_y, SLONG *look_z)
{
	//
	// Ease the camera in and out.
	//

	ULONG now = GetTickCount();
	float dt  = float(now - DRIVEBY.cam_tick);

	DRIVEBY.cam_tick = now;

	if (dt > 100.0F) {dt = 100.0F;}

	float step = dt / DRIVEBY_CAM_BLEND_MS;

	if (DRIVEBY.aiming)
	{
		DRIVEBY.cam_blend += step;

		if (DRIVEBY.cam_blend > 1.0F) {DRIVEBY.cam_blend = 1.0F;}
	}
	else
	{
		DRIVEBY.cam_blend -= step;

		if (DRIVEBY.cam_blend < 0.0F) {DRIVEBY.cam_blend = 0.0F;}
	}

	if (DRIVEBY.cam_blend <= 0.0F)
	{
		return 0;
	}

	*x      = DRIVEBY.cam_x;
	*y      = DRIVEBY.cam_y;
	*z      = DRIVEBY.cam_z;
	*look_x = DRIVEBY.look_x;
	*look_y = DRIVEBY.look_y;
	*look_z = DRIVEBY.look_z;

	float b = DRIVEBY.cam_blend;

	b = b * b * (3.0F - 2.0F * b);		// Smooth start and finish

	return SLONG(b * 256.0F);
}

//
// What's under the crosshair? Called while drawing, when the camera is set up
// for POLY_transform(). Tyres, people and cars- whichever is nearest the
// crosshair on the screen (within a little snapping distance).
//

static void DRIVEBY_aim_pick(Thing *p_person, Thing *p_car)
{
	SLONG i;
	SLONG j;
	SLONG w;

	DRIVEBY.has_aim_target = FALSE;

	float sx = float(p_car->WorldPos.X >> 8);
	float sy = float(p_car->WorldPos.Y >> 8) + 80.0F;
	float sz = float(p_car->WorldPos.Z >> 8);

	float snap = float(DisplayWidth) * 0.06F;	// pixels

	THING_INDEX found[DRIVEBY_FIND];

	SLONG num_found = THING_find_sphere(
						SLONG(sx), SLONG(sy), SLONG(sz),
						DRIVEBY_RANGE,
						found,
						DRIVEBY_FIND,
						(1 << CLASS_VEHICLE) | (1 << CLASS_PERSON));

	DRIVEBY_Target cand[DRIVEBY_MAX_TARGETS];
	SLONG          num = 0;

	for (i = 0; i < num_found; i++)
	{
		Thing *p_found = TO_THING(found[i]);

		if (p_found == p_car || p_found == p_person || p_found->State == STATE_DEAD)
		{
			continue;
		}

		float px[5];
		float py[5];
		float pz[5];
		SBYTE pw[5];
		SLONG np = 0;

		if (p_found->Class == CLASS_VEHICLE)
		{
			SLONG flat = VEH_flat_tyres(p_found);

			for (w = 0; w < 4; w++)
			{
				if (flat & (1 << w))
				{
					continue;
				}

				SLONG wx;
				SLONG wz;

				VEH_get_wheel_pos(p_found, w, &wx, &wz);

				px[np] = float(wx);
				py[np] = float(PAP_calc_map_height_at(wx, wz) + 18);
				pz[np] = float(wz);
				pw[np] = SBYTE(w);
				np    += 1;
			}

			px[np] = float(p_found->WorldPos.X >> 8);
			py[np] = float(p_found->WorldPos.Y >> 8) + 64.0F;
			pz[np] = float(p_found->WorldPos.Z >> 8);
			pw[np] = -1;
			np    += 1;
		}
		else
		{
			if (p_found->Genus.Person->PlayerID || p_found->State == STATE_DYING)
			{
				continue;
			}

			if (p_found->Genus.Person->Flags & (FLAG_PERSON_DRIVING | FLAG_PERSON_PASSENGER))
			{
				continue;
			}

			px[0] = float(p_found->WorldPos.X >> 8);
			py[0] = float(p_found->WorldPos.Y >> 8) + 80.0F;
			pz[0] = float(p_found->WorldPos.Z >> 8);
			pw[0] = -1;
			np    = 1;
		}

		for (j = 0; j < np && num < DRIVEBY_MAX_TARGETS; j++)
		{
			float dx   = px[j] - sx;
			float dz   = pz[j] - sz;
			float dist = sqrtf(dx * dx + dz * dz);

			if (dist < 48.0F || dist > float(DRIVEBY_RANGE))
			{
				continue;
			}

			POLY_Point pp;

			POLY_transform(px[j], py[j], pz[j], &pp);

			if (!pp.IsValid())
			{
				continue;	// Behind the camera.
			}

			float ex = pp.X - DRIVEBY.aim_x;
			float ey = pp.Y - DRIVEBY.aim_y;
			float e  = sqrtf(ex * ex + ey * ey);

			if (pw[j] >= 0)
			{
				e -= snap * 0.25F;	// When it's close, the tyre wins over the car body.
			}

			if (e > snap)
			{
				continue;
			}

			DRIVEBY_Target *t = &cand[num++];

			t->thing  = found[i];
			t->wheel  = pw[j];
			t->x      = px[j];
			t->y      = py[j];
			t->z      = pz[j];
			t->score  = e;
			t->chance = DRIVEBY_hit_chance(p_car, p_found, pw[j], dist, TRUE);
		}
	}

	//
	// The nearest to the crosshair that we can see.
	//

	while (num > 0)
	{
		SLONG best = 0;

		for (i = 1; i < num; i++)
		{
			if (cand[i].score < cand[best].score)
			{
				best = i;
			}
		}

		if (there_is_a_los(
				SLONG(sx), SLONG(sy), SLONG(sz),
				SLONG(cand[best].x), SLONG(cand[best].y) + 8, SLONG(cand[best].z),
				LOS_FLAG_IGNORE_PRIMS))
		{
			DRIVEBY.aim_target     = cand[best];
			DRIVEBY.has_aim_target = TRUE;

			return;
		}

		cand[best] = cand[--num];
	}
}

//
// Bang.
//

static void DRIVEBY_fire(Thing *p_person, Thing *p_car, SLONG has_target, DRIVEBY_Target *target)
{
	ULONG now = GetTickCount();

	//
	// Ammo- the pistol's clip, then the spare clips.
	//

	if (p_person->Genus.Person->Ammo == 0)
	{
		if (p_person->Genus.Person->ammo_packs_pistol)
		{
			SLONG clip = p_person->Genus.Person->ammo_packs_pistol;

			if (clip > SPECIAL_AMMO_IN_A_PISTOL)
			{
				clip = SPECIAL_AMMO_IN_A_PISTOL;
			}

			p_person->Genus.Person->Ammo               = UBYTE(clip);
			p_person->Genus.Person->ammo_packs_pistol -= UBYTE(clip);

			DRIVEBY.next_shot = now + DRIVEBY_RELOAD_MS;

			DRIVEBY_message("Przeladowanie...", DRIVEBY_RELOAD_MS);
		}
		else
		{
			DRIVEBY.next_shot = now + 400;

			DRIVEBY_message("Brak amunicji!", 1500);
		}

		MFX_play_thing(THING_NUMBER(p_person), S_PISTOL_DRY, MFX_REPLACE, p_person);

		return;
	}

	p_person->Genus.Person->Ammo -= 1;

	DRIVEBY.next_shot = now + DRIVEBY_FIRE_MS;

	//
	// Noise and flash.
	//

	SLONG sx = p_car->WorldPos.X >> 8;
	SLONG sy = (p_car->WorldPos.Y >> 8) + 80;
	SLONG sz = p_car->WorldPos.Z >> 8;

	MFX_play_thing(THING_NUMBER(p_person), SOUND_Range(S_PISTOL_SHOT,S_PISTOL_SHOT_END), MFX_REPLACE, p_person);

	PCOM_oscillate_tympanum(PCOM_SOUND_GUNSHOT, p_person, sx, sy, sz);

	GAME_FLAGS |= GF_PLAYER_FIRED_GUN;

	{
		SLONG mx = sx;
		SLONG mz = sz;

		if (has_target)
		{
			//
			// Muzzle at the window on the target's side.
			//

			float dx = target->x - float(sx);
			float dz = target->z - float(sz);
			float d  = sqrtf(dx * dx + dz * dz) + 1.0F;

			mx += SLONG(dx * 70.0F / d);
			mz += SLONG(dz * 70.0F / d);
		}

		UBYTE dlight = NIGHT_dlight_create(mx, sy, mz, 100, 30, 25, 5);

		if (dlight)
		{
			NIGHT_dlight[dlight].flag |= NIGHT_DLIGHT_FLAG_REMOVE;
		}
	}

	if (!has_target)
	{
		return;		// Shooting at nothing in particular.
	}

	Thing *p_target = TO_THING(target->thing);

	SLONG tx = SLONG(target->x);
	SLONG ty = SLONG(target->y);
	SLONG tz = SLONG(target->z);

	SLONG hit = float(Random() & 0xffff) < target->chance * 65536.0F;

	if (!hit)
	{
		//
		// Miss- the bullet hits the ground or a wall near the target.
		//

		PYRO_hitspang(
			p_person,
			(tx + SLONG(Random() & 0x3f) - 0x20) << 8,
			(ty + SLONG(Random() & 0x1f) - 0x10) << 8,
			(tz + SLONG(Random() & 0x3f) - 0x20) << 8);

		if (p_target->Class == CLASS_PERSON)
		{
			PCOM_attack_happened_but_missed(p_target, p_person);
		}
		else
		if (p_target->Class == CLASS_VEHICLE)
		{
			p_target->Genus.Vehicle->Flags |= FLAG_VEH_SHOT_AT;
		}

		return;
	}

	PYRO_hitspang(p_person, tx << 8, ty << 8, tz << 8);

	if (p_target->Class == CLASS_VEHICLE)
	{
		p_target->Genus.Vehicle->Flags |= FLAG_VEH_SHOT_AT;

		if (target->wheel >= 0)
		{
			if (VEH_puncture_tyre(p_target, target->wheel))
			{
				DRIVEBY_message("Opona przestrzelona!", 1500);
			}

			VEH_reduce_health(p_target, p_person, DRIVEBY_TYRE_DAMAGE);
		}
		else
		{
			VEH_reduce_health(p_target, p_person, DRIVEBY_PISTOL_DAMAGE);

			VEH_shot_at_tyres(p_target, p_person, HIT_TYPE_GUN_SHOT_PISTOL);	// A lucky shot might still get a tyre
		}
	}
	else
	if (p_target->Class == CLASS_PERSON)
	{
		apply_hit_to_person(
			p_target,
			0,
			HIT_TYPE_GUN_SHOT_PISTOL,
			DRIVEBY_PISTOL_DAMAGE,
			p_person,
			NULL);
	}
}

void DRIVEBY_process(Thing *p_person)
{
	if (CNET_network_game)
	{
		//
		// Not in a LAN game (yet): aiming reads this PC's mouse and camera
		// straight away, the other PC wouldn't know.
		//

		return;
	}

	DRIVEBY_init();

	DRIVEBY.active = FALSE;

	SLONG can_aim = FALSE;
	Thing *p_car  = NULL;

	if (DRIVEBY.enabled && p_person && !(GAME_FLAGS & GF_PAUSED) &&
		(p_person->Genus.Person->Flags & FLAG_PERSON_DRIVING) && p_person->Genus.Person->InCar)
	{
		p_car = TO_THING(p_person->Genus.Person->InCar);

		if (p_car->Class == CLASS_VEHICLE)
		{
			can_aim = TRUE;
		}
	}

	if (!can_aim)
	{
		if (DRIVEBY.aiming)
		{
			DRIVEBY_aim_stop();
			DRIVEBY.aiming = FALSE;
		}

		DRIVEBY.locked     = 0;
		DRIVEBY.has_target = FALSE;

		return;
	}

	SLONG fire  = Keys[DRIVEBY.key_fire] || LeftButton;
	SLONG next  = Keys[DRIVEBY.key_next];
	SLONG tyres = Keys[DRIVEBY.key_tyres];
	SLONG aim   = RightButton;

	if (!(p_person->Flags & FLAGS_HAS_GUN))
	{
		if ((fire || aim) && GetTickCount() > DRIVEBY.msg_until)
		{
			DRIVEBY_message("Nie masz pistoletu", 1500);
		}

		if (DRIVEBY.aiming)
		{
			DRIVEBY_aim_stop();
			DRIVEBY.aiming = FALSE;
		}

		DRIVEBY.has_target = FALSE;

		return;
	}

	DRIVEBY.active = TRUE;

	//
	// Right mouse button: aim with the mouse.
	//

	if (aim && !DRIVEBY.aiming)
	{
		DRIVEBY.aiming = TRUE;
		DRIVEBY_aim_start();
	}
	else
	if (!aim && DRIVEBY.aiming)
	{
		DRIVEBY.aiming = FALSE;
		DRIVEBY_aim_stop();
	}

	if (!DRIVEBY.aiming && DRIVEBY.cam_blend > 0.0F)
	{
		DRIVEBY_place_camera(p_car);	// Keep it with the car while it goes back
	}

	if (DRIVEBY.aiming)
	{
		DRIVEBY_aim_move();
		DRIVEBY_place_camera(p_car);

		if (fire && GetTickCount() >= DRIVEBY.next_shot)
		{
			SLONG ok = DRIVEBY.has_aim_target;

			if (ok)
			{
				Thing *p_target = TO_THING(DRIVEBY.aim_target.thing);

				if (p_target->Class != CLASS_VEHICLE && p_target->Class != CLASS_PERSON)
				{
					ok = FALSE;		// It's gone since we drew it.
				}
			}

			DRIVEBY_fire(p_person, p_car, ok, &DRIVEBY.aim_target);
		}

		return;
	}

	//
	// No mouse aiming: the automatic sight. Tyres / people-and-cars.
	//

	if (tyres && !DRIVEBY.was_tyres)
	{
		DRIVEBY.tyres  ^= 1;
		DRIVEBY.locked  = 0;

		DRIVEBY_message(DRIVEBY.tyres ? "Celujesz w opony" : "Celujesz w ludzi i auta", 1500);
	}

	SLONG want_next = next && !DRIVEBY.was_next;

	DRIVEBY.was_tyres = tyres;
	DRIVEBY.was_next  = next;

	DRIVEBY_find_targets(p_person, p_car, want_next);

	if (fire && GetTickCount() >= DRIVEBY.next_shot)
	{
		DRIVEBY_fire(p_person, p_car, DRIVEBY.has_target, &DRIVEBY.target);
	}
}

void DRIVEBY_draw_overlay(void)
{
	if (!DRIVEBY.enabled)
	{
		return;
	}

	Thing *darci = NET_PERSON(0);

	if (darci == NULL || !(darci->Genus.Person->Flags & FLAG_PERSON_DRIVING) || !darci->Genus.Person->InCar)
	{
		return;
	}

	Thing *p_car = TO_THING(darci->Genus.Person->InCar);

	CBYTE text[128];

	SLONG x = 16;
	SLONG y = DisplayHeight - 72;

	if (DRIVEBY.active)
	{
		SLONG           show   = FALSE;
		DRIVEBY_Target *target = NULL;

		if (DRIVEBY.aiming)
		{
			//
			// What's under the crosshair (in the middle of the screen) now?
			//

			DRIVEBY.aim_x = float(DisplayWidth  / 2);
			DRIVEBY.aim_y = float(DisplayHeight / 2);

			DRIVEBY_aim_pick(darci, p_car);

			show   = DRIVEBY.has_aim_target;
			target = &DRIVEBY.aim_target;

			//
			// The crosshair itself.
			//

			SLONG cx = SLONG(DRIVEBY.aim_x);
			SLONG cy = SLONG(DRIVEBY.aim_y);

			ULONG colour = (show) ? 0xff4040 : 0xffffff;

			static CBYTE plus[] = "+";

			FONT2D_DrawString(plus, cx - 4 + 1, cy - 6 + 1, 0x000000, 256, POLY_PAGE_FONT2D, 0);
			FONT2D_DrawString(plus, cx - 4,     cy - 6,     colour,   256, POLY_PAGE_FONT2D, 0);

			if (show)
			{
				Thing *p_target = TO_THING(target->thing);

				sprintf(text, "%s  %d%%",
					(target->wheel >= 0) ? "OPONA" : (p_target->Class == CLASS_VEHICLE) ? "AUTO" : "CEL",
					SLONG(target->chance * 100.0F + 0.5F));

				FONT2D_DrawString(text, cx + 12 + 1, cy + 8 + 1, 0x000000, 256, POLY_PAGE_FONT2D, 0);
				FONT2D_DrawString(text, cx + 12,     cy + 8,     0xffff40, 256, POLY_PAGE_FONT2D, 0);
			}
		}
		else
		{
			show   = DRIVEBY.has_target;
			target = &DRIVEBY.target;
		}

		if (show)
		{
			SLONG scale;

			if (target->wheel >= 0)
			{
				scale = 70;
			}
			else
			if (TO_THING(target->thing)->Class == CLASS_VEHICLE)
			{
				scale = 380;
			}
			else
			{
				scale = 200;
			}

			PANEL_draw_gun_sight(
				SLONG(target->x),
				SLONG(target->y) + 36,	// (it draws 36 below the point it's given)
				SLONG(target->z),
				SLONG(target->chance * 255.0F),
				scale);
		}

		//sprintf(text, "Strzelanie z auta - %s   Amunicja: %d + %d",
		//	(DRIVEBY.aiming) ? "celowanie myszka" : (DRIVEBY.tyres) ? "auto-cel: OPONY" : "auto-cel: ludzie i auta",
		//	darci->Genus.Person->Ammo,
		//	darci->Genus.Person->ammo_packs_pistol);

		//FONT2D_DrawString(text, x + 1, y + 1, 0x000000, 512, POLY_PAGE_FONT2D, 0);
		//FONT2D_DrawString(text, x,     y,     0xffffff, 512, POLY_PAGE_FONT2D, 0);

		//sprintf(text, "PPM (trzymaj) celuj z okna, mysz obraca, LPM strzal   |   Ctrl strzal w auto-cel, Shift nastepny, CapsLock opony");

		//FONT2D_DrawString(text, x + 1, y + 17, 0x000000, 640, POLY_PAGE_FONT2D, 0);
		//FONT2D_DrawString(text, x,     y + 16, 0xa0a0a0, 640, POLY_PAGE_FONT2D, 0);
	}

	if (GetTickCount() < DRIVEBY.msg_until)
	{
		FONT2D_DrawString(DRIVEBY.msg, x + 1, y - 19, 0x000000, 512, POLY_PAGE_FONT2D, 0);
		FONT2D_DrawString(DRIVEBY.msg, x,     y - 20, 0xffff40, 512, POLY_PAGE_FONT2D, 0);
	}
}
