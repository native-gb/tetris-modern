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
- [x] Extract demos, name-entry/high-score rendering and every ending sequence.
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
- [x] Implement deterministic replay with rules, gameplay options, and pacing identity.

## Rendering and screen state

- [x] Implement copyright/startup, title and configuration screens.
- [x] Implement exact original gameplay/status/statistics layouts.
- [x] Implement preview, active piece, board and clear animations.
- [x] Implement demos, game over, retry, high-score and scoreboard views.
- [x] Implement every dancer, musician, Mario, Luigi, rocket and Buran sequence.
- [x] Implement original local-versus rendering.
- [x] Separate simulation steps from high-refresh rendering.
- [x] Support integer scaling, fit, resize, borderless, 4K and ultrawide.

## Audio

- [x] Implement the game-specific music driver over `gb-audio`.
- [x] Implement every sound-effect script and priority interaction.
- [x] Match all 17 driver-state traces.
- [x] Verify music-off, pause, game-over, endings and fast pacing transitions.
- [x] Provide effect/music volume controls and an F1 audition panel.
- [ ] Complete a physical-device PCM likeness pass.

## Required enhancements

- [x] Implement persistent Original, Enhanced, Modern and Custom profiles.
- [x] Implement Original, Fast and Instant line-clear pacing.
- [x] Implement original 160x144 and native Widescreen Frame rendering.
- [x] Implement the procedural four-color star background.
- [x] Implement Off, Subtle and Full effects.
- [x] Implement landing/Tetris/game-over shake.
- [x] Implement center-out line burn and embers.
- [x] Implement the Tetris background pulse.
- [x] Implement reduced-motion and reduced-flash overrides.
- [x] Implement polyphonic native SFX while retaining Original channel stealing.
- [x] Implement Modern hard/sonic drop, ghost, hold, previews and quick restart.
- [x] Implement Modern DX repeat, spawn buffering, SRS and lock delay.
- [x] Implement seven-bag/history randomizers and modern scoring.
- [x] Implement Type-A Marathon, 40-line Sprint and three-minute Ultra.
- [x] Make Enhanced recommended while retaining complete Original behavior.

## Tetris 99-style handling pass

- [x] Separate Confirm and Back from clockwise/counterclockwise rotation actions.
- [x] Make Up hard drop in contemporary play and use modern physical-position defaults:
      south face counterclockwise, east face clockwise, north face Hold.
- [x] Bind both controller bumpers to Hold; remove bumper rotation defaults.
- [x] Add one-click clockwise/counterclockwise binding swap in the Controls window.
- [x] Add side rotation pushout to Enhanced and retain full SRS for Modern.
- [x] Replace the fixed DX repeat toggle with frame-exact DAS and ARR controls plus
      optional instant autorepeat.
- [x] Expose entry delay (ARE), using 6 frames in Enhanced/Modern and retaining the
      3-frame Game Boy timing in Original.
- [x] Add Guideline piece colors for the board, active piece, Hold, and Next, enabled by
      default in Enhanced and Modern.
- [x] Use a centered 1280x720 desktop window by default.
- [x] Keep all work local until the browser build receives explicit promotion approval.

Research note: Nintendo's published Tetris controls use Up for hard drop, A for
clockwise rotation, and B for counterclockwise rotation. On a Switch controller
those are the east and south physical buttons respectively, matching TETR.IO-style
east-clockwise and south-counterclockwise placement. Nintendo's Tetris 99 instructions
assign Hold to either L or R. TETR.IO publishes 12-frame DAS and 2-frame ARR as its
defaults; community measurements report the same values for Tetris 99, but Nintendo
does not publish those internal constants. This port therefore defaults to 12/2 while
keeping both values explicit and adjustable.

## Application, input and F1 tools

- [x] Implement keyboard/controller actions, hot-plug and persistence.
- [x] Implement independent local two-player device assignment.
- [x] List, rescan, and reassign connected controllers in the Controls window.
- [x] Provide direct ImGui management for both persistent Gubsy binding profiles.
- [x] Remove the per-cell board mutation editor from the player-facing tools.
- [x] Buffer host-frame button edges until the next fixed game step.
- [x] Add GPU-atlas and CPU-raster output, optional active-piece interpolation,
      independent VSync, hard 60/120/144/165/240 Hz limits, and inactive sleep.
- [x] Expose an explicit external-host policy for nonblocking browser pacing,
      browser-managed VSync, and page-owned suspension.
- [x] Disable ImGui controller navigation and preserve gameplay ownership.
- [x] Implement the compact F1 launcher and independent semantic windows.
- [x] Separate player settings, debug tools, and debug overlays in the F1 UI.
- [x] Add board/piece/randomizer/input/event inspection.
- [x] Add pause, step, replay and explicit state setup commands.
- [x] Add content/provenance and audio inspection.
- [x] Add settings, display and performance inspection.
- [x] Keep GBRE, mGBA, WRAM and source-map tooling outside the runtime UI.

## Verification and promotion

- [x] Complete a final C+- simplicity and readability audit across every domain;
      reject Game Boy-shaped state, reference-port transliteration, needless
      indirection, and abstractions that make the rules harder to follow.
- [x] Keep domain state in transparent structs with direct field reads; reserve
      private classes for concrete SDL/GPU/audio resource ownership.
- [x] Remove avoidable indirection, vague ownership, encoded state and framework machinery.
- [x] Confirm core rules can be followed directly from input through state change.
- [x] Port focused semantic vectors from the reference evidence.
- [x] Cover every original mode, selection, ending and multiplayer result.
- [x] Add extraction, domain, state, replay, audio and render tests.
- [x] Add SDL virtual-controller application acceptance.
- [x] Pass strict Debug and Release suites.
- [x] Pass the complete ASan/UBSan suite.
- [x] Pass ROM and clean-checkout audits.
- [ ] Complete manual keyboard/controller playthroughs and layout/audio passes.
      Required hands-on acceptance:
      - navigate startup, Type A, Type B, pause, preview, retry and quit once
        with keyboard and once with a physical controller;
      - hot-plug the controller and confirm player-one/player-two assignment;
      - compare Original and Enhanced at windowed, resized and borderless sizes;
      - audition all songs and effects for wrong notes, rhythm, envelopes,
        channel balance, clipping, stuck tones or missing cues;
      - inspect gameplay, game over, local versus, rocket and Buran layouts.
- [x] Document every intentional difference and remaining optional idea.
- [ ] Confirm no known missing behavior, content, enhancement or blocker remains.
- [ ] Promote repository names only after every preceding item is satisfied.

## Automated evidence

Verified through 2026-07-19 against ROM SHA-1
`74591cc9501af93873f9a5d3eb12da12c0723bbc`:

- strict Debug, Release and ASan/UBSan builds each pass all 10 CTest suites with
  ROM extraction, audio traces, render tests and virtual-controller acceptance;
- focused Modern tests cover seven-bag uniqueness, hold limits, hard and sonic
  drop, ghost rendering, DX repeat, SRS kicks, lock delay, combo/back-to-back/
  T-spin scoring, Sprint, Ultra, quick restart, replay identity and settings migration;
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
