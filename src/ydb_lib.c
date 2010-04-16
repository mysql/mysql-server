#include <stdio.h>
#include <toku_stdint.h>
#include <toku_portability.h>
#include <db.h>
#include "ydb.h"
#include <toku_assert.h>

#if defined(__GNUC__)

static void __attribute__((constructor)) libtokudb_init(void) {
    // printf("%s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
    int r = toku_ydb_init();
    assert(r==0);
}

static void __attribute__((destructor)) libtokudb_destroy(void) {
    // printf("%s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
    int r = toku_ydb_destroy();
    assert(r==0);
}

#endif

#if TOKU_WINDOWS
#include <windows.h>
#define UNUSED(x) x=x

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    UNUSED(h); UNUSED(reserved);
    // printf("%s:%lu\n", __FUNCTION__, reason);
    int r = 0;
    switch(reason) {
    case DLL_PROCESS_ATTACH:
        r = toku_ydb_init();
        break;
    case DLL_PROCESS_DETACH:
        r = toku_ydb_destroy();
        break;
    case DLL_THREAD_ATTACH:
        //TODO: Any new thread code if necessary, i.e. allocate per-thread
        //      storage.
        break;
    case DLL_THREAD_DETACH:
        //TODO: Any cleanup thread code if necessary, i.e. free per-thread
        //      storage.
        break;
    default:
        break;
    }
    assert(r==0);
    return TRUE;
}

#endif

