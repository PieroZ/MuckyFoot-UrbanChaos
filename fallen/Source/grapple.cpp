//
// Grappling hook: aim at the edge of a roof, throw, climb up the rope.
// See grapple.h
//

#include "game.h"
#include "statedef.h"
#include "animate.h"
#include "interact.h"
#include "collide.h"
#include "pap.h"
#include "mav.h"
#include "mfx.h"
#include "sound_id.h"
#include "hook.h"
#include "ddlib.h"
#include "font2d.h"
#include "..\DDEngine\Headers\panel.h"
#include "..\DDEngine\Headers\poly.h"
#include "..\DDEngine\Headers\console.h"
#include "config_extras.h"
#include "grapple.h"
#include <math.h>
#include "interfac.h"

extern void  set_generic_person_state_function(Thing *t_thing, UBYTE state);
extern SLONG person_normal_animate(Thing *p_person);
extern SLONG person_has_gun_out(Thing *p_person);

extern volatile HWND  hDDLibWindow;
extern volatile SLONG MouseAccDX;	// GMouse.cpp
extern volatile SLONG MouseAccDY;
extern void RecenterMouse(void);

//
// Our part of STATE_GRAPPLING (fn_person_grapple() leaves it to us).
//

#define SUB_STATE_GRAPPLE_ROPE	252

#define GR_NONE		0
#define GR_AIM		1
#define GR_THROW	2		// Throwing animation, then the hook flies
#define GR_SWING	3		// Pulled across to the wall
#define GR_CLIMB	4		// On the rope
#define GR_TOP		5		// Pulling herself up onto the roof

#define GR_HANDS_UP		250		// How far above her feet her hands are when hanging
#define GR_WALL_GAP		0x30	// How far out from the wall she hangs
#define GR_MIN_HEIGHT	0x80	// The roof must be at least this far above her...
#define GR_MAX_HEIGHT	0xc00	// ...and no more than this (the length of the rope)
#define GR_SLIDE_SPEED	6.0F	// Sliding down, units per frame
#define GR_SWING_MS		650
#define GR_PI			3.14159265F

typedef struct
{
	SLONG init;
	SLONG cheat;			// Got the hook from the cheat
	UBYTE key_aim;
	SLONG anim_climb;		// PROTOTYPE_DARCI1.all indices (0 = D'arci's own)
	SLONG anim_hang;
	SLONG anim_top;
	SLONG anim_throw;
	float climb_step;
	float range;

	SLONG state;
	SLONG cursor_hidden;

	//
	// Aiming.
	//

	float pitch;
	SLONG valid;
	float aim_x;			// Where the aim ray ends (for the sight when it's no good)
	float aim_y;
	float aim_z;

	//
	// Where the hook goes.
	//

	float anchor_x;
	float anchor_y;
	float anchor_z;
	float in_x;				// Horizontal direction into the building
	float in_z;

	//
	// Where she is.
	//

	float start_x;
	float start_y;
	float start_z;
	float wall_x;			// Feet when hanging against the wall
	float wall_z;
	float wall_y;			// ...at the bottom of the rope
	float climb_y;			// Feet height on the rope
	float top_y;			// Feet height at the top of the rope

	ULONG t0;				// GetTickCount() at the start of the current bit
	SLONG flight_ms;
	SLONG launched;

	float hook_x;
	float hook_y;
	float hook_z;
	float hand_x;
	float hand_y;
	float hand_z;

	SLONG anim_frames;		// Frames in the animation playing
	float last_progress;

	CBYTE msg[64];
	ULONG msg_until;

} GRAPPLE_State;

static GRAPPLE_State GR;

static void GR_message(const CBYTE *msg, ULONG ms)
{
	strncpy(GR.msg, msg, sizeof(GR.msg) - 1);
	GR.msg[sizeof(GR.msg) - 1] = 0;
	GR.msg_until = GetTickCount() + ms;
}

