// GMouse.cpp
// Guy Simmons, 11th December 1997.
// Modified 2025 - add click-consume logic and reset of ButtonState on UP

#include "DDLib.h"
#include <windows.h> // for WM_* and types
#include "Thing.h"
#include "free_roam_camera.h"

//---------------------------------------------------------------

volatile UBYTE    MouseMoved = 0,
LeftButton = 0,
MiddleButton = 0,
RightButton = 0;

volatile SLONG    MouseX = 0,
MouseY = 0;

volatile SLONG    MouseDX = 0, MouseDY = 0;

SLONG    OldMouseX = 0, OldMouseY = 0;

// existing structures (assumed declared in some header)
volatile LastMouse  LeftMouse = { 0,0,0,{ 0,0 } },
MiddleMouse = { 0,0,0,{ 0,0 } },
RightMouse = { 0,0,0,{ 0,0 } };

volatile MFPoint   MousePoint = { 0,0 };

volatile UBYTE LeftClickConsumed = 0;
volatile UBYTE MiddleClickConsumed = 0;
volatile UBYTE RightClickConsumed = 0;

//---------------------------------------------------------------

LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam)
{
    switch (code)
    {
    case WM_MOUSEMOVE:
        MouseX = (SLONG)LOWORD(lParam);
        MouseY = (SLONG)HIWORD(lParam);

        MouseDX = MouseX - OldMouseX;
        MouseDY = MouseY - OldMouseY;
        MousePoint.X = MouseX;
        MousePoint.Y = MouseY;
        MouseMoved = 1;

        OldMouseX = MouseX;
        OldMouseY = MouseY;
        break;

    case WM_RBUTTONUP:
        RightButton = 0;
        RightClickConsumed = 0;            // allow next click to be consumed again
        RightMouse.ButtonState = 0;        // RESET ButtonState
        break;

    case WM_RBUTTONDOWN:
        RightButton = 1;
        if (!RightMouse.ButtonState)
        {
            RightMouse.ButtonState = 1;
            RightMouse.MouseX = (SLONG)LOWORD(lParam);
            RightMouse.MouseY = (SLONG)HIWORD(lParam);
            RightMouse.MousePoint.X = (SLONG)LOWORD(lParam);
            RightMouse.MousePoint.Y = (SLONG)HIWORD(lParam);
        }
        break;

    case WM_RBUTTONDBLCLK:
        break;

    case WM_LBUTTONUP:
        LeftButton = 0;
        LeftClickConsumed = 0;             // allow next click to be consumed again
        LeftMouse.ButtonState = 0;         // RESET ButtonState
        break;

    case WM_LBUTTONDOWN:
        LeftButton = 1;
        if (!LeftMouse.ButtonState)
        {
            LeftMouse.ButtonState = 1;
            LeftMouse.MouseX = (SLONG)LOWORD(lParam);
            LeftMouse.MouseY = (SLONG)HIWORD(lParam);
            LeftMouse.MousePoint.X = (SLONG)LOWORD(lParam);
            LeftMouse.MousePoint.Y = (SLONG)HIWORD(lParam);
        }
        break;

    case WM_LBUTTONDBLCLK:
        break;

    case WM_MBUTTONUP:
        MiddleButton = 0;
        MiddleClickConsumed = 0;           // allow next click to be consumed again
        MiddleMouse.ButtonState = 0;       // RESET ButtonState 
        break;

    case WM_MBUTTONDOWN:
        MiddleButton = 1;
        if (!MiddleMouse.ButtonState)
        {
            MiddleMouse.ButtonState = 1;
            MiddleMouse.MouseX = (SLONG)LOWORD(lParam);
            MiddleMouse.MouseY = (SLONG)HIWORD(lParam);
            MiddleMouse.MousePoint.X = (SLONG)LOWORD(lParam);
            MiddleMouse.MousePoint.Y = (SLONG)HIWORD(lParam);
        }
        break;

    case WM_MBUTTONDBLCLK:
        break;
    }

    return FALSE; // or CallNextHookEx / CallWindowProc depending on how you installed proc
}

