# Changelog

## 0.1.0

First release.

- True widescreen (16:9, 16:10, 21:9, custom) for gameplay, NPC dialogue and the
  pause screen. Unused side space stays black; the character stays centred.
- HUD follows the edges of the widescreen picture; widescreen survives screen shake.
- Text speed (x1 / x2 / x3 / instant) and faster room fades.
- Skip intro with Start, save states, rewind, fast-forward.
- DualSense / gamepad support, pixel-art settings menu (Insert), Gunman's Proof
  styled launcher with an original cartridge art and app icon.

Known limits: Windows build is cross-compiled and lightly tested; projectile and
effect sprites can pop at the original screen edge; rooms wider than 512 px and
the first boss have not been checked; audio speeds up during fast room fades.
