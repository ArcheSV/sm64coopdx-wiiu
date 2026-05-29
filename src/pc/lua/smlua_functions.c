#include "smlua.h"
#include "smlua_cobject.h"

#include <PR/gbi.h>

#include "game/level_update.h"
#include "game/area.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/mario_actions_stationary.h"
#include "audio/external.h"
#include "object_fields.h"
#include "level_commands.h"
#include "engine/math_util.h"
#include "engine/level_script.h"
#include "pc/djui/djui_hud_utils.h"
#include "pc/utils/misc.h"
#include "pc/platform.h"
#include "pc/network/network_player.h"
#include "include/level_misc_macros.h"
#include "include/macro_presets.h"
#include "include/sounds.h"
#include "utils/smlua_anim_utils.h"
#include "utils/smlua_collision_utils.h"
#include "game/hardcoded.h"
#include "include/macros.h"

#include <math.h>

bool smlua_functions_valid_param_count(lua_State* L, int expected) {
    int top = lua_gettop(L);
    if (top != expected) {
        LOG_LUA_LINE("Improper param count: Expected %u, Received %u", expected, top);
        return false;
    }
    return true;
}

bool smlua_functions_valid_param_range(lua_State* L, int min, int max) {
    int top = lua_gettop(L);
    if (top < min || top > max) {
        LOG_LUA_LINE("Improper param count: Expected (%u - %u), Received %u", min, max, top);
        return false;
    }
    return true;
}

  ///////////
 // table //
///////////

int smlua_func_table_copy(lua_State *L) {
    LUA_STACK_CHECK_BEGIN_NUM(L, 1);

    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    if (lua_type(L, 1) != LUA_TTABLE) {
        LOG_LUA_LINE("table_copy() called with an invalid type for param 1: %s", luaL_typename(L, 1));
        return 0;
    }

    // Create a new table that will be the copy
    lua_newtable(L);

    // Iterate through original table
    lua_pushnil(L); // first key
    while (lua_next(L, 1) != 0) {

        // Stack at the start of iteration is orig_table, new_table, key, value
        // At the end of iteration, we need the key on top of the stack
        // But settable also needs the key, so we manipulate the stack to become:
        // orig_table, new_table, key, key, value   (before settable)
        // orig_table, new_table, key               (after settable)
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, 2);
    }

    LUA_STACK_CHECK_END(L);
    return 1;
}

static void table_deepcopy_table(lua_State *L, int idxTable, int idxCache);

static void table_deepcopy_value(lua_State *L, int idx, int idxCache) {
    idx = lua_absindex(L, idx);
    if (lua_type(L, idx) == LUA_TTABLE) {
        table_deepcopy_table(L, idx, idxCache);
    } else {
        lua_pushvalue(L, idx);
    }
}

static void table_deepcopy_table(lua_State *L, int idxTable, int idxCache) {
    idxTable = lua_absindex(L, idxTable);
    idxCache = lua_absindex(L, idxCache);

    // Check the cache to see if the table has already been copied
    lua_pushvalue(L, idxTable);
    lua_rawget(L, idxCache);
    if (!lua_isnil(L, -1)) {
        return;
    }
    lua_pop(L, 1);

    // Create a new table that will be the copy and add it to the cache
    lua_newtable(L);
    int idxNewTable = lua_gettop(L);
    lua_pushvalue(L, idxTable);
    lua_pushvalue(L, idxNewTable);
    lua_rawset(L, idxCache);

    // Iterate through original table
    lua_pushnil(L); // first key
    while (lua_next(L, idxTable) != 0) {
        int idxKey = lua_absindex(L, -2);
        int idxValue = lua_absindex(L, -1);

        // Copy key and value to new table
        table_deepcopy_value(L, idxKey, idxCache);
        table_deepcopy_value(L, idxValue, idxCache);
        lua_settable(L, idxNewTable);

        // Pop value to set key on top of the stack
        lua_pop(L, 1);
    }

    // Copy metatable
    if (lua_getmetatable(L, idxTable)) {
        table_deepcopy_value(L, -1, idxCache);
        lua_setmetatable(L, idxNewTable);
        lua_pop(L, 1);
    }
}

int smlua_func_table_deepcopy(lua_State *L) {
    LUA_STACK_CHECK_BEGIN_NUM(L, 1);

    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    if (lua_type(L, 1) != LUA_TTABLE) {
        LOG_LUA_LINE("table_deepcopy() called with an invalid type for param 1: %s", luaL_typename(L, 1));
        return 0;
    }

    // Cache to prevent copying the same table twice
    lua_newtable(L);
    int idxCache = lua_gettop(L);

    table_deepcopy_table(L, 1, idxCache);

    lua_remove(L, idxCache);

    LUA_STACK_CHECK_END(L);
    return 1;
}

static int smlua_func_create_read_only_table_newindex(lua_State *L) {
    luaL_tolstring(L, 2, NULL);
    const char *key = lua_tostring(L, -1);
    return luaL_error(L, "Attempting to modify key `%s` of read-only table", key ? key : "?");
}

static int smlua_func_create_read_only_table_call(lua_State *L) {
    lua_settop(L, 0);
    lua_pushvalue(L, lua_upvalueindex(1));
    return smlua_func_table_copy(L);
}

int smlua_func_create_read_only_table(lua_State *L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

#if defined(TARGET_WII_U)
    lua_newtable(L);
    int proxyIndex = lua_gettop(L);

    lua_pushvalue(L, 1);
    lua_setfield(L, proxyIndex, "_table");

    lua_getglobal(L, "_ReadOnlyTable");
    if (lua_type(L, -1) == LUA_TTABLE) {
        lua_setmetatable(L, proxyIndex);
    } else {
        lua_pop(L, 1);
    }
    return 1;
#else
    lua_createtable(L, 0, count);
    int proxyIndex = lua_gettop(L);

    lua_createtable(L, 0, 16);
    int metatableIndex = lua_gettop(L);

    lua_pushvalue(L, 1);
    lua_setfield(L, metatableIndex, "__index");

    lua_pushcfunction(L, smlua_func_create_read_only_table_newindex);
    lua_setfield(L, metatableIndex, "__newindex");

    lua_pushvalue(L, 1);
    lua_pushcclosure(L, smlua_func_create_read_only_table_call, 1);
    lua_setfield(L, metatableIndex, "__call");

    lua_pushboolean(L, false);
    lua_setfield(L, metatableIndex, "__metatable");

    lua_setmetatable(L, proxyIndex);
    return 1;
#endif
}

  //////////
 // misc //
//////////

int smlua_func_init_mario_after_warp(lua_State* L) {
    if (network_player_connected_count() >= 2) {
        LOG_LUA_LINE("init_mario_after_warp() can only be used in singleplayer");
        return 0;
    }

    if(!smlua_functions_valid_param_count(L, 0)) { return 0; }

    extern void init_mario_after_warp(void);
    init_mario_after_warp();

    return 1;
}

int smlua_func_reset_level(lua_State* L) {
    if (network_player_connected_count() >= 2) {
        LOG_LUA_LINE("reset_level() can only be used in singleplayer");
        return 0;
    }

    if(!smlua_functions_valid_param_count(L, 0)) { return 0; }

    gChangeLevel = gCurrLevelNum;

    return 1;
}

