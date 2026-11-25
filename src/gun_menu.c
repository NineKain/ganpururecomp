/*
 * Gunman's Proof -- settings menu (Insert).
 *
 * A small pixel-art panel the desktop host presents over the frozen game
 * (SnesDesktopHostGame.menu_*). Opened with Insert or the pad's Guide/PS button.
 *
 *   Display (windowed / fullscreen), window size, aspect ratio, master volume,
 *   and remapping for the keyboard (keybinds.ini) and the controller
 *   ([GamepadMap] Controls in config.ini).
 *
 * The panel is drawn on a 256x224 logical canvas with the shared 8x8 overlay
 * font and doubled to 512x448, so every logical pixel is a crisp 2x2 block.
 */
#include "gun_menu.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

#include "desktop/config.h"
#include "host_api.h"
#include "host_main.h"
#include "keybinds.h"
#include "common_rtl.h"      /* g_ppu */
#include "snes/ppu.h"
#include "snes_overlay_draw.h"
#include "snes_osd.h"

#define LW 256
#define LH 224

/* Palette (Sweetie-16 inspired). */
#define C_BG     0xFF1A1C2Cu
#define C_BG2    0xFF333C57u
#define C_STRIPE 0xFF1E2133u
#define C_LIGHT  0xFFF4F4F4u
#define C_MID    0xFF566C86u
#define C_DIM    0xFF94B0C2u
#define C_GOLD   0xFFFFCD75u
#define C_BLUE   0xFF41A6F6u
#define C_GREEN  0xFFA7F070u
#define C_RED    0xFFEF7D57u
#define C_SHADOW 0xFF0B0C16u

/* ── Settings kept by this file ──────────────────────────────────────────── */

static int g_aspect = kGunAspect_Auto;
static int g_aspect_loaded;
static int g_edges = 0;        /* 0 extend, 2 black (1 = retired mirror mode) */
static int g_actors = 1;       /* draw actors in the widescreen margins */
static int g_textspeed = 0;   /* 0 normal, 1 x2, 2 x3, 3 instant */
static int g_rumble = 1;      /* DualSense/pad feedback: 0 off, 1 subtle */
static int g_fades = 1;       /* room transitions: 0 normal, 1 fast, 2 faster */
static int g_skipintro = 1;    /* Start skips the opening cutscene */
static int g_hud = 1;          /* pin the HUD to the frame corner */
static int g_slide = 1;        /* slide the view at room edges (else fixed centre) */
static int g_fps = 0;          /* FPS counter */

static void IniPath(char *out, size_t n) {
  const char *c = snesrecomp_desktop_config_path();
  out[0] = 0;
  if (!c) return;
  snprintf(out, n, "%s", c);
  char *p = strrchr(out, '/');
  char *q = strrchr(out, '\\');
  if (q > p) p = q;
  if (p) p[1] = 0; else out[0] = 0;
  strncat(out, "gunman_menu.ini", n - strlen(out) - 1);
}

static void AspectLoad(void) {
  char path[600];
  IniPath(path, sizeof path);
  if (!path[0]) return;               /* config path not known yet */
  g_aspect_loaded = 1;
  FILE *f = fopen(path, "r");
  if (!f) return;
  char line[128];
  while (fgets(line, sizeof line, f)) {
    char k[32]; int v;
    if (sscanf(line, "%31[a-z]=%d", k, &v) != 2) continue;
    if (!strcmp(k, "aspect") && v >= 0 && v < kGunAspect_Count) g_aspect = v;
    else if (!strcmp(k, "edges") && v >= 0 && v < 3) g_edges = (v == 1) ? 0 : v;   /* MIRROR was removed: it mirrored the HUD text too */
    else if (!strcmp(k, "actors")) g_actors = v != 0;
    else if (!strcmp(k, "slide")) g_slide = v != 0;
    else if (!strcmp(k, "hud")) g_hud = v != 0;
    else if (!strcmp(k, "skipintro")) g_skipintro = v != 0;
    else if (!strcmp(k, "rumble")) g_rumble = v != 0;
    else if (!strcmp(k, "fades")) g_fades = v < 0 ? 0 : v > 2 ? 2 : v;
    else if (!strcmp(k, "textspeed")) g_textspeed = v < 0 ? 0 : v > 3 ? 3 : v;
    else if (!strcmp(k, "fps")) g_fps = v != 0;
  }
  fclose(f);
  /* A saved "4:3 ORIGINAL" choice must survive a restart: the host's own
   * widescreen flag comes from config.ini and would otherwise override it. */
  snesrecomp_desktop_set_widescreen(g_aspect != kGunAspect_4x3);
  snes_osd_set_fps_visible(g_fps);
}

