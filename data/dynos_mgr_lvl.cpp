#include "dynos.cpp.h"

extern "C" {
#include "engine/level_script.h"
#include "game/skybox.h"
#include "pc/platform.h"
}

struct OverrideLevelScript {
    const void *originalScript;
    const void *newScript;
    GfxData *gfxData;
};

static std::vector<OverrideLevelScript> &DynosOverrideLevelScripts() {
    static std::vector<OverrideLevelScript> sDynosOverrideLevelScripts;
    return sDynosOverrideLevelScripts;
}

std::vector<std::pair<std::string, GfxData *>> &DynOS_Lvl_GetArray() {
    static std::vector<std::pair<std::string, GfxData *>> sDynosCustomLevelScripts;
    return sDynosCustomLevelScripts;
}

LevelScript* DynOS_Lvl_GetScript(const char* aScriptEntryName) {
    auto& _CustomLevelScripts = DynOS_Lvl_GetArray();
    for (size_t i = 0; i < _CustomLevelScripts.size(); ++i) {
        auto& pair = _CustomLevelScripts[i];
        if (pair.first == aScriptEntryName) {
            auto& newScripts = pair.second->mLevelScripts;
            auto& newScriptNode = newScripts[newScripts.Count() - 1];
            return newScriptNode->mData;
        }
    }
    return NULL;
}

void DynOS_Lvl_ModShutdown() {
    DynOS_Level_Unoverride();

    auto& _CustomLevelScripts = DynOS_Lvl_GetArray();
    if (!_CustomLevelScripts.empty()) {
        for (auto& pair : _CustomLevelScripts) {
            DynOS_Tex_Invalid(pair.second);
            Delete(pair.second);
        }
        _CustomLevelScripts.clear();
    }

    auto& _OverrideLevelScripts = DynosOverrideLevelScripts();
    _OverrideLevelScripts.clear();
}

void DynOS_Lvl_Activate(s32 modIndex, const SysPath &aFilename, const char *aLevelName) {
    auto& _CustomLevelScripts = DynOS_Lvl_GetArray();
    auto& _OverrideLevelScripts = DynosOverrideLevelScripts();
    sys_trace("DynOS_Lvl_Activate: begin file=%s level=%s mod=%d", aFilename.c_str(), aLevelName, modIndex);

    // make sure vanilla levels were parsed
    sys_trace("DynOS_Lvl_Activate: level init begin");
    DynOS_Level_Init();
    sys_trace("DynOS_Lvl_Activate: level init end");

    // check for duplicates
    for (auto &customLevel : _CustomLevelScripts) {
        if (customLevel.first == aLevelName) {
            sys_trace("DynOS_Lvl_Activate: duplicate level=%s", aLevelName);
            return;
        }
    }

    std::string levelName = aLevelName;

    sys_trace("DynOS_Lvl_Activate: load binary begin level=%s", aLevelName);
    GfxData* _Node = DynOS_Lvl_LoadFromBinary(aFilename, levelName.c_str());
    sys_trace("DynOS_Lvl_Activate: load binary end level=%s gfx=%p", aLevelName, _Node);
    if (!_Node) {
        return;
    }

    // remember index
    _Node->mModIndex = modIndex;

    // Add to levels
    _CustomLevelScripts.emplace_back(levelName, _Node);
    sys_trace("DynOS_Lvl_Activate: added level=%s scripts=%u geos=%u cols=%u textures=%u",
              aLevelName, _Node->mLevelScripts.Count(), _Node->mGeoLayouts.Count(),
              _Node->mCollisions.Count(), _Node->mTextures.Count());
    DynOS_Tex_Valid(_Node);
    sys_trace("DynOS_Lvl_Activate: tex valid level=%s", aLevelName);

    // Override vanilla script
    auto& newScripts = _Node->mLevelScripts;
    if (newScripts.Count() <= 0) {
        PrintError("Could not find level scripts: '%s'", aLevelName);
        sys_trace("DynOS_Lvl_Activate: no scripts level=%s", aLevelName);
        return;
    }

    auto& newScriptNode = newScripts[newScripts.Count() - 1];
    sys_trace("DynOS_Lvl_Activate: script selected level=%s node=%s data=%p words=%u",
              aLevelName, newScriptNode->mName.begin(), newScriptNode->mData, newScriptNode->mSize);
    const void* originalScript = DynOS_Builtin_ScriptPtr_GetFromName(newScriptNode->mName.begin());
    if (originalScript == NULL) {
        sys_trace("DynOS_Lvl_Activate: no original script for %s", newScriptNode->mName.begin());
        return;
    }

    sys_trace("DynOS_Lvl_Activate: override begin level=%s original=%p new=%p",
              aLevelName, originalScript, newScriptNode->mData);
    DynOS_Level_Override((void*)originalScript, newScriptNode->mData, modIndex);
    sys_trace("DynOS_Lvl_Activate: override end level=%s", aLevelName);
    _OverrideLevelScripts.push_back({ originalScript, newScriptNode->mData, _Node});
    sys_trace("DynOS_Lvl_Activate: end level=%s", aLevelName);
}

