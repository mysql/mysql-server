#include <string.h>
#include <locktree.h>
#include <db.h>
#include <brttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
int verbose=0;

int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    int comparelen = key1len<key2len ? key1len : key2len;
    const unsigned char *k1;
    const unsigned char *k2;
    for (k1=key1, k2=key2;
	 comparelen>4;
	 k1+=4, k2+=4, comparelen-=4) {
	{ int v1=k1[0], v2=k2[0]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[1], v2=k2[1]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[2], v2=k2[2]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[3], v2=k2[3]; if (v1!=v2) return v1-v2; }
    }
    for (;
	 comparelen>0;
	 k1++, k2++, comparelen--) {
	if (*k1 != *k2) {
	    return (int)*k1-(int)*k2;
	}
    }
    return key1len-key2len;
}


int dbcmp (DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

void dbpanic(DB* db) {
    if (verbose) printf("AHH!!!!  Run away %p!!!\n", db);
}


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


void* toku_malloc(size_t size) {
    return malloc(size);
}

void toku_free(void* p) {
    free(p);
}

void* toku_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}
