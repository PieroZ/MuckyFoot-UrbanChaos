//
// Security systems: sentry turrets, computer terminals (files, CCTV, turret
// control) and the EMP grenade. See Source/security.cpp.
//
// The turrets and terminals are CLASS_BAT things (BAT_TYPE_TURRET and
// BAT_TYPE_TERMINAL) drawn with the anim prim data\anim006.all: mesh 0 is the
// turret, mesh 1 the terminal (made by Tools\uc_make_security_all.py). All
// their state is kept in the Bat structure, so it is saved with the game.
//
// What a level has comes from levels\<level name>.sec (a text file, format at
// the top of security.cpp), or from the console:
//   sentry [hidden] [friendly] [off]  - a turret 1.5 mapsquares in front of you
//   terminal                          - a computer in front of you (with demo files)
//   emp [n]                           - n EMP grenades (default 3)
//   secsave                           - writes the turrets / terminals now in the level to levels\<level>.sec
//   where                             - prints where you are (mapsquares) and which way you face
//
// A terminal's turret menu has "Manual control": aim (arrows), fire (punch /
// action), lock on to the next person (jump), back (kick / Esc).
//

#ifndef _SECURITY_
#define _SECURITY_

struct Thing;

void  SEC_reset(void);								// New level (from PCOM_init)

void  SEC_bat_process(struct Thing *p_thing);		// From BAT_normal() for turrets and terminals
void  SEC_bat_hit(struct Thing *p_thing, struct Thing *p_aggressor, SLONG damage);	// From BAT_apply_hit()
SLONG SEC_bat_targetable(struct Thing *p_thing);	// FALSE => the player's aiming should ignore it

//
// Once a frame for each player on foot, from the input code. Returns TRUE if
// the computer has the controls (the person mustn't get the input).
//

SLONG SEC_player_control(struct Thing *p_person, ULONG input);

//
// Once a frame for each computer person, from PCOM_process_person(). Somebody a
// turret on the player's side (or one the player aims from a terminal) is
// shooting at shoots back at it if he has a gun, or runs away. TRUE = this
// person was dealt with (skip his usual AI this frame).
//

SLONG SEC_person_ai(struct Thing *p_person);

//
// TRUE while a terminal screen is up. SEC_escape() is the Esc key while it is
// (Game.cpp calls it before the pause menu sees the key).
//

SLONG SEC_ui_open(void);
void  SEC_escape(void);

void  SEC_draw(void);								// The terminal screen / CCTV overlay / "use computer" prompt

//
// For fc.cpp: the CCTV camera. Returns TRUE and the camera position and the
// point it looks at (world units) while a CCTV feed is being watched.
//

SLONG SEC_camera(SLONG *x, SLONG *y, SLONG *z, SLONG *look_x, SLONG *look_y, SLONG *look_z);

//
// An EMP going off (grenade.cpp). Position in world units << 8 like WorldPos.
//

void  SEC_emp_blast(SLONG x, SLONG y, SLONG z, struct Thing *p_owner);

//
// Console commands.
//

void  SEC_cmd_sentry(CBYTE *args);
void  SEC_cmd_terminal(CBYTE *args);
void  SEC_cmd_emp(CBYTE *args);
void  SEC_cmd_save(void);
void  SEC_cmd_where(void);

#endif
