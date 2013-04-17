/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <toku_portability.h>
#include <string.h>
#include <locktree.h>
#include <locktree-internal.h>
#include <db.h>
#include <ft/fttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <toku_assert.h>
#include <errno.h>
int verbose=0;
#include <lth.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory.h>
#include <ft/key.h>


bool want_panic = false;

static inline int intcmp(DB *db __attribute__((__unused__)), const DBT* a, const DBT* b) {
    int x = *(int*)a->data;
    int y = *(int*)b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}


static inline int dbcmp (DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

bool panicked = false;

static inline int dbpanic(DB* db, int r) {
    if (verbose) printf("AHH!!!! %d is rampaging! Run away %p!!!\n", r, db);
    panicked = true;
    assert(want_panic);
    return EINVAL;
}


#define CKERR(r) { if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, strerror(r)); assert(r==0); }
#define CKERR2(r,r2) { if (r!=r2) fprintf(stderr, "%s:%d error %d %s, expected %d\n", __FILE__, __LINE__, r, strerror(r), r2); assert(r==r2); }

static inline void parse_args (int argc, const char *argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1], "-q")==0) {
	    verbose=0;
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-h-q]\n", argv0);
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


static inline DBT *dbt_init(DBT *dbt, const void *data, uint32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = (void *) data;  // this is unsafe, but just for tests
                                // which like to use string literals
    dbt->size = size;
    return dbt;
}


/**
   A comparison function between toku_point's.
   It is implemented as a wrapper of db compare functions,
   but it checks whether the point is +/- infty.
   Parameters are of type toku_point.
   Return values conform to cmp from qsort(3).
 */
// extern int toku_lt_point_cmp(void* a, void* b);

static inline void init_point(toku_point* point, toku_lock_tree* tree) {
    assert(point && tree);
    memset(point, 0, sizeof(toku_point));
    point->lt = tree;
}

#define READ_REQUEST(TXN, KEY) \
    toku_lock_request TXN ## _r_ ## KEY; \
    toku_lock_request_init(&TXN ## _r_ ## KEY, txn_ ## TXN, &key_ ## KEY, &key_ ## KEY, LOCK_REQUEST_READ);
#define WRITE_REQUEST(TXN, KEY) \
    toku_lock_request TXN ## _w_ ## KEY; \
    toku_lock_request_init(&TXN ## _w_ ## KEY, txn_ ## TXN, &key_ ## KEY, &key_ ## KEY, LOCK_REQUEST_WRITE)

static inline void
verify_txnid_set_sorted(txnid_set *txns) {
    size_t n = txnid_set_size(txns);
    for (size_t i = 1; i < n; i++)
        assert(txnid_set_get(txns, i) > txnid_set_get(txns, i-1));
}

static inline void
verify_and_clean_finished_request(toku_lock_tree *lt, toku_lock_request *request) {
    int r;
    txnid_set conflicts; 

    assert(request->state == LOCK_REQUEST_COMPLETE);
    assert(request->complete_r == 0);

    txnid_set_init(&conflicts);
    r = toku_lt_get_lock_request_conflicts(lt, request, &conflicts);
    assert(r == 0);
    assert(txnid_set_size(&conflicts) == 0);
    txnid_set_destroy(&conflicts);

    toku_lock_request_destroy(request);
}

static inline void
do_request_and_succeed(toku_lock_tree *lt, toku_lock_request *request) {
    int r;
    r = toku_lock_request_start(request, lt, false);
    CKERR(r);
    verify_and_clean_finished_request(lt, request);
}

static inline void
request_still_blocked(
        toku_lock_tree *lt,
        toku_lock_request *request,
        size_t num_conflicts,
        TXNID conflicting_txns[/*num_conflicts*/]) {
    int r;
    txnid_set conflicts; 

    assert(request->state == LOCK_REQUEST_PENDING);

    txnid_set_init(&conflicts);
    r = toku_lt_get_lock_request_conflicts(lt, request, &conflicts);
    CKERR(r);
    assert(txnid_set_size(&conflicts) == num_conflicts);
    verify_txnid_set_sorted(&conflicts);

    size_t i;
    for (i = 0; i < num_conflicts; i++) {
        assert(txnid_set_get(&conflicts, i) == conflicting_txns[i]);
    }
    txnid_set_destroy(&conflicts);
}

static inline void
do_request_that_blocks(
        toku_lock_tree *lt,
        toku_lock_request *request,
        int num_conflicts,
        TXNID conflicting_txns[/*num_conflicts*/]) {
    int r;

    r = toku_lock_request_start(request, lt, false);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    request_still_blocked(lt, request, num_conflicts, conflicting_txns);
}

