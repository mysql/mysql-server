/* Scan the bench.tokudb/bench.db over and over. */

#include <db.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <unistd.h>
#include "rdtsc.h"

const char *pname;
enum run_mode { RUN_HWC, RUN_LWC, RUN_VERIFY } run_mode = RUN_HWC;
int do_txns=1, prelock=0, prelockflag=0;
u_int32_t lock_flag = 0;
int do_rdtsc = 0;
#define NTRACE 1000000
int next_trace = 0;
unsigned long long tracebuffer[NTRACE];

static inline void add_trace() {
#if USE_RDTSC
    if (next_trace < NTRACE)
        tracebuffer[next_trace++] = rdtsc();
#endif
}

void print_trace() {
    if (do_rdtsc == 0) return;
    int i;
    for (i=0; i<NTRACE; i++)
        if (tracebuffer[i]) printf("%llu\n", tracebuffer[i]);
}

void parse_args (int argc, const char *argv[]) {
    pname=argv[0];
    argc--;
    argv++;
    int specified_run_mode=0;
    while (argc>0) {
	if (strcmp(*argv,"--verify-lwc")==0) {
	    if (specified_run_mode && run_mode!=RUN_VERIFY) { two_modes: fprintf(stderr, "You specified two run modes\n"); exit(1); }
	    run_mode = RUN_VERIFY;
	} else if (strcmp(*argv, "--lwc")==0)  {
	    if (specified_run_mode && run_mode!=RUN_LWC) goto two_modes;
	    run_mode = RUN_LWC;
	} else if (strcmp(*argv, "--hwc")==0)  {
	    if (specified_run_mode && run_mode!=RUN_VERIFY) goto two_modes;
	    run_mode = RUN_HWC;
	} else if (strcmp(*argv, "--prelock")==0) prelock=1;
        else if (strcmp(*argv, "--prelockflag")==0)      { prelockflag=1; lock_flag = DB_PRELOCKED; }
        else if (strcmp(*argv, "--prelockwriteflag")==0) { prelockflag=1; lock_flag = DB_PRELOCKED_WRITE; }
	else if (strcmp(*argv, "--nox")==0)              { do_txns=0; }
        else if (strcmp(*argv, "--rdtsc")==0)            { do_rdtsc=1; }
	else {
	    fprintf(stderr, "Usage:\n%s [--verify-lwc | --lwc | --nohwc] [--prelock] [--prelockflag] [--prelockwriteflag]\n", pname);
	    fprintf(stderr, "  --hwc               run heavy weight cursors (this is the default)\n");
	    fprintf(stderr, "  --verify-lwc        means to run the light weight cursor and the heavyweight cursor to verify that they get the same answer.\n");
	    fprintf(stderr, "  --lwc               run light weight cursors instead of heavy weight cursors\n");
	    fprintf(stderr, "  --prelock           acquire a read lock on the entire table before running\n");
	    fprintf(stderr, "  --prelockflag       pass DB_PRELOCKED to the the cursor get operation whenever the locks have been acquired\n");
	    fprintf(stderr, "  --prelockwriteflag  pass DB_PRELOCKED_WRITE to the cursor get operation\n");
	    fprintf(stderr, "  --nox               no transactions\n");
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
int env_open_flags_yesx = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOG|DB_INIT_LOCK;
int env_open_flags_nox = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;
char *dbfilename = "bench.db";

void setup (void) {
    int r;
    r = db_env_create(&env, 0);                                                           assert(r==0);
    r = env->set_cachesize(env, 0, 127*1024*1024, 1);                                     assert(r==0);
    r = env->open(env, dbdir, do_txns? env_open_flags_yesx : env_open_flags_nox, 0644);   assert(r==0);
    r = db_create(&db, env, 0);                                                           assert(r==0);
    if (do_txns) {
	r = env->txn_begin(env, 0, &tid, 0);                                              assert(r==0);
    }
    r = db->open(db, tid, dbfilename, NULL, DB_BTREE, 0, 0644);                           assert(r==0);
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
    if (do_txns) {
	r = tid->commit(tid, 0);                                    assert(r==0);
    }
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

void scanscan_hwc (void) {
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
        u_int32_t c_get_flags = DB_NEXT;
        if (prelockflag && (counter || prelock)) {
            c_get_flags |= lock_flag;
        }
        add_trace();
	while (0 == (r = dbc->c_get(dbc, &k, &v, c_get_flags))) {
            add_trace();
	    totalbytes += k.size + v.size;
	    rowcounter++;
	}
	r = dbc->c_close(dbc);                                      assert(r==0);
	double thistime = gettime();
	double tdiff = thistime-prevtime;
	printf("Scan    %lld bytes (%d rows) in %9.6fs at %9fMB/s\n", totalbytes, rowcounter, tdiff, 1e-6*totalbytes/tdiff);
        print_trace();
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
        u_int32_t f_flags = 0;
        if (prelockflag && (counter || prelock)) {
            f_flags |= lock_flag;
        }
	while (0 == (r = dbc->c_getf_next(dbc, f_flags, counttotalbytes, &e)));
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
        u_int32_t f_flags = 0;
        u_int32_t c_get_flags = DB_NEXT;
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


int main (int argc, const char *argv[]) {

    parse_args(argc,argv);

    setup();
    switch (run_mode) {
    case RUN_HWC:    scanscan_hwc();    goto ok;
    case RUN_LWC:    scanscan_lwc();    goto ok;
    case RUN_VERIFY: scanscan_verify(); goto ok;
    }
    assert(0);
 ok:
    shutdown();

#ifdef TOKUDB
    if (0) {
	extern void print_hash_histogram (void) __attribute__((__visibility__("default")));
	print_hash_histogram();
    }
#endif
    return 0;
}
