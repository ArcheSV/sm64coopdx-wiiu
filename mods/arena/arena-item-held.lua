gItemHeld=gItemHeld or {}
ATTACH_NONE=0
ATTACH_HAND=1
ATTACH_HEAD=2
function bhv_arena_item_held_init(obj)return obj end
function bhv_arena_item_held_loop(obj)return obj end
function on_level_init()gItemHeld={} end
function dot_along_angle()return 0 end
function bhv_arena_item_held_hammer_render(obj)return obj end
function on_object_render(obj)return obj end
