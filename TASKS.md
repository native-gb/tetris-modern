# Clean Native Tetris completion checklist

This checklist implements `PLAN.md`. A checked item requires working code and
proportionate evidence; it is not a progress estimate.

## Repository and build

- [x] Add CMake Debug, Release and ASan/UBSan presets with strict warnings.
- [x] Import pinned Gubsy and `gb-audio` with local-sibling fallbacks.
- [x] Add `scripts/build.sh`, `scripts/run.sh`, F5 tasks and floating window launch.
- [x] Add ignored ROM documentation and exact revision validation.
- [x] Keep a clean checkout independent from GBRE, mGBA and private repositories.

## Content

- [x] Define immutable typed `GameContent` catalogs.
- [x] Validate the v1.1 ROM size, header, checksums and SHA-1.
- [x] Extract all tiles, fonts, tilemaps, layouts and metasprites.
- [x] Extract piece, gravity and Type-B tables; encode instruction-defined score
      and level behavior as named semantic rules.
- [x] Extract demos, name-entry/high-score presentation and every ending sequence.
- [x] Extract all 17 music programs and 14 sound effects.
- [x] Attach exact generated provenance to every extracted object.
- [x] Add round-trip, count, range and overlap/accountability tests.

## Headless game

- [x] Implement board, row completion, collapse and garbage insertion.
- [x] Implement all pieces, orientations, spawns and collision geometry.
- [x] Implement original randomizer and deterministic sample injection.
- [x] Implement semantic input, DAS, repeat, soft drop and rotation.
- [x] Implement gravity, locking, line-clear phases and top-out.
- [x] Implement scoring, statistics, levels and saturation behavior.
- [x] Implement Type A and every rocket threshold.
- [x] Implement Type B and every height/level completion route.
- [x] Implement pause, reset, game over, retry and menu returns.
- [x] Implement high-score qualification, entry and persistence.
- [x] Implement both original demos and attract cycling.
- [x] Implement complete local two-player match and garbage semantics.
- [x] Implement deterministic replay with rules and pacing identity.

## Presentation and flow

- [x] Implement copyright/startup, title and configuration screens.
- [x] Implement exact original gameplay/status/statistics layouts.
- [x] Implement preview, active piece, board and clear animations.
- [x] Implement demos, game over, retry, high-score and scoreboard views.
- [x] Implement every dancer, musician, Mario, Luigi, rocket and Buran sequence.
- [x] Implement original local-versus presentation.
- [x] Separate simulation ticks from high-refresh rendering.
- [x] Support integer scaling, fit, resize, borderless, 4K and ultrawide.

## Audio

- [x] Implement the game-specific music driver over `gb-audio`.
- [x] Implement every sound-effect script and priority interaction.
- [x] Match all 17 driver-state traces.
- [x] Verify music-off, pause, game-over, endings and fast pacing transitions.
- [x] Provide effect/music volume controls and an F1 audition panel.
- [ ] Complete a physical-device PCM likeness pass.

## Required enhancements

- [x] Implement persistent Original, Enhanced and Custom profiles.
- [x] Implement Original, Fast and Instant line-clear pacing.
- [x] Implement original 160x144 and native Widescreen Frame presentation.
- [x] Implement the procedural four-color star background.
- [x] Implement Off, Subtle and Full effects.
- [x] Implement landing/Tetris/game-over shake.
- [x] Implement center-out line burn and embers.
- [x] Implement the Tetris background pulse.
- [x] Implement reduced-motion and reduced-flash overrides.
- [x] Make Enhanced recommended while retaining complete Original behavior.

## Application, input and F1 tools

- [x] Implement keyboard/controller actions, hot-plug and persistence.
- [x] Implement independent local two-player device assignment.
- [x] Disable ImGui controller navigation and preserve gameplay ownership.
- [x] Implement the compact F1 launcher and independent semantic windows.
- [x] Add board/piece/randomizer/input/event inspection.
- [x] Add pause, tick, replay and explicit state setup commands.
- [x] Add content/provenance and audio inspection.
- [x] Add settings, display and performance inspection.
- [x] Keep GBRE, mGBA, WRAM and source-map tooling outside the runtime UI.

## Verification and promotion

- [x] Complete a final C+- simplicity and readability audit across every domain;
      reject Game Boy-shaped state, reference-port transliteration, needless
      indirection, and abstractions that make the rules harder to follow.
- [x] Remove avoidable indirection, vague ownership, encoded state and framework machinery.
- [x] Confirm core rules can be followed directly from input through state change.
- [x] Port focused semantic vectors from the reference evidence.
- [x] Cover every original mode, selection, ending and multiplayer result.
- [x] Add extraction, domain, flow, replay, audio and render tests.
- [x] Add SDL virtual-controller application acceptance.
- [x] Pass strict Debug and Release suites.
- [x] Pass the complete ASan/UBSan suite.
- [x] Pass ROM and clean-checkout audits.
- [ ] Complete manual keyboard/controller playthroughs and layout/audio passes.
- [x] Document every intentional difference and remaining optional idea.
- [ ] Confirm no known missing behavior, content, enhancement or blocker remains.
- [ ] Promote repository names only after every preceding item is satisfied.

## Automated evidence

Verified on 2026-07-18 against ROM SHA-1
`74591cc9501af93873f9a5d3eb12da12c0723bbc`:

- strict Debug, Release and ASan/UBSan builds each pass all 10 CTest suites with
  ROM extraction, audio traces, render tests and virtual-controller acceptance;
- the isolated no-ROM checkout builds with pinned public dependencies and passes
  all 10 ROM-independent suites;
- all 17 music driver traces match and all 14 effect programs are covered;
- all 24 private original-vs-modern semantic scenarios pass, covering both
  startup paths, complete menu traversal, controls/DAS/pause/preview/SFX, the
  full attract cycle, rocket and Buran schedules, and all 17 music entries;
- production gameplay consumes authenticated ROM-derived gravity and all 28
  tetromino geometries through immutable `GameplayData`, with no duplicate
  built-in production table;
- an X11 application smoke validates ROM loading, window/controller/ImGui setup,
  canonical rendering and clean three-frame shutdown;
- the clean executable contains no GBRE, mGBA or private-RE runtime dependency.

The unchecked items are deliberately physical/manual gates, not hidden
implementation work. Repository promotion remains last.
