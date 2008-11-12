#include <stdio.h>
#include "ydb.h"

#if defined(__GNUC__)

static void __attribute__((constructor)) libtokudb_init(void) {
    // printf("%s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
    toku_ydb_init();
}

static void __attribute__((destructor)) libtokudb_destroy(void) {
    // printf("%s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
    toku_ydb_destroy();
}

#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define UNUSED(x) x=x

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    UNUSED(h); UNUSED(reserved);
    // printf("%s:%lu\n", __FUNCTION__, reason);
    if (reason == DLL_PROCESS_ATTACH)
        toku_ydb_init();
    if (reason == DLL_PROCESS_DETACH)
        toku_ydb_destroy();
    return TRUE;
}

#endif