int smlua_func_network_init_object(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 3)) { return 0; }

    struct Object* obj = smlua_to_cobject(L, 1, LOT_OBJECT);
    if (!gSmLuaConvertSuccess || obj == NULL) { LOG_LUA("network_init_object: Failed to convert parameter 1"); return 0; }

    bool standardSync = smlua_to_boolean(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("network_init_object: Failed to convert parameter 2"); return 0; }

    if (lua_type(L, 3) != LUA_TNIL && lua_type(L, 3) != LUA_TTABLE) {
        LOG_LUA_LINE("network_init_object() called with an invalid type for param 3: %s", luaL_typename(L, 3));
        return 0;
    }

    struct SyncObject* so = sync_object_init(obj, standardSync ? 4000.0f : SYNC_DISTANCE_ONLY_EVENTS);
    if (so == NULL) {
        LOG_LUA_LINE("network_init_object: Failed to allocate sync object.");
        return 0;
    }

    if (lua_type(L, 3) == LUA_TTABLE) {
        lua_pushnil(L);  // first key

        while (lua_next(L, 3) != 0) {
            // uses 'key' (at index -2) and 'value' (at index -1)
            if (lua_type(L, -1) != LUA_TSTRING) {
                LOG_LUA_LINE("Invalid type passed to network_init_object(): %s", luaL_typename(L, -1));
                lua_pop(L, 1); // pop value
                continue;
            }
            const char* fieldIdentifier = smlua_to_string(L, -1);
            if (!gSmLuaConvertSuccess) {
                LOG_LUA_LINE("Invalid field passed to network_init_object()");
                lua_pop(L, 1); // pop value
                continue;
            }

            struct LuaObjectField* data = smlua_get_object_field(LOT_OBJECT, fieldIdentifier);
            if (data == NULL) {
                data = smlua_get_custom_field(L, LOT_OBJECT, lua_gettop(L));
            }

            u8 lvtSize = 0;
            if ((data->valueType == LVT_U32) || (data->valueType == LVT_S32) || (data->valueType == LVT_F32)) { lvtSize = 32; }
            if ((data->valueType == LVT_U16) || (data->valueType == LVT_S16)) { lvtSize = 16; }
            if ((data->valueType == LVT_U8) || (data->valueType == LVT_S8)) { lvtSize = 8; }

            if (data == NULL || lvtSize == 0) {
                LOG_LUA_LINE("Invalid field passed to network_init_object(): %s", fieldIdentifier);
                lua_pop(L, 1); // pop value
                continue;
            }

            u8* field = ((u8*)(intptr_t)obj) + data->valueOffset;
            sync_object_init_field_with_size(obj, field, lvtSize);

            lua_pop(L, 1); // pop value
        }
        lua_pop(L, 1); // pop key
    }

    return 1;
}

int smlua_func_network_send_object(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }

    struct Object* obj = smlua_to_cobject(L, 1, LOT_OBJECT);
    if (!gSmLuaConvertSuccess || obj == NULL) { LOG_LUA("network_send_object: Failed to convert parameter 1"); return 0; }

    bool reliable = smlua_to_boolean(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("network_send_object: Failed to convert parameter 2"); return 0; }

    struct SyncObject* so = sync_object_get(obj->oSyncID);
    if (!so || so->o != obj) {
        LOG_LUA_LINE("network_send_object: Failed to retrieve sync object.");
        return 0;
    }

    network_send_object_reliability(obj, reliable);

    return 1;
}

int smlua_func_network_send(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }
    network_send_lua_custom(true);
    return 1;
}

int smlua_func_network_send_to(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 3)) { return 0; }
    network_send_lua_custom(false);
    return 1;
}

int smlua_func_network_send_bytestring(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }
    network_send_lua_custom_bytestring(true);
    return 1;
}

int smlua_func_network_send_bytestring_to(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 3)) { return 0; }
    network_send_lua_custom_bytestring(false);
    return 1;
}

int smlua_func_set_exclamation_box_contents(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    if (lua_type(L, 1) != LUA_TTABLE) {
        LOG_LUA_LINE("Invalid type passed to set_exclamation_box(): %s", luaL_typename(L, -1));
        return 0;
    }

    struct ExclamationBoxContent exclamationBoxNewContents[EXCLAMATION_BOX_MAX_SIZE];

    u8 exclamationBoxIndex = 0;
    lua_pushnil(L); // Initial pop
    while (lua_next(L, 1)) /* Main table index */ {
        if (lua_type(L, 3) != LUA_TTABLE) {
            LOG_LUA_LINE("set_exclamation_box: Subtable is not a table (Subtable %u)", exclamationBoxIndex);
            return 0;
        }

        lua_pushnil(L); // Subtable initial pop
        bool confirm[] = { false, false, false, false, false }; /* id, unused, firstByte, model, behavior */
        while (lua_next(L, 3)) /* Subtable index */ {
            // key is index -2, value is index -1
            const char* key = smlua_to_string(L, -2);
            if (!gSmLuaConvertSuccess) {
                LOG_LUA("set_exclamation_box: Failed to convert subtable key");
                return 0;
            }

            s32 value = smlua_to_integer(L, -1);
            if (!gSmLuaConvertSuccess) {
                LOG_LUA("set_exclamation_box: Failed to convert subtable value");
                return 0;
            }

            // Fill fields
            if (strcmp(key, "id") == 0) { exclamationBoxNewContents[exclamationBoxIndex].id = value; confirm[0] = true; }
            else if (strcmp(key, "unused") == 0) { exclamationBoxNewContents[exclamationBoxIndex].unused = value; confirm[1] = true; }
            else if (strcmp(key, "firstByte") == 0) { exclamationBoxNewContents[exclamationBoxIndex].firstByte = value; confirm[2] = true; }
            else if (strcmp(key, "model") == 0) { exclamationBoxNewContents[exclamationBoxIndex].model = value; confirm[3] = true; }
            else if (strcmp(key, "behavior") == 0) { exclamationBoxNewContents[exclamationBoxIndex].behavior = value; confirm[4] = true; }
            else {
                LOG_LUA_LINE_WARNING("set_exclamation_box: Invalid key passed (Subtable %d)", exclamationBoxIndex);
            }

            lua_pop(L, 1); // Pop value
        }
        // Check if the fields have been filled
        if (!(confirm[0]) || !(confirm[3]) || !(confirm[4])) {
            LOG_LUA("set_exclamation_box: A critical component of a content (id, model, or behavior) has not been set (Subtable %d)", exclamationBoxIndex);
            return 0;
        }
        if (!(confirm[1])) { exclamationBoxNewContents[exclamationBoxIndex].unused = 0; }
        if (!(confirm[2])) { exclamationBoxNewContents[exclamationBoxIndex].firstByte = 0; }

        if (++exclamationBoxIndex == EXCLAMATION_BOX_MAX_SIZE) { // There is an edge case where the 254th element will warn even though it works just fine
            // Immediately exit if at risk for out of bounds array access.
            lua_pop(L, 1);
            LOG_LUA_LINE_WARNING("set_exclamation_box: Too many items have been set for the exclamation box. Some content spawns may be lost.");
            break;
        }
        lua_pop(L, 1); // Pop subtable
    }

    memcpy(gExclamationBoxContents, exclamationBoxNewContents, sizeof(struct ExclamationBoxContent) * exclamationBoxIndex);
    gExclamationBoxSize = exclamationBoxIndex;

    return 1;
}

int smlua_func_get_exclamation_box_contents(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 0)) { return 0; }

    lua_newtable(L); // Index 1

    for (u8 i = 0; i < gExclamationBoxSize; i++) {
        lua_pushinteger(L, i); // Index 2
        lua_newtable(L); // Index 3

        lua_pushstring(L, "id");
        lua_pushinteger(L, gExclamationBoxContents[i].id);
        lua_settable(L, -3);

        lua_pushstring(L, "unused");
        lua_pushinteger(L, gExclamationBoxContents[i].unused);
        lua_settable(L, -3);

        lua_pushstring(L, "firstByte");
        lua_pushinteger(L, gExclamationBoxContents[i].firstByte);
        lua_settable(L, -3);

        lua_pushstring(L, "model");
        lua_pushinteger(L, gExclamationBoxContents[i].model);
        lua_settable(L, -3);

        lua_pushstring(L, "behavior");
        lua_pushinteger(L, gExclamationBoxContents[i].behavior);
        lua_settable(L, -3);

        lua_settable(L, 1); // Insert the subtable into the main table
    }

    return 1;
}

  //////////////
 // Textures //
//////////////

int smlua_func_get_texture_info(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    if (lua_type(L, -1) != LUA_TSTRING) {
        LOG_LUA_LINE("Invalid type passed to get_texture_info(): %s", luaL_typename(L, -1));
        lua_pop(L, 1); // pop value
        return 0;
    }

    struct TextureInfo texInfo = { 0 };
    const char* textureName = smlua_to_string(L, -1);
    sys_trace("get_texture_info: begin %s", textureName);
    if (!dynos_texture_get(textureName, &texInfo)) {
        LOG_LUA_LINE("Could not find texture info for '%s'", textureName);
        return 0;
    }
    sys_trace("get_texture_info: end %s tex=%p", textureName, texInfo.texture);

    lua_newtable(L);

    lua_pushstring(L, "texture");
    smlua_push_pointer(L, LVT_TEXTURE_P, (void *) texInfo.texture, NULL);
    lua_settable(L, -3);

    lua_pushstring(L, "width");
    lua_pushinteger(L, texInfo.width);
    lua_settable(L, -3);

    lua_pushstring(L, "height");
    lua_pushinteger(L, texInfo.height);
    lua_settable(L, -3);

    lua_pushstring(L, "format");
    lua_pushinteger(L, texInfo.format);
    lua_settable(L, -3);

    lua_pushstring(L, "size");
    lua_pushinteger(L, texInfo.size);
    lua_settable(L, -3);

    lua_pushstring(L, "name");
    lua_pushstring(L, texInfo.name);
    lua_settable(L, -3);

    return 1;
}

