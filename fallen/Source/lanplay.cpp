//
// lanplay.cpp
//
// Two-player LAN game in lockstep. See lanplay.h for how it works and how to
// switch it on.
//

#include "game.h"
#include "cnet.h"
#include "fc.h"
#include "pcom.h"
#include "interfac.h"
#include "gamemenu.h"
#include "ddlib.h"
#include "poly.h"
#include "font2d.h"
#include "config_extras.h"
#include "lanplay.h"
#include "lansock.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <mmsystem.h>

//
// Bump this whenever the messages or the simulation change in a way that
// makes two different builds unable to play together.
//

#define LP_VERSION		5
#define LP_MAGIC		0x4e4c4355	// 'UCLN'

#define LP_Q			128			// Ring of turns we keep inputs / checksums for.
#define LP_MAX_LEVEL	200

//
// Message types.
//

#define LPM_HELLO		1	// host -> client: mission + settings
#define LPM_ACK			2	// client -> host
#define LPM_READY		3	// both: finished loading
#define LPM_TICK		4	// both: input for one turn
#define LPM_BYE			5	// leaving
#define LPM_DUMP		6	// both: write a state dump at this turn (desync hunting)

typedef struct
{
	ULONG rng;
	ULONG players;
	ULONG people;
	ULONG vehicles;
	ULONG other;

} LP_Sums;

#define LP_SUM_RNG		(1 << 0)
#define LP_SUM_PLAYERS	(1 << 1)
#define LP_SUM_PEOPLE	(1 << 2)
#define LP_SUM_VEHICLES	(1 << 3)
#define LP_SUM_OTHER	(1 << 4)

typedef struct
{
	UBYTE type;
	UBYTE version;
	UWORD fps;
	UWORD delay;
	UWORD draw_dist;
	ULONG magic;
	ULONG seed;

	//
	// Settings that change how the game plays - the client takes the host's.
	//

	UBYTE custom_turn_enabled;
	UBYTE legolas_aim;
	SWORD custom_turn_value;
	SLONG max_range_blocks;
	SWORD mouse_shoot_cooldown;
	SWORD car_flee;
	SWORD tyre_shot_chance;
	UBYTE violence;
	UBYTE pad;

	UBYTE stats[8];		// Darci and Roper strength / constitution / skill / stamina

	CBYTE level[LP_MAX_LEVEL];

} LP_Hello;

typedef struct
{
	UBYTE type;
	UBYTE version;
	UWORD pad;
	ULONG magic;

} LP_Ack;

typedef struct
{
	UBYTE type;
	UBYTE num_players;
	UWORD pad;

} LP_Ready;

typedef struct
{
	UBYTE         type;
	UBYTE         pad[3];
	ULONG         turn;			// The turn this input is for.
	ULONG         sums_turn;	// The turn the checksums were taken at (before it ran).
	LP_Sums       sums;
	LANPLAY_Input in;

} LP_Tick;

typedef struct
{
	UBYTE type;
	UBYTE pad[3];
	ULONG turn;

} LP_Dump;

//
// Settings.
//

static SLONG LP_mode      = 0;		// 0 off, 1 host, 2 join
static CBYTE LP_address[64] = "127.0.0.1";
static SLONG LP_port      = 27015;
static SLONG LP_fps_value = 30;
static SLONG LP_delay     = 1;
static SLONG LP_draw_dist = 64;
static ULONG LP_seed      = 0;

//
// State of the game in progress.
//

LANPLAY_Input LANPLAY_in[2];

static ULONG         LP_turn;
static LANPLAY_Input LP_local_q [LP_Q];
static LANPLAY_Input LP_remote_q[LP_Q];
static ULONG         LP_remote_turn[LP_Q];

static LP_Sums       LP_my_sums      [LP_Q];
static ULONG         LP_my_sums_turn [LP_Q];
static LP_Sums       LP_their_sums   [LP_Q];
static ULONG         LP_their_sums_turn[LP_Q];

static SLONG LP_connected_game = FALSE;	// The other PC is still there.
static SLONG LP_lost_turn      = -1;
static SLONG LP_desync_turn    = -1;
static SLONG LP_desync_what    = 0;
static SLONG LP_dump_turn      = -1;
static SLONG LP_dump_written   = FALSE;
static SLONG LP_their_ready    = FALSE;
static SLONG LP_their_players  = 0;

static CBYTE LP_message[160]   = "";
static DWORD LP_message_ms     = 0;
static DWORD LP_message_until  = 0;	// 0 => starts counting when it is first drawn

static ULONG LP_sim_seed       = 1;
static ULONG LP_cosmetic_seed  = 12345;

//
// Per-player copies of things the game keeps in single globals.
//

static SLONG LP_look_pitch   [2];
static SWORD LP_inv_mode     [2];

static const LANPLAY_Input *LP_cur = NULL;
static SLONG                LP_cur_id;

static SLONG LP_save_mouse_dx;
static SLONG LP_save_mouse_dy;
static UBYTE LP_save_keys[9];
static UBYTE LP_save_shift;
static bool  LP_save_mouse_cfg;
static SLONG LP_save_look_pitch;
static SWORD LP_save_inv_mode;

static SLONG LP_view_swapped = FALSE;

//
// This PC's own settings, put back after a LAN game changed them.
//

static SLONG LP_settings_saved = FALSE;
static bool  LP_old_custom_turn_enabled;
static bool  LP_old_legolas_aim;
static bool  LP_old_killcam;
static SWORD LP_old_custom_turn_value;
static SLONG LP_old_max_range_blocks;
static SWORD LP_old_mouse_shoot_cooldown;
static SWORD LP_old_car_flee;
static SWORD LP_old_tyre_shot_chance;
static UBYTE LP_old_violence;
static UBYTE LP_old_stats[8];

static void LP_save_settings(void)
{
	if (LP_settings_saved)
	{
		return;
	}

	ConfigExtras &cfg = ConfigExtras::getInstance();

	LP_old_custom_turn_enabled  = cfg.mCustomTurnRateEnabled;
	LP_old_legolas_aim          = cfg.mLegolasAim;
	LP_old_killcam              = cfg.mKillCam;
	LP_old_custom_turn_value    = cfg.mCustomTurnRateValue;
	LP_old_max_range_blocks     = cfg.mMax_range_blocks;
	LP_old_mouse_shoot_cooldown = cfg.mMouseShootCooldown;
	LP_old_car_flee             = cfg.mCarFlee;
	LP_old_tyre_shot_chance     = cfg.mTyreShotChance;
	LP_old_violence             = VIOLENCE;

	LP_old_stats[0] = the_game.DarciStrength;
	LP_old_stats[1] = the_game.DarciConstitution;
	LP_old_stats[2] = the_game.DarciSkill;
	LP_old_stats[3] = the_game.DarciStamina;
	LP_old_stats[4] = the_game.RoperStrength;
	LP_old_stats[5] = the_game.RoperConstitution;
	LP_old_stats[6] = the_game.RoperSkill;
	LP_old_stats[7] = the_game.RoperStamina;

	LP_settings_saved = TRUE;
}

