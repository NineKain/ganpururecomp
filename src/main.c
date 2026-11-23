/*
 * Gunmans Proof — desktop host shim.
 *
 * Identity and hooks, nothing else. The host itself is the framework's
 * (snesrecomp/runner/src/desktop/host_main.h): the pre-boot launcher, ROM
 * resolution and digest checks, config.ini and keybinds.ini, the window and
 * the SDL / OpenGL presenters, audio, gamepads, the in-game save-state
 * browser and rewind filmstrip, the OSD, the pacing clock, crash handlers and
 * the post-mortem report, mod packages, Generate & rebuild, and the netplay
 * barrier when the project is built with it. A fix there reaches this
 * project on a submodule pull; nothing in this file needs to change for it.
 *
 * This file used to BE the host -- a ~700-line copy made at scaffold time --
 * so every project scaffolded before a fix kept the bug. Two ports of the
 * same game, a week apart, had different hosts.
 *
 * Nothing here ever ships a ROM. The player supplies one; the host makes
 * that easy (launcher, then positional argument, then a copy beside the
 * executable, then the rom.cfg cache, then a file picker) and checks what it
 * is handed against the digests in rom_identity.txt.
 *
 * Grow this file only with what is specific to THIS title: a custom
 * presenter (prepare_frame / draw_frame), an SPC player, a pacing rule
 * (keep_pacing_debt), a Mods provider. See SnesDesktopHostGame for the
 * complete list of hooks and what each is for.
 */

#include "host_main.h"
#include "game_rtl.h"
#include "desktop/config.h"
#include "snes/ppu.h"
#include "desktop/display_aspect.h"
#include "gun_menu.h"
#include "common_rtl.h"
#include "snes/interp_bridge.h"
#include "snes_overlay_draw.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "snesrecomp_rom_identity.h"  /* generated from rom_identity.txt */
#include "recomp_launcher.h"          /* RecompLauncherCGameInfo (launcher window icon) */

#ifndef __ANDROID__
#define SDL_MAIN_HANDLED 1
#endif
#include "desktop/sdl_compat.h"
#ifdef __ANDROID__
/* SDLActivity loads libmain.so and dlsyms its SDL_main. */
#define main SDL_main
#endif

#ifndef SNES_GAME_VERSION
#define SNES_GAME_VERSION "dev"
#endif

/* Widescreen: the PPU renders genuine extra columns on each side (no
 * stretching). Aspect ratio comes from the Insert menu (src/gun_menu.c):
 *   Auto    frame width follows the window's aspect, square pixels
 *   4:3     the original 256-wide frame shown at the CRT's 4:3
 *   16:9 / 16:10 / 21:9   a fixed frame, letterboxed in any other window
 * The height stays the native 224 lines; the width is capped at the PPU's
 * maximum field. */
static void GunPrepareFrame(int dw, int dh, int *w, int *h)
{
    *h = 224;
    *w = 256;
    int mode = GunMenu_AspectMode();
    if (mode == kGunAspect_4x3 || dw <= 0 || dh <= 0)
        return;
    double a = GunMenu_FixedAspect();
    if (a <= 0.0) a = (double)dw / (double)dh;
    int fw = (int)(a * 224.0 + 0.5);
    fw = (fw / 2) * 2;
    if (fw < 256) fw = 256;
    if (fw > kPpuMaxFrameWidth) fw = kPpuMaxFrameWidth;
    *w = fw;
}

/* Fit the displayed picture in the window, preserving its aspect. Pixels are
 * square except in 4:3 mode, which restores the original 4:3 shape. */
static void GunComputeViewport(int fw, int fh, int dw, int dh, SnesDisplayViewport *vp)
{
    double disp = GunMenu_AspectMode() == kGunAspect_4x3 ? 4.0 / 3.0 : (double)fw / fh;
    double sc_x = dw / (disp * fh), sc_y = (double)dh / fh;
    double sc = sc_x < sc_y ? sc_x : sc_y;
    vp->height = (int)(fh * sc + 0.5);
    vp->width  = (int)(disp * fh * sc + 0.5);
    vp->x = (dw - vp->width) / 2;
    vp->y = (dh - vp->height) / 2;
}
/* Real widescreen: each room is a wrapping 512px tilemap and the camera
 * (WRAM $0A60) is clamped to [$0B2A,$0B28]. Beyond those bounds the tilemap
 * holds wrapped, unrelated tiles, so the extra columns are only shown where
 * the room really continues; the view anchor then slides the picture so the
 * unused margin becomes room on the other side. */
