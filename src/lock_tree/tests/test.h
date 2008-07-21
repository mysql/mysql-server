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
#include <db_id.h>
#include <lth.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory.h>
#include <key.h>


BOOL want_panic = FALSE;

int intcmp(DB *db __attribute__((__unused__)), const DBT* a, const DBT* b) {
    int x = *(int*)a->data;
    int y = *(int*)b->data;

    return x - y;
}


int dbcmp (DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

toku_dbt_cmp compare_fun = dbcmp;
toku_dbt_cmp dup_compare = dbcmp;

toku_dbt_cmp get_compare_fun_from_db(__attribute__((unused)) DB* db) {
    return compare_fun;
}

toku_dbt_cmp get_dup_compare_from_db(__attribute__((unused)) DB* db) {
    return dup_compare;
}

BOOL panicked = FALSE;

int dbpanic(DB* db, int r) {
    if (verbose) printf("AHH!!!! %d is rampaging! Run away %p!!!\n", r, db);
    panicked = TRUE;
    assert(want_panic);
    return EINVAL;
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
static u_int32_t rstate=1;
static inline void mysrandom (int s) {
    rstate=s;
}
static inline u_int32_t myrandom (void) {
    rstate = (279470275ull*(u_int64_t)rstate)%4294967291ull;
    return rstate;
}


DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}


/**
   A comparison function between toku_point's.
   It is implemented as a wrapper of db compare and dup_compare functions,
   but it checks whether the point is +/- infty.
   Parameters are of type toku_point.
   Return values conform to cmp from qsort(3).
 */
extern int toku__lt_point_cmp(void* a, void* b);

void init_point(toku_point* point, toku_lock_tree* tree) {
    assert(point && tree);
    memset(point, 0, sizeof(toku_point));
    point->lt = tree;
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
