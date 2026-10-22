//
// Killcam - slow motion and an orbiting camera when the last enemy goes down.
//

#include "game.h"
#include "statedef.h"
#include "pcom.h"
#include "eway.h"
#include "mfx.h"
#include "killcam.h"
#include "cnet.h"
#include "config_extras.h"
#include "..\DDEngine\Headers\console.h"

#define KILLCAM_DURATION		2800	// Real milliseconds
#define KILLCAM_BLEND_IN		200
#define KILLCAM_BLEND_OUT		700
#define KILLCAM_COOLDOWN		1500
#define KILLCAM_SLOWEST			52		// Game speed at the slowest point (256 = normal) => 20%
#define KILLCAM_ENEMY_RADIUS	0x1800	// Look for other enemies this far from the player (world units)

extern SLONG is_person_ko(Thing* p_person);

static struct
{
	SLONG       active;
	THING_INDEX victim;
	SLONG       time;		// Real milliseconds since it started
	SLONG       cooldown;	// Real milliseconds until it may start again
	SLONG       pitched;	// The sound pitch has been changed

} KILLCAM;

//
// Bullet time on its own (no camera swing): D'arci jumping off a bike.
//

static SLONG KILLCAM_bt_left;	// Real milliseconds
static SLONG KILLCAM_bt_total;

#define KILLCAM_BT_SLOWEST	80	// 256 = normal => about 30%

void KILLCAM_bullet_time(SLONG ms)
{
	if (CNET_network_game)
	{
		return;	// Slow motion would have to happen on both PCs at once.
	}

	KILLCAM_bt_left  = ms;
	KILLCAM_bt_total = ms;
}


void KILLCAM_reset(void)
{
	if (KILLCAM.pitched)
	{
		MFX_set_global_pitch(1.0F);
	}

	memset(&KILLCAM, 0, sizeof(KILLCAM));

	KILLCAM_bt_left = 0;
}

void KILLCAM_toggle(void)
{
	ConfigExtras& extras = ConfigExtras::getInstance();

	extras.mKillCam = !extras.mKillCam;

	ENV_set_value_number("killcam", extras.mKillCam ? 1 : 0, "Extra");

	if (!extras.mKillCam)
	{
		KILLCAM_reset();
	}

	CONSOLE_text(extras.mKillCam ? "Killcam on" : "Killcam off", 3000);
}

SLONG KILLCAM_active(void)
{
	return KILLCAM.active;
}

UWORD KILLCAM_victim(void)
{
	return KILLCAM.victim;
}

SLONG KILLCAM_progress(void)
{
	SLONG t = (KILLCAM.time << 8) / KILLCAM_DURATION;

	SATURATE(t, 0, 256);

	return t;
}

SLONG KILLCAM_blend(void)
{
	if (!KILLCAM.active)
	{
		return 0;
	}

	SLONG in = (KILLCAM.time << 8) / KILLCAM_BLEND_IN;
	SLONG out = ((KILLCAM_DURATION - KILLCAM.time) << 8) / KILLCAM_BLEND_OUT;
	SLONG blend = 256;

	if (blend > in) { blend = in; }
	if (blend > out) { blend = out; }

	SATURATE(blend, 0, 256);

	return blend;
}

//
// Is this person still fighting the player?
//

static SLONG KILLCAM_is_after(Thing* p_person, THING_INDEX player)
{
	if (p_person->State == STATE_DEAD || p_person->State == STATE_DYING)
	{
		return FALSE;
	}

	if (p_person->Genus.Person->Flags & FLAG_PERSON_KO)
	{
		return FALSE;
	}

	if (PCOM_person_wants_to_kill(p_person) == player)
	{
		return TRUE;
	}

	if (p_person->Genus.Person->Target == player && p_person->Genus.Person->Mode == PERSON_MODE_FIGHT)
	{
		return TRUE;
	}

	return FALSE;
}

