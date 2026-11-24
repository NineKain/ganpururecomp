/* Gunman's Proof -- F1 settings menu (pixel-art, game-owned).
 * Hooks into SnesDesktopHostGame.menu_*; see src/gun_menu.c. */
#pragma once
#include <stdint.h>
#include "desktop/sdl_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

void GunMenu_Open(void);
int  GunMenu_IsOpen(void);
void GunMenu_Event(const SDL_Event *ev, int pad_button);
const uint32_t *GunMenu_Panel(int *w, int *h);

/* Aspect ratio setting, shared with the presenter in main.c. */
enum { kGunAspect_Auto, kGunAspect_4x3, kGunAspect_16x9, kGunAspect_16x10,
       kGunAspect_21x9, kGunAspect_Count };
int    GunMenu_PadFeedback(void);     /* 0 off, 1 subtle rumble */
int    GunMenu_BlackEdges(void);      /* SIDE EDGES = BLACK: original 4:3 picture only */
void   GunMenu_ApplyEdges(void);      /* per-frame widescreen margin policy */
int    GunMenu_AspectMode(void);      /* effective mode (4x3 when widescreen is off) */
double GunMenu_FixedAspect(void);     /* displayed w/h of a fixed mode; 0 for Auto */
int    GunMenu_WindowBaseWidth(void);
int    GunMenu_WideActors(void);      /* draw actors in the margins */
int    GunMenu_Glow(void);            /* 0 off, 1 soft, 2 strong */
int    GunMenu_RoomFades(void);       /* 0 normal, 1 fast, 2 faster */
int    GunMenu_TextSpeed(void);       /* 0 normal, 1 x2, 2 x3, 3 instant */
int    GunMenu_SkipIntro(void);       /* Start skips the opening cutscene */
int    GunMenu_HudCorners(void);      /* pin the BG3 HUD to the frame corner */
int    GunMenu_ViewSlide(void);       /* slide the view at room edges */ /* window width at scale 1 (height is 224) */

#ifdef __cplusplus
}
#endif
