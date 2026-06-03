![sm64coopdx Logo](textures/segment2/custom_coopdx_logo.rgba32.png)

# SM64 Coop DX Wii U

This fork is an experimental Wii U porting effort for [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx).

### Mods and hackroms

The game is fully playable on Wii U/Cemu, but Lua mods must be adapted for the Wii U version. The main limitation found so far is that large Lua files can crash during loading; in practice, mods should stay well below ~700 bytes per Lua file when possible, unless they have been tested specifically. Mods that use CObjects, engine structs, large scripts, or object-based hooks need Wii U-specific rewrites.

### Still to do

- Fix the CObject bridge on Wii U
- Make larger Lua scripts load reliably.
- Add a proper Wii U input path for chat or replace chat-only mod controls with menu controls.
- Adapt the remaining bundled mods one by one, starting with the smallest feature surface that can avoid CObjects.
- Restore more original mod functionality once CObject access is safe.
- Re-test Star Road with the current reduced-root setup and then decide whether the model placeholder workaround is enough or whether more `.lvl` object models need to be patched.

- For more information about pending and working mods, click [here](https://docs.google.com/spreadsheets/d/1ixHRN86vI2OHn1eHg6zEc2uuKkWrHM2VvalmhE62fiQ/edit?usp=sharing)
## Credits

- [Super Mario 64 Coop DX](https://github.com/coop-deluxe/sm64coopdx), created and maintained by the Coop Deluxe Team.
- [sm64ex-coop](https://github.com/djoslin0/sm64ex-coop), the earlier project that Coop DX continues and expands on.
- [sm64-port by aboood40091](https://github.com/aboood40091/sm64-port), used as the main reference for Wii U platform files, GX2 rendering, Wii U shaders, and controller support.
- [devkitPro](https://devkitpro.org/) and the Wii U homebrew toolchain.

## Legal Notice

This repository does not include ROMs, extracted game assets, or generated files derived from Nintendo content.
Building this project requires users to provide their own legally obtained US Super Mario 64 ROM.
