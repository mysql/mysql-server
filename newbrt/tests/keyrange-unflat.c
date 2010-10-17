/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

#include <unistd.h>

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void test_flat (u_int64_t limit) {
    char fname[]= __FILE__ ".brt";
    u_int64_t permute[limit];
    unlink(fname);
    CACHETABLE ct;
    // set the cachetable to size 1 so that things won't fit.
    int r = toku_brt_create_cachetable(&ct, 1, ZERO_LSN, NULL_LOGGER);                                assert(r==0);
    BRT t;
    r = toku_open_brt(fname, 1, &t, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db);   assert(r==0);
    u_int64_t i;
    // permute the numbers from 0 (inclusive) to limit (exclusive)
    time_t now;
    now = time(0); fprintf(stderr, "%.24s permute\n", ctime(&now));
    permute[0]=0;
    for (i=1; i<limit; i++) {
	permute[i]=i;
	int ra = random()%(i+1);
	permute[i]=permute[ra];
	permute[ra]=i;
    }
    now = time(0); fprintf(stderr, "%.24s insert\n", ctime(&now));
    for (i=0; i<limit; i++) {
        if (verbose) printf("%s:%d %"PRIu64"\n", __FUNCTION__, __LINE__, i);
	char key[100],val[100];
	u_int64_t ri = permute[i];
	snprintf(key, 100, "%08llu", (unsigned long long)2*ri+1);
	snprintf(val, 100, "%08llu", (unsigned long long)2*ri+1);
	DBT k,v;
	r = toku_brt_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v,val, 1+strlen(val)), null_txn);
	assert(r == 0);
    }
    // Don't flatten it.
    // search for the items that are there and those that aren't.
    // 
    now = time(0); fprintf(stderr, "%.24s keyrange\n", ctime(&now));
    u_int64_t prevless=0;
    u_int64_t prevgreater=limit;
    for (i=0; i<2*limit+1; i++) {
        if (verbose) printf("%s:%d %"PRIu64"\n", __FUNCTION__, __LINE__, i);
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)i);
	DBT k;
	u_int64_t less,equal,greater;
	r = toku_brt_keyrange(t, toku_fill_dbt(&k, key, 1+strlen(key)), &less, &equal, &greater);
	assert(r == 0);
	//printf("key %llu/%llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*limit, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater);
	assert(equal==0 || equal==1);
	assert(less>=prevless);	      prevless    = less;
	assert(greater<=prevgreater); prevgreater = greater;
    }
    now = time(0); fprintf(stderr, "%.24s done\n", ctime(&now));
    r = toku_close_brt(t, 0);          assert(r==0);
    r = toku_cachetable_close(&ct);    assert(r==0);
}

int
test_main (int argc , const char *argv[]) {
    u_int64_t limit = 10000;
#define DO_AFFINITY 1
#if DO_AFFINITY == 0
    default_parse_args(argc, argv);
#else
#include <sched.h>
    int ncpus = 0;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "--ncpus") == 0 && i+1 < argc) {
            ncpus = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--limit") == 0 && i+1 < argc) {
            limit = atoi(argv[++i]);
            continue;
        }
        break;
    }

    if (ncpus > 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int i = 0; i < ncpus; i++)
            CPU_SET(i, &cpuset);
        int r;
        r = sched_setaffinity(toku_os_getpid(), sizeof cpuset, &cpuset);
        assert(r == 0);
        
        cpu_set_t use_cpuset;
        CPU_ZERO(&use_cpuset);
        r = sched_getaffinity(toku_os_getpid(), sizeof use_cpuset, &use_cpuset);
        assert(r == 0);
        assert(memcmp(&cpuset, &use_cpuset, sizeof cpuset) == 0);
    }
#endif

    test_flat(limit);
    
    if (verbose) printf("test ok\n");
    return 0;
}