int smlua_func_texture_override_set(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }

    const char* textureName = smlua_to_string(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("texture_override_set: Failed to convert parameter 1"); return 0; }

    struct TextureInfo *overrideTexInfo = smlua_to_texture_info(L, 2);
    if (!overrideTexInfo || !gSmLuaConvertSuccess) { LOG_LUA("texture_override_set: Failed to convert parameter 2"); return 0; }

    dynos_texture_override_set(textureName, overrideTexInfo);

    return 1;
}

int smlua_func_texture_override_reset(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    const char* textureName = smlua_to_string(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("texture_override_reset: Failed to convert parameter 1"); return 0; }

    dynos_texture_override_reset(textureName);

    return 1;
}

  ////////////////////////////////
 // level script preprocessing //
////////////////////////////////

struct LuaLevelScriptParse {
    int reference;
    struct Mod* mod;
    struct ModFile* modFile;
};

struct LuaLevelScriptParse sLevelScriptParse = { 0 };

static bool smlua_func_level_find_lua_param(u64 *param, void *cmd, u32 offset, u32 luaParams, u32 luaParamFlag) {
    *param = dynos_level_cmd_get(cmd, offset);
    if (luaParams & luaParamFlag) {
        const char *paramStr = dynos_level_get_token(*param);
        gSmLuaConvertSuccess = true;
        *param = smlua_get_integer_mod_variable(gLevelScriptModIndex, paramStr);
        if (!gSmLuaConvertSuccess) {
            gSmLuaConvertSuccess = true;
            *param = smlua_get_any_integer_mod_variable(paramStr);
        }
        if (!gSmLuaConvertSuccess) {
            return false;
        }
    }
    return true;
}

#define smlua_func_level_get_lua_param(name, ptype, flag) \
    u64 name##Param; \
    if (!smlua_func_level_find_lua_param(&name##Param, cmd, flag##_OFFSET(type), luaParams, flag)) { \
        break; \
    } \
    ptype name = (ptype) name##Param;

s32 smlua_func_level_script_parse_callback(u8 type, void *cmd) {
    u32 areaIndex, bhvId, bhvArgs, bhvModelId;
    s16 bhvPosX, bhvPosY, bhvPosZ;
    s32 bhvPitch, bhvYaw, bhvRoll;
    bool area = false, bhv = false;
    MacroObject *macroData = NULL;

    // Gather arguments
    switch (type) {

        // AREA
        case 0x1F: {
            areaIndex = dynos_level_cmd_get(cmd, 2);
            area = true;
        } break;

        // OBJECT, OBJECT_WITH_ACTS
        case 0x24: {
            const BehaviorScript *bhvPtr = (const BehaviorScript *) dynos_level_cmd_get_ptr(cmd, 20);
            if (bhvPtr) {
                bhvId = get_id_from_behavior(bhvPtr);
                if (bhvId == id_bhv_max_count) {
                    bhvId = get_id_from_vanilla_behavior(bhvPtr); // for behaviors with no id in the script (e.g. bhvInstantActiveWarp)
                }
                bhvArgs = dynos_level_cmd_get(cmd, 16);
                bhvModelId = dynos_level_cmd_get(cmd, 3);
                bhvPosX = dynos_level_cmd_get(cmd, 4);
                bhvPosY = dynos_level_cmd_get(cmd, 6);
                bhvPosZ = dynos_level_cmd_get(cmd, 8);
                bhvPitch = (dynos_level_cmd_get(cmd, 10) * 0x8000) / 180;
                bhvYaw   = (dynos_level_cmd_get(cmd, 12) * 0x8000) / 180;
                bhvRoll  = (dynos_level_cmd_get(cmd, 14) * 0x8000) / 180;
                bhv = true;
            }
        } break;

        // OBJECT_EXT, OBJECT_WITH_ACTS_EXT
        // OBJECT_EXT2, OBJECT_WITH_ACTS_EXT2
        // OBJECT_EXT_LUA_PARAMS
        case 0x3F:
        case 0x40:
        case 0x43: {
            if (gLevelScriptModIndex != -1) {
                u16 luaParams = (
                    type == 0x3F ? OBJECT_EXT_LUA_BEHAVIOR : (
                    type == 0x40 ? OBJECT_EXT_LUA_BEHAVIOR | OBJECT_EXT_LUA_MODEL : (
                    dynos_level_cmd_get(cmd, 2)
                )));

                smlua_func_level_get_lua_param(modelId, u32, OBJECT_EXT_LUA_MODEL);
                smlua_func_level_get_lua_param(posX, s16, OBJECT_EXT_LUA_POS_X);
                smlua_func_level_get_lua_param(posY, s16, OBJECT_EXT_LUA_POS_Y);
                smlua_func_level_get_lua_param(posZ, s16, OBJECT_EXT_LUA_POS_Z);
                smlua_func_level_get_lua_param(angleX, s16, OBJECT_EXT_LUA_ANGLE_X);
                smlua_func_level_get_lua_param(angleY, s16, OBJECT_EXT_LUA_ANGLE_Y);
                smlua_func_level_get_lua_param(angleZ, s16, OBJECT_EXT_LUA_ANGLE_Z);
                smlua_func_level_get_lua_param(behParam, u32, OBJECT_EXT_LUA_BEH_PARAMS);
                smlua_func_level_get_lua_param(behavior, uintptr_t, OBJECT_EXT_LUA_BEHAVIOR);

                bhvArgs = behParam;
                bhvModelId = modelId;
                bhvPosX = posX;
                bhvPosY = posY;
                bhvPosZ = posZ;
                bhvPitch = angleX;
                bhvYaw = angleY;
                bhvRoll = angleZ;
                if (luaParams & OBJECT_EXT_LUA_BEHAVIOR) {
                    bhvId = (u32) behavior;
                } else {
                    bhvId = get_id_from_behavior((const BehaviorScript *) behavior);
                    if (bhvId == id_bhv_max_count) {
                        bhvId = get_id_from_vanilla_behavior((const BehaviorScript *) behavior);
                    }
                }

                bhv = true;
            }
        } break;

        // MACRO_OBJECTS
        case 0x39: {
            macroData = (MacroObject *) dynos_level_cmd_get_ptr(cmd, 4);
        } break;

        // None of the above
        default: return 0;
    }

    // Retrieve Lua state
    lua_State* L = gLuaState;
    if (L == NULL) { return 0; }
    struct LuaLevelScriptParse* preprocess = &sLevelScriptParse;
    lua_rawgeti(L, LUA_REGISTRYINDEX, preprocess->reference);

    // Push 'areaIndex'
    if (area) {
        lua_pushinteger(L, areaIndex);
    } else {
        lua_pushnil(L);
    }

    // Push 'bhvData'
    if (bhv) {
        lua_newtable(L);
        smlua_push_integer_field(-2, "behavior", bhvId);
        smlua_push_integer_field(-2, "behaviorArg", bhvArgs);
        smlua_push_integer_field(-2, "model", bhvModelId);
        smlua_push_integer_field(-2, "posX", bhvPosX);
        smlua_push_integer_field(-2, "posY", bhvPosY);
        smlua_push_integer_field(-2, "posZ", bhvPosZ);
        smlua_push_integer_field(-2, "pitch", bhvPitch);
        smlua_push_integer_field(-2, "yaw", bhvYaw);
        smlua_push_integer_field(-2, "roll", bhvRoll);
    } else {
        lua_pushnil(L);
    }

    // Push 'macroBhvIds' and 'macroBhvArgs' and 'macroBhvModels'
    if (macroData) {
        lua_newtable(L);
        s32 macroBhvIdsIdx = lua_gettop(L);
        lua_newtable(L);
        s32 macroBhvArgsIdx = lua_gettop(L);
        lua_newtable(L);
        s32 macroBhvModelsIdx = lua_gettop(L);
        for (s32 i = 0; *macroData != MACRO_OBJECT_END(); macroData += 5, i++) {
            s32 presetId = (s32) ((macroData[0] & 0x1FF) - 0x1F);
            s32 presetParams = MacroObjectPresets[presetId].param;
            s32 objParams = (macroData[4] & 0xFF00) | (presetParams & 0x00FF);
            s32 bhvParams = ((objParams & 0x00FF) << 16) | (objParams & 0xFF00);
            lua_pushinteger(L, i);
            lua_pushinteger(L, get_id_from_behavior(MacroObjectPresets[presetId].behavior));
            lua_settable(L, macroBhvIdsIdx);
            lua_pushinteger(L, i);
            lua_pushinteger(L, bhvParams);
            lua_settable(L, macroBhvArgsIdx);
            lua_pushinteger(L, i);
            lua_pushinteger(L, MacroObjectPresets[presetId].model);
            lua_settable(L, macroBhvModelsIdx);
        }
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushnil(L);
    }

    // call the callback
    if (0 != smlua_call_hook(L, 5, 0, 0, preprocess->mod, preprocess->modFile)) {
        LOG_LUA("Failed to call the callback behaviors: %u", type);
        return 0;
    }
    return 0;
}

int smlua_func_level_script_parse(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }

    lua_Integer levelNum = smlua_to_integer(L, 1);
    if (!gSmLuaConvertSuccess) {
        LOG_LUA_LINE("Invalid level script name");
        return 0;
    }

    struct LuaLevelScriptParse* preprocess = &sLevelScriptParse;
    preprocess->reference = LUA_NOREF;

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    if (ref == -1) {
        LOG_LUA_LINE("Level Script Parse: %lld tried to parse using undefined function", levelNum);
        return 0;
    }

    preprocess->reference = ref;
    preprocess->mod = gLuaActiveMod;
    preprocess->modFile = gLuaActiveModFile;

    void *script = dynos_level_get_script(levelNum);
    if (script == NULL) {
        LOG_LUA("Failed to find script: %lld", levelNum);
        return 0;
    }
    s32 modIndex = dynos_level_get_mod_index(levelNum);

    // Back up current values
    LevelScript *currLevelScript = gLevelScriptActive;
    s32 currModIndex = gLevelScriptModIndex;

    // Parse script
    gLevelScriptActive = (LevelScript *) script;
    gLevelScriptModIndex = modIndex;
    dynos_level_parse_script(script, smlua_func_level_script_parse_callback);

    // Restore current values
    gLevelScriptActive = currLevelScript;
    gLevelScriptModIndex = currModIndex;
    return 0;
}

  ///////////////////////
 // custom animations //
