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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// Define BDB if you want to compile this to use Berkeley DB
#include <stdint.h>
#include <inttypes.h>
#ifdef BDB
#include <sys/types.h>
#include <db.h>
#define DIRSUF bdb
#else
#include <tokudb.h>
#define DIRSUF tokudb
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

static inline float toku_tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}

#if !defined(DB_PRELOCKED_WRITE)
#define NO_DB_PRELOCKED
#define DB_PRELOCKED_WRITE 0
#endif

int verbose=1;

enum { SERIAL_SPACING = 1<<6 };
enum { DEFAULT_ITEMS_TO_INSERT_PER_ITERATION = 1<<20 };
enum { DEFAULT_ITEMS_PER_TRANSACTION = 1<<14 };

static void insert (long long v);
#define CKERR(r) ({ int __r = r; if (__r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, __r, db_strerror(r)); assert(__r==0); })
#define CKERR2(r,rexpect) if (r!=rexpect) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==rexpect);

/* default test parameters */
int keysize = sizeof (long long);
int valsize = sizeof (long long);
int pagesize = 0;
long long cachesize = 1000000000; // 1GB
int dupflags = 0;
int noserial = 0; // Don't do the serial stuff
int norandom = 0; // Don't do the random stuff
int prelock  = 0;
int prelockflag = 0;
int items_per_transaction = DEFAULT_ITEMS_PER_TRANSACTION;
int items_per_iteration   = DEFAULT_ITEMS_TO_INSERT_PER_ITERATION;
int finish_child_first = 0;  // Commit or abort child first (before doing so to the parent).  No effect if child does not exist.
int singlex_child = 0;  // Do a single transaction, but do all work with a child
int singlex = 0;  // Do a single transaction
int singlex_create = 0;  // Create the db using the single transaction (only valid if singlex)
int insert1first = 0;  // insert 1 before doing the rest
int do_transactions = 0;
int if_transactions_do_logging = DB_INIT_LOG; // set this to zero if we want no logging when transactions are used
int do_abort = 0;
int n_insertions_since_txn_began=0;
int env_open_flags = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;
u_int32_t put_flags = 0;
double compressibility = -1; // -1 means make it very compressible.  1 means use random bits everywhere.  2 means half the bits are random.
int do_append = 0;
u_int32_t checkpoint_period = 60;

static void do_prelock(DB* db, DB_TXN* txn) {
    if (prelock) {
#if !defined(NO_DB_PRELOCKED)
        int r = db->pre_acquire_table_lock(db, txn);
        assert(r==0);
#else
        (void) db; (void) txn;
#endif
    }
}

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)
const char *dbdir = "./bench."  STRINGIFY(DIRSUF);
char *dbfilename = "bench.db";
char *dbname;

DB_ENV *dbenv;
DB *db;
DB_TXN *parenttid=0;
DB_TXN *tid=0;


