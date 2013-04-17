/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2008-2011 Tokutek Inc.  All rights reserved."

// Test keyrange


#include "test.h"

#include <unistd.h>

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static const char *fname = TOKU_TEST_FILENAME;
static CACHETABLE ct;
static FT_HANDLE t;

static void close_ft_and_ct (void) {
    int r;
    r = toku_close_ft_handle_nolsn(t, 0);          assert(r==0);
    toku_cachetable_close(&ct);
}

static void open_ft_and_ct (bool unlink_old) {
    int r;
    if (unlink_old) unlink(fname);
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(fname, 1, &t, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);   assert(r==0);
}

static void close_and_reopen (void) {
    close_ft_and_ct();
    open_ft_and_ct(false);
}

static void reload (uint64_t limit) {
    // insert keys 1, 3, 5, ...
    for (uint64_t i=0; i<limit; i++) {
	char key[100],val[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	snprintf(val, 100, "%08llu", (unsigned long long)2*i+1);
	ft_lookup_and_check_nodup(t, key, val);
    }
}

enum memory_state {
    LEAVE_IN_MEMORY,       // leave the state in main memory
    CLOSE_AND_RELOAD,      // close the brts and reload them into main memory (that will cause >1 partitio in many leaves.)
    CLOSE_AND_REOPEN_LEAVE_ON_DISK   // close the brts, reopen them, but leave the state on disk.
};

static void maybe_reopen (enum memory_state ms, uint64_t limit) {
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

static void verify_keysrange(enum memory_state UU(ms), uint64_t limit,
        uint64_t intkey1,
        uint64_t intkey2,
        uint64_t less,
        uint64_t equal1,
        uint64_t middle,
        uint64_t equal2,
        uint64_t greater,
        bool middle3exact) {
    uint64_t max_item = limit * 2 - 1;
    uint64_t perfect_total = limit;
    uint64_t perfect_less = intkey1 / 2;
    uint64_t perfect_equal1 = intkey1 % 2 == 1;
    uint64_t perfect_equal2 = intkey2 % 2 == 1 && intkey2 <= max_item;
    uint64_t perfect_greater = intkey2 >= max_item ? 0 : (max_item + 1 - intkey2) / 2;
    uint64_t perfect_middle = perfect_total - perfect_less - perfect_equal1 - perfect_equal2 - perfect_greater;

    uint64_t total = less + equal1 + middle + equal2 + greater;
    assert(total > 0);
    assert(total < 2 * perfect_total);
    assert(total > perfect_total / 2);

    assert(equal1 == perfect_equal1 || (equal1 == 0 && !middle3exact));
    assert(equal2 == perfect_equal2 || (equal2 == 0 && !middle3exact));

    // As of 2013-02-25 this is accurate with fiddle ~= total/50.
    // Set to 1/10th to prevent flakiness.
    uint64_t fiddle = perfect_total / 10;
    assert(less + fiddle > perfect_less);
    assert(less < perfect_less + fiddle);

    assert(middle + fiddle > perfect_middle);
    assert(middle < perfect_middle + fiddle);

    assert(greater + fiddle > perfect_greater);
    assert(greater < perfect_greater + fiddle);

    if (middle3exact) {
        assert(middle == perfect_middle);
    }
}


static void test_keyrange (enum memory_state ms, uint64_t limit) {
    open_ft_and_ct(true);

    // insert keys 1, 3, 5, ...
    for (uint64_t i=0; i<limit; i++) {
	char key[100],val[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	snprintf(val, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k,v;
	toku_ft_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v,val, 1+strlen(val)), null_txn);
    }

    {
        struct ftstat64_s s;
        toku_ft_handle_stat64(t, null_txn, &s);
        
        assert(0 < s.nkeys && s.nkeys <= limit);
        assert(0 < s.dsize && s.dsize <= limit * (9 + 9)); // keylen = 9, vallen = 9
    }
    
    maybe_reopen(ms, limit);

    {
	uint64_t prev_less = 0, prev_greater = 1LL << 60; 
	uint64_t count_less_adjacent = 0, count_greater_adjacent = 0; // count the number of times that the next value is 1 more (less) than the previous.
	uint64_t equal_count = 0;

        // lookup keys 1, 3, 5, ...
	for (uint64_t i=0; i<limit; i++) {
	    char key[100];
	    snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	    DBT k;
	    uint64_t less,equal,greater;
	    toku_ft_keyrange(t, toku_fill_dbt(&k, key, 1+strlen(key)), &less, &equal, &greater);
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
		// However, "both" keys can be in the same basement (specifically the last basement node in the tree)
                // Without trying to figure out how many are in the last basement node, we expect at least the first half not to be in the last basement node.
                assert(i > limit / 2 || equal == 0);
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
    for (uint64_t i=0; i<1+limit; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i);
	DBT k;
	uint64_t less,equal,greater;
	toku_ft_keyrange(t, toku_fill_dbt(&k, key, 1+strlen(key)), &less, &equal, &greater);
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

    maybe_reopen(ms, limit);

    {
        uint64_t totalqueries = 0;
        uint64_t num_middle3_exact = 0;
        for (uint64_t i=0; i < 2*limit; i++) {
	    char key[100];
	    char keyplus4[100];
	    char keyplus5[100];
            uint64_t intkey = i;

	    snprintf(key, 100, "%08" PRIu64 "", intkey);
	    snprintf(keyplus4, 100, "%08" PRIu64 "", intkey+4);
	    snprintf(keyplus5, 100, "%08" PRIu64 "", intkey+5);

	    DBT k;
	    DBT k2;
	    DBT k3;
            toku_fill_dbt(&k, key, 1+strlen(key));
            toku_fill_dbt(&k2, keyplus4, 1+strlen(keyplus4));
            toku_fill_dbt(&k3, keyplus5, 1+strlen(keyplus5));
	    uint64_t less,equal1,middle,equal2,greater;
            bool middle3exact;
	    toku_ft_keysrange(t, &k, &k2, &less, &equal1, &middle, &equal2, &greater, &middle3exact);
            if (ms == CLOSE_AND_REOPEN_LEAVE_ON_DISK) {
                //TODO(yoni): when reading basement nodes is implemented, get rid of this hack
                middle3exact = false;
            }
            totalqueries++;
            num_middle3_exact += middle3exact;
            if (verbose > 1) {
                printf("Rkey2 %" PRIu64 "/%" PRIu64
                       " %" PRIu64
                       " %" PRIu64
                       " %" PRIu64
                       " %" PRIu64
                       " %" PRIu64
                       " %s\n",
                       intkey, 2*limit, less, equal1, middle, equal2, greater, middle3exact ? "true" : "false");
            }
            verify_keysrange(ms, limit, intkey, intkey+4,
                    less, equal1, middle, equal2, greater, middle3exact);

	    toku_ft_keysrange(t, &k, &k3, &less, &equal1, &middle, &equal2, &greater, &middle3exact);
            if (ms == CLOSE_AND_REOPEN_LEAVE_ON_DISK) {
                //TODO(yoni): when reading basement nodes is implemented, get rid of this hack
                middle3exact = false;
            }
            totalqueries++;
            num_middle3_exact += middle3exact;
            if (verbose > 1) {
                printf("Rkey3 %" PRIu64 "/%" PRIu64
                       " %" PRIu64
                       " %" PRIu64
                       " %" PRIu64
                       " %" PRIu64
                       " %" PRIu64
                       " %s\n",
                       intkey, 2*limit, less, equal1, middle, equal2, greater, middle3exact ? "true" : "false");
            }
            verify_keysrange(ms, limit, intkey, intkey+5,
                    less, equal1, middle, equal2, greater, middle3exact);
        }
        assert(num_middle3_exact <= totalqueries);
        if (ms == CLOSE_AND_REOPEN_LEAVE_ON_DISK) {
            //TODO(yoni): when reading basement nodes is implemented, get rid of this hack
            assert(num_middle3_exact == 0);
        } else {
            // About 85% of the time, the key for an int (and +4 or +5) is in the
            // same basement node.  Check >= 70% so this isn't very flaky.
            assert(num_middle3_exact > totalqueries * 7 / 10);
        }
    }

    close_ft_and_ct();
}

int
test_main (int argc , const char *argv[]) {
    uint64_t limit = 30000;

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