static void LP_restore_settings(void)
{
	if (!LP_settings_saved)
	{
		return;
	}

	ConfigExtras &cfg = ConfigExtras::getInstance();

	cfg.mCustomTurnRateEnabled = LP_old_custom_turn_enabled;
	cfg.mLegolasAim            = LP_old_legolas_aim;
	cfg.mKillCam               = LP_old_killcam;
	cfg.mCustomTurnRateValue   = LP_old_custom_turn_value;
	cfg.mMax_range_blocks      = LP_old_max_range_blocks;
	cfg.mMouseShootCooldown    = LP_old_mouse_shoot_cooldown;
	cfg.mCarFlee               = LP_old_car_flee;
	cfg.mTyreShotChance        = LP_old_tyre_shot_chance;
	VIOLENCE                   = LP_old_violence;

	if (LP_mode == 2)
	{
		//
		// We played with the host's Darci - get our own back.
		//

		the_game.DarciStrength     = LP_old_stats[0];
		the_game.DarciConstitution = LP_old_stats[1];
		the_game.DarciSkill        = LP_old_stats[2];
		the_game.DarciStamina      = LP_old_stats[3];
		the_game.RoperStrength     = LP_old_stats[4];
		the_game.RoperConstitution = LP_old_stats[5];
		the_game.RoperSkill        = LP_old_stats[6];
		the_game.RoperStamina      = LP_old_stats[7];
	}

	LP_settings_saved = FALSE;
}
static SLONG LP_view_saved_id;

static UBYTE LP_number_key[9] = {0, KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8};

//
// Log of Random() calls (see Game.h), for the desync dump.
//

UBYTE LANPLAY_trace = FALSE;

typedef struct
{
	ULONG       turn;
	const char *file;
	ULONG       line;
	ULONG       seed;	// RAND_SEED before the call

} LP_Trace;

#define LP_TRACE_MAX (1 << 18)	// Must be a power of two.

static LP_Trace *LP_trace_buf  = NULL;
static ULONG     LP_trace_upto = 0;

void LANPLAY_trace_random(const char *file, int line)
{
	if (LP_trace_buf == NULL)
	{
		return;
	}

	LP_Trace *t = &LP_trace_buf[LP_trace_upto & (LP_TRACE_MAX - 1)];

	t->turn = LP_turn;
	t->file = file;
	t->line = line;
	t->seed = RAND_SEED;

	LP_trace_upto += 1;
}

static void LP_trace_start(void)
{
	if (LP_trace_buf == NULL)
	{
		LP_trace_buf = (LP_Trace *) malloc(sizeof(LP_Trace) * LP_TRACE_MAX);
	}

	LP_trace_upto = 0;
	LANPLAY_trace = (LP_trace_buf != NULL);
}

static void LP_trace_stop(void)
{
	LANPLAY_trace = FALSE;

	if (LP_trace_buf)
	{
		free(LP_trace_buf);
		LP_trace_buf = NULL;
	}
}

static void LP_trace_write(FILE *f, SLONG from_turn, SLONG to_turn)
{
	if (LP_trace_buf == NULL)
	{
		return;
	}

	ULONG first = (LP_trace_upto > LP_TRACE_MAX) ? LP_trace_upto - LP_TRACE_MAX : 0;
	ULONG i;

	fprintf(f, "\nRandom() calls in turns %d..%d (turn  seed-before  file:line):\n", from_turn, to_turn);

	for (i = first; i < LP_trace_upto; i++)
	{
		LP_Trace *t = &LP_trace_buf[i & (LP_TRACE_MAX - 1)];

		if ((SLONG) t->turn < from_turn || (SLONG) t->turn > to_turn)
		{
			continue;
		}

		const char *name = t->file;
		const char *s;

		for (s = t->file; *s; s++)
		{
			if (*s == '\\' || *s == '/')
			{
				name = s + 1;
			}
		}

		fprintf(f, "%5d  %10u  %s:%d\n", t->turn, t->seed, name, t->line);
	}
}

//
// Externals from the rest of the game.
//

extern volatile SLONG MouseDX;
extern volatile SLONG MouseDY;
extern SLONG  look_pitch;
extern SWORD  CONTROLS_inventory_mode;
extern CBYTE  STARTSCR_mission[_MAX_PATH];
extern CBYTE  ELEV_fname_level[];
extern SLONG  INTERFAC_first_person_key(void);
extern void   ATTRACT_loadscreen_init(void);

//
// ------------------------------------------------------------------------
// Small helpers.
// ------------------------------------------------------------------------
//

static void LP_say(const CBYTE *msg, DWORD ms)
{
	strncpy(LP_message, msg, sizeof(LP_message) - 1);
	LP_message[sizeof(LP_message) - 1] = 0;

	LP_message_ms    = ms;
	LP_message_until = 0;	// Loading can take a while - start the clock once it's on screen.
}

static void LP_title(const CBYTE *text)
{
	static CBYTE original[200] = "";

	CBYTE title[300];

	if (!hDDLibWindow)
	{
		return;
	}

	if (original[0] == 0)
	{
		GetWindowTextA(hDDLibWindow, original, sizeof(original));

		if (original[0] == 0)
		{
			strcpy(original, "Urban Chaos");
		}
	}

	if (text)
	{
		sprintf(title, "%s - %s", original, text);
	}
	else
	{
		strcpy(title, original);
	}

	SetWindowTextA(hDDLibWindow, title);
}

//
// Keeps Windows happy while we wait. Returns FALSE if the game is being closed.
//

static SLONG LP_pump_windows(void)
{
	return SHELL_ACTIVE;
}

static SLONG LP_escape(void)
{
	if (Keys[KB_ESC])
	{
		Keys[KB_ESC] = 0;
		return TRUE;
	}

	return FALSE;
}

static void LP_read_settings(void)
{
	LP_mode      = ENV_get_value_number("lan_mode",  0,     "Network");
	LP_port      = ENV_get_value_number("lan_port",  27015, "Network");
	LP_fps_value = ENV_get_value_number("lan_fps",   30,    "Network");
	LP_delay     = ENV_get_value_number("lan_delay", 1,     "Network");

	CBYTE *addr = ENV_get_value_string("lan_address", "Network");

	if (addr && addr[0])
	{
		strncpy(LP_address, addr, sizeof(LP_address) - 1);
		LP_address[sizeof(LP_address) - 1] = 0;
	}

	//
	// The command line wins: -host  /  -join <address>  /  -port <n>
	//

	const char *cmd = GetCommandLineA();

	if (cmd)
	{
		const char *s;

		if (strstr(cmd, "-host") || strstr(cmd, "/host"))
		{
			LP_mode = 1;
		}

		if ((s = strstr(cmd, "-join ")) || (s = strstr(cmd, "/join ")))
		{
			s += 6;

			while (*s == ' ') s++;

			SLONG i = 0;

			while (*s && *s != ' ' && *s != '"' && i < (SLONG) sizeof(LP_address) - 1)
			{
				LP_address[i++] = *s++;
			}

			LP_address[i] = 0;

			if (i)
			{
				LP_mode = 2;
			}
		}

		if ((s = strstr(cmd, "-port ")) || (s = strstr(cmd, "/port ")))
		{
			SLONG p = atoi(s + 6);

			if (p > 0 && p < 65536)
			{
				LP_port = p;
			}
		}
	}

	SATURATE(LP_fps_value, 20, 40);
	SATURATE(LP_delay,     0,  8);
}

