//
// Security systems: sentry turrets, computer terminals and the EMP grenade.
// See security.h.
//
// ---------------------------------------------------------------------------
// The level file: levels\<level name>.sec   (e.g. levels\gang1.sec)
//
//   # comment
//   turret   <id> <x> <z> <yaw> [hostile|friendly] [exposed|hidden] [on|off] [range=8] [arc=0] [y=<height>]
//   terminal <id> <x> <z> <yaw> ["name"] [y=<height>]
//   link     <terminal id> <turret id | all>
//   camera   <terminal id> "name" <x> <z> <height> <yaw> [pitch]
//   file     <terminal id> "title"
//   ...any number of lines of text...
//   endfile
//
// x, z    - mapsquares (0..128, fractions allowed - the console command "where" prints them)
// yaw     - degrees, the way it faces (as "where" prints it)
// hidden  - the turret is down in the floor and comes up when it sees a target
// range   - mapsquares, arc - degrees each side of yaw it can turn (0 = all round)
// y       - absolute height in world units (default: the ground / roof under it)
// height  - camera height above the ground in world units (256 = one storey)
// A terminal with no "link" lines controls every turret in the level.
// The console command "secsave" writes the turrets and terminals that are in the
// level now (and the links / cameras / files read from the file) back to it.
// ---------------------------------------------------------------------------
//
// Manual control: a terminal's turret menu has "Manual control" - the player
// looks through the turret's sensor, turns it (left / right), raises / lowers
// the barrels (forwards / backwards), fires (punch or action, held = bursts)
// and locks on to the next person in view (jump; the turret then follows him).
// Kick / Esc goes back to the menu. While a turret is in manual control it does
// nothing on its own.
//
// People and turrets (SEC_person_ai, called by PCOM for the computer people):
// somebody a turret on the player's side (reprogrammed to "hostiles only", or
// in manual control) is shooting at, or that sees one aimed at him, shoots
// back at it if he has a gun - or runs away from it if he hasn't. Bystanders
// next to where turret bullets land run away from it too.
// ---------------------------------------------------------------------------
//

#include "game.h"
#include "statedef.h"
#include "animate.h"
#include "interact.h"
#include "collide.h"
#include "pap.h"
#include "mfx.h"
#include "sound.h"
#include "pcom.h"
#include "spark.h"
#include "psystem.h"
#include "pyro.h"
#include "interfac.h"
#include "elev.h"
#include "font2d.h"
#include "..\DDEngine\Headers\panel.h"
#include "..\DDEngine\Headers\poly.h"
#include "..\DDEngine\Headers\console.h"
#include <DDLib.h>
#include "polypage.h"
#include "config_extras.h"
#include "security.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern SLONG is_person_dead(Thing *p_person);
extern SLONG is_person_ko(Thing *p_person);
extern SLONG person_on_floor(Thing *p_person);
extern SLONG really_on_floor(Thing *p_person);
extern SLONG is_there_room_behind_person(Thing *p_person, SLONG hit_from_behind);
extern BOOL  PersonIsMIB(Thing *p_person);
extern void  DIRT_new_sparks(SLONG px, SLONG py, SLONG pz, UBYTE dir);
extern void  set_generic_person_state_function(Thing *t_thing, UBYTE state);
extern Thing *person_has_special(Thing *p_person, SLONG special_type);
extern SLONG set_face_thing(Thing *p_person_a, Thing *p_person_b);
extern SLONG PCOM_person_has_gun_in_hand(Thing *p_person);
extern void  PCOM_set_person_move_shoot(Thing *p_person);
extern void  PCOM_set_person_move_still(Thing *p_person);
extern void  PCOM_set_person_ai_flee_place(Thing *p_person, SLONG scary_x, SLONG scary_z);
extern void  PCOM_set_person_move_draw_firearm(Thing *p_person);	// pcom.cpp
extern SLONG PCOM_person_move_busy(Thing *p_person);				// pcom.cpp

// ---------------------------------------------------------------------------
// Turret state (kept in the Bat structure)
// ---------------------------------------------------------------------------
//
//   health   - hit points
//   state    - SEC_TS_*
//   substate - half the arc it can turn through, in 8ths of a 2048 circle (0 = all round)
//   flag     - SEC_F_*
//   target   - who it is shooting at
//   timer    - ms until it can fire again
//   want_x   - range in world units
//   want_y   - height of its base when it is up (world units)
//   want_z   - security id
//   glow     - ms of EMP / booting left (no power while > 0)
//   dx       - how far down in the floor it is (0 = up)
//   dy       - the yaw it was set up with (for the arc)
//   dz       - ms since it last had a target
//

#define SEC_TS_HIDDEN		1
#define SEC_TS_RISING		2
#define SEC_TS_UP			3
#define SEC_TS_LOWERING		4
#define SEC_TS_DEAD			5

#define SEC_F_CONCEALED		(1 << 0)	// Goes back into the floor when there is nothing to shoot
#define SEC_F_FRIENDLY		(1 << 1)	// Shoots the player's enemies instead of the player
#define SEC_F_POWEROFF		(1 << 2)	// Switched off at a terminal
#define SEC_F_HAS_HATCH		(1 << 3)	// Built into the floor- can be set to concealed
#define SEC_F_TERM_HACKED	(1 << 8)	// Terminal: somebody has logged in

#define SEC_HIDE_DEPTH		104			// World units under the floor when hidden
#define SEC_RISE_SPEED		90			// World units per second
#define SEC_TURN_SPEED		900			// 2048ths of a circle per second
#define SEC_AIM_TOLERANCE	56
#define SEC_RETRACT_MS		4000		// No target for this long => back into the floor
#define SEC_TURRET_HP		200
#define SEC_HEAD_HEIGHT		60			// Where the barrels are above the base
#define SEC_BARREL_LENGTH	78

#define SEC_ID_ALL			0xffff

// ---------------------------------------------------------------------------
// Level data: terminal names, links, cameras, files
// ---------------------------------------------------------------------------

#define SEC_MAX_TERMS		32
#define SEC_MAX_LINKS		128
#define SEC_MAX_CAMS		48
#define SEC_MAX_FILES		64
#define SEC_MAX_SPAWNS		96
#define SEC_TEXT_SIZE		(48 * 1024)

typedef struct
{
	UWORD id;
	CBYTE name[40];

} SEC_Term;

typedef struct
{
	UWORD term;
	UWORD turret;	// SEC_ID_ALL => all of them

} SEC_Link;

typedef struct
{
	UWORD term;
	CBYTE name[40];
	SLONG x;		// World units
	SLONG z;
	SLONG height;	// Above the ground
	SLONG yaw;		// 2048ths
	SLONG pitch;

} SEC_Cam;

typedef struct
{
	UWORD  term;
	CBYTE  title[64];
	CBYTE *text;

} SEC_File;

//
// A computer person who has to do something about a turret (SEC_person_ai).
//

#define SEC_MAX_THREATS		32
#define SEC_THREAT_MS		4000	// Forgotten this long after the turret last bothered him

typedef struct
{
	THING_INDEX person;
	THING_INDEX turret;
	SLONG       ms;			// Left before he forgets about it
	SLONG       cool;		// ms before he shoots at it again
	THING_INDEX old_target;	// His target before he turned on the turret
	UBYTE       attack;		// He may shoot at it (else he only runs if unarmed)
	UBYTE       shooting;	// He has the turret as his target

} SEC_Threat;

typedef struct
{
	UBYTE type;		// BAT_TYPE_TURRET / BAT_TYPE_TERMINAL
	UWORD id;
	SLONG x;		// World units
	SLONG y;		// -1 => on the ground
	SLONG z;
	SLONG yaw;
	UWORD flag;
	SLONG range;	// World units, 0 => default
	SLONG arc;		// 8ths

} SEC_Spawn;

// ---------------------------------------------------------------------------
// The terminal screen
// ---------------------------------------------------------------------------

#define SEC_UI_NONE			0
#define SEC_UI_BOOT			1		// Playing the 'use computer' animation
#define SEC_UI_MAIN			2
#define SEC_UI_FILES		3
#define SEC_UI_READ			4
#define SEC_UI_CAMS			5
#define SEC_UI_CCTV			6
#define SEC_UI_TURRETS		7
#define SEC_UI_TURRET		8
#define SEC_UI_MANUAL		9		// Aiming and firing a turret by hand

#define SEC_MAX_LIST		24
#define SEC_MAX_LINES		512

static struct
{
	SLONG       need_spawn;

	SEC_Term    term [SEC_MAX_TERMS];
	SLONG       term_upto;
	SEC_Link    link [SEC_MAX_LINKS];
	SLONG       link_upto;
	SEC_Cam     cam  [SEC_MAX_CAMS];
	SLONG       cam_upto;
	SEC_File    file [SEC_MAX_FILES];
	SLONG       file_upto;
	SEC_Spawn   spawn[SEC_MAX_SPAWNS];
	SLONG       spawn_upto;

	CBYTE       text[SEC_TEXT_SIZE];
	SLONG       text_upto;

	//
	// The screen.
	//

	SLONG       ui;
	THING_INDEX term_thing;
	THING_INDEX person;
	SLONG       sel;
	SLONG       scroll;
	SLONG       ui_ms;			// Real ms in this screen
	ULONG       prev_input;
	SLONG       escape;
	ULONG       last_tick;

	SLONG       list_upto;		// What the current list shows
	SLONG       list[SEC_MAX_LIST];

	SLONG       file_index;
	SLONG       cam_index;		// Into cam[], or -1 - (turret thing) for a turret's own camera
	THING_INDEX turret_thing;

	CBYTE      *line[SEC_MAX_LINES];
	UWORD       line_len[SEC_MAX_LINES];
	SLONG       line_upto;

	CBYTE       status[80];
	SLONG       status_ms;

	THING_INDEX near_term;		// For the "use computer" prompt

	//
	// Manual control of SEC.turret_thing (SEC_UI_MANUAL).
	//

	SLONG       man_pitch;		// 2048ths, + = barrels up
	THING_INDEX man_lock;		// Locked-on target (the turret follows him), or NULL
	SLONG       man_fire_ms;	// Real ms before it can fire again
	SLONG       man_shots;
	SLONG       man_hits;
	ULONG       raw_prev;		// The raw input last frame (manual control wants held keys)

	SEC_Threat  threat[SEC_MAX_THREATS];

} SEC;

