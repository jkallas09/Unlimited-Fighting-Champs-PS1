# Unlimited Fighting Champs — PS1 prototype

Original PlayStation homebrew combat prototype using PSn00bSDK v0.24. This is a 2D placeholder with a CPU opponent, health, stamina, strikes, blocking and match restart.

## Download the game

Open this repository's **Actions** tab, select the latest successful **Build PS1 BIN-CUE** run, and download **Unlimited-Fighting-Champs-PS1** under Artifacts (GitHub sign-in required). Extract the ZIP, keep the BIN and CUE in the same folder, and open `Unlimited_Fighting_Champs.cue` in DuckStation. Supply your own legally obtained PlayStation BIOS.

The workflow runs on pushes to main, pull requests and manual dispatch. It downloads the pinned official SDK release, verifies its SHA-256, compiles the game, verifies the executable and disc contents, and uploads the BIN, CUE, standalone EXE and checksums.

## Build locally

Install CMake 3.21+, Python 3 and the official PSn00bSDK v0.24 Linux release. Add the SDK's `bin` folder to PATH and set `PSN00BSDK_LIBS` to its absolute `lib/libpsn00b` path. Then run:

```sh
bash build_disc.sh
```

Outputs appear in `output/`. Validation checks the PS-X EXE header and RAM range, MODE2/2352 CUE format, ISO9660 PlayStation identifier, boot configuration and exact executable bytes in the disc. This does not replace emulator gameplay testing.

## Controls

D-pad moves; Square jabs; Triangle power punches; Cross kicks; Circle heavy kicks; L1/L2 block. Cross restarts after a match.

## Build fixes

- Use the SDK executable and CD-image CMake functions with C, C++ and ASM enabled.
- Include psxetc.h for synchronization APIs.
- Align GPU command buffers and append primitives in drawing order so backgrounds do not cover fighters.
- Use a single backslash in the SYSTEM.CNF boot path.
- Use deterministic disc sources instead of selecting an arbitrary executable from the build folder.

No commercial assets, music or BIOS are included. Emulator gameplay validation is still pending.