SLONG LANPLAY_requested(void)
{
	LP_read_settings();

	return LP_mode != 0;
}

//
// Length of the game turns. Like single player the turn is as long as the
// frame really took - but measured on the host and sent along with its input,
// so both PCs use exactly the same numbers.
//

#define LP_TICK_DEFAULT 33
#define LP_TICK_MIN     10
#define LP_TICK_MAX     100

static SLONG         LP_next_tick = LP_TICK_DEFAULT;	// Length of the next turn (ms).
static SLONG         LP_tick_hist[4];
static SLONG         LP_tick_hist_upto;
static ULONG         LP_game_ms;						// Game time so far, the same on both PCs.
static LARGE_INTEGER LP_host_last;
static SLONG         LP_host_last_valid = FALSE;
static SLONG         LP_timer_period    = FALSE;

static void LP_tick_reset(void)
{
	SLONG i;

	LP_next_tick       = LP_TICK_DEFAULT;
	LP_tick_hist_upto  = 0;
	LP_game_ms         = 0;
	LP_host_last_valid = FALSE;

	for (i = 0; i < 4; i++)
	{
		LP_tick_hist[i] = LP_TICK_DEFAULT;
	}
}

//
// Host only: how long since the last turn, in ms.
//

static SLONG LP_host_measure_tick(void)
{
	LARGE_INTEGER now;
	LARGE_INTEGER freq;

	QueryPerformanceCounter  (&now);
	QueryPerformanceFrequency(&freq);

	SLONG ms = LP_TICK_DEFAULT;

	if (LP_host_last_valid && freq.QuadPart)
	{
		ms = (SLONG) ((now.QuadPart - LP_host_last.QuadPart) * 1000 / freq.QuadPart);
	}

	LP_host_last       = now;
	LP_host_last_valid = TRUE;

	SATURATE(ms, LP_TICK_MIN, LP_TICK_MAX);

	return ms;
}

//
// Both PCs: the host's measured turn length arrives - average the last four
// (like single player does, so cars don't judder).
//

static void LP_tick_received(SLONG raw)
{
	if (raw <= 0)
	{
		raw = LP_TICK_DEFAULT;
	}

	LP_tick_hist[LP_tick_hist_upto & 3] = raw;
	LP_tick_hist_upto += 1;

	LP_next_tick = (LP_tick_hist[0] + LP_tick_hist[1] + LP_tick_hist[2] + LP_tick_hist[3] + 2) >> 2;

	LP_game_ms += LP_next_tick;
}

SLONG LANPLAY_tick_ms(void)
{
	return LP_next_tick;
}

ULONG LANPLAY_game_ms(void)
{
	return LP_game_ms;
}

SLONG LANPLAY_fps(void)
{
	return LP_fps_value;
}

SLONG LANPLAY_sim_draw_distance(void)
{
	if (CNET_network_game)
	{
		return LP_draw_dist;
	}

	return ConfigExtras::getInstance().mDrawDistance;
}

//
// ------------------------------------------------------------------------
// Random numbers.
// ------------------------------------------------------------------------
//

SLONG LANPLAY_sim_rand(void)
{
	if (!CNET_network_game)
	{
		return rand();
	}

	LP_sim_seed = LP_sim_seed * 214013 + 2531011;

	return (LP_sim_seed >> 16) & 0x7fff;
}

UWORD LANPLAY_cosmetic_rand(void)
{
	if (!CNET_network_game)
	{
		return Random();	// Exactly as before in single player.
	}

	LP_cosmetic_seed = LP_cosmetic_seed * 69069 + 1;

	return (UWORD) (LP_cosmetic_seed >> 7);
}

//
// ------------------------------------------------------------------------
// Checksums - to notice when the two games stop being the same.
// ------------------------------------------------------------------------
//

static ULONG LP_mix(ULONG h, SLONG v)
{
	h ^= (ULONG) v;
	h *= 0x01000193;

	return h;
}

static void LP_compute_sums(LP_Sums *s)
{
	SLONG  i;
	UWORD  index;
	Thing *p_thing;

	s->rng      = LP_mix(LP_mix(0x811c9dc5, RAND_SEED), (SLONG) LP_sim_seed);
	s->players  = 0x811c9dc5;
	s->people   = 0x811c9dc5;
	s->vehicles = 0x811c9dc5;
	s->other    = 0x811c9dc5;

	for (i = 0; i < NO_PLAYERS; i++)
	{
		p_thing = NET_PERSON(i);

		if (p_thing)
		{
			s->players = LP_mix(s->players, p_thing->WorldPos.X);
			s->players = LP_mix(s->players, p_thing->WorldPos.Y);
			s->players = LP_mix(s->players, p_thing->WorldPos.Z);
			s->players = LP_mix(s->players, p_thing->State);
			s->players = LP_mix(s->players, p_thing->SubState);
			s->players = LP_mix(s->players, p_thing->Genus.Person->Health);
			s->players = LP_mix(s->players, p_thing->Draw.Tweened->Angle);
		}
	}

	for (index = thing_class_head[CLASS_PERSON]; index; index = p_thing->NextLink)
	{
		p_thing = TO_THING(index);

		s->people = LP_mix(s->people, index);
		s->people = LP_mix(s->people, p_thing->WorldPos.X);
		s->people = LP_mix(s->people, p_thing->WorldPos.Z);
		s->people = LP_mix(s->people, p_thing->State);
		s->people = LP_mix(s->people, p_thing->Genus.Person->Health);
	}

	for (index = thing_class_head[CLASS_VEHICLE]; index; index = p_thing->NextLink)
	{
		p_thing = TO_THING(index);

		s->vehicles = LP_mix(s->vehicles, index);
		s->vehicles = LP_mix(s->vehicles, p_thing->WorldPos.X);
		s->vehicles = LP_mix(s->vehicles, p_thing->WorldPos.Y);
		s->vehicles = LP_mix(s->vehicles, p_thing->WorldPos.Z);
	}

	//
	// Things you can pick up, push, ride or blow up. (Not effects - those
	// are allowed to differ.)
	//

	for (i = 1; i < MAX_THINGS; i++)
	{
		p_thing = TO_THING(i);

		switch(p_thing->Class)
		{
			case CLASS_SPECIAL:
			case CLASS_BARREL:
			case CLASS_BIKE:
			case CLASS_BAT:
			case CLASS_PLAT:
				s->other = LP_mix(s->other, i);
				s->other = LP_mix(s->other, p_thing->Class);
				s->other = LP_mix(s->other, p_thing->WorldPos.X);
				s->other = LP_mix(s->other, p_thing->WorldPos.Z);
				break;

			default:
				break;
		}
	}
}

