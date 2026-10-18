# Unlimited Fighting Champs — PS1 beta v0.4

Original PlayStation homebrew combat game built with PSn00bSDK v0.24.

## What changed in beta v0.4

The game now boots into a title screen. Press **Start** to open the mode menu, select **Fight Now**, then choose one of four fighters before the match begins.

| Playable fighter | Nickname | Appearance |
| --- | --- | --- |
| Conor McSnack | The Notoriously Hungry | Green shorts, auburn hair and beard, lean build |
| Jon Boneless | Bones? What Bones? | Purple shorts, dark cropped hair, taller build |
| Brock Lunchner | The Lunch Break | Red shorts, blond buzz cut, heavyweight build |
| Chuck Liddoodle | The Iced Latte | Blue shorts, dark mohawk and beard |

These are fictional comic parody names. All four are selectable and playable against a different randomly chosen CPU opponent. Combat rules are shared in this beta; their current differences are appearance and proportions.

- Added title, mode select and character select screens with a live 3D preview.
- Start advances from the title; X or Start confirms menu choices.
- D-pad cycles through all four fighters; Circle goes back.
- Held buttons cannot skip through multiple screens.
- Pause a match, then press Circle to return to character select. After a result, wait for the short delay, then use Cross to rematch or Circle to choose another fighter.
- The match HUD displays the selected fighter names.

Only **Fight Now** is enabled in this beta. There is no career, tournament, multiplayer, music, sound, grappling or submissions yet. v0.3 was confirmed running by the user in emulation. v0.4 has automated menu/combat tests, visual inspection of software previews, compilation and disc validation; its new screens still need an emulator check.

## Download and play

Open **Actions**, select the latest successful **Build PS1 BIN-CUE** run, and download **Unlimited-Fighting-Champs-PS1** from Artifacts. Extract the ZIP, keep BIN and CUE together, and open `Unlimited_Fighting_Champs.cue` in DuckStation with your own legally obtained BIOS.

## Controls

| Button | Action |
| Start on title | Open mode select |
| X / Start in menus | Confirm choice |
| D-pad in character select | Choose fighter |
| Circle in menus | Back |
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

The selected fighter is the player; the other fighter is the CPU. Green bars show health; gold bars show stamina. Fights last 90 seconds or end with a knockout. Higher remaining health wins at the time limit.

## Build locally

Install a host C compiler, CMake 3.21+, Python 3 and the official PSn00bSDK v0.24 Linux release. Put the SDK `bin` folder on PATH and set `PSN00BSDK_LIBS` to its absolute `lib/libpsn00b` path, then run:

```sh
bash build_disc.sh
```

Outputs are in `output/`. The pinned SDK archive is SHA-256 checked by GitHub Actions. `verify_build.py` checks the executable header and RAM range, MODE2/2352 CUE format, ISO9660 PlayStation identifier, boot configuration and exact executable bytes within the disc.

Run combat regression checks on a host with a C compiler:

```sh
cc -std=c99 -O2 combat_test.c -o combat_test
./combat_test
```

These also cover title/mode/roster navigation, input latching, all four playable selections, distinct CPU picks, back navigation and rematches. Combat checks cover contact timing, one hit per attack, guard reduction, insufficient stamina, depth misses, knockout falls, rematch, pause and 15,000 randomized simulation frames.

## Source

`main.c` contains the integer-only world projection, GPU depth ordering, procedural fighter geometry, arena, input, animation and combat. No commercial game assets or BIOS are included.
