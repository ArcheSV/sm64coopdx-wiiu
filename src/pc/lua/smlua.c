#include "smlua.h"
#include "pc/lua/smlua_require.h"
#include "pc/lua/smlua_live_reload.h"
#include "game/hardcoded.h"
#include "pc/mods/mods.h"
#include "pc/mods/mods_utils.h"
#include "pc/mods/mod_storage.h"
#include "pc/mods/mod_fs.h"
#include "pc/crash_handler.h"
#include "pc/lua/utils/smlua_text_utils.h"
#include "pc/lua/utils/smlua_audio_utils.h"
#include "pc/lua/utils/smlua_model_utils.h"
#include "pc/lua/utils/smlua_level_utils.h"
#include "pc/lua/utils/smlua_anim_utils.h"
#include "pc/djui/djui.h"
#include "pc/fs/fmem.h"
#include "pc/platform.h"
#include "include/sounds.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

lua_State* gLuaState = NULL;
u8 gLuaInitializingScript = 0;
u8 gSmLuaSuppressErrors = 0;
struct Mod* gLuaLoadingMod = NULL;
struct Mod* gLuaActiveMod = NULL;
struct ModFile* gLuaActiveModFile = NULL;
struct Mod* gLuaLastHookMod = NULL;

void smlua_mod_error(void) {
    struct Mod* mod = gLuaActiveMod;
    if (mod == NULL) { mod = gLuaLastHookMod; }
    if (mod == NULL) { return; }
    char txt[255] = { 0 };
    snprintf(txt, 254, "'%s\\#ff0000\\' has script errors!", mod->name);
    static const struct DjuiColor color = { 255, 0, 0, 255 };
    djui_lua_error(txt, color);
}

void smlua_mod_warning(void) {
    struct Mod* mod = gLuaActiveMod;
    if (mod == NULL) { mod = gLuaLastHookMod; }
    if (mod == NULL) { return; }
    if (mod->ignoreScriptWarnings) { return; }
    char txt[255] = { 0 };
    snprintf(txt, 254, "'%s\\#ffe600\\' has script warnings!", mod->name);
    static const struct DjuiColor color = { 255, 230, 0, 255 };
    djui_lua_error(txt, color);
}

int smlua_error_handler(lua_State* L) {
    if (lua_type(L, -1) == LUA_TSTRING) {
        const char *msg = lua_tostring(L, -1);
#if defined(TARGET_WII_U)
        wiiu_diag_mark("LUA ERROR: %s", msg ? msg : "(null)");
        sys_trace("LUA ERROR: %s", msg ? msg : "(null)");
#endif
        LOG_LUA("%s", lua_tostring(L, -1));
    }
    smlua_logline();
    smlua_dump_stack();
    return 0;
}

int smlua_pcall(lua_State* L, int nargs, int nresults, UNUSED int errfunc) {
    gSmLuaConvertSuccess = true;
    int errorHandlerIndex = lua_gettop(L) - nargs;
    lua_pushcfunction(L, smlua_error_handler);
    lua_insert(L, errorHandlerIndex);

    int rc = lua_pcall(L, nargs, nresults, errorHandlerIndex);

    lua_remove(L, errorHandlerIndex);
    return rc;
}

void smlua_exec_file(const char* path) {
    lua_State* L = gLuaState;
    if (luaL_dofile(L, path) != LUA_OK) {
        LOG_LUA("Failed to load lua file '%s'.", path);
        LOG_LUA("%s", smlua_to_string(L, lua_gettop(L)));
    }
    lua_pop(L, lua_gettop(L));
}

void smlua_exec_str(const char* str) {
    lua_State* L = gLuaState;
    if (luaL_dostring(L, str) != LUA_OK) {
        LOG_LUA("Failed to load lua string.");
        LOG_LUA("%s", smlua_to_string(L, lua_gettop(L)));
    }
    lua_pop(L, lua_gettop(L));
}