///////////////////////

static u16 *smlua_to_u16_list(lua_State* L, int index, u32* length) {

    // Get number of values
    *length = lua_rawlen(L, index);
    if (!*length) { LOG_LUA("smlua_to_u16_list: Table must not be empty"); return NULL; }
    u16 *values = calloc(*length, sizeof(u16));

    // Retrieve values
    lua_pushnil(L);
    s32 top = lua_gettop(L);
    while (lua_next(L, index) != 0) {
        int indexKey = lua_gettop(L) - 1;
        int indexValue = lua_gettop(L) - 0;

        s32 key = smlua_to_integer(L, indexKey);
        if (!gSmLuaConvertSuccess) {
            LOG_LUA("smlua_to_u16_list: Failed to convert table key");
            free(values);
            return 0;
        }

        u16 value = smlua_to_integer(L, indexValue);
        if (!gSmLuaConvertSuccess) {
            LOG_LUA("smlua_to_u16_list: Failed to convert table value");
            free(values);
            return 0;
        }

        values[key - 1] = value;
        lua_settop(L, top);
    }
    lua_settop(L, top);
    return values;
}

int smlua_func_smlua_anim_util_register_animation(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 8)) { return 0; }

    const char *name = smlua_to_string(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("smlua_anim_util_register_animation: Failed to convert parameter 'name'"); return 0; }

    s16 flags = smlua_to_integer(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("smlua_anim_util_register_animation: Failed to convert parameter 'flags'"); return 0; }

    s16 animYTransDivisor = smlua_to_integer(L, 3);
    if (!gSmLuaConvertSuccess) { LOG_LUA("smlua_anim_util_register_animation: Failed to convert parameter 'animYTransDivisor'"); return 0; }

    s16 startFrame = smlua_to_integer(L, 4);
    if (!gSmLuaConvertSuccess) { LOG_LUA("smlua_anim_util_register_animation: Failed to convert parameter 'startFrame'"); return 0; }

    s16 loopStart = smlua_to_integer(L, 5);
    if (!gSmLuaConvertSuccess) { LOG_LUA("smlua_anim_util_register_animation: Failed to convert parameter 'loopStart'"); return 0; }

    s16 loopEnd = smlua_to_integer(L, 6);
    if (!gSmLuaConvertSuccess) { LOG_LUA("smlua_anim_util_register_animation: Failed to convert parameter 'loopEnd'"); return 0; }

    u32 valuesLength = 0;
    u16 *values = (u16 *) smlua_to_u16_list(L, 7, &valuesLength);
    if (!values) { LOG_LUA("smlua_anim_util_register_animation: Failed to convert parameter 'values'"); return 0; }

    u32 indexLength = 0;
    u16 *index = (u16 *) smlua_to_u16_list(L, 8, &indexLength);
    if (!index) { LOG_LUA("smlua_anim_util_register_animation: Failed to convert parameter 'index'"); free(values); return 0; }

    smlua_anim_util_register_animation(name, flags, animYTransDivisor, startFrame, loopStart, loopEnd, values, valuesLength, index, indexLength);

    return 1;
}

  /////////////
 // console //
/////////////

int smlua_func_log_to_console(lua_State* L) {
    if (!smlua_functions_valid_param_range(L, 1, 2)) { return 0; }

    int paramCount = lua_gettop(L);

    const char* message = smlua_to_string(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("log_to_console: Failed to convert parameter 1 for function"); return 0; }

    enum ConsoleMessageLevel level = CONSOLE_MESSAGE_INFO;
    if (paramCount >= 2) {
        level = smlua_to_integer(L, 2);
        if (!gSmLuaConvertSuccess) { LOG_LUA("log_to_console: Failed to convert parameter 2 for function"); return 0; }
    }

    djui_console_message_create(message, level);

    return 1;
}

  ////////////////////
 // scroll targets //
////////////////////

int smlua_func_add_scroll_target(lua_State* L) {
    // add_scroll_target used to require offset and size of the vertex buffer to be used
    if (!smlua_functions_valid_param_range(L, 2, 4)) { return 0; }
    int paramCount = lua_gettop(L);

    u32 index = smlua_to_integer(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("add_scroll_target: Failed to convert parameter 1 for function"); return 0; }
    const char* name = smlua_to_string(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("add_scroll_target: Failed to convert parameter 2 for function"); return 0; }

    // If the offset and size parameters are provided, use them, although they aren't required.
    u32 offset = 0;
    u32 size = 0;
    switch (paramCount) {
        case 4:
            size = smlua_to_integer(L, 4);
        case 3:
            offset = smlua_to_integer(L, 3);
            break;
    }

    dynos_add_scroll_target(index, name, offset, size);

    return 1;
}

  /////////////
 // raycast //
/////////////

int smlua_func_collision_find_surface_on_ray(lua_State* L) {
    if (!smlua_functions_valid_param_range(L, 6, 7)) { return 0; }
    int paramCount = lua_gettop(L);

    f32 startX = smlua_to_number(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("collision_find_surface_on_ray: Failed to convert parameter 1"); return 0; }
    f32 startY = smlua_to_number(L, 2);
    if (!gSmLuaConvertSuccess) { LOG_LUA("collision_find_surface_on_ray: Failed to convert parameter 2"); return 0; }
    f32 startZ = smlua_to_number(L, 3);
    if (!gSmLuaConvertSuccess) { LOG_LUA("collision_find_surface_on_ray: Failed to convert parameter 3"); return 0; }
    f32 dirX = smlua_to_number(L, 4);
    if (!gSmLuaConvertSuccess) { LOG_LUA("collision_find_surface_on_ray: Failed to convert parameter 4"); return 0; }
    f32 dirY = smlua_to_number(L, 5);
    if (!gSmLuaConvertSuccess) { LOG_LUA("collision_find_surface_on_ray: Failed to convert parameter 5"); return 0; }
    f32 dirZ = smlua_to_number(L, 6);
    if (!gSmLuaConvertSuccess) { LOG_LUA("collision_find_surface_on_ray: Failed to convert parameter 6"); return 0; }
    f32 precision = paramCount == 7 ? smlua_to_number(L, 7) : 3.0f;
    if (!gSmLuaConvertSuccess) { LOG_LUA("collision_find_surface_on_ray: Failed to convert parameter 7"); return 0; }

    smlua_push_object(L, LOT_RAYINTERSECTIONINFO, collision_find_surface_on_ray(startX, startY, startZ, dirX, dirY, dirZ, precision), NULL);

    return 1;
}

  ////////////////
 // graph node //
////////////////

