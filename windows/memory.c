/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "toku_portability.h"
#include "memory.h"
#include "assert.h"


#include <string.h>
#include <malloc.h>
#include <errno.h>

int toku_memory_check=0;

int toku_calloc_counter = 0;
int toku_malloc_counter = 0;
int toku_realloc_counter = 0;
int toku_free_counter = 0;

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

void *toku_calloc(size_t nmemb, size_t size) {
    void *vp;
    size_t newsize = resize(nmemb * size);
    toku_calloc_counter++;
    vp = malloc(newsize);
    if (vp) 
        memset(vp, 0, newsize);
    return vp;
}

void *toku_malloc(size_t size) {
    toku_malloc_counter++;
    return malloc(resize(size));
}

void *toku_xmalloc(size_t size) {
    void *r = toku_malloc(size);
    if (r==0) abort();
    return r;
}

void *toku_tagmalloc(size_t size, enum typ_tag typtag) {
    //printf("%s:%d tagmalloc\n", __FILE__, __LINE__);
    void *r = toku_malloc(size);
    if (!r) return 0;
    assert(size>sizeof(int));
    ((int*)r)[0] = typtag;
    return r;
}

void *toku_realloc(void *p, size_t size) {
    toku_realloc_counter++;
    return realloc(p, resize(size));
}

void toku_free(void* p) {
    toku_free_counter++;
    free(p);
}

void toku_free_n(void* p, size_t size __attribute__((unused))) {
    toku_free(p);
}

void *toku_memdup (const void *v, size_t len) {
    void *r=toku_malloc(len);
    memcpy(r,v,len);
    return r;
}

char *toku_strdup (const char *s) {
    return toku_memdup(s, strlen(s)+1);
}

void toku_memory_check_all_free (void) {
}

int toku_get_n_items_malloced (void) { return 0; }
void toku_print_malloced_items (void) {
}

void toku_malloc_report (void) {
}

void toku_malloc_cleanup (void) {
}