#if defined(TARGET_WII_U)
static bool smlua_lua_is_ident_char(char c) {
    return (c == '_') || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static void smlua_lua_update_depth(const char *wordStart, size_t wordLen, s32 *blockDepth, bool *skipThenDepth) {
    if (wordLen == 2 && !strncmp(wordStart, "do", wordLen)) {
        (*blockDepth)++;
    } else if (wordLen == 3 && !strncmp(wordStart, "end", wordLen)) {
        if (*blockDepth > 0) { (*blockDepth)--; }
    } else if (wordLen == 4 && !strncmp(wordStart, "then", wordLen)) {
        // elseif reuses the current if block, so its following then must not add depth.
        if (*skipThenDepth) {
            *skipThenDepth = false;
        } else {
            (*blockDepth)++;
        }
    } else if (wordLen == 5 && !strncmp(wordStart, "until", wordLen)) {
        if (*blockDepth > 0) { (*blockDepth)--; }
    } else if (wordLen == 6 && !strncmp(wordStart, "repeat", wordLen)) {
        (*blockDepth)++;
    } else if (wordLen == 6 && !strncmp(wordStart, "elseif", wordLen)) {
        *skipThenDepth = true;
    } else if (wordLen == 8 && !strncmp(wordStart, "function", wordLen)) {
        (*blockDepth)++;
    }
}

static void wiiu_probe_chunk22(lua_State *L, s32 afterChunk) {
    const char *source = "gGlobalSoundSource = create_read_only_table({ x = 0, y = 0, z = 0 })\n";
    int top = lua_gettop(L);
    size_t len = strlen(source);

    wiiu_diag_mark("probe22 after chunk %d load begin top=%d", afterChunk, top);
    int rc = luaL_loadbufferx(L, source, len, "probe_chunk22", "t");
    wiiu_diag_mark("probe22 after chunk %d load rc=%d top=%d", afterChunk, rc, lua_gettop(L));
    if (rc != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        wiiu_diag_mark("probe22 after chunk %d err=%s", afterChunk, err ? err : "(non-string)");
    }

    lua_settop(L, top);
}

static int smlua_exec_constants_chunk(const char *chunk, size_t chunkLen, s32 chunkIndex, s32 lineStart) {
    lua_State* L = gLuaState;
    int top = lua_gettop(L);
    char chunkName[64] = { 0 };
    snprintf(chunkName, sizeof(chunkName) - 1, "gSmluaConstants:%d", lineStart);
    u32 hash = 2166136261u;
    for (size_t i = 0; i < chunkLen; i++) {
        hash ^= (u8)chunk[i];
        hash *= 16777619u;
    }
    wiiu_diag_mark("constants chunk %d line=%d len=%u hash=%08X", chunkIndex, lineStart, (u32)chunkLen, hash);

    if (chunkIndex == 17) {
        const char *appPath = sys_user_path();
        if (appPath) {
            char dumpPath[SYS_MAX_PATH] = { 0 };
            snprintf(dumpPath, SYS_MAX_PATH, "%s/wiiu_constants_chunk17.lua", appPath);
            FILE *f = fopen(dumpPath, "wb");
            if (f) {
                fwrite(chunk, 1, chunkLen, f);
                fclose(f);
            }
        }
        wiiu_diag_mark("constants chunk 17 dumped len=%u hash=%08X", (u32)chunkLen, hash);
    }

    wiiu_diag_mark("constants chunk %d line=%d load begin len=%u first='%c%c%c%c'",
                   chunkIndex, lineStart, (u32)chunkLen,
                   chunkLen > 0 ? chunk[0] : ' ',
                   chunkLen > 1 ? chunk[1] : ' ',
                   chunkLen > 2 ? chunk[2] : ' ',
                   chunkLen > 3 ? chunk[3] : ' ');

    if (chunkIndex == 2) {
        wiiu_diag_mark("chunk2 special load begin top=%d", top);
        int rc = luaL_loadbufferx(L, chunk, chunkLen, "chunk2_special", "t");
        wiiu_diag_mark("chunk2 special load rc=%d top=%d", rc, lua_gettop(L));
        wiiu_probe_chunk22(L, 200);

        if (rc == LUA_OK) {
            wiiu_diag_mark("chunk2 special pcall begin top=%d", lua_gettop(L));
            rc = lua_pcall(L, 0, 0, 0);
            wiiu_diag_mark("chunk2 special pcall rc=%d top=%d", rc, lua_gettop(L));
        }

        if (rc != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            wiiu_diag_mark("chunk2 special err=%s", err ? err : "(non-string)");
        }

        wiiu_diag_mark("chunk2 gc count before=%d", lua_gc(L, LUA_GCCOUNT, 0));
        wiiu_diag_mark("chunk2 full gc begin");
        lua_gc(L, LUA_GCCOLLECT, 0);
        wiiu_diag_mark("chunk2 full gc end count=%d", lua_gc(L, LUA_GCCOUNT, 0));

        wiiu_probe_chunk22(L, 201);
        lua_settop(L, top);
        return rc;
    }

    sys_trace("smlua_init: constants chunk load begin %d line=%d len=%u top=%d", chunkIndex, lineStart, (u32)chunkLen, top);
    int rc = luaL_loadbufferx(L, chunk, chunkLen, chunkName, "t");
    wiiu_diag_mark("constants chunk %d line=%d load rc=%d top=%d", chunkIndex, lineStart, rc, lua_gettop(L));
    sys_trace("smlua_init: constants chunk load end %d rc=%d top=%d", chunkIndex, rc, lua_gettop(L));
    if (rc != LUA_OK) {
        LOG_LUA("Failed to load lua constants chunk %d at line %d.", chunkIndex, lineStart);
        LOG_LUA("%s", smlua_to_string(L, lua_gettop(L)));
        lua_settop(L, top);
        return rc;
    }

    wiiu_diag_mark("constants chunk %d line=%d pcall begin", chunkIndex, lineStart);
    sys_trace("smlua_init: constants chunk pcall begin %d line=%d", chunkIndex, lineStart);
    rc = lua_pcall(L, 0, LUA_MULTRET, 0);
    wiiu_diag_mark("constants chunk %d line=%d pcall rc=%d top=%d", chunkIndex, lineStart, rc, lua_gettop(L));
    sys_trace("smlua_init: constants chunk pcall end %d rc=%d top=%d", chunkIndex, rc, lua_gettop(L));
    if (rc != LUA_OK) {
        LOG_LUA("Failed to execute lua constants chunk %d at line %d.", chunkIndex, lineStart);
        LOG_LUA("%s", smlua_to_string(L, lua_gettop(L)));
    }

    lua_settop(L, top);
    if (rc == LUA_OK && chunkIndex <= 21) {
        wiiu_probe_chunk22(L, chunkIndex);
    }
    return rc;
}

static int smlua_wiiu_constants_smoke(const char *name, const char *source) {
    lua_State *L = gLuaState;
    int top = lua_gettop(L);
    size_t len = strlen(source);

    wiiu_diag_mark("constants smoke: %s load begin", name);
    int rc = luaL_loadbufferx(L, source, len, name, "t");
    wiiu_diag_mark("constants smoke: %s load rc=%d top=%d", name, rc, lua_gettop(L));
    if (rc == LUA_OK) {
        wiiu_diag_mark("constants smoke: %s pcall begin", name);
        rc = lua_pcall(L, 0, LUA_MULTRET, 0);
        wiiu_diag_mark("constants smoke: %s pcall rc=%d top=%d", name, rc, lua_gettop(L));
    }

    lua_settop(L, top);
    return rc;
}

static int smlua_wiiu_test_chunk(const char *name, const char *source, bool run, int nresults) {
    lua_State *L = gLuaState;
    int top = lua_gettop(L);
    size_t len = strlen(source);

    wiiu_diag_mark("%s load begin len=%u top=%d", name, (u32)len, top);
    int rc = luaL_loadbufferx(L, source, len, name, "t");
    wiiu_diag_mark("%s load rc=%d top=%d", name, rc, lua_gettop(L));

    if (rc == LUA_OK && run) {
        wiiu_diag_mark("%s pcall begin top=%d", name, lua_gettop(L));
        rc = lua_pcall(L, 0, nresults, 0);
        wiiu_diag_mark("%s pcall rc=%d top=%d", name, rc, lua_gettop(L));
    }

    if (rc != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        wiiu_diag_mark("%s err=%s", name, err ? err : "(non-string)");
    }

    lua_settop(L, top);
    wiiu_diag_mark("%s cleanup top=%d", name, lua_gettop(L));
    return rc;
}

static bool smlua_wiiu_constants_smoke_tests(void) {
    smlua_wiiu_test_chunk("bad_syntax_test", "function broken(", false, 0);
    smlua_wiiu_test_chunk("nested_function_test",
                          "function f(data)\n"
                          "    local mt = {\n"
                          "        __index = data,\n"
                          "        __call = function() return data end,\n"
                          "    }\n"
                          "    return mt\n"
                          "end\n",
                          false, 0);
    smlua_wiiu_test_chunk("exact_chunk22_pretest",
                          "gGlobalSoundSource = create_read_only_table({ x = 0, y = 0, z = 0 })\n",
                          false, 0);

    wiiu_diag_mark("constants smoke: all passed");
    return true;
}

static bool smlua_wiiu_line_starts_with(const char *line, size_t lineLen, const char *prefix) {
    size_t prefixLen = strlen(prefix);
    return lineLen >= prefixLen && memcmp(line, prefix, prefixLen) == 0;
}

static void smlua_wiiu_update_lua_depth_for_line(const char *line, size_t lineLen, s32 *depth, s32 *tableDepth, bool *skipThenDepth) {
    const char *p = line;
    const char *end = line + lineLen;

    while (p < end) {
        if ((end - p) >= 2 && p[0] == '-' && p[1] == '-') { return; }
        if (*p == '\'' || *p == '"') {
            char quote = *p++;
            while (p < end) {
                if (*p == '\\' && (p + 1) < end) {
                    p += 2;
                    continue;
                }
                if (*p++ == quote) { break; }
            }
            continue;
        }
        if (*p == '{') {
            (*tableDepth)++;
            p++;
            continue;
        }
        if (*p == '}') {
            if (*tableDepth > 0) { (*tableDepth)--; }
            p++;
            continue;
        }
        if (!smlua_lua_is_ident_char(*p)) {
            p++;
            continue;
        }

        const char *word = p;
        while (p < end && smlua_lua_is_ident_char(*p)) { p++; }
        smlua_lua_update_depth(word, (size_t)(p - word), depth, skipThenDepth);
    }
}

static int smlua_exec_constants_clean_chunk(const char *chunk, size_t chunkLen, s32 chunkIndex, s32 lineStart) {
    lua_State *L = gLuaState;
    int top = lua_gettop(L);
    char chunkName[64] = { 0 };
    snprintf(chunkName, sizeof(chunkName), "gSmluaConstants:%d", lineStart);

    wiiu_diag_mark("constants clean chunk %d line=%d load begin len=%u top=%d", chunkIndex, lineStart, (u32)chunkLen, top);
#if defined(TARGET_WII_U)
    wiiu_diag_mark("constants clean chunk %d line=%d malloc begin", chunkIndex, lineStart);
    char *chunkCopy = malloc(chunkLen + 1);
    if (chunkCopy == NULL) {
        wiiu_diag_mark("constants clean chunk %d line=%d malloc failed len=%u", chunkIndex, lineStart, (u32)chunkLen);
        return LUA_ERRMEM;
    }
    wiiu_diag_mark("constants clean chunk %d line=%d copy begin ptr=0x%08X", chunkIndex, lineStart, (u32)(uintptr_t)chunkCopy);
    memcpy(chunkCopy, chunk, chunkLen);
    chunkCopy[chunkLen] = '\0';
    wiiu_diag_mark("constants clean chunk %d line=%d lua load call begin", chunkIndex, lineStart);
    int rc = luaL_loadbufferx(L, chunkCopy, chunkLen, chunkName, "t");
    wiiu_diag_mark("constants clean chunk %d line=%d free begin", chunkIndex, lineStart);
    free(chunkCopy);
#else
    int rc = luaL_loadbufferx(L, chunk, chunkLen, chunkName, "t");
#endif
    wiiu_diag_mark("constants clean chunk %d line=%d load rc=%d top=%d", chunkIndex, lineStart, rc, lua_gettop(L));

    if (rc == LUA_OK) {
        wiiu_diag_mark("constants clean chunk %d line=%d pcall begin top=%d", chunkIndex, lineStart, lua_gettop(L));
        rc = lua_pcall(L, 0, 0, 0);
        wiiu_diag_mark("constants clean chunk %d line=%d pcall rc=%d top=%d", chunkIndex, lineStart, rc, lua_gettop(L));
        lua_gc(L, LUA_GCSTEP, 64);
    }

    if (rc != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        wiiu_diag_mark("constants clean chunk %d line=%d err=%s", chunkIndex, lineStart, err ? err : "(non-string)");
    }

    lua_settop(L, top);
    return rc;
}

static int smlua_exec_constants_clean_chunks(const char *str) {
    const size_t targetChunkSize = 8192;
    const char *chunkStart = str;
    const char *lineStart = str;
    s32 chunkLine = 1;
    s32 line = 1;
    s32 chunkIndex = 0;
    s32 depth = 0;
    s32 tableDepth = 0;
    bool skipThenDepth = false;

    while (*lineStart != '\0') {
        const char *lineEnd = strchr(lineStart, '\n');
        const char *nextLine = lineEnd ? lineEnd + 1 : lineStart + strlen(lineStart);
        size_t lineLen = (size_t)(nextLine - lineStart);

        smlua_wiiu_update_lua_depth_for_line(lineStart, lineLen, &depth, &tableDepth, &skipThenDepth);

        if (depth == 0 && tableDepth == 0 && (size_t)(nextLine - chunkStart) >= targetChunkSize) {
            int rc = smlua_exec_constants_clean_chunk(chunkStart, (size_t)(nextLine - chunkStart), chunkIndex++, chunkLine);
            if (rc != LUA_OK) { return rc; }
            chunkStart = nextLine;
            chunkLine = line + 1;
        }

        lineStart = nextLine;
        line++;
    }

    if (*chunkStart != '\0') {
        return smlua_exec_constants_clean_chunk(chunkStart, strlen(chunkStart), chunkIndex, chunkLine);
    }

    return LUA_OK;
}

typedef struct {
    const char *p;
    const char *end;
    lua_State *L;
    bool ok;
} SmluaConstExprParser;

typedef struct {
    bool isInteger;
    lua_Integer i;
    lua_Number n;
} SmluaConstValue;

static SmluaConstValue smlua_const_int(lua_Integer value) {
    SmluaConstValue v = { .isInteger = true, .i = value, .n = (lua_Number)value };
    return v;
}

static SmluaConstValue smlua_const_num(lua_Number value) {
    SmluaConstValue v = { .isInteger = false, .i = (lua_Integer)value, .n = value };
    return v;
}

static lua_Integer smlua_const_to_int(SmluaConstValue value) {
    return value.isInteger ? value.i : (lua_Integer)value.n;
}

static lua_Number smlua_const_to_num(SmluaConstValue value) {
    return value.isInteger ? (lua_Number)value.i : value.n;
}

static void smlua_const_skip_space(SmluaConstExprParser *parser) {
    while (parser->p < parser->end && isspace((unsigned char)*parser->p)) { parser->p++; }
}

static bool smlua_const_ident_start(char c) {
    return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool smlua_const_ident_char(char c) {
    return smlua_const_ident_start(c) || (c >= '0' && c <= '9') || c == '.';
}

static bool smlua_const_copy_name(char *dst, size_t dstSize, const char *start, size_t len) {
    if (len == 0 || len >= dstSize) { return false; }
    memcpy(dst, start, len);
    dst[len] = '\0';
    return true;
}

static void smlua_wiiu_raw_getglobal(lua_State *L, const char *name) {
    lua_pushglobaltable(L);
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    lua_remove(L, -2);
}

static void smlua_wiiu_raw_setglobal(lua_State *L, const char *name) {
    lua_pushglobaltable(L);
    lua_pushstring(L, name);
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 2);
}

static bool smlua_const_name_is_upper_constant(const char *start, size_t len) {
    bool hasUpper = false;
    for (size_t i = 0; i < len; i++) {
        char c = start[i];
        if (c >= 'A' && c <= 'Z') {
            hasUpper = true;
            continue;
        }
        if ((c >= '0' && c <= '9') || c == '_') { continue; }
        return false;
    }
    return hasUpper;
}

static bool smlua_wiiu_const_ensure_global(lua_State *L, const char *key, size_t keyLen);

static bool smlua_const_push_reference(lua_State *L, const char *start, size_t len) {
    char name[128];
    if (!smlua_const_copy_name(name, sizeof(name), start, len)) { return false; }

    char *dot = strchr(name, '.');
    if (dot == NULL) {
#if defined(TARGET_WII_U)
        if (!smlua_const_name_is_upper_constant(start, len)) {
            lua_getglobal(L, name);
            return true;
        }
#endif
        smlua_wiiu_raw_getglobal(L, name);
#if defined(TARGET_WII_U)
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            if (!smlua_wiiu_const_ensure_global(L, start, len)) { return false; }
            lua_getglobal(L, name);
        }
#endif
        return true;
    }

    *dot = '\0';
#if defined(TARGET_WII_U)
    if (smlua_const_name_is_upper_constant(name, strlen(name))) {
        smlua_wiiu_raw_getglobal(L, name);
    } else {
        lua_getglobal(L, name);
    }
#else
    smlua_wiiu_raw_getglobal(L, name);
#endif
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    lua_getfield(L, -1, dot + 1);
    lua_remove(L, -2);
    return true;
}

