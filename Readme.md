# Unlimited Fighting Champs — PS1 beta v0.6

Original PlayStation homebrew combat game built with PSn00bSDK v0.24.

## What changed in beta v0.6

### Fighter models and animation

- Replaced stretching strike poses with a fixed-point, two-bone articulated rig. Arms and legs keep constrained lengths through anticipation, extension, recoil and recovery.
- Jab and rear power punch have separate chamber positions. The free hand protects the head, the shoulders rotate with the torso, and punches return to a guard.
- Body and high kicks chamber the knee, turn the hips, pivot the supporting stance and return the foot to the floor. High kicks lift the hip and reach higher than body kicks.
- Added cheek planes, nostrils, lip contours, hairline strands, beard detail, collar tendons, obliques, navel, original character ink, finger divisions, glove stitches, thumbs, foot tendons and toe divisions.
- Shorts have moving thigh sections, contrast gussets, seams, waistband stitching and drawcords.

### Three-round fights

- Fights are scheduled for **three five-minute rounds**. The displayed clock runs at **2x speed**: one uninterrupted round takes approximately 2.5 real minutes at 60 Hz.
- An adult ring-card attendant walks across the arena with the next round number during each eight-second break. **Start** skips the break.
- Fighters return to their corners, recover stamina and six health points, then resume. Knockouts can end the match early.
- If all three rounds finish, a simple damage-based 10/9 scorecard determines the result; tied rounds score 10/10. This is a prototype scoring system, not a complete MMA judging model.

### Arena and presentation

- Expanded side grandstands with seats and individual spectators, aisle rails, ringside officials, production desks, glowing monitors, TV camera rigs, overhead trusses, animated lamps and padded-post straps.
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

Each character now has a different original eight-second entrance composition:

| Fighter | Track | Sound |
| --- | --- | --- |
| Conor McSnack | Emerald Ruckus | Bright plucked strings and stomping drums |
| Jon Boneless | Boneless After Dark | Sub bass, sparse bells and half-time drums |
| Brock Lunchner | Lunch Break Rampage | Synthesized power-chord distortion and heavy drums |
| Chuck Liddoodle | Iced Latte Overdrive | Bright electro-funk stabs and fast hats |

All four tracks were composed for this game using mathematically synthesized instruments, with no third-party samples, recordings or melody references. `compose_music.py` regenerates the 403,200-byte ADPCM bank, metadata and four decoded WAV previews. The bank fits entirely inside PS1 SPU RAM. A fighter's own track plays and fades during their entrance, whether they are the player or CPU.

### Four playable beta fighters

| Fighter | Nickname |
| --- | --- |
| Conor McSnack | The Notoriously Hungry |
| Jon Boneless | Bones? What Bones? |
| Brock Lunchner | The Lunch Break |
| Chuck Liddoodle | The Iced Latte |

They are fictional comic parody characters. Their beta differences are appearance and proportions; combat rules are shared. Fight Now is the available mode.

v0.6 has menu, walkout, round and physics regression tests, every-frame limb-constraint tests, software render inspection, PS1 compilation and disc validation. Actual emulator performance, the new camera angles and SPU audio still need checking. There is no career, tournament, multiplayer, grappling or submissions yet.

## Download and play

Open **Actions**, select the latest successful **Build PS1 BIN-CUE** run, and download **Unlimited-Fighting-Champs-PS1** from Artifacts. Extract the ZIP, keep BIN and CUE together, and open `Unlimited_Fighting_Champs.cue` in DuckStation with your own legally obtained BIOS.

## Controls

| Button | Action |
| --- | --- |
| Start on title | Open mode select |
| X / Start in menus | Confirm choice |
| D-pad in character select | Choose fighter |
| Circle in menus | Back |
| Start during walkouts / round breaks | Skip entrance / continue next round |
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

The selected fighter is the player; the other fighter is the CPU. Green bars show health; gold bars show stamina. Fights are three five-minute rounds with a 2x clock and can end early by knockout. A damage-based scorecard decides matches that reach the final bell.

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

These also cover all 27,000 fight ticks across three full rounds, interval auto/skip transitions, health/stamina recovery, decisions and every authored animation frame for all four profiles. They cover title/mode/roster navigation, input latching, all four playable selections, distinct CPU picks, automatic and skipped walkouts, back navigation and rematches. Physics checks include momentum, radial collision, guard coverage/direction, recoil, stun, guard breaks and late active-frame hits. Combat checks cover contact timing, one hit per attack, guard reduction, insufficient stamina, depth misses, knockout falls, rematch, pause and 15,000 randomized simulation frames.

## Source

`fighter_pose.h` contains the portable articulated animation rig. `main.c` contains the integer-only world projection, GPU depth ordering, procedural fighter geometry, arena, input, animation and combat. No commercial game assets or BIOS are included.
