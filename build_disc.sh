#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${PSN00BSDK_LIBS:?Set PSN00BSDK_LIBS to the SDK lib/libpsn00b directory}"
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PSN00BSDK_LIBS/cmake/sdk.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
mkdir -p output
cp build/Unlimited_Fighting_Champs.{bin,cue} build/ufc_ps1.exe output/
python3 verify_build.py
