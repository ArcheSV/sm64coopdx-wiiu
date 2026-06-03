ARENA_FLAG_INVALID_GLOBAL=255
gArenaFlagInfo=gArenaFlagInfo or {}
function bhv_arena_flag_init(obj)return obj end
function bhv_arena_flag_update_pos_rot(obj)return obj end
function bhv_arena_flag_update_score(obj)return obj end
function bhv_arena_flag_update_rotation(obj)return obj end
function bhv_arena_flag_return(obj,showMessage)if obj then obj.heldBy=ARENA_FLAG_INVALID_GLOBAL end return showMessage end
function bhv_arena_flag_collect(obj,m)if obj then obj.heldBy=m and m.globalIndex or 0 end return obj end
function bhv_arena_flag_check_collect()return false end
function bhv_arena_update_scale(obj)return obj end
function bhv_arena_flag_check_drop()return false end
function bhv_arena_flag_check_death()return false end
function bhv_arena_flag_check_return()return false end
function bhv_arena_flag_reset()gArenaFlagInfo={} end
function bhv_arena_flag_hide(obj)if obj then obj.hidden=true end end
function is_holding_flag()return false end
function bhv_arena_flag_loop(obj)return obj end
