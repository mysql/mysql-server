/* -*- mode: C; c-basic-offset: 4 -*- */

/* Idea: 
 *  create a dictionary
 *  repeat:  
 *    lots of inserts
 *    checkpoint
 *    note file size
 *    lots of deletes
 *    checkpoint 
 *    note file size
 *
 */

#include "test.h"

#define PATHSIZE 1024

DB_ENV *env;
DB *db;
char dbname[] = "foo.db";
char path[PATHSIZE];

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_PRIVATE;

int ninsert, nread, nread_notfound, nread_failed, ndelete, ndelete_notfound, ndelete_failed;

static TOKU_DB_FRAGMENTATION_S report;

static void
check_fragmentation(void) {
    int r = db->get_fragmentation(db, &report);
    CKERR(r);
}

static void
print_fragmentation(void) {
    printf("Fragmentation:\n");
    printf("\tTotal file size in bytes (file_size_bytes): %"PRIu64"\n", report.file_size_bytes);
    printf("\tCompressed User Data in bytes (data_bytes): %"PRIu64"\n", report.data_bytes);
    printf("\tNumber of blocks of compressed User Data (data_blocks): %"PRIu64"\n", report.data_blocks);
    printf("\tAdditional bytes used for checkpoint system (checkpoint_bytes_additional): %"PRIu64"\n", report.checkpoint_bytes_additional);
    printf("\tAdditional blocks used for checkpoint system  (checkpoint_blocks_additional): %"PRIu64"\n", report.checkpoint_blocks_additional);
    printf("\tUnused space in file (unused_bytes): %"PRIu64"\n", report.unused_bytes);
    printf("\tNumber of contiguous regions of unused space (unused_blocks): %"PRIu64"\n", report.unused_blocks);
    printf("\tSize of largest contiguous unused space (largest_unused_block): %"PRIu64"\n", report.largest_unused_block);
}

static void
close_em (void)
{
    int r;
    r = db->close(db, 0);   CKERR(r);
    r = env->close(env, 0); CKERR(r);
}


static void
setup(void)
{
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    int r;
    r = db_env_create(&env, 0);                                         CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);      CKERR(r);
    r = db_create(&db, env, 0);                                         CKERR(r);
    r = db->open(db, NULL, dbname, NULL, DB_BTREE, DB_CREATE, 0666);    CKERR(r);
}


static void
fill_rand(int n, uint64_t * d) {
    for (int i = 0; i < n; i++){
	*(d+i) = random64();
    }
}

#define INSERT_BIG 1500
#define INSERT_SMALL 0
static void
insert_n (u_int32_t ah, int datasize) {
    uint64_t vdata[datasize];
    fill_rand(datasize, vdata);
    u_int32_t an = htonl(ah);
    //    if (verbose) printf("insert an = %0X (ah = %0X)\n", an, ah);
    DBT key = {.size = 4,             .data=&an };
    DBT val = {.size = sizeof(vdata), .data=vdata};
    int r = db->put(db, NULL, &key, &val, DB_YESOVERWRITE);
    CKERR(r);
    ninsert++;
}

static void
delete_n (u_int32_t ah)
{
    u_int32_t an = htonl(ah);
    //    if (verbose) printf("delete an = %0X (ah = %0X)\n", an, ah);
    DBT key = {.size = 4,             .data=&an };
    int r = db->del(db, NULL, &key, DB_DELETE_ANY);
    if (r == 0)
	ndelete++;
    else if (r == DB_NOTFOUND)
	ndelete_notfound++;
    else
	ndelete_failed++;
#ifdef USE_BDB
    assert(r==0 || r==DB_NOTFOUND);
#else
    CKERR(r);
#endif
}

