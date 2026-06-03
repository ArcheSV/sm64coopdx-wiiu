CSHud=CSHud or {values={}}
function _G.hud_get_value(t)return CSHud.values[t] end
function _G.hud_set_value(t,v)CSHud.values[t]=v end
function hud_hide_element(e)CSHud.values[e]=false end
function hud_show_element(e)CSHud.values[e]=true end
function hud_get_element(e)return CSHud.values[e] end
function name_from_local_index(i)return "P"..tostring((i or 0)+1) end
function color_from_local_index()return {r=255,g=255,b=255} end
function life_icon_from_local_index()return nil end
function star_icon_from_local_index()return nil end
function render_life_icon_from_local_index()end
function render_life_icon_from_local_index_interpolated()end
function render_star_icon_from_local_index()end
function render_star_icon_from_local_index_interpolated()end
function health_meter_from_local_index()return nil end
function render_health_meter_from_local_index()end
function render_health_meter_from_local_index_interpolated()end
function zero_index_to_one_index(t)local o={}for k,v in pairs(t or {})do o[k+1]=v end return o end
function render_playerlist_and_modlist()end
function nametags_settings()return CSHud.values end
