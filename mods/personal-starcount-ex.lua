-- name: Personal Star Counter
-- incompatible: gamemode
-- description: Wii U-safe local star counter.

local total = tonumber(mod_storage_load("StarCounter")) or 0
local run = 0
local prev = hud_get_value(HUD_DISPLAY_STARS)

local function tick()
    local stars = hud_get_value(HUD_DISPLAY_STARS)
    if stars > prev then
        local delta = stars - prev
        run = run + delta
        total = total + delta
        mod_storage_save("StarCounter", tostring(total))
    end
    prev = stars
end

local function hud()
    wiiu_personal_star_counter_hud(prev, run, total)
end

hook_event(HOOK_UPDATE, tick)
hook_event(HOOK_ON_HUD_RENDER, hud)
