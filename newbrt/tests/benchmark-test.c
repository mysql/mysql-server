/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

/* Insert a bunch of stuff */
#include "includes.h"
#include "toku_time.h"

static const char fname[]="sinsert.brt";

enum { SERIAL_SPACING = 1<<6 };
enum { ITEMS_TO_INSERT_PER_ITERATION = 1<<20 };
//enum { ITEMS_TO_INSERT_PER_ITERATION = 1<<14 };
enum { BOUND_INCREASE_PER_ITERATION = SERIAL_SPACING*ITEMS_TO_INSERT_PER_ITERATION };

enum { NODE_SIZE = 1<<20 };

static int nodesize = NODE_SIZE;
static int keysize = sizeof (long long);
static int valsize = sizeof (long long);
static int do_verify =0; /* Do a slow verify after every insert. */

static int do_serial = 1;
static int do_random = 1;

static CACHETABLE ct;
static BRT t;

static void setup (void) {
    int r;
    unlink(fname);
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);         assert(r==0);
    r = toku_open_brt(fname, 1, &t, nodesize, ct, NULL_TXN, toku_builtin_compare_fun, (DB*)0); assert(r==0);
}

static void toku_shutdown (void) {
    int r;
    r = toku_close_brt(t, 0); assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
}
static void long_long_to_array (unsigned char *a, unsigned long long l) {
    int i;
    for (i=0; i<8; i++)
	a[i] = (l>>(56-8*i))&0xff;
}

static void insert (long long v) {
    unsigned char kc[keysize], vc[valsize];
    DBT  kt, vt;
    memset(kc, 0, sizeof kc);
    long_long_to_array(kc, v);
    memset(vc, 0, sizeof vc);
    long_long_to_array(vc, v);
    int r = toku_brt_insert(t, toku_fill_dbt(&kt, kc, keysize), toku_fill_dbt(&vt, vc, valsize), 0);
    CKERR(r);
    if (do_verify) toku_cachetable_verify(ct);
}

static void serial_insert_from (long long from) {
    long long i;
    for (i=0; i<ITEMS_TO_INSERT_PER_ITERATION; i++) {
	insert((from+i)*SERIAL_SPACING);
    }
}

static long long llrandom (void) {
    return (((long long)(random()))<<32) + random();
}

static void random_insert_below (long long below) {
    long long i;
    assert(0 < below);
    for (i=0; i<ITEMS_TO_INSERT_PER_ITERATION; i++) {
	insert(llrandom()%below);
    }
}

static void biginsert (long long n_elements, struct timeval *starttime) {
    long long i;
    struct timeval t1,t2;
    int iteration;
    for (i=0, iteration=0; i<n_elements; i+=ITEMS_TO_INSERT_PER_ITERATION, iteration++) {
	gettimeofday(&t1,0);
	if (do_serial)
            serial_insert_from(i);
	gettimeofday(&t2,0);
	if (verbose && do_serial) {
	    printf("serial %9.6fs %8.0f/s    ", toku_tdiff(&t2, &t1), ITEMS_TO_INSERT_PER_ITERATION/toku_tdiff(&t2, &t1));
	    fflush(stdout);
	}
	gettimeofday(&t1,0);
        if (do_random)
            random_insert_below((i+ITEMS_TO_INSERT_PER_ITERATION)*SERIAL_SPACING);
	gettimeofday(&t2,0);
	if (verbose && do_random) {
	    printf("random %9.6fs %8.0f/s    ", toku_tdiff(&t2, &t1), ITEMS_TO_INSERT_PER_ITERATION/toku_tdiff(&t2, &t1));
	    fflush(stdout);
        }
        if (verbose && (do_serial || do_random)) {
            double f = 0;
            if (do_serial) f += 1.0;
            if (do_random) f += 1.0;
	    printf("cumulative %9.6fs %8.0f/s\n", toku_tdiff(&t2, starttime), (ITEMS_TO_INSERT_PER_ITERATION*f/toku_tdiff(&t2, starttime))*(iteration+1));
	    fflush(stdout);
	}
    }
}

static void usage(void) {
    printf("benchmark-test [OPTIONS] [ITERATIONS]\n");
    printf("[-v]\n");
    printf("[-q]\n");
    printf("[--nodesize NODESIZE]\n");
    printf("[--keysize KEYSIZE]\n");
    printf("[--valsize VALSIZE]\n");
    printf("[--noserial]\n");
    printf("[--norandom]\n");
    printf("[--verify]\n");
}

int
test_main (int argc, const char *argv[]) {
    verbose=1; //Default
    /* parse parameters */
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (arg[0] != '-')
            break;
        if (strcmp(arg, "--nodesize") == 0) {
            if (i+1 < argc) {
                i++;
                nodesize = atoi(argv[i]);
            }
        } else if (strcmp(arg, "--keysize") == 0) {
            if (i+1 < argc) {
                i++;
                keysize = atoi(argv[i]);
            }
        } else if (strcmp(arg, "--valsize") == 0) {
            if (i+1 < argc) {
                i++;
                valsize = atoi(argv[i]);
            }
        } else if (strcmp(arg, "--verify")==0) {
	    do_verify = 1;
        } else if (strcmp(arg, "--noserial") == 0) {
            do_serial = 0;
        } else if (strcmp(arg, "--norandom") == 0) {
            do_random = 0;
	} else if (strcmp(arg, "-v")==0) {
	    verbose++;
	} else if (strcmp(arg, "-q")==0) {
	    verbose = 0;
	} else {
	    usage();
	    return 1;
	}
    }

    struct timeval t1,t2,t3;
    long long total_n_items;
    if (i < argc) {
	char *end;
	errno=0;
	total_n_items = ITEMS_TO_INSERT_PER_ITERATION * (long long) strtol(argv[i], &end, 10);
	assert(errno==0);
	assert(*end==0);
	assert(end!=argv[i]);
    } else {
	total_n_items = 1LL<<22; // 1LL<<16
    }

    if (verbose) {
	printf("nodesize=%d\n", nodesize);
	printf("keysize=%d\n", keysize);
	printf("valsize=%d\n", valsize);
	printf("Serial and random insertions of %d per batch\n", ITEMS_TO_INSERT_PER_ITERATION);
        fflush(stdout);
    }
    setup();
    gettimeofday(&t1,0);
    biginsert(total_n_items, &t1);
    gettimeofday(&t2,0);
    toku_shutdown();
    gettimeofday(&t3,0);
    if (verbose) {
        int f = 0;
        if (do_serial) f += 1;
        if (do_random) f += 1;
	printf("Shutdown %9.6fs\n", toku_tdiff(&t3, &t2));
	printf("Total time %9.6fs for %lld insertions = %8.0f/s\n", toku_tdiff(&t3, &t1), f*total_n_items, f*total_n_items/toku_tdiff(&t3, &t1));
        fflush(stdout);
    }
    unlink(fname);
    if (verbose>1) {
	toku_malloc_report();
    }
    toku_malloc_cleanup();
    return 0;
}

