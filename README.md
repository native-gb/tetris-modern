# Native GB Tetris

Native C++20 reimplementation of the 1989 Game Boy Tetris release. Game logic
runs as ordinary C++ code. The program validates a user-supplied ROM and
extracts its graphics, layouts, demos, music, and sound effects at startup. It
does not emulate the Game Boy CPU or reproduce its memory layout.

This repository is the clean implementation. Reverse-engineering evidence,
assembly, emulator scenarios, and byte maps remain in the private RE workspace.

## ROM

The only supported input is the 32 KiB World revision 1.1 ROM:

```text
Title     TETRIS
Revision  1
SHA-1     74591cc9501af93873f9a5d3eb12da12c0723bbc
```

Place it at:

```text
roms/Tetris (JUE) (V1.1) [!].gb
```

ROMs, extracted assets, save states, and captures are ignored by Git. An
incompatible size, cartridge header, checksum, revision, or hash is rejected
with a specific error.

## Build and run

Requirements are CMake 3.21 or newer, Ninja, Git, and a C++20 compiler. CMake
uses local sibling checkouts of Gubsy and `gb-audio` when present. Otherwise it
fetches their pinned revisions.

```bash
./scripts/run.sh
```

Other useful commands:

```bash
./scripts/build.sh dev
./scripts/test.sh dev
./scripts/test.sh release
./scripts/test.sh asan
```

F5 in VS Code builds and launches the same executable in a centered, resizable
640x576 floating window. F11 toggles borderless fullscreen.

## Controls

| Action | Player one | Player two | Controller |
|---|---|---|---|
| Move | Arrows or WASD | IJKL | D-pad |
| Rotate left / B | Z | U | West face button |
| Rotate right / A | X | O | South face button |
| Start / pause | Enter | P | Start |
| Select / preview | Backspace | Y | Back |
| Quit | Escape | — | Guide or right-stick click |

Start+Back opens Gubsy's persistent controller and keyboard binding screen. Two
connected controllers are assigned independently. ImGui gamepad navigation is
disabled, so the F1 tools never take controller ownership from the game.

## Modes and presentation

The implementation includes Type A, Type B, both attract demos, high scores and
name entry, all rocket and Type-B endings, the complete music/effect catalog,
and a local two-player replacement for the link-cable game.

The renderer builds canonical 160x144 scenes before composing them into the
host window. Original mode uses lossless integer scaling. Widescreen mode keeps
the Game Boy scene sharp and adds a native-resolution host frame. Rendering is
independent of the 59.7275 Hz fixed simulation tick.

Three persistent presentation profiles are available:

- Original: original line-clear timing, 160x144 composition, and no host effects.
- Enhanced: faster line clears, widescreen stars, and subtle semantic effects.
- Custom: independent line-clear, layout, background, effect, motion, flash,
  music-volume, and effect-volume settings.

Enhanced effects include landing, Tetris, and game-over shake; center-out line
burn and embers; and a Tetris background pulse. Reduced Motion and Reduced Flash
override the relevant effects.

## F1 tools

F1 opens a small launcher for independent windows covering:

- semantic game, flow, board, piece, and event state;
- held and pressed inputs, DAS, gravity, and assigned devices;
- randomizer samples, history, hidden piece, and retry count;
- deterministic replay, pause, single tick, and explicit state setup;
- extracted content and ROM provenance;
- audio channels, meters, songs, and effect audition;
- presentation profiles, accessibility, window modes, and timing;
- tile grids, sprite bounds, and board-cell collision overlays.

These tools operate on the clean domains. They do not embed mGBA, GBRE, WRAM
editors, assembly maps, or private scenario data.

## Code layout

```text
src/content/       ROM validation and typed runtime extraction
src/game/          deterministic headless game and flow
src/presentation/  canonical rendering, layouts, effects, and settings
src/audio/         Tetris music/effect driver and SDL output
src/app/           Gubsy window, input, persistence, and F1 tools
tests/             domain, flow, extraction, audio, render, and controller tests
```

The code follows the repository `AGENTS.md` C+- style: direct state, plain
functions, concrete names, modest abstraction, and no Game Boy-shaped gameplay
ownership. `PLAN.md` records the architecture and completion standard;
`TASKS.md` records verification evidence.

## Intentional differences

- Local two-player play replaces the original link-cable transport.
- Rendering and audio do not preserve hardware stalls or emulator slowdown.
- Host files persist enhancement settings, bindings, and high scores.
- Unreachable cartridge code, memory-reuse accidents, and hardware-specific
  bugs are not represented as game architecture. The unused deuce/advantage
  presentation data is extracted for provenance but has no reachable rule path.
- Enhanced presentation is the recommended default. Original rules and timing
  remain independently selectable and tested.

## Legal boundary

This repository contains original source code only. It does not contain the
game ROM, extracted Nintendo/Bullet-Proof Software assets, assembly, emulator
states, or private reverse-engineering evidence. Users must supply their own
compatible ROM; extracted content remains in memory and is not redistributed.

The project is not affiliated with or endorsed by The Tetris Company, Nintendo,
Bullet-Proof Software, or any other rights holder. Original source in this
repository is licensed under GPL-3.0-only. That license grants no rights to
third-party game content or trademarks.
