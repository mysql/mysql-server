/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "memory.h"
#include "toku_assert.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

int toku_memory_check=0;

void *toku_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void *toku_malloc(size_t size) {
    return malloc(size);
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
    return realloc(p, size);
}

void toku_free(void* p) {
    free(p);
}

void toku_free_n(void* p, size_t size __attribute__((unused))) {
    free(p);
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