void KILLCAM_person_defeated(Thing* p_victim, Thing* p_aggressor, SLONG death_type)
{
	SLONG i;

	if (!ConfigExtras::getInstance().mKillCam || KILLCAM.active || KILLCAM.cooldown > 0)
	{
		return;
	}

	if (death_type == PERSON_DEATH_TYPE_GET_DOWN)
	{
		//
		// Lying down with your hands on your head is not 'defeated'.
		//

		return;
	}

	if (p_victim == NULL || p_victim->Class != CLASS_PERSON || p_victim->Genus.Person->PlayerID)
	{
		return;
	}

	if (p_victim->State == STATE_DEAD || (p_victim->State == STATE_DYING && !is_person_ko(p_victim)))
	{
		//
		// Already down.
		//

		return;
	}

	Thing* p_player = NET_PERSON(0);

	if (p_player == NULL || p_player == p_victim)
	{
		return;
	}

	if (p_player->State == STATE_DEAD || p_player->State == STATE_DYING || EWAY_stop_player_moving())
	{
		return;
	}

	THING_INDEX player = THING_NUMBER(p_player);

	//
	// Was the victim fighting the player?
	//

	if (PCOM_person_wants_to_kill(p_victim) != player && p_victim->Genus.Person->Target != player)
	{
		return;
	}

	//
	// Did the player do it?
	//

	if (p_aggressor)
	{
		SLONG by_player = FALSE;

		if (p_aggressor == p_player)
		{
			by_player = TRUE;
		}
		else
		if (p_aggressor->Class == CLASS_PERSON && p_aggressor->Genus.Person->PlayerID)
		{
			by_player = TRUE;
		}
		else
		if (p_aggressor->Class == CLASS_BAT && BAT_player_active())
		{
			by_player = TRUE;	// The player's Balrog
		}

		if (!by_player)
		{
			return;
		}
	}
	else
	{
		//
		// Explosions and the like don't tell us who did it. Count it if it
		// happened near the player.
		//

		SLONG dx = abs(p_victim->WorldPos.X - p_player->WorldPos.X) >> 8;
		SLONG dz = abs(p_victim->WorldPos.Z - p_player->WorldPos.Z) >> 8;

		if (QDIST2(dx, dz) > 0x1000)
		{
			return;
		}
	}

	//
	// Is anybody else still after the player?
	//

	THING_INDEX found[64];

	SLONG num = THING_find_sphere(
		p_player->WorldPos.X >> 8,
		p_player->WorldPos.Y >> 8,
		p_player->WorldPos.Z >> 8,
		KILLCAM_ENEMY_RADIUS,
		found,
		64,
		1 << CLASS_PERSON);

	for (i = 0; i < num; i++)
	{
		Thing* p_found = TO_THING(found[i]);

		if (p_found == p_victim || p_found == p_player)
		{
			continue;
		}

		if (KILLCAM_is_after(p_found, player))
		{
			return;
		}
	}

	//
	// That was the last one!
	//

	KILLCAM.active = TRUE;
	KILLCAM.victim = THING_NUMBER(p_victim);
	KILLCAM.time = 0;
}

SLONG KILLCAM_process(SLONG real_millisecs)
{
	if (KILLCAM.cooldown > 0)
	{
		KILLCAM.cooldown -= real_millisecs;
	}

	if (!KILLCAM.active)
	{
		if (KILLCAM_bt_left > 0)
		{
			//
			// Bullet time: in quickly, out slowly.
			//

			SLONG done  = KILLCAM_bt_total - KILLCAM_bt_left;
			SLONG in    = (done << 8) / 150;
			SLONG out   = (KILLCAM_bt_left << 8) / 500;
			SLONG blend = 256;

			if (blend > in)  {blend = in;}
			if (blend > out) {blend = out;}

			SATURATE(blend, 0, 256);

			KILLCAM_bt_left -= real_millisecs;

			SLONG speed = 256 - ((256 - KILLCAM_BT_SLOWEST) * blend >> 8);

			MFX_set_global_pitch(0.55F + 0.45F * float(speed) / 256.0F);
			KILLCAM.pitched = TRUE;

			return speed;
		}

		if (KILLCAM.pitched)
		{
			MFX_set_global_pitch(1.0F);
			KILLCAM.pitched = FALSE;
		}

		return 256;
	}

	if (!KILLCAM.victim ||
		TO_THING(KILLCAM.victim)->Class != CLASS_PERSON ||
		EWAY_stop_player_moving())
	{
		//
		// The victim has gone or a cutscene has started.
		//

		KILLCAM.active = FALSE;
		KILLCAM.cooldown = KILLCAM_COOLDOWN;

		return KILLCAM_process(0);
	}

	KILLCAM.time += real_millisecs;

	if (KILLCAM.time >= KILLCAM_DURATION)
	{
		KILLCAM.active = FALSE;
		KILLCAM.cooldown = KILLCAM_COOLDOWN;

		return KILLCAM_process(0);
	}

	SLONG blend = KILLCAM_blend();
	SLONG speed = 256 - ((256 - KILLCAM_SLOWEST) * blend >> 8);

	//
	// Slowed-down sound effects too (music keeps going).
	//

	MFX_set_global_pitch(0.55F + 0.45F * float(speed) / 256.0F);
	KILLCAM.pitched = TRUE;

	return speed;
}
