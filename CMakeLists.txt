cmake_minimum_required(VERSION 3.21)
project(unlimited_fighting_champs LANGUAGES C CXX ASM)
psn00bsdk_add_executable(ufc_ps1 GPREL main.c)
target_compile_options(ufc_ps1 PRIVATE -O2 -Wall -Wextra -Werror=implicit-function-declaration)
psn00bsdk_add_cd_image(iso Unlimited_Fighting_Champs disc.xml
    DEPENDS ufc_ps1 "${PROJECT_SOURCE_DIR}/SYSTEM.CNF")

psn00bsdk_target_incbin(ufc_ps1 PRIVATE walkout_music walkout_music.adpcm)
