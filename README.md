# native-gb-tetris-modern

Experimental clean C++ reimplementation of the 1989 Game Boy release of
Tetris. This repository is intended to become the canonical player-facing
port after it demonstrates complete behavior and content parity with the
fidelity-oriented reference implementation.

The implementation will execute game logic natively, use SDL 3 through Gubsy,
and extract required graphics, layouts, demos, music, effects, and gameplay
tables from a compatible ROM supplied by the user. It will not emulate the
Game Boy CPU or preserve its memory layout.

This repository does not contain a ROM, extracted game assets, emulator data,
or reference assembly. It is not affiliated with or endorsed by The Tetris
Company, Nintendo, Bullet-Proof Software, or any other rights holder.

## Status

Planning scaffold. Full implementation is governed by [PLAN.md](PLAN.md) and
[TASKS.md](TASKS.md). Do not describe this project as complete or recommend it
over the reference port until every promotion gate in those documents passes.

The supported input is the 32 KiB World revision 1.1 ROM:

```text
SHA-1  74591cc9501af93873f9a5d3eb12da12c0723bbc
```

## Repository roles

```text
native-gb-tetris-modern/  clean domain-oriented implementation experiment
native-gb-tetris/         fidelity-oriented executable reference
native-gb-tetris-re/      private ROM mapping, scenarios, and oracle evidence
GBRE/                     generic reverse-engineering and comparison tools
gb-audio/                 reusable native Game Boy-style synthesizer
```

The modern game may use the other workspaces as development evidence. It must
build and run without the private repository, GBRE, mGBA, or the reference
simulation.

## License

Original source in this repository is licensed under GPL-3.0-only. That
license grants no rights to third-party game content or trademarks.