static bool smlua_const_get_reference_number(lua_State *L, const char *start, size_t len, SmluaConstValue *out) {
    if (!smlua_const_push_reference(L, start, len)) { return false; }

    int isInteger = 0;
    lua_Integer i = lua_tointegerx(L, -1, &isInteger);
    if (isInteger) {
        *out = smlua_const_int(i);
        lua_pop(L, 1);
        return true;
    }

    int isNumber = 0;
    lua_Number n = lua_tonumberx(L, -1, &isNumber);
    lua_pop(L, 1);
    if (!isNumber) { return false; }

    *out = smlua_const_num(n);
    return true;
}

static bool smlua_const_set_lhs(lua_State *L, const char *start, size_t len) {
    char name[128];
    if (!smlua_const_copy_name(name, sizeof(name), start, len)) { return false; }

    char *dot = strchr(name, '.');
    if (dot == NULL) {
#if defined(TARGET_WII_U)
        smlua_wiiu_raw_setglobal(L, name);
#else
        lua_setglobal(L, name);
#endif
        return true;
    }

    *dot = '\0';
    lua_getglobal(L, name);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_insert(L, -2);
    lua_setfield(L, -2, dot + 1);
    lua_pop(L, 1);
    return true;
}

static bool smlua_const_parse_expr(SmluaConstExprParser *parser, SmluaConstValue *out);

static bool smlua_const_parse_primary(SmluaConstExprParser *parser, SmluaConstValue *out) {
    smlua_const_skip_space(parser);
    if (parser->p >= parser->end) { return false; }

    if (*parser->p == '(') {
        parser->p++;
        if (!smlua_const_parse_expr(parser, out)) { return false; }
        smlua_const_skip_space(parser);
        if (parser->p >= parser->end || *parser->p != ')') { return false; }
        parser->p++;
        return true;
    }

    if (smlua_const_ident_start(*parser->p)) {
        const char *nameStart = parser->p;
        while (parser->p < parser->end && smlua_const_ident_char(*parser->p)) { parser->p++; }
        const char *nameEnd = parser->p;

        smlua_const_skip_space(parser);
        if (parser->p < parser->end && *parser->p == '(') {
            parser->p++;
            SmluaConstValue args[4];
            for (int i = 0; i < 4; i++) {
                if (!smlua_const_parse_expr(parser, &args[i])) { return false; }
                smlua_const_skip_space(parser);
                if (i < 3) {
                    if (parser->p >= parser->end || *parser->p != ',') { return false; }
                    parser->p++;
                }
            }
            smlua_const_skip_space(parser);
            if (parser->p >= parser->end || *parser->p != ')') { return false; }
            parser->p++;

            if ((size_t)(nameEnd - nameStart) == strlen("SOUND_ARG_LOAD")
                && memcmp(nameStart, "SOUND_ARG_LOAD", strlen("SOUND_ARG_LOAD")) == 0) {
                lua_Integer bank = smlua_const_to_int(args[0]);
                lua_Integer soundId = smlua_const_to_int(args[1]);
                lua_Integer priority = smlua_const_to_int(args[2]);
                lua_Integer flags = smlua_const_to_int(args[3]);
                lua_Integer result =
                    ((bank << SOUNDARGS_SHIFT_BANK) & SOUNDARGS_MASK_BANK) |
                    ((soundId << SOUNDARGS_SHIFT_SOUNDID) & SOUNDARGS_MASK_SOUNDID) |
                    ((priority << SOUNDARGS_SHIFT_PRIORITY) & SOUNDARGS_MASK_PRIORITY) |
                    (flags & SOUNDARGS_MASK_BITFLAGS) |
                    SOUND_STATUS_WAITING;
                *out = smlua_const_int(result);
                return true;
            }
            return false;
        }

        return smlua_const_get_reference_number(parser->L, nameStart, (size_t)(nameEnd - nameStart), out);
    }

    if (*parser->p == '+' || *parser->p == '-' || *parser->p == '.' || isdigit((unsigned char)*parser->p)) {
        char *end = NULL;
        bool hasDot = false;
        const char *scan = parser->p;
        while (scan < parser->end && (isalnum((unsigned char)*scan) || *scan == '.' || *scan == '+' || *scan == '-')) {
            if (*scan == '.') { hasDot = true; }
            scan++;
        }

        if (hasDot) {
            lua_Number n = strtod(parser->p, &end);
            if (end == parser->p || end > parser->end) { return false; }
            parser->p = end;
            *out = smlua_const_num(n);
            return true;
        }

        lua_Integer i = (lua_Integer)strtoll(parser->p, &end, 0);
        if (end == parser->p || end > parser->end) { return false; }
        parser->p = end;
        *out = smlua_const_int(i);
        return true;
    }

    return false;
}

static bool smlua_const_parse_unary(SmluaConstExprParser *parser, SmluaConstValue *out) {
    smlua_const_skip_space(parser);
    if (parser->p < parser->end && (*parser->p == '-' || *parser->p == '~')) {
        char op = *parser->p++;
        SmluaConstValue rhs;
        if (!smlua_const_parse_unary(parser, &rhs)) { return false; }
        *out = (op == '-') ? smlua_const_int(-smlua_const_to_int(rhs)) : smlua_const_int(~smlua_const_to_int(rhs));
        return true;
    }
    return smlua_const_parse_primary(parser, out);
}

static bool smlua_const_parse_mul(SmluaConstExprParser *parser, SmluaConstValue *out) {
    if (!smlua_const_parse_unary(parser, out)) { return false; }
    for (;;) {
        smlua_const_skip_space(parser);
        if (parser->p >= parser->end || (*parser->p != '*' && *parser->p != '/')) { return true; }
        char op = *parser->p++;
        SmluaConstValue rhs;
        if (!smlua_const_parse_unary(parser, &rhs)) { return false; }
        if (op == '*') {
            if (out->isInteger && rhs.isInteger) {
                *out = smlua_const_int(out->i * rhs.i);
            } else {
                *out = smlua_const_num(smlua_const_to_num(*out) * smlua_const_to_num(rhs));
            }
        } else {
            *out = smlua_const_num(smlua_const_to_num(*out) / smlua_const_to_num(rhs));
        }
    }
}

static bool smlua_const_parse_add(SmluaConstExprParser *parser, SmluaConstValue *out) {
    if (!smlua_const_parse_mul(parser, out)) { return false; }
    for (;;) {
        smlua_const_skip_space(parser);
        if (parser->p >= parser->end || (*parser->p != '+' && *parser->p != '-')) { return true; }
        char op = *parser->p++;
        SmluaConstValue rhs;
        if (!smlua_const_parse_mul(parser, &rhs)) { return false; }
        if (out->isInteger && rhs.isInteger) {
            *out = smlua_const_int(op == '+' ? out->i + rhs.i : out->i - rhs.i);
        } else {
            *out = smlua_const_num(op == '+' ? smlua_const_to_num(*out) + smlua_const_to_num(rhs)
                                             : smlua_const_to_num(*out) - smlua_const_to_num(rhs));
        }
    }
}

static bool smlua_const_parse_shift(SmluaConstExprParser *parser, SmluaConstValue *out) {
    if (!smlua_const_parse_add(parser, out)) { return false; }
    for (;;) {
        smlua_const_skip_space(parser);
        if ((parser->end - parser->p) < 2 || (parser->p[0] != '<' && parser->p[0] != '>') || parser->p[1] != parser->p[0]) { return true; }
        bool left = parser->p[0] == '<';
        parser->p += 2;
        SmluaConstValue rhs;
        if (!smlua_const_parse_add(parser, &rhs)) { return false; }
        *out = smlua_const_int(left ? (smlua_const_to_int(*out) << smlua_const_to_int(rhs))
                                    : (smlua_const_to_int(*out) >> smlua_const_to_int(rhs)));
    }
}

