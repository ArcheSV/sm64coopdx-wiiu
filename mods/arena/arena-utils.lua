function active_player(m)return m~=nil end
function clamp(v,a,b)if v<a then return a elseif v>b then return b end return v end
function convert_s16(n)n=n%65536 if n>=32768 then n=n-65536 end return n end
function strip_colors(s)return tostring(s or ""):gsub("\\#%x%x%x%x%x%x\\","") end
function get_other_team(t)return t==1 and 2 or 1 end
function team_name_str(t)return t==1 and "Red" or t==2 and "Blue" or "None" end
function team_color_str(t)return t==1 and "\\#ff7878\\" or t==2 and "\\#7878ff\\" or "\\#dcdcdc\\" end
function set_dist_and_angle(from,dist,pitch,yaw)return {x=0,y=0,z=0,dist=dist,pitch=pitch,yaw=yaw,from=from} end
function mario_health_float(m)return m and m.health and m.health/2048 or 1 end
function global_index_hurts_mario_state()return false end
function is_invuln_or_intang()return false end
function spawn_mist()end
function spawn_mist_advanced()end
function spawn_balls()end
function spawn_triangles()end
function spawn_horizontal_stars()end
function spawn_vertical_stars()end
function spawn_sparkles()end
function debug_pos(x,y,z)return {x=x,y=y,z=z} end
function SEQUENCE_ARGS(p,s)return p*65536+s end
