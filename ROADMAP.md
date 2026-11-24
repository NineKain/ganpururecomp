# Roadmap

## 0.3.0: Linux release

- Ship a Linux x86-64 tar.gz (done for 0.3.0; see CHANGELOG).
- Test on more distros and Steam Deck; build on an older base image for wider
  glibc compatibility (or an AppImage).
- Wayland / gamepad / audio checks on real hardware.

## 0.2.0 candidates

Verification gaps from 0.1.0 (fix or confirm first):
- Run the Windows build on real hardware (never run; cross-compiled only).
- Check the first boss: widescreen/HUD during screen shake (limit is 24 px).
- Check the beach map and other rooms, including rooms wider than 512 px.
- Projectile and effect sprites popping at the original screen edge.
- Audio during fast-forwarded room fades.
- Controller rumble feel on a real DualSense (strengths in `src/main.c`).

QoL ideas (not started):
- Auto-resume (framework `Autosave` option exposed in the menu).
- Pause on focus loss.
- Screenshot hotkey, quick save / quick load keys.
- Skip any cutscene with Start (the way skip-intro works).
- Hold a button to rush dialogue.
- Optional assist settings (off by default).

Project:
- GitHub Action that builds and tests the Windows exe.
- README screenshots / GIF of widescreen.
- Resolve the licence mismatch (GPL-3.0 repo vs PolyForm Noncommercial `snesrecomp`).