static SLONG LP_sums_differ(const LP_Sums *a, const LP_Sums *b)
{
	SLONG what = 0;

	if (a->rng      != b->rng     ) {what |= LP_SUM_RNG;     }
	if (a->players  != b->players ) {what |= LP_SUM_PLAYERS; }
	if (a->people   != b->people  ) {what |= LP_SUM_PEOPLE;  }
	if (a->vehicles != b->vehicles) {what |= LP_SUM_VEHICLES;}
	if (a->other    != b->other   ) {what |= LP_SUM_OTHER;   }

	return what;
}

static void LP_check_sums(ULONG turn)
{
	SLONG slot = turn % LP_Q;

	if (LP_desync_turn >= 0)
	{
		return;	// Already know.
	}

	if (LP_my_sums_turn[slot] != turn || LP_their_sums_turn[slot] != turn)
	{
		return;
	}

	SLONG what = LP_sums_differ(&LP_my_sums[slot], &LP_their_sums[slot]);

	if (what)
	{
		LP_desync_turn = turn;
		LP_desync_what = what;

		//
		// Get both PCs to dump their state at the same (future) turn.
		//

		SLONG dump_at = LP_turn + LP_delay + 6;

		if (LP_dump_turn < 0 || dump_at < LP_dump_turn)
		{
			LP_dump_turn = dump_at;
		}

		LP_Dump msg;
		memset(&msg, 0, sizeof(msg));
		msg.type = LPM_DUMP;
		msg.turn = LP_dump_turn;

		LANSOCK_send(&msg, sizeof(msg));
	}
}

static void LP_write_dump(void)
{
	CBYTE fname[64];

	sprintf(fname, "lan_desync_P%d.txt", PLAYER_ID + 1);

	FILE *f = fopen(fname, "w");

	if (f == NULL)
	{
		return;
	}

	fprintf(f, "Urban Chaos LAN state dump. Player %d, turn %d, GAME_TURN %d\n", PLAYER_ID + 1, LP_turn, GAME_TURN);
	fprintf(f, "First desync at turn %d, what=%d (1 rng, 2 players, 4 people, 8 vehicles, 16 other)\n", LP_desync_turn, LP_desync_what);
	fprintf(f, "RAND_SEED %u  sim_seed %u  level %s\n\n", (ULONG) RAND_SEED, LP_sim_seed, ELEV_fname_level);

	for (SLONG i = 1; i < MAX_THINGS; i++)
	{
		Thing *p_thing = TO_THING(i);

		if (p_thing->Class == CLASS_NONE)
		{
			continue;
		}

		fprintf(f, "%5d class %2d  pos %8d %8d %8d  state %3d sub %3d  flags %08x",
			i,
			p_thing->Class,
			p_thing->WorldPos.X,
			p_thing->WorldPos.Y,
			p_thing->WorldPos.Z,
			p_thing->State,
			p_thing->SubState,
			p_thing->Flags & ~FLAGS_IN_VIEW);

		if (p_thing->Class == CLASS_PERSON)
		{
			fprintf(f, "  type %2d hp %4d ai %2d ai_state %2d move %2d angle %4d",
				p_thing->Genus.Person->PersonType,
				p_thing->Genus.Person->Health,
				p_thing->Genus.Person->pcom_ai,
				p_thing->Genus.Person->pcom_ai_state,
				p_thing->Genus.Person->pcom_move,
				p_thing->Draw.Tweened->Angle);
		}

		fprintf(f, "\n");
	}

	//
	// Where the random numbers went just before the games split: the first
	// line that differs between the two dumps is the culprit.
	//

	if (LP_desync_turn >= 0)
	{
		LP_trace_write(f, LP_desync_turn - 1, LP_desync_turn);
	}

	fclose(f);

	LP_dump_written = TRUE;
}

//
// ------------------------------------------------------------------------
// Network messages.
// ------------------------------------------------------------------------
//

static void LP_lost_connection(void)
{
	if (LP_connected_game)
	{
		LP_connected_game = FALSE;
		LP_lost_turn      = LP_turn;

		LANSOCK_close();

		LP_say("LAN: drugi gracz sie rozlaczyl - grasz dalej sam", 8000);
	}
}

static void LP_handle_message(UBYTE *buf, SLONG len)
{
	switch(buf[0])
	{
		case LPM_TICK:

			if (len >= (SLONG) sizeof(LP_Tick))
			{
				LP_Tick *t    = (LP_Tick *) buf;
				SLONG    slot = t->turn % LP_Q;

				LP_remote_q   [slot] = t->in;
				LP_remote_turn[slot] = t->turn;

				SLONG sslot = t->sums_turn % LP_Q;

				LP_their_sums     [sslot] = t->sums;
				LP_their_sums_turn[sslot] = t->sums_turn;

				LP_check_sums(t->sums_turn);
			}

			break;

		case LPM_READY:

			if (len >= (SLONG) sizeof(LP_Ready))
			{
				LP_their_ready   = TRUE;
				LP_their_players = ((LP_Ready *) buf)->num_players;
			}

			break;

		case LPM_DUMP:

			if (len >= (SLONG) sizeof(LP_Dump))
			{
				SLONG at = ((LP_Dump *) buf)->turn;

				if (LP_dump_turn < 0 || at < LP_dump_turn)
				{
					if (at >= (SLONG) LP_turn)
					{
						LP_dump_turn = at;
					}
				}
			}

			break;

		case LPM_BYE:
			LP_lost_connection();
			break;

		default:
			break;
	}
}

//
// Reads everything that has arrived.
//

static void LP_pump_net(void)
{
	UBYTE buf[LANSOCK_MAX_MSG];

	while (LP_connected_game)
	{
		SLONG len = LANSOCK_poll(buf, sizeof(buf));

		if (len < 0)
		{
			LP_lost_connection();
			break;
		}

		if (len == 0)
		{
			break;
		}

		LP_handle_message(buf, len);
	}
}

//
// Waits for one particular message during the handshake.
//

