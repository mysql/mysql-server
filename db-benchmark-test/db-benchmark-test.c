/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

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

enum { SERIAL_SPACING = 1<<6 };
enum { ITEMS_TO_INSERT_PER_ITERATION = 1<<20 };
enum { ITEMS_PER_TRANSACTION = 1<<14 };
//enum { ITEMS_TO_INSERT_PER_ITERATION = 1<<14 };
enum { BOUND_INCREASE_PER_ITERATION = SERIAL_SPACING*ITEMS_TO_INSERT_PER_ITERATION };

#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);

/* default test parameters */
int keysize = sizeof (long long);
int valsize = sizeof (long long);
int pagesize = 0;
long long cachesize = 128*1024*1024;

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

    if (dbenv->set_lk_max) {
	r = dbenv->set_lk_max(dbenv, ITEMS_PER_TRANSACTION*2);
	assert(r==0);
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

DBT *toku_fill_dbt(DBT *dbt, void *data, int size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->size = size;
    dbt->data = data;
    return dbt;
}

void insert (long long v) {
    unsigned char kc[keysize], vc[valsize];
    DBT  kt, vt;
    memset(kc, 0, sizeof kc);
    long_long_to_array(kc, v);
    memset(vc, 0, sizeof vc);
    long_long_to_array(vc, v);
    int r = db->put(db, tid, toku_fill_dbt(&kt, kc, keysize), toku_fill_dbt(&vt, vc, valsize), 0);
    CKERR(r);
    if (do_transactions) {
	if (n_insertions_since_txn_began>=ITEMS_PER_TRANSACTION) {
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
	    r=db->put(db, tid, toku_fill_dbt(&k, "a", 1), toku_fill_dbt(&v, "b", 1), 0);
	    CKERR(r);
	}
				      
    }
    for (i=0; i<ITEMS_TO_INSERT_PER_ITERATION; i++) {
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
    for (i=0; i<ITEMS_TO_INSERT_PER_ITERATION; i++) {
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
    for (i=0, iteration=0; i<n_elements; i+=ITEMS_TO_INSERT_PER_ITERATION, iteration++) {
	gettimeofday(&t1,0);
	serial_insert_from(i);
	gettimeofday(&t2,0);
	printf("serial %9.6fs %8.0f/s    ", tdiff(&t2, &t1), ITEMS_TO_INSERT_PER_ITERATION/tdiff(&t2, &t1));
	fflush(stdout);
	gettimeofday(&t1,0);
	random_insert_below((i+ITEMS_TO_INSERT_PER_ITERATION)*SERIAL_SPACING);
	gettimeofday(&t2,0);
	printf("random %9.6fs %8.0f/s    ", tdiff(&t2, &t1), ITEMS_TO_INSERT_PER_ITERATION/tdiff(&t2, &t1));
	printf("cumulative %9.6fs %8.0f/s\n", tdiff(&t2, starttime), (ITEMS_TO_INSERT_PER_ITERATION*2.0/tdiff(&t2, starttime))*(iteration+1));
    }
}



const long long default_n_items = 1LL<<22;

void print_usage (const char *argv0) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, " %s [-x] [--keysize KEYSIZE] [--valsize VALSIZE] [ n_iterations ]\n", argv0);
    fprintf(stderr, "   where\n");
    fprintf(stderr, "    -x              do transactions (one transaction per iteration) (default: no transactions at all)\n");
    fprintf(stderr, "    --keysize KEYSIZE sets the key size (default 8)\n");
    fprintf(stderr, "    --valsize VALSIZE sets the value size (default 8)\n");
    fprintf(stderr, "    --cachesize CACHESIZE set the database cache size\n");
    fprintf(stderr, "    --pagesize PAGESIZE sets the database page size\n");
    fprintf(stderr, "   n_iterations     how many iterations (default %lld iterations of %d items per iteration)\n", default_n_items/ITEMS_TO_INSERT_PER_ITERATION, ITEMS_TO_INSERT_PER_ITERATION);
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
        if (strcmp(arg, "--cachesize") == 0) {
            if (i+1 < argc) {
                i++;
                cachesize = strtoll(argv[i], 0, 10);
            }
            continue;
        }
        if (strcmp(arg, "--keysize") == 0) {
            if (i+1 < argc) {
                i++;
                keysize = atoi(argv[i]);
            }
            continue;
        }
        if (strcmp(arg, "--valsize") == 0) {
            if (i+1 < argc) {
                i++;
                valsize = atoi(argv[i]);
            }
            continue;
        }
        if (strcmp(arg, "--pagesize") == 0) {
            if (i+1 < argc) {
                i++;
                pagesize = atoi(argv[i]);
            }
            continue;
        }
        print_usage(argv[0]);
        return 1;
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
        total_n_items = ITEMS_TO_INSERT_PER_ITERATION * (long long)n_iterations;
    }
    printf("Serial and random insertions of %d per batch%s\n", ITEMS_TO_INSERT_PER_ITERATION, do_transactions ? " (with transactions)" : "");
    setup();
    gettimeofday(&t1,0);
    biginsert(total_n_items, &t1);
    gettimeofday(&t2,0);
    shutdown();
    gettimeofday(&t3,0);
    printf("Shutdown %9.6fs\n", tdiff(&t3, &t2));
    printf("Total time %9.6fs for %lld insertions = %8.0f/s\n", tdiff(&t3, &t1), 2*total_n_items, 2*total_n_items/tdiff(&t3, &t1));
    return 0;
}