static void AspectSave(void) {
  char path[600];
  IniPath(path, sizeof path);
  if (!path[0]) return;
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "aspect=%d\nedges=%d\nactors=%d\nslide=%d\nhud=%d\nskipintro=%d\nfps=%d\ntextspeed=%d\nfades=%d\nrumble=%d\n",
          g_aspect, g_edges, g_actors, g_slide, g_hud, g_skipintro, g_fps, g_textspeed, g_fades, g_rumble);
  fclose(f);
}

int GunMenu_WideActors(void) { if (!g_aspect_loaded) AspectLoad(); return g_actors; }
int GunMenu_PadFeedback(void) { if (!g_aspect_loaded) AspectLoad(); return g_rumble; }
int GunMenu_RoomFades(void) { if (!g_aspect_loaded) AspectLoad(); return g_fades; }
int GunMenu_TextSpeed(void) { if (!g_aspect_loaded) AspectLoad(); return g_textspeed; }
int GunMenu_SkipIntro(void) { if (!g_aspect_loaded) AspectLoad(); return g_skipintro; }
int GunMenu_HudCorners(void) { if (!g_aspect_loaded) AspectLoad(); return g_hud; }
int GunMenu_BlackEdges(void) { if (!g_aspect_loaded) AspectLoad(); return g_edges == 2; }
int GunMenu_ViewSlide(void)  { return 0; } /* off: the 4:3 picture stays centred, character stays where the game puts him */

int GunMenu_AspectMode(void) {
  if (!g_aspect_loaded) AspectLoad();
  if (g_aspect == kGunAspect_4x3 || !g_config.widescreen) return kGunAspect_4x3;
  return g_aspect;
}

double GunMenu_FixedAspect(void) {
  switch (GunMenu_AspectMode()) {
  case kGunAspect_4x3:  return 4.0 / 3.0;
  case kGunAspect_16x9: return 16.0 / 9.0;
  case kGunAspect_16x10: return 16.0 / 10.0;
  case kGunAspect_21x9: return 21.0 / 9.0;
  default: return 0.0;
  }
}

int GunMenu_WindowBaseWidth(void) {
  double a = GunMenu_FixedAspect();
  return a > 0.0 ? (int)(224.0 * a + 0.5) : 398;   /* Auto opens as 16:9 */
}

static const char *const kEdgeName[3] = { "EXTEND", "EXTEND", "BLACK" };

/* Called by the host every simulated frame (begin_sim_frame): how the extra
 * widescreen columns are filled. Extend shows what the PPU renders there
 * (real picture, but the original tilemap wraps at 256 px); Mirror reflects
 * the native edge; Black clamps the backgrounds to the original 256 columns. */
void GunMenu_ApplyEdges(void) {
  if (!g_aspect_loaded) AspectLoad();
  if (!g_ppu) return;
  PpuSetWidescreenLayerClamp(g_ppu, g_edges == 2 ? 0x0F : 0x00);
  PpuSetWidescreenLayerMirror(g_ppu, g_edges == 1 ? 0x0F : 0x00);
}

static const char *const kAspectName[kGunAspect_Count] = {
  "AUTO (FIT)", "4:3 ORIGINAL", "16:9", "16:10", "21:9"
};

static void SetAspect(int mode) {
  g_aspect = mode;
  AspectSave();
  snesrecomp_desktop_set_widescreen(mode != kGunAspect_4x3);
  /* Re-apply the scale so the window takes the new shape. */
  snesrecomp_set_window_scale(snesrecomp_get_window_scale());
}

/* ── Controller bindings: config.ini [GamepadMap] Controls ───────────────── */

enum { NB = 12 };
/* Row order == the order of the Controls line. */
static const char *const kRowName[NB] = {
  "UP", "DOWN", "LEFT", "RIGHT", "SELECT", "START", "A", "B", "X", "Y", "L", "R"
};
static const char *const kDefTok[NB] = {
  "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "Back", "Start",
  "B", "A", "Y", "X", "Lb", "Rb"
};
/* keybinds.ini order is a b x y l r start select up down left right. */
static const int kKbIdx[NB] = { 8, 9, 10, 11, 7, 6, 0, 1, 2, 3, 4, 5 };

static char g_tok[NB][16];

static const struct { const char *tok, *label; } kPadLabel[] = {
  { "DpadUp", "D-PAD UP" }, { "DpadDown", "D-PAD DOWN" },
  { "DpadLeft", "D-PAD LEFT" }, { "DpadRight", "D-PAD RIGHT" },
  { "Back", "CREATE/BACK" }, { "Guide", "PS/GUIDE" },
  { "Start", "OPTIONS/START" }, { "L3", "L3" }, { "R3", "R3" },
  { "Lb", "L1/LB" }, { "L1", "L1/LB" }, { "Rb", "R1/RB" }, { "R1", "R1/RB" },
  { "L2", "L2/LT" }, { "R2", "R2/RT" },
  { "A", "CROSS/A" }, { "B", "CIRCLE/B" }, { "X", "SQUARE/X" }, { "Y", "TRIANGLE/Y" },
};

