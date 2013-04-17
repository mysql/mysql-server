/* Test to see if the mempool uses madvise to mitigate #4934. */
#include "mempool.h"
#include "memory.h"
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
int mallctl(const char *op, void *r, size_t *rsize, void *s, size_t ssize);
int mallctl(const char *op, void *r, size_t *rsize, void *s, size_t ssize) {
    if (strcmp(op, "version")==0) {
	assert(*rsize == sizeof(char *));
	char **rc=r;
	*rc = "libc faking as jemalloc";
	assert(s==NULL);
	assert(ssize==0);
	return 0;
    } else if (strcmp(op, "opt.lg_chunk")==0) {
	assert(*rsize==sizeof(size_t));
	size_t *lg_chunk_p=r;
	*lg_chunk_p = 22;
	assert(s==NULL);
	assert(ssize==0);
	return 0;
    } else {
	assert(0);
    }
    return 0;
}

struct known_sizes {
    void *p;
    size_t size;
} known_sizes[100];
int n_known_sizes=0;

size_t malloc_usable_size(const void *p);
size_t malloc_usable_size(const void *p) {
    for (int i=0; i<n_known_sizes; i++) {
	if (p==known_sizes[i].p) {
	    return known_sizes[i].size;
	}
    }
    printf("p=%p\n", p);
    abort();
}

void *mem;
int counter=0;

int madvise (void *addr, size_t length, int advice) {
    char *a=addr;
    char *m=mem;
    if (counter==0) {
	assert(m+4096==a);
	assert(length=16*1024*1024-4096);
	assert(advice=MADV_DONTNEED);
    } else if (counter==1) {
	assert(m+2*1024*1024+4096==a);
	assert(length=16*1024*1024-2*1024*1024-4096);
	assert(advice=MADV_DONTNEED);
    } else if (counter==2) {
	assert(m+4*1024*1024+4096==a);
	assert(length=16*1024*1024-4*1024*1024-4096);
	assert(advice=MADV_DONTNEED);
    } else {
	printf("madvise(%p, 0x%lx, %d)\n", addr, length, advice);
	abort();
    }
    counter++;
    return 0;
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    // toku_memory_startup(); is called by the linker.

    struct mempool m;
    size_t siz = 16*1024*1024;
    {
	int r = posix_memalign(&mem, 2*1024*1024, siz);
	assert(r==0);
	known_sizes[n_known_sizes++] = (struct known_sizes){mem, siz};
    }
    toku_mempool_init(&m, mem, siz);

    void *a  = toku_mempool_malloc(&m, 1, 1);
    assert(a==mem);
    void *b = toku_mempool_malloc(&m, 2*1024*1024 - 4096, 4096);
    assert(b==(char*)mem+4096);

    void *c = toku_mempool_malloc(&m, 1, 1);
    assert(c==(char*)mem+2*1024*1024);

    void *d = toku_mempool_malloc(&m, 2*1024*1024, 1);
    assert(d==(char*)mem+2*1024*1024+1);

    toku_free(mem);
    
    return 0;
}