static void GR_init(void)
{
	if (GR.init)
	{
		return;
	}

	GR.init       = TRUE;
	GR.key_aim    = UBYTE(ENV_get_value_number("grapple_aim",        34,  "Keyboard"));	// G
	GR.anim_climb =       ENV_get_value_number("grapple_anim_climb", 0,   "Extra");
	GR.anim_hang  =       ENV_get_value_number("grapple_anim_hang",  0,   "Extra");
	GR.anim_top   =       ENV_get_value_number("grapple_anim_top",   0,   "Extra");
	GR.anim_throw =       ENV_get_value_number("grapple_anim_throw", 0,   "Extra");
	GR.climb_step = float(ENV_get_value_number("grapple_climb_step", 128, "Extra"));
	GR.range      = float(ENV_get_value_number("grapple_range",      20,  "Extra")) * 256.0F;

	if (GR.climb_step < 16.0F) {GR.climb_step = 16.0F;}
}

void GRAPPLE_reset(void)
{
	SLONG cheat = GR.cheat;

	if (GR.cursor_hidden)
	{
		ShowCursor(TRUE);
	}

	memset(&GR, 0, sizeof(GR));

	GR.cheat = cheat;	// The cheat lasts until you turn it off
}

SLONG GRAPPLE_active(void)
{
	return GR.state != GR_NONE;
}

static SLONG GR_has_hook(Thing *p_person)
{
	return GR.cheat || (p_person->Genus.Person->Flags & FLAG_PERSON_GRAPPLING);
}

void GRAPPLE_toggle_cheat(void)
{
	GR.cheat ^= 1;

	CONSOLE_text(GR.cheat ? "Masz hak z lina - trzymaj G albo PPM zeby celowac" : "Hak zabrany", 4000);
}

// ---------------------------------------------------------------------------
// Animations
// ---------------------------------------------------------------------------

static GameKeyFrame *GR_anim(Thing *p_person, SLONG pzi_index, SLONG darci_anim)
{
	if (pzi_index > 0)
	{
		GameKeyFrameChunk *gc = &game_chunk[ANIM_TYPE_PZI];

		if (gc->AnimList && pzi_index < gc->MaxAnimFrames && gc->AnimList[pzi_index])
		{
			return gc->AnimList[pzi_index];
		}
	}

	return global_anim_array[p_person->Genus.Person->AnimType][darci_anim];
}

static void GR_play(Thing *p_person, SLONG pzi_index, SLONG darci_anim)
{
	GameKeyFrame *first = GR_anim(p_person, pzi_index, darci_anim);

	if (first == NULL)
	{
		return;
	}

	DrawTween *dt = p_person->Draw.Tweened;

	dt->AnimTween    = 0;
	dt->QueuedFrame  = 0;
	dt->CurrentFrame = first;
	dt->NextFrame    = (first->NextFrame) ? first->NextFrame : first;
	dt->Locked       = 0;
	dt->FrameIndex   = 0;
	dt->CurrentAnim  = darci_anim;	// (only used as a name for it)

	p_person->Genus.Person->Flags2 &= ~FLAG2_SYNC_SOUNDFX;

	//
	// How many frames is it?
	//

	GR.anim_frames = 1;

	GameKeyFrame *f = first;

	while (!(f->Flags & ANIM_FLAG_LAST_FRAME) && f->NextFrame && f->NextFrame != first && GR.anim_frames < 64)
	{
		f = f->NextFrame;
		GR.anim_frames += 1;
	}

	GR.last_progress = 0.0F;
}

//
// How far through the animation (0..1).
//

static float GR_progress(Thing *p_person)
{
	DrawTween *dt = p_person->Draw.Tweened;

	float p = (float(dt->FrameIndex) + float(dt->AnimTween) / 256.0F) / float(MAX(GR.anim_frames, 1));

	if (p > 1.0F) {p = 1.0F;}

	return p;
}

// ---------------------------------------------------------------------------
// Aiming
// ---------------------------------------------------------------------------

static void GR_forward(Thing *p_person, float *fx, float *fz)
{
	SLONG angle = p_person->Draw.Tweened->Angle & 2047;

	*fx = -float(SIN(angle)) * (1.0F / 65536.0F);	// People move along (-sin, -cos) of their angle
	*fz = -float(COS(angle)) * (1.0F / 65536.0F);
}