/* Actor cull window. The sprite builder ($86:8396/82D1/8615) only emits an
 * actor whose screen X is in [-32,255]; widen it to the live margins. Sprites
 * in the right margin set the OAM X9 bit like left ones do: the PPU decodes
 * raw X in [256,256+right) as the right margin and the rest as negative. */
static int g_wsL, g_wsR;
typedef struct { uint32_t pc, on, x9, cull; } GunCullSite;
static const GunCullSite kCull[] = {
    {0x86839E, 0x8683B0, 0x868458, 0x8683AB},
    {0x8682D9, 0x8682E8, 0x868325, 0x8682E3},
    {0x86861D, 0x86862C, 0x868695, 0x868627},
    /* $87 shot/effect sprites: carry out = X9, so the hook sets C as well. */
    {0x878DF9, 0x878E0E, 0x878E1A, 0x878E03},
    {0x878E33, 0x878E47, 0x878E52, 0x878E3D},
    {0x878E6A, 0x878E7E, 0x878E89, 0x878E74},
    {0x878EA1, 0x878EB5, 0x878EC0, 0x878EAB},
};
static void GunCullHook(CpuState *cpu, uint32_t pc)
{
    for (unsigned i = 0; i < sizeof kCull / sizeof kCull[0]; i++) {
        if (kCull[i].pc != pc) continue;
        int x = (int16_t)cpu->A;
        uint32_t t;
        if ((uint16_t)x < 0x100) t = kCull[i].on;
        else if (x >= 0 ? x < 256 + g_wsR + 16 : x >= -(32 + g_wsL + 16)) t = kCull[i].x9;
        else t = kCull[i].cull;
        cpu->_flag_C = (t == kCull[i].x9);
        interp_bridge_pre_opcode_redirect(t);
        return;
    }
}
static inline int GunW(int a) { return g_ram[a] | (g_ram[a + 1] << 8); }
/* In real room gameplay (not paused, in a menu, on the map or in a cutscene):
 * the scroll shadows follow the camera and the camera sits inside the room
 * bounds. Everything else keeps the authentic centred 256px picture. */
/* Pause menu: game state 1, sub-states 0x16..0x1A, with the room camera still
 * valid behind it. Gets the same widescreen margins as gameplay. */
static int GunIsPause(void)
{
    int cam = GunW(0x0A60), lo = GunW(0x0B2A), hi = GunW(0x0B28), sub = g_ram[0x0104];
    return g_ram[0x0106] == 1 && sub >= 0x16 && sub <= 0x1A &&
           GunW(0x0120) == cam && GunW(0x0122) == GunW(0x0A62) &&
           hi >= lo && hi - lo <= 0x400 && cam >= lo && cam <= hi;
}
static int GunIsPlay(void)
{
    int cam = GunW(0x0A60), lo = GunW(0x0B2A), hi = GunW(0x0B28);
    return g_ram[0x0AFC] == 0 && g_ram[0x016E] <= 1 &&
           GunW(0x0120) == cam && GunW(0x0122) == GunW(0x0A62) &&
           hi >= lo && hi - lo <= 0x400 && cam >= lo && cam <= hi;
}

/* Skip intro: pressing Start during the opening story cutscene enters the
 * game's own first-stage setup routine instead (nothing is recorded or shipped;
 * the game builds the room from the player's ROM). */
static volatile int g_skip_pending;
/* Runs at the top of the game's main loop (84:8100). When a skip was requested,
 * call the game's own "start first stage" routine (84:8F18, the same one the
 * story script calls when the cutscene ends) as a subroutine of the loop. */
static void GunSkipHook(CpuState *c, uint32_t pc)
{
    (void)pc;
    if (!g_skip_pending) return;
    g_skip_pending = 0;
    g_ram[0x0A60] = 0x00; g_ram[0x0A61] = 0x0F; g_ram[0x0A62] = 0x31; g_ram[0x0A63] = 0x0D;   /* start screen */
    g_ram[0x0A8C] = 0xFF;
    g_ram[0x0BF8] = 7; g_ram[0x0BFA] = 7;                       /* first-stage spawn list */
    g_ram[0x0B20] = 0x9C; g_ram[0x0B21] = 0x0F; g_ram[0x0B22] = 0x99; g_ram[0x0B23] = 0x0D; /* start position */
    g_ram[(c->S) & 0x1FFF] = 0x80; g_ram[(c->S - 1) & 0x1FFF] = 0xFF;   /* return to 84:8100 */
    c->S -= 2; c->DB = 0x84; c->D = 0;
    interp_bridge_pre_opcode_redirect(0x848F18);
    fprintf(stderr, "[skip-intro] entered first-stage setup\n");
}
/* ---- Text speed -------------------------------------------------------
 * The game's dialogue printer is a per-frame task (88:92E4). To print faster
 * we let it run extra "logical frames" before it yields, running the game's
 * own V-blank uploader (84:88D8) after each one so glyph tiles reach VRAM. */
