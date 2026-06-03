gItemData=gItemData or {
    [ITEM_METAL_CAP or 1]={name="Metal Cap",timeout=450},
    [ITEM_HAMMER or 2]={name="Hammer",timeout=450},
    [ITEM_FIRE_FLOWER or 3]={name="Fire Flower",timeout=450},
    [ITEM_CANNON_BOX or 4]={name="Cannon Box",timeout=450},
    [ITEM_BOBOMB or 5]={name="Bob-omb",timeout=450},
    [ITEM_COIN or 6]={name="Coin",timeout=120},
}
function bhv_arena_item_init(obj)return obj end
function bhv_arena_item_collect(obj)return obj end
function bhv_arena_item_collect_metal_cap(obj)return obj end
function bhv_arena_item_collect_coin(obj)return obj end
function bhv_arena_item_update_touch()return false end
function bhv_arena_item_update_model(obj)return obj end
function bhv_arena_item_update_rotation(obj)return obj end
function bhv_arena_item_check_collect()return false end
function bhv_arena_item_loop(obj)return obj end