static SLONG GR_angle_towards(float dx, float dz)
{
	SLONG a = SLONG(atan2f(-dx, -dz) * (2048.0F / (2.0F * GR_PI)));

	return a & 2047;
}

//
// Follows the aim ray until it reaches a building. The hook catches if the
// ray gets there near the top edge of the roof.
//

static void GR_find_target(Thing *p_person)
{
	float fx;
	float fz;

	GR_forward(p_person, &fx, &fz);

	float cp = cosf(GR.pitch);
	float sp = sinf(GR.pitch);

	float ex = float(p_person->WorldPos.X >> 8);
	float ey = float(p_person->WorldPos.Y >> 8) + 0x60;	// From her shoulders
	float ez = float(p_person->WorldPos.Z >> 8);

	SLONG base = p_person->WorldPos.Y >> 8;

	GR.valid = FALSE;

	SLONG last_mx = SLONG(ex) >> 8;
	SLONG last_mz = SLONG(ez) >> 8;

	float d;

	for (d = 16.0F; d < GR.range; d += 16.0F)
	{
		float x = ex + fx * cp * d;
		float y = ey + sp * d;
		float z = ez + fz * cp * d;

		GR.aim_x = x;
		GR.aim_y = y;
		GR.aim_z = z;

		SLONG mx = SLONG(x) >> 8;
		SLONG mz = SLONG(z) >> 8;

		if (!WITHIN(mx, 0, PAP_SIZE_HI - 1) || !WITHIN(mz, 0, PAP_SIZE_HI - 1))
		{
			return;
		}

		SLONG h = MAVHEIGHT(mx, mz) << 6;	// Top of whatever is in this mapsquare (roofs too)

		if (y > float(h))
		{
			last_mx = mx;
			last_mz = mz;

			continue;	// Still in the air.
		}

		//
		// The ray has reached something. A building tall enough, and did we
		// aim at its top edge?
		//

		if (h - base < GR_MIN_HEIGHT || h - base > GR_MAX_HEIGHT)
		{
			return;		// Too low (or too high for the rope).
		}

		if (y < float(h) - 0x80)
		{
			return;		// Hit the wall well below the roof- aim higher.
		}

		//
		// The edge: step back along the ray to where we left the last mapsquare.
		//

		float ax = x;
		float az = z;

		SLONG i;

		for (i = 0; i < 8; i++)
		{
			float bx = ax - fx * 2.0F;
			float bz = az - fz * 2.0F;

			if ((SLONG(bx) >> 8) == last_mx && (SLONG(bz) >> 8) == last_mz)
			{
				break;
			}

			ax = bx;
			az = bz;
		}

		//
		// Can she see it?
		//

		if (!there_is_a_los(
				SLONG(ex), SLONG(ey), SLONG(ez),
				SLONG(ax - fx * 12.0F), h + 16, SLONG(az - fz * 12.0F),
				LOS_FLAG_IGNORE_PRIMS))
		{
			return;
		}

		GR.anchor_x = ax;
		GR.anchor_y = float(h);
		GR.anchor_z = az;
		GR.in_x     = fx;
		GR.in_z     = fz;
		GR.valid    = TRUE;

		return;
	}
}

static void GR_cursor(SLONG hide)
{
	if (hide && !GR.cursor_hidden)
	{
		ShowCursor(FALSE);
		GR.cursor_hidden = TRUE;
	}
	else
	if (!hide && GR.cursor_hidden)
	{
		ShowCursor(TRUE);
		GR.cursor_hidden = FALSE;
	}
}

// ---------------------------------------------------------------------------
// On the rope
// ---------------------------------------------------------------------------

static void GR_place(Thing *p_person, float x, float y, float z)
{
	GameCoord pos;

	pos.X = SLONG(x * 256.0F);
	pos.Y = SLONG(y * 256.0F);
	pos.Z = SLONG(z * 256.0F);

	move_thing_on_map(p_person, &pos);
}

static void GR_hand(Thing *p_person)
{
	SLONG px;
	SLONG py;
	SLONG pz;

	calc_sub_objects_position(
		p_person,
		p_person->Draw.Tweened->AnimTween,
		SUB_OBJECT_RIGHT_HAND,
	   &px,
	   &py,
	   &pz);

	GR.hand_x = float(px + (p_person->WorldPos.X >> 8));
	GR.hand_y = float(py + (p_person->WorldPos.Y >> 8));
	GR.hand_z = float(pz + (p_person->WorldPos.Z >> 8));
}

