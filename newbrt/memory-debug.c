/* An implementation of memory that can be made to return NULL and ENOMEM on certain mallocs. */

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "memory.h"
#include "toku_assert.h"

#include <errno.h>
#include <string.h>

int *toku_dead_mallocs=0;
int toku_malloc_counter=0;

void toku_malloc_cleanup(void) {}
void toku_free(void*x) {
    free(x);
}

// if it fails, return 1, and set errno
static int does_malloc_fail (void) {
    if (toku_dead_mallocs) {
	int mnum = *toku_dead_mallocs;
	if (mnum==toku_malloc_counter) {
	    toku_malloc_counter++;
	    toku_dead_mallocs++;
	    errno=ENOMEM;
	    return 1;
	}
    }
    toku_malloc_counter++;
    return 0;
}

void* toku_malloc(size_t n) {
    if (does_malloc_fail()) return 0;
    return malloc(n);
}
void *toku_tagmalloc(size_t size, enum typ_tag typtag) {
    //printf("%s:%d tagmalloc\n", __FILE__, __LINE__);
    void *r = toku_malloc(size);
    if (!r) return 0;
    assert(size>sizeof(int));
    ((int*)r)[0] = typtag;
    return r;
}

void *toku_memdup (const void *v, size_t len) {
    void *r=toku_malloc(len);
    memcpy(r,v,len);
    return r;
}

char *toku_strdup (const char *s) {
    return toku_memdup(s, strlen(s)+1);
}

void *toku_realloc(void *p, size_t size) {
    if (p==0) return toku_malloc(size);
    if (does_malloc_fail()) return 0;
    return realloc(p, size);
}