static int g_tx_active, g_tx_phase, g_tx_left;
static void GunTxEnter(CpuState *c, uint32_t pc)
{
    (void)c; (void)pc;
    static const int kExtra[4] = { 0, 1, 2, 400 };
    g_tx_active = 1; g_tx_phase = 0;
    g_tx_left = kExtra[GunMenu_TextSpeed() & 3];
}
static void GunTxYield(CpuState *c, uint32_t pc)
{
    (void)pc;
    if (!g_tx_active) return;
    if (g_tx_phase == 0) {
        int x = g_ram[0x0A72];
        int resume = GunW(0x0620 + x);
        if (g_tx_left <= 0 || resume != 0x92E4) { g_tx_active = 0; return; }
        g_tx_phase = 1;
        g_ram[(c->S) & 0x1FFF] = 0x80; g_ram[(c->S - 1) & 0x1FFF] = 0x07;   /* RTS -> 84:8008 */
        c->S -= 2;
        interp_bridge_pre_opcode_redirect(0x8488D8);
    } else {
        g_tx_phase = 0; g_tx_left--;
        interp_bridge_pre_opcode_redirect(0x8892E7);
    }
}
static uint32_t GunExtraInputs(uint32_t human)
{
    static int prev, play_run, been_play;
    int start = (human & SNES_PAD_START) != 0;
    int edge = start && !prev;
    prev = start;
    int play = GunIsPlay();
    play_run = play ? play_run + 1 : 0;
    if (play_run > 60) been_play = 1;      /* later cutscenes must never be skipped */
    /* Opening story cutscene only: game state 3, no room bounds yet, and the
     * first stage has not been reached since the game started. */
    int story = !been_play && !play && g_ram[0x0106] == 3 && GunW(0x0A60) != 0 &&
                GunW(0x0B2A) == 0 && GunW(0x0B28) == 0;
    if (edge && story && GunMenu_SkipIntro()) g_skip_pending = 1;
    return 0;
}
/* ---- Room fades --------------------------------------------------------
 * A room change is: fade-out (1 brightness step/frame, state $0104=0x0E),
 * forced-blank loading (0x0F, ~50 frames of waiting on V-blank), a timed black
 * hold (0x14, counter $0CD8), then fade-in (0x12). Bigger fade steps and a
 * shorter hold shorten it; the loading frames are invisible (forced blank), so
 * they are fast-forwarded silently. Nothing about the game's logic changes. */
static void GunFadeOutHook(CpuState *c, uint32_t pc)
{
    (void)c; (void)pc;
    int sp = GunMenu_RoomFades(); if (!sp) return;
    int step = sp == 1 ? 3 : 7, b = g_ram[0x0140];
    if (!(b & 0x80) && b > 1) { int nb = b - (step - 1); g_ram[0x0140] = (uint8_t)(nb < 1 ? 1 : nb); }
}
static void GunFadeInHook(CpuState *c, uint32_t pc)
{
    (void)c; (void)pc;
    int sp = GunMenu_RoomFades(); if (!sp) return;
    int step = sp == 1 ? 3 : 7, b = g_ram[0x0140];
    if (!(b & 0x80) && b < 0x0F) { int nb = b + (step - 1); g_ram[0x0140] = (uint8_t)(nb > 0x0E ? 0x0E : nb); }
}
static void GunHoldHook(CpuState *c, uint32_t pc)
{
    (void)c; (void)pc;
    int sp = GunMenu_RoomFades(); if (!sp) return;
    int cap = sp == 1 ? 12 : 2;
    if (g_ram[0x0CD8] > cap) g_ram[0x0CD8] = (uint8_t)cap;
}
static int GunWantTurbo(void)
{
    /* Only while nothing is visible: forced blank (0x80) or fully black (0). */
    if (GunMenu_RoomFades() && g_ram[0x0106] == 0 && (g_ram[0x0104] == 0x0F || g_ram[0x0104] == 0x14) &&
        (g_ram[0x0140] == 0x80 || g_ram[0x0140] == 0x00)) return 2;
    return 0;
}

static void GunConfigureLauncher(RecompLauncherCGameInfo *gi)
{
    gi->window_icon_path = "assets/img/icon.png";   /* staged by CMake beside the exe */
}

