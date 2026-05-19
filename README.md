![sm64coopdx Logo](textures/segment2/custom_coopdx_logo.rgba32.png)

# SM64 Coop DX Wii U

This fork is an experimental Wii U porting effort for [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx).

The goal is to explore whether Coop DX can be adapted into a Wii U Homebrew Launcher build, starting with an offline `.rpx` target before attempting any network features. This is being done as a preservation and homebrew learning project.

## Current Status

This Wii U port is in an early experimental stage.

The project now builds with the Wii U toolchain and can produce a testable RPX. In Cemu, startup has progressed past filesystem setup, ROM lookup, and part of the asset loading flow, reaching the early loading/UI path. The port is not playable yet and still crashes during later initialization, currently around audio/runtime compatibility work.

The build and packaging flow is still provisional, and some diagnostics are being used while the port is being stabilized.

## Credits

- [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx), created and maintained by the Coop Deluxe Team.
- [sm64ex-coop](https://github.com/djoslin0/sm64ex-coop), the earlier project that Coop DX continues and expands on.
- [sm64-port by aboood40091](https://github.com/aboood40091/sm64-port), used as the main reference for Wii U platform files, GX2 rendering, Wii U shaders, and controller support.
- [devkitPro](https://devkitpro.org/) and the Wii U homebrew toolchain.

## Legal Notice

This repository does not include ROMs, extracted game assets, or generated files derived from Nintendo content. Building this project requires users to provide their own legally obtained Super Mario 64 ROM.

