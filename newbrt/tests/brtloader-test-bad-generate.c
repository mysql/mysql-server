/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// The purpose of this test is force errors returned from the generate function 

#define DONT_DEPRECATE_MALLOC
#define DONT_DEPRECATE_WRITES
#include "test.h"
#include "brtloader.h"
#include "brtloader-internal.h"
#include "brtloader-error-injector.h"
#include "memory.h"

#if defined(__cplusplus)
extern "C" {
#endif

static void copy_dbt(DBT *dest, const DBT *src) {
    assert(dest->flags & DB_DBT_REALLOC);
    dest->data = toku_realloc(dest->data, src->size);
    dest->size = src->size;
    memcpy(dest->data, src->data, src->size);
}

static int generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val) {
    if (verbose) printf("%s %p %p %p %p %p %p\n", __FUNCTION__, dest_db, src_db, dest_key, dest_val, src_key, src_val);

    assert(dest_db == NULL); assert(src_db == NULL);

    int result;
    if (event_count_trigger == event_add_and_fetch()) {
        event_hit();
        result = EINVAL;
    } else {
        copy_dbt(dest_key, src_key);
        copy_dbt(dest_val, src_val);
        result = 0;
    }

    if (verbose) printf("%s %d\n", __FUNCTION__, result);
    return result;
}

static int qsort_compare_ints (const void *a, const void *b) {
    int avalue = *(int*)a;
    int bvalue = *(int*)b;
    if (avalue<bvalue) return -1;
    if (avalue>bvalue) return +1;
    return 0;
}

static int compare_int(DB *desc, const DBT *akey, const DBT *bkey) {
    assert(desc == NULL);
    assert(akey->size == sizeof (int));
    assert(bkey->size == sizeof (int));
    return qsort_compare_ints(akey->data, bkey->data);
}

static void populate_rowset(struct rowset *rowset, int seq, int nrows) {
    for (int i = 0; i < nrows; i++) {
        int k = seq * nrows + i;
        int v = seq * nrows + i;
        DBT key = { .size = sizeof k, .data = &k };
        DBT val = { .size = sizeof v, .data = &v };
        add_row(rowset, &key, &val);
    }
}

static void test_extractor(int nrows, int nrowsets, BOOL expect_fail) {
    if (verbose) printf("%s %d %d\n", __FUNCTION__, nrows, nrowsets);

    int r;

    // open the brtloader. this runs the extractor.
    const int N = 1;
    BRT brts[N];
    const char *fnames[N];
    brt_compare_func compares[N];
    for (int i = 0; i < N; i++) {
        brts[i] = NULL;
        fnames[i] = "";
        compares[i] = compare_int;
    }

    BRTLOADER loader;
    r = toku_brt_loader_open(&loader, NULL, generate, NULL, N, brts, fnames, compares, "tempXXXXXX", ZERO_LSN, TXNID_NONE);
    assert(r == 0);

    struct rowset *rowset[nrowsets];
    for (int i = 0 ; i < nrowsets; i++) {
        rowset[i] = (struct rowset *) toku_malloc(sizeof (struct rowset));
        assert(rowset[i]);
        init_rowset(rowset[i], toku_brtloader_get_rowset_budget_for_testing());
        populate_rowset(rowset[i], i, nrows);
    }

    // feed rowsets to the extractor
    for (int i = 0; i < nrowsets; i++) {
        r = queue_enq(loader->primary_rowset_queue, rowset[i], 1, NULL);
        assert(r == 0);
    }

    r = toku_brt_loader_finish_extractor(loader);
    assert(r == 0);
    
    int loader_error;
    r = toku_brt_loader_get_error(loader, &loader_error);
    assert(r == 0);

    assert(expect_fail ? loader_error != 0 : loader_error == 0);

    // abort the brtloader.  this ends the test
    r = toku_brt_loader_abort(loader, TRUE);
    assert(r == 0);
}

static int nrows = 1;
static int nrowsets = 2;

static int usage(const char *progname) {
    fprintf(stderr, "Usage:\n %s [-h] [-v] [-q] [-s] [-r %d] [--rowsets %d]\n", progname, nrows, nrowsets);
    return 1;
}

int test_main (int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0],"-h")==0) {
            return usage(progname);
        } else if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
        } else if (strcmp(argv[0],"-r") == 0 && argc >= 1) {
            argc--; argv++;
            nrows = atoi(argv[0]);
        } else if (strcmp(argv[0],"--nrowsets") == 0 && argc >= 1) {
            argc--; argv++;
            nrowsets = atoi(argv[0]);
        } else if (strcmp(argv[0],"-s") == 0) {
            toku_brtloader_set_size_factor(1);
	} else if (argc!=1) {
            return usage(progname);
	    exit(1);
	}
        else {
            break;
        }
	argc--; argv++;
    }

    // callibrate
    test_extractor(nrows, nrowsets, FALSE);

    // run tests
    int event_limit = event_count;
    if (verbose) printf("event_limit=%d\n", event_limit);

    for (int i = 1; i <= event_limit; i++) {
        reset_event_counts();
        event_count_trigger = i;
        test_extractor(nrows, nrowsets, TRUE);
    }

    return 0;
}

#if defined(__cplusplus)
}
#endif
