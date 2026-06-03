sArenaSpawnPoints=sArenaSpawnPoints or {}
function bhv_arena_spawn_init(obj)sArenaSpawnPoints[#sArenaSpawnPoints+1]=obj or {x=0,y=0,z=0}return obj end
function find_spawn_point()return #sArenaSpawnPoints>0 and sArenaSpawnPoints[1] or nil end
function on_level_init()sArenaSpawnPoints={} end
