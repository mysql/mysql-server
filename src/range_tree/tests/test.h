#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <rangetree.h>
#include <errno.h>
#include "memory.h"
int verbose=0;

#define CKERR(r) do { if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, strerror(r)); assert(r==0); } while (0)
#define CKERR2(r,r2) do { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); } while (0)

static inline void
parse_args (int argc, const char *argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1], "-q")==0) {
	    verbose=0;
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-h|-q]\n", argv0);
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
static u_int32_t rstate=1;
static inline void mysrandom (int s) {
    rstate=s;
}
static inline u_int32_t myrandom (void) {
    rstate = (279470275ull*(u_int64_t)rstate)%4294967291ull;
    return rstate;
}


static inline int
dummy_cmp (const toku_point *a __attribute__((__unused__)),  
	   const toku_point *b __attribute__((__unused__))) {
    return 0;
}

static inline int
TXNID_cmp (const TXNID a, const TXNID b) {
    return a < b ? -1 : (a != b); /* \marginpar{!?} */
}

struct __toku_point {
    int n;
};

static inline int
int_cmp (const toku_point* a, const toku_point*b) {
    int x = *(int*)a;
    int y = *(int*)b;
    return x -y;
}

static inline int
char_cmp (const TXNID a, const TXNID b) {
    int x = (char)a;
    int y = (char)b;
    return x -y;
}

int mallocced = 0;
int failon    = -1;

static inline void*
fail_malloc (size_t size) {
    if (++mallocced == failon) {
        errno = ENOMEM;
        return NULL;
    }
    return malloc(size);
}

static inline void
verify_all_overlap (toku_interval* query, toku_range* list, unsigned listlen) {
    unsigned i;
    
    for (i = 0; i < listlen; i++) {
        /* Range A and B overlap iff A.left <= B.right && B.left <= A.right */
        assert(int_cmp(query->left, list[i].ends.right) <= 0 &&
               int_cmp(list[i].ends.left, query->right) <= 0);
    }
}

