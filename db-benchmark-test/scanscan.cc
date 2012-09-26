/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* Scan the bench.tokudb/bench.db over and over. */
#define DONT_DEPRECATE_MALLOC

#include <toku_portability.h>
#include "tokudb_common_funcs.h"
#include <toku_assert.h>
#include <db.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef TOKUDB
#include <ft/key.h>
#include <ft/cachetable.h>
#endif

static const char *pname;
static enum run_mode { RUN_HWC, RUN_LWC, RUN_VERIFY, RUN_RANGE} run_mode = RUN_HWC;
static int do_txns=1, prelock=0, prelockflag=0;
static int cleaner_period=0, cleaner_iterations=0;
static uint32_t lock_flag = 0;
static long limitcount=-1;
static uint32_t cachesize = 127*1024*1024;
static uint64_t start_range = 0, end_range = 0;
static int n_experiments = 2;
static int bulk_fetch = 1;

static int verbose = 0;
static int engine_status = 0;
static const char *log_dir = NULL;


static int print_usage (const char *argv0) {
    fprintf(stderr, "Usage:\n%s [--verify-lwc | --lwc | --nohwc] [--prelock] [--prelockflag] [--prelockwriteflag] [--env DIR] [--verbose]\n", argv0);
    fprintf(stderr, "  --verify-lwc             means to run the light weight cursor and the heavyweight cursor to verify that they get the same answer.\n");
    fprintf(stderr, "  --lwc                    run light weight cursors instead of heavy weight cursors\n");
    fprintf(stderr, "  --prelock                acquire a read lock on the entire table before running\n");
    fprintf(stderr, "  --prelockflag            pass DB_PRELOCKED to the the cursor get operation whenever the locks have been acquired\n");
    fprintf(stderr, "  --prelockwriteflag       pass DB_PRELOCKED_WRITE to the cursor get operation\n");
    fprintf(stderr, "  --nox                    no transactions (no locking)\n");
    fprintf(stderr, "  --count <count>          read the first COUNT rows and then  stop.\n");
    fprintf(stderr, "  --cachesize <n>          set the env cachesize to <n>\n");
    fprintf(stderr, "  --cleaner-period <n>     set the cleaner period to <n>\n");
    fprintf(stderr, "  --cleaner-iterations <n> set the cleaner iterations to <n>\n");
    fprintf(stderr, "  --env DIR                put db files in DIR instead of default\n");
    fprintf(stderr, "  --log_dir LOGDIR         put the logs in LOGDIR\n");
    fprintf(stderr, "  --range LOW HIGH         set the LOW and HIGH key boundaries in which random range queries are made\n");
    fprintf(stderr, "  --experiments N          run N experiments (default:%d)\n", n_experiments);
    fprintf(stderr, "  --srandom N              srandom(N)\n");
    fprintf(stderr, "  --recover                run recovery\n");
    fprintf(stderr, "  --verbose                print verbose information\n");
    fprintf(stderr, "  --bulk_fetch 0|1         do bulk fetch on lwc operations (default: 1)\n");
    fprintf(stderr, "  --engine_status          print engine status at end of test \n");
    return 1;
}

static DB_ENV *env;
static DB *db;
static DB_TXN *tid = NULL;

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)
static const char *dbdir = "./bench."  STRINGIFY(DIRSUF); /* DIRSUF is passed in as a -D argument to the compiler. */
static int env_open_flags_yesx = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOG|DB_INIT_LOCK;
static int env_open_flags_nox = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;
static const char *dbfilename = "bench.db";

static double gettime (void) {
    struct timeval tv;
    int r = gettimeofday(&tv, 0);
    assert(r==0);
    return tv.tv_sec + 1e-6*tv.tv_usec;
}