static const char *PadLabel(const char *tok) {
  for (size_t i = 0; i < sizeof kPadLabel / sizeof kPadLabel[0]; i++)
    if (!strcasecmp(tok, kPadLabel[i].tok)) return kPadLabel[i].label;
  return tok;
}

static const char *TokForButton(int b) {
  switch (b) {
  case kGamepadBtn_A: return "A";
  case kGamepadBtn_B: return "B";
  case kGamepadBtn_X: return "X";
  case kGamepadBtn_Y: return "Y";
  case kGamepadBtn_Back: return "Back";
  case kGamepadBtn_Start: return "Start";
  case kGamepadBtn_L1: return "Lb";
  case kGamepadBtn_R1: return "Rb";
  case kGamepadBtn_L3: return "L3";
  case kGamepadBtn_R3: return "R3";
  case kGamepadBtn_DpadUp: return "DpadUp";
  case kGamepadBtn_DpadDown: return "DpadDown";
  case kGamepadBtn_DpadLeft: return "DpadLeft";
  case kGamepadBtn_DpadRight: return "DpadRight";
  default: return NULL;
  }
}

static char *ReadAll(const char *path, size_t *n) {
  FILE *f = fopen(path, "rb");
  if (!f) { *n = 0; return NULL; }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *b = (char *)malloc((size_t)sz + 1);
  if (!b) { fclose(f); *n = 0; return NULL; }
  *n = fread(b, 1, (size_t)sz, f);
  b[*n] = 0;
  fclose(f);
  return b;
}

/* Finds the "Controls =" line inside [GamepadMap]. Returns its start/end
 * offsets (end excludes the newline) and the offset of the section header
 * line's end, or -1s. */
static void FindControls(const char *s, long *ls, long *le, long *hdr_end) {
  *ls = *le = *hdr_end = -1;
  int in = 0;
  for (const char *p = s; *p;) {
    const char *eol = strchr(p, '\n');
    size_t len = eol ? (size_t)(eol - p) : strlen(p);
    const char *t = p;
    while (t < p + len && (*t == ' ' || *t == '\t')) t++;
    if (*t == '[') {
      in = !strncasecmp(t, "[GamepadMap]", 12);
      if (in) *hdr_end = (long)(p + len - s);
    } else if (in && !strncasecmp(t, "Controls", 8)) {
      const char *u = t + 8;
      while (u < p + len && (*u == ' ' || *u == '\t')) u++;
      if (u < p + len && *u == '=') { *ls = (long)(p - s); *le = (long)(p + len - s); return; }
    }
    if (!eol) break;
    p = eol + 1;
  }
}

