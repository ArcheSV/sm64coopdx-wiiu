![sm64coopdx Logo](textures/segment2/custom_coopdx_logo.rgba32.png)

# SM64 Coop DX Wii U

This fork is an experimental Wii U porting effort for [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx).

## Goal

The goal of this project is to explore how far sm64coopdx can be ported to the Wii U,
starting with an offline `.rpx` target before attempting any network features.
This is also a personal learning project to get hands-on experience with C
and homebrew development for consoles.

Sorry for the messy and shitty code I made, oops.

## Current Status
The Wii U build compiles and is playable in Cemu. Lua mod support is now **partially working**, but only through a reduced Wii U-safe path.

The following bundled mods have been tested together without crashing:

- `Cheats`
- `Faster Swimming`
- `Personal Star Counter`
- `Hide and Seek` reduced loader stub

This does not mean full upstream Lua mod compatibility works yet. Large scripts and scripts that touch engine C objects are still unsafe on Wii U.

### What currently works

- Small Lua scripts can load and register no-argument hooks.
- `Cheats` works through Wii U-specific C helpers for the safe toggles.
- `Personal Star Counter` works and renders the star HUD icons through C instead of exposing `gTextures` to Lua.
- `Faster Swimming` works with a conservative speed multiplier.
- `Hide and Seek` currently only verifies that the mod can load. The chat command cannot be meaningfully tested yet because the Wii U GX2 backend does not currently wire keyboard callbacks.

### Cause of the crashes

The main crash cause found so far is the Lua CObject bridge on Wii U. Passing or exposing engine structs such as `MarioState`, `Object`, `NetworkPlayer`, controller data, `gMarioStates`, `gNetworkPlayers`, or `gTextures` to Lua can crash in Cemu/Wii U.

The original mods commonly use hooks such as `HOOK_MARIO_UPDATE`, `HOOK_ON_INTERACT`, or direct globals like `gMarioStates[0]`. Those paths force the engine to push C objects into Lua, and that is not stable in this port yet.

There is also a second issue: `lua_load()` is very layout-sensitive on Wii U. Small binary changes can move the crash point, so not every successful compile produces the same Lua loading behavior.

### Quick workaround

The quick solution that currently works is:

- Keep Wii U Lua mod files very small.
- Avoid Lua hooks that receive engine objects as arguments.
- Avoid `gMarioStates`, `gNetworkPlayers`, `gTextures`, sync CObjects, and similar globals from Lua.
- Use plain Lua only for menu registration and no-argument hooks.
- Move all engine-state access into Wii U-specific C helper functions.

This is why the current bundled mods are reduced compared to the upstream versions.

### Observed Lua size limits

These are practical limits from testing, not a guaranteed technical specification:

- A truncated 4096-byte `cheats.lua` prefix reached `lua_load()` and failed cleanly with a syntax error instead of hard-crashing.
- An 8192-byte prefix crashed during `lua_load()`.
- Real scripts around 1 KB have also crashed depending on contents and binary layout.
- The currently stable bundled Wii U-safe scripts are much smaller: roughly 77 to 648 bytes each.

For now, Wii U-safe Lua mods should be kept under about 700 bytes when possible, and anything that needs engine data should be implemented in C helpers.

### Still to do

- Fix the CObject bridge on Wii U instead of bypassing it.
- Make larger Lua scripts load reliably.
- Add a proper Wii U input path for chat or replace chat-only mod controls with menu controls.
- Restore more original mod functionality once CObject access is safe.

## Credits

- [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx), created and maintained by the Coop Deluxe Team.
- [sm64ex-coop](https://github.com/djoslin0/sm64ex-coop), the earlier project that Coop DX continues and expands on.
- [sm64-port by aboood40091](https://github.com/aboood40091/sm64-port), used as the main reference for Wii U platform files, GX2 rendering, Wii U shaders, and controller support.
- [devkitPro](https://devkitpro.org/) and the Wii U homebrew toolchain.

## Legal Notice

This repository does not include ROMs, extracted game assets, or generated files derived from Nintendo content.
Building this project requires users to provide their own legally obtained US Super Mario 64 ROM.