static void
scan(int n) {
    DBT k,v;
    DBC *dbc;
    int i,r;

    r = db->cursor(db, 0, &dbc, 0);                           
    CKERR(r);
    memset(&k, 0, sizeof(k));
    memset(&v, 0, sizeof(v));
    r = dbc->c_get(dbc, &k, &v, DB_FIRST);
    if (r==0) {
	nread++;
	//	if (verbose) printf("First read, r = %0X, key = %0X (size=%d)\n", r, *(uint32_t*)k.data, k.size);
    }
    else if (r == DB_NOTFOUND)  {
	nread_notfound++;
	//	if (verbose) printf("First read failed: %d\n", r);
    }
    else
	nread_failed++;
    for (i = 1; i<n; i++) {
	r = dbc->c_get(dbc, &k, &v, DB_NEXT);
	if (r == 0) {
	    nread++;
	    //	    if (verbose) printf("read key = %0X (size=%d)\n", *(uint32_t*)k.data, k.size);
	}
	else if (r == DB_NOTFOUND)
	    nread_notfound++;
	else
	    nread_failed++;
    }
    r = dbc->c_close(dbc);
    CKERR(r);

}

static void
get_file_pathname(void) {
    DBT dname;
    DBT iname;
    dbt_init(&dname, dbname, sizeof(dbname));
    dbt_init(&iname, NULL, 0);
    iname.flags |= DB_DBT_MALLOC;
    int r = env->get_iname(env, &dname, &iname);
    CKERR(r);
    sprintf(path, "%s/%s", ENVDIR, (char*)iname.data);
    toku_free(iname.data);
    if (verbose) printf("path = %s\n", path);
}


static int 
getsizeM(void) {
    toku_struct_stat buf;
    int r = toku_stat(path, &buf);
    CKERR(r);    
    int sizeM = (int)buf.st_size >> 20;
    check_fragmentation();
    if (verbose>1)
        print_fragmentation();
    return sizeM;
}

static void
test_filesize (void)
{
    int N=1<<14;
    int r, i, sizeM;

    get_file_pathname();
    
    for (int iter = 0; iter < 3; iter++) {
	int offset = N * iter;

	for (i=0; i<N; i++) {
	    insert_n(i + offset, INSERT_BIG);
	}
	
	r = env->txn_checkpoint(env, 0, 0, 0);
	CKERR(r);
	int sizefirst = sizeM = getsizeM();
	if (verbose) printf("Filesize after iteration %d insertion and checkpoint = %dM\n", iter, sizeM);
	
	int preserve = 2;
	for (i = preserve; i<(N); i++) {  // leave a little at the beginning
	    delete_n(i + offset);
	}
	scan(N);

	r = env->txn_checkpoint(env, 0, 0, 0);
	CKERR(r);
	sizeM = getsizeM();
	if (verbose) printf("Filesize after iteration %d deletion and checkpoint 1 = %dM\n", iter, sizeM);

	for (i=0; i<N; i++) {
	    insert_n(i + offset, INSERT_SMALL);
	}
	for (i = preserve; i<(N); i++) {  // leave a little at the beginning
	    delete_n(i + offset);
	}
	scan(N);
	r = env->txn_checkpoint(env, 0, 0, 0);
	CKERR(r);
	sizeM = getsizeM();
	if (verbose) printf("Filesize after iteration %d deletion and checkpoint 2 = %dM\n", iter, sizeM);
        assert(sizeM < sizefirst);

	if (verbose) printf("ninsert = %d\n", ninsert);
	if (verbose) printf("nread = %d, nread_notfound = %d, nread_failed = %d\n", nread, nread_notfound, nread_failed);
	if (verbose) printf("ndelete = %d, ndelete_notfound = %d, ndelete_failed = %d\n", ndelete, ndelete_notfound, ndelete_failed);
    }
}

int test_main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    parse_args(argc, argv);
    setup();
    if (verbose) print_engine_status(env);
    test_filesize();
    if (verbose) {
        print_engine_status(env);
    }
    check_fragmentation();
    if (verbose) print_fragmentation();
    close_em();
    return 0;
}
