# Unlimited Fighting Champs — PS1 3D prototype v0.2

Original PlayStation homebrew combat game built with PSn00bSDK v0.24.

## What changed in v0.2

- Replaced the rectangle fighters with articulated, flat-shaded polygon models: tapered torsos, octagonal heads, arms, gloves, shorts, legs and feet.
- Added an eight-sided 3D arena with mat markings, posts, three rails and audience silhouettes.
- Added a perspective tracking camera with automatic distance adjustment and two selectable angles.
- Added walk cycles, punch extensions, raised kicks, guard poses, hit flashes and knockout falls.
- Strikes now land at the contact frame of their animation and can miss if the opponent moves away.
- Added body separation, pause, winner messages and knockout/time-limit results.

This remains a deliberately simple early 3D prototype. There are no textures, music, sound effects, grappling, submissions, character selection or finished fighter animations yet. v0.1 was confirmed working by the user in emulation. v0.2 has compile/disc validation, host combat simulation tests and a software preview of the same polygon draw commands; its emulator gameplay and performance still need testing.

## Download and play

Open **Actions**, select the latest successful **Build PS1 BIN-CUE** run, and download **Unlimited-Fighting-Champs-PS1** from Artifacts. Extract the ZIP, keep BIN and CUE together, and open `Unlimited_Fighting_Champs.cue` in DuckStation with your own legally obtained BIOS.

## Controls

| Button | Action |
| --- | --- |
| D-pad left/right | Move across the arena |
| D-pad up/down | Move into/out of the arena |
| Square | Jab |
| Triangle | Power punch |
| Cross | Kick / rematch after result delay |
| Circle | Heavy kick |
| L1 or L2 | Guard |
| Start | Pause / resume |
| Select | Change camera angle |

Blue is the player; red is the CPU. Green bars show health; gold bars show stamina. Fights last 90 seconds or end with a knockout. Higher remaining health wins at the time limit.

## Build locally

Install CMake 3.21+, Python 3 and the official PSn00bSDK v0.24 Linux release. Put the SDK `bin` folder on PATH and set `PSN00BSDK_LIBS` to its absolute `lib/libpsn00b` path, then run:

```sh
bash build_disc.sh
```

Outputs are in `output/`. The pinned SDK archive is SHA-256 checked by GitHub Actions. `verify_build.py` checks the executable header and RAM range, MODE2/2352 CUE format, ISO9660 PlayStation identifier, boot configuration and exact executable bytes within the disc.

Run combat regression checks on a host with a C compiler:

```sh
cc -std=c99 -O2 combat_test.c -o combat_test
./combat_test
```

These cover contact timing, one hit per attack, guard reduction, insufficient stamina, depth misses, knockout falls, rematch, pause and 15,000 randomized simulation frames.

## Source

`main.c` contains the integer-only world projection, GPU depth ordering, procedural fighter geometry, arena, input, animation and combat. No commercial game assets or BIOS are included.
