#include <string.h>
#include <stdio.h>

#include "memarena.h"
#include "memory.h"
#include "toku_assert.h"

struct memarena {
    char *buf;
    size_t buf_used, buf_size;
    char **other_bufs;
    int n_other_bufs;
};

MEMARENA memarena_create (void) {
    MEMARENA MALLOC(result);  assert(result);
    result->buf_size = 1024;
    result->buf_used = 0;
    result->other_bufs = NULL;
    result->n_other_bufs = 0;
    result->buf = toku_malloc(result->buf_size);  assert(result->buf);
    return result;
}

void memarena_clear (MEMARENA ma) {
    // Free the other bufs.
    int i;
    for (i=0; i<ma->n_other_bufs; i++) {
	toku_free(ma->other_bufs[i]);
	ma->other_bufs[i]=0;
    }
    ma->n_other_bufs=0;
    // But reuse the main buffer
    ma->buf_used = 0;
}

size_t round_to_page (size_t size) {
    const size_t PAGE_SIZE = 4096;
    const size_t result = PAGE_SIZE+((size-1)&~(PAGE_SIZE-1));
    assert(0==(result&(PAGE_SIZE-1))); // make sure it's aligned
    assert(result>=size);              // make sure it's not too small
    assert(size<result+PAGE_SIZE);     // make sure we didn't grow by more than a page.
    return result;
}

void* malloc_in_memarena (MEMARENA ma, size_t size) {
    if (ma->buf_size < ma->buf_used + size) {
	// The existing block isn't big enough.
	// Add the block to the vector of blocks.
	if (ma->buf) {
	    int old_n = ma->n_other_bufs;
	    REALLOC_N(old_n+1, ma->other_bufs);
	    assert(ma->other_bufs);
	    ma->other_bufs[old_n]=ma->buf;
	    ma->n_other_bufs = old_n+1;
	}
	// Make a new one
	{
	    size_t new_size = 2*ma->buf_size;
	    if (new_size<size) new_size=size;
	    new_size=round_to_page(new_size); // at least size, but round to the next page size
	    ma->buf = toku_malloc(new_size);
	    assert(ma->buf);
	    ma->buf_used = 0;
	    ma->buf_size = new_size;
	}
    }
    // allocate in the existing block.
    char *result=ma->buf+ma->buf_used;
    ma->buf_used+=size;
    return result;
}

void *memarena_memdup (MEMARENA ma, const void *v, size_t len) {
    void *r=malloc_in_memarena(ma, len);
    memcpy(r,v,len);
    return r;
}

void memarena_close(MEMARENA *map) {
    MEMARENA ma=*map;
    if (ma->buf) {
	toku_free(ma->buf);
	ma->buf=0;
    }
    int i;
    for (i=0; i<ma->n_other_bufs; i++) {
	toku_free(ma->other_bufs[i]);
    }
    if (ma->other_bufs) toku_free(ma->other_bufs);
    ma->other_bufs=0;
    ma->n_other_bufs=0;
    toku_free(ma);
    *map = 0;
}

void memarena_move_buffers(MEMARENA dest, MEMARENA source) {
    REALLOC_N(dest->n_other_bufs + source->n_other_bufs + 1, dest->other_bufs);
    int i;
    for (i=0; i<source->n_other_bufs; i++) {
	dest->other_bufs[dest->n_other_bufs++] = source->other_bufs[i];
    }
    dest->other_bufs[dest->n_other_bufs++] = source->buf;
    source->n_other_bufs = 0;
    toku_free(source->other_bufs);
    source->other_bufs = 0;
    source->buf = 0;
    source->buf_size = 0;
    source->buf_used = 0;
}