static void GunBeginFrame(unsigned n)
{
    (void)n;
    GunMenu_ApplyEdges();
    static int hooked;
    if (!hooked) {
        hooked = 1;
        for (unsigned i = 0; i < sizeof kCull / sizeof kCull[0]; i++)
            interp_bridge_add_pre_opcode_hook(kCull[i].pc, GunCullHook);
        interp_bridge_add_pre_opcode_hook(0x848100, GunSkipHook);
        interp_bridge_add_pre_opcode_hook(0x8892E4, GunTxEnter);
        interp_bridge_add_pre_opcode_hook(0x849514, GunFadeOutHook);
        interp_bridge_add_pre_opcode_hook(0x8498C7, GunFadeInHook);
        interp_bridge_add_pre_opcode_hook(0x849607, GunHoldHook);
        interp_bridge_add_pre_opcode_hook(0x848008, GunTxYield);
    }
    if (!g_ppu || g_ppu->extraLeftRight == 0) { g_wsL = g_wsR = -16; return; }
    int cam = GunW(0x0A60), lo = GunW(0x0B2A), hi = GunW(0x0B28);
    int camY = GunW(0x0A62);
    int l = 0, r = 0;
    /* Only in real room gameplay: not paused / in a menu or the map ($0AFC,
     * $016E), the BG1 scroll shadows follow the camera, and the camera sits
     * inside the room bounds. Everything else (title, name entry, cutscenes,
     * pause, inventory, map) stays centred at the authentic 256px. */
    int play = GunIsPlay();
    int ws = play || GunIsPause();
    if (ws) {
        l = cam - lo - 2; r = hi - cam - 2;
        if (l < 0) l = 0;
        if (r < 0) r = 0;
    }
    /* NPC dialogue needs no special case: the box is part of the centred 4:3
     * picture and the dim window only covers it, so the widescreen world simply
     * shows around it. (The old "black bar" was the HDMA window ordering bug in
     * the renderer, fixed in beam_frame_driver.c.) */
    /* The status HUD (BG3, top 40 lines, all in the left 160px) is pinned to
     * the corner of the widescreen frame instead of floating in the picture. */
    int hud = play && GunMenu_HudCorners();
    PpuSetWidescreenHudSplit(g_ppu, hud ? 40 : 0, 160, 164);
    PpuSetWidescreenHudAlwaysVisible(g_ppu, hud != 0);
    /* Combat-zone weapon/punch boxes are two big sprites in OAM slots 0-1. */
    PpuSetWsHudOamBand(g_ppu, hud ? 40 : 0, 160, 164);
    PpuSetWsHudOamShiftRange(g_ppu, 0, hud ? 2 : 0);
    PpuSetWidescreenViewAnchor(g_ppu, GunMenu_ViewSlide() != 0);
    PpuSetExtraSideSpace(g_ppu, l, r, 0);
    /* Outside real gameplay the pillars must be black,
     * not whatever the tilemap wraps around to. */
    if (!ws) {
        PpuSetWidescreenLayerClamp(g_ppu, 0x0F); PpuSetWidescreenLayerMirror(g_ppu, 0x00);
        if (g_ppu->renderBuffer) memset(g_ppu->renderBuffer, 0, (size_t)g_ppu->renderPitch * 224);
    }
    if (GunMenu_WideActors()) { g_wsL = g_ppu->extraLeftCur; g_wsR = g_ppu->extraRightCur; }
    else g_wsL = g_wsR = -16;   /* original window [-32,255] */
}
static int GunWindowBaseWidth(int frame_w)  { (void)frame_w; return GunMenu_WindowBaseWidth(); }
static int GunWindowBaseHeight(void)        { return 224; }

