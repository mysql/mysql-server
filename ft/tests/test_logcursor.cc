/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#include <toku_portability.h>
#include <string.h>

#include "logger/logcursor.h"
#include "test.h"

#if defined(HAVE_LIMITS_H)
# include <limits.h>
#endif
#if defined(HAVE_SYS_SYSLIMITS_H)
# include <sys/syslimits.h>
#endif

const char LOGDIR[100] = "./dir.test_logcursor";
const int FSYNC = 1;
const int NO_FSYNC = 0;

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
    char rmrf_msg[100];
    sprintf(rmrf_msg, "rm -rf %s", LOGDIR);
    r = system(rmrf_msg);
    CKERR(r);
    toku_os_mkdir(LOGDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    if ( (r=create_logfiles()) !=0 ) return r;

    if ( (r=test_0()) !=0 ) return r;
    if ( (r=test_1()) !=0 ) return r;

    r = system(rmrf_msg);
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
    TXNID_PAIR txnid = {.parent_id64 = TXNID_NONE, .child_id64 = TXNID_NONE};
    LSN begin_checkpoint_lsn;

    uint32_t num_fassociate = 0;
    uint32_t num_xstillopen = 0;
    
    bs_aname.len = 4; bs_aname.data=(char *)"a.db";
    bs_bname.len = 4; bs_bname.data=(char *)"b.db";
    bs_a.len = 2; bs_a.data=(char *)"a";
    bs_b.len = 2; bs_b.data=(char *)"b";
    bs_empty.len = 0; bs_empty.data = NULL;


    // create and open logger
    r = toku_logger_create(&logger); assert(r==0);
    r = toku_logger_open(LOGDIR, logger); assert(r==0);

    // use old x1.tdb test log as basis
    //xbegin                    'b': lsn=1 parenttxnid=0 crc=00005f1f len=29
    txnid.parent_id64 = 1;
    toku_log_xbegin(logger, &lsn, NO_FSYNC, txnid, TXNID_PAIR_NONE);
    //fcreate                   'F': lsn=2 txnid=1 filenum=0 fname={len=4 data="a.db"} mode=0777 treeflags=0 crc=18a3d525 len=49
    toku_log_fcreate(logger, &lsn, NO_FSYNC, NULL, txnid, fn_aname, bs_aname, 0x0777, 0, 0, TOKU_DEFAULT_COMPRESSION_METHOD, 0);
    //commit                    'C': lsn=3 txnid=1 crc=00001f1e len=29
    toku_log_xcommit(logger, &lsn, FSYNC, NULL, txnid);
    //xbegin                    'b': lsn=4 parenttxnid=0 crc=00000a1f len=29
    txnid.parent_id64 = 4; // Choosing ids based on old test instead of what should happen now.
    toku_log_xbegin(logger, &lsn, NO_FSYNC, txnid, TXNID_PAIR_NONE);
    //fcreate                   'F': lsn=5 txnid=4 filenum=1 fname={len=4 data="b.db"} mode=0777 treeflags=0 crc=14a47925 len=49
    toku_log_fcreate(logger, &lsn, NO_FSYNC, NULL, txnid, fn_bname, bs_bname, 0x0777, 0, 0, TOKU_DEFAULT_COMPRESSION_METHOD, 0);
    //commit                    'C': lsn=6 txnid=4 crc=0000c11e len=29
    toku_log_xcommit(logger, &lsn, FSYNC, NULL, txnid);
    //xbegin                    'b': lsn=7 parenttxnid=0 crc=0000f91f len=29
    txnid.parent_id64 = 7; // Choosing ids based on old test instead of what should happen now.
    toku_log_xbegin(logger, &lsn, NO_FSYNC, txnid, TXNID_PAIR_NONE);
    //enq_insert                'I': lsn=8 filenum=0 xid=7 key={len=2 data="a\000"} value={len=2 data="b\000"} crc=40b863e4 len=45
    toku_log_enq_insert(logger, &lsn, NO_FSYNC, NULL, fn_aname, txnid, bs_a, bs_b);
    //begin_checkpoint          'x': lsn=9 timestamp=1251309957584197 crc=cd067878 len=29
    toku_log_begin_checkpoint(logger, &begin_checkpoint_lsn, NO_FSYNC, 1251309957584197, 0);
    //fassociate                'f': lsn=11 filenum=1 fname={len=4 data="b.db"} crc=a7126035 len=33
    toku_log_fassociate(logger, &lsn, NO_FSYNC, fn_bname, 0, bs_bname, 0);
    num_fassociate++;
    //fassociate                'f': lsn=12 filenum=0 fname={len=4 data="a.db"} crc=a70c5f35 len=33
    toku_log_fassociate(logger, &lsn, NO_FSYNC, fn_aname, 0, bs_aname, 0);
    num_fassociate++;
   //xstillopen                's': lsn=10 txnid=7 parent=0 crc=00061816 len=37 <- obsolete
    {
        FILENUMS filenums = {0, NULL};
        toku_log_xstillopen(logger, &lsn, NO_FSYNC, NULL, txnid, TXNID_PAIR_NONE,
                                0, filenums, 0, 0, 0,
                                ROLLBACK_NONE, ROLLBACK_NONE, ROLLBACK_NONE);
    }
    num_xstillopen++;
    //end_checkpoint            'X': lsn=13 txnid=9 timestamp=1251309957586872 crc=cd285c30 len=37
    toku_log_end_checkpoint(logger, &lsn, FSYNC, begin_checkpoint_lsn, 1251309957586872, num_fassociate, num_xstillopen);
    //enq_insert                'I': lsn=14 filenum=1 xid=7 key={len=2 data="b\000"} value={len=2 data="a\000"} crc=40388be4 len=45
    toku_log_enq_insert(logger, &lsn, NO_FSYNC, NULL, fn_bname, txnid, bs_b, bs_a);
    //commit                    'C': lsn=15 txnid=7 crc=00016d1e len=29
    toku_log_xcommit(logger, &lsn, FSYNC, NULL, txnid);

    // close logger
    r = toku_logger_close(&logger); assert(r==0);
    return r;
}

