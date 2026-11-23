# Gunman's Proof Recomp

A native PC port of **Gunman's Proof** (SNES), built by static recompilation on
[snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp). True
widescreen, 60 fps, gamepad support and a pixel-art in-game settings menu.

> **You need your own copy of the game.** No ROM data is in this repository or
> in any release. The recompiled C is generated locally from your ROM and is
> never committed.

## Features

- **True widescreen** (16:9, 16:10, 21:9, custom). The world is drawn into the
  margins; the picture is never stretched. Gameplay, NPC dialogue and the pause
  screen are widescreen. Map, cutscenes, title and menus stay at the original 4:3.
- The character stays centred and the view never slides.
- Optional score/life HUD in the screen corners.
- **Text speed**: normal, x2, x3 or instant.
- **Room fades**: normal, fast (default) or faster.
- **Skip intro**: press Start during the opening story.
- Save states, rewind, fast-forward.
- DualSense, Xbox and other gamepads (SDL3), remappable controls.
- Press **Insert** in-game for the settings menu.

Nothing in the ROM is modified. Text speed, fades and skip-intro run the game's
own routines more often or earlier.

## Play (Windows release)

1. Put your ROM next to the exe, named `GunmansProof.sfc`, or pick it in the
   launcher, or drag it onto the exe.
2. Run `GunmansProofSNESRecomp.exe`.

Your ROM must match the checked revision (see [ROM identity](#rom-identity)).

## Controls

| Action | Keyboard / pad |
|---|---|
| Settings menu | `Insert`, or PS / Guide button |
| Load / save state | `F1`-`F10` / `Shift+F1`-`F10` |
| Save-state menu | `F11` |
| Rewind | `F12` |
| Fast-forward | `Tab` |
| Pause | `P` |
| Reset | `Ctrl+R` |
| FPS counter | `F` |
| Fullscreen | `Alt+Enter` |
| Window size | `Ctrl+Up` / `Ctrl+Down` |
| Volume | Keypad `+` / `-` |

DualSense: Cross = B, Circle = A, Square = Y, Triangle = X, L1/R1 = L/R,
Create = Select, Options = Start. Everything is remappable in the menu, and
hotkeys can be edited in `config.ini`.

## Settings menu

| Row | What it does |
|---|---|
| VIDEO | Fullscreen, window size, smoothing, vsync, frame blend, sprite limit, FPS counter |
| WIDESCREEN | Aspect ratio, side edges (EXTEND / MIRROR / BLACK), wide actors, HUD in corners |
| VOLUME | Master volume |
| CONTROLS | Keyboard and controller remapping |
| SKIP INTRO | Start skips the opening story (on by default) |
| TEXT SPEED | NORMAL / X2 / X3 / INSTANT |
| ROOM FADES | NORMAL / FAST / FASTER |

Known limits: projectile and effect sprites can still pop at the original
screen edge, rooms wider than 512 px are untested, and audio speeds up during
the fast-forwarded room fades.

## Build from source

Requirements: CMake, a C/C++ compiler, SDL3 build dependencies, and a Rust
toolchain (used once to build the analyzer).

```sh
git clone https://github.com/NineKain/ganpururecomp.git && cd ganpururecomp
bash tools/regen.sh --rom /path/to/your/Gunmans_Proof.sfc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/GunmansProofSNESRecomp
```

`tools/regen.sh` verifies the ROM, generates `src/gen/*.c` and syncs
`recomp/funcs.h`. Re-run it after changing anything under `recomp/`. The path
can also be set once with `SNESRECOMP_ROM`. `scripts/package_release.sh` makes
the player-facing zip.

## ROM identity

| | |
|---|---|
| Title | Gunman's Proof |
| Mapping | LoROM, no coprocessor |
| Region | JPN |
| Size | 3,145,728 bytes |
| CRC32 | `acdee757` |
| SHA-256 | `b18e09347f056c572f7e0e220ca204a642246d0d9825b2c817f3edf30c22ed8c` |

Anything else is refused before it boots.

## Repository layout

| Path | Contents |
|---|---|
| `src/main.c`, `src/gun_menu.c` | Game-specific host code: widescreen, hooks, menu |
| `recomp/`, `rom_identity.txt` | Analysis config and ROM digests |
| `src/gen/` | Generated C. Never committed |
| `snesrecomp/`, `recomp-ui/` | Vendored framework and launcher UI (see `patches/`, `framework_pins.txt`) |
| `launcher_assets/` | Launcher artwork and app icon (original art: cartridge, sheriff star) |
| `tools/`, `scripts/` | ROM-to-C pipeline, release packaging |

## Credits

Built on [snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp) and
[recomp-ui](https://github.com/mstan/recomp-ui), vendored with their own licenses.

## License

This project's own source is GPL-3.0 (`LICENSE`). The vendored `snesrecomp/`
framework is under the **PolyForm Noncommercial License 1.0.0**
(`snesrecomp/LICENSE`, `snesrecomp/THIRD_PARTY_ATTRIBUTION.md`), so builds that
include it are for non-commercial use. `recomp-ui/` has its own `LICENSE`.
The game data is not covered and is not distributed here.