static void GR_enter_rope_state(Thing *p_person)
{
	set_generic_person_state_function(p_person, STATE_GRAPPLING);

	p_person->SubState              = SUB_STATE_GRAPPLE_ROPE;
	p_person->Velocity              = 0;
	p_person->DeltaVelocity         = 0;
	p_person->DY                    = 0;
	p_person->OnFace                = 0;
	p_person->Genus.Person->Action  = ACTION_GRAPPLE;
}

static void GR_let_go(Thing *p_person)
{
	GR.state = GR_NONE;

	set_person_drop_down(p_person, 0);
}

static void GR_start_throw(Thing *p_person)
{
	//
	// The level's grappling hook: stop the old spinning rope, we've got it now.
	//

	if (p_person->Genus.Person->Flags & FLAG_PERSON_GRAPPLING)
	{
		p_person->Genus.Person->Flags &= ~FLAG_PERSON_GRAPPLING;

		HOOK_hide();

		GR.cheat = TRUE;	// (it stays with her from now on)
	}

	GR_enter_rope_state(p_person);

	p_person->Draw.Tweened->Angle   = GR_angle_towards(GR.anchor_x - float(p_person->WorldPos.X >> 8), GR.anchor_z - float(p_person->WorldPos.Z >> 8));
	p_person->Draw.Tweened->AngleTo = p_person->Draw.Tweened->Angle;

	GR_play(p_person, GR.anim_throw, ANIM_GRAPPLING_HOOK_RELEASE);

	GR.start_x  = float(p_person->WorldPos.X >> 8);
	GR.start_y  = float(p_person->WorldPos.Y >> 8);
	GR.start_z  = float(p_person->WorldPos.Z >> 8);
	GR.launched = FALSE;
	GR.t0       = GetTickCount();
	GR.state    = GR_THROW;

	//
	// Where she'll hang: against the wall below the edge.
	//

	GR.wall_x = GR.anchor_x - GR.in_x * GR_WALL_GAP;
	GR.wall_z = GR.anchor_z - GR.in_z * GR_WALL_GAP;

	SLONG wmx = SLONG(GR.wall_x) >> 8;
	SLONG wmz = SLONG(GR.wall_z) >> 8;

	float ground = float(PAP_calc_map_height_at(SLONG(GR.wall_x), SLONG(GR.wall_z)));

	if (WITHIN(wmx, 0, PAP_SIZE_HI - 1) && WITHIN(wmz, 0, PAP_SIZE_HI - 1))
	{
		float floor = float(MAVHEIGHT(wmx, wmz) << 6);

		if (floor > ground && floor < GR.anchor_y - GR_MIN_HEIGHT)
		{
			ground = floor;		// Standing on something next to the wall
		}
	}

	GR.wall_y = MAX(ground, GR.start_y) + 8.0F;
	GR.top_y  = GR.anchor_y - GR_HANDS_UP;

	if (GR.wall_y > GR.top_y)
	{
		GR.wall_y = GR.top_y;
	}

	MFX_play_thing(THING_NUMBER(p_person), S_METAL_LAND_LITE, MFX_REPLACE, p_person);
}

//
// The hook flying from her hand to the roof.
//