static bool smlua_const_parse_and(SmluaConstExprParser *parser, SmluaConstValue *out) {
    if (!smlua_const_parse_shift(parser, out)) { return false; }
    for (;;) {
        smlua_const_skip_space(parser);
        if (parser->p >= parser->end || *parser->p != '&') { return true; }
        parser->p++;
        SmluaConstValue rhs;
        if (!smlua_const_parse_shift(parser, &rhs)) { return false; }
        *out = smlua_const_int(smlua_const_to_int(*out) & smlua_const_to_int(rhs));
    }
}

static bool smlua_const_parse_xor(SmluaConstExprParser *parser, SmluaConstValue *out) {
    if (!smlua_const_parse_and(parser, out)) { return false; }
    for (;;) {
        smlua_const_skip_space(parser);
        if (parser->p >= parser->end || *parser->p != '~') { return true; }
        parser->p++;
        SmluaConstValue rhs;
        if (!smlua_const_parse_and(parser, &rhs)) { return false; }
        *out = smlua_const_int(smlua_const_to_int(*out) ^ smlua_const_to_int(rhs));
    }
}

static bool smlua_const_parse_or(SmluaConstExprParser *parser, SmluaConstValue *out) {
    if (!smlua_const_parse_xor(parser, out)) { return false; }
    for (;;) {
        smlua_const_skip_space(parser);
        if (parser->p >= parser->end || *parser->p != '|') { return true; }
        parser->p++;
        SmluaConstValue rhs;
        if (!smlua_const_parse_xor(parser, &rhs)) { return false; }
        *out = smlua_const_int(smlua_const_to_int(*out) | smlua_const_to_int(rhs));
    }
}

static bool smlua_const_parse_expr(SmluaConstExprParser *parser, SmluaConstValue *out) {
    return smlua_const_parse_or(parser, out);
}

static bool smlua_const_push_string(lua_State *L, const char *start, const char *end) {
    if (end <= start || (*start != '\'' && *start != '"') || end[-1] != *start) { return false; }
    lua_pushlstring(L, start + 1, (size_t)(end - start - 2));
    return true;
}

static bool smlua_const_exec_assignment(lua_State *L, const char *lineStart, const char *lineEnd, s32 lineNumber) {
    const char *line = lineStart;
    while (line < lineEnd && isspace((unsigned char)*line)) { line++; }
    while (lineEnd > line && isspace((unsigned char)lineEnd[-1])) { lineEnd--; }
    if (line == lineEnd || (lineEnd - line >= 2 && line[0] == '-' && line[1] == '-')) { return true; }

    const char *eq = memchr(line, '=', (size_t)(lineEnd - line));
    if (eq == NULL) {
        wiiu_diag_mark("constants cbind line %d missing equals", lineNumber);
        return false;
    }

    const char *lhsStart = line;
    const char *lhsEnd = eq;
    while (lhsEnd > lhsStart && isspace((unsigned char)lhsEnd[-1])) { lhsEnd--; }
    const char *rhsStart = eq + 1;
    while (rhsStart < lineEnd && isspace((unsigned char)*rhsStart)) { rhsStart++; }

#if defined(TARGET_WII_U)
    size_t lhsLen = (size_t)(lhsEnd - lhsStart);
    if ((lhsLen == strlen("table.copy") && memcmp(lhsStart, "table.copy", lhsLen) == 0)
        || (lhsLen == strlen("table.deepcopy") && memcmp(lhsStart, "table.deepcopy", lhsLen) == 0)) {
        wiiu_diag_mark("constants cbind line %d skip %.*s on Wii U", lineNumber, (int)lhsLen, lhsStart);
        return true;
    }
#endif

#if defined(TARGET_WII_U)
    if (lineNumber <= 16 || (lineNumber % 256) == 0) {
        int lhsDiagLen = (int)(lhsEnd - lhsStart);
        if (lhsDiagLen > 96) { lhsDiagLen = 96; }
        wiiu_diag_mark("constants cbind progress line %d lhs %.*s top=%d", lineNumber, lhsDiagLen, lhsStart, lua_gettop(L));
    }
#endif

    bool pushed = false;
    if ((rhsStart < lineEnd && (*rhsStart == '\'' || *rhsStart == '"'))) {
        pushed = smlua_const_push_string(L, rhsStart, lineEnd);
    } else if (rhsStart < lineEnd && smlua_const_ident_start(*rhsStart)) {
        const char *scan = rhsStart;
        while (scan < lineEnd && smlua_const_ident_char(*scan)) { scan++; }
        const char *after = scan;
        while (after < lineEnd && isspace((unsigned char)*after)) { after++; }
        if (after == lineEnd) {
            pushed = smlua_const_push_reference(L, rhsStart, (size_t)(scan - rhsStart));
        }
    }

    if (!pushed) {
        SmluaConstExprParser parser = { .p = rhsStart, .end = lineEnd, .L = L, .ok = true };
        SmluaConstValue value;
        if (!smlua_const_parse_expr(&parser, &value)) {
            wiiu_diag_mark("constants cbind line %d parse failed", lineNumber);
            return false;
        }
        smlua_const_skip_space(&parser);
        if (parser.p != parser.end) {
            wiiu_diag_mark("constants cbind line %d trailing parse data", lineNumber);
            return false;
        }
        if (value.isInteger) {
            lua_pushinteger(L, value.i);
        } else {
            lua_pushnumber(L, value.n);
        }
    }

    if (!smlua_const_set_lhs(L, lhsStart, (size_t)(lhsEnd - lhsStart))) {
        lua_pop(L, 1);
        wiiu_diag_mark("constants cbind line %d set failed", lineNumber);
        return false;
    }
    return true;
}

static int smlua_exec_constants_cbind(const char *str) {
    lua_State *L = gLuaState;
    const char *line = str;
    s32 lineNumber = 1;
    s32 assignCount = 0;

    wiiu_diag_mark("constants cbind begin");
    while (*line != '\0') {
        const char *lineEnd = strchr(line, '\n');
        if (lineEnd == NULL) { lineEnd = line + strlen(line); }
        if (!smlua_const_exec_assignment(L, line, lineEnd, lineNumber)) {
            wiiu_diag_mark("constants cbind failed line=%d assignments=%d", lineNumber, assignCount);
            return LUA_ERRRUN;
        }

        const char *trim = line;
        while (trim < lineEnd && isspace((unsigned char)*trim)) { trim++; }
        if (trim < lineEnd && !(lineEnd - trim >= 2 && trim[0] == '-' && trim[1] == '-')) { assignCount++; }

        if (*lineEnd == '\0') { break; }
        line = lineEnd + 1;
        lineNumber++;
    }

    wiiu_diag_mark("constants cbind end assignments=%d", assignCount);
    return LUA_OK;
}

static bool smlua_const_push_assignment_value(lua_State *L, const char *lineStart, const char *lineEnd, const char *key, size_t keyLen) {
    const char *line = lineStart;
    while (line < lineEnd && isspace((unsigned char)*line)) { line++; }
    while (lineEnd > line && isspace((unsigned char)lineEnd[-1])) { lineEnd--; }
    if (line == lineEnd || (lineEnd - line >= 2 && line[0] == '-' && line[1] == '-')) { return false; }

    const char *eq = memchr(line, '=', (size_t)(lineEnd - line));
    if (eq == NULL) { return false; }

    const char *lhsStart = line;
    const char *lhsEnd = eq;
    while (lhsEnd > lhsStart && isspace((unsigned char)lhsEnd[-1])) { lhsEnd--; }
    if ((size_t)(lhsEnd - lhsStart) != keyLen || memcmp(lhsStart, key, keyLen) != 0) { return false; }

    const char *rhsStart = eq + 1;
    while (rhsStart < lineEnd && isspace((unsigned char)*rhsStart)) { rhsStart++; }

    if ((rhsStart < lineEnd && (*rhsStart == '\'' || *rhsStart == '"'))) {
        return smlua_const_push_string(L, rhsStart, lineEnd);
    }

    if (rhsStart < lineEnd && smlua_const_ident_start(*rhsStart)) {
        const char *scan = rhsStart;
        while (scan < lineEnd && smlua_const_ident_char(*scan)) { scan++; }
        const char *after = scan;
        while (after < lineEnd && isspace((unsigned char)*after)) { after++; }
        if (after == lineEnd) {
            return smlua_const_push_reference(L, rhsStart, (size_t)(scan - rhsStart));
        }
    }

    SmluaConstExprParser parser = { .p = rhsStart, .end = lineEnd, .L = L, .ok = true };
    SmluaConstValue value;
    if (!smlua_const_parse_expr(&parser, &value)) { return false; }
    smlua_const_skip_space(&parser);
    if (parser.p != parser.end) { return false; }

    if (value.isInteger) {
        lua_pushinteger(L, value.i);
    } else {
        lua_pushnumber(L, value.n);
    }
    return true;
}

static bool smlua_wiiu_const_find_assignment(const char *key, size_t keyLen, const char **outStart, const char **outEnd) {
    extern char gSmluaConstants[];
    const char *line = gSmluaConstants;
    while (*line != '\0') {
        const char *lineEnd = strchr(line, '\n');
        if (lineEnd == NULL) { lineEnd = line + strlen(line); }
        const char *scan = line;
        while (scan < lineEnd && isspace((unsigned char)*scan)) { scan++; }
        if ((size_t)(lineEnd - scan) > keyLen && memcmp(scan, key, keyLen) == 0) {
            const char *after = scan + keyLen;
            while (after < lineEnd && isspace((unsigned char)*after)) { after++; }
            if (after < lineEnd && *after == '=') {
                *outStart = line;
                *outEnd = lineEnd;
                return true;
            }
        }
        if (*lineEnd == '\0') { break; }
        line = lineEnd + 1;
    }
    return false;
}

