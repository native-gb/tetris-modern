# Clean Native Tetris completion checklist

This checklist implements `PLAN.md`. A checked item requires working code and
proportionate evidence; it is not a progress estimate.

## Repository and build

- [ ] Add CMake Debug, Release and ASan/UBSan presets with strict warnings.
- [ ] Import pinned Gubsy and `gb-audio` with local-sibling fallbacks.
- [ ] Add `scripts/build.sh`, `scripts/run.sh`, F5 tasks and floating window launch.
- [ ] Add ignored ROM documentation and exact revision validation.
- [ ] Keep a clean checkout independent from GBRE, mGBA and private repositories.

## Content

- [ ] Define immutable typed `GameContent` catalogs.
- [ ] Validate the v1.1 ROM size, header, checksums and SHA-1.
- [ ] Extract all tiles, fonts, tilemaps, layouts and metasprites.
- [ ] Extract all piece, gravity, score, level and Type-B tables.
- [ ] Extract demos, high-score data and every presentation sequence.
- [ ] Extract all 17 music programs and 14 sound effects.
- [ ] Attach exact generated provenance to every extracted object.
- [ ] Add round-trip, count, range and overlap/accountability tests.

## Headless game

- [ ] Implement board, row completion, collapse and garbage insertion.
- [ ] Implement all pieces, orientations, spawns and collision geometry.
- [ ] Implement original randomizer and deterministic sample injection.
- [ ] Implement semantic input, DAS, repeat, soft drop and rotation.
- [ ] Implement gravity, locking, line-clear phases and top-out.
- [ ] Implement scoring, statistics, levels and saturation behavior.
- [ ] Implement Type A and every rocket threshold.
- [ ] Implement Type B and every height/level completion route.
- [ ] Implement pause, reset, game over, retry and menu returns.
- [ ] Implement high-score qualification, entry and persistence.
- [ ] Implement both original demos and attract cycling.
- [ ] Implement complete local two-player match and garbage semantics.
- [ ] Implement deterministic replay with rules and pacing identity.

## Presentation and flow

- [ ] Implement copyright/startup, title and configuration screens.
- [ ] Implement exact original gameplay/status/statistics layouts.
- [ ] Implement preview, active piece, board and clear animations.
- [ ] Implement demos, game over, retry, high-score and scoreboard views.
- [ ] Implement every dancer, musician, Mario, Luigi, rocket and Buran sequence.
- [ ] Implement original local-versus presentation.
- [ ] Separate simulation ticks from high-refresh rendering.
- [ ] Support integer scaling, fit, resize, borderless, 4K and ultrawide.

## Audio

- [ ] Implement the game-specific music driver over `gb-audio`.
- [ ] Implement every sound-effect script and priority interaction.
- [ ] Match all 17 driver-state traces.
- [ ] Verify music-off, pause, game-over, endings and fast pacing transitions.
- [ ] Provide effect/music volume controls and an F1 audition panel.
- [ ] Complete a physical-device PCM likeness pass.

## Required enhancements

- [ ] Implement persistent Original, Enhanced and Custom profiles.
- [ ] Implement Original, Fast and Instant line-clear pacing.
- [ ] Implement original 160x144 and native Widescreen Frame presentation.
- [ ] Implement the procedural four-color star background.
- [ ] Implement Off, Subtle and Full effects.
- [ ] Implement landing/Tetris/game-over shake.
- [ ] Implement center-out line burn and embers.
- [ ] Implement the Tetris background pulse.
- [ ] Implement reduced-motion and reduced-flash overrides.
- [ ] Make Enhanced recommended while retaining complete Original behavior.

## Application, input and F1 tools

- [ ] Implement keyboard/controller actions, hot-plug and persistence.
- [ ] Implement independent local two-player device assignment.
- [ ] Disable ImGui controller navigation and preserve gameplay ownership.
- [ ] Implement the compact F1 launcher and independent semantic windows.
- [ ] Add board/piece/randomizer/input/event inspection.
- [ ] Add pause, tick, replay and explicit state setup commands.
- [ ] Add content/provenance and audio inspection.
- [ ] Add settings, display and performance inspection.
- [ ] Keep GBRE, mGBA, WRAM and source-map tooling outside the runtime UI.

## Verification and promotion

- [ ] Complete a final C+- simplicity and readability audit across every domain;
      reject Game Boy-shaped state, reference-port transliteration, needless
      indirection, and abstractions that make the rules harder to follow.
- [ ] Remove avoidable indirection, vague ownership, encoded state and framework machinery.
- [ ] Confirm core rules can be followed directly from input through state change.
- [ ] Port focused semantic vectors from the reference evidence.
- [ ] Cover every original mode, selection, ending and multiplayer result.
- [ ] Add extraction, domain, flow, replay, audio and render tests.
- [ ] Add SDL virtual-controller application acceptance.
- [ ] Pass strict Debug and Release suites.
- [ ] Pass the complete ASan/UBSan suite.
- [ ] Pass ROM and clean-checkout audits.
- [ ] Complete manual keyboard/controller playthroughs and layout/audio passes.
- [ ] Document every intentional difference and remaining optional idea.
- [ ] Confirm no known missing behavior, content, enhancement or blocker remains.
- [ ] Promote repository names only after every preceding item is satisfied.
