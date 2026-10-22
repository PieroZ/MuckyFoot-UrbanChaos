//
// lanplay.h
//
// Two-player LAN game in lockstep.
//
// Both machines run the whole simulation. Every game turn each machine sends
// its own player's input to the other one, and nothing moves on until both
// inputs for that turn are known. So the two games stay identical as long as
// the simulation only depends on those inputs.
//
// Settings (config.ini, section [Network]):
//
//   lan_mode    = 0 off, 1 host (wait for the other PC), 2 join (connect to lan_address)
//   lan_address = 192.168.1.10   (only for joining)
//   lan_port    = 27015
//   lan_fps     = 30             (game turns per second, the host's value is used)
//   lan_delay   = 1              (turns of input delay, the host's value is used)
//
// Or from the command line: "-host"  /  "-join 192.168.1.10"  /  "-port 27015"
//
// The host picks the mission; whatever the joining PC picked is replaced by
// the host's mission.
//

#ifndef LANPLAY_H
#define LANPLAY_H

typedef struct
{
	ULONG input;		// The usual INPUT_MASK_* word from get_hardware_input().
	SWORD mouse_dx;
	SWORD mouse_dy;
	SWORD cam_yaw;		// That player's camera, for camera-relative controls (0..2047).
	SWORD cam_pitch;	// FC_Cam pitch >> 8
	UBYTE weapon_key;	// 0 = none, 1..8 = number key pressed this turn.
	UBYTE flags;		// LANPLAY_FLAG_*
	UWORD pad;			// From the host: how long its last frame took (ms).

} LANPLAY_Input;

#define LANPLAY_FLAG_SHIFT			(1 << 0)
#define LANPLAY_FLAG_FIRST_PERSON	(1 << 1)	// The first-person-look key is held.
#define LANPLAY_FLAG_MOUSE_CFG		(1 << 2)	// [Extra] mouse_input=1 on that PC

//
// The inputs both players use this turn (index = player id).
//

extern LANPLAY_Input LANPLAY_in[2];

//
// Starting and stopping.
//

SLONG LANPLAY_requested   (void);	// Has the user asked for a LAN game?
SLONG LANPLAY_setup       (void);	// Before loading the level: connect + handshake. TRUE => network game.
void  LANPLAY_seed_for_loading(void);	// Same random numbers on both PCs while loading.
void  LANPLAY_ensure_players(void);	// While loading: make sure there's a second player.
void  LANPLAY_level_loaded(void);	// After loading: wait for the other PC, sync the random seeds.
void  LANPLAY_finish      (void);	// Leaving the level.

//
// Every game turn, from do_packets().
//

void  LANPLAY_do_packets(void);

SLONG LANPLAY_tick_ms(void);		// Length of the next game turn (ms) - the same on both PCs.
ULONG LANPLAY_game_ms(void);		// Game time so far (ms) - the same on both PCs.
SLONG LANPLAY_fps    (void);

//
// While a player's input is being processed: puts that player's mouse, number
// keys, shift etc. into the globals the game code reads, and back afterwards.
//

void                 LANPLAY_begin_player(SLONG player_id);
void                 LANPLAY_end_player  (void);
const LANPLAY_Input *LANPLAY_current     (void);	// NULL when no player is being processed.

//
// Drawing: makes this PC's player be NET_PERSON(0) / PLAYER_ID 0 so the panel,
// map, sound etc. all follow the local player.
//

void  LANPLAY_view_begin(void);
void  LANPLAY_view_end  (void);

//
// Things that used to depend on the local camera, made the same on both PCs.
//

void   LANPLAY_mark_in_view     (void);				// FLAGS_IN_VIEW from distance to the players.
SLONG  LANPLAY_sim_draw_distance(void);				// In mapsquares, same on both PCs.
SLONG  LANPLAY_is_hidden        (SLONG x, SLONG z);	// World coords: far from every player?
struct Thing *LANPLAY_wander_focus(void);			// Who to put wandering people around.

//
// Random numbers: the simulation one (replaces rand() in game logic) and a
// cosmetic one (replaces Random() in drawing code so the view can't change
// the simulation's random sequence).
//

SLONG LANPLAY_sim_rand     (void);
UWORD LANPLAY_cosmetic_rand(void);

//
// Status text (waiting / disconnected / out of sync).
//

void  LANPLAY_draw_status(void);

#endif
