-- name: Faster Swimming

local WIIU_SUPER_SWIMMING = 3

local function wiiu_faster_swimming_boost()
    wiiu_set_cheat(WIIU_SUPER_SWIMMING, true)
    wiiu_faster_swimming_tick()
    wiiu_cheats_tick()
end

hook_event(HOOK_UPDATE, wiiu_faster_swimming_boost)
