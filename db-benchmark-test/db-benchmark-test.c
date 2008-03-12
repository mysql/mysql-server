/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Insert a bunch of stuff */
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <db.h>

#if !defined(DB_YESOVERWRITE)
#define DB_YESOVERWRITE 0
#endif

enum { SERIAL_SPACING = 1<<6 };
enum { DEFAULT_ITEMS_TO_INSERT_PER_ITERATION = 1<<20 };
enum { DEFAULT_ITEMS_PER_TRANSACTION = 1<<14 };

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

/* default test parameters */
int keysize = sizeof (long long);
int valsize = sizeof (long long);
int pagesize = 0;
long long cachesize = 128*1024*1024;
int dupflags = 0;
int noserial = 0; // Don't do the serial stuff
int norandom = 0; // Don't do the random stuff
int items_per_transaction = DEFAULT_ITEMS_PER_TRANSACTION;
int items_per_iteration   = DEFAULT_ITEMS_TO_INSERT_PER_ITERATION;

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)
char *dbdir = "./bench."  STRINGIFY(DIRSUF) "/"; /* DIRSUF is passed in as a -D argument to the compiler. */;
char *dbfilename = "bench.db";
char *dbname;

DB_ENV *dbenv;
DB *db;
DB_TXN *tid=0;

int do_transactions = 0;
int n_insertions_since_txn_began=0;

void setup (void) {
    int r;
   
    {
	char unlink_cmd[strlen(dbdir) + strlen("rf -rf ") + 1];
	snprintf(unlink_cmd, sizeof(unlink_cmd), "rm -rf %s", dbdir);
	//printf("unlink_cmd=%s\n", unlink_cmd);
	system(unlink_cmd);
    }
    if (strcmp(dbdir, ".") != 0)
        mkdir(dbdir, 0755);

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
    if (dbenv->set_lk_max) {
	r = dbenv->set_lk_max(dbenv, items_per_transaction*2);
	assert(r==0);
    }
#endif
    if (dbenv->set_lk_max_locks) {
        r = dbenv->set_lk_max_locks(dbenv, items_per_transaction*2);
        assert(r == 0);
    }

    if (dbenv->set_cachesize) {
        r = dbenv->set_cachesize(dbenv, cachesize / (1024*1024*1024), cachesize % (1024*1024*1024), 1);
        if (r != 0) 
            printf("WARNING: set_cachesize %d\n", r);
    }

    {
	int flags = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL | (do_transactions ? (DB_INIT_TXN | DB_INIT_LOG | DB_INIT_LOCK): 0);
	r = dbenv->open(dbenv, dbdir, flags, 0644);
	assert(r == 0);
    }

    r = db_create(&db, dbenv, 0);
    assert(r == 0);

    if (do_transactions) {
	r=dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
    }
    if (pagesize && db->set_pagesize) {
        r = db->set_pagesize(db, pagesize); 
        assert(r == 0);
    }
    if (dupflags) {
        r = db->set_flags(db, dupflags);
        assert(r == 0);
    }
    r = db->open(db, tid, dbfilename, NULL, DB_BTREE, DB_CREATE, 0644);
    assert(r == 0);
    if (do_transactions) {
	r=tid->commit(tid, 0);    assert(r==0);
    }

}

void shutdown (void) {
    int r;
    
    r = db->close(db, 0);
    assert(r == 0);
    r = dbenv->close(dbenv, 0);
    assert(r == 0);
}

void long_long_to_array (unsigned char *a, unsigned long long l) {
    int i;
    for (i=0; i<8; i++)
	a[i] = (l>>(56-8*i))&0xff;
}

DBT *fill_dbt(DBT *dbt, const void *data, int size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->size = size;
    dbt->data = (void *) data;
    return dbt;
}

void insert (long long v) {
    unsigned char kc[keysize], vc[valsize];
    DBT  kt, vt;
    memset(kc, 0, sizeof kc);
    long_long_to_array(kc, v);
    memset(vc, 0, sizeof vc);
    long_long_to_array(vc, v);
    int r = db->put(db, tid, fill_dbt(&kt, kc, keysize), fill_dbt(&vt, vc, valsize), DB_YESOVERWRITE);
    CKERR(r);
    if (do_transactions) {
	if (n_insertions_since_txn_began>=items_per_transaction) {
	    n_insertions_since_txn_began=0;
	    r = tid->commit(tid, 0); assert(r==0);
	    r=dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
	    n_insertions_since_txn_began=0;
	}
	n_insertions_since_txn_began++;
    }
}

void serial_insert_from (long long from) {
    long long i;
    if (do_transactions) {
	int r = dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
	{
	    DBT k,v;
	    r=db->put(db, tid, fill_dbt(&k, "a", 1), fill_dbt(&v, "b", 1), DB_YESOVERWRITE);
	    CKERR(r);
	}
				      
    }
    for (i=0; i<items_per_iteration; i++) {
	insert((from+i)*SERIAL_SPACING);
    }
    if (do_transactions) {
	int  r= tid->commit(tid, 0);             assert(r==0);
	tid=0;
    }
}