static void GR_process_throw(Thing *p_person)
{
	ULONG now = GetTickCount();

	if (person_normal_animate(p_person))
	{
		//
		// Hold the last frame of the throw.
		//
	}

	GR_hand(p_person);

	if (!GR.launched)
	{
		GR.hook_x = GR.hand_x;
		GR.hook_y = GR.hand_y;
		GR.hook_z = GR.hand_z;

		if (now - GR.t0 > 300 || GR_progress(p_person) > 0.6F)
		{
			GR.launched = TRUE;
			GR.t0       = now;

			float dx = GR.anchor_x - GR.hand_x;
			float dy = GR.anchor_y - GR.hand_y;
			float dz = GR.anchor_z - GR.hand_z;

			GR.flight_ms = 250 + SLONG(sqrtf(dx * dx + dy * dy + dz * dz) * 0.35F);

			GR.start_x = GR.hand_x;		// (re-used: where the hook started from)
			GR.start_y = GR.hand_y;
			GR.start_z = GR.hand_z;
		}

		return;
	}

	float t = float(now - GR.t0) / float(GR.flight_ms);

	if (t >= 1.0F)
	{
		//
		// Caught!
		//

		GR.hook_x = GR.anchor_x;
		GR.hook_y = GR.anchor_y;
		GR.hook_z = GR.anchor_z;

		MFX_play_xyz(THING_NUMBER(p_person), S_METAL_LAND_HEAVY, 0, SLONG(GR.anchor_x) << 8, SLONG(GR.anchor_y) << 8, SLONG(GR.anchor_z) << 8);

		GR.start_x = float(p_person->WorldPos.X >> 8);
		GR.start_y = float(p_person->WorldPos.Y >> 8);
		GR.start_z = float(p_person->WorldPos.Z >> 8);
		GR.t0      = now;
		GR.state   = GR_SWING;

		GR_play(p_person, GR.anim_hang, ANIM_DANGLE);

		return;
	}

	//
	// An arc up to the edge.
	//

	GR.hook_x = GR.start_x + (GR.anchor_x - GR.start_x) * t;
	GR.hook_y = GR.start_y + (GR.anchor_y - GR.start_y) * t + sinf(t * GR_PI) * 96.0F;
	GR.hook_z = GR.start_z + (GR.anchor_z - GR.start_z) * t;
}

//
// Pulled across to the wall.
//

static void GR_process_swing(Thing *p_person)
{
	float t = float(GetTickCount() - GR.t0) / float(GR_SWING_MS);

	if (t > 1.0F) {t = 1.0F;}

	float e = t * t * (3.0F - 2.0F * t);

	float x = GR.start_x + (GR.wall_x - GR.start_x) * e;
	float z = GR.start_z + (GR.wall_z - GR.start_z) * e;
	float y = GR.start_y + (GR.wall_y - GR.start_y) * e + sinf(t * GR_PI) * 24.0F;

	GR_place(p_person, x, y, z);

	person_normal_animate(p_person);

	if (t >= 1.0F)
	{
		GR.climb_y = GR.wall_y;
		GR.state   = GR_CLIMB;

		GR_message("Strzalka w gore - wspinaj sie, w dol - zjedz, skok - pusc", 4000);
	}
}

static void GR_process_climb(Thing *p_person, ULONG input)
{
	if (input & INPUT_MASK_JUMP)
	{
		GR_let_go(p_person);

		return;
	}

	DrawTween *dt = p_person->Draw.Tweened;

	if (input & INPUT_MASK_FORWARDS)
	{
		//
		// Climbing: the animation decides how fast she goes up- one loop
		// of it is grapple_climb_step units.
		//

		if (dt->CurrentAnim != ANIM_UP_LADDER)
		{
			GR_play(p_person, GR.anim_climb, ANIM_UP_LADDER);
		}

		float before = GR_progress(p_person);

		if (person_normal_animate(p_person))
		{
			GR_play(p_person, GR.anim_climb, ANIM_UP_LADDER);	// Loop it

			before -= 1.0F;
		}

		float after = GR_progress(p_person);

		GR.climb_y += (after - before) * GR.climb_step;

	}
	else
	if (input & INPUT_MASK_BACKWARDS)
	{
		//
		// Sliding down.
		//

		if (dt->CurrentAnim != ANIM_DANGLE)
		{
			GR_play(p_person, GR.anim_hang, ANIM_DANGLE);
		}

		GR.climb_y -= GR_SLIDE_SPEED * float(TICK_RATIO) / 256.0F;

		if (GR.climb_y <= GR.wall_y)
		{
			//
			// Back on the ground.
			//

			GR.climb_y = GR.wall_y;

			GR_place(p_person, GR.wall_x, GR.wall_y - 8.0F, GR.wall_z);

			GR.state = GR_NONE;

			set_person_idle(p_person);

			return;
		}
	}
	else
	{
		//
		// Just hanging there.
		//

		if (dt->CurrentAnim != ANIM_UP_LADDER && dt->CurrentAnim != ANIM_DANGLE)
		{
			GR_play(p_person, GR.anim_hang, ANIM_DANGLE);
		}
	}

	if (GR.climb_y >= GR.top_y)
	{
		//
		// At the top: pull up onto the roof.
		//

		GR.climb_y = GR.top_y;
		GR.state   = GR_TOP;

		GR_play(p_person, GR.anim_top, ANIM_PULL_UP_NEW);
	}

	GR_place(p_person, GR.wall_x, GR.climb_y, GR.wall_z);

	dt->Angle   = GR_angle_towards(GR.in_x, GR.in_z);
	dt->AngleTo = dt->Angle;
}

