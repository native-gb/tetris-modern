# Clean Native Tetris implementation plan

## Mission

Build a complete, clean, domain-oriented C++20 reimplementation of Game Boy
Tetris (JUE) revision 1.1. It must reproduce every meaningful reachable feature
and all original content, retain every enhancement already shipped by the
fidelity-oriented native reference port, and remain easier to understand,
test, modify, render at modern resolutions, and extend.

This is a clean implementation, not a cleanup branch of the reference port.
Do not begin by copying its simulation files and renaming fields. Use the
reference implementation as an executable specification and source of focused
tests while designing native state around the actual game domains.

“Modern” describes the architecture and host presentation. It does not silently
replace the original rules with seven-bag randomization, SRS, hold, ghost
pieces, or guideline scoring. Such rules remain future, separately identified
game modes unless the user explicitly expands this plan.

## Repository roles and eventual promotion

During development:

```text
native-gb-tetris-modern  clean implementation under this plan
native-gb-tetris         fidelity-oriented executable reference
native-gb-tetris-re      private original evidence and scenario corpus
GBRE                     generic mapping and comparison workbench
```

After every completion gate passes:

```text
native-gb-tetris            canonical clean player-facing implementation
native-gb-tetris-reference  fidelity-oriented executable specification
native-gb-tetris-re         private original evidence and oracle material
```

Do not rename or promote repositories merely because the clean game is
playable. Promotion requires complete content, flows, enhancements, tests,
controller operation, and clean-checkout verification.

## Instructions and code quality

Follow `AGENTS.md`. Use the local `gauche`, `Splonks/splonks-cpp`, and
`Splonks/gubsy` projects as application and style references. Prefer the kind
of small, direct, legible code associated with Redis or a focused single-header
library: concrete state, explicit updates, short call chains, and names that
describe game concepts.

The repository's C+- style is a completion requirement, not a secondary polish
goal. The clean implementation should read like a small native game written
from first principles: ordinary data, plain functions, obvious ownership, and
the minimum machinery needed to express the rules. Preserve original behavior
through clear domain concepts, not through Game Boy-shaped variables, encoded
state, compatibility scaffolding, or a line-by-line transliteration of the
reference port.

Avoid architecture theater:

- no generic ECS for seven pieces and a small number of presentation actors;
- no service locator, dependency-injection framework, event bus framework, or
  manager-of-managers;
- no inheritance hierarchy for pieces, screens, effects, or game modes;
- no `utils` dumping ground;
- no templated abstraction whose only purpose is removing a few repeated lines;
- no original WRAM addresses, packed state aliases, or assembly routine order
  in gameplay ownership;
- no use of animation IDs as hidden gameplay state;
- no raw ROM offsets outside extraction and provenance modules.

Prefer value types, enums, fixed-size arrays, spans, explicit commands, and
plain functions. Integer or fixed-point arithmetic is preferred for
deterministic pixel behavior; floating point belongs at host presentation
boundaries where exact simulation identity does not depend on it.

Code quality is a first-class deliverable, equal to functional completeness.
A working port that is still crunchy, assembly-shaped, over-abstracted, or
difficult to read does not satisfy this plan. Before completion, perform a
dedicated simplicity audit across every domain: remove accidental state and
indirection, replace vague or encoded names, flatten unnecessary call chains,
split oversized files only along cohesive ownership boundaries, and confirm
that a reader can follow each important rule from input to state change without
reverse-engineering the C++ itself. Do not use the clean rewrite as an excuse
for cleverness; the smallest clear design wins.

## Clean architecture boundary

The repository should remain mostly flat, with files grouped into a few clear
directories only where ownership benefits:

```text
src/content/       ROM validation, extraction, immutable content catalogs
src/game/          headless deterministic game domains
src/presentation/  layouts, indexed rendering, animation, effects
src/audio/         Tetris driver, effects, gb-audio state and SDL output bridge
src/app/           Gubsy window, host input, settings, application loop
src/debug/         F1 development UI and explicit debug commands
tests/             domain, integration, extraction, render and application tests
```

