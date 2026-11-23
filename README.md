# Gunman's Proof Recomp

A native PC recompilation of **Gunman's Proof** (SNES), built on
[snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp), with true
widescreen, 60 fps and a pixel-art in-game settings menu.

> **You must legally own a copy of the game.** No ROM data is distributed with
> this project, in the repository or in any release. The recompiled C is
> generated locally from your own copy and is never committed.

## Features

- **True widescreen** (16:9 / 16:10 / 21:9 / custom): the world is drawn into
  the margins instead of stretching the 4:3 picture. Gameplay, NPC dialogue
  and the pause screen are widescreen; the map, cutscenes, title and menus stay
  at the authentic 4:3. The character stays centred; the view never slides.
- Score / life HUD can sit in the screen corners (HUD IN CORNERS).
- Faster **text speed** (x1 / x2 / x3 / instant) and **faster room fades**.
- Skip intro with Start, rewind (F12), fast-forward (Tab), save states.
- DualSense / gamepad support, rebindable keys.
- **Insert** opens a pixel-art settings menu in-game.

## Status

Playable. Tested on Linux; the Windows build is cross-compiled and has had
less testing. Untested: long playthroughs, the name-entry screen, and audio during the
fast-forwarded room fades (it speeds up; harmless).

## ROM identity

| | |
|---|---|
| File | `72f6229a-Gunmans_Proof_English_v1.03.sfc` |
| Publisher | — |
| Developer | — |
| Year | — |
| Mapping | lorom |
| Region | JPN (Japan) |
| Coprocessor | none |
| CRC32 | `acdee757` |
| SHA-256 | `b18e09347f056c572f7e0e220ca204a642246d0d9825b2c817f3edf30c22ed8c` |

`tools/regen.sh` refuses to run against anything else, so a mismatched
revision fails immediately instead of producing subtly wrong output.

## Build

```sh
bash tools/regen.sh --rom /path/to/72f6229a-Gunmans_Proof_English_v1.03.sfc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The ROM does not have to live in the repository — keeping it on your own
drive is the better habit, and `SNESRECOMP_ROM` sets the path once for a
shell. Without either, `tools/regen.sh` finds `72f6229a-Gunmans_Proof_English_v1.03.sfc` at the repo root,
then the path the setup wizard recorded in `rom.cfg`; `.gitignore` blocks
both from ever being committed.

`tools/regen.sh` verifies the ROM, generates `src/gen/*.c`, and re-syncs
`recomp/funcs.h`. Re-run it whenever you change anything under `recomp/`.

## Run

```sh
./build/GunmansProofSNESRecomp                     # launcher picks the ROM
./build/GunmansProofSNESRecomp /path/to/72f6229a-Gunmans_Proof_English_v1.03.sfc # or name it and skip the launcher
```

With no ROM on the command line the build opens the recomp-ui launcher: a ROM
picker with a verification badge, plus display / audio / input settings. Your
choice is cached in `rom.cfg` beside the executable, so the next launch opens
on it and "Skip launcher on boot" makes it immediate. If the launcher is not
built in (`--no-recomp-ui`), the host falls back to a native file picker.

Either way the ROM is checked against the digests above before anything boots
— a dump that could not have produced this build is refused at the door rather
than mis-executing ten frames in.

## Layout

| Path | What lives there |
|---|---|
| `recomp/` | Analysis input: `bank*.cfg`, `symbols.toml`, generated `funcs.h` |
| `rom_identity.txt` | ROM digests — read by the build, `tools/regen.sh` and CI |
| `src/` | Host code you own: `main.c`, `game_rtl.c` |
| `src/gen/` | Generated C. Never committed — regenerate locally |
| `snesrecomp/` | Framework, vendored with local changes (see `patches/`, `framework_pins.txt`) |
| `recomp-ui/` | Launcher UI, vendored with the Gunman theme (see `patches/`) |
| `launcher_assets/` | Launcher artwork (original pixel cartridge) |
| `patches/` | Diffs of the local changes against the pinned upstream commits |
| `tools/` | `regen.sh` — the ROM → C pipeline |
| `scripts/` | `package_release.sh` — player-facing zip |
| `framework_pins.txt` | Exact framework commits this project was scaffolded against |

## Multiplayer

Single player.

## Credits

Built on [snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp) and
[recomp-ui](https://github.com/mstan/recomp-ui) (vendored, with their own licenses).

## License

This project's own source is under the license in `LICENSE`. The framework
carries its own terms — see `snesrecomp/LICENSE` and
`snesrecomp/THIRD_PARTY_ATTRIBUTION.md`. Neither covers the game data, which
is not distributed here.