static bool smlua_wiiu_const_ensure_global(lua_State *L, const char *key, size_t keyLen) {
    if (keyLen == 0 || keyLen >= 128 || memchr(key, '.', keyLen) != NULL) { return false; }

    char name[128];
    if (!smlua_const_copy_name(name, sizeof(name), key, keyLen)) { return false; }
    smlua_wiiu_raw_getglobal(L, name);
    if (!lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 1);

    const char *lineStart = NULL;
    const char *lineEnd = NULL;
    if (!smlua_wiiu_const_find_assignment(key, keyLen, &lineStart, &lineEnd)) { return false; }
    if (!smlua_const_push_assignment_value(L, lineStart, lineEnd, key, keyLen)) { return false; }
    smlua_wiiu_raw_setglobal(L, name);
    return true;
}

static bool smlua_wiiu_const_can_lazy_lookup(const char *name, size_t nameLen) {
    if (nameLen == 0 || nameLen >= 128 || memchr(name, '.', nameLen) != NULL) { return false; }
    if (name[0] == 'g') { return false; }
    if (nameLen >= 3 && memcmp(name, "id_", 3) == 0) { return true; }
    return smlua_const_name_is_upper_constant(name, nameLen);
}

bool smlua_wiiu_bind_constant_global_if_exists(lua_State *L, const char *name) {
    if (name == NULL) { return false; }
    size_t nameLen = strlen(name);
    if (!smlua_wiiu_const_can_lazy_lookup(name, nameLen)) { return false; }
    return smlua_wiiu_const_ensure_global(L, name, nameLen);
}

static bool smlua_wiiu_const_should_prebind(const char *name, size_t nameLen) {
    if (nameLen == 0 || nameLen >= 128 || memchr(name, '.', nameLen) != NULL) { return false; }
    if (name[0] == 'g') { return false; }

    bool hasUpper = false;
    for (size_t i = 0; i < nameLen; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c >= 'A' && c <= 'Z') {
            hasUpper = true;
            continue;
        }
        if ((c >= '0' && c <= '9') || c == '_') { continue; }
        return false;
    }
    return hasUpper;
}

static void smlua_wiiu_bind_script_constants(lua_State *L, const char *buffer, size_t length, const char *path) {
    int top = lua_gettop(L);
    s32 bound = 0;
    const char *p = buffer;
    const char *end = buffer + length;

    wiiu_diag_mark("smlua_load_script: bind constants begin %s", path);
    while (p < end) {
        if ((*p == '\'' || *p == '"')) {
            char quote = *p++;
            while (p < end) {
                if (*p == '\\' && (p + 1) < end) {
                    p += 2;
                } else if (*p++ == quote) {
                    break;
                }
            }
            continue;
        }
        if ((end - p) >= 2 && p[0] == '-' && p[1] == '-') {
            p += 2;
            while (p < end && *p != '\n') { p++; }
            continue;
        }
        if (!smlua_const_ident_start(*p)) {
            p++;
            continue;
        }
        const char *name = p;
        while (p < end && smlua_const_ident_char(*p)) { p++; }
        size_t nameLen = (size_t)(p - name);
        if (smlua_wiiu_const_should_prebind(name, nameLen)) {
            if (smlua_wiiu_const_ensure_global(L, name, nameLen)) { bound++; }
        }
    }
    lua_settop(L, top);
    wiiu_diag_mark("smlua_load_script: bind constants end count=%d %s", bound, path);
}

static int smlua_exec_constants_lazy(const char *str) {
    const char *line = str;
    s32 lineNumber = 1;
    s32 eagerCount = 0;
    s32 skippedCount = 0;

    wiiu_diag_mark("constants lazy globals begin");
    while (*line != '\0') {
        const char *lineEnd = strchr(line, '\n');
        if (lineEnd == NULL) { lineEnd = line + strlen(line); }

        const char *scan = line;
        while (scan < lineEnd && isspace((unsigned char)*scan)) { scan++; }
        const char *trimmedEnd = lineEnd;
        while (trimmedEnd > scan && isspace((unsigned char)trimmedEnd[-1])) { trimmedEnd--; }

        bool skipLine = true;
        if (scan != trimmedEnd && !(trimmedEnd - scan >= 2 && scan[0] == '-' && scan[1] == '-')) {
            const char *eq = memchr(scan, '=', (size_t)(trimmedEnd - scan));
            if (eq != NULL) {
                const char *lhsEnd = eq;
                while (lhsEnd > scan && isspace((unsigned char)lhsEnd[-1])) { lhsEnd--; }
                size_t lhsLen = (size_t)(lhsEnd - scan);
                skipLine = smlua_wiiu_const_can_lazy_lookup(scan, lhsLen) || (lhsLen > 0 && scan[0] == 'g');
            }
        }

        if (skipLine) {
            skippedCount++;
        } else {
            if (!smlua_const_exec_assignment(gLuaState, line, lineEnd, lineNumber)) {
                wiiu_diag_mark("constants lazy globals failed line=%d eager=%d skipped=%d", lineNumber, eagerCount, skippedCount);
                return LUA_ERRRUN;
            }
            eagerCount++;
        }

        if (*lineEnd == '\0') { break; }
        line = lineEnd + 1;
        lineNumber++;
    }

    wiiu_diag_mark("constants lazy globals end eager=%d skipped=%d", eagerCount, skippedCount);
    return LUA_OK;
}
#endif

static int smlua_exec_constants(const char *str) {
#if defined(TARGET_WII_U)
    return smlua_exec_constants_lazy(str);
#else
    smlua_exec_str(str);
    return LUA_OK;
#endif
}

#define LUA_BOM_11 0x0000000000005678llu
#define LUA_BOM_19 0x4077280000000000llu

static bool smlua_check_binary_header(struct ModFile *file) {
    FILE *f = f_open_r(file->cachedPath);
    if (f) {

        // Read signature
        char signature[sizeof(LUA_SIGNATURE)] = { 0 };
        if (f_read(signature, 1, sizeof(LUA_SIGNATURE) - 1, f) != sizeof(LUA_SIGNATURE) - 1) {
            LOG_LUA("Failed to load lua script '%s': File too short.", file->cachedPath);
            f_close(f);
            f_delete(f);
            return false;
        }

        // Check signature
        if (strcmp(signature, LUA_SIGNATURE) != 0) {
            f_close(f);
            return true; // Not a binary lua
        }

#if defined(TARGET_WII_U)
        LOG_LUA("Rejected binary Lua chunk on Wii U: '%s'", file->cachedPath);
        f_close(f);
        f_delete(f);
        return false;
#endif

        // Read version number
        u8 version;
        if (f_read(&version, 1, 1, f) != 1) {
            LOG_LUA("Failed to load lua script '%s': File too short.", file->cachedPath);
            f_close(f);
            f_delete(f);
            return false;
        }

        // Check version number
        u8 expectedVersion = strtoul(LUA_VERSION_MAJOR LUA_VERSION_MINOR, NULL, 16);
        if (version != expectedVersion) {
            LOG_LUA("Failed to load lua script '%s': Lua versions don't match (%X, expected %X).", file->cachedPath, version, expectedVersion);
            f_close(f);
            f_delete(f);
            return false;
        }

        // Read the rest of the header
        u8 header[28];
        if (f_read(header, 1, 28, f) != 28) {
            LOG_LUA("Failed to load lua script '%s': File too short.", file->cachedPath);
            f_close(f);
            f_delete(f);
            return false;
        }

        // The following errors are silent (they're due to non-matching endianness/bitness and shouldn't prevent the rest of the mod from loading)

        // Check endianness
        u64 bom11 = 0;
        u64 bom19 = 0;
        memcpy(&bom11, header + 12, sizeof(bom11));
        memcpy(&bom19, header + 20, sizeof(bom19));
        if (bom11 != LUA_BOM_11) {
            LOG_ERROR("Failed to load lua script '%s': BOM at offset 0x11 don't match (%016llX, expected %016llX).", file->cachedPath, bom11, LUA_BOM_11);
            f_close(f);
            f_delete(f);
            return false;
        }
        if (bom19 != LUA_BOM_19) {
            LOG_ERROR("Failed to load lua script '%s': BOM at offset 0x19 don't match (%016llX, expected %016llX).", file->cachedPath, bom19, LUA_BOM_19);
            f_close(f);
            f_delete(f);
            return false;
        }

        // Check sizes
        u8 sizeOfCInteger = header[7];
        u8 sizeOfCPointer = header[8];
        u8 sizeOfCFloat = header[9];
        u8 sizeOfLuaInteger = header[10];
        u8 sizeOfLuaNumber = header[11];
        if (sizeOfCInteger != sizeof(int)) {
            LOG_ERROR("Failed to load lua script '%s': sizes of C Integer don't match (%d, expected %llu).", file->cachedPath, sizeOfCInteger, (long long unsigned)sizeof(int));
            f_close(f);
            f_delete(f);
            return false;
        }
        if (sizeOfCPointer != sizeof(void *)) { // 4 for 32-bit architectures, 8 for 64-bit
            LOG_ERROR("Failed to load lua script '%s': sizes of C Pointer don't match (%d, expected %llu).", file->cachedPath, sizeOfCPointer, (long long unsigned)sizeof(void *));
            f_close(f);
            f_delete(f);
            return false;
        }
        if (sizeOfCFloat != sizeof(float)) {
            LOG_ERROR("Failed to load lua script '%s': sizes of C Float don't match (%d, expected %llu).", file->cachedPath, sizeOfCFloat, (long long unsigned)sizeof(float));
            f_close(f);
            f_delete(f);
            return false;
        }
        if (sizeOfLuaInteger != sizeof(LUA_INTEGER)) {
            LOG_ERROR("Failed to load lua script '%s': sizes of Lua Integer don't match (%d, expected %llu).", file->cachedPath, sizeOfLuaInteger, (long long unsigned)sizeof(LUA_INTEGER));
            f_close(f);
            f_delete(f);
            return false;
        }
        if (sizeOfLuaNumber != sizeof(LUA_NUMBER)) {
            LOG_ERROR("Failed to load lua script '%s': sizes of Lua Number don't match (%d, expected %llu).", file->cachedPath, sizeOfLuaNumber, (long long unsigned)sizeof(LUA_NUMBER));
            f_close(f);
            f_delete(f);
            return false;
        }

        // All's good
        f_close(f);
        return true;
    }
    LOG_LUA("Failed to load lua script '%s': File not found.", file->cachedPath);
    return false;
}

