![sm64coopdx Logo](textures/segment2/custom_coopdx_logo.rgba32.png)

# SM64 Coop DX Wii U

This fork is an experimental Wii U porting effort for [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx).

## Goal

The goal of this project is to explore how far sm64coopdx can be ported to the Wii U,
starting with an offline `.rpx` target before attempting any network features.
This is also a personal learning project to get hands-on experience with C
and homebrew development for consoles.

## Current Status

This Wii U port is in an very early experimental stage.

The project now builds with the Wii U toolchain and can produce a testable RPX.
In Cemu, the port is not playable yet, but it no longer crashes during this stage, and some textures (such as the ground, castle, and grass) now load correctly.

## Credits

- [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx), created and maintained by the Coop Deluxe Team.
- [sm64ex-coop](https://github.com/djoslin0/sm64ex-coop), the earlier project that Coop DX continues and expands on.
- [sm64-port by aboood40091](https://github.com/aboood40091/sm64-port), used as the main reference for Wii U platform files, GX2 rendering, Wii U shaders, and controller support.
- [devkitPro](https://devkitpro.org/) and the Wii U homebrew toolchain.

## Legal Notice

This repository does not include ROMs, extracted game assets, or generated files derived from Nintendo content.
Building this project requires users to provide their own legally obtained US Super Mario 64 ROM.