typedef struct { s16 type; u16 lot; } GraphNodeLot;
static GraphNodeLot graphNodeLots[] = {
    { GRAPH_NODE_TYPE_ANIMATED_PART, LOT_GRAPHNODEANIMATEDPART },
    { GRAPH_NODE_TYPE_BACKGROUND, LOT_GRAPHNODEBACKGROUND },
    { GRAPH_NODE_TYPE_BILLBOARD, LOT_GRAPHNODEBILLBOARD },
    { GRAPH_NODE_TYPE_CAMERA, LOT_GRAPHNODECAMERA },
    { GRAPH_NODE_TYPE_CULLING_RADIUS, LOT_GRAPHNODECULLINGRADIUS },
    { GRAPH_NODE_TYPE_DISPLAY_LIST, LOT_GRAPHNODEDISPLAYLIST },
    { GRAPH_NODE_TYPE_FUNCTIONAL, LOT_FNGRAPHNODE },
    { GRAPH_NODE_TYPE_GENERATED_LIST, LOT_GRAPHNODEGENERATED },
    { GRAPH_NODE_TYPE_HELD_OBJ, LOT_GRAPHNODEHELDOBJECT },
    { GRAPH_NODE_TYPE_LEVEL_OF_DETAIL, LOT_GRAPHNODELEVELOFDETAIL },
    { GRAPH_NODE_TYPE_MASTER_LIST, LOT_GRAPHNODEMASTERLIST },
    { GRAPH_NODE_TYPE_OBJECT, LOT_GRAPHNODEOBJECT },
    { GRAPH_NODE_TYPE_OBJECT_PARENT, LOT_GRAPHNODEOBJECTPARENT },
    { GRAPH_NODE_TYPE_ORTHO_PROJECTION, LOT_GRAPHNODEORTHOPROJECTION },
    { GRAPH_NODE_TYPE_PERSPECTIVE, LOT_GRAPHNODEPERSPECTIVE },
    { GRAPH_NODE_TYPE_ROOT, LOT_GRAPHNODE },
    { GRAPH_NODE_TYPE_ROTATION, LOT_GRAPHNODEROTATION },
    { GRAPH_NODE_TYPE_SCALE, LOT_GRAPHNODESCALE },
    { GRAPH_NODE_TYPE_SHADOW, LOT_GRAPHNODESHADOW },
    { GRAPH_NODE_TYPE_START, LOT_GRAPHNODESTART },
    { GRAPH_NODE_TYPE_SWITCH_CASE, LOT_GRAPHNODESWITCHCASE },
    { GRAPH_NODE_TYPE_TRANSLATION, LOT_GRAPHNODETRANSLATION },
    { GRAPH_NODE_TYPE_TRANSLATION_ROTATION, LOT_GRAPHNODETRANSLATIONROTATION },
    { GRAPH_NODE_TYPE_BONE, LOT_GRAPHNODEBONE },
};

int smlua_func_cast_graph_node(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    struct GraphNode* graphNode;

    if (smlua_is_cobject(L, 1, LOT_GRAPHNODE)) {
        graphNode = (struct GraphNode*)smlua_to_cobject(L, 1, LOT_GRAPHNODE);
        if (!gSmLuaConvertSuccess) { LOG_LUA("cast_graph_node: Failed to convert parameter 1"); return 0; }
    } else if (smlua_is_cobject(L, 1, LOT_FNGRAPHNODE)) {
        graphNode = (struct GraphNode*)smlua_to_cobject(L, 1, LOT_FNGRAPHNODE);
        if (!gSmLuaConvertSuccess) { LOG_LUA("cast_graph_node: Failed to convert parameter 1"); return 0; }
    } else {
        LOG_LUA("cast_graph_node: Failed to convert parameter 1");
        return 0;
    }

    u16 lot = 0;
    for (u8 i = 0; i < ARRAY_COUNT(graphNodeLots); i++) {
        if (graphNode->type != graphNodeLots[i].type) continue;
        lot = graphNodeLots[i].lot;
        break;
    }
    if (lot == 0) {
        LOG_LUA("cast_graph_node: Invalid GraphNode type");
        return 0;
    }

    smlua_push_object(L, lot, graphNode, NULL);

    // Register this graph node as modified so it can be reset later
    dynos_actor_register_modified_graph_node(graphNode);

    return 1;
}

  /////////////
 // strings //
/////////////

int smlua_func_get_uncolored_string(lua_State* L) {
    if (!smlua_functions_valid_param_count(L, 1)) { return 0; }

    const char *str = smlua_to_string(L, 1);
    if (!gSmLuaConvertSuccess) { LOG_LUA("get_uncolored_string: Failed to convert parameter 1"); return 0; }

    char *strNoColor = str_remove_color_codes(str);
    lua_pushstring(L, strNoColor);
    free(strNoColor);

    return 1;
}

  //////////////////
 // display list //
//////////////////

static int get_gfx_command_specifiers_count(const char *command) {
    int count = 0;
    for (; *command; count += (*command == '%'), command++);
    return count;
}

int smlua_func_gfx_set_command(lua_State* L) {
    int top = lua_gettop(L);
    if (top < 2) {
        LOG_LUA_LINE("gfx_set_command: Improper param count: Expected at least 2, Received %u", top);
        return 0;
    }

    Gfx* gfx = smlua_to_cobject(L, 1, LOT_GFX);
    if (!gSmLuaConvertSuccess || !gfx) {
        LOG_LUA_LINE("gfx_set_command: Failed to convert parameter %u", 1);
        return 0;
    }

    const char *command = smlua_to_string(L, 2);
    if (!gSmLuaConvertSuccess) {
        LOG_LUA_LINE("gfx_set_command: Failed to convert parameter %u", 2);
        return 0;
    }

    // Compare the number of provided parameters to the number of specifiers in the command
    int paramCount = top - 2;
    int specifiersCount = get_gfx_command_specifiers_count(command);
    if (specifiersCount != paramCount) {
        LOG_LUA_LINE("gfx_set_command: Command \"%s\": Invalid number of command parameters: Expected %u, provided %u", command, specifiersCount, paramCount);
        return 0;
    }

    // Parse the command
    const u32 errorSize = 0x400;
    char errorMsg[errorSize];
    if (!dynos_smlua_parse_gfx_command(L, gfx, command, specifiersCount != 0, errorMsg, errorSize)) {
        LOG_LUA_LINE("gfx_set_command: Command \"%s\": %s", command, errorMsg);
        return 0;
    }

    return 1;
}

#if defined(TARGET_WII_U)
static void smlua_wiiu_set_global_function(lua_State *L, const char *name, lua_CFunction fn) {
    lua_pushcfunction(L, fn);
    lua_setglobal(L, name);
}

static void smlua_wiiu_set_math_function(lua_State *L, const char *name, lua_CFunction fn) {
    lua_getglobal(L, "math");
    lua_pushcfunction(L, fn);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

static double smlua_wiiu_num(lua_State *L, int index) {
    return luaL_checknumber(L, index);
}

static int smlua_wiiu_push_num(lua_State *L, double value) {
    lua_pushnumber(L, value);
    return 1;
}

static int smlua_func_SOUND_ARG_LOAD(lua_State *L) {
    int top = lua_gettop(L);
    if (top < 3 || top > 4) {
        LOG_LUA_LINE("Improper param count for 'SOUND_ARG_LOAD': Expected 3-4, Received %u", top);
        return 0;
    }

    s32 bank = smlua_to_integer(L, 1);
    if (!gSmLuaConvertSuccess) { return 0; }
    s32 soundId = smlua_to_integer(L, 2);
    if (!gSmLuaConvertSuccess) { return 0; }
    s32 priority = smlua_to_integer(L, 3);
    if (!gSmLuaConvertSuccess) { return 0; }
    s32 flags = 0;
    if (top >= 4 && !lua_isnil(L, 4)) {
        flags = smlua_to_integer(L, 4);
        if (!gSmLuaConvertSuccess) { return 0; }
    }

    lua_pushinteger(L, (s32)SOUND_ARG_LOAD(bank, soundId, priority, flags));
    return 1;
}

static int smlua_wiiu_network_player_palette(lua_State *L, bool override) {
    if (!smlua_functions_valid_param_count(L, 2)) { return 0; }

    struct NetworkPlayer *np = (struct NetworkPlayer *)smlua_to_cobject(L, 1, LOT_NETWORKPLAYER);
    if (!gSmLuaConvertSuccess) { return 0; }
    int part = smlua_to_integer(L, 2);
    if (!gSmLuaConvertSuccess) { return 0; }

    lua_newtable(L);
    lua_pushinteger(L, override ? network_player_get_override_palette_color_channel(np, part, 0) : network_player_get_palette_color_channel(np, part, 0));
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, override ? network_player_get_override_palette_color_channel(np, part, 1) : network_player_get_palette_color_channel(np, part, 1));
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, override ? network_player_get_override_palette_color_channel(np, part, 2) : network_player_get_palette_color_channel(np, part, 2));
    lua_setfield(L, -2, "b");
    return 1;
}