#if defined(TARGET_WII_U)
#define SMLUA_WIIU_SCRIPT_BUFFER_SIZE (256 * 1024)
static char sSmluaWiiuScriptBuffer[SMLUA_WIIU_SCRIPT_BUFFER_SIZE] __attribute__((aligned(64)));

struct SmluaWiiuScriptReader {
    const char* buffer;
    size_t length;
    size_t offset;
    size_t chunkSize;
    u32 chunks;
};

static const char* smlua_wiiu_script_reader(lua_State* L, void* data, size_t* size) {
    (void)L;
    struct SmluaWiiuScriptReader* reader = (struct SmluaWiiuScriptReader*)data;
    if (reader->offset >= reader->length) {
        *size = 0;
        return NULL;
    }

    size_t remaining = reader->length - reader->offset;
    size_t chunkSize = remaining < reader->chunkSize ? remaining : reader->chunkSize;
    const char *chunk = reader->buffer + reader->offset;
    reader->offset += chunkSize;
    reader->chunks++;
    *size = chunkSize;
    return chunk;
}

#if defined(WIIU_LUA_TRACE_LINES)
static void smlua_wiiu_line_trace_hook(lua_State* L, lua_Debug* ar) {
    lua_getinfo(L, "Sl", ar);
    wiiu_diag_mark("smlua_line: %s:%d top=%d", ar->short_src, ar->currentline, lua_gettop(L));
}
#endif

#endif

int smlua_load_script(struct Mod* mod, struct ModFile* file, u16 remoteIndex, bool isModInit) {
    int rc = LUA_OK;
    wiiu_diag_mark("smlua_load_script: begin %s/%s init=%d", mod ? mod->relativePath : "<null>", file ? file->relativePath : "<null>", isModInit);
    if (!smlua_check_binary_header(file)) { return LUA_ERRMEM; }

    lua_State* L = gLuaState;

    s32 prevTop = lua_gettop(L);

    gSmLuaConvertSuccess = true;
    gLuaInitializingScript = 1;
    LOG_INFO("Loading lua script '%s'", file->cachedPath);

    wiiu_diag_mark("smlua_load_script: open begin %s", file->cachedPath);
    FILE *f = f_open_r(file->cachedPath);
    if (!f) {
        LOG_LUA("Failed to load lua script '%s': File not found.", file->cachedPath);
        gLuaInitializingScript = 0;
        lua_settop(L, prevTop);
        return LUA_ERRFILE;
    }
    wiiu_diag_mark("smlua_load_script: open end %s", file->cachedPath);

    f_seek(f, 0, SEEK_END);
    size_t length = f_tell(f);
    wiiu_diag_mark("smlua_load_script: size=%u %s", (u32)length, file->cachedPath);
#if defined(TARGET_WII_U)
    bool bufferIsStatic = (length + 1) <= SMLUA_WIIU_SCRIPT_BUFFER_SIZE;
    void *buffer = bufferIsStatic ? (void*)sSmluaWiiuScriptBuffer : calloc(length + 1, 1);
    if (bufferIsStatic) {
        memset(buffer, 0, length + 1);
        wiiu_diag_mark("smlua_load_script: buffer static len=%u cap=%u ptr=0x%08X %s",
                       (u32)length, (u32)SMLUA_WIIU_SCRIPT_BUFFER_SIZE, (u32)(uintptr_t)buffer, file->cachedPath);
    }
#else
    void *buffer = calloc(length + 1, 1);
#endif
    if (!buffer) {
        LOG_LUA("Failed to load lua script '%s': Cannot allocate buffer.", file->cachedPath);
        gLuaInitializingScript = 0;
        lua_settop(L, prevTop);
        return LUA_ERRMEM;
    }

    f_rewind(f);
    if (f_read(buffer, 1, length, f) < length) {
        LOG_LUA("Failed to load lua script '%s': Unexpected early end of file.", file->cachedPath);
        gLuaInitializingScript = 0;
        lua_settop(L, prevTop);
        return LUA_ERRFILE;
    }
    f_close(f);
    f_delete(f);

#if defined(TARGET_WII_U) && defined(WIIU_LUA_PREBIND_SCRIPT_CONSTANTS)
    smlua_wiiu_bind_script_constants(L, (const char*)buffer, length, file->cachedPath);
#endif

#if defined(TARGET_WII_U)
    char chunkName[128] = { 0 };
    snprintf(chunkName, sizeof(chunkName), "@%s", file->relativePath);
    struct SmluaWiiuScriptReader reader = {
        .buffer = (const char*)buffer,
        .length = length,
        .offset = 0,
        .chunkSize = 512,
        .chunks = 0,
    };
    wiiu_diag_mark("smlua_load_script: gc before lua_load count=%d %s", lua_gc(L, LUA_GCCOUNT, 0), file->cachedPath);
    wiiu_diag_mark("smlua_load_script: lua_load stream begin len=%u top=%d chunk=%s path=%s", (u32)length, lua_gettop(L), chunkName, file->cachedPath);
    rc = lua_load(L, smlua_wiiu_script_reader, &reader, chunkName, "t");
    wiiu_diag_mark("smlua_load_script: lua_load stream rc=%d top=%d chunks=%u offset=%u %s", rc, lua_gettop(L), reader.chunks, (u32)reader.offset, file->cachedPath);
#else
    wiiu_diag_mark("smlua_load_script: luaL_loadbufferx begin len=%u top=%d %s", (u32)length, lua_gettop(L), file->cachedPath);
    rc = luaL_loadbufferx(L, (const char*)buffer, length, file->cachedPath,
                          NULL
    );
    wiiu_diag_mark("smlua_load_script: luaL_loadbufferx rc=%d top=%d %s", rc, lua_gettop(L), file->cachedPath);
#endif
    if (rc != LUA_OK) { // only run on success
        LOG_LUA("Failed to load lua script '%s'.", file->cachedPath);
        LOG_LUA("%s", smlua_to_string(L, lua_gettop(L)));
        gLuaInitializingScript = 0;
#if defined(TARGET_WII_U)
        if (!bufferIsStatic) { free(buffer); }
#else
        free(buffer);
#endif
        lua_settop(L, prevTop);
        return rc;
    }
#if defined(TARGET_WII_U)
    if (!bufferIsStatic) { free(buffer); }
#else
    free(buffer);
#endif

    if (!lua_isfunction(L, -1)) {
        LOG_LUA("Expected loaded Lua chunk function on stack, got %s", lua_typename(L, lua_type(L, -1)));
        gLuaInitializingScript = 0;
        lua_settop(L, prevTop);
        return LUA_ERRRUN;
    }
    int chunkIndex = lua_gettop(L);

#if defined(TARGET_WII_U)
    (void)chunkIndex;
    wiiu_diag_mark("smlua_load_script: wiiu global env begin init=%d top=%d %s", isModInit, lua_gettop(L), file->cachedPath);
    if (isModInit) {
        wiiu_diag_mark("smlua_load_script: wiiu direct global per-file begin %s", file->cachedPath);
        smlua_sync_table_init_global_globals(remoteIndex);
        smlua_cobject_init_global_globals();
        wiiu_diag_mark("smlua_load_script: wiiu direct global per-file end %s", file->cachedPath);
    }
    wiiu_diag_mark("smlua_load_script: wiiu global env end top=%d %s", lua_gettop(L), file->cachedPath);
#else
    if (isModInit) {
        wiiu_diag_mark("smlua_load_script: env begin %s", file->cachedPath);
        // check if this is the first time this mod has been loaded
        lua_getfield(L, LUA_REGISTRYINDEX, mod->relativePath);
        bool firstInit = (lua_type(L, -1) == LUA_TNIL);
        lua_pop(L, 1);
        wiiu_diag_mark("smlua_load_script: firstInit=%d %s", firstInit, file->cachedPath);

        // create mod's "global" table
        if (firstInit) {
            wiiu_diag_mark("smlua_load_script: env newtable begin top=%d %s", lua_gettop(L), file->cachedPath);
            lua_newtable(L); // create _ENV tables
            wiiu_diag_mark("smlua_load_script: env metatable begin top=%d %s", lua_gettop(L), file->cachedPath);
            lua_newtable(L); // create metatable
            wiiu_diag_mark("smlua_load_script: env get _G begin top=%d %s", lua_gettop(L), file->cachedPath);
            lua_getglobal(L, "_G"); // get global table
            wiiu_diag_mark("smlua_load_script: env get _G end top=%d type=%s %s", lua_gettop(L), lua_typename(L, lua_type(L, -1)), file->cachedPath);

            // remove certain default functions
            lua_pushstring(L, "load");           lua_pushnil(L); lua_settable(L, -3);
            lua_pushstring(L, "loadfile");       lua_pushnil(L); lua_settable(L, -3);
            lua_pushstring(L, "loadstring");     lua_pushnil(L); lua_settable(L, -3);
            lua_pushstring(L, "collectgarbage"); lua_pushnil(L); lua_settable(L, -3);
            lua_pushstring(L, "dofile");         lua_pushnil(L); lua_settable(L, -3);

            // set global as the metatable
            wiiu_diag_mark("smlua_load_script: env set __index begin top=%d %s", lua_gettop(L), file->cachedPath);
            lua_setfield(L, -2, "__index");
            wiiu_diag_mark("smlua_load_script: env setmetatable begin top=%d %s", lua_gettop(L), file->cachedPath);
            lua_setmetatable(L, -2);

            // push to registry with path as name (must be unique)
            wiiu_diag_mark("smlua_load_script: env registry set begin top=%d key=%s", lua_gettop(L), mod->relativePath);
            lua_setfield(L, LUA_REGISTRYINDEX, mod->relativePath);
            wiiu_diag_mark("smlua_load_script: env registry set end top=%d %s", lua_gettop(L), file->cachedPath);
        }

        // load mod's "global" table
        lua_getfield(L, LUA_REGISTRYINDEX, mod->relativePath);
        wiiu_diag_mark("smlua_load_script: setupvalue begin top=%d %s", lua_gettop(L), file->cachedPath);
        if (lua_setupvalue(L, chunkIndex, 1) == NULL) { // set upvalue (_ENV)
            LOG_LUA("Failed to setup _ENV for '%s'", file->cachedPath);
            gLuaInitializingScript = 0;
            lua_settop(L, prevTop);
            return LUA_ERRRUN;
        }
        wiiu_diag_mark("smlua_load_script: setupvalue end top=%d %s", lua_gettop(L), file->cachedPath);

        // load per-file globals
        if (firstInit) {
            wiiu_diag_mark("smlua_load_script: per-file globals begin %s", file->cachedPath);
            smlua_sync_table_init_globals(mod->relativePath, remoteIndex);
            smlua_cobject_init_per_file_globals(mod->relativePath);
            wiiu_diag_mark("smlua_load_script: per-file globals end %s", file->cachedPath);
        }
    } else {
        // this block is run on files that are loaded for 'require' function
        // get the mod's global table
        lua_getfield(L, LUA_REGISTRYINDEX, mod->relativePath);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            LOG_LUA("mod environment not found");
            lua_settop(L, prevTop);
            return LUA_ERRRUN;
        }
        if (lua_setupvalue(L, chunkIndex, 1) == NULL) { // set _ENV
            LOG_LUA("Failed to setup _ENV for '%s'", file->cachedPath);
            gLuaInitializingScript = 0;
            lua_settop(L, prevTop);
            return LUA_ERRRUN;
        }
    }