The headless game library must not include SDL, Gubsy, ImGui, filesystem
settings, ROM parsing, audio devices, or rendering. It consumes immutable
`GameContent`, a `Rules` value, and explicit per-tick commands. It publishes a
stable snapshot and semantic events.

Recommended game domains are concrete rather than generic:

- `Board`: 10x18 occupied cells, row completion, collapse and garbage insertion;
- `Tetromino`: piece kind, orientation, occupied cells and spawn geometry;
- `PieceGenerator`: original divider/history algorithm and deterministic samples;
- `PlayerInput`: held/pressed actions, DAS, repeat, rotation and soft drop;
- `FallingPiece`: movement, gravity, collision, rotation, locking and top-out;
- `Scoring`: line awards, soft-drop points, BCD/display saturation and statistics;
- `SinglePlayer`: Type A/Type B goals, level/speed progress and completion;
- `VersusMatch`: two boards, shared generation, attacks, wins and match flow;
- `GameFlow`: startup, title, menus, demo, play, pause, results, high scores and endings;
- `Replay`: rules identity, initial state, random samples and tick commands;
- `GameEvents`: meaningful facts consumed by presentation and audio.

Do not force each bullet into a class. A small struct plus cohesive free
functions is often clearer.

## Immutable ROM content

Support the exact 32 KiB World revision 1.1 ROM with SHA-1
`74591cc9501af93873f9a5d3eb12da12c0723bbc`. Reject incompatible size,
metadata, checksums, revision, and hash with a clear error.

Extraction produces one immutable, typed `GameContent` value. It should contain
named catalogs for:

- 1bpp and 2bpp tiles, fonts and palettes;
- tilemaps, menus and screen layouts;
- metasprites and presentation actor frames;
- piece shapes, orientations and spawn data;
- gravity, piece, and Type-B data tables, plus named semantic rules for score
  and level behavior encoded in original instructions;
- demo inputs and deterministic demo piece data;
- high-score defaults and name-entry presentation data;
- dancers, musicians, Mario, Luigi, rockets and Buran sequences;
- all 17 music programs, instruments and channel sequences;
- all 14 sound-effect definitions and register scripts.

Do not generate or commit enormous C++ arrays containing extracted Nintendo
data. Decode from the validated user ROM at startup. A future cache may live in
ignored user data, but it must be versioned by ROM hash, extraction schema and
output hashes.

Every extracted object records source provenance: ROM profile, exact range,
decoder, expected size/count and validation result. Lossless transforms receive
round-trip or byte-accounting tests. Gameplay and presentation consume named
catalog objects, never offsets.

## Mapping and traceability

Heavy byte mapping remains in `native-gb-tetris-re` and GBRE. The clean
repository retains only:

- automatically generated content provenance;
- stable semantic behavior IDs in focused tests;
- occasional `GBRE:` anchors at domain or function granularity;
- a generated semantic coverage summary;
- documentation for intentional behavior changes.

Do not maintain clean C++ line ranges against assembly. One clean function may
replace several original routines, and one original routine may inform several
clean domains. The modern application must not link GBRE, mGBA, private
scenarios, or the reference simulation at runtime.

## Complete original behavior

Implement every reachable original feature:

- copyright/startup, title, attract loop and both recorded demos;
- complete configuration UI and navigation;
- Type A and Type B at every selectable level, height and music setting;
- all seven pieces, all orientations, exact occupied cells and spawn behavior;
- original preview behavior and preview toggle;
- original randomizer, divider sampling, history rejection and forced candidate;
- movement, DAS, repeat, soft drop, rotation, rollback and collision;
- gravity levels 0-20, locking, line detection, clear phases, collapse and top-out;
- scoring, soft-drop accounting, line statistics, level progression and saturation;
- pause, reset, game over, retry and return-to-menu flows;
- high-score qualification, entry, storage and presentation;
- Type-A rockets and every threshold/sequence;
- every Type-B height ending, dancers, musicians and Buran material;
- all original animations and presentation actors;
- original music selection, music-off behavior and every sound cue;
- complete local two-player replacement for link play, including independent
  devices, shared piece semantics, attacks, garbage, wins, draws and match flow.

