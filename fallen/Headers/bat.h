//
// Bats/gargoles that circle an area and swoop to attack the player.
//

#ifndef _BAT_
#define _BAT_



//
// The bat structures.
//

static const int BAT_TYPE_UNUSED = 0;
static const int BAT_TYPE_BAT = 1;
static const int BAT_TYPE_GARGOYLE = 2;
static const int BAT_TYPE_BALROG = 3;
static const int BAT_TYPE_BANE = 4;
static const int BAT_TYPE_TURRET = 6;


typedef struct
{
	UBYTE type;
	UBYTE health;
	UBYTE state;
	UBYTE substate;
	UBYTE home_x;
	UBYTE home_z;
	UWORD target;
	UWORD timer;
	UWORD flag;
	SWORD want_x;
	SWORD want_y;
	SWORD want_z;
	UWORD glow;	// How much bane glows...
	SLONG dx;
	SLONG dy;
	SLONG dz;

} Bat;

typedef Bat *BatPtr;

#define RBAT_MAX_BATS 40 //32
#define BAT_MAX_BATS  (save_table[SAVE_TABLE_BAT].Maximum)


//
// Initailises the bats.
//

void BAT_init(void);


//
// Creates a new bat thing. Returns the THING_INDEX or NULL if
// it couldn't create a bat.
//

THING_INDEX BAT_create(
				SLONG type,		// BAT_TYPE_*
				SLONG x,
				SLONG z,
				UWORD yaw);

THING_INDEX BAT_create_specify_anim(
	SLONG type,
	SLONG x,
	SLONG z,
	UWORD yaw,
	SLONG animId);



//
// The bat recieves a wound from the aggressor.
//

void BAT_apply_hit(
		Thing *p_me,
		Thing *p_aggressor,
		SLONG  damage);



void BAT_set_anim(Thing* p_thing, SLONG anim);

void BAT_set_anim_test(Thing* p_thing, SLONG anim);
void BAT_set_anim_and_type(Thing* p_thing, SLONG anim, UBYTE type);
#endif