static int smlua_func_network_player_get_palette_color(lua_State *L) {
    return smlua_wiiu_network_player_palette(L, false);
}

static int smlua_func_network_player_get_override_palette_color(lua_State *L) {
    return smlua_wiiu_network_player_palette(L, true);
}

static int smlua_math_sqr(lua_State *L) { double x = smlua_wiiu_num(L, 1); return smlua_wiiu_push_num(L, x * x); }
static int smlua_math_clamp(lua_State *L) {
    double x = smlua_wiiu_num(L, 1), a = smlua_wiiu_num(L, 2), b = smlua_wiiu_num(L, 3);
    if (x < a) { x = a; }
    if (x > b) { x = b; }
    return smlua_wiiu_push_num(L, x);
}
static int smlua_math_hypot(lua_State *L) {
    double a = smlua_wiiu_num(L, 1), b = smlua_wiiu_num(L, 2);
    return smlua_wiiu_push_num(L, sqrt(a * a + b * b));
}
static int smlua_math_sign(lua_State *L) { lua_pushinteger(L, smlua_wiiu_num(L, 1) >= 0 ? 1 : -1); return 1; }
static int smlua_math_sign0(lua_State *L) {
    double x = smlua_wiiu_num(L, 1);
    lua_pushinteger(L, x != 0 ? (x > 0 ? 1 : -1) : 0);
    return 1;
}
static int smlua_math_lerp(lua_State *L) {
    double a = smlua_wiiu_num(L, 1), b = smlua_wiiu_num(L, 2), t = smlua_wiiu_num(L, 3);
    return smlua_wiiu_push_num(L, a + (b - a) * t);
}
static int smlua_math_invlerp(lua_State *L) {
    double a = smlua_wiiu_num(L, 1), b = smlua_wiiu_num(L, 2), x = smlua_wiiu_num(L, 3);
    return smlua_wiiu_push_num(L, (x - a) / (b - a));
}
static int smlua_math_remap(lua_State *L) {
    double a = smlua_wiiu_num(L, 1), b = smlua_wiiu_num(L, 2), c = smlua_wiiu_num(L, 3), d = smlua_wiiu_num(L, 4), x = smlua_wiiu_num(L, 5);
    return smlua_wiiu_push_num(L, c + (d - c) * ((x - a) / (b - a)));
}
static int smlua_math_round(lua_State *L) {
    double x = smlua_wiiu_num(L, 1);
    lua_pushinteger(L, (lua_Integer)(x > 0 ? floor(x + 0.5) : ceil(x - 0.5)));
    return 1;
}
static lua_Integer smlua_math_signed_bits(double x, int bits) {
    u64 mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
    u64 u = ((u64)(s64)floor(x)) & mask;
    u64 sign = 1ULL << (bits - 1);
    s64 out = (s64)u;
    if (u & sign) { out -= (s64)(1ULL << bits); }
    return (lua_Integer)out;
}
static lua_Integer smlua_math_unsigned_bits(double x, int bits) {
    u64 mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
    return (lua_Integer)(((u64)(s64)floor(x)) & mask);
}
static int smlua_math_s8(lua_State *L) { lua_pushinteger(L, smlua_math_signed_bits(smlua_wiiu_num(L, 1), 8)); return 1; }
static int smlua_math_s16(lua_State *L) { lua_pushinteger(L, smlua_math_signed_bits(smlua_wiiu_num(L, 1), 16)); return 1; }
static int smlua_math_s32(lua_State *L) { lua_pushinteger(L, smlua_math_signed_bits(smlua_wiiu_num(L, 1), 32)); return 1; }
static int smlua_math_u8(lua_State *L) { lua_pushinteger(L, smlua_math_unsigned_bits(smlua_wiiu_num(L, 1), 8)); return 1; }
static int smlua_math_u16(lua_State *L) { lua_pushinteger(L, smlua_math_unsigned_bits(smlua_wiiu_num(L, 1), 16)); return 1; }
static int smlua_math_u32(lua_State *L) { lua_pushinteger(L, smlua_math_unsigned_bits(smlua_wiiu_num(L, 1), 32)); return 1; }

#define WIIU_LUA_PI 3.14159265358979323846
static double tw_out_bounce(double x) {
    if (x < 1.0 / 2.75) { return 7.5625 * x * x; }
    if (x < 2.0 / 2.75) { x -= 1.5 / 2.75; return 7.5625 * x * x + 0.75; }
    if (x < 2.5 / 2.75) { x -= 2.25 / 2.75; return 7.5625 * x * x + 0.9375; }
    x -= 2.625 / 2.75;
    return 7.5625 * x * x + 0.984375;
}
static int tw_push(lua_State *L, double value) { return smlua_wiiu_push_num(L, value); }
static int tw_in_sine(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 1.0 - cos((x * WIIU_LUA_PI) / 2.0)); }
static int tw_out_sine(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, sin((x * WIIU_LUA_PI) / 2.0)); }
static int tw_in_out_sine(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, -(cos(WIIU_LUA_PI * x) - 1.0) / 2.0); }
static int tw_out_in_sine(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 0.5 * sin(x * WIIU_LUA_PI) : 1.0 - 0.5 * cos(((x * 2.0 - 1.0) * (WIIU_LUA_PI / 2.0)))); }
static int tw_in_quad(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x * x); }
static int tw_out_quad(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 1.0 - ((1.0 - x) * (1.0 - x))); }
static int tw_in_out_quad(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 2.0 * x * x : 1.0 - pow(-2.0 * x + 2.0, 2.0) / 2.0); }
static int tw_out_in_quad(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 0.5 * (-(2.0 * x) * ((2.0 * x) - 2.0)) : 0.5 + 0.5 * pow(2.0 * x - 1.0, 2.0)); }
static int tw_in_cubic(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x * x * x); }
static int tw_out_cubic(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 1.0 - pow(1.0 - x, 3.0)); }
static int tw_in_out_cubic(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 4.0 * x * x * x : 1.0 - pow(-2.0 * x + 2.0, 3.0) / 2.0); }
static int tw_out_in_cubic(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 0.5 * (pow(2.0 * x - 1.0, 3.0) + 1.0) : 0.5 + 0.5 * pow(2.0 * x - 1.0, 3.0)); }
static int tw_in_quart(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, pow(x, 4.0)); }
static int tw_out_quart(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 1.0 - pow(1.0 - x, 4.0)); }
static int tw_in_out_quart(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 8.0 * pow(x, 4.0) : 1.0 - pow(-2.0 * x + 2.0, 4.0) / 2.0); }
static int tw_out_in_quart(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 0.5 * (1.0 - pow(2.0 * x - 1.0, 4.0)) : 0.5 + 0.5 * pow(2.0 * x - 1.0, 4.0)); }
static int tw_in_quint(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, pow(x, 5.0)); }
static int tw_out_quint(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 1.0 - pow(1.0 - x, 5.0)); }
static int tw_in_out_quint(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 16.0 * pow(x, 5.0) : 1.0 - pow(-2.0 * x + 2.0, 5.0) / 2.0); }
static int tw_out_in_quint(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 0.5 * (pow(2.0 * x - 1.0, 5.0) + 1.0) : 0.5 + 0.5 * pow(2.0 * x - 1.0, 5.0)); }
static int tw_in_expo(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x == 0.0 ? x : pow(2.0, 10.0 * x - 10.0)); }
static int tw_out_expo(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x == 1.0 ? x : 1.0 - pow(2.0, -10.0 * x)); }
static int tw_in_out_expo(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, (x == 0.0 || x == 1.0) ? x : (x < 0.5 ? pow(2.0, 20.0 * x - 10.0) / 2.0 : (2.0 - pow(2.0, -20.0 * x + 10.0)) / 2.0)); }
static int tw_out_in_expo(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, (x == 0.0 || x == 1.0) ? x : (x < 0.5 ? 0.5 * (1.0 - pow(2.0, -20.0 * x)) : 0.5 + 0.5 * pow(2.0, 20.0 * x - 20.0))); }
static int tw_in_circ(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 1.0 - sqrt(1.0 - x * x)); }
static int tw_out_circ(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, sqrt(1.0 - (x - 1.0) * (x - 1.0))); }
static int tw_in_out_circ(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? (1.0 - sqrt(1.0 - pow(2.0 * x, 2.0))) / 2.0 : (sqrt(1.0 - pow(-2.0 * x + 2.0, 2.0)) + 1.0) / 2.0); }
static int tw_out_in_circ(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 0.5 * sqrt(1.0 - pow(2.0 * x - 1.0, 2.0)) : 0.5 + 0.5 * (1.0 - sqrt(1.0 - pow(2.0 * x - 1.0, 2.0)))); }
static int tw_in_back(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 2.70158 * pow(x, 3.0) - 1.70158 * x * x); }
static int tw_out_back(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 1.0 + 2.70158 * pow(x - 1.0, 3.0) + 1.70158 * pow(x - 1.0, 2.0)); }
static int tw_in_out_back(lua_State *L) { double x = smlua_wiiu_num(L, 1); double c = 1.70158 * 1.525; return tw_push(L, x < 0.5 ? (pow(2.0 * x, 2.0) * ((c + 1.0) * 2.0 * x - c)) / 2.0 : (pow(2.0 * x - 2.0, 2.0) * ((c + 1.0) * (x * 2.0 - 2.0) + c) + 2.0) / 2.0); }
static int tw_out_in_back(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 0.5 * (1.0 + 2.70158 * pow(2.0 * x - 1.0, 3.0) + 1.70158 * pow(2.0 * x - 1.0, 2.0)) : 0.5 + 0.5 * (2.70158 * pow(2.0 * x - 1.0, 3.0) - 1.70158 * pow(2.0 * x - 1.0, 2.0))); }
static int tw_in_elastic(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, (x == 0.0 || x == 1.0) ? x : -pow(2.0, 10.0 * x - 10.0) * sin((x * 10.0 - 10.75) * ((2.0 * WIIU_LUA_PI) / 3.0))); }
static int tw_out_elastic(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, (x == 0.0 || x == 1.0) ? x : pow(2.0, -10.0 * x) * sin((x * 10.0 - 0.75) * ((2.0 * WIIU_LUA_PI) / 3.0)) + 1.0); }
static int tw_in_out_elastic(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, (x == 0.0 || x == 1.0) ? x : (x < 0.5 ? -0.5 * pow(2.0, 20.0 * x - 10.0) * sin((20.0 * x - 11.125) * ((2.0 * WIIU_LUA_PI) / 4.5)) : 0.5 * pow(2.0, -20.0 * x + 10.0) * sin((20.0 * x - 11.125) * ((2.0 * WIIU_LUA_PI) / 4.5)) + 1.0)); }
static int tw_out_in_elastic(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, (x == 0.0 || x == 1.0) ? x : (x < 0.5 ? 0.5 * (pow(2.0, -10.0 * (x * 2.0)) * sin(((x * 2.0) * 10.0 - 0.75) * ((2.0 * WIIU_LUA_PI) / 3.0)) + 1.0) : 0.5 + 0.5 * (-pow(2.0, 10.0 * ((x - 0.5) * 2.0) - 10.0) * sin((((x - 0.5) * 2.0) * 10.0 - 10.75) * ((2.0 * WIIU_LUA_PI) / 3.0))))); }
static int tw_in_bounce(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, 1.0 - tw_out_bounce(1.0 - x)); }
static int tw_out_bounce_lua(lua_State *L) { return tw_push(L, tw_out_bounce(smlua_wiiu_num(L, 1))); }
static int tw_in_out_bounce(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? (1.0 - tw_out_bounce(1.0 - 2.0 * x)) / 2.0 : (1.0 + tw_out_bounce(2.0 * x - 1.0)) / 2.0); }
static int tw_out_in_bounce(lua_State *L) { double x = smlua_wiiu_num(L, 1); return tw_push(L, x < 0.5 ? 0.5 * tw_out_bounce(x * 2.0) : 0.5 + 0.5 * (1.0 - tw_out_bounce(1.0 - (2.0 * x - 1.0)))); }