#endif

    // run chunks
    LOG_INFO("Executing '%s'", file->relativePath);
#if defined(TARGET_WII_U) && defined(WIIU_LUA_TRACE_LINES)
    if (isModInit) {
        wiiu_diag_mark("smlua_load_script: line trace enable %s", file->cachedPath);
        lua_sethook(L, smlua_wiiu_line_trace_hook, LUA_MASKLINE, 0);
    }
#endif
    wiiu_diag_mark("smlua_load_script: pcall begin top=%d %s", lua_gettop(L), file->cachedPath);
    rc = smlua_pcall(L, 0, 1, 0);
#if defined(TARGET_WII_U) && defined(WIIU_LUA_TRACE_LINES)
    if (isModInit) {
        lua_sethook(L, NULL, 0, 0);
        wiiu_diag_mark("smlua_load_script: line trace disable %s", file->cachedPath);
    }
#endif
#if defined(TARGET_WII_U)
    wiiu_diag_mark("smlua_load_script: gc after pcall count=%d %s", lua_gc(L, LUA_GCCOUNT, 0), file->cachedPath);
#endif
    wiiu_diag_mark("smlua_load_script: pcall rc=%d top=%d %s", rc, lua_gettop(L), file->cachedPath);
    if (rc != LUA_OK) {
        LOG_LUA("Failed to execute lua script '%s'.", file->cachedPath);
    }

    gLuaInitializingScript = 0;
    wiiu_diag_mark("smlua_load_script: end rc=%d %s", rc, file->cachedPath);

    return rc;
}

#if defined(TARGET_WII_U)
#ifndef WIIU_LUA_STAGE
#define WIIU_LUA_STAGE 999
#endif

#define WIIU_STAGE_RETURN(n) do { \
    wiiu_diag_mark("smlua_init: reached stage %d", (n)); \
    LOG_INFO("[WIIU LUA] reached stage %d", (n)); \
    if (WIIU_LUA_STAGE == (n)) { \
        wiiu_diag_mark("smlua_init: stopping at stage %d", (n)); \
        LOG_INFO("[WIIU LUA] stopping at stage %d", (n)); \
        return; \
    } \
} while (0)

static int smlua_wiiu_panic(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    wiiu_diag_mark("LUA PANIC: %s", msg ? msg : "(non-string panic)");
    LOG_ERROR("[WIIU LUA PANIC] %s", msg ? msg : "(non-string panic)");
    return 0;
}

static void smlua_wiiu_reserve_globals(lua_State* L) {
    int top = lua_gettop(L);
    wiiu_diag_mark("smlua_init: reserve globals skipped top=%d", top);
    sys_trace("smlua_init: reserve globals skipped top=%d", top);
    lua_settop(L, top);
}
#else
#define WIIU_STAGE_RETURN(n) do { } while (0)
#endif

static void smlua_require_lib(lua_State* L, const char* name, lua_CFunction openf) {
#if defined(TARGET_WII_U)
    int top = lua_gettop(L);
    wiiu_diag_mark("smlua_require_lib: %s direct open begin top=%d", name, top);
    openf(L);
    wiiu_diag_mark("smlua_require_lib: %s direct open returned top=%d type=%s", name, lua_gettop(L), lua_typename(L, lua_type(L, -1)));
    if (lua_istable(L, -1)) {
        lua_pushvalue(L, -1);
        lua_setglobal(L, name);
        lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
        if (lua_istable(L, -1)) {
            lua_pushvalue(L, -2);
            lua_setfield(L, -2, name);
        }
        lua_pop(L, 1);
    }
    lua_settop(L, top);
    wiiu_diag_mark("smlua_require_lib: %s direct open end top=%d", name, lua_gettop(L));
#else
    luaL_requiref(L, name, openf, 1);
    lua_pop(L, 1);
#endif
}