static const char kDefaultIni[] =
    "[General]\n"
    "# Automatically save state on quit and reload on start\n"
    "Autosave = 0\n"
    "\n"
    "# Disable the SDL_Delay that happens each frame (slightly better\n"
    "# perf if your display is set to exactly 60hz)\n"
    "DisableFrameDelay = 0\n"
    "\n"
    "[Graphics]\n"
    "# Render real extra picture on the sides to fit the window (no stretching)\n"
    "Widescreen = 1\n"
    "\n"
    "# Window size (Auto or WidthxHeight)\n"
    "WindowSize = Auto\n"
    "\n"
    "# Fullscreen mode (0=windowed, 1=desktop fullscreen, 2=fullscreen w/mode change)\n"
    "Fullscreen = 0\n"
    "\n"
    "# Window scale (1=100%, 2=200%, 3=300%, etc.)\n"
    "WindowScale = 3\n"
    "\n"
    "# Use the optimized SNES PPU implementation\n"
    "NewRenderer = 1\n"
    "\n"
    "# Don't keep the aspect ratio\n"
    "IgnoreAspectRatio = 0\n"
    "\n"
    "# Remove the sprite limits per scan line\n"
    "NoSpriteLimits = 1\n"
    "\n"
    "[Sound]\n"
    "EnableAudio = 1\n"
    "# 0..100; VolumeUp / VolumeDown move it in 5% steps and show a bar.\n"
    "Volume = 100\n"
    "AudioFreq = 32000\n"
    "AudioChannels = 2\n"
    "AudioSamples = 512\n"
    "\n"
    "[KeyMap]\n"
    "# This section is for system-level shortcuts (save/load state,\n"
    "# fullscreen, pause, etc.). The 12 SNES controller buttons live\n"
    "# in keybinds.ini next to the executable.\n"
    "Fullscreen = Alt+Return\n"
    "Reset = Ctrl+r\n"
    "Pause = Shift+p\n"
    "PauseDimmed = p\n"
    "Turbo = Tab\n"
    "WindowBigger = Ctrl+Up\n"
    "WindowSmaller = Ctrl+Down\n"
    "VolumeUp = Keypad +\n"
    "VolumeDown = Keypad -\n"
    "DisplayPerf = f\n"
    "ToggleRenderer = r\n"
    "SaveStateMenu = F11\n"
    "Rewind = F12\n"
    "Load =      F1,     F2,     F3,     F4,     F5,     F6,     F7,     F8,     F9,     F10\n"
    "Save = Shift+F1,Shift+F2,Shift+F3,Shift+F4,Shift+F5,Shift+F6,Shift+F7,Shift+F8,Shift+F9,Shift+F10\n"
    "\n"
    "[GamepadMap]\n"
    "# Enable each player's gamepad slot. SDL_GameController-compatible\n"
    "# controllers (Xbox, PlayStation, Switch Pro, etc.) auto-detect\n"
    "# when plugged in. Set to false to force keyboard-only.\n"
    "EnableGamepad1 = true\n"
    "EnableGamepad2 = true\n"
    "\n"
    "# Default Xbox-layout mapping. Order matches kKeys_Controls:\n"
    "#   Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R\n"
    "# Edit + restart to rebind. Shoulder = L1/Lb (top), trigger = L2.\n"
    "Controls =   DpadUp, DpadDown, DpadLeft, DpadRight, Back, Start, B, A, Y, X, Lb, Rb\n"
    "ControlsP2 = DpadUp, DpadDown, DpadLeft, DpadRight, Back, Start, B, A, Y, X, Lb, Rb\n";

static const SnesDesktopHostGame kGameHost = {
    .display_name        = "Gunman's Proof",
    .window_title        = "Gunman's Proof",
    .region              = SNESRECOMP_ROM_REGION,
    .rom_file            = SNESRECOMP_ROM_FILE,
    .expected_sha256_hex = SNESRECOMP_ROM_EXPECTED_SHA256,
    .expected_crc32_hex  = SNESRECOMP_ROM_EXPECTED_CRC32,
    .game_id             = SNESRECOMP_ROM_GAME_ID,
    .build_version       = SNES_GAME_VERSION,
    .game_info           = &kGameInfo,
    .num_players         = 1,
    .widescreen_supported = 1,
    .native_widescreen   = 1,
    .prepare_frame       = GunPrepareFrame,
    .window_base_width   = GunWindowBaseWidth,
    .window_base_height  = GunWindowBaseHeight,
    .compute_viewport    = GunComputeViewport,
    .default_config_ini  = kDefaultIni,
    .extra_inputs        = GunExtraInputs,
    .want_turbo          = GunWantTurbo,
    .begin_sim_frame     = GunBeginFrame,
    .configure_launcher  = GunConfigureLauncher,
    .menu_hotkey         = SDLK_INSERT,
    .menu_open           = GunMenu_Open,
    .menu_is_open        = GunMenu_IsOpen,
    .menu_event          = GunMenu_Event,
    .menu_panel          = GunMenu_Panel,
    /* Battery-backed SRAM shows the launcher's SAVES panel. Leave NULL for a
     * title without one. The path is exe-relative. */
    .sram_path           = "saves/save.srm",
};

#ifndef __ANDROID__
#undef main   /* desktop: keep plain main() even if SDL_main.h remapped it */
#endif
int main(int argc, char **argv)
{
    return snesrecomp_desktop_main(&kGameHost, argc, argv);
}