Preserve observable rules and timing where they affect play. Do not preserve
hardware stalls, memory reuse, arbitrary-code behavior, or meaningless
intermediate bytes. Understand unusual behavior before deciding whether it is
an original rule, a compatibility option, or a corrected default.

## Required enhancements

Carry forward every enhancement currently implemented in the reference port:

- persistent Original, Enhanced and Custom profiles with schema migration;
- Original, Fast and Instant fixed-tick line-clear pacing;
- exact original 160x144 presentation with integer scaling;
- native-resolution Widescreen Frame presentation;
- four-color procedural star background;
- Off, Subtle and Full effect intensity;
- landing, Tetris and game-over impact shake;
- center-out line burn with palette-indexed embers;
- Tetris background pulse;
- reduced-motion and reduced-flash overrides;
- borderless fullscreen, resizable aspect-correct windowing and integer-scale
  behavior already available in the reference application.

Enhanced is the recommended player-facing preset; Original remains a complete,
tested compatibility preset. Pacing and rules identities are stored in replay
and high-score metadata when they can affect outcomes.

Implement effects from semantic events. Never infer a Tetris, lock or ending
by scraping rendered pixels. Render the canonical original scene to a
palette-limited 160x144 target, then compose it into the host scene. Keep sharp
gameplay pixels authoritative while effects and backgrounds use separate layers.

Deferred ideas in the old enhancement roadmap—SRS, hold, seven-bag, Sprint,
Marathon, ghost pieces, arbitrary next queues and a general compositor toybox—
are not required for this bounded clean-port goal.

## Presentation

Use a resolution-independent presentation model with four coordinate spaces:

- board/layout coordinates;
- original 160x144 Game Boy composition coordinates;
- native logical presentation coordinates;
- final host pixels.

Original mode must reproduce all menus, gameplay panels, statistics, high
scores, demos and ending compositions from ROM-derived layout and sprite data.
Pixel-perfect comparison is a verification aid, not permission to let the
renderer own game rules.

Render independently from the deterministic simulation tick. High-refresh
displays must remain smooth without accelerating play. Window resize, display
changes, focus transitions, integer scaling, fit mode, borderless fullscreen,
1080p, 1440p, 4K and ultrawide composition must remain stable.

## Audio

Use the shared `gb-audio` library through `native-gb::audio`. Tetris-specific
music bytecode, instruments, effect priority, sequencing and ROM extraction
remain local, explicit code.

Reproduce all original music and effects closely enough that manual listening
does not reveal obvious wrong notes, envelopes, rhythm, looping, channel
ownership or stuck effects. Preserve music-off behavior, pause behavior,
ending cues and fast/instant pacing transitions. Keep music and effect volume
separate at the host mix boundary.

Driver-state tests should compare the complete 17-song catalog against existing
reference traces. PCM likeness receives focused tests and manual audition, but
the clean architecture must not become an embedded emulator.

## Input, controllers and application

Use Gubsy and SDL 3. Provide keyboard and controller operation, hot-plugging,
rebinding, persistence and independent two-player device assignment. A player
must be able to launch, navigate every menu, play, pause, retry and quit without
a keyboard.

Keep ImGui controller navigation disabled so game input is never captured by
developer UI. Host input becomes explicit semantic actions and rising edges
before entering the fixed-tick simulation. Render frequency must not alter DAS,
rotation, pause, menu edges or gameplay.

F5 must build and launch the actual game in a normal floating window without
terminal/debugger workarounds. `scripts/run.sh` and build presets must provide
the same path outside VS Code.

## F1 development interface

Keep the F1 interface small and specific to the clean architecture. Provide a
launcher and independent windows for:

- game flow, mode, score, level, timers and current semantic events;
- board cells, active/preview pieces, collision positions and completed rows;
- randomizer history, supplied samples and upcoming deterministic decisions;
- held/pressed inputs, DAS/repeat state and assigned devices;
- replay recording, playback, seed/state setup, pause and single-tick stepping;
- immutable content catalogs and source provenance;
- audio driver/channel state and effect audition;
- Original/Enhanced/Custom settings, accessibility and display controls;
- simulation, render and audio timing/performance.

Debug mutations enter through explicit commands at simulation boundaries and
are visible in replay/debug state. Closing the UI must not alter gameplay.

