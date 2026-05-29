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
The Wii U build compiles and is playable in Cemu, but **Lua mod support is not yet stable**.

### Known issue

The game crashes during Lua mod initialization, usually when loading or executing the first enabled script (`cheats.lua`). The flow correctly reaches the Lua startup phase, opens the mod file without errors, and then crashes around `lua_load()` / the first `pcall()` execution path.

### Investigated and ruled out

Several hypotheses have been tested without success:

- Replaced `luaL_loadbufferx()` with streamed `lua_load()`.
- Avoided the custom per-mod `_ENV` path on Wii U.
- Changed bulk Lua constants binding to lazy binding.
- Rebuilt the Wii U Lua 5.3.5 static library with `LUA_32BITS`.
- Tested a dedicated Lua thread with a larger stack.
- Tested a fixed-size Lua arena allocator to rule out obvious OOM issues.
- Tried simplifying sync/global table setup.
- Tried Lua line tracing, but discarded because the extra instrumentation shifts the crash point and yields unreliable results.

### What cause the crash?

The most likely hypothesis is an instability in the Wii U/Cemu Lua VM integration during mod script loading or first execution, possibly related to ABI, memory layout, compiler, or runtime behavior differences, so I don't think that is a specific line in the Lua script.

## Credits

- [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx), created and maintained by the Coop Deluxe Team.
- [sm64ex-coop](https://github.com/djoslin0/sm64ex-coop), the earlier project that Coop DX continues and expands on.
- [sm64-port by aboood40091](https://github.com/aboood40091/sm64-port), used as the main reference for Wii U platform files, GX2 rendering, Wii U shaders, and controller support.
- [devkitPro](https://devkitpro.org/) and the Wii U homebrew toolchain.

## Legal Notice

This repository does not include ROMs, extracted game assets, or generated files derived from Nintendo content.
Building this project requires users to provide their own legally obtained US Super Mario 64 ROM.


