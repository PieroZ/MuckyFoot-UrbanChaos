//
// Grappling hook: aim at the edge of a roof, throw, climb up the rope.
//
// Getting it: pick up the grappling hook in a level, or the cheat "hak".
// On foot:
//   hold G (config.ini [Keyboard] grapple_aim) or the right mouse button - aim
//       arrows / mouse = where to throw; a green sight = the hook will catch there
//   let go - throw
// On the rope:
//   up arrow = climb, down arrow = slide down, jump = let go
//   at the top she pulls herself up onto the roof.
//
// The animations can come from PROTOTYPE_DARCI1.all (config.ini [Extra],
// numbers = animation index in that file, 0 = use D'arci's own one):
//   grapple_anim_climb, grapple_anim_hang, grapple_anim_top, grapple_anim_throw
//   grapple_climb_step = how far one loop of the climb animation goes up (default 128)
//

#ifndef _GRAPPLE_
#define _GRAPPLE_

struct Thing;

//
// Once a frame for the player on foot. Returns TRUE if it used the input
// (aiming / on the rope) - then the normal controls shouldn't get it.
//

SLONG GRAPPLE_process(struct Thing *p_person, ULONG input);

SLONG GRAPPLE_active(void);			// Aiming or on the rope
void  GRAPPLE_toggle_cheat(void);	// "hak"
void  GRAPPLE_reset(void);			// New level

void  GRAPPLE_draw_overlay(void);	// From OVERLAY_draw_gun_sights()

//
// For fc.cpp: the over-the-shoulder camera while aiming. Returns how much of
// it to use, 0..256 (0 => leave the camera alone). Positions in world units.
//

SLONG GRAPPLE_camera(SLONG *x, SLONG *y, SLONG *z, SLONG *look_x, SLONG *look_y, SLONG *look_z);

//
// For aeng.cpp: is the hook out, where is it and where does the rope start?
// (world units). taut = the rope is pulled straight.
//

SLONG GRAPPLE_get_draw(
		SLONG *hook_x, SLONG *hook_y, SLONG *hook_z,
		SLONG *hook_yaw, SLONG *hook_pitch,
		SLONG *hand_x, SLONG *hand_y, SLONG *hand_z,
		SLONG *taut);

#endif
