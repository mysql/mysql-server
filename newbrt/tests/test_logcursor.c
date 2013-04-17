#include <toku_portability.h>
#include <string.h>

#include "test.h"
#include "brttypes.h"
#include "includes.h"

const char LOGDIR[100] = "./dir.test_logcursor";
const int FSYNC = 1;
const int NO_FSYNC = 0;

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN;
const char *namea="a.db";
const char *nameb="b.db";
const char *a="a";
const char *b="b";

const FILENUM fn_aname = {0};
const FILENUM fn_bname = {1};
BYTESTRING bs_aname, bs_bname;
BYTESTRING bs_a, bs_b;
BYTESTRING bs_empty;

static int create_logfiles(void);

static int test_0 (void);
static int test_1 (void);
static void usage(void) {
    printf("test_logcursors [OPTIONS]\n");
    printf("[-v]\n");
    printf("[-q]\n");
}

int test_main(int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (arg[0] != '-')
            break;
	if (strcmp(arg, "-v")==0) {
	    verbose++;
	} else if (strcmp(arg, "-q")==0) {
	    verbose = 0;
	} else {
	    usage();
	    return 1;
	}
    }

    int r = 0;
    // start from a clean directory
    char rmrf_cmd[100];
    sprintf(rmrf_cmd, "rm -rf %s", LOGDIR);
    r = system(rmrf_cmd);
    CKERR(r);
    toku_os_mkdir(LOGDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    if ( (r=create_logfiles()) !=0 ) return r;

    if ( (r=test_0()) !=0 ) return r;
    if ( (r=test_1()) !=0 ) return r;

    r = system(rmrf_cmd);
    CKERR(r);
    return r;
}

