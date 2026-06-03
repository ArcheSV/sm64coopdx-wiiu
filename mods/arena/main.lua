-- name: Arena
-- incompatible: gamemode arena
-- pausable: false
GAME_MODE_DM=0
GAME_MODE_TDM=1
GAME_MODE_CTF=2
GAME_MODE_KOTH=3
Arena=Arena or {}
Arena.wiiuDisabled=true
Arena.enabled=false
Arena.mode=GAME_MODE_DM
Arena.players=Arena.players or {}
Arena.round=0
local function player(i)
    Arena.players[i]=Arena.players[i] or {team=0,kills=0,deaths=0,score=0,rank=0}
    return Arena.players[i]
end
function Arena.register_player(i,team)local p=player(i)p.team=team or p.team return p end
function Arena.record_death(v,a)
    local vp=player(v or 0)vp.deaths=vp.deaths+1
    if a and a~=v then local ap=player(a)ap.kills=ap.kills+1 ap.score=ap.score+1 end
    return calculate_rankings()
end
function calculate_rankings()
    local t={} for i,p in pairs(Arena.players)do t[#t+1]={i=i,p=p}end
    table.sort(t,function(a,b)return a.p.score>b.p.score end)
    for r,e in ipairs(t)do e.p.rank=r end
    return t
end
function calculate_team_rank(team)return calculate_team_score(team) end
function calculate_team_score(team)local s=0 for _,p in pairs(Arena.players)do if p.team==team then s=s+p.score end end return s end
function pick_team_on_join(i)local r=calculate_team_score(1)<=calculate_team_score(2)and 1 or 2 return Arena.register_player(i or 0,r)end
function shuffle_teams()local n=1 for _,p in pairs(Arena.players)do p.team=n n=3-n end end
function round_begin()Arena.enabled=true Arena.round=Arena.round+1 return Arena.round end
function round_end()Arena.enabled=false return Arena.round end
function on_arena_player_death(v,a)return Arena.record_death(v,a) end
function end_round_if_team_empty()return false end
function level_check()return true end
function on_sync_valid()end
function on_pause_exit()end
function on_server_update()end
function on_update()if Arena.enabled then calculate_rankings()end end
function on_gamemode_command(msg)Arena.mode=tonumber(msg)or Arena.mode return true end
function on_level_command()return false end
function on_jump_leniency_command()return false end
function get_level_choices()return {} end
function Arena.reset()Arena.players={} Arena.enabled=false end
function Arena.update()return on_update() end