static void benchmark_setup (void) {
    int r;
   
    if (!do_append) {
        char unlink_cmd[strlen(dbdir) + strlen("rm -rf ") + 1];
        snprintf(unlink_cmd, sizeof(unlink_cmd), "rm -rf %s", dbdir);
        //printf("unlink_cmd=%s\n", unlink_cmd);
        system(unlink_cmd);
        
        if (strcmp(dbdir, ".") != 0) {
            r = mkdir(dbdir,S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
            assert(r == 0);
        }
    }

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

#if !defined(TOKUDB)
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
    if (dbenv->set_lk_max) {
        r = dbenv->set_lk_max(dbenv, items_per_transaction*2);
        assert(r==0);
    }
#elif DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 7
    if (dbenv->set_lk_max_locks) {
        r = dbenv->set_lk_max_locks(dbenv, items_per_transaction*2);
        assert(r==0);
    }
    if (dbenv->set_lk_max_lockers) {
        r = dbenv->set_lk_max_lockers(dbenv, items_per_transaction*2);
        assert(r==0);
    }
    if (dbenv->set_lk_max_objects) {
        r = dbenv->set_lk_max_objects(dbenv, items_per_transaction*2);
        assert(r==0);
    }
#else
#error
#endif
#endif

    if (dbenv->set_cachesize) {
        r = dbenv->set_cachesize(dbenv, cachesize / (1024*1024*1024), cachesize % (1024*1024*1024), 1);
        if (r != 0) 
            printf("WARNING: set_cachesize %d\n", r);
    }
    {
        r = dbenv->open(dbenv, dbdir, env_open_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        assert(r == 0);
    }

#if defined(TOKUDB)
    if (checkpoint_period) {
        printf("set checkpoint_period %u\n", checkpoint_period);
        r = dbenv->checkpointing_set_period(dbenv, checkpoint_period); assert(r == 0);
        u_int32_t period;
        r = dbenv->checkpointing_get_period(dbenv, &period); assert(r == 0 && period == checkpoint_period);
    }
#endif

    r = db_create(&db, dbenv, 0);
    assert(r == 0);

    if (do_transactions) {
        r=dbenv->txn_begin(dbenv, 0, &tid, 0); CKERR(r);
    }
    if (pagesize && db->set_pagesize) {
        r = db->set_pagesize(db, pagesize); 
        assert(r == 0);
    }
    if (dupflags) {
        r = db->set_flags(db, dupflags);
        assert(r == 0);
    }
    r = db->open(db, tid, dbfilename, NULL, DB_BTREE, DB_CREATE, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (r!=0) fprintf(stderr, "errno=%d, %s\n", errno, strerror(errno));
    assert(r == 0);
    if (insert1first) {
        if (do_transactions) {
            r=tid->commit(tid, 0);
            assert(r==0);
            tid = NULL;
            r=dbenv->txn_begin(dbenv, 0, &tid, 0); CKERR(r);
        }
        insert(-1);
        if (singlex) {
            r=tid->commit(tid, 0);
            assert(r==0);
            tid = NULL;
            r=dbenv->txn_begin(dbenv, 0, &tid, 0); CKERR(r);
        }
    }
    else if (singlex && !singlex_create) {
        r=tid->commit(tid, 0);
        assert(r==0);
        tid = NULL;
        r=dbenv->txn_begin(dbenv, 0, &tid, 0); CKERR(r);
    }
    if (do_transactions) {
        if (singlex)
            do_prelock(db, tid);
        else {
            r=tid->commit(tid, 0);
            assert(r==0);
            tid = NULL;
        }
    }
    if (singlex_child) {
        parenttid = tid;
        tid = NULL;
        r=dbenv->txn_begin(dbenv, parenttid, &tid, 0); CKERR(r);
    }

}

static void benchmark_shutdown (void) {
    int r;
    
    if (do_transactions && singlex && !insert1first && (singlex_create || prelock)) {
#if defined(TOKUDB)
        //There should be a single 'truncate' in the rollback instead of many 'insert' entries.
        struct txn_stat *s;
        r = tid->txn_stat(tid, &s);
        assert(r==0);
        //TODO: #1125 Always do the test after performance testing is done.
        if (singlex_child) fprintf(stderr, "SKIPPED 'small rollback' test for child txn\n");
        else
            assert(s->rollback_raw_count < 100);  // gross test, not worth investigating details
        free(s);
        //system("ls -l bench.tokudb");
#endif
    }
    if (do_transactions && singlex) {
        if (!singlex_child || finish_child_first) {
            assert(tid);
            r = (do_abort ? tid->abort(tid) : tid->commit(tid, 0));    assert(r==0);
            tid = NULL; 
        }
        if (singlex_child) {
            assert(parenttid);
            r = (do_abort ? parenttid->abort(parenttid) : parenttid->commit(parenttid, 0));    assert(r==0);
            parenttid = NULL;
        }
        else
            assert(!parenttid);
    }
    assert(!tid);
    assert(!parenttid);

    r = db->close(db, 0);
    assert(r == 0);
    r = dbenv->close(dbenv, 0);
    assert(r == 0);
}

static void long_long_to_array (unsigned char *a, int array_size, unsigned long long l) {
    int i;
    for (i=0; i<8 && i<array_size; i++)
    a[i] = (l>>(56-8*i))&0xff;
}

static DBT *fill_dbt(DBT *dbt, const void *data, int size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->size = size;
    dbt->data = (void *) data;
    return dbt;
}

// Fill array with 0's if compressibilty==-1, otherwise fill array with data that is likely to compress by a factor of compressibility.
static void fill_array (unsigned char *data, int size) {
    memset(data, 0, size);
    if (compressibility>0) {
        int i;
        for (i=0; i<size/compressibility; i++) {
            data[i] = (unsigned char) random();
        }
    }
}

static void insert (long long v) {
    unsigned char kc[keysize], vc[valsize];
    DBT  kt, vt;
    fill_array(kc, sizeof kc);
    long_long_to_array(kc, keysize, v); // Fill in the array first, then write the long long in.
    fill_array(vc, sizeof vc);
    long_long_to_array(vc, valsize, v);
    int r = db->put(db, tid, fill_dbt(&kt, kc, keysize), fill_dbt(&vt, vc, valsize), put_flags);
    CKERR(r);
    if (do_transactions) {
        if (n_insertions_since_txn_began>=items_per_transaction && !singlex) {
            n_insertions_since_txn_began=0;
            r = tid->commit(tid, 0); assert(r==0);
            tid = NULL;
            r=dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
            do_prelock(db, tid);
            n_insertions_since_txn_began=0;
        }
        n_insertions_since_txn_began++;
    }
}

static void serial_insert_from (long long from) {
    long long i;
    if (do_transactions && !singlex) {
        int r = dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
        do_prelock(db, tid);
        {
            DBT k,v;
            r=db->put(db, tid, fill_dbt(&k, "a", 1), fill_dbt(&v, "b", 1), put_flags);
            CKERR(r);
        }
    }
    for (i=0; i<items_per_iteration; i++) {
        insert((from+i)*SERIAL_SPACING);
    }
    if (do_transactions && !singlex) {
        int  r= tid->commit(tid, 0);             assert(r==0);
        tid=NULL;
    }
}

static long long llrandom (void) {
    return (((long long)(random()))<<32) + random();
}

static void random_insert_below (long long below) {
    long long i;
    if (do_transactions && !singlex) {
        int r = dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
        do_prelock(db, tid);
    }
    for (i=0; i<items_per_iteration; i++) {
        insert(llrandom()%below);
    }
    if (do_transactions && !singlex) {
        int  r= tid->commit(tid, 0);             assert(r==0);
        tid=NULL;
    }
}

static void biginsert (long long n_elements, struct timeval *starttime) {
    long long i;
    struct timeval t1,t2;
    int iteration;
    for (i=0, iteration=0; i<n_elements; i+=items_per_iteration, iteration++) {
        if (verbose) {
            printf("%d ", iteration);
            fflush(stdout);
        }
        if (!noserial) {
            gettimeofday(&t1,0);
            serial_insert_from(i);
            gettimeofday(&t2,0);
            if (verbose) {
                printf("serial %9.6fs %8.0f/s    ", toku_tdiff(&t2, &t1), items_per_iteration/toku_tdiff(&t2, &t1));
                fflush(stdout);
            }
        }
        if (!norandom) {
            gettimeofday(&t1,0);
            random_insert_below((i+items_per_iteration)*SERIAL_SPACING);
            gettimeofday(&t2,0);
            if (verbose) {
                printf("random %9.6fs %8.0f/s    ", toku_tdiff(&t2, &t1), items_per_iteration/toku_tdiff(&t2, &t1));
                fflush(stdout);
            }
        }
        if (verbose) {
            printf("cumulative %9.6fs %8.0f/s\n", toku_tdiff(&t2, starttime), (((float)items_per_iteration*(!noserial+!norandom))/toku_tdiff(&t2, starttime))*(iteration+1));
            fflush(stdout);
        }
    }
}

const long long default_n_items = 1LL<<22;

static int print_usage (const char *argv0) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, " %s [-x] [--keysize KEYSIZE] [--valsize VALSIZE] [--noserial] [--norandom] [ n_iterations ]\n", argv0);
    fprintf(stderr, "   where\n");
    fprintf(stderr, "    -x                    do transactions (XCOUNT transactions per iteration) (default: no transactions at all)\n");
    fprintf(stderr, "    --keysize KEYSIZE     sets the key size (default 8)\n");
    fprintf(stderr, "    --valsize VALSIZE     sets the value size (default 8)\n");
    fprintf(stderr, "    --noserial            causes the serial insertions to be skipped\n");
    fprintf(stderr, "    --norandom            causes the random insertions to be skipped\n");
    fprintf(stderr, "    --cachesize CACHESIZE set the database cache size\n");
    fprintf(stderr, "    --pagesize PAGESIZE   sets the database page size\n");
    fprintf(stderr, "    --compressibility C   creates data that should compress by about a factor C.   Default C is large.   C is an float.\n");
    fprintf(stderr, "    --xcount N            how many insertions per transaction (default=%d)\n", DEFAULT_ITEMS_PER_TRANSACTION);
    fprintf(stderr, "    --singlex             (implies -x) Run the whole job as a single transaction.  (Default don't run as a single transaction.)\n");
    fprintf(stderr, "    --singlex-child       (implies -x) Run the whole job as a single transaction, do all work a child of that transaction.\n");
    fprintf(stderr, "    --finish-child-first  Commit/abort child before doing so to parent (no effect if no child).\n");
    fprintf(stderr, "    --singlex-create      (implies --singlex)  Create the file using the single transaction (Default is to use a different transaction to create.)\n");
    fprintf(stderr, "    --prelock             Prelock the database.\n");
    fprintf(stderr, "    --prelockflag         Prelock the database and send the DB_PRELOCKED_WRITE flag.\n");
    fprintf(stderr, "    --abort               Abort the singlex after the transaction is over. (Requires --singlex.)\n");
    fprintf(stderr, "    --nolog               If transactions are used, then don't write the recovery log\n");
    fprintf(stderr, "    --periter N           how many insertions per iteration (default=%d)\n", DEFAULT_ITEMS_TO_INSERT_PER_ITERATION);
    fprintf(stderr, "    --env DIR\n");
    fprintf(stderr, "    --append              append to an existing file\n");
    fprintf(stderr, "    --checkpoint-period %" PRIu32 "       checkpoint period\n", checkpoint_period); 
    fprintf(stderr, "   n_iterations     how many iterations (default %lld)\n", default_n_items/DEFAULT_ITEMS_TO_INSERT_PER_ITERATION);

    return 1;
}

#define UU(x) x __attribute__((__unused__))

int main (int argc, const char *argv[]) {
    struct timeval t1,t2,t3;
    long long total_n_items = default_n_items;
    char *endptr;
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (arg[0] != '-')
            break;
        if (strcmp(arg, "-q") == 0) {
            verbose--; if (verbose<0) verbose=0;
        } else if (strcmp(arg, "-x") == 0) {
            do_transactions = 1;
        } else if (strcmp(arg, "--noserial") == 0) {
            noserial=1;
        } else if (strcmp(arg, "--norandom") == 0) {
            norandom=1;
        } else if (strcmp(arg, "--compressibility") == 0) {
            compressibility = atof(argv[++i]);
        } else if (strcmp(arg, "--nolog") == 0) {
            if_transactions_do_logging = 0;
        } else if (strcmp(arg, "--singlex-create") == 0) {
            do_transactions = 1;
            singlex = 1;
            singlex_create = 1;
        } else if (strcmp(arg, "--finish-child-first") == 0) {
            finish_child_first = 1;
        } else if (strcmp(arg, "--singlex-child") == 0) {
            do_transactions = 1;
            singlex = 1;
            singlex_child = 1;
        } else if (strcmp(arg, "--singlex") == 0) {
            do_transactions = 1;
            singlex = 1;
        } else if (strcmp(arg, "--insert1first") == 0) {
            insert1first = 1;
        } else if (strcmp(arg, "--xcount") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            items_per_transaction = strtoll(argv[++i], &endptr, 10); assert(*endptr == 0);
        } else if (strcmp(arg, "--abort") == 0) {
            do_abort = 1;
        } else if (strcmp(arg, "--periter") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            items_per_iteration = strtoll(argv[++i], &endptr, 10); assert(*endptr == 0);
        } else if (strcmp(arg, "--cachesize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            cachesize = strtoll(argv[++i], &endptr, 10); assert(*endptr == 0);
        } else if (strcmp(arg, "--keysize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            keysize = atoi(argv[++i]);
        } else if (strcmp(arg, "--valsize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            valsize = atoi(argv[++i]);
        } else if (strcmp(arg, "--pagesize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            pagesize = atoi(argv[++i]);
        } else if (strcmp(arg, "--env") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            dbdir = argv[++i];
        } else if (strcmp(arg, "--prelock") == 0) {
            prelock=1;
        } else if (strcmp(arg, "--prelockflag") == 0) {
            prelock=1;
            prelockflag=1;
        } else if (strcmp(arg, "--srandom") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            srandom(atoi(argv[++i]));
        } else if (strcmp(arg, "--append") == 0) {
            do_append = 1;
        } else if (strcmp(arg, "--checkpoint-period") == 0) {
            if (i+1 >= argc) return print_usage(argv[9]);
            checkpoint_period = (u_int32_t) atoi(argv[++i]);
        } else if (strcmp(arg, "--unique_checks") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            int unique_checks = atoi(argv[++i]);
            if (unique_checks)
                put_flags = DB_NOOVERWRITE;
            else
                put_flags = 0;
        } else {
            return print_usage(argv[0]);
        }
    }
    if (do_transactions) {
        env_open_flags |= DB_INIT_TXN | if_transactions_do_logging | DB_INIT_LOCK;
    }
    if (do_transactions && prelockflag) {
        put_flags |= DB_PRELOCKED_WRITE;
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
    if (verbose) {
        if (!noserial) printf("serial ");
        if (!noserial && !norandom) printf("and ");
        if (!norandom) printf("random ");
        printf("insertions of %d per batch%s\n", items_per_iteration, do_transactions ? " (with transactions)" : "");
    }
    benchmark_setup();
    gettimeofday(&t1,0);
    biginsert(total_n_items, &t1);
    gettimeofday(&t2,0);
    benchmark_shutdown();
    gettimeofday(&t3,0);
    if (verbose) {
        printf("Shutdown %9.6fs\n", toku_tdiff(&t3, &t2));
        printf("Total time %9.6fs for %lld insertions = %8.0f/s\n", toku_tdiff(&t3, &t1), 
               (!noserial+!norandom)*total_n_items, (!noserial+!norandom)*total_n_items/toku_tdiff(&t3, &t1));
    }

    return 0;
}