//---------------------------------------------------------------

void RecenterMouse(void)
{
    RECT client_rect;
    POINT p;

    // Don't trap the cursor when window isn't active
    if (GetForegroundWindow() != hDDLibWindow)
    {
        return;
    }

    GetWindowRect(hDDLibWindow, &client_rect);
    // Get mouse position relative to engine window.

    SetCursorPos((client_rect.left + client_rect.right) >> 1,
        (client_rect.top + client_rect.bottom) >> 1);

    p.x = (client_rect.left + client_rect.right) >> 1;
    p.y = (client_rect.top + client_rect.bottom) >> 1;

    ScreenToClient(hDDLibWindow, &p);

    OldMouseX = p.x;
    OldMouseY = p.y;
}

#include "sound_id.h"
#include "config_extras.h"


void HandleMouseInput(Thing* target)
{
    static SLONG mouse_shoot_cooldown = 0;
    if (mouse_shoot_cooldown > 0)
    {
        mouse_shoot_cooldown--;
    }
    if (LeftButton && !LeftClickConsumed)
    {
        LeftClickConsumed = 1;
        Thing* darci = NET_PERSON(0);
        extern SLONG person_has_gun_out(Thing * p_person);
        if (darci &&
           // target &&
            mouse_shoot_cooldown == 0 &&
            person_has_gun_out(darci))
        {
            //darci->Genus.Person->Target = THING_NUMBER(target);
            //set_person_shoot(darci, 1);
            extern void set_person_manual_shoot(Thing * p_person);
            darci->Genus.Person->Target = target ? THING_NUMBER(target) : 0;
            set_person_manual_shoot(darci);

            mouse_shoot_cooldown = ConfigExtras::getInstance().mMouseShootCooldown;

            //if (target->Class == CLASS_VEHICLE)
            //{
            //    target->Genus.Vehicle->Siren = 1;
            //}
        }
   //     LeftClickConsumed = 1;
   //     Thing* darci = NET_PERSON(0);
   //     if (target && target->Class == CLASS_VEHICLE)
   //     {
   //         extern void VEH_reduce_health(Thing * p_car, Thing * p_person, SLONG damage);
   //         Thing* darci = NET_PERSON(0);
   //         VEH_reduce_health(target, darci, 5);
   //         target->Genus.Vehicle->Siren = 1;
   //     }
   //     else if (target && target->Class == CLASS_PERSON)
   //     {
   //         SWORD dmg = 5;

   //         extern void	actually_fire_gun(Thing * p_person);
			//darci->Genus.Person->Target = THING_NUMBER(target);
   //         actually_fire_gun(darci);
   //         extern void MFX_play_thing(UWORD channel_id, ULONG wave, ULONG flags, Thing * p);

   //         MFX_play_thing(THING_NUMBER(target), S_PISTOL_SHOT, 0, target);

   //         //target->Genus.Person->Health -= dmg;
   //     }
   //     else if (target && target->Class == CLASS_BARREL)
   //     {
   //         
   //         BARREL_shoot(target, darci);
   //         set_person_shoot(darci, 0);
   //     }
    }

    // MIDDLE CLICK example (optional) - also single-fire
    if (MiddleButton && !MiddleClickConsumed)
    {
        MiddleClickConsumed = 1;
        // TODO: middle mouse button
    }

    // RIGHT CLICK toggles the free-roam camera
    if (RightButton && !RightClickConsumed)
    {
        RightClickConsumed = 1;
		Thing* darci = NET_PERSON(0);
        FreeRoamCamera& frc = FreeRoamCamera::GetInstance();
        frc.CopyFromFC(&FC_cam[0]);
        frc.IsActive = !frc.IsActive;

        frc.PositionX = darci->WorldPos.X;
        frc.PositionY = darci->WorldPos.Y + (96 << 8);
        frc.PositionZ = darci->WorldPos.Z;

        if (frc.IsActive)
        {
            frc.InitFreeRoamTargetsFromCurrent();
        }
    }
}

//---------------------------------------------------------------
