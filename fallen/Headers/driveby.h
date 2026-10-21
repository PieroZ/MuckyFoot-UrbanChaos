//
// Drive-by: shooting the pistol out of the car window.
//
// While D'arci drives a car and has her pistol:
//   - Hold the RIGHT mouse button to aim: the camera moves to the car window
//     and the mouse turns it; the crosshair in the middle of the screen snaps
//     onto the tyre / person / car under it (tyres win when they're close) and
//     shows the chance to hit. LEFT mouse button shoots.
//   - Without the mouse: the nearest target in front of / beside the car gets
//     a sight automatically (green = good chance, red = poor).
//     driveby_fire  = 29 (Left Ctrl)  shoot
//     driveby_next  = 42 (Left Shift) next target
//     driveby_tyres = 58 (Caps Lock)  aim at tyres / at people and cars
//     (config.ini [Keyboard], DirectInput scan codes)
// A hit tyre bursts (see VEH_puncture_tyre).
// config.ini [Extra] driveby = 0 switches it off, driveby_mouse_speed = crosshair speed in % (100).
//

#ifndef _DRIVEBY_
#define _DRIVEBY_

struct Thing;

void  DRIVEBY_process(struct Thing *p_person);	// Once a frame while the player is driving
void  DRIVEBY_draw_overlay(void);				// From OVERLAY_draw_gun_sights()
SLONG DRIVEBY_active(void);						// TRUE while the drive-by controls are in use

// For fc.cpp: the camera at the window while aiming. Returns how much of it to
// use, 0..256 (0 => leave the camera alone). Positions in world units.
SLONG DRIVEBY_camera(SLONG *x, SLONG *y, SLONG *z, SLONG *look_x, SLONG *look_y, SLONG *look_z);

#endif