void smlua_init(void) {
    wiiu_diag_mark("smlua_init: begin");
    sys_trace("smlua_init: begin");
    smlua_shutdown();
    wiiu_diag_mark("smlua_init: after shutdown");
    sys_trace("smlua_init: after shutdown");
    WIIU_STAGE_RETURN(0);

    wiiu_diag_mark("smlua_init: luaL_newstate begin");
    gLuaState = luaL_newstate();
    lua_State* L = gLuaState;
    wiiu_diag_mark("smlua_init: luaL_newstate end state=%p", (void*)L);
    sys_trace("smlua_init: state created %p", L);
    if (L == NULL) {
        LOG_LUA("Failed to create Lua state.");
        sys_trace("smlua_init: state create failed");
        return;
    }
#if defined(TARGET_WII_U)
    lua_atpanic(L, smlua_wiiu_panic);
    smlua_wiiu_reserve_globals(L);
    wiiu_diag_mark("lua gc left running");
#endif
    WIIU_STAGE_RETURN(1);

    // load libraries
    wiiu_diag_mark("smlua_init: libs begin");
    sys_trace("smlua_init: load libraries begin");
    sys_trace("smlua_init: luaopen_base begin");
    smlua_require_lib(L, "_G", luaopen_base);
    sys_trace("smlua_init: luaopen_base end");
    WIIU_STAGE_RETURN(2);
#if defined(DEVELOPMENT)
    sys_trace("smlua_init: luaopen_debug begin");
    smlua_require_lib(L, "debug", luaopen_debug);
    sys_trace("smlua_init: luaopen_debug end");
    sys_trace("smlua_init: luaopen_io begin");
    smlua_require_lib(L, "io", luaopen_io);
    sys_trace("smlua_init: luaopen_io end");
    sys_trace("smlua_init: luaopen_os begin");
    smlua_require_lib(L, "os", luaopen_os);
    sys_trace("smlua_init: luaopen_os end");
    sys_trace("smlua_init: luaopen_package begin");
    smlua_require_lib(L, "package", luaopen_package);
    sys_trace("smlua_init: luaopen_package end");
#endif
    sys_trace("smlua_init: luaopen_math begin");
    smlua_require_lib(L, LUA_MATHLIBNAME, luaopen_math);
    sys_trace("smlua_init: luaopen_math end");
    WIIU_STAGE_RETURN(3);
    sys_trace("smlua_init: luaopen_string begin");
    smlua_require_lib(L, LUA_STRLIBNAME, luaopen_string);
    sys_trace("smlua_init: luaopen_string end");
    WIIU_STAGE_RETURN(4);
    sys_trace("smlua_init: luaopen_table begin");
    smlua_require_lib(L, LUA_TABLIBNAME, luaopen_table);
    sys_trace("smlua_init: luaopen_table end");
    WIIU_STAGE_RETURN(5);
    sys_trace("smlua_init: luaopen_coroutine begin");
    smlua_require_lib(L, LUA_COLIBNAME, luaopen_coroutine);
    sys_trace("smlua_init: luaopen_coroutine end");
    WIIU_STAGE_RETURN(6);
    sys_trace("smlua_init: luaopen_utf8 begin");
    smlua_require_lib(L, LUA_UTF8LIBNAME, luaopen_utf8);
    sys_trace("smlua_init: luaopen_utf8 end");
    WIIU_STAGE_RETURN(7);
    sys_trace("smlua_init: load libraries end");
    sys_trace("smlua_init: load libraries stack clear top=%d", lua_gettop(L));
    lua_settop(L, 0);
    wiiu_diag_mark("smlua_init: libs end");

    wiiu_diag_mark("smlua_init: bind hooks begin");
    sys_trace("smlua_init: bind begin");
    sys_trace("smlua_init: bind hooks begin");
    smlua_bind_hooks();
    sys_trace("smlua_init: bind hooks end");
    wiiu_diag_mark("smlua_init: bind hooks end");
    WIIU_STAGE_RETURN(8);
    wiiu_diag_mark("smlua_init: bind cobject begin");
    sys_trace("smlua_init: bind cobject begin");
    smlua_bind_cobject();
    sys_trace("smlua_init: bind cobject end");
    wiiu_diag_mark("smlua_init: bind cobject end");
#if defined(TARGET_WII_U)
    wiiu_diag_mark("smlua_init: cobject refs begin early");
    smlua_cobject_init_refs();
    wiiu_diag_mark("smlua_init: cobject refs end early");
#endif
    WIIU_STAGE_RETURN(9);
    wiiu_diag_mark("smlua_init: bind functions begin");
    sys_trace("smlua_init: bind functions begin");
    smlua_bind_functions();
    sys_trace("smlua_init: bind functions end");
    wiiu_diag_mark("smlua_init: bind functions end");
    WIIU_STAGE_RETURN(10);
#if defined(TARGET_WII_U) && defined(WIIU_LUA_EARLY_SYNC_BIND)
    wiiu_diag_mark("smlua_init: bind wiiu sync/read-only early begin");
    smlua_bind_sync_table();
    smlua_bind_read_only_table();
    smlua_bind_wiiu_read_only_constants();
    wiiu_diag_mark("smlua_init: bind wiiu sync/read-only early end");
#endif
    wiiu_diag_mark("smlua_init: bind functions_autogen begin");
    sys_trace("smlua_init: bind functions_autogen begin");
    smlua_bind_functions_autogen();
    sys_trace("smlua_init: bind functions_autogen end");
    wiiu_diag_mark("smlua_init: bind functions_autogen end");
    WIIU_STAGE_RETURN(11);
#if defined(TARGET_WII_U)
    wiiu_diag_mark("smlua_init: bind wiiu builtin helpers begin");
    smlua_bind_wiiu_builtin_helpers();
    wiiu_diag_mark("smlua_init: bind wiiu builtin helpers end");
#endif
    wiiu_diag_mark("smlua_init: bind sync_table begin");
    sys_trace("smlua_init: bind sync_table begin");
#if defined(TARGET_WII_U) && defined(WIIU_LUA_EARLY_SYNC_BIND)
    wiiu_diag_mark("smlua_init: bind sync_table skipped, already bound early on Wii U");
#else
    smlua_bind_sync_table();
#if !defined(TARGET_WII_U) || defined(WIIU_LUA_C_READ_ONLY_TABLE)
    smlua_bind_read_only_table();
#endif
#endif
    sys_trace("smlua_init: bind sync_table end");
    wiiu_diag_mark("smlua_init: bind sync_table end");
    WIIU_STAGE_RETURN(12);
    wiiu_diag_mark("smlua_init: bind require begin");
    sys_trace("smlua_init: bind require begin");
    smlua_init_require_system();
    sys_trace("smlua_init: bind require end");
    wiiu_diag_mark("smlua_init: bind require end");
    WIIU_STAGE_RETURN(13);
    wiiu_diag_mark("smlua_init: bind table functions begin");
    sys_trace("smlua_init: bind table functions begin");
    smlua_bind_table_functions();
    sys_trace("smlua_init: bind table functions end");
    wiiu_diag_mark("smlua_init: bind table functions end");
    sys_trace("smlua_init: bind end");

    WIIU_STAGE_RETURN(14);

    extern char gSmluaConstants[];
#if defined(TARGET_WII_U)
    wiiu_diag_mark("lua gc running for constants cbind");
#endif
    wiiu_diag_mark("smlua_init: constants begin first='%c%c%c%c'", gSmluaConstants[0], gSmluaConstants[1], gSmluaConstants[2], gSmluaConstants[3]);
    sys_trace("smlua_init: constants begin");
#if defined(TARGET_WII_U) && defined(WIIU_LUA_SMOKE_TESTS)
    if (!smlua_wiiu_constants_smoke_tests()) {
        sys_trace("smlua_init: constants smoke failed");
        return;
    }
#endif
    if (smlua_exec_constants(gSmluaConstants) != LUA_OK) {
        wiiu_diag_mark("smlua_init: constants failed");
        sys_trace("smlua_init: constants failed");
        return;
    }
    sys_trace("smlua_init: constants loaded");
    wiiu_diag_mark("smlua_init: constants end");
#if defined(TARGET_WII_U)
    wiiu_diag_mark("lua gc running after constants");
#endif
    WIIU_STAGE_RETURN(15);

    wiiu_diag_mark("smlua_init: cobject globals begin");
    smlua_cobject_init_globals();
    wiiu_diag_mark("smlua_init: cobject globals end");
    wiiu_diag_mark("smlua_init: model util begin");
    smlua_model_util_initialize();
    sys_trace("smlua_init: globals/model utils ready");
    wiiu_diag_mark("smlua_init: model util end");
    WIIU_STAGE_RETURN(16);

    // load scripts
#if defined(TARGET_WII_U) && defined(WIIU_LUA_NO_SCRIPT_LOAD)
    wiiu_diag_mark("smlua_init: script loading disabled");
    LOG_INFO("[WIIU LUA] script loading disabled");
    return;
#endif
    wiiu_diag_mark("smlua_init: script loading begin");
    sys_trace("smlua_init: script loading begin");
    mods_size_enforce(&gActiveMods);
    LOG_INFO("Loading scripts:");
    for (int i = 0; i < gActiveMods.entryCount; i++) {
        struct Mod* mod = gActiveMods.entries[i];
        sys_trace("smlua_init: mod scripts begin %s", mod->relativePath);
        LOG_INFO("    %s", mod->relativePath);
        gLuaLoadingMod = mod;
        gLuaActiveMod = mod;
        gLuaLastHookMod = mod;
        gLuaLoadingMod->customBehaviorIndex = 0;
        gPcDebug.lastModRun = gLuaActiveMod;
        for (int j = 0; j < mod->fileCount; j++) {
            struct ModFile* file = &mod->files[j];
            // skip loading non-lua files
#if defined(TARGET_WII_U)
            if (!path_ends_with(file->relativePath, ".lua")) {
                continue;
            }
#else
            if (!(path_ends_with(file->relativePath, ".lua") || path_ends_with(file->relativePath, ".luac"))) {
                continue;
            }
#endif

            // skip loading scripts in subdirectories
            if (strchr(file->relativePath, '/') != NULL || strchr(file->relativePath, '\\') != NULL) {
                continue;
            }

            gLuaActiveModFile = file;
            sys_trace("smlua_init: script begin %s/%s", mod->relativePath, file->relativePath);

#if defined(TARGET_WII_U) && defined(WIIU_LUA_DIRECT_SCRIPT_LOAD)
            wiiu_diag_mark("smlua_init: direct script load begin %s/%s", mod->relativePath, file->relativePath);
            int rc = smlua_load_script(mod, file, i, true);
            sys_trace("smlua_init: script end rc=%d %s/%s", rc, mod->relativePath, file->relativePath);
            wiiu_diag_mark("smlua_init: direct script load end rc=%d %s/%s", rc, mod->relativePath, file->relativePath);
#else
#if defined(TARGET_WII_U)
            wiiu_diag_mark("smlua_init: wiiu root script load begin %s/%s", mod->relativePath, file->relativePath);
            int rc = smlua_load_script(mod, file, i, true);
            sys_trace("smlua_init: script end rc=%d %s/%s", rc, mod->relativePath, file->relativePath);
            wiiu_diag_mark("smlua_init: wiiu root script load end rc=%d %s/%s", rc, mod->relativePath, file->relativePath);
#else
            // file has been required by some module before this
            if (!smlua_get_cached_module_result(L, mod, file)) {
                smlua_mark_module_as_loading(L, mod, file);

                s32 prevTop = lua_gettop(L);
                int rc = smlua_load_script(mod, file, i, true);

                if (rc == LUA_OK) {
                    smlua_cache_module_result(L, mod, file, prevTop);
                }
                sys_trace("smlua_init: script end rc=%d %s/%s", rc, mod->relativePath, file->relativePath);
            } else {
                sys_trace("smlua_init: script cached %s/%s", mod->relativePath, file->relativePath);
            }
#endif
#endif

            lua_settop(L, 0);
        }
        gLuaActiveMod = NULL;
        gLuaActiveModFile = NULL;
        gLuaLoadingMod = NULL;
        sys_trace("smlua_init: mod scripts end %s", mod->relativePath);
    }

    sys_trace("smlua_init: HOOK_ON_MODS_LOADED begin");
    smlua_call_event_hooks(HOOK_ON_MODS_LOADED);
    sys_trace("smlua_init: HOOK_ON_MODS_LOADED end");
    wiiu_diag_mark("smlua_init: script loading end");
}

void smlua_update(void) {
    lua_State* L = gLuaState;
    if (L == NULL) { return; }

    if (network_allow_mod_dev_mode()) { smlua_live_reload_update(L); }

    audio_sample_destroy_pending_copies();

    smlua_call_event_hooks(HOOK_UPDATE);

    // Collect our garbage after calling our hooks.
    // If we don't, Lag can quickly build up from our mods.
    // Truth is smlua generates so much garbage that the
    // incremental collection fails to keep up after some time.
    // So, for now, stop the GC from running during the hooks
    // and perform a full GC at the end of the frame.
    // EDIT: That builds up lag over time, so we need to keep
    // doing incremental garbage collection.
    // The real fix would be to make smlua produce less
    // garbage.
    // lua_gc(L, LUA_GCSTOP, 0);
    // lua_gc(L, LUA_GCCOLLECT, 0);
}

void smlua_shutdown(void) {
    hardcoded_reset_default_values();
    smlua_text_utils_reset_all();
    smlua_audio_utils_reset_all();
    audio_custom_shutdown();
    smlua_clear_hooks();
    smlua_model_util_clear();
    smlua_level_util_reset();
    smlua_anim_util_reset();
    mod_storage_shutdown();
    mod_fs_shutdown();
    lua_State* L = gLuaState;
    if (L != NULL) {
        lua_close(L);
        gLuaState = NULL;
    }
    gSmLuaCObjects = 0;
    gSmLuaCPointers = 0;
    gSmLuaCObjectMetatable = 0;
    gSmLuaCPointerMetatable = 0;
    gLuaLoadingMod = NULL;
    gLuaActiveMod = NULL;
    gLuaActiveModFile = NULL;
    gLuaLastHookMod = NULL;
}