static int smlua_math_tween(lua_State *L) {
    double a = smlua_wiiu_num(L, 2), b = smlua_wiiu_num(L, 3), x = smlua_wiiu_num(L, 4);
    double t;
    if (lua_isfunction(L, 1)) {
        lua_pushvalue(L, 1);
        lua_pushnumber(L, x);
        lua_call(L, 1, 1);
        t = luaL_checknumber(L, -1);
    } else {
        t = smlua_wiiu_num(L, 1);
    }
    return smlua_wiiu_push_num(L, a + t * (b - a));
}

void smlua_bind_wiiu_builtin_helpers(void) {
    lua_State *L = gLuaState;

    smlua_bind_function(L, "SOUND_ARG_LOAD", smlua_func_SOUND_ARG_LOAD);
    smlua_bind_function(L, "network_player_get_palette_color", smlua_func_network_player_get_palette_color);
    smlua_bind_function(L, "network_player_get_override_palette_color", smlua_func_network_player_get_override_palette_color);

    smlua_wiiu_set_math_function(L, "sqr", smlua_math_sqr);
    smlua_wiiu_set_math_function(L, "clamp", smlua_math_clamp);
    smlua_wiiu_set_math_function(L, "hypot", smlua_math_hypot);
    smlua_wiiu_set_math_function(L, "sign", smlua_math_sign);
    smlua_wiiu_set_math_function(L, "sign0", smlua_math_sign0);
    smlua_wiiu_set_math_function(L, "lerp", smlua_math_lerp);
    smlua_wiiu_set_math_function(L, "invlerp", smlua_math_invlerp);
    smlua_wiiu_set_math_function(L, "remap", smlua_math_remap);
    smlua_wiiu_set_math_function(L, "round", smlua_math_round);
    smlua_wiiu_set_math_function(L, "tween", smlua_math_tween);
    smlua_wiiu_set_math_function(L, "s8", smlua_math_s8);
    smlua_wiiu_set_math_function(L, "s16", smlua_math_s16);
    smlua_wiiu_set_math_function(L, "s32", smlua_math_s32);
    smlua_wiiu_set_math_function(L, "u8", smlua_math_u8);
    smlua_wiiu_set_math_function(L, "u16", smlua_math_u16);
    smlua_wiiu_set_math_function(L, "u32", smlua_math_u32);

    smlua_wiiu_set_global_function(L, "IN_SINE", tw_in_sine);
    smlua_wiiu_set_global_function(L, "OUT_SINE", tw_out_sine);
    smlua_wiiu_set_global_function(L, "IN_OUT_SINE", tw_in_out_sine);
    smlua_wiiu_set_global_function(L, "OUT_IN_SINE", tw_out_in_sine);
    smlua_wiiu_set_global_function(L, "IN_QUAD", tw_in_quad);
    smlua_wiiu_set_global_function(L, "OUT_QUAD", tw_out_quad);
    smlua_wiiu_set_global_function(L, "IN_OUT_QUAD", tw_in_out_quad);
    smlua_wiiu_set_global_function(L, "OUT_IN_QUAD", tw_out_in_quad);
    smlua_wiiu_set_global_function(L, "IN_CUBIC", tw_in_cubic);
    smlua_wiiu_set_global_function(L, "OUT_CUBIC", tw_out_cubic);
    smlua_wiiu_set_global_function(L, "IN_OUT_CUBIC", tw_in_out_cubic);
    smlua_wiiu_set_global_function(L, "OUT_IN_CUBIC", tw_out_in_cubic);
    smlua_wiiu_set_global_function(L, "IN_QUART", tw_in_quart);
    smlua_wiiu_set_global_function(L, "OUT_QUART", tw_out_quart);
    smlua_wiiu_set_global_function(L, "IN_OUT_QUART", tw_in_out_quart);
    smlua_wiiu_set_global_function(L, "OUT_IN_QUART", tw_out_in_quart);
    smlua_wiiu_set_global_function(L, "IN_QUINT", tw_in_quint);
    smlua_wiiu_set_global_function(L, "OUT_QUINT", tw_out_quint);
    smlua_wiiu_set_global_function(L, "IN_OUT_QUINT", tw_in_out_quint);
    smlua_wiiu_set_global_function(L, "OUT_IN_QUINT", tw_out_in_quint);
    smlua_wiiu_set_global_function(L, "IN_EXPO", tw_in_expo);
    smlua_wiiu_set_global_function(L, "OUT_EXPO", tw_out_expo);
    smlua_wiiu_set_global_function(L, "IN_OUT_EXPO", tw_in_out_expo);
    smlua_wiiu_set_global_function(L, "OUT_IN_EXPO", tw_out_in_expo);
    smlua_wiiu_set_global_function(L, "IN_CIRC", tw_in_circ);
    smlua_wiiu_set_global_function(L, "OUT_CIRC", tw_out_circ);
    smlua_wiiu_set_global_function(L, "IN_OUT_CIRC", tw_in_out_circ);
    smlua_wiiu_set_global_function(L, "OUT_IN_CIRC", tw_out_in_circ);
    smlua_wiiu_set_global_function(L, "IN_BACK", tw_in_back);
    smlua_wiiu_set_global_function(L, "OUT_BACK", tw_out_back);
    smlua_wiiu_set_global_function(L, "IN_OUT_BACK", tw_in_out_back);
    smlua_wiiu_set_global_function(L, "OUT_IN_BACK", tw_out_in_back);
    smlua_wiiu_set_global_function(L, "IN_ELASTIC", tw_in_elastic);
    smlua_wiiu_set_global_function(L, "OUT_ELASTIC", tw_out_elastic);
    smlua_wiiu_set_global_function(L, "IN_OUT_ELASTIC", tw_in_out_elastic);
    smlua_wiiu_set_global_function(L, "OUT_IN_ELASTIC", tw_out_in_elastic);
    smlua_wiiu_set_global_function(L, "IN_BOUNCE", tw_in_bounce);
    smlua_wiiu_set_global_function(L, "OUT_BOUNCE", tw_out_bounce_lua);
    smlua_wiiu_set_global_function(L, "IN_OUT_BOUNCE", tw_in_out_bounce);
    smlua_wiiu_set_global_function(L, "OUT_IN_BOUNCE", tw_out_in_bounce);
}

