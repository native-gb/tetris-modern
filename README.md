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
./scripts/check-clean-checkout.sh
```

F5 in VS Code builds and launches the same executable in a centered, resizable
1280x720 floating window. F11 toggles borderless fullscreen. The game boots
directly to the title; the original copyright page remains available through
the debug screen picker.

## Controls

| Action | Player one | Player two | Controller |
|---|---|---|---|
| Move | Arrows or WASD | IJKL | D-pad |
| Back | Z | U | East face button |
| Confirm | X | O | South face button |
| Rotate counterclockwise | Z | U | South face button |
| Rotate clockwise | X | O | East face button |
| Hard / sonic drop | Space or Up | H or I | D-pad Up or either trigger |
| Hold piece | C | T | North face button, either bumper, or left-stick click |
| Quick restart | R | N | North face or Select/Back in pause and game-over menus |
| Start / pause | Enter | P | Start |
| Main menu (paused / game over) | Z | U | East face button |
| Select / preview | Backspace | Y | Back |
| Quit | Escape | — | Guide or right-stick click |

Start+Back or F2 opens the persistent Controls window. It edits the live Gubsy
profiles for either player and supports adding, replacing, removing, resetting,
and swapping clockwise/counterclockwise bindings. Start+Back closes it for controller-only
play. Its Controllers tab lists detected devices, rescans browser controllers,
and assigns each device to Player 1 or Player 2. The same tab reports raw browser,
SDL joystick, SDL gamepad-mapping, and Gubsy-open counts so controller discovery
failures identify the layer that rejected the device. Two connected controllers are
assigned independently. ImGui gamepad
navigation is disabled, so the F1 tools never take controller ownership from the game.
Host-frame button edges remain buffered until a fixed game step consumes them,
so a short tap is not lost between simulation steps.

On-screen prompts use Nintendo legends by default. Auto detection, Xbox, and
PlayStation legends can be selected from F1 > Accessibility without changing the
position-based bindings themselves.

Modern pause and game-over panels expose the complete session flow. Start resumes.
North/Y or Select/Back retries the current setup, Back returns to the title, and Confirm continues
from game over into the original high-score and level-selection flow. The direct
menu escape is application navigation rather than a gameplay enhancement. The R key
remains a direct in-game retry; every retry route is controlled by the Quick Restart option.

## Modes and rendering

The implementation includes Type A, Type B, both attract demos, high scores and
name entry, all rocket and Type-B endings, the complete music/effect catalog,
and a local two-player replacement for the link-cable game.

The renderer builds canonical 160x144 scenes before composing them into the
host window. Original mode uses lossless integer scaling. Widescreen mode keeps
the Game Boy scene sharp and adds a native-resolution host frame. Modern mode
can extend the scene to 25 visible tile columns when they fit without reducing the
integer scale of the original view. Rendering is independent of the 59.7275 Hz
fixed simulation step.
The GPU tile-atlas renderer is the default, with a selectable CPU software
raster fallback that consumes the same drawing code. VSync and a hard
60/120/144/165/240 Hz presentation cap are independent. Optional active-piece
interpolation uses the fixed-step accumulator without adding game updates. It is
off by default, rendering current integer state at a forced 60 Hz; higher rates
are useful only when interpolation is explicitly enabled. Inactive windows
block on SDL events and resume without simulation catch-up. External hosts such
as the browser can instead keep sampling input and fixed steps while skipping
presentation above the selected cap; they own visibility suspension and VSync.

Four persistent profiles are available:

- Original: original line-clear timing, 160x144 composition, hardware-limited
  sound effects, and no host effects.
- Enhanced: faster line clears, optional moving widescreen snow, subtle semantic effects,
  independently mixed sound effects, hard drop, separate clockwise/counterclockwise inputs,
  side rotation pushout, Guideline piece colors, adjustable 12/2-frame DAS/ARR, and a
  6-frame entry delay. This is the default;
  original generation, gravity, locking, scoring, and goals remain unchanged.
- Modern: the Enhanced presentation plus hard drop, a landing ghost, hold, five
  previews, seven-bag randomization, SRS wall/floor kicks, lock delay, frame-adjustable
  horizontal repeat, spawn-input buffering, Guideline piece colors, modern scoring, quick restart, and
  Type-A Marathon, 40-line Sprint, or three-minute Ultra play. Its responsive
  extended HUD is enabled by default.
- Custom: granular gameplay, handling, assistance, randomizer, scoring, visual,
  audio, motion, flash, and volume settings.

Enhanced effects include landing, Tetris, and game-over shake; center-out line
burn and embers; a Tetris background pulse; and depth-scaled title motion assembled
from the original moon, city, onion-tower, and snow/star tiles. Screen
shake, background motion, title parallax, the extended canvas, and reduced flash
are independent settings. Snow is the thematic default; four-point pixel stars
and a solid dark-green background remain available.

Modern rules are opt-in. Original and Enhanced therefore remain directly useful
for the 1989 game, while Modern supplies the conveniences expected by current
Tetris players. The grey ghost marks the exact hard-drop landing cells; it does
not alter collision or reserve board cells.

Horizontal handling is configured in simulation frames: DAS is the delay before
a held direction repeats, ARR is the interval between repeated shifts, and Instant
Autorepeat moves to the wall when DAS expires. Enhanced and Modern default to 12-frame
DAS and 2-frame ARR, matching TETR.IO's published defaults and the commonly measured
Tetris 99 values. Nintendo does not publish Tetris 99's internal frame constants, so
the latter remains a community measurement rather than an exact compatibility claim.
The adjacent ARE control sets the delay between locking one piece and beginning the next
entry or line-clear phase. Enhanced and Modern use the commonly measured Tetris 99 value
of 6 frames; Original retains the reimplemented Game Boy timing of 3 frames.

Modern Type B retains the original garbage-height setup, 25-line objective, and
completion scenes. It adds the same handling, hold, five-piece queue, randomizer,
lock delay, ghost, and live modern scoring as Type A. Its HUD reports starting
height and remaining lines, and its result screen preserves the score accumulated
during play instead of recalculating the original deferred total.

## F1 tools

F1 opens player settings and a collapsed Debug category. Debug Tools then
launches independent inspection windows, including a separate Debug Overlays
window, covering:

- semantic game, screen, board, piece, and event state;
- held and pressed inputs, DAS, gravity, and assigned devices;
- randomizer samples, history, hidden piece, queue, hold, combo, T-spin, and
  lock-delay state;
- deterministic replay, pause, single step, and explicit state setup;
- extracted content and ROM provenance;
- audio channels, meters, songs, and effect audition;
- rendering profiles, accessibility, window modes, and timing;
- tile grids, sprite bounds, and board-cell collision overlays.

These tools operate on the clean domains. They do not embed mGBA, GBRE, WRAM
editors, assembly maps, or private scenario data.

## Code layout

```text
src/content/       ROM validation and typed runtime extraction
src/game/          deterministic state, rules, menus, demos, and endings
src/video/         canonical rendering, layouts, and effects
src/audio/         Tetris music/effect driver and SDL output
src/storage/       high-score file persistence
src/settings.*     persisted gameplay, video, accessibility, and audio choices
src/frame.*        time pump, input collection, simulation steps, and frame rendering
src/window.*       SDL/Gubsy window setup, events, and display requests
src/main.cpp       initialization, the three-phase main loop, and cleanup
tests/             state, rules, extraction, audio, render, and controller tests
tools/             standalone semantic trace adapters for development audits
```

The code follows the repository `AGENTS.md` C+- style: game, replay, and
audio-driver state are transparent structs; plain state is read directly; and
operations are methods only when they naturally belong to one value. Private
classes are limited to SDL audio ownership. Video textures live in a transparent
`Video` struct operated on by free functions. There are no getter walls,
`edit_*` accessors, inheritance hierarchies, or Game Boy-shaped gameplay
ownership. `main.cpp` owns each independent domain directly; after initialization
its loop only begins a frame, advances fixed game steps, and renders. `PLAN.md`
records the architecture and completion standard;
`TASKS.md` records verification evidence.

Authenticated gravity and piece geometry enter the headless game through one
small immutable `GameplayData` value; the simulation retains neither ROM
offsets nor duplicate production tables. Rules encoded by original
instructions—score bases, thresholds, menu limits, and progression formulas—are
written as named game rules instead of being disguised as synthetic ROM tables.

## Intentional differences

- Local two-player play replaces the original link-cable transport.
- Rendering and audio do not preserve hardware stalls or emulator slowdown.
- Host files persist enhancement settings, bindings, and high scores.
- Unreachable cartridge code, memory-reuse accidents, and hardware-specific
  bugs are not represented as game architecture. The unused deuce/advantage
  rendering data is extracted for provenance but has no reachable rule path.
- Enhanced rendering is the recommended classic default. Modern is the opt-in
  contemporary ruleset. Original rules and timing remain independently
  selectable and tested.

## Legal boundary

This repository contains original source code only. It does not contain the
game ROM, extracted Nintendo/Bullet-Proof Software assets, assembly, emulator
states, or private reverse-engineering evidence. Users must supply their own
compatible ROM; extracted content remains in memory and is not redistributed.

The project is not affiliated with or endorsed by The Tetris Company, Nintendo,
Bullet-Proof Software, or any other rights holder. Original source in this
repository is licensed under GPL-3.0-only. That license grants no rights to
third-party game content or trademarks.

`./scripts/audit-distribution.py` checks the current publication inputs and all
reachable Git history for ROMs, extracted media, cartridge-authored presentation
text, and private-runtime coupling. Optional `--rom` and `--artifact` arguments
also reject copied cartridge windows and private markers in built executables.