long long llrandom (void) {
    return (((long long)(random()))<<32) + random();
}

void random_insert_below (long long below) {
    long long i;
    if (do_transactions) {
	int r = dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
    }
    for (i=0; i<items_per_iteration; i++) {
	insert(llrandom()%below);
    }
    if (do_transactions) {
	int  r= tid->commit(tid, 0);             assert(r==0);
	tid=0;
    }
}

double tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec-b->tv_sec)+1e-6*(a->tv_usec-b->tv_usec);
}

void biginsert (long long n_elements, struct timeval *starttime) {
    long long i;
    struct timeval t1,t2;
    int iteration;
    for (i=0, iteration=0; i<n_elements; i+=items_per_iteration, iteration++) {
	if (!noserial) {
	    gettimeofday(&t1,0);
	    serial_insert_from(i);
	    gettimeofday(&t2,0);
	    printf("serial %9.6fs %8.0f/s    ", tdiff(&t2, &t1), items_per_iteration/tdiff(&t2, &t1));
	    fflush(stdout);
	}
        if (!norandom) {
	gettimeofday(&t1,0);
	random_insert_below((i+items_per_iteration)*SERIAL_SPACING);
	gettimeofday(&t2,0);
	printf("random %9.6fs %8.0f/s    ", tdiff(&t2, &t1), items_per_iteration/tdiff(&t2, &t1));
        }
	printf("cumulative %9.6fs %8.0f/s\n", tdiff(&t2, starttime), (((float)items_per_iteration*(!noserial+!norandom))/tdiff(&t2, starttime))*(iteration+1));
    }
}



const long long default_n_items = 1LL<<22;

int print_usage (const char *argv0) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, " %s [-x] [--keysize KEYSIZE] [--valsize VALSIZE] [--noserial] [ n_iterations ]\n", argv0);
    fprintf(stderr, "   where\n");
    fprintf(stderr, "    -x              do transactions (one transaction per iteration) (default: no transactions at all)\n");
    fprintf(stderr, "    --keysize KEYSIZE sets the key size (default 8)\n");
    fprintf(stderr, "    --valsize VALSIZE sets the value size (default 8)\n");
    fprintf(stderr, "    --cachesize CACHESIZE set the database cache size\n");
    fprintf(stderr, "    --pagesize PAGESIZE sets the database page size\n");
    fprintf(stderr, "    --noserial         causes the serial insertions to be skipped\n");
    fprintf(stderr, "    --xcount N         how many insertions per transaction (default=%d)\n", DEFAULT_ITEMS_PER_TRANSACTION);
    fprintf(stderr, "    --periter N      how many insertions per iteration (default=%d)\n", DEFAULT_ITEMS_TO_INSERT_PER_ITERATION);
    fprintf(stderr, "   n_iterations     how many iterations (default %lld)\n", default_n_items/DEFAULT_ITEMS_TO_INSERT_PER_ITERATION);

    return 1;
}

int main (int argc, const char *argv[]) {
    struct timeval t1,t2,t3;
    long long total_n_items = default_n_items;
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (arg[0] != '-')
            break;
        if (strcmp(arg, "-x") == 0) {
            do_transactions = 1;
            continue;
        }
	if (strcmp(arg, "--noserial") == 0) {
	    noserial=1;
	    continue;
	}
	if (strcmp(arg, "--norandom") == 0) {
	    norandom=1;
	    continue;
	}
	if (strcmp(arg, "--xcount") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            items_per_transaction = strtoll(argv[++i], 0, 10);
            continue;
        }
	if (strcmp(arg, "--periter") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            items_per_iteration = strtoll(argv[++i], 0, 10);
            continue;
        }
	if (strcmp(arg, "--cachesize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            cachesize = strtoll(argv[++i], 0, 10);
            continue;
        }
        if (strcmp(arg, "--keysize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            keysize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--valsize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            valsize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--pagesize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            pagesize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--dupsort") == 0) {
            dupflags = DB_DUP + DB_DUPSORT;
            continue;
        }
        return print_usage(argv[0]);
    }
    if (i<argc) {
        /* if it looks like a number */
        char *end;
        errno=0;
        long n_iterations = strtol(argv[i], &end, 10);
        if (errno!=0 || *end!=0 || end==argv[i]) {
            print_usage(argv[0]);
            return 1;
        }
        total_n_items = items_per_iteration * (long long)n_iterations;
    }
    if (!noserial) printf("serial ");
    if (!noserial && !norandom) printf("and ");
    if (!norandom) printf("random ");
    printf("insertions of %d per batch%s\n", items_per_iteration, do_transactions ? " (with transactions)" : "");
    setup();
    gettimeofday(&t1,0);
    biginsert(total_n_items, &t1);
    gettimeofday(&t2,0);
    shutdown();
    gettimeofday(&t3,0);
    printf("Shutdown %9.6fs\n", tdiff(&t3, &t2));
    printf("Total time %9.6fs for %lld insertions = %8.0f/s\n", tdiff(&t3, &t1), 
           (!noserial+!norandom)*total_n_items, (!noserial+!norandom)*total_n_items/tdiff(&t3, &t1));
    return 0;
}

