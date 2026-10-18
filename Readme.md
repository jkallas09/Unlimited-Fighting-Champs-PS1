# Unlimited Fighting Champs — PS1 beta v0.5

Original PlayStation homebrew combat game built with PSn00bSDK v0.24.

## What changed in beta v0.5

### Arena and presentation

- Rebuilt the setting with a lit, subdivided canvas, mat branding, padded cage posts, diamond mesh, audience terraces, an entrance tunnel, colored light strips and arena lighting.
- Corrected the perspective camera's floor projection. The camera now follows the action, adjusts its distance and has three presets: broadcast, diagonal and ringside. Press **Select** to cycle them.
- Camera-side cage panels are hidden to keep the fighters visible.
- Redesigned the title screen with red/white sports graphics, a live 3D fighter and arena backdrop. The fight HUD now has named fighter panels, a central timer and segmented health bars.

### Fight physics

- Added acceleration, momentum, deceleration and circular body separation.
- Fighters turn toward each other in 3D; attack direction locks after the initial wind-up so sidesteps can evade strikes.
- Hits have an active window, one-hit-per-attack protection, recoil impulses, brief hit-stun and hit-stop. A hit can interrupt an opponent's wind-up.
- High and body guards cover different attacks and only defend from the front. Blocking consumes stamina; an exhausted guard can break.
- Attacks at the edge of reach and attacks thrown with low stamina do less damage. Guarding, attacking and fatigue affect movement.

These are grounded arcade combat rules, not a full rigid-body or ragdoll simulation. Grappling and submissions are not implemented yet.

### Fighter walkouts and original music

After selecting a character, both fighters get an entrance sequence with a tracking camera and name card. **Start** skips to the next entrance, then the fight. The entrances also finish automatically.

The walkout theme, **Main Event Pulse**, is an original eight-second synth/bass/drum composition generated for this project. It uses no third-party recordings, samples or melody references. The game uploads the ADPCM track to PS1 SPU RAM and plays/fades it during the walkouts. `compose_music.py` regenerates the track, the SPU asset and a decoded WAV preview with Python 3.

### Four playable beta fighters

| Fighter | Nickname |
| --- | --- |
| Conor McSnack | The Notoriously Hungry |
| Jon Boneless | Bones? What Bones? |
| Brock Lunchner | The Lunch Break |
| Chuck Liddoodle | The Iced Latte |

They are fictional comic parody characters. Their beta differences are appearance and proportions; combat rules are shared. Fight Now is the available mode.

v0.5 has menu, walkout and physics regression tests, software render inspection, PS1 compilation and disc validation. Actual emulator performance, the new camera angles and SPU audio still need checking. There is no career, tournament, multiplayer, grappling or submissions yet.

## Download and play

Open **Actions**, select the latest successful **Build PS1 BIN-CUE** run, and download **Unlimited-Fighting-Champs-PS1** from Artifacts. Extract the ZIP, keep BIN and CUE together, and open `Unlimited_Fighting_Champs.cue` in DuckStation with your own legally obtained BIOS.

## Controls

| Button | Action |
| --- | --- |
| Start on title | Open mode select |
| X / Start in menus | Confirm choice |
| D-pad in character select | Choose fighter |
| Circle in menus | Back |
| Start during walkouts | Skip to next entrance / fight |
| D-pad | Move around the arena |
| Square | Jab |
| Triangle | Power punch |
| Cross | Body kick / rematch after result delay |
| Circle | High kick |
| L1 | High guard |
| L2 | Body guard |
| Start in a match | Pause / resume |
| Circle while paused | Return to character select |
| Select | Cycle broadcast / diagonal / ringside camera |

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

These also cover title/mode/roster navigation, input latching, all four playable selections, distinct CPU picks, automatic and skipped walkouts, back navigation and rematches. Physics checks include momentum, radial collision, guard coverage/direction, recoil, stun, guard breaks and late active-frame hits. Combat checks cover contact timing, one hit per attack, guard reduction, insufficient stamina, depth misses, knockout falls, rematch, pause and 15,000 randomized simulation frames.

## Source

`main.c` contains the integer-only world projection, GPU depth ordering, procedural fighter geometry, arena, input, animation and combat. No commercial game assets or BIOS are included.
