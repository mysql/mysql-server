/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

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
    toku_ydb_destroy();
}

#endif

#if TOKU_WINDOWS
#include <windows.h>
#define UNUSED(x) x=x

bool WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    UNUSED(h); UNUSED(reserved);
    // printf("%s:%lu\n", __FUNCTION__, reason);
    int r = 0;
    switch(reason) {
    case DLL_PROCESS_ATTACH:
        r = toku_ydb_init();
        break;
    case DLL_PROCESS_DETACH:
        toku_ydb_destroy();
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
    return true;
}

#endif

