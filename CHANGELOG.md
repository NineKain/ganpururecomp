# Changelog

## Unreleased

- New CRT FILTER (Insert menu, VIDEO page; also in the launcher's Shader list): FAST (one pass: scanlines, light blur, faint glow, phosphor mask) and ACCURATE (four passes: linear-light beam softening, brightness-dependent scanline beam, halation glow, aperture-grille mask, display gamma). Original shaders in assets/shaders. Needs the OpenGL renderer; scanlines and the mask fade out on small windows and look best at 3x or larger.
- Removed the MIRROR side-edge mode (it mirrored the HUD text into the margin). A saved MIRROR setting falls back to EXTEND.
- Launcher: the ROM panel no longer clips text (wrapped notice, shorter built-in browse button, Import/Clear on their own row).
- Game pauses automatically when the window loses focus and resumes when it returns (a manual pause stays paused; off during netplay).
- Rumble: no more false "hurt" pulses while walking; stronger fire tick (A/X/Y).
- Settings menu text no longer clips.

## 0.3.0

- First official **Linux (x86-64)** build: `GunmansProof-v0.3.0-linux-x86_64.tar.gz`.
  Needs glibc 2.39 or newer (Ubuntu 24.04+, Fedora 40+, Debian 13+), OpenGL and
  FreeType. SDL is built in.

## 0.2.0 (in progress)

- Nothing yet.

## 0.1.0

First release.

- True widescreen (16:9, 16:10, 21:9, custom) for gameplay, NPC dialogue and the
  pause screen. Unused side space stays black; the character stays centred.
- HUD stays pinned to the screen corners (floating over the black side area is fine); widescreen survives screen shake.
- Text speed (x1 / x2 / x3 / instant) and faster room fades.
- PAD FEEDBACK: very light controller rumble when firing and when losing a heart.
- Skip intro with Start, save states, rewind, fast-forward.
- DualSense / gamepad support, pixel-art settings menu (Insert), Gunman's Proof
  styled launcher with an original cartridge art and app icon.

- Fixes: "4:3 ORIGINAL" is now remembered after a restart; SIDE EDGES = BLACK
  shows only the plain 4:3 picture (no HUD or sprites in the side bars).

Known limits: Windows build is cross-compiled and lightly tested; projectile and
effect sprites can pop at the original screen edge; rooms wider than 512 px and
the first boss have not been checked; audio speeds up during fast room fades.
