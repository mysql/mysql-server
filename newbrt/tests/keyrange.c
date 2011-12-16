/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2008-2011 Tokutek Inc.  All rights reserved."

// Test keyrange

#include "includes.h"
#include "test.h"

#include <unistd.h>

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static char fname[]= __FILE__ ".brt";
static CACHETABLE ct;
static BRT t;

static void close_brt_and_ct (void) {
    int r;
    r = toku_close_brt(t, 0);          assert(r==0);
    r = toku_cachetable_close(&ct);    assert(r==0);
}

static void open_brt_and_ct (bool unlink_old) {
    int r;
    if (unlink_old) unlink(fname);
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);                                assert(r==0);
    r = toku_open_brt(fname, 1, &t, 1<<12, 1<<9, ct, null_txn, toku_builtin_compare_fun, null_db);   assert(r==0);
}

static void close_and_reopen (void) {
    close_brt_and_ct();
    open_brt_and_ct(false);
}

static void reload (u_int64_t limit) {
    // insert keys 1, 3, 5, ...
    for (u_int64_t i=0; i<limit; i++) {
	char key[100],val[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	snprintf(val, 100, "%08llu", (unsigned long long)2*i+1);
	brt_lookup_and_check_nodup(t, key, val);
    }
}

enum memory_state {
    LEAVE_IN_MEMORY,       // leave the state in main memory
    CLOSE_AND_RELOAD,      // close the brts and reload them into main memory (that will cause >1 partitio in many leaves.)
    CLOSE_AND_REOPEN_LEAVE_ON_DISK   // close the brts, reopen them, but leave the state on disk.
};

static void maybe_reopen (enum memory_state ms, u_int64_t limit) {
    switch (ms) {
    case CLOSE_AND_RELOAD:
	close_and_reopen();
	reload(limit);
	return;
    case CLOSE_AND_REOPEN_LEAVE_ON_DISK:
	close_and_reopen();
	return;
    case LEAVE_IN_MEMORY:
	return;
    }
    assert(0);
}

static void test_keyrange (enum memory_state ms, u_int64_t limit) {
    open_brt_and_ct(true);

    // insert keys 1, 3, 5, ...
    for (u_int64_t i=0; i<limit; i++) {
	char key[100],val[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	snprintf(val, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k,v;
	int r = toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v,val, 1+strlen(val)), null_txn);
	assert(r == 0);
    }

    {
        struct brtstat64_s s;
        int r = toku_brt_stat64(t, null_txn, &s); assert(r == 0);
        
        assert(0 < s.nkeys && s.nkeys < limit);
        assert(0 < s.dsize && s.dsize < limit * (9 + 9)); // keylen = 9, vallen = 9
    }
    
    maybe_reopen(ms, limit);

    {
	u_int64_t prev_less = 0, prev_greater = 1LL << 60; 
	u_int64_t count_less_adjacent = 0, count_greater_adjacent = 0; // count the number of times that the next value is 1 more (less) than the previous.
	u_int64_t equal_count = 0;

        // lookup keys 1, 3, 5, ...
	for (u_int64_t i=0; i<limit; i++) {
	    char key[100];
	    snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	    DBT k;
	    u_int64_t less,equal,greater;
	    int r = toku_brt_keyrange(t, toku_fill_dbt(&k, key, 1+strlen(key)), &less, &equal, &greater);
	    assert(r == 0);
	    if (verbose > 1) 
                printf("Pkey %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i+1, (unsigned long long)2*limit, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);

            assert(0 < less + equal + greater);
            assert(less + equal + greater <= 2 * limit);
            assert(equal == 0 || equal == 1);

	    // It's an estimate, and the values don't even change monotonically.
	    // And all the leaves are in main memory so it's always present.
	    if (ms!=CLOSE_AND_REOPEN_LEAVE_ON_DISK) {
		if (equal==1) equal_count++;
#if 0
		// The first few items are exact for less.
		if (i<70) {
		    assert(less==i);
		}
		// The last few items are exact for greater.
		if (limit-i<70) {
		    assert(greater<=limit-i-1);
		}
#endif
	    } else {
		// after reopen, none of the basements are in memory
		assert(equal == 0);
#if 0
		if (i<10) {
		    assert(less==0);
		}
		if (limit-i<10) {
		    assert(greater==0);
		}
#endif
	    }
	    // Count the number of times that prev_less is 1 less than less.
	    if (prev_less+1 == less) {
		count_less_adjacent++;
	    }
	    if (prev_greater-1 == greater) {
		count_greater_adjacent++;
	    }
	    // the best we can do:  It's an estimate.  At least in the current implementation for this test (which has small rows)
	    // the estimate grows monotonically as the leaf grows.
	    prev_less = less;
	    prev_greater = greater;
	}
	if (ms!=CLOSE_AND_REOPEN_LEAVE_ON_DISK) {
	    // If we were doing the in-memory case then most keys are adjacent.
	    assert(count_less_adjacent >= 0.9 * limit); // we expect at least 90% to be right.
	    assert(count_greater_adjacent >= 0.9 * limit); // we expect at least 90% to be right.
	    assert(equal_count >= 0.9 * limit);
	}
    }

    maybe_reopen(ms, limit);

    // lookup keys 0, 2, 4, ... not in the tree
    for (u_int64_t i=0; i<1+limit; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i);
	DBT k;
	u_int64_t less,equal,greater;
	int r = toku_brt_keyrange(t, toku_fill_dbt(&k, key, 1+strlen(key)), &less, &equal, &greater);
	assert(r == 0);
        if (verbose > 1)
            printf("Akey %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*limit, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);

        assert(0 < less + equal + greater);
        assert(less + equal + greater <= 2 * limit);
	assert(equal == 0);
#if 0
	// The first few items are exact (looking a key that's not there)
	if (ms!=CLOSE_AND_REOPEN_LEAVE_ON_DISK) {
	    if (i<70) {
		assert(less==i);
	    }
	    // The last few items are exact (looking up a key that's not there)
	    if (limit-i<70) {
		assert(greater<=limit-i);
	    }
	} else {
	    if (i<10) {
		assert(less==0);
	    }
	    if (limit-i<10) {
		assert(greater==0);
	    }
	}
#endif
    }

    close_brt_and_ct();
}

int
test_main (int argc , const char *argv[]) {
    u_int64_t limit = 30000;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            limit = atoll(argv[++i]);
            continue;
        }
    }

    test_keyrange(LEAVE_IN_MEMORY, limit);
    test_keyrange(CLOSE_AND_REOPEN_LEAVE_ON_DISK, limit);
    test_keyrange(CLOSE_AND_RELOAD, limit);

    if (verbose) printf("test ok\n");
    return 0;
}