GfxData* DynOS_Lvl_GetActiveGfx(void) {
    auto& _CustomLevelScripts = DynOS_Lvl_GetArray();
    for (auto &lvlEntry : _CustomLevelScripts) {
        auto& gfxData = lvlEntry.second;
        auto& scripts = gfxData->mLevelScripts;
        for (auto& s : scripts) {
            if (gLevelScriptActive == s->mData) {
                return gfxData;
            }
        }
    }
    return NULL;
}

const char* DynOS_Lvl_GetToken(u32 index) {
    GfxData* gfxData = DynOS_Lvl_GetActiveGfx();
    if (gfxData == NULL) {
        return NULL;
    }

    // have to 1-index due to to pointer read code
    index = index - 1;

    if (index >= gfxData->mLuaTokenList.Count()) {
        return NULL;
    }

    return gfxData->mLuaTokenList[index].begin();
}

Trajectory* DynOS_Lvl_GetTrajectory(const char* aName) {
    auto& _CustomLevelScripts = DynOS_Lvl_GetArray();

    for (auto& script : _CustomLevelScripts) {
        for (auto& trajectoryNode : script.second->mTrajectories) {
            if (trajectoryNode->mName == aName) {
                return trajectoryNode->mData;
            }
        }
    }
    return NULL;
}

void DynOS_Lvl_LoadBackground(void *aPtr) {
    auto& _CustomLevelScripts = DynOS_Lvl_GetArray();

    // ensure this texture list exists
    GfxData* foundGfxData = NULL;
    DataNode<TexData*>* foundList = NULL;
    for (auto& script : _CustomLevelScripts) {
        auto &textureLists = script.second->mTextureLists;
        for (auto& textureList : textureLists) {
            if (textureList == aPtr) {
                foundGfxData = script.second;
                foundList = textureList;
                goto double_break;
            }
        }
    }
double_break:

    if (foundList == NULL) {
        PrintError("Could not find custom background");
        return;
    }

    // Load up custom background
    for (s32 i = 0; i < 80; i++) {
        // find texture
        for (auto& tex : foundGfxData->mTextures) {
            if (tex->mData == foundList->mData[i]) {
                gCustomSkyboxPtrList[i] = (Texture*)tex;
                break;
            }
        }
    }
}

void *DynOS_Lvl_Override(void *aCmd) {
    auto& _OverrideLevelScripts = DynosOverrideLevelScripts();
    for (auto& overrideStruct : _OverrideLevelScripts) {
        if (aCmd == overrideStruct.originalScript || aCmd == overrideStruct.newScript) {
            aCmd = (void*)overrideStruct.newScript;
            gLevelScriptModIndex = overrideStruct.gfxData->mModIndex;
            gLevelScriptActive = (LevelScript*)aCmd;
        }
    }

    auto& _CustomLevelScripts = DynOS_Lvl_GetArray();
    for (auto& script : _CustomLevelScripts) {
        auto& scripts = script.second->mLevelScripts;
        for (auto& s : scripts) {
            if (aCmd == s->mData) {
                gLevelScriptModIndex = script.second->mModIndex;
                gLevelScriptActive = (LevelScript*)aCmd;
            }
        }
    }

    return aCmd;
}