static void parse_args (int argc, char *const argv[]) {
    pname=argv[0];
    argc--; argv++;
    int specified_run_mode=0;
    while (argc>0) {
        if (strcmp(*argv,"--verbose")==0) {
            verbose++;
        } else if (strcmp(*argv,"--verify-lwc")==0) {
	    if (specified_run_mode && run_mode!=RUN_VERIFY) { two_modes: fprintf(stderr, "You specified two run modes\n"); exit(1); }
	    run_mode = RUN_VERIFY;
	} else if (strcmp(*argv, "--lwc")==0)  {
	    if (specified_run_mode && run_mode!=RUN_LWC) goto two_modes;
	    run_mode = RUN_LWC;
	} else if (strcmp(*argv, "--hwc")==0)  {
	    if (specified_run_mode && run_mode!=RUN_VERIFY) goto two_modes;
	    run_mode = RUN_HWC;
	} else if (strcmp(*argv, "--prelock")==0) prelock=1;
#ifdef TOKUDB
        else if (strcmp(*argv, "--prelockflag")==0)      { prelockflag=1; lock_flag = DB_PRELOCKED; }
        else if (strcmp(*argv, "--prelockwriteflag")==0) { prelockflag=1; lock_flag = DB_PRELOCKED_WRITE; }
#endif
	else if (strcmp(*argv, "--nox")==0)              { do_txns=0; }
	else if (strcmp(*argv, "--count")==0)            {
	    char *end;
            argc--; argv++; 
	    errno=0; limitcount=strtol(*argv, &end, 10); assert(errno==0);
	    printf("Limiting count to %ld\n", limitcount);
        } else if (strcmp(*argv, "--cachesize")==0 && argc>0) {
            char *end;
            argc--; argv++; 
            cachesize=(uint32_t)strtol(*argv, &end, 10);
        } else if (strcmp(*argv, "--cleaner-period")==0 && argc>0) {
            char *end;
            argc--; argv++; 
            cleaner_period=(uint32_t)strtol(*argv, &end, 10);
        } else if (strcmp(*argv, "--cleaner-iterations")==0 && argc>0) {
            char *end;
            argc--; argv++; 
            cleaner_iterations=(uint32_t)strtol(*argv, &end, 10);
	} else if (strcmp(*argv, "--env") == 0) {
            argc--; argv++;
	    if (argc==0) exit(print_usage(pname));
	    dbdir = *argv;
	} else if (strcmp(*argv, "--log_dir") == 0) {
            argc--; argv++;
	    if (argc==0) exit(print_usage(pname));
	    log_dir = *argv;
        } else if (strcmp(*argv, "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(*argv, "--range") == 0 && argc > 2) {
            run_mode = RUN_RANGE;
            argc--; argv++;
            start_range = strtoll(*argv, NULL, 10);
            argc--; argv++;
            end_range = strtoll(*argv, NULL, 10);
        } else if (strcmp(*argv, "--experiments") == 0 && argc > 1) {
            argc--; argv++;
            n_experiments = strtol(*argv, NULL, 10);
        } else if (strcmp(*argv, "--srandom") == 0 && argc > 1) {
	    argc--; argv++;
            srandom(atoi(*argv));
        } else if (strcmp(*argv, "--recover") == 0) {
            env_open_flags_yesx |= DB_RECOVER;
            env_open_flags_nox |= DB_RECOVER;
        } else if (strcmp(*argv, "--bulk_fetch") == 0 && argc > 1) {
            argc--; argv++;
            bulk_fetch = atoi(*argv);
        } else if (strcmp(*argv, "--engine_status") == 0) {
            engine_status = 1;
	} else {
            exit(print_usage(pname));
	}
	argc--; argv++;
    }
    //Prelocking is meaningless without transactions
    if (do_txns==0) {
      //prelockflag=0;
        lock_flag=0;
        //prelock=0;
    }
}


static void scanscan_setup (void) {
    int r;
    r = db_env_create(&env, 0);                                                           assert(r==0);
    r = env->set_cachesize(env, 0, cachesize, 1);                                         assert(r==0);
    if (log_dir) {
        r = env->set_lg_dir(env, log_dir);                                                assert(r==0);
    }
    double tstart = gettime();
    r = env->open(env, dbdir, do_txns? env_open_flags_yesx : env_open_flags_nox, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);   assert(r==0);
    double tend = gettime();
    if (verbose)
        printf("env open %f seconds\n", tend-tstart);
#ifdef TOKUDB
    if (cleaner_period) {
        r = env->cleaner_set_period(env, cleaner_period); assert(r == 0);
    }
    if (cleaner_iterations) {
        r = env->cleaner_set_iterations(env, cleaner_iterations); assert(r == 0);
    }
#endif
    r = db_create(&db, env, 0);                                                           assert(r==0);
    if (do_txns) {
	r = env->txn_begin(env, 0, &tid, 0);                                              assert(r==0);
    }
    r = db->open(db, tid, dbfilename, NULL, DB_BTREE, 0, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);                           assert(r==0);
#ifdef TOKUDB
    if (prelock) {
	r = db->pre_acquire_table_lock(db,
				      tid);
	assert(r==0);
    }
#endif
}

static void scanscan_shutdown (void) {
    int r;
    r = db->close(db, 0);                                       assert(r==0);
    if (do_txns) {
	r = tid->commit(tid, 0);                                    assert(r==0);
    }
    if (verbose || engine_status)
	print_engine_status(env);
    r = env->close(env, 0);                                     assert(r==0);
    env = NULL;
}


static void scanscan_hwc (void) {
    int r;
    int counter=0;
    for (counter=0; counter<n_experiments; counter++) {
	long long totalbytes=0;
	int rowcounter=0;
	double prevtime = gettime();
	DBT k,v;
	DBC *dbc;
	r = db->cursor(db, tid, &dbc, 0);                           assert(r==0);
	memset(&k, 0, sizeof(k));
	memset(&v, 0, sizeof(v));
        uint32_t c_get_flags = DB_NEXT;
        if (prelockflag && (counter || prelock)) {
            c_get_flags |= lock_flag;
        }
	while (0 == (r = dbc->c_get(dbc, &k, &v, c_get_flags))) {
	    totalbytes += k.size + v.size;
	    rowcounter++;
	    if (limitcount>0 && rowcounter>=limitcount) break;
	}
	assert(r==DB_NOTFOUND); // complain about things like lock-not-found
	r = dbc->c_close(dbc);                                      assert(r==0);
	double thistime = gettime();
	double tdiff = thistime-prevtime;
	printf("Scan    %lld bytes (%d rows) in %9.6fs at %9fMB/s\n", totalbytes, rowcounter, tdiff, 1e-6*totalbytes/tdiff);
    }
}

#ifdef TOKUDB

struct extra_count {
    long long totalbytes;
    int rowcounter;
};

static int counttotalbytes (DBT const *key, DBT const *data, void *extrav) {
    struct extra_count *CAST_FROM_VOIDP(e, extrav);
    e->totalbytes += key->size + data->size;
    e->rowcounter++;
    return bulk_fetch ? TOKUDB_CURSOR_CONTINUE : 0;
}

static void scanscan_lwc (void) {
    int r;
    int counter=0;
    for (counter=0; counter<n_experiments; counter++) {
	struct extra_count e = {0,0};
	double prevtime = gettime();
	DBC *dbc;
	r = db->cursor(db, tid, &dbc, 0);                           assert(r==0);
	if(prelock) {
            r = dbc->c_pre_acquire_range_lock(dbc, db->dbt_neg_infty(), db->dbt_pos_infty()); assert(r==0);
	}
        uint32_t f_flags = 0;
        if (prelockflag && (counter || prelock)) {
            f_flags |= lock_flag;
        }
	long rowcounter=0;
	while (0 == (r = dbc->c_getf_next(dbc, f_flags, counttotalbytes, &e))) {
	    rowcounter++;
	    if (limitcount>0 && rowcounter>=limitcount) break;
	}
	//assert(r==DB_NOTFOUND);
	r = dbc->c_close(dbc);                                      assert(r==0);
	double thistime = gettime();
	double tdiff = thistime-prevtime;
	printf("LWC Scan %lld bytes (%d rows) in %9.6fs at %9fMB/s\n", e.totalbytes, e.rowcounter, tdiff, 1e-6*e.totalbytes/tdiff);
    }
}

#endif

static void scanscan_range (void) {
    int r;

    double texperiments[n_experiments];
    uint64_t k = 0;
    char kv[8];
    DBT key, val;
  
    for (int counter = 0; counter < n_experiments; counter++) {

        if (1) { //if ((counter&1) == 0) {   
   	makekey:
	    // generate a random key in the key range
	    k = (start_range + (random() % (end_range - start_range))) * (1<<6);
	    for (int i = 0; i < 8; i++)
                kv[i] = k >> (56-8*i);
	}
	memset(&key, 0, sizeof key); key.data = &kv, key.size = sizeof kv;
	memset(&val, 0, sizeof val);

        double tstart = gettime();

        DBC *dbc;
        r = db->cursor(db, tid, &dbc, 0); assert(r==0);

        // set the cursor to the random key
        r = dbc->c_get(dbc, &key, &val, DB_SET_RANGE+lock_flag);
        if (r != 0) {
            assert(r == DB_NOTFOUND);
            //printf("%s:%d %" PRIu64 "\n", __FUNCTION__, __LINE__, k);
            goto makekey;
        }

#ifdef TOKUDB
        // do the range scan
	long rowcounter = 0;
	struct extra_count e = {0,0};
        while (limitcount > 0 && rowcounter < limitcount) {
            r = dbc->c_getf_next(dbc, prelockflag ? lock_flag : 0, counttotalbytes, &e);
            if (r != 0)
                break;
	    rowcounter++;
	}
#endif

        r = dbc->c_close(dbc);                                      
        assert(r==0);

        texperiments[counter] = gettime() - tstart;
        printf("%" PRIu64 " %f\n", k, texperiments[counter]); fflush(stdout);
    }

    // print the times
    double tsum = 0.0, tmin = 0.0, tmax = 0.0;
    for (int counter = 0; counter < n_experiments; counter++) {
        if (counter==0 || texperiments[counter] < tmin)
            tmin = texperiments[counter];
        if (counter==0 || texperiments[counter] > tmax)
            tmax = texperiments[counter];
        tsum += texperiments[counter];
    }
    printf("%f %f %f/%d = %f\n", tmin, tmax, tsum, n_experiments, tsum / n_experiments);
}

#ifdef TOKUDB

struct extra_verify {
    long long totalbytes;
    int rowcounter;
    DBT k,v; // the k and v are gotten using the old cursor
};

static int
checkbytes (DBT const *key, DBT const *data, void *extrav) {
    struct extra_verify *CAST_FROM_VOIDP(e, extrav);
    e->totalbytes += key->size + data->size;
    e->rowcounter++;
    assert(e->k.size == key->size);
    assert(e->v.size == data->size);
    assert(memcmp(e->k.data, key->data,  key->size)==0);
    assert(memcmp(e->v.data, data->data, data->size)==0);
    assert(e->k.data != key->data);
    assert(e->v.data != data->data);
    return 0;
}
    

static void scanscan_verify (void) {
    int r;
    int counter=0;
    for (counter=0; counter<n_experiments; counter++) {
	struct extra_verify v;
	v.totalbytes=0;
	v.rowcounter=0;
	double prevtime = gettime();
	DBC *dbc1, *dbc2;
	r = db->cursor(db, tid, &dbc1, 0);                           assert(r==0);
	r = db->cursor(db, tid, &dbc2, 0);                           assert(r==0);
	memset(&v.k, 0, sizeof(v.k));
	memset(&v.v, 0, sizeof(v.v));
        uint32_t f_flags = 0;
        uint32_t c_get_flags = DB_NEXT;
        if (prelockflag && (counter || prelock)) {
            f_flags     |= lock_flag;
            c_get_flags |= lock_flag;
        }
	while (1) {
	    int r1,r2;
	    r2 = dbc1->c_get(dbc1, &v.k, &v.v, c_get_flags);
	    r1 = dbc2->c_getf_next(dbc2, f_flags, checkbytes, &v);
	    assert(r1==r2);
	    if (r1) break;
	}
	r = dbc1->c_close(dbc1);                                      assert(r==0);
	r = dbc2->c_close(dbc2);                                      assert(r==0);
	double thistime = gettime();
	double tdiff = thistime-prevtime;
	printf("verify   %lld bytes (%d rows) in %9.6fs at %9fMB/s\n", v.totalbytes, v.rowcounter, tdiff, 1e-6*v.totalbytes/tdiff);
    }
}

#endif

static int test_main (int argc, char *const argv[]) {

    parse_args(argc,argv);

    scanscan_setup();
    switch (run_mode) {
    case RUN_HWC:    scanscan_hwc();    break;
#ifdef TOKUDB
    case RUN_LWC:    scanscan_lwc();    break;
    case RUN_VERIFY: scanscan_verify(); break;
#endif
    case RUN_RANGE:  scanscan_range();  break;
    default:         assert(0);         break;
    }
    scanscan_shutdown();

    return 0;
}