Do not port the reference application’s internal WRAM views, byte editors,
CPP/assembly/ROM map panels, pinned mGBA process, scenario coordinator frontend,
or literal-state mismatch tables. Those remain external development tools.

## Verification strategy

Reuse evidence, not architecture. Convert relevant reference behavior into
small domain tests and semantic vectors. Tests should identify behavior using
stable names such as `TETRIS-RNG-003`, not original line numbers.

Required automated coverage includes:

- every piece/orientation, spawn and boundary collision;
- original randomizer for all divider samples and history branches;
- DAS, repeat, rotation, soft drop, gravity, lock and top-out;
- all line-clear sizes, scoring levels, saturation and statistics;
- all Type-A and Type-B selections, progression and completion routes;
- every menu, pause, retry, high-score, demo and ending transition;
- local two-player attacks, garbage, wins, losses, draws and device isolation;
- every extracted catalog and source range;
- all music/effect driver programs;
- Original and Enhanced settings, pacing and effects events;
- deterministic replay round trips;
- original-layout render landmarks and native-layout resize behavior;
- controller-only application smoke through SDL virtual devices.

Provide Debug, Release and ASan/UBSan presets with strict warnings as errors.
The clean checkout must build and run ROM-independent tests without private
siblings. ROM tests register when the ignored compatible ROM exists. External
differential scenarios may use GBRE and the reference port, but they must not
become runtime dependencies or force tick-for-frame identity where semantic
equivalence is intended.

## Execution order

1. Establish build presets, Gubsy/gb-audio dependencies, scripts, F5 and strict
   test scaffolding.
2. Define immutable `GameContent`, validate the ROM and port extraction with
   provenance and round-trip tests.
3. Implement the headless board, pieces, randomizer, input timing, falling,
   collision, locking, clearing and scoring domains.
4. Implement complete Type A, Type B, menus, high scores, demos and game flow.
5. Implement complete local two-player semantics and device-independent match
   state.
6. Implement original presentation, every animation and every ending from the
   content catalogs.
7. Implement the Tetris audio driver on `gb-audio` and all original programs.
8. Implement the complete Gubsy application, controllers, persistence and
   player-facing display behavior.
9. Implement every required reference enhancement using clean events and
   presentation layers.
10. Implement the bounded F1 interface and deterministic replay/state tools.
11. Run semantic differential checks, full build matrix, controller smoke,
    manual playthroughs, audio/layout passes and clean-checkout audit.
12. Update documentation and only then evaluate repository promotion.

Work in dependency-driven vertical slices, but continue until the definition
of done is actually satisfied. A playable board or one completed mode is not a
terminal milestone.

## Definition of done

The clean implementation is complete only when:

- every original single-player, demo, ending and local two-player feature is
  implemented and reachable from normal application flow;
- all original audiovisual and gameplay content is extracted from the validated
  user ROM with exact provenance and no tracked extracted material;
- the simulation contains no copied reference core, WRAM-shaped ownership,
  placeholder flow, stub behavior or unexplained magic state;
- a final C+- simplicity audit finds no avoidable framework machinery, vague
  ownership, encoded gameplay state, needless indirection, or function/file
  structure that remains difficult to follow;
- every currently shipped reference enhancement is present and independently
  configurable;
- Original mode passes semantic/reference comparisons and Enhanced mode passes
  its own deterministic tests;
- keyboard and controller-only play, two-player assignment, hot-plugging,
  windowing, fullscreen and high-refresh rendering work;
- F1 tools provide the clean semantic inspection needed for future work without
  embedding private RE machinery;
- strict Debug and Release builds, ASan/UBSan, all unit/integration/ROM/render/
  audio/controller/replay tests and clean-checkout tests pass;
- README, controls, ROM requirements, build/run/test instructions, architecture,
  enhancement behavior, intentional differences and legal boundaries are
  accurate;
- manual playtesting finds no known blocker, missing content path, obvious
  presentation defect or obviously wrong audio.

Do not mark the goal complete because the remaining work is inconvenient,
because the reference implementation exists, or because a subset of tests
passes. Record concrete blockers and continue through every safe in-scope task.