static SLONG LP_wait_for(UBYTE type, void *out, SLONG out_size, DWORD timeout_ms, const CBYTE *what)
{
	UBYTE buf[LANSOCK_MAX_MSG];
	DWORD start = GetTickCount();

	while (1)
	{
		SLONG len = LANSOCK_poll(buf, sizeof(buf));

		if (len < 0)
		{
			LP_say("LAN: polaczenie przerwane", 6000);
			return FALSE;
		}

		if (len > 0)
		{
			if (buf[0] == type && len >= out_size)
			{
				memcpy(out, buf, out_size);
				return TRUE;
			}

			continue;
		}

		if (!LP_pump_windows())
		{
			return FALSE;
		}

		if (LP_escape())
		{
			LP_say("LAN: anulowano", 4000);
			return FALSE;
		}

		if (GetTickCount() - start > timeout_ms)
		{
			CBYTE str[128];
			sprintf(str, "LAN: brak odpowiedzi (%s)", what);
			LP_say(str, 6000);
			return FALSE;
		}

		LANSOCK_wait(20);
	}
}

//
// ------------------------------------------------------------------------
// Connecting.
// ------------------------------------------------------------------------
//

static SLONG LP_host_handshake(void)
{
	CBYTE str[160];

	if (!LANSOCK_listen((unsigned short) LP_port))
	{
		sprintf(str, "LAN: nie moge nasluchiwac na porcie %d", LP_port);
		LP_say(str, 8000);
		return FALSE;
	}

	sprintf(str, "LAN: czekam na drugiego gracza (port %d)... Esc = anuluj", LP_port);
	LP_title(str);

	while (1)
	{
		SLONG r = LANSOCK_accept_poll();

		if (r > 0)
		{
			break;
		}

		if (r < 0 || !LP_pump_windows())
		{
			LP_say("LAN: blad gniazda", 6000);
			return FALSE;
		}

		if (LP_escape())
		{
			LP_say("LAN: anulowano - gra jednoosobowa", 4000);
			return FALSE;
		}

		Sleep(20);
	}

	sprintf(str, "LAN: polaczono z %s, wysylam misje...", LANSOCK_peer_name());
	LP_title(str);

	//
	// Tell the other PC what we're playing and how.
	//

	LP_Hello hello;
	memset(&hello, 0, sizeof(hello));

	ConfigExtras &cfg = ConfigExtras::getInstance();

	LP_seed      = (ULONG) time(NULL) ^ (GetTickCount() << 8);
	LP_draw_dist = cfg.mDrawDistance;

	hello.type                 = LPM_HELLO;
	hello.version              = LP_VERSION;
	hello.magic                = LP_MAGIC;
	hello.fps                  = (UWORD) LP_fps_value;
	hello.delay                = (UWORD) LP_delay;
	hello.draw_dist            = (UWORD) LP_draw_dist;
	hello.seed                 = LP_seed;
	hello.custom_turn_enabled  = cfg.mCustomTurnRateEnabled;
	hello.legolas_aim          = cfg.mLegolasAim;
	hello.custom_turn_value    = cfg.mCustomTurnRateValue;
	hello.max_range_blocks     = cfg.mMax_range_blocks;
	hello.mouse_shoot_cooldown = cfg.mMouseShootCooldown;
	hello.car_flee             = cfg.mCarFlee;
	hello.tyre_shot_chance     = cfg.mTyreShotChance;
	hello.violence             = VIOLENCE;
	hello.stats[0]             = the_game.DarciStrength;
	hello.stats[1]             = the_game.DarciConstitution;
	hello.stats[2]             = the_game.DarciSkill;
	hello.stats[3]             = the_game.DarciStamina;
	hello.stats[4]             = the_game.RoperStrength;
	hello.stats[5]             = the_game.RoperConstitution;
	hello.stats[6]             = the_game.RoperSkill;
	hello.stats[7]             = the_game.RoperStamina;

	if ((GAME_STATE & GS_REPLAY) || STARTSCR_mission[0] == 0)
	{
		strncpy(hello.level, ELEV_fname_level, LP_MAX_LEVEL - 1);
	}
	else
	{
		strncpy(hello.level, STARTSCR_mission, LP_MAX_LEVEL - 1);
	}

	if (hello.level[0] == 0)
	{
		LP_say("LAN: najpierw wybierz misje w menu", 6000);
		return FALSE;
	}

	if (!LANSOCK_send(&hello, sizeof(hello)))
	{
		LP_say("LAN: polaczenie przerwane", 6000);
		return FALSE;
	}

	LP_Ack ack;

	if (!LP_wait_for(LPM_ACK, &ack, sizeof(ack), 15000, "potwierdzenie"))
	{
		return FALSE;
	}

	if (ack.magic != LP_MAGIC || ack.version != LP_VERSION)
	{
		LP_say("LAN: druga gra ma inna wersje - skompiluj ten sam kod na obu PC", 8000);
		return FALSE;
	}

	return TRUE;
}

static SLONG LP_join_handshake(void)
{
	CBYTE str[160];

	sprintf(str, "LAN: lacze z %s:%d... Esc = anuluj", LP_address, LP_port);
	LP_title(str);

	DWORD start = GetTickCount();

	while (1)
	{
		if (!LANSOCK_connect_start(LP_address, (unsigned short) LP_port))
		{
			sprintf(str, "LAN: zly adres '%s'", LP_address);
			LP_say(str, 8000);
			return FALSE;
		}

		DWORD attempt = GetTickCount();
		SLONG r       = 0;

		while (r == 0 && GetTickCount() - attempt < 2000)
		{
			r = LANSOCK_connect_poll();

			if (!LP_pump_windows())
			{
				return FALSE;
			}

			if (LP_escape())
			{
				LP_say("LAN: anulowano - gra jednoosobowa", 4000);
				return FALSE;
			}

			if (r == 0)
			{
				Sleep(20);
			}
		}

		if (r > 0)
		{
			break;
		}

		//
		// Host not there (yet) - keep trying for a minute.
		//

		if (GetTickCount() - start > 60000)
		{
			sprintf(str, "LAN: nie moge polaczyc z %s:%d", LP_address, LP_port);
			LP_say(str, 8000);
			return FALSE;
		}

		Sleep(300);
	}

	LP_title("LAN: polaczono, czekam na misje od hosta...");

	LP_Hello hello;

	if (!LP_wait_for(LPM_HELLO, &hello, sizeof(hello), 30000, "misja od hosta"))
	{
		return FALSE;
	}

	LP_Ack ack;
	memset(&ack, 0, sizeof(ack));
	ack.type    = LPM_ACK;
	ack.version = LP_VERSION;
	ack.magic   = LP_MAGIC;

	LANSOCK_send(&ack, sizeof(ack));

	if (hello.magic != LP_MAGIC || hello.version != LP_VERSION)
	{
		LP_say("LAN: host ma inna wersje gry - skompiluj ten sam kod na obu PC", 8000);
		return FALSE;
	}

	//
	// Play the host's mission with the host's settings.
	//

	hello.level[LP_MAX_LEVEL - 1] = 0;

	LP_save_settings();

	strcpy(STARTSCR_mission, hello.level);
	strcpy(ELEV_fname_level, hello.level);

	ConfigExtras &cfg = ConfigExtras::getInstance();

	LP_fps_value = hello.fps;
	LP_delay     = hello.delay;
	LP_draw_dist = hello.draw_dist;
	LP_seed      = hello.seed;

	cfg.mCustomTurnRateEnabled = hello.custom_turn_enabled != 0;
	cfg.mLegolasAim            = hello.legolas_aim != 0;
	cfg.mCustomTurnRateValue   = hello.custom_turn_value;
	cfg.mMax_range_blocks      = hello.max_range_blocks;
	cfg.mMouseShootCooldown    = hello.mouse_shoot_cooldown;
	cfg.mCarFlee               = hello.car_flee;
	cfg.mTyreShotChance        = hello.tyre_shot_chance;

	VIOLENCE = hello.violence;

	the_game.DarciStrength     = hello.stats[0];
	the_game.DarciConstitution = hello.stats[1];
	the_game.DarciSkill        = hello.stats[2];
	the_game.DarciStamina      = hello.stats[3];
	the_game.RoperStrength     = hello.stats[4];
	the_game.RoperConstitution = hello.stats[5];
	the_game.RoperSkill        = hello.stats[6];
	the_game.RoperStamina      = hello.stats[7];

	SATURATE(LP_fps_value, 20, 40);
	SATURATE(LP_delay,     0,  8);

	return TRUE;
}