static ULONG SEC_real_ms(void)
{
	return GetTickCount();
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static SLONG SEC_tick_ms(void)
{
	//
	// Game ticks run at 20 a second when TICK_RATIO == 1 << TICK_SHIFT, so this is
	// how many (game) milliseconds this tick is. It slows down in slow motion.
	//

	return (50 * TICK_RATIO) >> TICK_SHIFT;
}

static SLONG SEC_yaw_offset(void)
{
	return (ConfigExtras::getInstance().mTurretYawOffset * 2048 / 360) & 2047;
}

//
// The Angle a thing at (x1,z1) needs to face (x2,z2) - the same as people use.
//

static SLONG SEC_angle_to(SLONG x1, SLONG z1, SLONG x2, SLONG z2)
{
	return (Arctan(x2 - x1, -(z2 - z1)) + 1024) & 2047;
}

static SLONG SEC_angle_diff(SLONG a, SLONG b)	// b - a in -1024..1023
{
	return ((b - a + 1024) & 2047) - 1024;
}

static SLONG SEC_dist(Thing *a, Thing *b)
{
	SLONG dx = abs(a->WorldPos.X - b->WorldPos.X) >> 8;
	SLONG dz = abs(a->WorldPos.Z - b->WorldPos.Z) >> 8;

	return QDIST2(dx, dz);
}

static SLONG SEC_is_security(Thing *p_thing)
{
	return p_thing &&
		   p_thing->Class == CLASS_BAT &&
		  (p_thing->Genus.Bat->type == BAT_TYPE_TURRET ||
		   p_thing->Genus.Bat->type == BAT_TYPE_TERMINAL);
}

static Thing *SEC_find_by_id(SLONG type, SLONG id)
{
	SLONG list = thing_class_head[CLASS_BAT];

	while(list)
	{
		Thing *p_thing = TO_THING(list);

		list = p_thing->NextLink;

		if (p_thing->Genus.Bat->type == type && p_thing->Genus.Bat->want_z == id)
		{
			return p_thing;
		}
	}

	return NULL;
}

static SLONG SEC_next_free_id(SLONG type)
{
	SLONG id = 100;

	while(SEC_find_by_id(type, id))
	{
		id += 1;
	}

	return id;
}

static SEC_Term *SEC_term_info(SLONG id)
{
	SLONG i;

	for (i = 0; i < SEC.term_upto; i++)
	{
		if (SEC.term[i].id == id)
		{
			return &SEC.term[i];
		}
	}

	return NULL;
}

static void SEC_set_status(const CBYTE *msg)
{
	strncpy(SEC.status, msg, sizeof(SEC.status) - 1);
	SEC.status[sizeof(SEC.status) - 1] = 0;
	SEC.status_ms = 2500;
}

static CBYTE *SEC_text_store(CBYTE *text)
{
	SLONG len = strlen(text) + 1;

	if (SEC.text_upto + len > SEC_TEXT_SIZE)
	{
		return "(out of room for text)";
	}

	CBYTE *ans = &SEC.text[SEC.text_upto];

	memcpy(ans, text, len);

	SEC.text_upto += len;

	return ans;
}

static void SEC_add_term_name(SLONG id, CBYTE *name)
{
	SEC_Term *st = SEC_term_info(id);

	if (st == NULL)
	{
		if (SEC.term_upto >= SEC_MAX_TERMS)
		{
			return;
		}

		st     = &SEC.term[SEC.term_upto++];
		st->id = id;
	}

	strncpy(st->name, name, sizeof(st->name) - 1);
	st->name[sizeof(st->name) - 1] = 0;
}

static void SEC_add_cam(SLONG term, CBYTE *name, SLONG x, SLONG z, SLONG height, SLONG yaw, SLONG pitch)
{
	if (SEC.cam_upto >= SEC_MAX_CAMS)
	{
		return;
	}

	SEC_Cam *sc = &SEC.cam[SEC.cam_upto++];

	sc->term   = term;
	sc->x      = x;
	sc->z      = z;
	sc->height = height;
	sc->yaw    = yaw & 2047;
	sc->pitch  = pitch;

	strncpy(sc->name, name, sizeof(sc->name) - 1);
	sc->name[sizeof(sc->name) - 1] = 0;
}

static void SEC_add_file(SLONG term, CBYTE *title, CBYTE *text)
{
	if (SEC.file_upto >= SEC_MAX_FILES)
	{
		return;
	}

	SEC_File *sf = &SEC.file[SEC.file_upto++];

	sf->term = term;
	sf->text = SEC_text_store(text);

	strncpy(sf->title, title, sizeof(sf->title) - 1);
	sf->title[sizeof(sf->title) - 1] = 0;
}

// ---------------------------------------------------------------------------
// Creating turrets and terminals
// ---------------------------------------------------------------------------

static Thing *SEC_create(SLONG type, SLONG id, SLONG x, SLONG y, SLONG z, SLONG yaw, UWORD flag, SLONG range, SLONG arc)
{
	SATURATE(x, 0x100, (PAP_SIZE_HI << 8) - 0x100);
	SATURATE(z, 0x100, (PAP_SIZE_HI << 8) - 0x100);

	//
	// 'yaw' is the way it faces (like a person's Angle). The models point down -z;
	// [Extra] turret_yaw_offset turns them if that is the wrong way round.
	//

	yaw = (yaw + SEC_yaw_offset()) & 2047;

	THING_INDEX t_index = BAT_create(BAT_TYPE_TURRET, x, z, yaw);

	if (!t_index)
	{
		CONSOLE_text("No room for another turret / terminal", 4000);

		return NULL;
	}

	Thing *p_thing = TO_THING(t_index);
	Bat   *p_bat   = p_thing->Genus.Bat;

	if (y < 0)
	{
		y = PAP_calc_map_height_at(x, z);
	}

	p_bat->type     = type;
	p_bat->want_z   = id;
	p_bat->want_y   = y;
	p_bat->want_x   = range;
	p_bat->substate = arc;
	p_bat->flag     = flag;
	p_bat->target   = NULL;
	p_bat->timer    = 0;
	p_bat->glow     = 0;
	p_bat->dx       = 0;
	p_bat->dy       = yaw & 2047;
	p_bat->dz       = 0;

	p_thing->Draw.Tweened->Angle  = yaw & 2047;
	p_thing->Draw.Tweened->Tilt   = 0;
	p_thing->Draw.Tweened->Roll   = 0;
	p_thing->Draw.Tweened->MeshID = (type == BAT_TYPE_TERMINAL) ? 1 : 0;

	if (type == BAT_TYPE_TERMINAL)
	{
		p_bat->health = 255;
		p_bat->state  = 0;
	}
	else
	{
		p_bat->health = SEC_TURRET_HP;

		if (flag & SEC_F_CONCEALED)
		{
			p_bat->flag  |= SEC_F_HAS_HATCH;
			p_bat->state  = SEC_TS_HIDDEN;
			p_bat->dx     = SEC_HIDE_DEPTH;
		}
		else
		{
			p_bat->state  = SEC_TS_UP;
		}
	}

	p_thing->WorldPos.Y = (y - p_bat->dx) << 8;

	return p_thing;
}

// ---------------------------------------------------------------------------
// The level file
// ---------------------------------------------------------------------------

static void SEC_level_path(CBYTE *path, SLONG size)
{
	CBYTE *dot = NULL;
	CBYTE *ch;

	if (ELEV_fname_level[0] == 0)
	{
		strncpy(path, "levels\\default.sec", size);
		path[size - 1] = 0;

		return;
	}

	strncpy(path, ELEV_fname_level, size - 8);
	path[size - 8] = 0;

	for (ch = path; *ch; ch++)
	{
		if (*ch == '.')  {dot = ch;}
		if (*ch == '\\') {dot = NULL;}
	}

	if (dot)
	{
		*dot = 0;
	}

	strcat(path, ".sec");
}

//
// Reads the next word from *pp. Words in "quotes" can have spaces in them.
// Returns FALSE if there are no more.
//

static SLONG SEC_token(CBYTE **pp, CBYTE *out, SLONG size)
{
	CBYTE *p = *pp;
	SLONG  n = 0;

	while(*p == ' ' || *p == '\t')
	{
		p++;
	}

	if (*p == 0 || *p == '\r' || *p == '\n' || *p == '#')
	{
		*pp = p;
		out[0] = 0;

		return FALSE;
	}

	if (*p == '"')
	{
		p++;

		while(*p && *p != '"' && *p != '\r' && *p != '\n')
		{
			if (n < size - 1) {out[n++] = *p;}
			p++;
		}

		if (*p == '"')
		{
			p++;
		}
	}
	else
	{
		while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
		{
			if (n < size - 1) {out[n++] = *p;}
			p++;
		}
	}

	out[n] = 0;
	*pp    = p;

	return TRUE;
}

static SLONG SEC_squares(CBYTE *word)	// "64.5" => world units
{
	return SLONG(atof(word) * 256.0);
}

static SLONG SEC_degrees(CBYTE *word)	// degrees => 2048ths
{
	return SLONG(atof(word) * 2048.0 / 360.0) & 2047;
}

static void SEC_load_level_file(void)
{
	CBYTE path[_MAX_PATH];
	CBYTE line[512];
	CBYTE word[128];

	SEC_level_path(path, sizeof(path));

	FILE *handle = fopen(path, "rb");

	if (handle == NULL)
	{
		return;
	}

	SLONG in_file    = FALSE;
	SLONG file_term  = 0;
	CBYTE file_title[64];
	static CBYTE file_text[8192];
	SLONG file_len   = 0;

	while(fgets(line, sizeof(line), handle))
	{
		CBYTE *p = line;

		if (in_file)
		{
			//
			// Inside a file: everything up to "endfile" is its text.
			//

			CBYTE *q = line;

			while(*q == ' ' || *q == '\t') {q++;}

			if (strnicmp(q, "endfile", 7) == 0)
			{
				file_text[file_len] = 0;

				SEC_add_file(file_term, file_title, file_text);

				in_file = FALSE;

				continue;
			}

			SLONG len = strlen(line);

			while(len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
			{
				len--;
			}

			if (file_len + len + 2 < SLONG(sizeof(file_text)))
			{
				memcpy(&file_text[file_len], line, len);
				file_len += len;
				file_text[file_len++] = '\n';
			}

			continue;
		}

		if (!SEC_token(&p, word, sizeof(word)))
		{
			continue;
		}

		if (stricmp(word, "turret") == 0 || stricmp(word, "terminal") == 0)
		{
			SLONG is_turret = (stricmp(word, "turret") == 0);

			CBYTE a[4][64];
			SLONG i;

			for (i = 0; i < 4; i++)
			{
				if (!SEC_token(&p, a[i], sizeof(a[i])))
				{
					break;
				}
			}

			if (i < 4 || SEC.spawn_upto >= SEC_MAX_SPAWNS)
			{
				continue;
			}

			SEC_Spawn *ss = &SEC.spawn[SEC.spawn_upto++];

			ss->type  = is_turret ? BAT_TYPE_TURRET : BAT_TYPE_TERMINAL;
			ss->id    = atoi(a[0]);
			ss->x     = SEC_squares(a[1]);
			ss->z     = SEC_squares(a[2]);
			ss->yaw   = SEC_degrees(a[3]);
			ss->y     = -1;
			ss->flag  = 0;
			ss->range = 0;
			ss->arc   = 0;

			while(SEC_token(&p, word, sizeof(word)))
			{
				if      (stricmp(word, "friendly") == 0) {ss->flag |=  SEC_F_FRIENDLY;}
				else if (stricmp(word, "hostile" ) == 0) {ss->flag &= ~SEC_F_FRIENDLY;}
				else if (stricmp(word, "hidden"  ) == 0) {ss->flag |=  SEC_F_CONCEALED;}
				else if (stricmp(word, "exposed" ) == 0) {ss->flag &= ~SEC_F_CONCEALED;}
				else if (stricmp(word, "off"     ) == 0) {ss->flag |=  SEC_F_POWEROFF;}
				else if (stricmp(word, "on"      ) == 0) {ss->flag &= ~SEC_F_POWEROFF;}
				else if (strnicmp(word, "range=", 6) == 0) {ss->range = SEC_squares(word + 6);}
				else if (strnicmp(word, "arc=",   4) == 0) {ss->arc   = SEC_degrees(word + 4) >> 3;}
				else if (strnicmp(word, "y=",     2) == 0) {ss->y     = atoi(word + 2);}
				else if (!is_turret)
				{
					SEC_add_term_name(ss->id, word);
				}
			}
		}
		else
		if (stricmp(word, "link") == 0)
		{
			CBYTE a[2][32];

			if (SEC_token(&p, a[0], sizeof(a[0])) &&
				SEC_token(&p, a[1], sizeof(a[1])) &&
				SEC.link_upto < SEC_MAX_LINKS)
			{
				SEC_Link *sl = &SEC.link[SEC.link_upto++];

				sl->term   = atoi(a[0]);
				sl->turret = (stricmp(a[1], "all") == 0) ? SEC_ID_ALL : atoi(a[1]);
			}
		}
		else
		if (stricmp(word, "camera") == 0)
		{
			CBYTE a[7][64];
			SLONG i;

			for (i = 0; i < 7; i++)
			{
				if (!SEC_token(&p, a[i], sizeof(a[i])))
				{
					break;
				}
			}

			if (i >= 6)
			{
				SEC_add_cam(
					atoi(a[0]),
					a[1],
					SEC_squares(a[2]),
					SEC_squares(a[3]),
					atoi(a[4]),
					SEC_degrees(a[5]),
					(i >= 7) ? SEC_degrees(a[6]) : 1920);	// a little down
			}
		}
		else
		if (stricmp(word, "file") == 0)
		{
			CBYTE a[2][64];

			if (SEC_token(&p, a[0], sizeof(a[0])))
			{
				if (!SEC_token(&p, a[1], sizeof(a[1])))
				{
					strcpy(a[1], "Untitled");
				}

				file_term = atoi(a[0]);
				strncpy(file_title, a[1], sizeof(file_title) - 1);
				file_title[sizeof(file_title) - 1] = 0;
				file_len  = 0;
				in_file   = TRUE;
			}
		}
	}

	if (in_file)
	{
		file_text[file_len] = 0;
		SEC_add_file(file_term, file_title, file_text);
	}

	fclose(handle);

	CBYTE msg[_MAX_PATH + 40];

	sprintf(msg, "Security: %d objects from %s", SEC.spawn_upto, path);
	CONSOLE_text(msg, 3000);
}

static void SEC_spawn_level(void)
{
	SLONG i;

	SEC_load_level_file();

	for (i = 0; i < SEC.spawn_upto; i++)
	{
		SEC_Spawn *ss = &SEC.spawn[i];

		if (SEC_find_by_id(ss->type, ss->id))
		{
			//
			// Already there (a saved game was loaded).
			//

			continue;
		}

		SEC_create(ss->type, ss->id, ss->x, ss->y, ss->z, ss->yaw, ss->flag, ss->range, ss->arc);
	}
}

void SEC_reset(void)
{
	memset(&SEC, 0, sizeof(SEC));

	SEC.need_spawn = TRUE;
	SEC.file_index = -1;

	PolyPage::SetGreenScreen(false);
}

// ---------------------------------------------------------------------------
// Turrets
// ---------------------------------------------------------------------------

static SLONG SEC_turret_range(Bat *p_bat)
{
	if (p_bat->want_x > 0)
	{
		return p_bat->want_x;
	}

	return ConfigExtras::getInstance().mTurretRange << 8;
}

static SLONG SEC_turret_head_y(Thing *p_turret)
{
	return (p_turret->WorldPos.Y >> 8) + SEC_HEAD_HEIGHT;
}

static SLONG SEC_person_is_hostile(Thing *p_person)
{
	Person *pp = p_person->Genus.Person;

	if (pp->PlayerID)
	{
		return FALSE;
	}

	if (PersonIsMIB(p_person))
	{
		return TRUE;
	}

	Thing *p_player = NET_PERSON(0);

	if (p_player)
	{
		if (PCOM_person_wants_to_kill(p_person) == THING_NUMBER(p_player))
		{
			return TRUE;
		}

		if (PCOM_person_a_hates_b(p_person, p_player))
		{
			return TRUE;
		}
	}

	switch(pp->pcom_ai)
	{
		case PCOM_AI_GANG:
		case PCOM_AI_GUARD:
		case PCOM_AI_ASSASIN:
		case PCOM_AI_BULLY:
		case PCOM_AI_SHOOT_DEAD:
		case PCOM_AI_MIB:
		case PCOM_AI_BOSS:
			return TRUE;
	}

	return FALSE;
}

static SLONG SEC_can_shoot_at(Thing *p_turret, Thing *p_target, SLONG range)
{
	Bat *p_bat = p_turret->Genus.Bat;

	if (p_target->Class != CLASS_PERSON)
	{
		return FALSE;
	}

	if (is_person_dead(p_target) || is_person_ko(p_target) || p_target->Genus.Person->Health <= 0)
	{
		return FALSE;
	}

	if (p_target->Genus.Person->Flags & (FLAG_PERSON_DRIVING | FLAG_PERSON_PASSENGER | FLAG_PERSON_ARRESTED))
	{
		return FALSE;
	}

	if (!(p_target->Flags & FLAGS_ON_MAPWHO))
	{
		return FALSE;
	}

	SLONG dist = SEC_dist(p_turret, p_target);

	if (dist > range)
	{
		return FALSE;
	}

	SLONG ty = p_target->WorldPos.Y >> 8;
	SLONG hy = SEC_turret_head_y(p_turret);

	if (abs(ty - hy) > 0x300)
	{
		return FALSE;
	}

	if (p_bat->substate)
	{
		//
		// It can only turn so far.
		//

		SLONG want = (SEC_angle_to(p_turret->WorldPos.X >> 8, p_turret->WorldPos.Z >> 8, p_target->WorldPos.X >> 8, p_target->WorldPos.Z >> 8) + SEC_yaw_offset()) & 2047;

		if (abs(SEC_angle_diff(p_bat->dy, want)) > (p_bat->substate << 3))
		{
			return FALSE;
		}
	}

	if (!there_is_a_los(
			p_turret->WorldPos.X >> 8, hy, p_turret->WorldPos.Z >> 8,
			p_target->WorldPos.X >> 8, ty + 0x50, p_target->WorldPos.Z >> 8,
			LOS_FLAG_IGNORE_SEETHROUGH_FENCE_FLAG | LOS_FLAG_IGNORE_PRIMS))
	{
		return FALSE;
	}

	return TRUE;
}

static Thing *SEC_turret_find_target(Thing *p_turret)
{
	Bat   *p_bat = p_turret->Genus.Bat;
	SLONG  range = SEC_turret_range(p_bat);
	Thing *best  = NULL;
	SLONG  best_dist = INFINITY;
	SLONG  i;

	if (p_bat->flag & SEC_F_FRIENDLY)
	{
		SLONG num = THING_find_sphere(
						p_turret->WorldPos.X >> 8,
						p_turret->WorldPos.Y >> 8,
						p_turret->WorldPos.Z >> 8,
						range,
						THING_array,
						THING_ARRAY_SIZE,
						1 << CLASS_PERSON);

		for (i = 0; i < num; i++)
		{
			Thing *p_found = TO_THING(THING_array[i]);

			if (!SEC_person_is_hostile(p_found))
			{
				continue;
			}

			if (!SEC_can_shoot_at(p_turret, p_found, range))
			{
				continue;
			}

			SLONG dist = SEC_dist(p_turret, p_found);

			if (dist < best_dist)
			{
				best      = p_found;
				best_dist = dist;
			}
		}
	}
	else
	{
		for (i = 0; i < 2; i++)
		{
			Thing *p_player = NET_PERSON(i);

			if (p_player == NULL || p_player->Class != CLASS_PERSON)
			{
				continue;
			}

			if (!SEC_can_shoot_at(p_turret, p_player, range))
			{
				continue;
			}

			SLONG dist = SEC_dist(p_turret, p_player);

			if (dist < best_dist)
			{
				best      = p_player;
				best_dist = dist;
			}
		}
	}

	return best;
}

// ---------------------------------------------------------------------------
// People and turrets: shoot back or run away
// ---------------------------------------------------------------------------

static SEC_Threat *SEC_threat_of(Thing *p_person)
{
	SLONG i;
	THING_INDEX me = THING_NUMBER(p_person);

	for (i = 0; i < SEC_MAX_THREATS; i++)
	{
		if (SEC.threat[i].person == me)
		{
			return &SEC.threat[i];
		}
	}

	return NULL;
}

//
// 'p_person' has to do something about 'p_turret'. attack: he may shoot at it
// (an enemy of the player in the sights of a turret on the player's side); else
// he only runs away, and only if he has no gun.
//

static void SEC_threat_add(Thing *p_person, Thing *p_turret, SLONG attack)
{
	SLONG i;

	if (p_person == NULL || p_person->Class != CLASS_PERSON || p_person->Genus.Person->PlayerID)
	{
		return;
	}

	if (is_person_dead(p_person) || is_person_ko(p_person) || p_person->Genus.Person->Health <= 0)
	{
		return;
	}

	SEC_Threat *t = SEC_threat_of(p_person);

	if (t)
	{
		if (t->turret != THING_NUMBER(p_turret) && t->shooting)
		{
			//
			// Another turret: he turns on that one (his old target stays the one from before).
			//

			if (p_person->Genus.Person->Target == t->turret)
			{
				p_person->Genus.Person->Target = THING_NUMBER(p_turret);
			}
		}

		t->turret  = THING_NUMBER(p_turret);
		t->ms      = SEC_THREAT_MS;
		t->attack |= (attack) ? 1 : 0;

		return;
	}

	for (i = 0; i < SEC_MAX_THREATS; i++)
	{
		if (SEC.threat[i].person == NULL)
		{
			t = &SEC.threat[i];

			t->person     = THING_NUMBER(p_person);
			t->turret     = THING_NUMBER(p_turret);
			t->ms         = SEC_THREAT_MS;
			t->cool       = 250 + (Random() % 600);	// Reaction time
			t->old_target = NULL;
			t->attack     = (attack) ? 1 : 0;
			t->shooting   = FALSE;

			return;
		}
	}
}

//
// Everybody near (x,z) - where turret bullets are landing.
//

static void SEC_threat_near(SLONG x, SLONG z, SLONG radius, Thing *p_turret, SLONG attack_if_hostile)
{
	THING_INDEX found[32];
	SLONG       i;

	SLONG num = THING_find_sphere(x, PAP_calc_map_height_at(x, z), z, radius, found, 32, 1 << CLASS_PERSON);

	for (i = 0; i < num; i++)
	{
		Thing *p_found = TO_THING(found[i]);

		SEC_threat_add(p_found, p_turret, attack_if_hostile && SEC_person_is_hostile(p_found));
	}
}

//
// A turret on the player's side looks around: enemies it could shoot know it.
//

static void SEC_threat_scan(Thing *p_turret)
{
	THING_INDEX found[32];
	SLONG       i;
	SLONG       range = SEC_turret_range(p_turret->Genus.Bat);

	SLONG num = THING_find_sphere(
					p_turret->WorldPos.X >> 8,
					p_turret->WorldPos.Y >> 8,
					p_turret->WorldPos.Z >> 8,
					range,
					found,
					32,
					1 << CLASS_PERSON);

	for (i = 0; i < num; i++)
	{
		Thing *p_found = TO_THING(found[i]);

		if (p_found->Genus.Person->PlayerID || !SEC_person_is_hostile(p_found))
		{
			continue;
		}

		if (SEC_can_shoot_at(p_turret, p_found, range))
		{
			SEC_threat_add(p_found, p_turret, TRUE);
		}
	}
}

static SLONG SEC_person_armed(Thing *p_person)
{
	Thing *p_special;

	if (p_person->Genus.Person->PersonType == PERSON_MIB1 ||
		p_person->Genus.Person->PersonType == PERSON_MIB2 ||
		p_person->Genus.Person->PersonType == PERSON_MIB3)
	{
		return TRUE;
	}

	if ((p_person->Flags & FLAGS_HAS_GUN) && p_person->Genus.Person->Ammo)
	{
		return TRUE;
	}

	if ((p_special = person_has_special(p_person, SPECIAL_AK47))    && p_special->Genus.Special->ammo) {return TRUE;}
	if ((p_special = person_has_special(p_person, SPECIAL_SHOTGUN)) && p_special->Genus.Special->ammo) {return TRUE;}

	return FALSE;
}

static void SEC_threat_end(SEC_Threat *t, Thing *p_person)
{
	if (p_person && p_person->Class == CLASS_PERSON && t->shooting)
	{
		if (p_person->Genus.Person->Target == t->turret)
		{
			//
			// Back to whoever he was after before.
			//

			Thing *p_old = (t->old_target) ? TO_THING(t->old_target) : NULL;

			if (p_old && p_old->Class == CLASS_PERSON && !is_person_dead(p_old))
			{
				p_person->Genus.Person->Target = t->old_target;
			}
			else
			{
				p_person->Genus.Person->Target = NULL;
			}
		}
	}

	t->person   = NULL;
	t->shooting = FALSE;
}

SLONG SEC_person_ai(Thing *p_person)
{
	SEC_Threat *t = SEC_threat_of(p_person);

	if (t == NULL)
	{
		return FALSE;
	}

	SLONG   ms = SEC_tick_ms();
	Person *pp = p_person->Genus.Person;

	t->ms -= ms;

	Thing *p_turret = TO_THING(t->turret);

	SLONG still_there =
		t->ms > 0 &&
		SEC_is_security(p_turret) &&
		p_turret->Genus.Bat->type == BAT_TYPE_TURRET &&
		p_turret->Genus.Bat->state != SEC_TS_DEAD &&
		p_turret->Genus.Bat->state != SEC_TS_HIDDEN;

	SLONG able =
		!(pp->Flags & (FLAG_PERSON_HELPLESS | FLAG_PERSON_DRIVING | FLAG_PERSON_PASSENGER | FLAG_PERSON_BIKING | FLAG_PERSON_ARRESTED)) &&
		!is_person_dead(p_person) &&
		!is_person_ko(p_person) &&
		pp->Health > 0 &&
		p_person->State != STATE_DYING &&
		p_person->State != STATE_DEAD;

	if (!still_there || !able)
	{
		SEC_threat_end(t, p_person);

		return FALSE;
	}

	if (!SEC_person_armed(p_person))
	{
		//
		// Nothing to fight it with: run away from it (PCOM's own fleeing - he
		// comes back to what he was doing when he is far enough).
		//

		if (pp->pcom_ai_state != PCOM_AI_STATE_FLEE_PLACE &&
			pp->pcom_ai_state != PCOM_AI_STATE_FLEE_PERSON)
		{
			SEC_threat_end(t, p_person);	// (gives his target back first)

			PCOM_set_person_ai_flee_place(p_person, p_turret->WorldPos.X >> 8, p_turret->WorldPos.Z >> 8);
		}
		else
		{
			SEC_threat_end(t, p_person);
		}

		return FALSE;
	}

	if (!t->attack)
	{
		//
		// Armed, but it's not his business (a turret on his own side, say).
		//

		SEC_threat_end(t, p_person);

		return FALSE;
	}

	//
	// Can he see it from here?
	//

	SLONG px = p_person->WorldPos.X >> 8;
	SLONG py = p_person->WorldPos.Y >> 8;
	SLONG pz = p_person->WorldPos.Z >> 8;

	if (SEC_dist(p_person, p_turret) > 0x100 * 14 ||
		!there_is_a_los(
			px, py + 0x60, pz,
			p_turret->WorldPos.X >> 8, SEC_turret_head_y(p_turret), p_turret->WorldPos.Z >> 8,
			LOS_FLAG_IGNORE_SEETHROUGH_FENCE_FLAG | LOS_FLAG_IGNORE_PRIMS))
	{
		//
		// No: let him do what he does (he may come round the corner).
		//

		if (t->shooting && pp->Target == t->turret)
		{
			pp->Target  = (t->old_target) ? t->old_target : NULL;
			t->shooting = FALSE;
		}

		return FALSE;
	}

	//
	// Shoot back. Wait while he is drawing / firing / staggering.
	//

	if (PCOM_person_move_busy(p_person))
	{
		return TRUE;
	}

	if (p_person->State != STATE_IDLE    &&
		p_person->State != STATE_MOVEING &&
		p_person->State != STATE_GUN)
	{
		return TRUE;
	}

	if (!PCOM_person_has_gun_in_hand(p_person))
	{
		PCOM_set_person_move_draw_firearm(p_person);

		return TRUE;
	}

	if (p_person->State == STATE_MOVEING)
	{
		PCOM_set_person_move_still(p_person);
	}

	set_face_thing(p_person, p_turret);

	t->cool -= ms;

	if (t->cool <= 0)
	{
		if (!t->shooting)
		{
			t->old_target = (pp->Target != t->turret) ? pp->Target : NULL;
			t->shooting   = TRUE;
		}

		//
		// The shot is worked out straight away (set_person_shoot -> actually_fire_gun,
		// which hits bats with BAT_apply_hit), so the turret is his target only for
		// that moment: the rest of the game expects a person's target to be a person.
		//

		THING_INDEX keep = (pp->Target != t->turret) ? pp->Target : t->old_target;

		pp->Target = t->turret;

		PCOM_set_person_move_shoot(p_person);

		pp->Target = keep;

		t->cool = 450 + (Random() % 900);
	}

	return TRUE;
}

static void SEC_hurt_person(Thing *p_target, Thing *p_turret, SLONG damage)
{
	if (is_person_dead(p_target) || is_person_ko(p_target))
	{
		return;
	}

	//
	// Blood.
	//

	if (VIOLENCE)
	{
		PARTICLE_Add(
			p_target->WorldPos.X + ((Random() & 0x1fff) - 0xfff),
			p_target->WorldPos.Y + (0x50 << 8),
			p_target->WorldPos.Z + ((Random() & 0x1fff) - 0xfff),
			0, 0, 0,
			POLY_PAGE_SMOKECLOUD2, 2 + ((Random() & 3) << 2), 0x7FFF0000,
			PFLAG_SPRITEANI | PFLAG_SPRITELOOP | PFLAG_FADE, 10, 75, 1, 20, 5);
	}

	p_target->Genus.Person->Health -= damage;

	SLONG dangle = get_dangle(p_target, p_turret);
	SLONG behind = (dangle > 512 && dangle < 1536);

	if (p_target->Genus.Person->Health <= 0)
	{
		p_target->Genus.Person->Health = 0;

		if (!is_there_room_behind_person(p_target, behind))
		{
			behind = !behind;
		}

		SLONG death_type = PERSON_DEATH_TYPE_COMBAT;

		if (person_on_floor(p_target) && p_target->OnFace == 0 && really_on_floor(p_target))
		{
			death_type = PERSON_DEATH_TYPE_SHOT_AK47;
		}

		set_person_dead(p_target, NULL, death_type, behind, 0);
	}
	else
	if ((Random() & 3) == 0 && p_target->State != STATE_JUMPING)
	{
		set_person_recoil(p_target, ANIM_HIT_FRONT_MID, 0);
	}
}

static void SEC_turret_fire(Thing *p_turret, Thing *p_target)
{
	Bat *p_bat = p_turret->Genus.Bat;
	ConfigExtras &extras = ConfigExtras::getInstance();

	//
	// Where the bullet comes out: the end of one of the two barrels.
	//

	SLONG a    = (p_turret->Draw.Tweened->Angle - SEC_yaw_offset()) & 2047;
	SLONG fx   = -SIN(a);
	SLONG fz   = -COS(a);
	SLONG side = (GAME_TURN & 1) ? 9 : -9;

	SLONG mx = (p_turret->WorldPos.X >> 8) + (fx * SEC_BARREL_LENGTH >> 16) + (COS(a) * side >> 16);
	SLONG my = SEC_turret_head_y(p_turret);
	SLONG mz = (p_turret->WorldPos.Z >> 8) + (fz * SEC_BARREL_LENGTH >> 16) - (SIN(a) * side >> 16);

	MFX_play_xyz(THING_NUMBER(p_turret), S_AK47_SHOT, 0, mx << 8, my << 8, mz << 8);

	PARTICLE_Add(
		mx << 8, my << 8, mz << 8,
		0, 0, 0,
		POLY_PAGE_BLOOM1, 0, 0xffffd060,
		PFLAG_FADE, 6, 40, 1, -6, 0);

	//
	// Will it hit?
	//

	SLONG dist   = SEC_dist(p_turret, p_target);
	SLONG chance = 215 - (dist * 110 >> 11);

	chance -= p_target->Velocity;

	if (p_target->Genus.Person->PlayerID)
	{
		chance -= 25;
	}

	if (p_target->SubState == SUB_STATE_FLIPING)
	{
		chance >>= 1;
	}

	SATURATE(chance, 30, 240);

	SLONG tx = p_target->WorldPos.X >> 8;
	SLONG ty = (p_target->WorldPos.Y >> 8) + 0x50;
	SLONG tz = p_target->WorldPos.Z >> 8;

	SLONG hit     = ((Random() & 0xff) < chance);
	SLONG blocked = FALSE;
	SLONG damage  = (p_target->Genus.Person->PlayerID) ? extras.mTurretDamagePlayer : extras.mTurretDamageNpc;

	if (hit)
	{
		//
		// The riot shield works against turrets too.
		//

		Thing *p_shield = SPECIAL_get_active_shield(p_target);

		if (p_shield)
		{
			SLONG arc    = extras.mShieldArc * 2048 / 360;
			SLONG dangle = get_dangle(p_target, p_turret);

			if (dangle > 1024)
			{
				dangle = 2048 - dangle;
			}

			if (dangle <= arc)
			{
				if ((Random() % 100) < extras.mShieldBlockChance)
				{
					SPECIAL_shield_hit(p_target, p_shield, 1);

					blocked = TRUE;
				}
				else
				{
					damage = damage * extras.mShieldDamagePercent / 100;
				}
			}
		}
	}

	if (!hit || blocked)
	{
		//
		// Ricochet next to the target.
		//

		tx += (Random() & 0x3f) - 0x1f;
		tz += (Random() & 0x3f) - 0x1f;

		if (!blocked)
		{
			ty = PAP_calc_map_height_at(tx, tz) + (Random() & 0x3f);
		}

		MFX_play_xyz(0, SOUND_Range(S_RICOCHET_START, S_RICOCHET_END), 0, tx << 8, ty << 8, tz << 8);

		DIRT_new_sparks(tx, ty, tz, 2);
	}

	//
	// The one it shoots at and the people round him know about it (a turret on
	// the player's side gets shot at by the armed ones; the others run).
	//

	if (!p_target->Genus.Person->PlayerID)
	{
		SEC_threat_add(p_target, p_turret, (p_bat->flag & SEC_F_FRIENDLY) != 0);
	}

	if ((GAME_TURN & 3) == 0)
	{
		SEC_threat_near(tx, tz, 0x300, p_turret, (p_bat->flag & SEC_F_FRIENDLY) != 0);
	}

	//
	// A tracer from the barrel to where it hit.
	//

	{
		SPARK_Pinfo p1;
		SPARK_Pinfo p2;

		p1.type   = SPARK_TYPE_POINT;
		p1.flag   = 0;
		p1.person = 0;
		p1.limb   = 0;
		p1.x      = mx;
		p1.y      = my;
		p1.z      = mz;

		p2.type   = SPARK_TYPE_POINT;
		p2.flag   = 0;
		p2.person = 0;
		p2.limb   = 0;
		p2.x      = tx;
		p2.y      = ty;
		p2.z      = tz;

		SPARK_create(&p1, &p2, 3);
	}

	if (hit && !blocked)
	{
		if (damage < 1)
		{
			damage = 1;
		}

		SEC_hurt_person(p_target, p_turret, damage);
	}

	p_bat->timer = extras.mTurretFireMs;
}

// ---------------------------------------------------------------------------
// Manual control: where the barrels point, firing, picking targets
// ---------------------------------------------------------------------------

#define SEC_MAN_PITCH_MIN	(-170)		// About 30 degrees down
#define SEC_MAN_PITCH_MAX	( 114)		// 20 up
#define SEC_MAN_PITCH_START	(-48)		// Like the sensor feed

//
// The direction the barrels point (unit vector, world space).
//

static void SEC_man_dir(Thing *p_turret, float *dx, float *dy, float *dz)
{
	//
	// (SIN / COS are the game's 2048-step tables, 1.0 = 65536; no <math.h> here:
	// it would redefine the game's INFINITY.)
	//

	SLONG a = (p_turret->Draw.Tweened->Angle - SEC_yaw_offset()) & 2047;
	SLONG p = SEC.man_pitch & 2047;

	const float one = 1.0f / 65536.0f;

	float cp = float(COS(p)) * one;

	*dx = -float(SIN(a)) * one * cp;
	*dy =  float(SIN(p)) * one;
	*dz = -float(COS(a)) * one * cp;
}

//
// The person a shot along the barrels would hit (closest along the line, line of
// sight from the barrels), or NULL; *along = how far.
//

static Thing *SEC_man_person_on_line(Thing *p_turret, SLONG mx, SLONG my, SLONG mz, float dx, float dy, float dz, SLONG range, SLONG *along)
{
	THING_INDEX found[64];
	SLONG       i;
	Thing      *best      = NULL;
	float       best_dist = float(range);

	SLONG num = THING_find_sphere(
					p_turret->WorldPos.X >> 8,
					p_turret->WorldPos.Y >> 8,
					p_turret->WorldPos.Z >> 8,
					range,
					found,
					64,
					1 << CLASS_PERSON);

	for (i = 0; i < num; i++)
	{
		Thing *p_found = TO_THING(found[i]);

		if (p_found->Genus.Person->PlayerID)
		{
			continue;	// The player's own turret doesn't shoot him
		}

		if (is_person_dead(p_found) || p_found->Genus.Person->Health <= 0)
		{
			continue;
		}

		if (!(p_found->Flags & FLAGS_ON_MAPWHO) || (p_found->Genus.Person->Flags & (FLAG_PERSON_DRIVING | FLAG_PERSON_PASSENGER)))
		{
			continue;
		}

		//
		// A person is a stick from his feet to his head (0xb0 up), about 0x28 thick.
		//

		float fx = float(p_found->WorldPos.X >> 8) - float(mx);
		float fy = float(p_found->WorldPos.Y >> 8) - float(my);
		float fz = float(p_found->WorldPos.Z >> 8) - float(mz);

		float t     = fx * dx + fz * dz;		// Along the line, flat
		float flat2 = dx * dx + dz * dz;

		if (flat2 < 0.0025f)
		{
			continue;
		}

		t /= flat2;								// In units of the 3D direction

		if (t <= 0.0f || t >= best_dist)
		{
			continue;
		}

		float lx = dx * t - fx;
		float lz = dz * t - fz;
		float ly = dy * t - fy;					// Height of the line over his feet

		float side2 = lx * lx + lz * lz;
		float slop  = 0x28 + t * (1.0f / 24.0f);	// A little spread further away

		if (side2 > slop * slop || ly < -8.0f || ly > float(0xb0))
		{
			continue;
		}

		if (!there_is_a_los(
				mx, my, mz,
				p_found->WorldPos.X >> 8, (p_found->WorldPos.Y >> 8) + 0x50, p_found->WorldPos.Z >> 8,
				LOS_FLAG_IGNORE_SEETHROUGH_FENCE_FLAG | LOS_FLAG_IGNORE_PRIMS))
		{
			continue;
		}

		best      = p_found;
		best_dist = t;
	}

	*along = SLONG(best_dist);

	return best;
}

static void SEC_man_fire(Thing *p_turret)
{
	Bat *p_bat = p_turret->Genus.Bat;
	ConfigExtras &extras = ConfigExtras::getInstance();

	SLONG a    = (p_turret->Draw.Tweened->Angle - SEC_yaw_offset()) & 2047;
	SLONG side = (GAME_TURN & 1) ? 9 : -9;

	SLONG mx = (p_turret->WorldPos.X >> 8) + (-SIN(a) * SEC_BARREL_LENGTH >> 16) + (COS(a) * side >> 16);
	SLONG my = SEC_turret_head_y(p_turret);
	SLONG mz = (p_turret->WorldPos.Z >> 8) + (-COS(a) * SEC_BARREL_LENGTH >> 16) - (SIN(a) * side >> 16);

	float dx, dy, dz;

	SEC_man_dir(p_turret, &dx, &dy, &dz);

	MFX_play_xyz(THING_NUMBER(p_turret), S_AK47_SHOT, 0, mx << 8, my << 8, mz << 8);

	PARTICLE_Add(
		mx << 8, my << 8, mz << 8,
		0, 0, 0,
		POLY_PAGE_BLOOM1, 0, 0xffffd060,
		PFLAG_FADE, 6, 40, 1, -6, 0);

	SEC.man_shots += 1;

	SLONG  range  = SEC_turret_range(p_bat) * 5 >> 2;	// A person aiming reaches a bit further
	SLONG  along  = range;
	Thing *p_hit  = SEC_man_person_on_line(p_turret, mx, my, mz, dx, dy, dz, range, &along);

	SLONG tx, ty, tz;
	SLONG hit = FALSE;

	if (p_hit)
	{
		SLONG chance = 235 - (along * 80 >> 11) - p_hit->Velocity;

		SATURATE(chance, 40, 240);

		hit = ((Random() & 0xff) < chance);

		tx = p_hit->WorldPos.X >> 8;
		ty = (p_hit->WorldPos.Y >> 8) + 0x50;
		tz = p_hit->WorldPos.Z >> 8;
	}
	else
	{
		//
		// Where the bullet stops: a wall, the ground, or nothing within range.
		//

		SLONG ex = mx + SLONG(dx * range);
		SLONG ey = my + SLONG(dy * range);
		SLONG ez = mz + SLONG(dz * range);

		if (!there_is_a_los(mx, my, mz, ex, ey, ez, LOS_FLAG_IGNORE_SEETHROUGH_FENCE_FLAG | LOS_FLAG_IGNORE_PRIMS))
		{
			tx = los_failure_x;
			ty = los_failure_y;
			tz = los_failure_z;

			MFX_play_xyz(0, SOUND_Range(S_RICOCHET_START, S_RICOCHET_END), 0, tx << 8, ty << 8, tz << 8);

			DIRT_new_sparks(tx, ty, tz, 2);
		}
		else
		{
			tx = ex;
			ty = ey;
			tz = ez;
		}
	}

	if (p_hit && !hit)
	{
		//
		// Just missed him.
		//

		tx += (Random() & 0x3f) - 0x1f;
		tz += (Random() & 0x3f) - 0x1f;
		ty  = PAP_calc_map_height_at(tx, tz) + (Random() & 0x3f);

		MFX_play_xyz(0, SOUND_Range(S_RICOCHET_START, S_RICOCHET_END), 0, tx << 8, ty << 8, tz << 8);

		DIRT_new_sparks(tx, ty, tz, 2);
	}

	{
		SPARK_Pinfo p1;
		SPARK_Pinfo p2;

		p1.type   = SPARK_TYPE_POINT;
		p1.flag   = 0;
		p1.person = 0;
		p1.limb   = 0;
		p1.x      = mx;
		p1.y      = my;
		p1.z      = mz;

		p2.type   = SPARK_TYPE_POINT;
		p2.flag   = 0;
		p2.person = 0;
		p2.limb   = 0;
		p2.x      = tx;
		p2.y      = ty;
		p2.z      = tz;

		SPARK_create(&p1, &p2, 3);
	}

	if (p_hit)
	{
		//
		// Whoever it was aimed at fights back (or runs) - whatever side he is on.
		//

		SEC_threat_add(p_hit, p_turret, TRUE);

		if (hit)
		{
			SEC.man_hits += 1;

			SEC_hurt_person(p_hit, p_turret, MAX(extras.mTurretDamageNpc, 1));
		}

		p_bat->target = THING_NUMBER(p_hit);
	}
	else
	{
		p_bat->target = NULL;
	}

	SEC_threat_near(tx, tz, 0x280, p_turret, TRUE);
}

//
// The next person in view to the right of the one locked on (or of the crosshair).
//

static SLONG SEC_man_is_target(Thing *p_turret, Thing *p_person)
{
	if (p_person == NULL || p_person->Class != CLASS_PERSON || p_person->Genus.Person->PlayerID)
	{
		return FALSE;
	}

	return SEC_can_shoot_at(p_turret, p_person, SEC_turret_range(p_turret->Genus.Bat) * 5 >> 2);
}

static void SEC_man_next_target(Thing *p_turret)
{
	THING_INDEX found[64];
	SLONG       i;
	SLONG       range = SEC_turret_range(p_turret->Genus.Bat) * 5 >> 2;
	SLONG       here  = (p_turret->Draw.Tweened->Angle - SEC_yaw_offset()) & 2047;

	SLONG num = THING_find_sphere(
					p_turret->WorldPos.X >> 8,
					p_turret->WorldPos.Y >> 8,
					p_turret->WorldPos.Z >> 8,
					range,
					found,
					64,
					1 << CLASS_PERSON);

	//
	// Angle of the current lock (or the crosshair) - then the smallest turn to the
	// right (clockwise: the angle goes down) past it. Hostile people first.
	//

	SLONG from = here;

	if (SEC.man_lock && SEC_man_is_target(p_turret, TO_THING(SEC.man_lock)))
	{
		Thing *p_lock = TO_THING(SEC.man_lock);

		from = SEC_angle_to(p_turret->WorldPos.X >> 8, p_turret->WorldPos.Z >> 8, p_lock->WorldPos.X >> 8, p_lock->WorldPos.Z >> 8);
	}

	Thing *best       = NULL;
	SLONG  best_score = 0x7fffffff;

	for (i = 0; i < num; i++)
	{
		Thing *p_found = TO_THING(found[i]);

		if (THING_NUMBER(p_found) == SEC.man_lock || !SEC_man_is_target(p_turret, p_found))
		{
			continue;
		}

		SLONG a = SEC_angle_to(p_turret->WorldPos.X >> 8, p_turret->WorldPos.Z >> 8, p_found->WorldPos.X >> 8, p_found->WorldPos.Z >> 8);

		SLONG turn = (from - a) & 2047;		// To the right
		SLONG score = turn + ((SEC_person_is_hostile(p_found)) ? 0 : 4096);

		if (score < best_score)
		{
			best       = p_found;
			best_score = score;
		}
	}

	if (best)
	{
		SEC.man_lock = THING_NUMBER(best);

		MFX_play_ambient(0, S_BEEP, 0);
		SEC_set_status(SEC_person_is_hostile(best) ? "Target locked - hostile." : "Target locked - NOT a known hostile.");
	}
	else
	{
		SEC.man_lock = NULL;

		MFX_play_ambient(0, S_PISTOL_DRY, 0);
		SEC_set_status("No target in range.");
	}
}

static void SEC_turret_destroy(Thing *p_turret)
{
	Bat *p_bat = p_turret->Genus.Bat;

	p_bat->state  = SEC_TS_DEAD;
	p_bat->health = 0;
	p_bat->target = NULL;

	GameCoord pos = p_turret->WorldPos;

	pos.Y += SEC_HEAD_HEIGHT << 8;

	PYRO_create(pos, PYRO_FIREBOMB);

	MFX_play_xyz(THING_NUMBER(p_turret), SOUND_Range(S_EXPLODE_START, S_EXPLODE_END), 0, pos.X, pos.Y, pos.Z);

	DIRT_new_sparks(pos.X >> 8, pos.Y >> 8, pos.Z >> 8, 2 | 32);

	p_turret->Draw.Tweened->Tilt = 180;

	CBYTE msg[64];

	sprintf(msg, "Turret T-%02d destroyed", p_bat->want_z);
	CONSOLE_text(msg, 3000);
}

static void SEC_emp_sparks(Thing *p_thing)
{
	SPARK_in_sphere(
		p_thing->WorldPos.X >> 8,
		(p_thing->WorldPos.Y >> 8) + 0x40,
		p_thing->WorldPos.Z >> 8,
		0x40,
		20,
		2);
}

static void SEC_turret_process(Thing *p_turret)
{
	Bat       *p_bat = p_turret->Genus.Bat;
	DrawTween *dt    = p_turret->Draw.Tweened;
	SLONG      ms    = SEC_tick_ms();

	dt->MeshID = 0;

	if (p_bat->state == SEC_TS_DEAD)
	{
		//
		// Smouldering wreck.
		//

		if ((Random() & 0x1f) == 0)
		{
			PARTICLE_Add(
				p_turret->WorldPos.X, p_turret->WorldPos.Y + (0x40 << 8), p_turret->WorldPos.Z,
				0, 0x200, 0,
				POLY_PAGE_SMOKECLOUD2, 2, 0x7f404040,
				PFLAG_SPRITEANI | PFLAG_SPRITELOOP | PFLAG_FADE | PFLAG_RESIZE, 60, 40, 1, 2, 2);
		}

		if ((Random() & 0x3f) == 0)
		{
			SEC_emp_sparks(p_turret);
		}

		return;
	}

	//
	// EMP / booting up.
	//

	if (p_bat->glow)
	{
		p_bat->glow = (p_bat->glow > ms) ? p_bat->glow - ms : 0;

		if ((Random() & 0x7) == 0)
		{
			SEC_emp_sparks(p_turret);
		}

		if (p_bat->glow == 0)
		{
			MFX_play_xyz(THING_NUMBER(p_turret), S_SWITCH, 0, p_turret->WorldPos.X, p_turret->WorldPos.Y, p_turret->WorldPos.Z);
		}
	}

	SLONG powered = !(p_bat->flag & SEC_F_POWEROFF) && p_bat->glow == 0;

	if (p_bat->timer)
	{
		p_bat->timer = (p_bat->timer > ms) ? p_bat->timer - ms : 0;
	}

	//
	// In manual control from a terminal it does nothing on its own (SEC_ui_process
	// turns it and fires it).
	//

	SLONG manual = (SEC.ui == SEC_UI_MANUAL && SEC.turret_thing == THING_NUMBER(p_turret));

	Thing *p_target = NULL;

	if (powered && !manual)
	{
		p_target = SEC_turret_find_target(p_turret);
	}

	if (!manual)
	{
		p_bat->target = (p_target) ? THING_NUMBER(p_target) : NULL;
	}

	if (p_target || manual)
	{
		p_bat->dz = 0;
	}
	else
	if (p_bat->dz < 30000)
	{
		p_bat->dz += ms;
	}

	SLONG move = SEC_RISE_SPEED * ms / 1000 + 1;

	switch(p_bat->state)
	{
		case SEC_TS_HIDDEN:

			if (p_target || (manual && powered))
			{
				p_bat->state = SEC_TS_RISING;

				MFX_play_xyz(THING_NUMBER(p_turret), S_SWITCH, 0, p_turret->WorldPos.X, p_turret->WorldPos.Y, p_turret->WorldPos.Z);
				DIRT_new_sparks(p_turret->WorldPos.X >> 8, p_bat->want_y, p_turret->WorldPos.Z >> 8, 2);
			}
			else
			if (!(p_bat->flag & SEC_F_CONCEALED) && powered)
			{
				//
				// Reprogrammed to be out all the time.
				//

				p_bat->state = SEC_TS_RISING;
			}

			break;

		case SEC_TS_RISING:

			if (powered)
			{
				p_bat->dx -= move;

				if (p_bat->dx <= 0)
				{
					p_bat->dx    = 0;
					p_bat->state = SEC_TS_UP;
				}
			}

			break;

		case SEC_TS_UP:

			if (powered && !p_target && !manual && (p_bat->flag & SEC_F_CONCEALED) && p_bat->dz > SEC_RETRACT_MS)
			{
				p_bat->state = SEC_TS_LOWERING;

				MFX_play_xyz(THING_NUMBER(p_turret), S_SWITCH, 0, p_turret->WorldPos.X, p_turret->WorldPos.Y, p_turret->WorldPos.Z);
			}

			break;

		case SEC_TS_LOWERING:

			if (powered)
			{
				if (p_target || manual)
				{
					p_bat->state = SEC_TS_RISING;
				}
				else
				{
					p_bat->dx += move;

					if (p_bat->dx >= SEC_HIDE_DEPTH)
					{
						p_bat->dx    = SEC_HIDE_DEPTH;
						p_bat->state = SEC_TS_HIDDEN;
					}
				}
			}

			break;

		default:
			p_bat->state = SEC_TS_UP;
			break;
	}

	//
	// On the player's side: the enemies it could shoot know it is there.
	//

	if (powered && p_bat->state == SEC_TS_UP &&
		((p_bat->flag & SEC_F_FRIENDLY) || manual) &&
		((GAME_TURN + THING_NUMBER(p_turret)) & 7) == 0)
	{
		SEC_threat_scan(p_turret);
	}

	//
	// Turning and shooting.
	//

	if (powered && p_bat->state == SEC_TS_UP && !manual)
	{
		SLONG want;

		if (p_target)
		{
			want = (SEC_angle_to(
						p_turret->WorldPos.X >> 8, p_turret->WorldPos.Z >> 8,
						p_target->WorldPos.X >> 8, p_target->WorldPos.Z >> 8) + SEC_yaw_offset()) & 2047;
		}
		else
		{
			//
			// Nothing to shoot: sweep slowly from side to side.
			//

			SLONG sweep = (p_bat->substate) ? (p_bat->substate << 3) : 300;

			if (sweep > 300) {sweep = 300;}

			want = (p_bat->dy + (SIN((GAME_TURN * 6 + THING_NUMBER(p_turret) * 97) & 2047) * sweep >> 16)) & 2047;
		}

		SLONG diff = SEC_angle_diff(dt->Angle, want);
		SLONG step = SEC_TURN_SPEED * ms / 1000 + 1;

		if (!p_target)
		{
			step >>= 2;
		}

		SATURATE(diff, -step, step);

		dt->Angle = (dt->Angle + diff) & 2047;

		if (p_target && p_bat->timer == 0 && abs(SEC_angle_diff(dt->Angle, want)) < SEC_AIM_TOLERANCE)
		{
			SEC_turret_fire(p_turret, p_target);
		}
	}

	//
	// No power: the head droops.
	//

	SLONG want_tilt = (powered || p_bat->state == SEC_TS_HIDDEN) ? 0 : 70;

	if (dt->Tilt < want_tilt) {dt->Tilt += 4;}
	if (dt->Tilt > want_tilt) {dt->Tilt -= 4;}

	p_turret->WorldPos.Y = (p_bat->want_y - p_bat->dx) << 8;
}

// ---------------------------------------------------------------------------
// Terminals
// ---------------------------------------------------------------------------

static void SEC_close_ui(SLONG back_to_idle);

static void SEC_terminal_process(Thing *p_term)
{
	Bat  *p_bat = p_term->Genus.Bat;
	SLONG ms    = SEC_tick_ms();

	p_term->Draw.Tweened->MeshID = 1;

	if (p_bat->glow)
	{
		p_bat->glow = (p_bat->glow > ms) ? p_bat->glow - ms : 0;

		if ((Random() & 0x7) == 0)
		{
			SEC_emp_sparks(p_term);
		}
	}

	p_term->WorldPos.Y = p_bat->want_y << 8;
}

void SEC_bat_process(Thing *p_thing)
{
	if (p_thing->Genus.Bat->type == BAT_TYPE_TERMINAL)
	{
		SEC_terminal_process(p_thing);
	}
	else
	{
		SEC_turret_process(p_thing);
	}
}

void SEC_bat_hit(Thing *p_thing, Thing *p_aggressor, SLONG damage)
{
	Bat *p_bat = p_thing->Genus.Bat;

	SLONG sx = p_thing->WorldPos.X >> 8;
	SLONG sy = (p_thing->WorldPos.Y >> 8) + SEC_HEAD_HEIGHT;
	SLONG sz = p_thing->WorldPos.Z >> 8;

	DIRT_new_sparks(sx + (Random() & 0x1f) - 0xf, sy, sz + (Random() & 0x1f) - 0xf, 2);

	MFX_play_xyz(0, SOUND_Range(S_RICOCHET_START, S_RICOCHET_END), 0, sx << 8, sy << 8, sz << 8);

	if (p_bat->type != BAT_TYPE_TURRET)
	{
		return;	// Terminals are armoured.
	}

	if (p_bat->state == SEC_TS_DEAD || p_bat->state == SEC_TS_HIDDEN)
	{
		return;
	}

	damage = damage / 3 + 1;

	if (p_bat->health <= damage)
	{
		SEC_turret_destroy(p_thing);
	}
	else
	{
		p_bat->health -= damage;
	}
}

SLONG SEC_bat_targetable(Thing *p_thing)
{
	if (!SEC_is_security(p_thing))
	{
		return TRUE;
	}

	Bat *p_bat = p_thing->Genus.Bat;

	if (p_bat->type == BAT_TYPE_TERMINAL)
	{
		return FALSE;
	}

	return (p_bat->state != SEC_TS_DEAD && p_bat->dx < 50);
}

// ---------------------------------------------------------------------------
// EMP
// ---------------------------------------------------------------------------

void SEC_emp_blast(SLONG x, SLONG y, SLONG z, Thing *p_owner)
{
	ConfigExtras &extras = ConfigExtras::getInstance();

	SLONG radius = extras.mEmpRadius << 8;
	SLONG ms     = extras.mEmpSeconds * 1000;
	SLONG count  = 0;
	SLONG i;

	SATURATE(ms, 1000, 65000);

	SLONG wx = x >> 8;
	SLONG wy = y >> 8;
	SLONG wz = z >> 8;

	//
	// A flash of blue-white light, a crackle and sparks everywhere.
	//

	MFX_play_xyz(0, SOUND_Range(S_ELEC_START, S_ELEC_END), 0, x, y, z);
	MFX_play_xyz(0, SOUND_Range(S_ELEC_START, S_ELEC_END), 0, x, y + 0x4000, z);

	PARTICLE_Add(x, y + (0x30 << 8), z, 0, 0, 0, POLY_PAGE_BLOOM1, 0, 0xff80c0ff, PFLAG_FADE, 60, 255, 1, -4, 0);
	PARTICLE_Add(x, y + (0x30 << 8), z, 0, 0, 0, POLY_PAGE_BLOOM1, 0, 0xff4080ff, PFLAG_FADE | PFLAG_RESIZE, 90, 160, 1, -3, 12);

	for (i = 0; i < 6; i++)
	{
		SPARK_in_sphere(wx, wy + 0x30, wz, 0xc0, 40, 4);
	}

	for (i = 0; i < 12; i++)
	{
		SLONG a  = i * 2048 / 12;
		SLONG sx = wx + (SIN(a) * 0x120 >> 16);
		SLONG sz = wz + (COS(a) * 0x120 >> 16);

		DIRT_new_sparks(sx, PAP_calc_map_height_at(sx, sz) + 8, sz, 2);
	}

	//
	// Knock out everything electrical in range.
	//

	SLONG list = thing_class_head[CLASS_BAT];

	while(list)
	{
		Thing *p_thing = TO_THING(list);

		list = p_thing->NextLink;

		if (!SEC_is_security(p_thing))
		{
			continue;
		}

		SLONG dx = abs((p_thing->WorldPos.X >> 8) - wx);
		SLONG dz = abs((p_thing->WorldPos.Z >> 8) - wz);
		SLONG dy = abs((p_thing->WorldPos.Y >> 8) - wy);

		if (QDIST2(dx, dz) > radius || dy > 0x400)
		{
			continue;
		}

		Bat *p_bat = p_thing->Genus.Bat;

		if (p_bat->type == BAT_TYPE_TURRET && p_bat->state == SEC_TS_DEAD)
		{
			continue;
		}

		if (p_bat->glow < ms)
		{
			p_bat->glow = ms;
		}

		SEC_emp_sparks(p_thing);

		count += 1;

		if (p_bat->type == BAT_TYPE_TERMINAL && SEC.ui != SEC_UI_NONE && SEC.term_thing == THING_NUMBER(p_thing))
		{
			SEC_close_ui(TRUE);
		}
	}

	CBYTE msg[64];

	sprintf(msg, "EMP: %d security system%s down", count, (count == 1) ? "" : "s");
	CONSOLE_text(msg, 3000);
}

// ---------------------------------------------------------------------------
// The terminal screen: what is on it
// ---------------------------------------------------------------------------

static SLONG SEC_term_controls(SLONG term_id, SLONG turret_id)
{
	SLONG i;
	SLONG any = FALSE;

	for (i = 0; i < SEC.link_upto; i++)
	{
		if (SEC.link[i].term == term_id)
		{
			any = TRUE;

			if (SEC.link[i].turret == SEC_ID_ALL || SEC.link[i].turret == turret_id)
			{
				return TRUE;
			}
		}
	}

	return !any;	// No links => all of them.
}

static Thing *SEC_ui_term(void)
{
	if (!SEC.term_thing)
	{
		return NULL;
	}

	Thing *p_term = TO_THING(SEC.term_thing);

	if (!SEC_is_security(p_term) || p_term->Genus.Bat->type != BAT_TYPE_TERMINAL)
	{
		return NULL;
	}

	return p_term;
}

static SLONG SEC_ui_term_id(void)
{
	Thing *p_term = SEC_ui_term();

	return (p_term) ? p_term->Genus.Bat->want_z : -1;
}

static void SEC_build_list(void)
{
	SLONG i;
	SLONG id = SEC_ui_term_id();

	SEC.list_upto = 0;

	switch(SEC.ui)
	{
		case SEC_UI_FILES:

			for (i = 0; i < SEC.file_upto && SEC.list_upto < SEC_MAX_LIST; i++)
			{
				if (SEC.file[i].term == id)
				{
					SEC.list[SEC.list_upto++] = i;
				}
			}

			break;

		case SEC_UI_CAMS:

			for (i = 0; i < SEC.cam_upto && SEC.list_upto < SEC_MAX_LIST; i++)
			{
				if (SEC.cam[i].term == id)
				{
					SEC.list[SEC.list_upto++] = i;
				}
			}

			break;

		case SEC_UI_TURRETS:
			{
				SLONG list = thing_class_head[CLASS_BAT];

				while(list && SEC.list_upto < SEC_MAX_LIST)
				{
					Thing *p_thing = TO_THING(list);

					list = p_thing->NextLink;

					if (p_thing->Genus.Bat->type == BAT_TYPE_TURRET && SEC_term_controls(id, p_thing->Genus.Bat->want_z))
					{
						SEC.list[SEC.list_upto++] = THING_NUMBER(p_thing);
					}
				}
			}

			break;
	}
}

static SLONG SEC_count(SLONG ui)
{
	SLONG old = SEC.ui;

	SEC.ui = ui;
	SEC_build_list();
	SEC.ui = old;

	return SEC.list_upto;
}

static void SEC_go(SLONG ui)
{
	SEC.ui     = ui;
	SEC.sel    = 0;
	SEC.scroll = 0;
	SEC.ui_ms  = 0;

	SEC_build_list();

	PolyPage::SetGreenScreen(ui == SEC_UI_CCTV || ui == SEC_UI_MANUAL);
}

//
// Breaks the file text into lines that fit the screen.
//

#define SEC_TEXT_WIDTH 500

static void SEC_wrap_file(SLONG file)
{
	SEC.line_upto = 0;

	if (!WITHIN(file, 0, SEC.file_upto - 1))
	{
		return;
	}

	CBYTE *p = SEC.file[file].text;

	while(*p && SEC.line_upto < SEC_MAX_LINES)
	{
		CBYTE *start      = p;
		CBYTE *last_space = NULL;
		SLONG  width      = 0;

		while(*p && *p != '\n')
		{
			width += FONT2D_GetLetterWidth(*p);

			if (*p == ' ')
			{
				last_space = p;
			}

			if (width > SEC_TEXT_WIDTH)
			{
				if (last_space && last_space > start)
				{
					p = last_space;
				}

				break;
			}

			p++;
		}

		SEC.line    [SEC.line_upto] = start;
		SEC.line_len[SEC.line_upto] = p - start;
		SEC.line_upto += 1;

		if (*p == '\n' || *p == ' ')
		{
			p++;
		}
	}
}

static void SEC_close_ui(SLONG back_to_idle)
{
	PolyPage::SetGreenScreen(false);

	if (SEC.person)
	{
		Thing *p_person = TO_THING(SEC.person);

		if (back_to_idle &&
			p_person->Class == CLASS_PERSON &&
			p_person->State == STATE_MOVEING &&
		   (p_person->SubState == SUB_STATE_SIMPLE_ANIM || p_person->SubState == SUB_STATE_SIMPLE_ANIM_OVER))
		{
			p_person->Genus.Person->Flags &= ~(FLAG_PERSON_NON_INT_M | FLAG_PERSON_NON_INT_C | FLAG_PERSON_NO_RETURN_TO_NORMAL);

			set_person_idle(p_person);
		}
	}

	SEC.ui         = SEC_UI_NONE;
	SEC.term_thing = NULL;
	SEC.person     = NULL;
}

SLONG SEC_ui_open(void)
{
	return SEC.ui != SEC_UI_NONE;
}

void SEC_escape(void)
{
	SEC.escape = TRUE;
}

// ---------------------------------------------------------------------------
// Using a terminal
// ---------------------------------------------------------------------------

#define SEC_USE_DIST	0xa0

static Thing *SEC_nearest_terminal(Thing *p_person)
{
	Thing *best = NULL;
	SLONG  best_dist = SEC_USE_DIST;

	SLONG list = thing_class_head[CLASS_BAT];

	while(list)
	{
		Thing *p_thing = TO_THING(list);

		list = p_thing->NextLink;

		if (p_thing->Genus.Bat->type != BAT_TYPE_TERMINAL)
		{
			continue;
		}

		if (abs((p_thing->WorldPos.Y - p_person->WorldPos.Y) >> 8) > 0x80)
		{
			continue;
		}

		SLONG dist = SEC_dist(p_thing, p_person);

		if (dist < best_dist)
		{
			best      = p_thing;
			best_dist = dist;
		}
	}

	return best;
}

static void SEC_start_using(Thing *p_person, Thing *p_term)
{
	//
	// Stand in front of the screen, facing it.
	//

	SLONG ta = (p_term->Draw.Tweened->Angle - SEC_yaw_offset()) & 2047;

	GameCoord pos;

	pos.X = p_term->WorldPos.X + ((-SIN(ta) * 50) >> 8);
	pos.Z = p_term->WorldPos.Z + ((-COS(ta) * 50) >> 8);
	pos.Y = p_person->WorldPos.Y;

	if (!there_is_a_los(
			p_person->WorldPos.X >> 8, (p_person->WorldPos.Y >> 8) + 0x40, p_person->WorldPos.Z >> 8,
			pos.X >> 8, (pos.Y >> 8) + 0x40, pos.Z >> 8,
			LOS_FLAG_IGNORE_SEETHROUGH_FENCE_FLAG | LOS_FLAG_IGNORE_PRIMS))
	{
		pos = p_person->WorldPos;	// Don't walk through a wall.
	}

	move_thing_on_map(p_person, &pos);

	p_person->Draw.Tweened->Angle = SEC_angle_to(
										pos.X >> 8, pos.Z >> 8,
										p_term->WorldPos.X >> 8, p_term->WorldPos.Z >> 8);

	set_person_do_a_simple_anim(p_person, ANIM_USE_COMPUTER);

	p_person->Genus.Person->Flags |= FLAG_PERSON_NO_RETURN_TO_NORMAL;

	SEC.term_thing = THING_NUMBER(p_term);
	SEC.person     = THING_NUMBER(p_person);
	SEC.ui         = SEC_UI_BOOT;
	SEC.ui_ms      = 0;
	SEC.escape     = FALSE;
	SEC.status[0]  = 0;

	p_term->Genus.Bat->flag |= SEC_F_TERM_HACKED;

	MFX_play_thing(THING_NUMBER(p_term), S_BEEP, 0, p_term);
}

#define SEC_IN_UP		(1 << 0)
#define SEC_IN_DOWN		(1 << 1)
#define SEC_IN_LEFT		(1 << 2)
#define SEC_IN_RIGHT	(1 << 3)
#define SEC_IN_OK		(1 << 4)
#define SEC_IN_BACK		(1 << 5)

static ULONG SEC_read_input(ULONG input)
{
	ULONG now = 0;

	if (input & INPUT_MASK_FORWARDS)  {now |= SEC_IN_UP;}
	if (input & INPUT_MASK_BACKWARDS) {now |= SEC_IN_DOWN;}
	if (input & INPUT_MASK_LEFT)      {now |= SEC_IN_LEFT;}
	if (input & INPUT_MASK_RIGHT)     {now |= SEC_IN_RIGHT;}
	if (input & (INPUT_MASK_ACTION | INPUT_MASK_PUNCH | INPUT_MASK_JUMP)) {now |= SEC_IN_OK;}
	if (input & (INPUT_MASK_KICK | INPUT_MASK_CANCEL))                    {now |= SEC_IN_BACK;}

	if (Keys[KB_ENTER]) {Keys[KB_ENTER] = 0; now |= SEC_IN_OK;}

	ULONG pressed = now & ~SEC.prev_input;

	SEC.prev_input = now;

	if (SEC.escape)
	{
		SEC.escape = FALSE;
		pressed   |= SEC_IN_BACK;
	}

	return pressed;
}

static void SEC_move_sel(ULONG pressed, SLONG count)
{
	if (count <= 0)
	{
		SEC.sel = 0;

		return;
	}

	if (pressed & SEC_IN_UP)   {SEC.sel -= 1; MFX_play_ambient(0, S_MENU_CLICK_START, 0);}
	if (pressed & SEC_IN_DOWN) {SEC.sel += 1; MFX_play_ambient(0, S_MENU_CLICK_START, 0);}

	if (SEC.sel < 0)      {SEC.sel = count - 1;}
	if (SEC.sel >= count) {SEC.sel = 0;}
}

//
// The menu of things you can do to a turret.
//

#define SEC_TA_POWER	0
#define SEC_TA_TARGET	1
#define SEC_TA_HATCH	2
#define SEC_TA_CAMERA	3
#define SEC_TA_MANUAL	4
#define SEC_TA_BACK		5
#define SEC_TA_NUMBER	6

static void SEC_manual_process(ULONG input, ULONG pressed, ULONG raw_pressed, SLONG ms);

static void SEC_ui_process(Thing *p_person, ULONG input)
{
	ULONG now = SEC_real_ms();
	SLONG ms  = now - SEC.last_tick;

	SEC.last_tick = now;

	if (ms < 0 || ms > 250) {ms = 0;}

	SEC.ui_ms += ms;

	if (SEC.status_ms > 0)
	{
		SEC.status_ms -= ms;
	}

	ULONG pressed = SEC_read_input(input);

	ULONG raw_pressed = input & ~SEC.raw_prev;	// (manual control wants the real buttons)

	SEC.raw_prev = input;

	Thing *p_term = SEC_ui_term();

	//
	// Interrupted?
	//

	if (p_term == NULL ||
		p_person->State != STATE_MOVEING ||
	   (p_person->SubState != SUB_STATE_SIMPLE_ANIM && p_person->SubState != SUB_STATE_SIMPLE_ANIM_OVER))
	{
		SEC_close_ui(FALSE);

		return;
	}

	if (p_term->Genus.Bat->glow && SEC.ui != SEC_UI_BOOT)
	{
		//
		// The power went (EMP).
		//

		if (pressed & (SEC_IN_OK | SEC_IN_BACK))
		{
			SEC_close_ui(TRUE);
		}

		return;
	}

	switch(SEC.ui)
	{
		case SEC_UI_BOOT:

			if (pressed & SEC_IN_BACK)
			{
				SEC_close_ui(TRUE);

				return;
			}

			if (p_person->SubState == SUB_STATE_SIMPLE_ANIM_OVER || SEC.ui_ms > 5000)
			{
				SEC_go(SEC_UI_MAIN);
			}

			break;

		case SEC_UI_MAIN:

			SEC_move_sel(pressed, 4);

			if (pressed & SEC_IN_BACK)
			{
				SEC_close_ui(TRUE);

				return;
			}

			if (pressed & SEC_IN_OK)
			{
				MFX_play_ambient(0, S_BEEP, 0);

				switch(SEC.sel)
				{
					case 0: SEC_go(SEC_UI_FILES);   break;
					case 1: SEC_go(SEC_UI_CAMS);    break;
					case 2: SEC_go(SEC_UI_TURRETS); break;
					case 3: SEC_close_ui(TRUE);     return;
				}
			}

			break;

		case SEC_UI_FILES:

			SEC_move_sel(pressed, SEC.list_upto);

			if (pressed & SEC_IN_BACK)
			{
				SEC_go(SEC_UI_MAIN);
				SEC.sel = 0;
			}
			else
			if ((pressed & SEC_IN_OK) && SEC.list_upto)
			{
				SLONG keep = SEC.sel;

				SEC.file_index = SEC.list[SEC.sel];
				SEC_wrap_file(SEC.file_index);
				SEC_go(SEC_UI_READ);
				SEC.list[0] = keep;	// Remember where we were in the list
			}

			break;

		case SEC_UI_READ:

			if (pressed & SEC_IN_UP)   {SEC.scroll -= 1;}
			if (pressed & SEC_IN_DOWN) {SEC.scroll += 1;}
			if (pressed & SEC_IN_LEFT) {SEC.scroll -= 10;}
			if (pressed & SEC_IN_RIGHT){SEC.scroll += 10;}

			SATURATE(SEC.scroll, 0, MAX(SEC.line_upto - 1, 0));

			if (pressed & (SEC_IN_BACK | SEC_IN_OK))
			{
				SLONG keep = SEC.list[0];

				SEC_go(SEC_UI_FILES);
				SEC.sel = keep;
			}

			break;

		case SEC_UI_CAMS:

			SEC_move_sel(pressed, SEC.list_upto);

			if (pressed & SEC_IN_BACK)
			{
				SEC_go(SEC_UI_MAIN);
				SEC.sel = 1;
			}
			else
			if ((pressed & SEC_IN_OK) && SEC.list_upto)
			{
				SEC.cam_index = SEC.sel;
				SEC_go(SEC_UI_CCTV);
				SEC.sel = SEC.cam_index;
			}

			break;

		case SEC_UI_CCTV:

			if (SEC.cam_index >= 0)
			{
				//
				// Watching one of the terminal's cameras: left / right for the next one.
				//

				SLONG count = SEC_count(SEC_UI_CAMS);

				SEC.ui = SEC_UI_CCTV;

				if (count)
				{
					if (pressed & SEC_IN_LEFT)  {SEC.cam_index = (SEC.cam_index + count - 1) % count; SEC.ui_ms = 0; MFX_play_ambient(0, S_MENU_CLICK_START, 0);}
					if (pressed & SEC_IN_RIGHT) {SEC.cam_index = (SEC.cam_index + 1) % count;         SEC.ui_ms = 0; MFX_play_ambient(0, S_MENU_CLICK_START, 0);}
				}

				if (pressed & (SEC_IN_BACK | SEC_IN_OK))
				{
					SLONG keep = SEC.cam_index;

					SEC_go(SEC_UI_CAMS);
					SEC.sel = keep;
				}
			}
			else
			{
				//
				// Looking through a turret's own camera.
				//

				if (pressed & (SEC_IN_BACK | SEC_IN_OK))
				{
					SEC_go(SEC_UI_TURRET);
					SEC.sel = SEC_TA_CAMERA;
				}
			}

			break;

		case SEC_UI_MANUAL:

			SEC_manual_process(input, pressed, raw_pressed, ms);

			break;

		case SEC_UI_TURRETS:

			SEC_move_sel(pressed, SEC.list_upto);

			if (pressed & SEC_IN_BACK)
			{
				SEC_go(SEC_UI_MAIN);
				SEC.sel = 2;
			}
			else
			if ((pressed & SEC_IN_OK) && SEC.list_upto)
			{
				SEC.turret_thing = SEC.list[SEC.sel];
				SEC_go(SEC_UI_TURRET);
			}

			break;

		case SEC_UI_TURRET:
			{
				Thing *p_turret = TO_THING(SEC.turret_thing);

				if (!SEC_is_security(p_turret))
				{
					SEC_go(SEC_UI_TURRETS);

					break;
				}

				Bat *p_bat = p_turret->Genus.Bat;

				SEC_move_sel(pressed, SEC_TA_NUMBER);

				if (pressed & SEC_IN_BACK)
				{
					SEC_go(SEC_UI_TURRETS);

					break;
				}

				if (pressed & SEC_IN_OK)
				{
					if (p_bat->state == SEC_TS_DEAD && SEC.sel != SEC_TA_BACK)
					{
						SEC_set_status("No response - the turret is destroyed.");
						MFX_play_ambient(0, S_PISTOL_DRY, 0);

						break;
					}

					MFX_play_ambient(0, S_BEEP, 0);

					switch(SEC.sel)
					{
						case SEC_TA_POWER:

							p_bat->flag ^= SEC_F_POWEROFF;

							SEC_set_status((p_bat->flag & SEC_F_POWEROFF) ? "Power cut. Turret offline." : "Power restored. Turret online.");

							break;

						case SEC_TA_TARGET:

							p_bat->flag  ^= SEC_F_FRIENDLY;
							p_bat->target = NULL;

							SEC_set_status((p_bat->flag & SEC_F_FRIENDLY) ? "Reprogrammed: targeting hostiles only." : "Reprogrammed: targeting intruders.");

							break;

						case SEC_TA_HATCH:

							if (!(p_bat->flag & SEC_F_HAS_HATCH))
							{
								SEC_set_status("This unit has no retractable mount.");
							}
							else
							{
								p_bat->flag ^= SEC_F_CONCEALED;

								if (p_bat->flag & SEC_F_CONCEALED)
								{
									p_bat->dz = SEC_RETRACT_MS;	// Go down now if there is nothing to shoot
								}

								SEC_set_status((p_bat->flag & SEC_F_CONCEALED) ? "Mount set to concealed." : "Mount set to always deployed.");
							}

							break;

						case SEC_TA_CAMERA:

							SEC.cam_index = -1;
							SEC_go(SEC_UI_CCTV);

							break;

						case SEC_TA_MANUAL:

							if (p_bat->glow || (p_bat->flag & SEC_F_POWEROFF))
							{
								SEC_set_status("The turret is offline - restore its power first.");
								MFX_play_ambient(0, S_PISTOL_DRY, 0);

								break;
							}

							SEC.man_pitch   = SEC_MAN_PITCH_START;
							SEC.man_lock    = NULL;
							SEC.man_fire_ms = 300;		// Not straight away (the button that chose this)
							SEC.man_shots   = 0;
							SEC.man_hits    = 0;
							SEC.cam_index   = -1;
							SEC.raw_prev    = input;

							SEC_go(SEC_UI_MANUAL);

							break;

						case SEC_TA_BACK:

							SEC_go(SEC_UI_TURRETS);

							break;
					}
				}
			}

			break;
	}
}

//
// Manual control of SEC.turret_thing: called from SEC_ui_process.
//

static void SEC_manual_process(ULONG input, ULONG pressed, ULONG raw_pressed, SLONG ms)
{
	Thing *p_turret = TO_THING(SEC.turret_thing);

	if (!SEC_is_security(p_turret) || p_turret->Genus.Bat->type != BAT_TYPE_TURRET)
	{
		SEC_go(SEC_UI_TURRETS);

		return;
	}

	Bat       *p_bat = p_turret->Genus.Bat;
	DrawTween *dt    = p_turret->Draw.Tweened;

	if (pressed & SEC_IN_BACK)
	{
		p_bat->target = NULL;
		SEC.man_lock  = NULL;

		SEC_go(SEC_UI_TURRET);
		SEC.sel = SEC_TA_MANUAL;

		return;
	}

	if (p_bat->state == SEC_TS_DEAD || p_bat->glow || (p_bat->flag & SEC_F_POWEROFF))
	{
		return;		// "NO SIGNAL" on the screen; kick / Esc to go back
	}

	if (SEC.man_fire_ms > 0)
	{
		SEC.man_fire_ms -= ms;
	}

	//
	// Turning: left / right (like a person: left = the angle goes up). Touching
	// them lets go of the lock.
	//

	SLONG turn  = (SEC_TURN_SPEED / 2) * ms / 1000 + 1;
	SLONG angle = dt->Angle;

	if (input & (INPUT_MASK_LEFT | INPUT_MASK_RIGHT))
	{
		SEC.man_lock = NULL;

		if (input & INPUT_MASK_LEFT)  {angle += turn;}
		if (input & INPUT_MASK_RIGHT) {angle -= turn;}
	}

	//
	// The barrels: forwards = up.
	//

	SLONG tilt = 260 * ms / 1000 + 1;

	if (input & INPUT_MASK_FORWARDS)  {SEC.man_pitch += tilt; SEC.man_lock = NULL;}
	if (input & INPUT_MASK_BACKWARDS) {SEC.man_pitch -= tilt; SEC.man_lock = NULL;}

	//
	// Jump: lock on to the next person in view. Locked, it follows him.
	//

	if (raw_pressed & INPUT_MASK_JUMP)
	{
		SEC_man_next_target(p_turret);
	}

	if (SEC.man_lock)
	{
		Thing *p_lock = TO_THING(SEC.man_lock);

		if (!SEC_man_is_target(p_turret, p_lock))
		{
			SEC.man_lock = NULL;

			SEC_set_status("Target lost.");
		}
		else
		{
			SLONG want = (SEC_angle_to(
							p_turret->WorldPos.X >> 8, p_turret->WorldPos.Z >> 8,
							p_lock->WorldPos.X >> 8, p_lock->WorldPos.Z >> 8) + SEC_yaw_offset()) & 2047;

			SLONG diff = SEC_angle_diff(angle, want);
			SLONG step = SEC_TURN_SPEED * ms / 1000 + 1;

			SATURATE(diff, -step, step);

			angle += diff;

			//
			// Up / down to his chest.
			//

			SLONG dy   = ((p_lock->WorldPos.Y >> 8) + 0x50) - SEC_turret_head_y(p_turret);
			SLONG dist = SEC_dist(p_turret, p_lock);
			SLONG wp   = Arctan(dy, MAX(dist, 1)) & 2047;

			if (wp > 1024) {wp -= 2048;}

			SLONG pd = wp - SEC.man_pitch;

			SATURATE(pd, -step, step);

			SEC.man_pitch += pd;
		}
	}

	SATURATE(SEC.man_pitch, SEC_MAN_PITCH_MIN, SEC_MAN_PITCH_MAX);

	//
	// It can only turn so far.
	//

	if (p_bat->substate)
	{
		SLONG lim = p_bat->substate << 3;
		SLONG off = SEC_angle_diff(p_bat->dy, angle & 2047);

		SATURATE(off, -lim, lim);

		angle = p_bat->dy + off;
	}

	dt->Angle = angle & 2047;

	//
	// Punch / action (held: it keeps firing at its rate).
	//

	if ((input & (INPUT_MASK_PUNCH | INPUT_MASK_ACTION)) && SEC.man_fire_ms <= 0)
	{
		if (p_bat->state == SEC_TS_UP)
		{
			SEC_man_fire(p_turret);

			SEC.man_fire_ms = ConfigExtras::getInstance().mTurretFireMs;
		}
	}
}

SLONG SEC_player_control(Thing *p_person, ULONG input)
{
	if (SEC.need_spawn)
	{
		SEC.need_spawn = FALSE;

		SEC_spawn_level();
	}

	if (SEC.ui != SEC_UI_NONE)
	{
		if (SEC.person != THING_NUMBER(p_person))
		{
			return FALSE;	// The other player in a network game is using it.
		}

		SEC_ui_process(p_person, input);

		return TRUE;
	}

	SEC.near_term = NULL;

	if (p_person->Genus.Person->Flags & (FLAG_PERSON_DRIVING | FLAG_PERSON_PASSENGER | FLAG_PERSON_BIKING))
	{
		SEC.prev_input = 0;

		return FALSE;
	}

	if (p_person->State != STATE_IDLE && p_person->State != STATE_MOVEING)
	{
		SEC.prev_input = 0;

		return FALSE;
	}

	Thing *p_term = SEC_nearest_terminal(p_person);

	ULONG action = input & INPUT_MASK_ACTION;
	ULONG new_action = action && !(SEC.prev_input & SEC_IN_OK);

	SEC.prev_input = (action) ? SEC_IN_OK : 0;

	if (p_term == NULL)
	{
		return FALSE;
	}

	SEC.near_term = THING_NUMBER(p_term);

	if (new_action && p_person->SubState != SUB_STATE_SIMPLE_ANIM)
	{
		if (p_term->Genus.Bat->glow)
		{
			CONSOLE_text("The computer has no power", 2000);

			return TRUE;
		}

		SEC.last_tick = SEC_real_ms();

		SEC_start_using(p_person, p_term);

		return TRUE;
	}

	return FALSE;
}

// ---------------------------------------------------------------------------
// CCTV
// ---------------------------------------------------------------------------

SLONG SEC_camera(SLONG *x, SLONG *y, SLONG *z, SLONG *look_x, SLONG *look_y, SLONG *look_z)
{
	if (SEC.ui != SEC_UI_CCTV && SEC.ui != SEC_UI_MANUAL)
	{
		return FALSE;
	}

	SLONG cx, cy, cz, yaw, pitch;

	if (SEC.ui == SEC_UI_CCTV && SEC.cam_index >= 0)
	{
		SLONG old = SEC.ui;

		SEC.ui = SEC_UI_CAMS;
		SEC_build_list();
		SEC.ui = old;

		if (!WITHIN(SEC.cam_index, 0, SEC.list_upto - 1))
		{
			return FALSE;
		}

		SEC_Cam *sc = &SEC.cam[SEC.list[SEC.cam_index]];

		cx    = sc->x;
		cz    = sc->z;
		cy    = PAP_calc_map_height_at(cx, cz) + sc->height;
		pitch = sc->pitch;

		//
		// Real security cameras pan slowly from side to side.
		//

		yaw = sc->yaw + (SIN((GAME_TURN * 3) & 2047) * 140 >> 16);
	}
	else
	{
		//
		// The turret's sensor.
		//

		Thing *p_turret = TO_THING(SEC.turret_thing);

		if (!SEC_is_security(p_turret))
		{
			return FALSE;
		}

		cx    = p_turret->WorldPos.X >> 8;
		cz    = p_turret->WorldPos.Z >> 8;
		cy    = SEC_turret_head_y(p_turret) + 30;
		yaw   = (p_turret->Draw.Tweened->Angle - SEC_yaw_offset()) & 2047;
		pitch = (SEC.ui == SEC_UI_MANUAL) ? (SEC.man_pitch & 2047) : 2000;

		//
		// Start a little in front of the turret so we don't look at its own barrels.
		//

		cx += -SIN(yaw) * 70 >> 16;
		cz += -COS(yaw) * 70 >> 16;
	}

	yaw &= 2047;

	//
	// Pitch: 0 = level, a bit less than 2048 = looking down.
	//

	SLONG p    = SEC_angle_diff(0, pitch & 2047);
	SLONG dist = 0x400;
	SLONG fx   = -SIN(yaw) * dist >> 16;
	SLONG fz   = -COS(yaw) * dist >> 16;
	SLONG fy   = SIN(p & 2047) * dist >> 16;

	*x      = cx;
	*y      = cy;
	*z      = cz;
	*look_x = cx + fx;
	*look_y = cy + fy;
	*look_z = cz + fz;

	return TRUE;
}

// ---------------------------------------------------------------------------
// Drawing the screen
// ---------------------------------------------------------------------------

#define SEC_COL_TEXT	0x90ffb0
#define SEC_COL_DIM		0x70d890
#define SEC_COL_HEAD	0xffffff
#define SEC_COL_WARN	0xff8060
#define SEC_COL_OK		0xa0ff70

//
// The screen is drawn twice: first all the boxes (pass 0), then all the
// text (pass 1) in a frame of its own, so no panel can end up on top of the
// writing.
//

static SLONG SEC_pass;

#define SEC_PX1	56
#define SEC_PY1	48
#define SEC_PX2	584
#define SEC_PY2	432

static void SEC_box(SLONG x1, SLONG y1, SLONG x2, SLONG y2, ULONG argb)
{
	if (SEC_pass != 0) return;

	PANEL_draw_quad(float(x1), float(y1), float(x2), float(y2), POLY_PAGE_ALPHA_OVERLAY, argb);
}

static void SEC_frame(SLONG x1, SLONG y1, SLONG x2, SLONG y2, ULONG argb)
{
	SEC_box(x1, y1, x2, y1 + 2, argb);
	SEC_box(x1, y2 - 2, x2, y2, argb);
	SEC_box(x1, y1, x1 + 2, y2, argb);
	SEC_box(x2 - 2, y1, x2, y2, argb);
}

static void SEC_text(CBYTE *str, SLONG x, SLONG y, ULONG rgb)
{
	if (SEC_pass != 1) return;

	FONT2D_DrawString(str, x, y, rgb, 256, POLY_PAGE_FONT2D, 0);
}

static void SEC_scanlines(SLONG x1, SLONG y1, SLONG x2, SLONG y2, ULONG argb)
{
	SLONG y;

	for (y = y1; y < y2; y += 4)
	{
		SEC_box(x1, y, x2, y + 1, argb);
	}
}

static void SEC_menu_item(SLONG index, SLONG y, CBYTE *str, ULONG rgb)
{
	if (SEC.sel == index)
	{
		SEC_box(SEC_PX1 + 16, y - 4, SEC_PX2 - 16, y + 15, 0xc0208a48);

		SEC_text(">", SEC_PX1 + 22, y, SEC_COL_HEAD);
		SEC_text(str, SEC_PX1 + 40, y, SEC_COL_HEAD);
	}
	else
	{
		SEC_text(str, SEC_PX1 + 40, y, rgb);
	}
}

static void SEC_turret_status(Thing *p_turret, CBYTE *out)
{
	Bat *p_bat = p_turret->Genus.Bat;

	const CBYTE *power;
	const CBYTE *mode;
	const CBYTE *mount;

	if (p_bat->state == SEC_TS_DEAD)
	{
		sprintf(out, "T-%02d   DESTROYED", p_bat->want_z);

		return;
	}

	if (p_bat->glow)                     {power = "NO POWER (EMP)";}
	else if (p_bat->flag & SEC_F_POWEROFF) {power = "OFFLINE";}
	else                                 {power = "ONLINE";}

	mode  = (p_bat->flag & SEC_F_FRIENDLY) ? "HOSTILES" : "INTRUDERS";

	switch(p_bat->state)
	{
		case SEC_TS_HIDDEN:   mount = "RETRACTED"; break;
		case SEC_TS_RISING:   mount = "DEPLOYING"; break;
		case SEC_TS_LOWERING: mount = "RETRACTING"; break;
		default:              mount = "DEPLOYED";  break;
	}

	if (SEC.ui == SEC_UI_MANUAL && SEC.turret_thing == THING_NUMBER(p_turret))
	{
		mode = "MANUAL";
	}

	sprintf(out, "T-%02d   %-14s %-10s %s%s", p_bat->want_z, power, mode, mount, (p_bat->target) ? "  *FIRING*" : "");
}

static void SEC_draw_terminal(void)
{
	SLONG  i;
	SLONG  y;
	CBYTE  str[160];

	Thing    *p_term = SEC_ui_term();
	SLONG     id     = SEC_ui_term_id();
	SEC_Term *st     = SEC_term_info(id);

	//
	// The screen.
	//

	SEC_box(0, 0, 640, 480, 0x30000000);
	SEC_box(SEC_PX1, SEC_PY1, SEC_PX2, SEC_PY2, 0xd8103c24);
	SEC_scanlines(SEC_PX1, SEC_PY1, SEC_PX2, SEC_PY2, 0x14000000);
	SEC_frame(SEC_PX1, SEC_PY1, SEC_PX2, SEC_PY2, 0xff60ff90);
	SEC_frame(SEC_PX1 + 6, SEC_PY1 + 6, SEC_PX2 - 6, SEC_PY2 - 6, 0x9050e080);

	sprintf(str, "SECURENET TERMINAL  //  %s", (st && st->name[0]) ? st->name : "WORKSTATION");
	SEC_text(str, SEC_PX1 + 20, SEC_PY1 + 16, SEC_COL_HEAD);

	sprintf(str, "NODE %03d", (id >= 0) ? id : 0);
	SEC_text(str, SEC_PX2 - 110, SEC_PY1 + 16, SEC_COL_DIM);

	SEC_box(SEC_PX1 + 16, SEC_PY1 + 38, SEC_PX2 - 16, SEC_PY1 + 40, 0xe060ff90);

	y = SEC_PY1 + 56;

	if (p_term && p_term->Genus.Bat->glow && SEC.ui != SEC_UI_BOOT)
	{
		SEC_text("*** POWER FAILURE ***", SEC_PX1 + 160, y + 100, SEC_COL_WARN);
		SEC_text("Electromagnetic pulse detected. System halted.", SEC_PX1 + 60, y + 130, SEC_COL_TEXT);
		SEC_text("[ACTION] Log off", SEC_PX1 + 190, y + 180, SEC_COL_DIM);
	}
	else
	switch(SEC.ui)
	{
		case SEC_UI_BOOT:
			{
				static CBYTE *boot[] =
				{
					"SecureNet BIOS v2.14  (c) 1999 Mallory Systems",
					"Memory test ............ 65536K OK",
					"Mounting /dev/sec0 ..... OK",
					"Loading kernel ......... OK",
					"Bypassing login ........ OK",
					"Welcome, ADMINISTRATOR."
				};

				SLONG lines = SEC.ui_ms / 450 + 1;

				if (lines > 6) {lines = 6;}

				for (i = 0; i < lines; i++)
				{
					SEC_text(boot[i], SEC_PX1 + 24, y + i * 22, (i == lines - 1) ? SEC_COL_HEAD : SEC_COL_TEXT);
				}

				if ((SEC_real_ms() / 300) & 1)
				{
					SEC_box(SEC_PX1 + 24, y + lines * 22, SEC_PX1 + 34, y + lines * 22 + 14, 0xff50ff80);
				}
			}
			break;

		case SEC_UI_MAIN:

			SEC_text("MAIN MENU", SEC_PX1 + 24, y, SEC_COL_DIM);

			y += 34;

			sprintf(str, "Mail and documents  (%d)", SEC_count(SEC_UI_FILES));
			SEC_menu_item(0, y, str, SEC_COL_TEXT); y += 28;

			sprintf(str, "CCTV cameras  (%d)", SEC_count(SEC_UI_CAMS));
			SEC_menu_item(1, y, str, SEC_COL_TEXT); y += 28;

			sprintf(str, "Security turrets  (%d)", SEC_count(SEC_UI_TURRETS));
			SEC_menu_item(2, y, str, SEC_COL_TEXT); y += 28;

			SEC_menu_item(3, y, "Log off", SEC_COL_TEXT);

			break;

		case SEC_UI_FILES:

			SEC_text("MAIL AND DOCUMENTS", SEC_PX1 + 24, y, SEC_COL_DIM);

			y += 34;

			if (SEC.list_upto == 0)
			{
				SEC_text("No documents.", SEC_PX1 + 40, y, SEC_COL_TEXT);
			}

			for (i = 0; i < SEC.list_upto; i++)
			{
				SEC_menu_item(i, y, SEC.file[SEC.list[i]].title, SEC_COL_TEXT);

				y += 24;
			}

			break;

		case SEC_UI_READ:

			if (WITHIN(SEC.file_index, 0, SEC.file_upto - 1))
			{
				SEC_text(SEC.file[SEC.file_index].title, SEC_PX1 + 24, y, SEC_COL_HEAD);

				y += 30;

				for (i = SEC.scroll; i < SEC.line_upto && y < SEC_PY2 - 50; i++)
				{
					SLONG len = SEC.line_len[i];

					if (len > SLONG(sizeof(str)) - 1)
					{
						len = sizeof(str) - 1;
					}

					memcpy(str, SEC.line[i], len);
					str[len] = 0;

					SEC_text(str, SEC_PX1 + 30, y, SEC_COL_TEXT);

					y += 19;
				}

				if (SEC.line_upto > 0)
				{
					sprintf(str, "%d / %d", MIN(SEC.scroll + 1, SEC.line_upto), SEC.line_upto);
					SEC_text(str, SEC_PX2 - 90, SEC_PY1 + 56, SEC_COL_DIM);
				}
			}

			break;

		case SEC_UI_CAMS:

			SEC_text("CCTV CAMERAS", SEC_PX1 + 24, y, SEC_COL_DIM);

			y += 34;

			if (SEC.list_upto == 0)
			{
				SEC_text("No cameras on this node.", SEC_PX1 + 40, y, SEC_COL_TEXT);
			}

			for (i = 0; i < SEC.list_upto; i++)
			{
				sprintf(str, "CAM %02d   %s", i + 1, SEC.cam[SEC.list[i]].name);
				SEC_menu_item(i, y, str, SEC_COL_TEXT);

				y += 24;
			}

			break;

		case SEC_UI_TURRETS:

			SEC_text("SECURITY TURRETS", SEC_PX1 + 24, y, SEC_COL_DIM);
			SEC_text("UNIT   POWER          TARGET     MOUNT", SEC_PX1 + 40, y + 22, SEC_COL_DIM);

			y += 48;

			if (SEC.list_upto == 0)
			{
				SEC_text("No turrets on this network.", SEC_PX1 + 40, y, SEC_COL_TEXT);
			}

			for (i = 0; i < SEC.list_upto; i++)
			{
				Thing *p_turret = TO_THING(SEC.list[i]);

				SEC_turret_status(p_turret, str);

				SEC_menu_item(i, y, str, (p_turret->Genus.Bat->state == SEC_TS_DEAD) ? SEC_COL_WARN : SEC_COL_TEXT);

				y += 24;
			}

			break;

		case SEC_UI_TURRET:
			{
				Thing *p_turret = TO_THING(SEC.turret_thing);

				if (!SEC_is_security(p_turret))
				{
					break;
				}

				Bat *p_bat = p_turret->Genus.Bat;

				SEC_turret_status(p_turret, str);
				SEC_text(str, SEC_PX1 + 24, y, SEC_COL_HEAD);

				sprintf(str, "Integrity %d%%    Range %d m",
					p_bat->health * 100 / SEC_TURRET_HP,
					SEC_turret_range(p_bat) * 3 >> 8);
				SEC_text(str, SEC_PX1 + 24, y + 22, SEC_COL_DIM);

				y += 60;

				sprintf(str, "Power:      %s", (p_bat->flag & SEC_F_POWEROFF) ? "OFF  (restore power)" : "ON   (cut power)");
				SEC_menu_item(SEC_TA_POWER, y, str, SEC_COL_TEXT); y += 28;

				sprintf(str, "Targeting:  %s", (p_bat->flag & SEC_F_FRIENDLY) ? "HOSTILES ONLY  (switch to intruders)" : "INTRUDERS  (switch to hostiles only)");
				SEC_menu_item(SEC_TA_TARGET, y, str, SEC_COL_TEXT); y += 28;

				if (p_bat->flag & SEC_F_HAS_HATCH)
				{
					sprintf(str, "Mount:      %s", (p_bat->flag & SEC_F_CONCEALED) ? "CONCEALED  (keep deployed)" : "ALWAYS DEPLOYED  (conceal)");
				}
				else
				{
					sprintf(str, "Mount:      FIXED");
				}
				SEC_menu_item(SEC_TA_HATCH, y, str, (p_bat->flag & SEC_F_HAS_HATCH) ? SEC_COL_TEXT : SEC_COL_DIM); y += 28;

				SEC_menu_item(SEC_TA_CAMERA, y, "View sensor feed", SEC_COL_TEXT); y += 28;
				SEC_menu_item(SEC_TA_MANUAL, y, "Manual control  (aim and fire it yourself)",
					(p_bat->glow || (p_bat->flag & SEC_F_POWEROFF) || p_bat->state == SEC_TS_DEAD) ? SEC_COL_DIM : SEC_COL_TEXT); y += 28;
				SEC_menu_item(SEC_TA_BACK,   y, "Back", SEC_COL_TEXT);
			}

			break;
	}

	if (SEC.status_ms > 0 && SEC.status[0])
	{
		SEC_text(SEC.status, SEC_PX1 + 24, SEC_PY2 - 58, SEC_COL_OK);
	}

	//
	// What the keys do.
	//

	if (SEC.ui == SEC_UI_READ)
	{
		SEC_text("[UP/DOWN] scroll   [LEFT/RIGHT] page   [ACTION/KICK] back", SEC_PX1 + 24, SEC_PY2 - 30, SEC_COL_DIM);
	}
	else
	if (SEC.ui != SEC_UI_BOOT)
	{
		SEC_text("[UP/DOWN] select   [ACTION/ENTER] open   [KICK/ESC] back", SEC_PX1 + 24, SEC_PY2 - 30, SEC_COL_DIM);
	}

}

static void SEC_draw_cctv(void)
{
	CBYTE str[96];

	//
	// Black bars, scanlines, noise and the camera's caption.
	//

	SEC_scanlines(0, 0, 640, 480, 0x20000000);

	SEC_box(0, 0, 640, 30, 0xb0000000);
	SEC_box(0, 450, 640, 480, 0xb0000000);

	if (SEC.ui_ms < 250)
	{
		//
		// Static when switching cameras.
		//

		SLONG i;

		for (i = 0; i < 60; i++)
		{
			SLONG x = Random() % 640;
			SLONG y = Random() % 480;

			SEC_box(x, y, x + 2 + (Random() & 31), y + 2, 0xa0ffffff);
		}

		SEC_box(0, 0, 640, 480, (0xd0 - SEC.ui_ms * 0xd0 / 250) << 24 | 0x101010);
	}

	if (SEC.ui == SEC_UI_MANUAL)
	{
		Thing *p_turret = TO_THING(SEC.turret_thing);

		if (!SEC_is_security(p_turret))
		{
			return;
		}

		Bat *p_bat = p_turret->Genus.Bat;

		SEC_text("[ARROWS] aim  [PUNCH/ACTION] fire  [JUMP] next target  [KICK] back", 20, 458, 0xa0a0a0);

		if (p_bat->state == SEC_TS_DEAD || p_bat->glow || (p_bat->flag & SEC_F_POWEROFF))
		{
			SEC_box(0, 30, 640, 450, 0xe0101010);
			SEC_text((CBYTE *) ((p_bat->state == SEC_TS_DEAD) ? "NO SIGNAL - TURRET DESTROYED" : "NO SIGNAL - TURRET OFFLINE"), 200, 230, 0xff8060);

			sprintf(str, "TURRET T-%02d  MANUAL CONTROL", p_bat->want_z);
			SEC_text(str, 20, 8, 0xffffff);

			return;
		}

		//
		// Crosshair: where the barrels point (the view is the turret's).
		//

		ULONG ch = (SEC.man_lock) ? 0xe0ff3030 : 0xc0ff4040;

		SEC_box(300, 239, 314, 241, ch);
		SEC_box(326, 239, 340, 241, ch);
		SEC_box(319, 220, 321, 234, ch);
		SEC_box(319, 246, 321, 260, ch);
		SEC_box(319, 239, 321, 241, ch);

		if (SEC.man_lock)
		{
			//
			// Locked on: brackets round the crosshair.
			//

			SEC_box(290, 225, 300, 227, ch); SEC_box(290, 225, 292, 235, ch);
			SEC_box(340, 225, 350, 227, ch); SEC_box(348, 225, 350, 235, ch);
			SEC_box(290, 253, 300, 255, ch); SEC_box(290, 245, 292, 255, ch);
			SEC_box(340, 253, 350, 255, ch); SEC_box(348, 245, 350, 255, ch);

			Thing *p_lock = TO_THING(SEC.man_lock);

			sprintf(str, "LOCKED  %s  %d m", SEC_person_is_hostile(p_lock) ? "HOSTILE" : "UNKNOWN", SEC_dist(p_turret, p_lock) * 3 >> 8);
			SEC_text(str, 380, 236, SEC_person_is_hostile(p_lock) ? 0xff6060 : 0xffd060);
		}

		//
		// Elevation and how far round it has turned (of what it can).
		//

		sprintf(str, "ELEV %+d", SEC.man_pitch * 360 / 2048);
		SEC_text(str, 20, 400, 0xffffff);

		if (p_bat->substate)
		{
			SLONG lim = p_bat->substate << 3;
			SLONG off = SEC_angle_diff(p_bat->dy, p_turret->Draw.Tweened->Angle);
			SLONG x   = 320 - off * 120 / MAX(lim, 1);

			SEC_box(200, 40, 440, 42, 0x80ffffff);
			SEC_box(x - 2, 36, x + 2, 46, 0xffff4040);
		}

		sprintf(str, "ROUNDS %d  HITS %d  INTEGRITY %d%%", SEC.man_shots, SEC.man_hits, p_bat->health * 100 / SEC_TURRET_HP);
		SEC_text(str, 20, 420, 0xffffff);

		if (SEC.status_ms > 0 && SEC.status[0])
		{
			SEC_text(SEC.status, 20, 380, 0xa0ff70);
		}

		sprintf(str, "TURRET T-%02d  MANUAL CONTROL", p_bat->want_z);	// The caption (below)
	}
	else
	if (SEC.cam_index >= 0)
	{
		SLONG count = SEC_count(SEC_UI_CAMS);

		if (WITHIN(SEC.cam_index, 0, count - 1))
		{
			SLONG old = SEC.ui;

			SEC.ui = SEC_UI_CAMS;
			SEC_build_list();
			SEC.ui = old;

			sprintf(str, "CAM %02d  %s", SEC.cam_index + 1, SEC.cam[SEC.list[SEC.cam_index]].name);
		}
		else
		{
			sprintf(str, "NO SIGNAL");
		}

		SEC_text("[LEFT/RIGHT] switch camera   [ACTION/KICK] back", 20, 458, 0xa0a0a0);
	}
	else
	{
		Thing *p_turret = TO_THING(SEC.turret_thing);

		sprintf(str, "TURRET T-%02d  SENSOR", SEC_is_security(p_turret) ? p_turret->Genus.Bat->want_z : 0);

		SEC_text("[ACTION/KICK] back", 20, 458, 0xa0a0a0);

		//
		// Crosshair.
		//

		SEC_box(316, 228, 324, 230, 0xc0ff4040);
		SEC_box(316, 250, 324, 252, 0xc0ff4040);
		SEC_box(306, 230, 308, 250, 0xc0ff4040);
		SEC_box(332, 230, 334, 250, 0xc0ff4040);
	}

	SEC_text(str, 20, 8, 0xffffff);

	if ((SEC_real_ms() / 600) & 1)
	{
		SEC_box(560, 10, 572, 22, 0xffff2020);
	}

	SEC_text("REC", 580, 8, 0xffffff);

	{
		ULONG t = SEC_real_ms() / 1000;

		sprintf(str, "%02d:%02d:%02d", (t / 3600) % 24, (t / 60) % 60, t % 60);
		SEC_text(str, 520, 458, 0xffffff);
	}
}

void SEC_draw(void)
{
	if (SEC.ui == SEC_UI_NONE)
	{
		if (SEC.near_term)
		{
			Thing *p_term = TO_THING(SEC.near_term);

			if (SEC_is_security(p_term))
			{
#ifndef TARGET_DC
				for (SEC_pass = 0; SEC_pass < 2; SEC_pass++) {
				POLY_frame_init(FALSE, FALSE);
#endif
				SEC_box(210, 392, 430, 418, 0xc0103c24);
				SEC_frame(210, 392, 430, 418, 0xc060ff90);

				if (p_term->Genus.Bat->glow)
				{
					SEC_text("The computer has no power", 236, 398, 0xff8060);
				}
				else
				{
					SEC_text("[ACTION] Use computer", 250, 398, 0xa0ffb0);
				}
#ifndef TARGET_DC
				POLY_frame_draw(FALSE, FALSE);
				}
				SEC_pass = 0;
#endif
			}
		}

		return;
	}

	for (SEC_pass = 0; SEC_pass < 2; SEC_pass++)
	{
#ifndef TARGET_DC
		POLY_frame_init(FALSE, FALSE);
#endif

		if (SEC.ui == SEC_UI_CCTV || SEC.ui == SEC_UI_MANUAL)
		{
			SEC_draw_cctv();
		}
		else
		{
			SEC_draw_terminal();
		}

#ifndef TARGET_DC
		POLY_frame_draw(FALSE, FALSE);
#endif
	}

	SEC_pass = 0;
}

// ---------------------------------------------------------------------------
// Console commands
// ---------------------------------------------------------------------------

static void SEC_in_front(Thing *p_person, SLONG dist, SLONG *x, SLONG *z)
{
	SLONG a = p_person->Draw.Tweened->Angle;

	*x = (p_person->WorldPos.X >> 8) + (-SIN(a) * dist >> 16);
	*z = (p_person->WorldPos.Z >> 8) + (-COS(a) * dist >> 16);
}

void SEC_cmd_sentry(CBYTE *args)
{
	Thing *darci = NET_PERSON(0);

	if (darci == NULL)
	{
		return;
	}

	UWORD flag = 0;
	CBYTE word[32];
	CBYTE *p = args;

	while(SEC_token(&p, word, sizeof(word)))
	{
		if (stricmp(word, "hidden")   == 0) {flag |= SEC_F_CONCEALED;}
		if (stricmp(word, "friendly") == 0) {flag |= SEC_F_FRIENDLY;}
		if (stricmp(word, "off")      == 0) {flag |= SEC_F_POWEROFF;}
	}

	SLONG x, z;

	SEC_in_front(darci, 0x180, &x, &z);

	SLONG id = SEC_next_free_id(BAT_TYPE_TURRET);

	Thing *p_turret = SEC_create(
						BAT_TYPE_TURRET,
						id,
						x, darci->WorldPos.Y >> 8, z,		// At your height (roofs, floors of buildings)
						(darci->Draw.Tweened->Angle + 1024) & 2047,	// Facing you
						flag,
						0, 0);

	if (p_turret)
	{
		if (!(flag & SEC_F_FRIENDLY))
		{
			p_turret->Genus.Bat->glow = 3000;	// Three seconds to get out of the way.
		}

		CBYTE msg[96];

		sprintf(msg, "Turret T-%02d: %s, %s%s", id,
			(flag & SEC_F_FRIENDLY)  ? "friendly" : "hostile (boots up in 3 s)",
			(flag & SEC_F_CONCEALED) ? "hidden" : "exposed",
			(flag & SEC_F_POWEROFF)  ? ", switched off" : "");

		CONSOLE_text(msg, 4000);
	}
}

void SEC_cmd_terminal(CBYTE *args)
{
	Thing *darci = NET_PERSON(0);

	if (darci == NULL)
	{
		return;
	}

	SLONG x, z;

	SEC_in_front(darci, 0x80, &x, &z);

	SLONG id = SEC_next_free_id(BAT_TYPE_TERMINAL);

	Thing *p_term = SEC_create(
						BAT_TYPE_TERMINAL,
						id,
						x, darci->WorldPos.Y >> 8, z,
						(darci->Draw.Tweened->Angle + 1024) & 2047,
						0, 0, 0);

	if (p_term == NULL)
	{
		return;
	}

	SEC_add_term_name(id, "Field Office");

	//
	// Something to read...
	//

	SEC_add_file(id, "RE: Thursday delivery",
		"From: M. Harlan <m.harlan@mallorysys.net>\n"
		"To: Warehouse Supervisor\n"
		"\n"
		"The second container comes in on Thursday night. Same as last time:\n"
		"no paperwork, no police, and keep the sentries on until the truck\n"
		"is gone. If anyone asks, it's medical supplies.\n"
		"\n"
		"And for God's sake change the admin password. 'password1' is not\n"
		"a password.\n"
		"\n"
		"- M.");

	SEC_add_file(id, "Security memo 07/12",
		"ALL STAFF\n"
		"\n"
		"The new SecureNet sentry turrets are now online in the yard and on\n"
		"the loading bay. They will fire on ANYONE not carrying a staff tag.\n"
		"\n"
		"Maintenance can take a unit offline from any SecureNet terminal\n"
		"(Security turrets > select unit > Power). Do NOT walk in front of a\n"
		"live unit to 'check if it works'. We have had two incidents already.\n"
		"\n"
		"Units are vulnerable to electromagnetic interference. Do not use\n"
		"the welding gear near them.");

	SEC_add_file(id, "Shift log",
		"22:04  Patrol B reports all clear on the east fence.\n"
		"22:31  Camera 2 flickering again. Ticket raised.\n"
		"23:10  Unknown woman seen near the gate. Gone before the guards got there.\n"
		"23:58  Turret T-01 fired at a cat. Cat fine. Turret fine. Guard not fine.\n"
		"00:15  Delivery postponed to 02:00.");

	//
	// ...and some cameras.
	//

	{
		SLONG ta = (p_term->Draw.Tweened->Angle - SEC_yaw_offset()) & 2047;

		SEC_add_cam(id, "This room",
			(p_term->WorldPos.X >> 8) + (-SIN(ta) * 0x200 >> 16) + (COS(ta) * 0x100 >> 16),
			(p_term->WorldPos.Z >> 8) + (-COS(ta) * 0x200 >> 16) - (SIN(ta) * 0x100 >> 16),
			0x140,
			SEC_angle_to(
				(p_term->WorldPos.X >> 8) + (-SIN(ta) * 0x200 >> 16) + (COS(ta) * 0x100 >> 16),
				(p_term->WorldPos.Z >> 8) + (-COS(ta) * 0x200 >> 16) - (SIN(ta) * 0x100 >> 16),
				p_term->WorldPos.X >> 8,
				p_term->WorldPos.Z >> 8),
			1880);

		SEC_add_cam(id, "Street",
			darci->WorldPos.X >> 8,
			darci->WorldPos.Z >> 8,
			0x300,
			darci->Draw.Tweened->Angle,
			1850);
	}

	CBYTE msg[64];

	sprintf(msg, "Computer %d: walk up to it and press ACTION", id);
	CONSOLE_text(msg, 4000);
}

void SEC_cmd_emp(CBYTE *args)
{
	Thing *darci = NET_PERSON(0);

	if (darci == NULL)
	{
		return;
	}

	SLONG n = atoi(args);

	if (n <= 0) {n = 3;}
	if (n > 8)  {n = 8;}

	Thing *p_has = person_has_special(darci, SPECIAL_EMP_GRENADE);

	if (p_has)
	{
		p_has->Genus.Special->ammo += n;
	}
	else
	{
		Thing *p_emp = alloc_special(
						SPECIAL_EMP_GRENADE,
						SPECIAL_SUBSTATE_NONE,
						darci->WorldPos.X >> 8,
						darci->WorldPos.Y >> 8,
						darci->WorldPos.Z >> 8,
						0);

		if (p_emp == NULL)
		{
			return;
		}

		p_emp->SubState            = SPECIAL_SUBSTATE_NONE;
		p_emp->Genus.Special->ammo = n;

		person_get_item(darci, p_emp);
	}

	CBYTE msg[64];

	sprintf(msg, "EMP grenades: %d (select it like a grenade)", (person_has_special(darci, SPECIAL_EMP_GRENADE)) ? person_has_special(darci, SPECIAL_EMP_GRENADE)->Genus.Special->ammo : n);
	CONSOLE_text(msg, 4000);
}

void SEC_cmd_where(void)
{
	Thing *darci = NET_PERSON(0);

	if (darci == NULL)
	{
		return;
	}

	CBYTE msg[96];

	sprintf(msg, "x %.2f  z %.2f  y %d  yaw %d",
		float(darci->WorldPos.X >> 8) / 256.0F,
		float(darci->WorldPos.Z >> 8) / 256.0F,
		darci->WorldPos.Y >> 8,
		darci->Draw.Tweened->Angle * 360 / 2048);

	CONSOLE_text(msg, 6000);
}

static void SEC_write_text(FILE *handle, CBYTE *text)
{
	fputs(text, handle);

	if (text[0] && text[strlen(text) - 1] != '\n')
	{
		fputs("\n", handle);
	}
}

void SEC_cmd_save(void)
{
	CBYTE path[_MAX_PATH];
	SLONG i;

	SEC_level_path(path, sizeof(path));

	FILE *handle = fopen(path, "wb");

	if (handle == NULL)
	{
		CONSOLE_text("secsave: can't write the file", 4000);

		return;
	}

	fprintf(handle, "# Urban Chaos security setup for %s (see Source\\security.cpp)\r\n", ELEV_fname_level);
	fprintf(handle, "# turret   <id> <x> <z> <yaw> [hostile|friendly] [exposed|hidden] [on|off] [range=8] [arc=0] [y=height]\r\n");
	fprintf(handle, "# terminal <id> <x> <z> <yaw> [\"name\"] [y=height]\r\n");
	fprintf(handle, "# link <terminal> <turret|all>    camera <terminal> \"name\" <x> <z> <height> <yaw> [pitch]\r\n");
	fprintf(handle, "# file <terminal> \"title\"  ...text...  endfile\r\n\r\n");

	SLONG count = 0;
	SLONG list  = thing_class_head[CLASS_BAT];

	while(list)
	{
		Thing *p_thing = TO_THING(list);

		list = p_thing->NextLink;

		if (!SEC_is_security(p_thing))
		{
			continue;
		}

		Bat *p_bat = p_thing->Genus.Bat;

		float x   = float(p_thing->WorldPos.X >> 8) / 256.0F;
		float z   = float(p_thing->WorldPos.Z >> 8) / 256.0F;
		SLONG yaw = ((p_bat->dy - SEC_yaw_offset()) & 2047) * 360 / 2048;

		if (p_bat->type == BAT_TYPE_TURRET)
		{
			if (p_bat->state == SEC_TS_DEAD)
			{
				continue;
			}

			fprintf(handle, "turret %d %.2f %.2f %d %s %s %s range=%.1f arc=%d y=%d\r\n",
				p_bat->want_z, x, z, yaw,
				(p_bat->flag & SEC_F_FRIENDLY) ? "friendly" : "hostile",
				(p_bat->flag & (SEC_F_CONCEALED | SEC_F_HAS_HATCH)) ? "hidden" : "exposed",
				(p_bat->flag & SEC_F_POWEROFF) ? "off" : "on",
				float(SEC_turret_range(p_bat)) / 256.0F,
				(p_bat->substate << 3) * 360 / 2048,
				p_bat->want_y);
		}
		else
		{
			SEC_Term *st = SEC_term_info(p_bat->want_z);

			fprintf(handle, "terminal %d %.2f %.2f %d \"%s\" y=%d\r\n",
				p_bat->want_z, x, z, yaw,
				(st) ? st->name : "Terminal",
				p_bat->want_y);
		}

		count += 1;
	}

	fprintf(handle, "\r\n");

	for (i = 0; i < SEC.link_upto; i++)
	{
		if (SEC.link[i].turret == SEC_ID_ALL)
		{
			fprintf(handle, "link %d all\r\n", SEC.link[i].term);
		}
		else
		{
			fprintf(handle, "link %d %d\r\n", SEC.link[i].term, SEC.link[i].turret);
		}
	}

	for (i = 0; i < SEC.cam_upto; i++)
	{
		SEC_Cam *sc = &SEC.cam[i];

		fprintf(handle, "camera %d \"%s\" %.2f %.2f %d %d %d\r\n",
			sc->term, sc->name,
			float(sc->x) / 256.0F, float(sc->z) / 256.0F,
			sc->height, sc->yaw * 360 / 2048, sc->pitch * 360 / 2048);
	}

	for (i = 0; i < SEC.file_upto; i++)
	{
		fprintf(handle, "\r\nfile %d \"%s\"\r\n", SEC.file[i].term, SEC.file[i].title);
		SEC_write_text(handle, SEC.file[i].text);
		fprintf(handle, "endfile\r\n");
	}

	fclose(handle);

	CBYTE msg[_MAX_PATH + 40];

	sprintf(msg, "Saved %d turrets / terminals to %s", count, path);
	CONSOLE_text(msg, 5000);
}
