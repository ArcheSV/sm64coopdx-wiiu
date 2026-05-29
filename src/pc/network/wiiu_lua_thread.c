#if defined(TARGET_WII_U)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <coreinit/thread.h>
#include "pc/platform.h"

#if defined(WIIU_LUA_USE_THREAD)

#ifndef WIIU_LUA_THREAD_STACK_BYTES
#define WIIU_LUA_THREAD_STACK_BYTES (32 * 1024 * 1024)
#endif

#ifndef WIIU_LUA_THREAD_AFFINITY
#define WIIU_LUA_THREAD_AFFINITY OS_THREAD_ATTRIB_AFFINITY_CPU1
#endif

#ifndef WIIU_LUA_THREAD_PRIORITY_DELTA
#define WIIU_LUA_THREAD_PRIORITY_DELTA 0
#endif

static OSThread sWiiuLuaThread __attribute__((aligned(8)));
static void (*sWiiuLuaThreadEntry)(void) = NULL;
static uint8_t sWiiuLuaThreadStack[WIIU_LUA_THREAD_STACK_BYTES] __attribute__((aligned(0x100)));

static int wiiu_lua_clamp_thread_priority(int priority) {
    if (priority < 0) { return 0; }
    if (priority > 31) { return 31; }
    return priority;
}

static int wiiu_lua_thread_entry(int argc, const char **argv) {
    (void)argc;
    (void)argv;

    if (sWiiuLuaThreadEntry != NULL) {
        sWiiuLuaThreadEntry();
    }

    return 0;
}

bool wiiu_run_lua_init_thread(void (*entry)(void), size_t stackSize) {
    if (entry == NULL || stackSize == 0) {
        return false;
    }
    if (stackSize > sizeof(sWiiuLuaThreadStack)) {
        wiiu_diag_mark("wiiu_lua_thread: requested stack too large size=%u max=%u",
                       (uint32_t)stackSize, (uint32_t)sizeof(sWiiuLuaThreadStack));
        return false;
    }

    sWiiuLuaThreadEntry = entry;
    memset(&sWiiuLuaThread, 0, sizeof(sWiiuLuaThread));

    uint8_t *stackTop = sWiiuLuaThreadStack + stackSize;
    OSThread *currentThread = OSGetCurrentThread();
    int currentPriority = OSGetThreadPriority(currentThread);
    uint32_t currentAffinity = OSGetThreadAffinity(currentThread);
    int threadPriority = wiiu_lua_clamp_thread_priority(currentPriority + WIIU_LUA_THREAD_PRIORITY_DELTA);
    uint32_t threadAffinity = WIIU_LUA_THREAD_AFFINITY;

    wiiu_diag_mark("wiiu_lua_thread: OSCreateThread requested base=%p top=%p size=%u priority=%d->%d affinity=0x%X->0x%X",
                   (void*)sWiiuLuaThreadStack, (void*)stackTop, (uint32_t)stackSize,
                   currentPriority, threadPriority, currentAffinity, threadAffinity);

    BOOL createResult = OSCreateThread(&sWiiuLuaThread,
                                       wiiu_lua_thread_entry,
                                       0,
                                       NULL,
                                       stackTop,
                                       (uint32_t)stackSize,
                                       threadPriority,
                                       (OSThreadAttributes)threadAffinity);
    if (!createResult) {
        wiiu_diag_mark("wiiu_lua_thread: OSCreateThread failed");
        sWiiuLuaThreadEntry = NULL;
        return false;
    }

    int32_t resumeResult = OSResumeThread(&sWiiuLuaThread);

    int threadResult = -1;
    BOOL joinResult = OSJoinThread(&sWiiuLuaThread, &threadResult);
    wiiu_diag_mark("wiiu_lua_thread: OSResumeThread result=%d OSJoinThread result=%d thread_result=%d",
                   resumeResult, joinResult, threadResult);

    sWiiuLuaThreadEntry = NULL;
    memset(&sWiiuLuaThread, 0, sizeof(sWiiuLuaThread));
    return joinResult && threadResult == 0;
}

#else

bool wiiu_run_lua_init_thread(void (*entry)(void), size_t stackSize) {
    (void)entry;
    (void)stackSize;
    return false;
}

#endif

#endif
