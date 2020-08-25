#include <stdio.h>
#include <string.h>
#include "lib/src/libultra_internal.h"
#include "macros.h"
#include "platform.h"
#include "fs/fs.h"

#define SAVE_FILENAME "sm64_save_file.bin"

#ifdef TARGET_WEB
#include <emscripten.h>
#endif

extern OSMgrArgs piMgrArgs;

u64 osClockRate = 62500000;

s32 osPiStartDma(UNUSED OSIoMesg *mb, UNUSED s32 priority, UNUSED s32 direction,
                 uintptr_t devAddr, void *vAddr, size_t nbytes,
                 UNUSED OSMesgQueue *mq) {
    memcpy(vAddr, (const void *) devAddr, nbytes);
    return 0;
}

void osSetEventMesg(UNUSED OSEvent e, UNUSED OSMesgQueue *mq, UNUSED OSMesg msg) {
}
s32 osJamMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED s32 flag) {
    return 0;
}
s32 osSendMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED s32 flag) {
    return 0;
}
s32 osRecvMesg(UNUSED OSMesgQueue *mq, UNUSED OSMesg *msg, UNUSED s32 flag) {
    return 0;
}

uintptr_t osVirtualToPhysical(void *addr) {
    return (uintptr_t) addr;
}

void osCreateViManager(UNUSED OSPri pri) {
}
void osViSetMode(UNUSED OSViMode *mode) {
}
void osViSetEvent(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED u32 retraceCount) {
}
void osViBlack(UNUSED u8 active) {
}
void osViSetSpecialFeatures(UNUSED u32 func) {
}
void osViSwapBuffer(UNUSED void *vaddr) {
}

OSTime osGetTime(void) {
    return 0;
}

void osWritebackDCacheAll(void) {
}

void osWritebackDCache(UNUSED void *a, UNUSED size_t b) {
}

void osInvalDCache(UNUSED void *a, UNUSED size_t b) {
}

u32 osGetCount(void) {
    static u32 counter;
    return counter++;
}

s32 osAiSetFrequency(u32 freq) {
    u32 a1;
    s32 a2;
    u32 D_8033491C;

#ifdef VERSION_EU
    D_8033491C = 0x02E6025C;
#else
    D_8033491C = 0x02E6D354;
#endif

    a1 = D_8033491C / (float) freq + .5f;

    if (a1 < 0x84) {
        return -1;
    }

    a2 = (a1 / 66) & 0xff;
    if (a2 > 16) {
        a2 = 16;
    }

    return D_8033491C / (s32) a1;
}

s32 osEepromProbe(UNUSED OSMesgQueue *mq) {
    return 1;
}

s32 osEepromLongRead(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    u8 content[512];
    s32 ret = -1;

#ifdef TARGET_WEB
    if (EM_ASM_INT({
        var s = localStorage.sm64_save_file;
        if (s && s.length === 684) {
            try {
                var binary = atob(s);
                if (binary.length === 512) {
                    for (var i = 0; i < 512; i++) {
                        HEAPU8[$0 + i] = binary.charCodeAt(i);
                    }
                    return 1;
                }
            } catch (e) {
            }
        }
        return 0;
    }, content)) {
        memcpy(buffer, content + address * 8, nbytes);
        ret = 0;
    }
#else
    fs_file_t *fp = fs_open(SAVE_FILENAME);
    if (fp == NULL) {
        return -1;
    }
    if (fs_read(fp, content, 512) == 512) {
        memcpy(buffer, content + address * 8, nbytes);
        ret = 0;
    }
    fs_close(fp);
#endif
    return ret;
}

s32 osEepromLongWrite(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    u8 content[512] = {0};
    if (address != 0 || nbytes != 512) {
        osEepromLongRead(mq, 0, content, 512);
    }
    memcpy(content + address * 8, buffer, nbytes);

#ifdef TARGET_WEB
    EM_ASM({
        var str = "";
        for (var i = 0; i < 512; i++) {
            str += String.fromCharCode(HEAPU8[$0 + i]);
        }
        localStorage.sm64_save_file = btoa(str);
    }, content);
    s32 ret = 0;
#else
    FILE *fp = fopen(fs_get_write_path(SAVE_FILENAME), "wb");
    if (fp == NULL) {
        return -1;
    }
    s32 ret = fwrite(content, 1, 512, fp) == 512 ? 0 : -1;
    fclose(fp);
#endif
    return ret;
}

#ifdef __WIIU__
#include <coreinit/thread.h>

static int wThreadMain(int argc, const char **argv) {
    N64_OSThread *thread = (N64_OSThread *)argv;
WHBLogPrintf("wThreadMain: 0x%08X 0x%08X", thread->entry, thread->arg);
    thread->entry(thread->arg);
    return 0;
}
#endif

void osCreateThread(N64_OSThread *thread, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri pri) {
#ifdef __WIIU__
    // TODO: Priority mapping:
    // Wii U: 0 = highest priority, 31 = lowest, 16 = default
    // N64: 127 = highest, 0 = lowest
    OSCreateThread(&(thread->wiiUThread), wThreadMain, 1, (char *)thread, thread->stack + WTHREAD_STACK_SIZE, WTHREAD_STACK_SIZE, 16 /* TODO */, OS_THREAD_ATTRIB_DETACHED | OS_THREAD_ATTRIB_AFFINITY_CPU1);
//    thread->entry(thread->arg); // This works...
    thread->entry = entry;
    thread->arg = arg;
    WHBLogPrintf("osCreateThread: 0x%08X 0x%08X", thread->entry, thread->arg);
    char name[32];
    sprintf(name, "Super Mario Thread %d", id);
    OSSetThreadName(&(thread->wiiUThread), name);
#endif
}

void osDestroyThread(N64_OSThread *thread) {
#ifdef __WIIU__
    OSCancelThread(&(thread->wiiUThread));
#endif
}

void osStartThread(N64_OSThread *thread) {
#ifdef __WIIU__
WHBLogPrint("osStartThread");
    OSResumeThread(&(thread->wiiUThread));
#endif
}

void osStopThread(N64_OSThread *thread) {
#ifdef __WIIU__
    OSCancelThread(&(thread->wiiUThread));
#endif
}

/* TODO: These functions are already implemented elsewhere
OSPri osGetThreadPri(N64_OSThread *thread) {
    // STUB
    return 127;
}

void osSetThreadPri(N64_OSThread *thread, OSPri pri) {
    // STUB
}
*/
