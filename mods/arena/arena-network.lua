Arena=Arena or {}
Arena.localPackets=Arena.localPackets or {}
local function push(kind,a,b,c)local p={kind=kind,a=a,b=b,c=c}Arena.localPackets[#Arena.localPackets+1]=p return p end
function send_arena_death(victimGlobalId,attackerGlobalId)if Arena.record_death then Arena.record_death(victimGlobalId,attackerGlobalId)end return push("death",victimGlobalId,attackerGlobalId)end
function on_packet_arena_death_receive(d)return d and send_arena_death(d.victimGlobalId,d.attackerGlobalId)end
function on_packet_arena_respawn_receive(d)return push("respawn",d and d.globalId)end
function send_arena_respawn()return push("respawn",0)end
function send_arena_hammer_hit(victimGlobalId,attackerGlobalId)return push("hammer",victimGlobalId,attackerGlobalId)end
function on_packet_arena_hammer_hit_receive(d)return d and push("hammer",d.victimGlobalId,d.attackerGlobalId)end
function send_arena_flag(team,globalIndex,msg)return push("flag",team,globalIndex,msg)end
function on_packet_arena_flag_receive(d)return d and push("flag",d.team,d.globalIndex,d.msg)end
function on_packet_receive(d)return d and push(d.kind or "packet",d.a,d.b,d.c)end