static void smlua_wiiu_set_number_field(lua_State *L, const char *key, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, -2, key);
}

static void smlua_wiiu_finish_read_only_global(lua_State *L, const char *name) {
    int rawIndex = lua_gettop(L);
    lua_newtable(L);
    int proxyIndex = lua_gettop(L);

    lua_pushvalue(L, rawIndex);
    lua_setfield(L, proxyIndex, "_table");

    lua_getglobal(L, "_ReadOnlyTable");
    if (lua_type(L, -1) == LUA_TTABLE) {
        lua_setmetatable(L, proxyIndex);
    } else {
        lua_pop(L, 1);
    }

    lua_remove(L, rawIndex);
    lua_setglobal(L, name);
}

static void smlua_wiiu_bind_vec(lua_State *L, const char *name, double x, double y, double z, double w, int count) {
    lua_newtable(L);
    smlua_wiiu_set_number_field(L, "x", x);
    smlua_wiiu_set_number_field(L, "y", y);
    if (count >= 3) { smlua_wiiu_set_number_field(L, "z", z); }
    if (count >= 4) { smlua_wiiu_set_number_field(L, "w", w); }
    smlua_wiiu_finish_read_only_global(L, name);
}

static void smlua_wiiu_bind_mat4(lua_State *L, const char *name, const double *m) {
    static const char *keys[] = {
        "m00", "m01", "m02", "m03",
        "m10", "m11", "m12", "m13",
        "m20", "m21", "m22", "m23",
        "m30", "m31", "m32", "m33",
    };

    lua_newtable(L);
    for (int i = 0; i < 16; i++) {
        smlua_wiiu_set_number_field(L, keys[i], m[i]);
    }
    smlua_wiiu_finish_read_only_global(L, name);
}

void smlua_bind_wiiu_read_only_constants(void) {
    lua_State *L = gLuaState;
    static const double mat4Zero[16] = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    };
    static const double mat4Identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
    static const double mat4Fullscreen[16] = {
        0.00625, 0, 0, 0,
        0, 0.008333333333333333, 0, 0,
        0, 0, -1, 0,
        -1, -1, -1, 1,
    };

    smlua_wiiu_bind_vec(L, "gGlobalSoundSource", 0, 0, 0, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec2fZero", 0, 0, 0, 0, 2);
    smlua_wiiu_bind_vec(L, "gVec2fOne", 1, 1, 0, 0, 2);
    smlua_wiiu_bind_vec(L, "gVec3fZero", 0, 0, 0, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec3fOne", 1, 1, 1, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec3fX", 1, 0, 0, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec3fY", 0, 1, 0, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec3fZ", 0, 0, 1, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec4fZero", 0, 0, 0, 0, 4);
    smlua_wiiu_bind_vec(L, "gVec4fOne", 1, 1, 1, 1, 4);
    smlua_wiiu_bind_vec(L, "gVec2iZero", 0, 0, 0, 0, 2);
    smlua_wiiu_bind_vec(L, "gVec2iOne", 1, 1, 0, 0, 2);
    smlua_wiiu_bind_vec(L, "gVec3iZero", 0, 0, 0, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec3iOne", 1, 1, 1, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec4iZero", 0, 0, 0, 0, 4);
    smlua_wiiu_bind_vec(L, "gVec4iOne", 1, 1, 1, 1, 4);
    smlua_wiiu_bind_vec(L, "gVec2sZero", 0, 0, 0, 0, 2);
    smlua_wiiu_bind_vec(L, "gVec2sOne", 1, 1, 0, 0, 2);
    smlua_wiiu_bind_vec(L, "gVec3sZero", 0, 0, 0, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec3sOne", 1, 1, 1, 0, 3);
    smlua_wiiu_bind_vec(L, "gVec4sZero", 0, 0, 0, 0, 4);
    smlua_wiiu_bind_vec(L, "gVec4sOne", 1, 1, 1, 1, 4);
    smlua_wiiu_bind_mat4(L, "gMat4Zero", mat4Zero);
    smlua_wiiu_bind_mat4(L, "gMat4Identity", mat4Identity);
    smlua_wiiu_bind_mat4(L, "gMat4Fullscreen", mat4Fullscreen);
}
#endif

  //////////
 // bind //
//////////

void smlua_bind_functions(void) {
    lua_State* L = gLuaState;

    // misc
    smlua_bind_function(L, "table_copy", smlua_func_table_copy);
    smlua_bind_function(L, "table_deepcopy", smlua_func_table_deepcopy);
    smlua_bind_function(L, "create_read_only_table", smlua_func_create_read_only_table);
    smlua_bind_function(L, "init_mario_after_warp", smlua_func_init_mario_after_warp);
    smlua_bind_function(L, "network_init_object", smlua_func_network_init_object);
    smlua_bind_function(L, "network_send_object", smlua_func_network_send_object);
    smlua_bind_function(L, "reset_level", smlua_func_reset_level);
    smlua_bind_function(L, "network_send", smlua_func_network_send);
    smlua_bind_function(L, "network_send_to", smlua_func_network_send_to);
    smlua_bind_function(L, "network_send_bytestring", smlua_func_network_send_bytestring);
    smlua_bind_function(L, "network_send_bytestring_to", smlua_func_network_send_bytestring_to);
    smlua_bind_function(L, "set_exclamation_box_contents", smlua_func_set_exclamation_box_contents);
    smlua_bind_function(L, "get_exclamation_box_contents", smlua_func_get_exclamation_box_contents);
    smlua_bind_function(L, "get_texture_info", smlua_func_get_texture_info);
    smlua_bind_function(L, "texture_override_set", smlua_func_texture_override_set);
    smlua_bind_function(L, "texture_override_reset", smlua_func_texture_override_reset);
    smlua_bind_function(L, "level_script_parse", smlua_func_level_script_parse);
    smlua_bind_function(L, "smlua_anim_util_register_animation", smlua_func_smlua_anim_util_register_animation);
    smlua_bind_function(L, "log_to_console", smlua_func_log_to_console);
    smlua_bind_function(L, "add_scroll_target", smlua_func_add_scroll_target);
    smlua_bind_function(L, "collision_find_surface_on_ray", smlua_func_collision_find_surface_on_ray);
    smlua_bind_function(L, "cast_graph_node", smlua_func_cast_graph_node);
    smlua_bind_function(L, "get_uncolored_string", smlua_func_get_uncolored_string);
    smlua_bind_function(L, "gfx_set_command", smlua_func_gfx_set_command);
}

void smlua_bind_table_functions(void) {
    lua_State* L = gLuaState;
#if defined(TARGET_WII_U)
    wiiu_diag_mark("smlua_bind_table_functions: skipped on Wii U");
    (void)L;
    return;
#endif
    lua_getglobal(L, "table");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_pushcfunction(L, smlua_func_table_copy);
    lua_setfield(L, -2, "copy");

    lua_pushcfunction(L, smlua_func_table_deepcopy);
    lua_setfield(L, -2, "deepcopy");

    lua_pop(L, 1);
}