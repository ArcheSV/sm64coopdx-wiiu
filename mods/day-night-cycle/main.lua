-- name: Day Night Cycle DX
-- description: Wii U-safe internal clock lighting cycle.

local H = 1800
local DAY = H * 24
local time = H * 4

local function mix(a, b, t)
    return math.floor(a + (b - a) * t + 0.5)
end

local function set3(f, r, g, b)
    f(0, r); f(1, g); f(2, b)
end

local function apply(r, g, b, ar, ag, ab, dir)
    set_lighting_dir(0, 0); set_lighting_dir(1, dir); set_lighting_dir(2, dir)
    set3(set_lighting_color, r, g, b)
    set3(set_lighting_color_ambient, ar, ag, ab)
    set3(set_vertex_color, mix(r, ar, 0.5), mix(g, ag, 0.5), mix(b, ab, 0.5))
    set3(set_skybox_color, r, g, b)
    set3(set_fog_color, mix(r, ar, 0.5), mix(g, ag, 0.5), mix(b, ab, 0.5))
    set_fog_intensity(1)
end

local function blend(t, r1, g1, b1, ar1, ag1, ab1, d1, r2, g2, b2, ar2, ag2, ab2, d2)
    apply(
        mix(r1, r2, t), mix(g1, g2, t), mix(b1, b2, t),
        mix(ar1, ar2, t), mix(ag1, ag2, t), mix(ab1, ab2, t),
        d1 + (d2 - d1) * t
    )
end

local function update()
    time = (time + 1) % DAY
    local h = time / H
    if h < 4 or h >= 21 then
        apply(90, 100, 150, 70, 90, 150, -0.4)
    elseif h < 5 then
        blend(h - 4, 90, 100, 150, 70, 90, 150, -0.4, 255, 250, 100, 200, 200, 255, 0)
    elseif h < 6 then
        blend(h - 5, 255, 250, 100, 200, 200, 255, 0, 255, 255, 255, 255, 255, 255, 0)
    elseif h < 19 then
        apply(255, 255, 255, 255, 255, 255, 0)
    elseif h < 20 then
        blend(h - 19, 255, 255, 255, 255, 255, 255, 0, 255, 140, 80, 255, 140, 160, -0.4)
    else
        blend(h - 20, 255, 140, 80, 255, 140, 160, -0.4, 90, 100, 150, 70, 90, 150, -0.4)
    end
end

hook_event(HOOK_UPDATE, update)
