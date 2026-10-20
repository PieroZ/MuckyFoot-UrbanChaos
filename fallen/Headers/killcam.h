//
// Killcam: when the player takes down the last enemy who was fighting him,
// the game goes into slow motion and the camera swings around the defeated
// enemy (Max Payne style), then everything returns to normal.
//
// On/off: config.ini [Extra] killcam=1 / 0, or the console command "killcam".
//

#ifndef _KILLCAM_
#define _KILLCAM_

struct Thing;

//
// Called by set_person_dead() (deaths and knock-outs) before anything changes.
//

void KILLCAM_person_defeated(struct Thing *p_victim, struct Thing *p_aggressor, SLONG death_type);

//
// Called once per frame with the real time that passed. Returns how fast the
// game should run (256 = normal speed).
//

SLONG KILLCAM_process(SLONG real_millisecs);

//
// For the camera (fc.cpp).
//

SLONG KILLCAM_active(void);
UWORD KILLCAM_victim(void);		// THING_INDEX of the defeated enemy
SLONG KILLCAM_progress(void);	// 0 .. 256 through the effect
SLONG KILLCAM_blend(void);		// 0 .. 256 how much the killcam camera / slow motion is in effect

void KILLCAM_toggle(void);		// Console command
void KILLCAM_reset(void);		// New level

#endif
