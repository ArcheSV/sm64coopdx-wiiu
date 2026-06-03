gArenaPlayers=gArenaPlayers or {}
local function ps(i)gArenaPlayers[i]=gArenaPlayers[i]or{item=0,ammo=0,team=0,alive=true}return gArenaPlayers[i]end
function mario_hammer_is_attack()return false end
function mario_hammer_position()return {x=0,y=0,z=0} end
function mario_hammer_pound()return false end
function mario_hammer_on_set_action()return false end
function mario_hammer_update()end
function mario_local_hammer_check()return false end
function mario_fire_flower_use()return false end
function mario_bobomb_use()return false end
function mario_cannon_box_update()end
function allow_pvp_attack()return false end
function on_pvp_attack()return false end
function on_interact()return false end
function on_set_mario_action()end
function mario_local_update()end
function mario_update()end
function player_reset_sync_table(i)local p=ps(i or 0)p.item=0 p.ammo=0 p.alive=true return p end
function player_respawn(i)local p=ps(i or 0)p.alive=true return p end
function on_death(i)local p=ps(i or 0)p.alive=false return p end
function on_player_connected(i)return player_reset_sync_table(i or 0)end
function on_player_disconnected(i)gArenaPlayers[i or 0]=nil end
function before_phys_step()end