static void GR_process_top(Thing *p_person)
{
	SLONG end = person_normal_animate(p_person);

	float t = GR_progress(p_person);

	if (end) {t = 1.0F;}

	//
	// From hanging below the edge to standing on the roof.
	//

	float roof_x = GR.anchor_x + GR.in_x * 0x40;
	float roof_z = GR.anchor_z + GR.in_z * 0x40;

	float x = GR.wall_x  + (roof_x       - GR.wall_x)  * t;
	float y = GR.top_y   + (GR.anchor_y  - GR.top_y)   * MIN(1.0F, t * 1.6F);
	float z = GR.wall_z  + (roof_z       - GR.wall_z)  * t;

	GR_place(p_person, x, y, z);

	if (end)
	{
		GR.state = GR_NONE;

		set_person_idle(p_person);
	}
}

// ---------------------------------------------------------------------------
// The main bit
// ---------------------------------------------------------------------------

SLONG GRAPPLE_process(Thing *p_person, ULONG input)
{
	GR_init();

	if (p_person == NULL || (GAME_FLAGS & GF_PAUSED))
	{
		return GR.state != GR_NONE;
	}

	//
	// Something else happened to her (got hit, fell...) while on the rope?
	//

	if (GR.state >= GR_THROW)
	{
		if (p_person->State != STATE_GRAPPLING || p_person->SubState != SUB_STATE_GRAPPLE_ROPE)
		{
			GR.state = GR_NONE;

			return FALSE;
		}
	}

	switch(GR.state)
	{
		case GR_NONE:
		{
			if (!GR_has_hook(p_person))
			{
				return FALSE;
			}

			if (p_person->Genus.Person->Flags & (FLAG_PERSON_DRIVING | FLAG_PERSON_BIKING | FLAG_PERSON_PASSENGER))
			{
				return FALSE;
			}

			SLONG key = Keys[GR.key_aim] || (RightButton && !person_has_gun_out(p_person));

			if (!key)
			{
				return FALSE;
			}

			SLONG ok =
				(p_person->State == STATE_IDLE) ||
				(p_person->State == STATE_MOVEING && (p_person->SubState == SUB_STATE_WALKING || p_person->SubState == SUB_STATE_RUNNING)) ||
				(p_person->State == STATE_GRAPPLING && p_person->SubState == SUB_STATE_GRAPPLING_WINDUP);

			if (!ok)
			{
				return FALSE;
			}

			if (p_person->State != STATE_IDLE)
			{
				set_person_idle(p_person);
			}

			GR.state = GR_AIM;
			GR.pitch = 0.55F;	// About 30 degrees up
			GR.valid = FALSE;

			MouseAccDX = 0;
			MouseAccDY = 0;

			GR_cursor(TRUE);

			return TRUE;
		}

		case GR_AIM:
		{
			SLONG key = Keys[GR.key_aim] || RightButton;

			//
			// Turn and look up / down.
			//

			SLONG mdx = MouseAccDX;
			SLONG mdy = MouseAccDY;

			MouseAccDX = 0;
			MouseAccDY = 0;

			if (GetForegroundWindow() == hDDLibWindow)
			{
				RecenterMouse();
			}

			SLONG turn = -mdx * 2;	// (the camera follows, see GRAPPLE_camera())

			if (input & INPUT_MASK_LEFT)  {turn += 20;}
			if (input & INPUT_MASK_RIGHT) {turn -= 20;}

			p_person->Draw.Tweened->Angle   = (p_person->Draw.Tweened->Angle + turn) & 2047;
			p_person->Draw.Tweened->AngleTo = p_person->Draw.Tweened->Angle;

			GR.pitch -= float(mdy) * 0.004F;

			if (input & INPUT_MASK_FORWARDS)  {GR.pitch += 0.02F;}
			if (input & INPUT_MASK_BACKWARDS) {GR.pitch -= 0.02F;}

			if (GR.pitch < 0.0F)  {GR.pitch = 0.0F;}
			if (GR.pitch > 1.35F) {GR.pitch = 1.35F;}

			GR_find_target(p_person);

			if (!key)
			{
				//
				// Let go of the button: throw (if it'll catch on something).
				//

				GR_cursor(FALSE);

				if (GR.valid)
				{
					GR_start_throw(p_person);
				}
				else
				{
					GR.state = GR_NONE;

					GR_message("Tu hak sie nie zaczepi - celuj w krawedz dachu", 2500);
				}
			}

			return TRUE;
		}

		case GR_THROW:	GR_process_throw(p_person);        return TRUE;
		case GR_SWING:	GR_process_swing(p_person);        GR_hand(p_person); return TRUE;
		case GR_CLIMB:	GR_process_climb(p_person, input); GR_hand(p_person); return TRUE;
		case GR_TOP:	GR_process_top(p_person);          GR_hand(p_person); return TRUE;
	}

	return FALSE;
}

