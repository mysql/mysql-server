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
//enum { ITEMS_TO_INSERT_PER_ITERATION = 1<<14 };
enum { BOUND_INCREASE_PER_ITERATION = SERIAL_SPACING*ITEMS_TO_INSERT_PER_ITERATION };

char *dbdir = ".";
char *dbfilename = "bench.db";
char *dbname;

DB_ENV *dbenv;
DB *db;

void setup (void) {
    int r;
   
    char fullname[strlen(dbdir) + strlen("/") + strlen(dbfilename) + 1];
    sprintf(fullname, "%s/%s", dbdir, dbfilename);
    unlink(fullname);
    if (strcmp(dbdir, ".") != 0)
        mkdir(dbdir, 0755);

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

    if (dbenv->set_cachesize) {
        r = dbenv->set_cachesize(dbenv, 0, 128*1024*1024, 1);
        if (r != 0) 
            printf("WARNING: set_cachesize %d\n", r);
    }

    r = dbenv->open(dbenv, dbdir, DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL, 0644);
    assert(r == 0);

    r = db_create(&db, dbenv, 0);
    assert(r == 0);

    r = db->open(db, NULL, dbfilename, NULL, DB_BTREE, DB_CREATE, 0644);
    assert(r == 0);
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

DBT *fill_dbt(DBT *dbt, void *data, int size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->size = size;
    dbt->data = data;
    return dbt;
}

void insert (long long v) {
    unsigned char kc[8], vc[8];
    DBT  kt, vt;
    long_long_to_array(kc, v);
    long_long_to_array(vc, v);
    int r = db->put(db, NULL, fill_dbt(&kt, kc, 8), fill_dbt(&vt, vc, 8), 0);
    assert(r == 0);
}

void serial_insert_from (long long from) {
    long long i;
    for (i=0; i<ITEMS_TO_INSERT_PER_ITERATION; i++) {
	insert((from+i)*SERIAL_SPACING);
    }
}

long long llrandom (void) {
    return (((long long)(random()))<<32) + random();
}

void random_insert_below (long long below) {
    long long i;
    for (i=0; i<ITEMS_TO_INSERT_PER_ITERATION; i++) {
	insert(llrandom()%below);
    }
}

double tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec-b->tv_sec)+1e-6*(a->tv_usec-b->tv_usec);
}

void biginsert (long long n_elements, struct timeval *starttime) {
    long i;
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



int main (int argc, char *argv[]) {
    struct timeval t1,t2,t3;
    long long total_n_items;
    if (argc==2) {
	char *end;
	errno=0;
	total_n_items = ITEMS_TO_INSERT_PER_ITERATION * (long long) strtol(argv[1], &end, 10);
	assert(errno==0);
	assert(*end==0);
	assert(end!=argv[1]);
    } else {
	total_n_items = 1LL<<22; // 1LL<<16
    }
    printf("Serial and random insertions of %d per batch\n", ITEMS_TO_INSERT_PER_ITERATION);
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