SLONG LANPLAY_setup(void)
{
	CNET_network_game = FALSE;
	CNET_i_am_host    = FALSE;
	CNET_num_players  = 1;
	CNET_player_id    = 0;

	LP_connected_game = FALSE;
	LP_view_swapped   = FALSE;
	LP_cur            = NULL;

	LP_read_settings();

	if (LP_mode == 0)
	{
		return FALSE;
	}

	//
	// Something to look at while we wait (the window title says what's going on).
	//

	ATTRACT_loadscreen_init();

	if (!LANSOCK_init())
	{
		LP_say("LAN: Winsock nie wystartowal", 6000);
		return FALSE;
	}

	SLONG ok = (LP_mode == 1) ? LP_host_handshake() : LP_join_handshake();

	LP_title(NULL);

	if (!ok)
	{
		LANSOCK_close();
		return FALSE;
	}

	CNET_network_game = TRUE;
	CNET_i_am_host    = (LP_mode == 1);
	CNET_num_players  = 2;
	CNET_player_id    = (LP_mode == 1) ? 0 : 1;

	LP_connected_game = TRUE;

	//
	// Things that only make sense for one player on one PC are off:
	// slow motion would have to happen on both PCs at once.
	//

	LP_save_settings();

	ConfigExtras::getInstance().mKillCam = false;

	return TRUE;
}

//
// ------------------------------------------------------------------------
// Loading the level.
// ------------------------------------------------------------------------
//

void LANPLAY_seed_for_loading(void)
{
	if (!CNET_network_game)
	{
		return;
	}

	SetSeed(LP_seed);
	srand(LP_seed);

	LP_sim_seed = LP_seed ^ 0x13579bdf;
}

void LANPLAY_ensure_players(void)
{
	if (!CNET_network_game)
	{
		return;
	}

	if (NO_PLAYERS >= 2 || NO_PLAYERS == 0 || NET_PERSON(0) == NULL)
	{
		return;
	}

	//
	// The mission only has one player - put a second Darci next to the first.
	//

	Thing *p_first = NET_PERSON(0);

	SLONG x = (p_first->WorldPos.X >> 8) + (SIN((p_first->Draw.Tweened->Angle + 512) & 2047) >> 9);
	SLONG z = (p_first->WorldPos.Z >> 8) + (COS((p_first->Draw.Tweened->Angle + 512) & 2047) >> 9);
	SLONG y = (p_first->WorldPos.Y >> 8);

	SLONG has = (p_first->Flags & FLAGS_HAS_GUN) ? PCOM_HAS_GUN : 0;

	THING_INDEX p_index = PCOM_create_player(
							PLAYER_DARCI,
							has,
							x, y, z,
							1,
							p_first->Draw.Tweened->Angle);

	if (p_index)
	{
		NET_PERSON(1) = TO_THING(p_index);

		NET_PLAYER(1)->Genus.Player->Strength     = the_game.DarciStrength;
		NET_PLAYER(1)->Genus.Player->Constitution = the_game.DarciConstitution;
		NET_PLAYER(1)->Genus.Player->Stamina      = the_game.DarciStamina;
		NET_PLAYER(1)->Genus.Player->Skill        = the_game.DarciSkill;

		NO_PLAYERS = 2;
	}
}

void LANPLAY_level_loaded(void)
{
	SLONG i;

	if (!CNET_network_game)
	{
		return;
	}

	if (NO_PLAYERS < 2 || NET_PERSON(PLAYER_ID) == NULL)
	{
		LP_say("LAN: ta misja nie ma dwoch graczy - gra jednoosobowa", 8000);
		LANPLAY_finish();

		if (NET_PERSON(0))
		{
			FC_look_at(0, THING_NUMBER(NET_PERSON(0)));
			FC_setup_initial_camera(0);
		}

		return;
	}

	//
	// Tell the other PC we're ready, and wait for it to finish loading too.
	//

	LP_their_ready = FALSE;

	LP_Ready ready;
	memset(&ready, 0, sizeof(ready));
	ready.type        = LPM_READY;
	ready.num_players = (UBYTE) NO_PLAYERS;

	LANSOCK_send(&ready, sizeof(ready));

	LP_title("LAN: czekam az drugi PC zaladuje misje...");

	DWORD start = GetTickCount();

	while (!LP_their_ready && LP_connected_game)
	{
		LP_pump_net();

		if (LP_their_ready)
		{
			break;
		}

		if (!LP_pump_windows() || GetTickCount() - start > 180000)
		{
			LP_lost_connection();
			break;
		}

		if (LP_escape())
		{
			LP_lost_connection();
			break;
		}

		LANSOCK_wait(20);
	}

	LP_title(NULL);

	if (LP_their_ready && LP_their_players != NO_PLAYERS)
	{
		LP_say("LAN: uwaga - na drugim PC misja ma inna liczbe graczy", 8000);
	}

	//
	// Same random numbers from here on, on both PCs.
	//

	SetSeed(LP_seed);
	srand(LP_seed);

	LP_sim_seed      = LP_seed ^ 0x5a5a1234;
	LP_cosmetic_seed = GetTickCount();

	//
	// Start the turn queues. The first 'delay' turns have no input.
	//

	LP_turn         = 0;
	LP_desync_turn  = -1;
	LP_desync_what  = 0;
	LP_dump_turn    = -1;
	LP_dump_written = FALSE;
	LP_lost_turn    = -1;

	for (i = 0; i < LP_Q; i++)
	{
		memset(&LP_local_q [i], 0, sizeof(LANPLAY_Input));
		memset(&LP_remote_q[i], 0, sizeof(LANPLAY_Input));

		LP_remote_turn    [i] = 0xffffffff;
		LP_my_sums_turn   [i] = 0xffffffff;
		LP_their_sums_turn[i] = 0xffffffff;
	}

	for (i = 0; i < LP_delay; i++)
	{
		LP_remote_turn[i] = i;
	}

	memset(LANPLAY_in, 0, sizeof(LANPLAY_in));

	LP_trace_start();

	LP_tick_reset();

	//
	// 1 ms timer resolution while we wait for the other PC (Windows'
	// default is ~16 ms, which shows as stutter).
	//

	if (!LP_timer_period)
	{
		timeBeginPeriod(1);
		LP_timer_period = TRUE;
	}

	for (i = 0; i < 2; i++)
	{
		LP_look_pitch[i] = 0;
		LP_inv_mode  [i] = 0;
	}

	//
	// Our camera looks at our player.
	//

	FC_look_at(0, THING_NUMBER(NET_PERSON(PLAYER_ID)));
	FC_setup_initial_camera(0);

	if (LP_connected_game)
	{
		LP_say(CNET_i_am_host ? "LAN: gra sieciowa - jestes graczem 1" : "LAN: gra sieciowa - jestes graczem 2", 5000);
	}
}

