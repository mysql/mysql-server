#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <rangetree.h>
#include <errno.h>
#include "../../../newbrt/memory.h"
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
static u_int32_t rstate=1;
static inline void mysrandom (int s) {
    rstate=s;
}
static inline u_int32_t myrandom (void) {
    rstate = (279470275ull*(u_int64_t)rstate)%4294967291ull;
    return rstate;
}


int dummy_cmp(const toku_point *a __attribute__((__unused__)),  
              const toku_point *b __attribute__((__unused__))) {
    return 0;
}

int TXNID_cmp(const TXNID a, const TXNID b) {
    return a < b ? -1 : (a != b); /* \marginpar{!?} */
}

int int_cmp(const toku_point* a, const toku_point*b) {
    int x = *(int*)a;
    int y = *(int*)b;
    return x -y;
}

int char_cmp(const TXNID a, const TXNID b) {
    int x = (char)a;
    int y = (char)b;
    return x -y;
}

int mallocced = 0;
int failon    = -1;

void* fail_malloc(size_t size) {
    if (++mallocced == failon) {
        errno = ENOMEM;
        return NULL;
    }
    return malloc(size);
}

void verify_all_overlap(toku_interval* query, toku_range* list, unsigned listlen) {
    unsigned i;
    
    for (i = 0; i < listlen; i++) {
        /* Range A and B overlap iff A.left <= B.right && B.left <= A.right */
        assert(int_cmp(query->left, list[i].ends.right) <= 0 &&
               int_cmp(list[i].ends.left, query->right) <= 0);
    }
}

