#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <rangetree.h>
#include <errno.h>
int verbose=0;

#define CKERR(r) ({ if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, strerror(r)); assert(r==0); })
#define CKERR2(r,r2) ({ if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); })

void parse_args (int argc, const char *argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-h]\n", argv0);
	    exit(resultcode);
	} else {
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

// Simle LCG random number generator.  Not high quality, but good enough.
static uint32_t rstate=1;
static inline void mysrandom (int s) {
    rstate=s;
}
static inline uint32_t myrandom (void) {
    rstate = (279470275ull*(uint64_t)rstate)%4294967291ull;
    return rstate;
}


int dummy_cmp(void *a __attribute__((__unused__)),  
              void *b __attribute__((__unused__))) {
    return 0;
}

int ptr_cmp(void *a, void *b) {
    return a < b ? -1 : (a != b); /* \marginpar{!?} */
}

int int_cmp(void *a, void *b) {
    int x = *(int*)a;
    int y = *(int*)b;
    return x -y;
}

int char_cmp(void *a, void *b) {
    int x = *(char*)a;
    int y = *(char*)b;
    return x -y;
}

void* toku_malloc(size_t size) {
    return malloc(size);
}

void toku_free(void* p) {
    free(p);
}

void* toku_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}
