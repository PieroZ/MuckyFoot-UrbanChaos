#include "PersonPZI.h"
#include	"Game.h"
#include	"Cop.h"
#include	"cam.h"
#include	"Darci.h"
#include	"Roper.h"
#include	"Thug.h"
#include	"id.h"
#include	"statedef.h"
#include	"animate.h"
#include	"combat.h"
#include	"sample.h"
#include	"guns.h"
#include	"cnet.h"
#include	"interfac.h"
//#include	"command.h"
#include	"sewer.h"
#include	"mav.h"
#include	"Sound.h"
#include	"eway.h"
#include	"spark.h"
#include	"drip.h"
#include	"puddle.h"
#include	"pap.h"
#include	"supermap.h"
#include	"ns.h"
#include	"dirt.h"
#include	"hook.h"
#include	"pcom.h"
#include	"tracks.h"
#include	"..\DDEngine\Headers\Matrix.h"
#include	"ob.h"
#include	"wmove.h"
#include	"balloon.h"
#include	"inside2.h"
#include	"walkable.h"
#include	"overlay.h"
#include	"psystem.h"
#include	"poly.h"
#include	"memory.h"
#include	"fmatrix.h"
#include	"fc.h"
#include	"mfx.h"
#include	"night.h"
#include	"ware.h"
#include	"xlat_str.h"
#include	"pow.h"
#include	"frontend.h"
#include "DebugVars.h"
#ifdef		PSX 
#include "c:\fallen\psxeng\headers\psxeng.h"
#include "c:\fallen\psxeng\headers\panel.h"
#else
#include "aeng.h"
#include "panel.h"
#endif

void lighting_strike_person(Thing* p_person)
{
	SLONG px;
	SLONG py;
	SLONG pz;

	calc_sub_objects_position(
		p_person,
		p_person->Draw.Tweened->AnimTween,
		SUB_OBJECT_PELVIS,
		&px,
		&py,
		&pz);

	//px = p_person->WorldPos.X;
	//py = p_person->WorldPos.Y;
	//pz = p_person->WorldPos.Z;

	px <<= 8;
	py <<= 8;
	pz <<= 8;

	px += p_person->WorldPos.X;
	py += p_person->WorldPos.Y;
	pz += p_person->WorldPos.Z;

	//
	// Make this person blow up!
	//

				POW_create(
					POW_CREATE_LARGE_SEMI,
					px,
					py,
					pz,0,0,0);

				//POW_create(
				//	POW_CREATE_LARGE_SEMI,
				//	p_mine->WorldPos.X,
				//	p_mine->WorldPos.Y,
				//	p_mine->WorldPos.Z,
				//	0, 0, 0);
				PYRO_create(p_person->WorldPos, PYRO_DUSTWAVE);

					//
					// Massive lightning strike
					//

				return;
	SPARK_Pinfo p1;
	SPARK_Pinfo p2;
	UBYTE i;

	PARTICLE_Add(px, py, pz, 0, 0, 0, POLY_PAGE_BLOOM1, 0, 0xffFFFFFF, PFLAG_FADE, 40, 255, 1, -5, 0);

	for (i = 0; i < 20; i++)
	{
		PARTICLE_Add(px + ((Random() & 0x9ff) - 0x4ff),
			py + ((Random() & 0x9ff) - 0x4ff),
			pz + ((Random() & 0x9ff) - 0x4ff),
			(Random() & 0x1ff) - 0xff, 256 + (Random() & 0x3ff), (Random() & 0x1ff) - 0x7f,
			POLY_PAGE_SMOKECLOUD2, 2 + ((Random() & 3) << 2), 0x7F7fFFFF,
			PFLAG_SPRITEANI | PFLAG_SPRITELOOP | PFLAG_FADE | PFLAG_RESIZE,
			300, 70, 1, 2, 2);
		DIRT_new_sparks(px >> 8, py >> 8, pz >> 8, 2);
	}

	//				MFX_play_ambient(0,S_THUNDER_END-(Random()&1),0);
	MFX_stop(THING_NUMBER(p_person), S_MIB_LEVITATE);
	MFX_play_thing(THING_NUMBER(p_person), S_MIB_EXPLODE, 0, p_person);

	px >>= 8; py >>= 8; pz >>= 8;


	for (i = 0; i < 10; i++)
	{
		p1.type = SPARK_TYPE_POINT;
		p1.flag = 0;
		p1.person = 0;
		p1.limb = 0;
		p1.x = px + (Random() & 0x3f) - 0x1f;
		p1.y = py + 1000;
		p1.z = pz + (Random() & 0x3f) - 0x1f;
		p2.type = SPARK_TYPE_POINT;
		p2.flag = 0;
		p2.person = 0;
		p2.limb = 0;
		p2.x = px + (Random() & 0x3f) - 0x1f;
		p2.y = PAP_calc_map_height_at(px, pz);
		p2.z = pz + (Random() & 0x3f) - 0x1f;

		SPARK_create(
			&p1,
			&p2,
			25 + (Random() & 0xf));
		DIRT_new_sparks(p2.x, p2.y, p2.z, 2);

	}

	create_shockwave(
		px,
		py,
		pz,
		0x300,
		200,
		p_person);

	DIRT_gust(p_person, px, pz, px + 400, pz);

	remove_thing_from_map(p_person);
}