// ---------------------------------------------------------------------------
// The aiming camera: over D'arci's right shoulder, looking where she aims
// (fc.cpp asks for it every frame).
// ---------------------------------------------------------------------------

#define GR_CAM_BLEND_MS		200.0F
#define GR_CAM_BACK			150.0F	// Behind her...
#define GR_CAM_SIDE			40.0F	// ...over her right shoulder...
#define GR_CAM_UP			150.0F	// ...at about head height

static float GR_cam_blend;
static ULONG GR_cam_tick;

SLONG GRAPPLE_camera(SLONG *x, SLONG *y, SLONG *z, SLONG *look_x, SLONG *look_y, SLONG *look_z)
{
	ULONG now = GetTickCount();
	float dt  = float(now - GR_cam_tick);

	GR_cam_tick = now;

	if (dt > 100.0F) {dt = 100.0F;}

	SLONG want = (GR.state == GR_AIM || GR.state == GR_THROW);

	Thing *darci = NET_PERSON(0);

	if (darci == NULL)
	{
		want = FALSE;
	}

	if (want)
	{
		GR_cam_blend += dt / GR_CAM_BLEND_MS;

		if (GR_cam_blend > 1.0F) {GR_cam_blend = 1.0F;}
	}
	else
	{
		GR_cam_blend -= dt / GR_CAM_BLEND_MS;

		if (GR_cam_blend < 0.0F) {GR_cam_blend = 0.0F;}
	}

	if (GR_cam_blend <= 0.0F || darci == NULL)
	{
		return 0;
	}

	//
	// Which way she is aiming.
	//

	float fx;
	float fz;

	GR_forward(darci, &fx, &fz);

	float rx = -fz;		// Her right (forward is (-sin, -cos), right is (cos, -sin))
	float rz =  fx;

	float cp = cosf(GR.pitch);
	float sp = sinf(GR.pitch);

	float px = float(darci->WorldPos.X >> 8);
	float py = float(darci->WorldPos.Y >> 8);
	float pz = float(darci->WorldPos.Z >> 8);

	//
	// Behind and above her shoulder. Looking up, the camera drops a bit
	// lower behind her so she doesn't fill the screen.
	//

	float cx = px - fx * GR_CAM_BACK * cp + rx * GR_CAM_SIDE;
	float cy = py + GR_CAM_UP - sp * 60.0F;
	float cz = pz - fz * GR_CAM_BACK * cp + rz * GR_CAM_SIDE;

	//
	// Not inside a wall.
	//

	SLONG i;

	for (i = 0; i < 3; i++)
	{
		if (there_is_a_los(
				SLONG(px), SLONG(py) + 0x60, SLONG(pz),
				SLONG(cx), SLONG(cy),        SLONG(cz),
				LOS_FLAG_IGNORE_PRIMS))
		{
			break;
		}

		cx = px + (cx - px) * 0.5F;
		cy = py + 0x60 + (cy - py - 0x60) * 0.5F;
		cz = pz + (cz - pz) * 0.5F;
	}

	//
	// Looking along the aim, or at the hook while it flies.
	//

	float lx = cx + fx * cp * 1024.0F;
	float ly = cy + sp * 1024.0F;
	float lz = cz + fz * cp * 1024.0F;

	if (GR.state == GR_THROW && GR.launched)
	{
		lx = GR.hook_x;
		ly = GR.hook_y;
		lz = GR.hook_z;
	}

	*x      = SLONG(cx);
	*y      = SLONG(cy);
	*z      = SLONG(cz);
	*look_x = SLONG(lx);
	*look_y = SLONG(ly);
	*look_z = SLONG(lz);

	float b = GR_cam_blend;

	b = b * b * (3.0F - 2.0F * b);

	return SLONG(b * 256.0F);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

SLONG GRAPPLE_get_draw(
		SLONG *hook_x, SLONG *hook_y, SLONG *hook_z,
		SLONG *hook_yaw, SLONG *hook_pitch,
		SLONG *hand_x, SLONG *hand_y, SLONG *hand_z,
		SLONG *taut)
{
	if (GR.state < GR_THROW || GR.state == GR_TOP)
	{
		return FALSE;
	}

	*hook_x = SLONG(GR.hook_x);
	*hook_y = SLONG(GR.hook_y);
	*hook_z = SLONG(GR.hook_z);
	*hand_x = SLONG(GR.hand_x);
	*hand_y = SLONG(GR.hand_y);
	*hand_z = SLONG(GR.hand_z);

	*hook_yaw   = GR_angle_towards(GR.in_x, GR.in_z);
	*hook_pitch = 0;
	*taut       = (GR.state >= GR_SWING);

	return TRUE;
}

void GRAPPLE_draw_overlay(void)
{
	Thing *darci = NET_PERSON(0);

	if (darci == NULL)
	{
		return;
	}

	SLONG x = 16;
	SLONG y = DisplayHeight - 72;

	if (GR.state == GR_AIM)
	{
		//
		// Green sight on the roof edge it'll catch, red where you're aiming if it won't.
		//

		if (GR.valid)
		{
			PANEL_draw_gun_sight(SLONG(GR.anchor_x), SLONG(GR.anchor_y) + 36, SLONG(GR.anchor_z), 255, 160);
		}
		else
		{
			PANEL_draw_gun_sight(SLONG(GR.aim_x), SLONG(GR.aim_y) + 36, SLONG(GR.aim_z), 0, 120);
		}

		//
		// A small crosshair where she aims (the middle of the view).
		//

		{
			static CBYTE plus[] = "+";

			POLY_Point pp;

			POLY_transform(GR.aim_x, GR.aim_y, GR.aim_z, &pp);

			if (pp.IsValid())
			{
				FONT2D_DrawString(plus, SLONG(pp.X) - 3, SLONG(pp.Y) - 5, 0x000000, 64, POLY_PAGE_FONT2D, 0);
				FONT2D_DrawString(plus, SLONG(pp.X) - 4, SLONG(pp.Y) - 6, (GR.valid) ? 0x40ff40 : 0xffffff, 64, POLY_PAGE_FONT2D, 0);
			}
		}

		static CBYTE text1[] = "Hak: strzalki / mysz - kierunek, pusc G / PPM - rzut";

		FONT2D_DrawString(text1, x + 1, y + 1, 0x000000, 640, POLY_PAGE_FONT2D, 0);
		FONT2D_DrawString(text1, x,     y,     0xffffff, 640, POLY_PAGE_FONT2D, 0);
	}

	if (GetTickCount() < GR.msg_until)
	{
		FONT2D_DrawString(GR.msg, x + 1, y - 19, 0x000000, 640, POLY_PAGE_FONT2D, 0);
		FONT2D_DrawString(GR.msg, x,     y - 20, 0xffff40, 640, POLY_PAGE_FONT2D, 0);
	}
}
