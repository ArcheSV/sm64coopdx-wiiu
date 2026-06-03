sArenaKothPoints=sArenaKothPoints or {}
gArenaKothActiveObj=nil
function bhv_arena_koth_init(obj)sArenaKothPoints[#sArenaKothPoints+1]=obj or {x=0,y=0,z=0}return obj end
function find_koth_point()return #sArenaKothPoints>0 and 1 or -1 end
function bhv_arena_koth_active_init(obj)gArenaKothActiveObj=obj return obj end
function bhv_arena_koth_active_loop(obj)return obj end
function on_level_init()sArenaKothPoints={} end