void LANPLAY_finish(void)
{
	LANPLAY_view_end();

	if (LP_connected_game)
	{
		UBYTE bye = LPM_BYE;

		LANSOCK_send(&bye, 1);
	}

	LANSOCK_close();

	LP_trace_stop();

	if (LP_timer_period)
	{
		timeEndPeriod(1);
		LP_timer_period = FALSE;
	}

	LP_connected_game = FALSE;
	LP_cur            = NULL;

	CNET_network_game = FALSE;
	CNET_i_am_host    = FALSE;
	CNET_num_players  = 1;
	CNET_player_id    = 0;

	LP_restore_settings();
}

//
// ------------------------------------------------------------------------
// Every turn.
// ------------------------------------------------------------------------
//

static void LP_build_local_input(LANPLAY_Input *in)
{
	SLONG i;

	memset(in, 0, sizeof(LANPLAY_Input));

	in->input = get_hardware_input(INPUT_TYPE_ALL);

	in->mouse_dx  = (SWORD) MouseDX;
	in->mouse_dy  = (SWORD) MouseDY;
	in->cam_yaw   = (SWORD) ((FC_cam[0].yaw >> 8) & 2047);
	in->cam_pitch = (SWORD) (FC_cam[0].pitch >> 8);

	for (i = 1; i <= 8; i++)
	{
		if (Keys[LP_number_key[i]])
		{
			Keys[LP_number_key[i]] = 0;
			in->weapon_key = (UBYTE) i;
			break;
		}
	}

	if (ShiftFlag)                                 {in->flags |= LANPLAY_FLAG_SHIFT;}
	if (INTERFAC_first_person_key())               {in->flags |= LANPLAY_FLAG_FIRST_PERSON;}
	if (ConfigExtras::getInstance().mMouseInput)   {in->flags |= LANPLAY_FLAG_MOUSE_CFG;}

	if (GAMEMENU_is_paused())
	{
		//
		// The menu is open on this PC - the game carries on, but our player
		// stands still.
		//

		in->input      = 0;
		in->mouse_dx   = 0;
		in->mouse_dy   = 0;
		in->weapon_key = 0;
		in->flags     &= ~(LANPLAY_FLAG_SHIFT | LANPLAY_FLAG_FIRST_PERSON);
	}
}

void LANPLAY_do_packets(void)
{
	LANPLAY_Input local;
	LP_Sums       sums;

	SLONG me    = PLAYER_ID;
	SLONG other = 1 - PLAYER_ID;

	LP_pump_net();

	if ((SLONG) LP_turn == LP_dump_turn && !LP_dump_written)
	{
		LP_write_dump();
	}

	//
	// Our input now is used 'delay' turns from now, on both PCs.
	//

	LP_build_local_input(&local);
	LP_compute_sums(&sums);

	//
	// The host's input also carries how long this frame took.
	//

	local.pad = CNET_i_am_host ? (UWORD) LP_host_measure_tick() : 0;

	LP_my_sums     [LP_turn % LP_Q] = sums;
	LP_my_sums_turn[LP_turn % LP_Q] = LP_turn;

	LP_check_sums(LP_turn);

	ULONG for_turn = LP_turn + LP_delay;

	LP_local_q[for_turn % LP_Q] = local;

	if (LP_connected_game)
	{
		LP_Tick tick;
		memset(&tick, 0, sizeof(tick));

		tick.type      = LPM_TICK;
		tick.turn      = for_turn;
		tick.sums_turn = LP_turn;
		tick.sums      = sums;
		tick.in        = local;

		if (!LANSOCK_send(&tick, sizeof(tick)))
		{
			LP_lost_connection();
		}
	}

	//
	// Wait until we have the other player's input for this turn.
	//

	SLONG slot   = LP_turn % LP_Q;
	DWORD start  = GetTickCount();
	SLONG titled = FALSE;

	while (LP_connected_game && LP_remote_turn[slot] != LP_turn)
	{
		LP_pump_net();

		if (LP_remote_turn[slot] == LP_turn || !LP_connected_game)
		{
			break;
		}

		DWORD waited = GetTickCount() - start;

		if (waited > 500 && !titled)
		{
			LP_title("LAN: czekam na drugiego gracza...");
			titled = TRUE;
		}

		if (!LP_pump_windows() || waited > 30000)
		{
			LP_lost_connection();
			break;
		}

		if (waited > 2000 && LP_escape())
		{
			LP_lost_connection();
			break;
		}

		LANSOCK_wait(1);
	}

	if (titled)
	{
		LP_title(NULL);
	}

	LANPLAY_in[me] = LP_local_q[slot];

	if (LP_remote_turn[slot] == LP_turn)
	{
		LANPLAY_in[other] = LP_remote_q[slot];
	}
	else
	{
		memset(&LANPLAY_in[other], 0, sizeof(LANPLAY_Input));	// Gone - the other player stands still.

		if (!CNET_i_am_host)
		{
			LANPLAY_in[other].pad = LP_TICK_DEFAULT;	// No host to time the game any more.
		}
	}

	//
	// Length of the next turn: from the host's input (player 0), the same on both PCs.
	//

	LP_tick_received(LANPLAY_in[0].pad);

	LP_turn += 1;
}

//
// ------------------------------------------------------------------------
// One player's input in the globals.
// ------------------------------------------------------------------------
//

