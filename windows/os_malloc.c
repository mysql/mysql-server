/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "toku_portability.h"
#include <stdlib.h>

#include <string.h>
#include <malloc.h>
#include <errno.h>

static inline size_t resize(size_t n) {
    if (n >= 1*1024*1024) 
        n = (n+7) & ~7; // round up to make windbg !heap happy
#define DO_PAD_64K 0
#if DO_PAD_64K
    else if (64*1024 <= n && n < 1*1024*1024)
	n = 1*1024*1024; // map anything >= 64K to 1M
#endif
#define DO_ROUND_POW2 1
#if DO_ROUND_POW2
    else {
        // make all buffers a power of 2 in size including the windows overhead
        size_t r = 0;
        size_t newn = 1<<r;
        size_t overhead = 0x24;
        n += overhead;
        while (n > newn) {
            r++;
            newn = 1<<r;
        }
        n = newn - overhead;
    }
#endif
    return n;
}

void *
os_malloc(size_t size)
{
    return malloc(resize(size));
}

void *
os_realloc(void *p, size_t size)
{
    return realloc(p, resize(size));
}

void
os_free(void* p)
{
    free(p);
}