int test_0 (void) {
    int r=0;
    struct toku_logcursor *cursor;
    struct log_entry *entry;

    r = toku_logcursor_create(&cursor, LOGDIR);    if (verbose) printf("create returns %d\n", r);  assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);
    
    r = toku_logcursor_create(&cursor, LOGDIR);    if (verbose) printf("create returns %d\n", r);   assert(r==0);
    r = toku_logcursor_first(cursor, &entry);     if (verbose) printf("First Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, LOGDIR);    if (verbose) printf("create returns %d\n", r);  assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, LOGDIR);    if (verbose) printf("create returns %d\n", r);  assert(r==0);
    r = toku_logcursor_last(cursor, &entry);      if (verbose) printf("Last Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, LOGDIR);    if (verbose) printf("create returns %d\n", r);  assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      assert(r==DB_NOTFOUND);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, LOGDIR);    if (verbose) printf("create returns %d\n", r);  assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      assert(r==DB_NOTFOUND);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    r = toku_logcursor_create(&cursor, LOGDIR);    if (verbose) printf("create returns %d\n", r);  assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_next(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); assert(r==0);
    r = toku_logcursor_prev(cursor, &entry);      if (verbose) printf("Entry = %c\n", entry->cmd); 
    if ( verbose) {
        if ( r == DB_NOTFOUND ) printf("PASS\n"); 
        else printf("FAIL\n"); 
    }
    assert(r==DB_NOTFOUND);
    r = toku_logcursor_destroy(&cursor);          if (verbose) printf("destroy returns %d\n", r);  assert(r==0);

    return 0;
}

// test per-file version
int test_1 () {
    int r=0;
    char logfile[PATH_MAX];
    sprintf(logfile, "log000000000000.tokulog%d", TOKU_LOG_VERSION);

    struct toku_logcursor *cursor;
    struct log_entry *entry;

    r = toku_logcursor_create_for_file(&cursor, LOGDIR, logfile);   
    if (verbose) printf("create returns %d\n", r);   
    assert(r==0);

    r = toku_logcursor_last(cursor, &entry);      
    if (verbose) printf("entry = %c\n", entry->cmd); 
    assert(r==0); 
    assert(entry->cmd =='C');

    r = toku_logcursor_destroy(&cursor);          
    if (verbose) printf("destroy returns %d\n", r);  
    assert(r==0);

    return 0;
}

int create_logfiles() {
    int r = 0;

    TOKULOGGER logger;

    LSN lsn = {0};
    TXNID txnid = 0;
    TXNID cp_txnid = 0;

    u_int32_t num_fassociate = 0;
    u_int32_t num_xstillopen = 0;
    
    bs_aname.len = 4; bs_aname.data="a.db";
    bs_bname.len = 4; bs_bname.data="b.db";
    bs_a.len = 2; bs_a.data="a";
    bs_b.len = 2; bs_b.data="b";
    bs_empty.len = 0; bs_empty.data = NULL;


    // create and open logger
    r = toku_logger_create(&logger); assert(r==0);
    r = toku_logger_open(LOGDIR, logger); assert(r==0);

    // use old x1.tdb test log as basis
    //xbegin                    'b': lsn=1 parenttxnid=0 crc=00005f1f len=29
    r = toku_log_xbegin(logger, &lsn, NO_FSYNC, 0); assert(r==0); txnid = lsn.lsn;
    //fcreate                   'F': lsn=2 txnid=1 filenum=0 fname={len=4 data="a.db"} mode=0777 treeflags=0 crc=18a3d525 len=49
    r = toku_log_fcreate(logger, &lsn, NO_FSYNC, txnid, fn_aname, bs_aname, 0x0777, 0, 0); assert(r==0);
    //commit                    'C': lsn=3 txnid=1 crc=00001f1e len=29
    r = toku_log_xcommit(logger, &lsn, FSYNC, txnid); assert(r==0);
    //xbegin                    'b': lsn=4 parenttxnid=0 crc=00000a1f len=29
    r = toku_log_xbegin(logger, &lsn, NO_FSYNC, 0); assert(r==0); txnid = lsn.lsn;
    //fcreate                   'F': lsn=5 txnid=4 filenum=1 fname={len=4 data="b.db"} mode=0777 treeflags=0 crc=14a47925 len=49
    r = toku_log_fcreate(logger, &lsn, NO_FSYNC, txnid, fn_bname, bs_bname, 0x0777, 0, 0); assert(r==0);
    //commit                    'C': lsn=6 txnid=4 crc=0000c11e len=29
    r = toku_log_xcommit(logger, &lsn, FSYNC, txnid); assert(r==0);
    //xbegin                    'b': lsn=7 parenttxnid=0 crc=0000f91f len=29
    r = toku_log_xbegin(logger, &lsn, NO_FSYNC, 0); assert(r==0); txnid = lsn.lsn;
    //enq_insert                'I': lsn=8 filenum=0 xid=7 key={len=2 data="a\000"} value={len=2 data="b\000"} crc=40b863e4 len=45
    r = toku_log_enq_insert(logger, &lsn, NO_FSYNC, fn_aname, txnid, bs_a, bs_b); assert(r==0);
    //begin_checkpoint          'x': lsn=9 timestamp=1251309957584197 crc=cd067878 len=29
    r = toku_log_begin_checkpoint(logger, &lsn, NO_FSYNC, 1251309957584197); assert(r==0); cp_txnid = lsn.lsn;
    //fassociate                'f': lsn=11 filenum=1 fname={len=4 data="b.db"} crc=a7126035 len=33
    r = toku_log_fassociate(logger, &lsn, NO_FSYNC, fn_bname, 0, bs_bname); assert(r==0);
    num_fassociate++;
    //fassociate                'f': lsn=12 filenum=0 fname={len=4 data="a.db"} crc=a70c5f35 len=33
    r = toku_log_fassociate(logger, &lsn, NO_FSYNC, fn_aname, 0, bs_aname); assert(r==0);
    num_fassociate++;
   //xstillopen                's': lsn=10 txnid=7 parent=0 crc=00061816 len=37 <- obsolete
    {
        FILENUMS filenums = {0, NULL};
        r = toku_log_xstillopen(logger, &lsn, NO_FSYNC, txnid, 0,
                                0, filenums, 0, 0, 0,
                                ROLLBACK_NONE, ROLLBACK_NONE, ROLLBACK_NONE);
        assert(r==0);
    }
    num_xstillopen++;
    //end_checkpoint            'X': lsn=13 txnid=9 timestamp=1251309957586872 crc=cd285c30 len=37
    r = toku_log_end_checkpoint(logger, &lsn, FSYNC, cp_txnid, 1251309957586872, num_fassociate, num_xstillopen); assert(r==0);
    //enq_insert                'I': lsn=14 filenum=1 xid=7 key={len=2 data="b\000"} value={len=2 data="a\000"} crc=40388be4 len=45
    r = toku_log_enq_insert(logger, &lsn, NO_FSYNC, fn_bname, txnid, bs_b, bs_a); assert(r==0);
    //commit                    'C': lsn=15 txnid=7 crc=00016d1e len=29
    r = toku_log_xcommit(logger, &lsn, FSYNC, txnid); assert(r==0);

    // close logger
    r = toku_logger_close(&logger); assert(r==0);
    return r;
}

