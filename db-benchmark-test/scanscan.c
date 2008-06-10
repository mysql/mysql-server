/* Scan the bench.tokudb/bench.db over and over. */

#include <db.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <unistd.h>

const char *pname;
int verify_lwc=0, lwc=0, hwc=1, prelock=0;


void parse_args (int argc, const char *argv[]) {
    pname=argv[0];
    argc--;
    argv++;
    while (argc>0) {
	if (strcmp(*argv,"--verify-lwc")==0) verify_lwc=1;
	else if (strcmp(*argv, "--lwc")==0)  lwc=1;
	else if (strcmp(*argv, "--nohwc")==0) hwc=0;
	else if (strcmp(*argv, "--prelock")==0) prelock=1;
	else {
	    printf("Usage:\n%s [--verify-lwc] [--lwc] [--nohwc]\n", pname);
	    exit(1);
	}
	argc--;
	argv++;
    }
}


DB_ENV *env;
DB *db;
DB_TXN *tid=0;

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)
const char *dbdir = "./bench."  STRINGIFY(DIRSUF) "/"; /* DIRSUF is passed in as a -D argument to the compiler. */;
#define TXNS
#ifdef TXNS
int env_open_flags = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOG|DB_INIT_LOCK;
#else
int env_open_flags = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;
#endif
char *dbfilename = "bench.db";

void setup (void) {
    int r;
    r = db_env_create(&env, 0);                                 assert(r==0);
    r = env->set_cachesize(env, 0, 127*1024*1024, 1);           assert(r==0);
    r = env->open(env, dbdir, env_open_flags, 0644);            assert(r==0);
    r = db_create(&db, env, 0);                                 assert(r==0);
#ifdef TXNS
    r = env->txn_begin(env, 0, &tid, 0);                        assert(r==0);
#endif
    r = db->open(db, tid, dbfilename, NULL, DB_BTREE, 0, 0644); assert(r==0);
    if (prelock) {
	r = db->pre_acquire_read_lock(db,
				      tid,
				      db->dbt_neg_infty(), db->dbt_neg_infty(),
				      db->dbt_pos_infty(), db->dbt_pos_infty());
	assert(r==0);
    }
}

void shutdown (void) {
    int r;
    r = db->close(db, 0);                                       assert(r==0);
#ifdef TXNS
    r = tid->commit(tid, 0);                                    assert(r==0);
#endif
    r = env->close(env, 0);                                     assert(r==0);
    {
	extern unsigned long toku_get_maxrss(void);
	printf("maxrss=%.2fMB\n", toku_get_maxrss()/256.0);
    }
}

double gettime (void) {
    struct timeval tv;
    int r = gettimeofday(&tv, 0);
    assert(r==0);
    return tv.tv_sec + 1e-6*tv.tv_usec;
}

void scanscan (void) {
    int r;
    int counter=0;
    for (counter=0; counter<2; counter++) {
	long long totalbytes=0;
	int rowcounter=0;
	double prevtime = gettime();
	DBT k,v;
	DBC *dbc;
	r = db->cursor(db, tid, &dbc, 0);                           assert(r==0);
	memset(&k, 0, sizeof(k));
	memset(&v, 0, sizeof(v));
	while (0 == (r = dbc->c_get(dbc, &k, &v, DB_NEXT))) {
	    totalbytes += k.size + v.size;
	    rowcounter++;
	}
	r = dbc->c_close(dbc);                                      assert(r==0);
	double thistime = gettime();
	double tdiff = thistime-prevtime;
	printf("Scan    %lld bytes (%d rows) in %9.6fs at %9fMB/s\n", totalbytes, rowcounter, tdiff, 1e-6*totalbytes/tdiff);
    }
}

struct extra_count {
    long long totalbytes;
    int rowcounter;
};
void counttotalbytes (DBT const *key, DBT const *data, void *extrav) {
    struct extra_count *e=extrav;
    e->totalbytes += key->size + data->size;
    e->rowcounter++;
}

void scanscan_lwc (void) {
    int r;
    int counter=0;
    for (counter=0; counter<2; counter++) {
	struct extra_count e = {0,0};
	double prevtime = gettime();
	DBC *dbc;
	r = db->cursor(db, tid, &dbc, 0);                           assert(r==0);
	while (0 == (r = dbc->c_getf_next(dbc, 0, counttotalbytes, &e)));
	r = dbc->c_close(dbc);                                      assert(r==0);
	double thistime = gettime();
	double tdiff = thistime-prevtime;
	printf("LWC Scan %lld bytes (%d rows) in %9.6fs at %9fMB/s\n", e.totalbytes, e.rowcounter, tdiff, 1e-6*e.totalbytes/tdiff);
    }
}

struct extra_verify {
    long long totalbytes;
    int rowcounter;
    DBT k,v; // the k and v are gotten using the old cursor
};
void checkbytes (DBT const *key, DBT const *data, void *extrav) {
    struct extra_verify *e=extrav;
    e->totalbytes += key->size + data->size;
    e->rowcounter++;
    assert(e->k.size == key->size);
    assert(e->v.size == data->size);
    assert(memcmp(e->k.data, key->data,  key->size)==0);
    assert(memcmp(e->v.data, data->data, data->size)==0);
    assert(e->k.data != key->data);
    assert(e->v.data != data->data);
}
    

void scanscan_verify (void) {
    int r;
    int counter=0;
    for (counter=0; counter<2; counter++) {
	struct extra_verify v;
	v.totalbytes=0;
	v.rowcounter=0;
	double prevtime = gettime();
	DBC *dbc1, *dbc2;
	r = db->cursor(db, tid, &dbc1, 0);                           assert(r==0);
	r = db->cursor(db, tid, &dbc2, 0);                           assert(r==0);
	memset(&v.k, 0, sizeof(v.k));
	memset(&v.v, 0, sizeof(v.v));
	while (1) {
	    int r1,r2;
	    r2 = dbc1->c_get(dbc1, &v.k, &v.v, DB_NEXT);
	    r1 = dbc2->c_getf_next(dbc2, 0, checkbytes, &v);
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


int main (int argc, const char *argv[]) {

    parse_args(argc,argv);

    if (hwc) {
	setup();
	scanscan();
	shutdown();
    }

    if (lwc) {
	setup();
	scanscan_lwc();
	shutdown();
    }

    if (verify_lwc) {
	setup();
	scanscan_verify();
	shutdown();
    }

    return 0;
}