void LANPLAY_begin_player(SLONG player_id)
{
	SLONG i;

	if (!CNET_network_game || LP_cur || !WITHIN(player_id, 0, 1))
	{
		return;
	}

	const LANPLAY_Input *in = &LANPLAY_in[player_id];

	LP_save_mouse_dx    = MouseDX;
	LP_save_mouse_dy    = MouseDY;
	LP_save_shift       = ShiftFlag;
	LP_save_mouse_cfg   = ConfigExtras::getInstance().mMouseInput;
	LP_save_look_pitch  = look_pitch;
	LP_save_inv_mode    = CONTROLS_inventory_mode;

	for (i = 1; i <= 8; i++)
	{
		LP_save_keys[i]        = Keys[LP_number_key[i]];
		Keys[LP_number_key[i]] = (in->weapon_key == i);
	}

	MouseDX     = in->mouse_dx;
	MouseDY     = in->mouse_dy;
	ShiftFlag   = (in->flags & LANPLAY_FLAG_SHIFT) ? 1 : 0;

	ConfigExtras::getInstance().mMouseInput = (in->flags & LANPLAY_FLAG_MOUSE_CFG) != 0;

	look_pitch              = LP_look_pitch[player_id];
	CONTROLS_inventory_mode = LP_inv_mode  [player_id];

	LP_cur    = in;
	LP_cur_id = player_id;
}

void LANPLAY_end_player(void)
{
	SLONG i;

	if (LP_cur == NULL)
	{
		return;
	}

	LP_look_pitch[LP_cur_id] = look_pitch;
	LP_inv_mode  [LP_cur_id] = CONTROLS_inventory_mode;

	for (i = 1; i <= 8; i++)
	{
		Keys[LP_number_key[i]] = LP_save_keys[i];
	}

	MouseDX     = LP_save_mouse_dx;
	MouseDY     = LP_save_mouse_dy;
	ShiftFlag   = LP_save_shift;

	ConfigExtras::getInstance().mMouseInput = LP_save_mouse_cfg;

	//
	// Outside of the player processing the globals show our own player
	// (the camera and the panel read them).
	//

	look_pitch              = LP_look_pitch[PLAYER_ID];
	CONTROLS_inventory_mode = LP_inv_mode  [PLAYER_ID];

	LP_cur = NULL;
}

const LANPLAY_Input *LANPLAY_current(void)
{
	return LP_cur;
}

//
// ------------------------------------------------------------------------
// Drawing from our own player's point of view.
// ------------------------------------------------------------------------
//

void LANPLAY_view_begin(void)
{
	if (!CNET_network_game || LP_view_swapped || PLAYER_ID == 0)
	{
		return;
	}

	Thing *p;

	p                 = NET_PERSON(0);
	NET_PERSON(0)     = NET_PERSON(PLAYER_ID);
	NET_PERSON(PLAYER_ID) = p;

	p                 = NET_PLAYER(0);
	NET_PLAYER(0)     = NET_PLAYER(PLAYER_ID);
	NET_PLAYER(PLAYER_ID) = p;

	LP_view_saved_id = PLAYER_ID;
	PLAYER_ID        = 0;
	LP_view_swapped  = TRUE;
}

void LANPLAY_view_end(void)
{
	if (!LP_view_swapped)
	{
		return;
	}

	PLAYER_ID = (UBYTE) LP_view_saved_id;

	Thing *p;

	p                 = NET_PERSON(0);
	NET_PERSON(0)     = NET_PERSON(PLAYER_ID);
	NET_PERSON(PLAYER_ID) = p;

	p                 = NET_PLAYER(0);
	NET_PLAYER(0)     = NET_PLAYER(PLAYER_ID);
	NET_PLAYER(PLAYER_ID) = p;

	LP_view_swapped = FALSE;
}

//
// ------------------------------------------------------------------------
// Camera-free replacements.
// ------------------------------------------------------------------------
//

void LANPLAY_mark_in_view(void)
{
	SLONG i;
	SLONG j;

	if (!CNET_network_game)
	{
		return;
	}

	SLONG range = LP_draw_dist << 8;	// mapsquares -> world units

	for (i = 1; i < MAX_THINGS; i++)
	{
		Thing *p_thing = TO_THING(i);

		if (p_thing->Class == CLASS_NONE)
		{
			continue;
		}

		p_thing->Flags &= ~FLAGS_IN_VIEW;

		SLONG tx = p_thing->WorldPos.X >> 8;
		SLONG tz = p_thing->WorldPos.Z >> 8;

		for (j = 0; j < NO_PLAYERS; j++)
		{
			Thing *p_player = NET_PERSON(j);

			if (p_player == NULL)
			{
				continue;
			}

			if (abs(tx - (p_player->WorldPos.X >> 8)) < range &&
				abs(tz - (p_player->WorldPos.Z >> 8)) < range)
			{
				p_thing->Flags |= FLAGS_IN_VIEW;
				break;
			}
		}
	}
}

SLONG LANPLAY_is_hidden(SLONG x, SLONG z)
{
	SLONG j;
	SLONG range = (LP_draw_dist + 3) << 8;

	for (j = 0; j < NO_PLAYERS; j++)
	{
		Thing *p_player = NET_PERSON(j);

		if (p_player == NULL)
		{
			continue;
		}

		SLONG dx = abs(x - (p_player->WorldPos.X >> 8));
		SLONG dz = abs(z - (p_player->WorldPos.Z >> 8));

		if (QDIST2(dx, dz) < range)
		{
			return FALSE;
		}
	}

	return TRUE;
}

Thing *LANPLAY_wander_focus(void)
{
	static UBYTE alternate = 0;

	if (!CNET_network_game || NO_PLAYERS < 2)
	{
		return NET_PERSON(0);
	}

	alternate ^= 1;

	Thing *p_person = NET_PERSON(alternate);

	return p_person ? p_person : NET_PERSON(0);
}

//
// ------------------------------------------------------------------------
// Status text.
// ------------------------------------------------------------------------
//

void LANPLAY_draw_status(void)
{
	CBYTE str[200];
	SLONG y = 40;

	if (LP_message[0] && LP_message_until == 0)
	{
		LP_message_until = GetTickCount() + LP_message_ms;
	}

	if (LP_message[0] && GetTickCount() < LP_message_until)
	{
		FONT2D_DrawString(LP_message, 21, y + 1, 0x000000);
		FONT2D_DrawString(LP_message, 20, y,     0xffff80);

		y += 20;
	}

	if (!CNET_network_game)
	{
		return;
	}

	if (LP_desync_turn >= 0)
	{
		sprintf(str, "LAN: gry sie rozjechaly w turze %d (kod %d)%s",
			LP_desync_turn,
			LP_desync_what,
			LP_dump_written ? " - zapisano lan_desync_P*.txt" : "");

		FONT2D_DrawString(str, 21, y + 1, 0x000000);
		FONT2D_DrawString(str, 20, y,     0xff4040);

		y += 20;
	}
}