static void PadLoad(void) {
  for (int i = 0; i < NB; i++) snprintf(g_tok[i], sizeof g_tok[i], "%s", kDefTok[i]);
  const char *path = snesrecomp_desktop_config_path();
  size_t n;
  char *s = path ? ReadAll(path, &n) : NULL;
  if (!s) return;
  long ls, le, he;
  FindControls(s, &ls, &le, &he);
  if (ls >= 0) {
    char *v = strchr(s + ls, '=') + 1;
    char *end = s + le;
    int i = 0;
    while (i < NB && v < end) {
      while (v < end && (*v == ' ' || *v == '\t')) v++;
      char *c = v;
      while (c < end && *c != ',') c++;
      char *e = c;
      while (e > v && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
      size_t l = (size_t)(e - v);
      if (l > 0 && l < sizeof g_tok[0]) { memcpy(g_tok[i], v, l); g_tok[i][l] = 0; }
      i++;
      v = c < end ? c + 1 : end;
    }
  }
  free(s);
}

static void PadSave(void) {
  const char *path = snesrecomp_desktop_config_path();
  if (!path) return;
  char line[400] = "Controls = ";
  for (int i = 0; i < NB; i++) {
    strncat(line, g_tok[i], sizeof line - strlen(line) - 1);
    if (i + 1 < NB) strncat(line, ", ", sizeof line - strlen(line) - 1);
  }
  size_t n;
  char *s = ReadAll(path, &n);
  if (!s) return;
  long ls, le, he;
  FindControls(s, &ls, &le, &he);
  FILE *f = fopen(path, "wb");
  if (!f) { free(s); return; }
  if (ls >= 0) {
    fwrite(s, 1, (size_t)ls, f);
    fputs(line, f);
    fputs(s + le, f);
  } else if (he >= 0) {
    fwrite(s, 1, (size_t)he, f);
    fputc('\n', f);
    fputs(line, f);
    fputs(s + he, f);
  } else {
    fputs(s, f);
    fprintf(f, "\n[GamepadMap]\n%s\n", line);
  }
  fclose(f);
  free(s);
  ConfigReloadGamepadMap(path);
}

/* ── Menu state ──────────────────────────────────────────────────────────── */

enum { SCR_MAIN, SCR_VIDEO, SCR_WIDE, SCR_CONTROLS, SCR_KEYS, SCR_PADS, SCR_N };

static struct {
  int open, scr, sel[SCR_N];
  int capture;            /* row being rebound, or -1 */
  uint32_t capture_t;
  int quit_armed;
  int hdx, hdy;           /* held direction (d-pad / stick) */
  uint32_t hs, hl;
  int sx, sy;             /* stick state */
} M;

static int RowCount(void) {
  switch (M.scr) {
  case SCR_MAIN: return 10;
  case SCR_VIDEO: return 9;
  case SCR_WIDE: return 5;
  case SCR_CONTROLS: return 3;
  default: return NB + 2;   /* buttons, reset, back */
  }
}

void GunMenu_Open(void) {
  M.open = 1;
  M.scr = SCR_MAIN;
  memset(M.sel, 0, sizeof M.sel);
  M.capture = -1;
  M.quit_armed = 0;
  M.hdx = M.hdy = M.sx = M.sy = 0;
  PadLoad();
}
int GunMenu_IsOpen(void) { return M.open; }

static void Close(void) { M.open = 0; M.quit_armed = 0; M.capture = -1; }

static void Back(void) {
  M.quit_armed = 0;
  if (M.capture >= 0) { M.capture = -1; return; }
  if (M.scr == SCR_MAIN) Close();
  else if (M.scr == SCR_KEYS || M.scr == SCR_PADS) M.scr = SCR_CONTROLS;
  else M.scr = SCR_MAIN;
}

static int Clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static const char *const kVsyncName[3] = { "OFF", "ON", "ADAPTIVE" };

/* CRT filter: OFF / FAST / ACCURATE are the two bundled presets; anything else
 * that the launcher or config.ini selected shows as CUSTOM. */
static const char kCrtFast[] = "assets/shaders/crt-fast.glslp";
static const char kCrtAccurate[] = "assets/shaders/crt-accurate.glslp";
static int CrtMode(void) {
  const char *p = snesrecomp_get_shader();
  if (!p || !p[0]) return 0;
  if (strstr(p, "crt-fast")) return 1;
  if (strstr(p, "crt-accurate")) return 2;
  return 3;
}
static void CrtCycle(int d) {
  if (!snesrecomp_shader_supported()) return;
  int c = CrtMode();
  c = (c == 3) ? 0 : (c + (d < 0 ? 2 : 1)) % 3;
  snesrecomp_set_shader(c == 1 ? kCrtFast : c == 2 ? kCrtAccurate : "");
}

static void Adjust(int d) {
  int r = M.sel[M.scr];
  M.quit_armed = 0;
  switch (M.scr) {
  case SCR_MAIN:
    if (r == 2) snesrecomp_set_volume(Clampi(g_config.volume + d * 5, 0, 100));
    else if (r == 4) { g_skipintro = !g_skipintro; AspectSave(); }
    else if (r == 5) { g_textspeed = Clampi(g_textspeed + (d < 0 ? -1 : 1), 0, 3); AspectSave(); }
    else if (r == 6) { g_fades = Clampi(g_fades + (d < 0 ? -1 : 1), 0, 2); AspectSave(); }
    else if (r == 7) { g_rumble = !g_rumble; AspectSave(); }
    break;
  case SCR_VIDEO:
    switch (r) {
    case 0: snesrecomp_set_fullscreen(!snesrecomp_get_fullscreen()); break;
    case 1: snesrecomp_set_window_scale(Clampi(snesrecomp_get_window_scale() + d, 1, 6)); break;
    case 2: snesrecomp_set_linear_filtering(!snesrecomp_get_linear_filtering()); break;
    case 3: snesrecomp_set_vsync((snesrecomp_get_vsync() + d + 3) % 3); break;
    case 4: snesrecomp_set_frame_blend(!snesrecomp_get_frame_blend()); break;
    case 5: snesrecomp_set_no_sprite_limits(!snesrecomp_get_no_sprite_limits()); break;
    case 6: g_fps = !g_fps; snes_osd_set_fps_visible(g_fps); AspectSave(); break;
    case 7: CrtCycle(d); break;
    default: break;
    }
    break;
  case SCR_WIDE:
    switch (r) {
    case 0: {
      int m = GunMenu_AspectMode();
      SetAspect((m + d + kGunAspect_Count) % kGunAspect_Count);
      break;
    }
    case 1: g_edges = (g_edges == 2) ? 0 : 2; AspectSave(); break;   /* EXTEND <-> BLACK */
    case 2: g_actors = !g_actors; AspectSave(); break;
    case 3: g_hud = !g_hud; AspectSave(); break;
    default: break;
    }
    break;
  default: break;
  }
}

static void KeysReset(void) {
  keybinds_reset_player(1);
  keybinds_save();
}

static void Activate(void) {
  int r = M.sel[M.scr];
  switch (M.scr) {
  case SCR_MAIN:
    if (r == 0) M.scr = SCR_VIDEO;
    else if (r == 1) M.scr = SCR_WIDE;
    else if (r == 2) { /* slider: left/right */ }
    else if (r == 3) { M.scr = SCR_CONTROLS; }
    else if (r == 4) { Adjust(1); }
    else if (r == 5) { g_textspeed = (g_textspeed + 1) % 4; AspectSave(); }
    else if (r == 6) { g_fades = (g_fades + 1) % 3; AspectSave(); }
    else if (r == 7) { g_rumble = !g_rumble; AspectSave(); }
    else if (r == 8) { Close(); }
    else if (r == 9) {
      if (M.quit_armed) snesrecomp_request_exit();
      M.quit_armed = !M.quit_armed;
    }
    break;
  case SCR_VIDEO:
    if (r == 8) Back(); else Adjust(1);
    break;
  case SCR_WIDE:
    if (r == 4) Back(); else Adjust(1);
    break;
  case SCR_CONTROLS:
    if (r == 0) M.scr = SCR_KEYS;
    else if (r == 1) { M.scr = SCR_PADS; PadLoad(); }
    else Back();
    break;
  case SCR_KEYS:
  case SCR_PADS:
    if (r < NB) { M.capture = r; M.capture_t = SDL_GetTicks(); }
    else if (r == NB) {
      if (M.scr == SCR_KEYS) KeysReset();
      else {
        for (int i = 0; i < NB; i++) snprintf(g_tok[i], sizeof g_tok[i], "%s", kDefTok[i]);
        PadSave();
      }
    } else Back();
    break;
  }
}

static void Move(int dx, int dy) {
  if (M.capture >= 0) return;
  if (dy) {
    int n = RowCount();
    M.sel[M.scr] = (M.sel[M.scr] + dy + n) % n;
    M.quit_armed = 0;
  }
  if (dx) Adjust(dx);
}

static void BindKey(SDL_Scancode sc) {
  int row = M.capture;
  int idx = kKbIdx[row];
  SDL_Scancode old = keybinds_get_button(1, idx);
  for (int r = 0; r < NB; r++)           /* swap on conflict */
    if (r != row && keybinds_get_button(1, kKbIdx[r]) == sc)
      keybinds_set_button(1, kKbIdx[r], old);
  keybinds_set_button(1, idx, sc);
  keybinds_save();
  M.capture = -1;
}

static void BindPad(int btn) {
  const char *tok = TokForButton(btn);
  if (!tok) return;
  int row = M.capture;
  char old[16];
  snprintf(old, sizeof old, "%s", g_tok[row]);
  for (int r = 0; r < NB; r++)
    if (r != row && !strcasecmp(g_tok[r], tok))
      snprintf(g_tok[r], sizeof g_tok[r], "%s", old);
  snprintf(g_tok[row], sizeof g_tok[row], "%s", tok);
  PadSave();
  M.capture = -1;
}

static void SetHeld(int dx, int dy) {
  M.hdx = dx; M.hdy = dy;
  M.hs = M.hl = SDL_GetTicks();
}

void GunMenu_Event(const SDL_Event *ev, int pad_button) {
  if (!M.open) return;
  switch (ev->type) {
  case SDL_KEYDOWN: {
    int key = SNESRECOMP_SDL_EVENT_KEY(*ev);
#if SNESRECOMP_SDL3
    SDL_Scancode sc = ev->key.scancode;
#else
    SDL_Scancode sc = ev->key.keysym.scancode;
#endif
    if (M.capture >= 0 && M.scr == SCR_KEYS) {
      if (key == SDLK_ESCAPE) M.capture = -1;
      else if (key != SDLK_INSERT) BindKey(sc);
      return;
    }
    if (key == SDLK_INSERT && !SNESRECOMP_SDL_EVENT_REPEAT(*ev)) { Close(); return; }
    if (key == SDLK_ESCAPE) { Back(); return; }
    if (M.capture >= 0) return;
    if (key == SDLK_UP) Move(0, -1);
    else if (key == SDLK_DOWN) Move(0, 1);
    else if (key == SDLK_LEFT) Move(-1, 0);
    else if (key == SDLK_RIGHT) Move(1, 0);
    else if ((key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) &&
             !SNESRECOMP_SDL_EVENT_REPEAT(*ev)) Activate();
    break;
  }
  case SDL_CONTROLLERBUTTONDOWN:
    if (M.capture >= 0 && M.scr == SCR_PADS) {
      if (pad_button == kGamepadBtn_Guide) M.capture = -1; else BindPad(pad_button);
      return;
    }
    if (M.capture >= 0) return;
    switch (pad_button) {
    case kGamepadBtn_DpadUp:    Move(0, -1); SetHeld(0, -1); break;
    case kGamepadBtn_DpadDown:  Move(0, 1);  SetHeld(0, 1);  break;
    case kGamepadBtn_DpadLeft:  Move(-1, 0); SetHeld(-1, 0); break;
    case kGamepadBtn_DpadRight: Move(1, 0);  SetHeld(1, 0);  break;
    case kGamepadBtn_A: Activate(); break;          /* Cross */
    case kGamepadBtn_B: Back(); break;              /* Circle */
    case kGamepadBtn_Start:
    case kGamepadBtn_Guide: Close(); break;
    default: break;
    }
    break;
  case SDL_CONTROLLERBUTTONUP: {
    int b = SNESRECOMP_SDL_EVENT_BUTTON(*ev);
    if (b == SDL_CONTROLLER_BUTTON_DPAD_UP || b == SDL_CONTROLLER_BUTTON_DPAD_DOWN ||
        b == SDL_CONTROLLER_BUTTON_DPAD_LEFT || b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
      M.hdx = M.hdy = 0;
    break;
  }
  case SDL_CONTROLLERAXISMOTION: {
    int ax = SNESRECOMP_SDL_EVENT_AXIS(*ev), v = SNESRECOMP_SDL_EVENT_AXIS_VALUE(*ev);
    int d = v > 16000 ? 1 : v < -16000 ? -1 : 0;
    if (ax == SDL_CONTROLLER_AXIS_LEFTX && d != M.sx) {
      M.sx = d;
      if (d) { Move(d, 0); SetHeld(d, 0); } else M.hdx = 0;
    } else if (ax == SDL_CONTROLLER_AXIS_LEFTY && d != M.sy) {
      M.sy = d;
      if (d) { Move(0, d); SetHeld(0, d); } else M.hdy = 0;
    }
    break;
  }
  default: break;
  }
}

/* ── Drawing ─────────────────────────────────────────────────────────────── */

static uint32_t L[LW * LH];
static uint32_t P[512 * 448];

static void Fill(int x, int y, int w, int h, uint32_t c) {
  snes_ovl_fill_rect(L, LW, LH, x, y, w, h, c);
}
static void Box(int x, int y, int w, int h, uint32_t c) {
  snes_ovl_stroke_rect(L, LW, LH, x, y, w, h, c);
}
static void Text(int x, int y, const char *s, uint32_t c) {
  snes_ovl_draw_text(L, LW, LH, x + 1, y + 1, s, C_SHADOW, 1);
  snes_ovl_draw_text(L, LW, LH, x, y, s, c, 1);
}
static void TextC(int y, const char *s, uint32_t c) {
  Text((LW - (int)strlen(s) * 8) / 2, y, s, c);
}
static void TextR(int xr, int y, const char *s, uint32_t c) {
  Text(xr - (int)strlen(s) * 8, y, s, c);
}
static void Upper(char *s) { for (; *s; s++) *s = (char)toupper((unsigned char)*s); }

static void Cursor(int x, int y) {
  static const char *const spr[7] = {
    "X......", "XX.....", "XXX....", "XXXX...", "XXX....", "XX.....", "X......"
  };
  for (int j = 0; j < 7; j++)
    for (int i = 0; i < 7; i++)
      if (spr[j][i] == 'X') {
        L[(y + j) * LW + x + i] = C_GOLD;
      }
}

static void Frame(void) {
  Fill(4, 8, 248, 208, C_BG);
  for (int y = 10; y < 214; y += 2) Fill(6, y, 244, 1, C_STRIPE);   /* scanline texture */
  Box(4, 8, 248, 208, C_LIGHT);
  Box(6, 10, 244, 204, C_MID);
  /* notched corners */
  uint32_t k = 0;
  L[8 * LW + 4] = k; L[8 * LW + 251] = k; L[215 * LW + 4] = k; L[215 * LW + 251] = k;
  Fill(7, 11, 242, 14, C_BG2);
  Fill(7, 25, 242, 1, C_MID);
}

static void Row(int i, int y, const char *label, uint32_t lc) {
  int sel = M.sel[M.scr] == i;
  if (sel) {
    Fill(9, y - 2, 238, 12, C_BG2);
    Cursor(12, y + 1);
  }
  Text(24, y, label, sel ? C_GOLD : lc);
}

static void Choice(int y, const char *v, int sel, uint32_t c) {
  char b[40];
  snprintf(b, sizeof b, sel ? "< %s >" : "%s", v);
  TextR(240, y, b, c);
}

static void Slider(int y, int pct, int sel) {
  int x = 128, steps = 20;
  Box(x, y, steps * 3 + 3, 8, C_MID);
  for (int i = 0; i < steps; i++)
    Fill(x + 2 + i * 3, y + 2, 2, 4, i * 5 < pct ? (sel ? C_GOLD : C_BLUE) : C_BG2);
  char b[8];
  snprintf(b, sizeof b, "%d%%", pct);
  TextR(240, y, b, C_LIGHT);
}

static void Footer(const char *help) {
  Fill(7, 174, 242, 1, C_MID);
  TextC(178, help, C_GOLD);
  TextC(189, "UP/DOWN MOVE  LEFT/RIGHT SET", C_DIM);
  TextC(199, "A/CROSS OK  B/CIRCLE BACK", C_DIM);
}

static void Toggle(int y, int on, int sel) {
  Choice(y, on ? "ON" : "OFF", sel, on ? C_GREEN : C_MID);
}

static void DrawMain(void) {
  TextC(14, "SETTINGS", C_GOLD);
  int y = 33, st = 13, s = M.sel[SCR_MAIN];
  Row(0, y, "VIDEO", C_LIGHT);  TextR(240, y, ">>", C_BLUE); y += st;
  Row(1, y, "WIDESCREEN", C_LIGHT); TextR(240, y, ">>", C_BLUE); y += st;
  Row(2, y, "VOLUME", C_LIGHT);
  Slider(y, g_config.volume, s == 2); y += st;
  Row(3, y, "CONTROLS", C_LIGHT); TextR(240, y, ">>", C_BLUE); y += st;
  Row(4, y, "SKIP INTRO (START)", C_LIGHT);
  Toggle(y, g_skipintro, s == 4); y += st;
  Row(5, y, "TEXT SPEED", C_LIGHT);
  { static const char *const n[4] = { "NORMAL", "X2", "X3", "INSTANT" }; Choice(y, n[g_textspeed], s == 5, g_textspeed ? C_GREEN : C_MID); } y += st;
  Row(6, y, "ROOM FADES", C_LIGHT);
  { static const char *const n[3] = { "NORMAL", "FAST", "FASTER" }; Choice(y, n[g_fades], s == 6, g_fades ? C_GREEN : C_MID); } y += st;
  Row(7, y, "PAD FEEDBACK", C_LIGHT);
  Choice(y, g_rumble ? "SUBTLE" : "OFF", s == 7, g_rumble ? C_GREEN : C_MID); y += st;
  Row(8, y, "RESUME", C_LIGHT); y += st;
  Row(9, y, M.quit_armed ? "SURE? PRESS AGAIN" : "QUIT GAME", M.quit_armed ? C_RED : C_LIGHT);
  Footer("INSERT OR PS BUTTON CLOSES");
}

static void DrawVideo(void) {
  TextC(14, "VIDEO", C_GOLD);
  char b[48];
  int y = 34, st = 14, s = M.sel[SCR_VIDEO];
  Row(0, y, "DISPLAY", C_LIGHT);
  Choice(y, snesrecomp_get_fullscreen() ? "FULLSCREEN" : "WINDOWED", s == 0, C_BLUE); y += st;
  int sc = snesrecomp_get_window_scale();
  Row(1, y, "WINDOW SIZE", C_LIGHT);
  snprintf(b, sizeof b, "%dx%d", GunMenu_WindowBaseWidth() * sc, 224 * sc);
  Choice(y, b, s == 1, snesrecomp_get_fullscreen() ? C_MID : C_BLUE); y += st;
  Row(2, y, "SMOOTHING", C_LIGHT);
  Toggle(y, snesrecomp_get_linear_filtering(), s == 2); y += st;
  Row(3, y, "VSYNC", C_LIGHT);
  Choice(y, kVsyncName[Clampi(snesrecomp_get_vsync(), 0, 2)], s == 3, C_BLUE); y += st;
  Row(4, y, "FRAME BLEND", C_LIGHT);
  Toggle(y, snesrecomp_get_frame_blend(), s == 4); y += st;
  Row(5, y, "SPRITE LIMIT", C_LIGHT);
  Choice(y, snesrecomp_get_no_sprite_limits() ? "REMOVED" : "ORIGINAL", s == 5, C_BLUE); y += st;
  Row(6, y, "FPS COUNTER", C_LIGHT);
  Toggle(y, g_fps, s == 6); y += st;
  Row(7, y, "CRT FILTER", C_LIGHT);
  { static const char *const n[4] = { "OFF", "FAST", "ACCURATE", "CUSTOM" };
    int c = CrtMode();
    if (!snesrecomp_shader_supported()) Choice(y, "NEEDS OPENGL", s == 7, C_MID);
    else Choice(y, n[c], s == 7, c ? C_GREEN : C_MID); }
  y += st;
  Row(8, y, "BACK", C_LIGHT);
  static const char *const help[9] = {
    "WINDOWED OR FULLSCREEN", "WINDOW SIZE (WINDOWED ONLY)", "BILINEAR SCALING (SOFTER)",
    "SYNC TO THE DISPLAY", "BLEND FRAMES (FLICKER)", "REMOVES SPRITE FLICKER", "SHOW FRAMES PER SECOND", "CRT SCREEN LOOK (OPENGL)", ""
  };
  Footer(help[s]);
}

static void DrawWide(void) {
  TextC(14, "WIDESCREEN", C_GOLD);
  int y = 38, st = 18, s = M.sel[SCR_WIDE];
  Row(0, y, "ASPECT RATIO", C_LIGHT);
  Choice(y, kAspectName[GunMenu_AspectMode()], s == 0, C_GREEN); y += st;
  Row(1, y, "SIDE EDGES", C_LIGHT);
  Choice(y, kEdgeName[g_edges], s == 1, C_GREEN); y += st;
  Row(2, y, "WIDE ACTORS", C_LIGHT);
  Toggle(y, g_actors, s == 2); y += st;
  Row(3, y, "HUD AT CORNERS", C_LIGHT);
  Toggle(y, g_hud, s == 3); y += st;
  Row(4, y, "BACK", C_LIGHT);
  static const char *const help[5] = {
    "4:3 IS THE ORIGINAL PICTURE", "EXTEND: ROOM  BLACK: PLAIN 4:3",
    "SHOW ACTORS IN THE MARGINS",
    "HUD AT THE SCREEN CORNERS", ""
  };
  Footer(help[s]);
}

static void DrawControls(void) {
  TextC(14, "CONTROLS", C_GOLD);
  Row(0, 40, "KEYBOARD", C_LIGHT);
  TextR(240, 40, ">>", C_BLUE);
  Row(1, 58, "CONTROLLER", C_LIGHT);
  TextR(240, 58, ">>", C_BLUE);
  Row(2, 76, "BACK", C_LIGHT);
  Fill(7, 174, 242, 1, C_MID);
  TextC(182, "PLAYER 1 BUTTON MAPPING", C_DIM);
  TextC(194, "INSERT OR PS BUTTON CLOSES", C_DIM);
}

static void DrawBinds(int keys) {
  TextC(14, keys ? "KEYBOARD" : "CONTROLLER", C_GOLD);
  int y = 32;
  for (int i = 0; i < NB; i++, y += 12) {
    Row(i, y, kRowName[i], C_LIGHT);
    char v[40];
    if (keys) {
      snprintf(v, sizeof v, "%s", SDL_GetScancodeName(keybinds_get_button(1, kKbIdx[i])));
      if (!v[0]) snprintf(v, sizeof v, "NONE");
      Upper(v);
      v[16] = 0;
    } else {
      snprintf(v, sizeof v, "%s", PadLabel(g_tok[i]));
    }
    TextR(240, y, v, M.capture == i ? C_RED : C_BLUE);
  }
  Row(NB, y, "RESET TO DEFAULTS", C_LIGHT);
  y += 12;
  Row(NB + 1, y, "BACK", C_LIGHT);
  if (M.capture >= 0) {
    Fill(24, 78, 208, 60, C_BG);
    Box(24, 78, 208, 60, C_GOLD);
    Box(26, 80, 204, 56, C_MID);
    TextC(88, keys ? "PRESS A KEY FOR" : "PRESS A BUTTON FOR", C_LIGHT);
    TextC(102, kRowName[M.capture], C_GOLD);
    TextC(118, keys ? "ESC TO CANCEL" : "ESC/PS CANCELS", C_DIM);
  }
}

const uint32_t *GunMenu_Panel(int *w, int *h) {
  uint32_t now = SDL_GetTicks();
  /* Auto-repeat of a held d-pad / stick direction. */
  if ((M.hdx || M.hdy) && now - M.hs >= SNES_OVL_REPEAT_DELAY &&
      now - M.hl >= SNES_OVL_REPEAT_RATE) {
    M.hl = now;
    Move(M.hdx, M.hdy);
  }
  if (M.capture >= 0 && M.scr == SCR_PADS && now - M.capture_t > 6000) M.capture = -1;

  memset(L, 0, sizeof L);
  Frame();
  switch (M.scr) {
  case SCR_MAIN: DrawMain(); break;
  case SCR_VIDEO: DrawVideo(); break;
  case SCR_WIDE: DrawWide(); break;
  case SCR_CONTROLS: DrawControls(); break;
  case SCR_KEYS: DrawBinds(1); break;
  case SCR_PADS: DrawBinds(0); break;
  }
  if (M.scr >= SCR_KEYS && M.capture < 0)
    TextC(201, "ENTER/CROSS REBIND  ESC BACK", C_DIM);
  /* 2x nearest */
  for (int y = 0; y < LH; y++) {
    uint32_t *d0 = &P[(y * 2) * 512];
    for (int x = 0; x < LW; x++) {
      uint32_t c = L[y * LW + x];
      d0[x * 2] = d0[x * 2 + 1] = c;
    }
    memcpy(&P[(y * 2 + 1) * 512], d0, 512 * sizeof(uint32_t));
  }
  *w = 512;
  *h = 448;
  return P;
}
